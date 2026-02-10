#include "../Includes.h"
#include "OffsetUpdater.h"
#include <winhttp.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "winhttp.lib")

// Convert hex string to uintptr_t
std::uintptr_t COffsetUpdater::HexStringToUintptr(const std::string& hexStr)
{
    if (hexStr.empty())
        return 0;
    
    std::string cleanStr = hexStr;
    // Remove "0x" or "0X" prefix if present
    if (cleanStr.length() > 2 && (cleanStr.substr(0, 2) == "0x" || cleanStr.substr(0, 2) == "0X"))
        cleanStr = cleanStr.substr(2);
    
    // Remove any whitespace
    cleanStr.erase(std::remove_if(cleanStr.begin(), cleanStr.end(), ::isspace), cleanStr.end());
    
    if (cleanStr.empty())
        return 0;
    
    std::uintptr_t result = 0;
    std::stringstream ss;
    ss << std::hex << cleanStr;
    ss >> result;
    return result;
}

// Get cache file path
std::string COffsetUpdater::GetCacheFilePath()
{
    char szPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, szPath)))
    {
        std::string path = std::string(szPath) + "\\CS2-External\\offsets_cache.json";
        
        // Create directory if it doesn't exist
        std::filesystem::path dirPath = std::filesystem::path(path).parent_path();
        if (!std::filesystem::exists(dirPath))
            std::filesystem::create_directories(dirPath);
        
        return path;
    }
    return "offsets_cache.json";
}

// HTTP request helper using WinHTTP
bool COffsetUpdater::HTTPRequest(const std::wstring& url, std::string& response)
{
    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;
    bool bSuccess = false;

    try
    {
        // Initialize WinHTTP
        hSession = WinHttpOpen(
            L"CS2-External-OffsetUpdater/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );

        if (!hSession)
            return false;

        // Parse URL using wide string version
        URL_COMPONENTSW urlComp;
        ZeroMemory(&urlComp, sizeof(urlComp));
        urlComp.dwStructSize = sizeof(urlComp);
        urlComp.dwSchemeLength = (DWORD)-1;
        urlComp.dwHostNameLength = (DWORD)-1;
        urlComp.dwUrlPathLength = (DWORD)-1;
        urlComp.dwExtraInfoLength = (DWORD)-1;

        if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp))
        {
            WinHttpCloseHandle(hSession);
            return false;
        }

        std::wstring hostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
        std::wstring urlPath(urlComp.lpszUrlPath ? urlComp.lpszUrlPath : L"/");
        if (urlComp.lpszExtraInfo)
            urlPath += std::wstring(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);

        // Connect to server
        hConnect = WinHttpConnect(
            hSession,
            hostName.c_str(),
            urlComp.nPort,
            0
        );

        if (!hConnect)
        {
            WinHttpCloseHandle(hSession);
            return false;
        }

        // Create request
        hRequest = WinHttpOpenRequest(
            hConnect,
            L"GET",
            urlPath.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            urlComp.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0
        );

        if (!hRequest)
        {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        // Send request
        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
        {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        // Receive response
        if (!WinHttpReceiveResponse(hRequest, nullptr))
        {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        // Read response data
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        response.clear();

        do
        {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
                break;

            if (dwSize == 0)
                break;

            std::vector<char> buffer(dwSize + 1);
            ZeroMemory(buffer.data(), dwSize + 1);

            if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded))
                break;

            response.append(buffer.data(), dwDownloaded);
        } while (dwSize > 0);

        bSuccess = !response.empty();
    }
    catch (...)
    {
        bSuccess = false;
    }

    // Cleanup
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);

    return bSuccess;
}

// Parse JSON response and extract offsets
bool COffsetUpdater::ParseOffsetsFromJSON(const std::string& jsonResponse, OffsetData& outOffsets)
{
    try
    {
        nlohmann::json json = nlohmann::json::parse(jsonResponse);

        // Helper function to extract offset value (handles both string and number formats)
        auto extractOffset = [](const nlohmann::json& obj, const std::string& key) -> std::uintptr_t {
            if (!obj.contains(key))
                return 0;
            
            auto value = obj[key];
            if (value.is_string())
            {
                std::string str = value.get<std::string>();
                // Remove "0x" prefix if present
                if (str.length() > 2 && str.substr(0, 2) == "0x")
                    str = str.substr(2);
                return HexStringToUintptr(str);
            }
            else if (value.is_number())
            {
                return value.get<std::uintptr_t>();
            }
            return 0;
        };

        // Try cs2-dumper format (uses "client.dll" with dot, not underscore)
        nlohmann::json clientDll;
        if (json.contains("client.dll"))
        {
            clientDll = json["client.dll"];
        }
        else if (json.contains("client_dll"))
        {
            // Fallback for alternative format
            clientDll = json["client_dll"];
        }
        
        if (!clientDll.is_null())
        {
            // Extract offsets (these are RVA offsets, not full addresses)
            outOffsets.m_uEntityList = extractOffset(clientDll, "dwEntityList");
            outOffsets.m_uViewMatrix = extractOffset(clientDll, "dwViewMatrix");
            outOffsets.m_uLocalPlayerController = extractOffset(clientDll, "dwLocalPlayerController");
            outOffsets.m_uPlantedC4 = extractOffset(clientDll, "dwPlantedC4");
            outOffsets.m_uGlobalVars = extractOffset(clientDll, "dwGlobalVars");
            outOffsets.m_uCSGOInput = extractOffset(clientDll, "dwCSGOInput");
            outOffsets.m_uSensitivity = extractOffset(clientDll, "dwSensitivity");
            
            // Try to get EntitySystem separately (might be named differently)
            if (outOffsets.m_uEntitySystem == 0)
                outOffsets.m_uEntitySystem = extractOffset(clientDll, "dwGameEntitySystem");
        }

        // Try engine2.dll for NetworkGameClient (uses dot, not underscore)
        nlohmann::json engine2Dll;
        if (json.contains("engine2.dll"))
        {
            engine2Dll = json["engine2.dll"];
        }
        else if (json.contains("engine2_dll"))
        {
            // Fallback for alternative format
            engine2Dll = json["engine2_dll"];
        }
        
        if (!engine2Dll.is_null())
        {
            outOffsets.m_uNetworkGameClient = extractOffset(engine2Dll, "dwNetworkGameClient");
        }

        // EntitySystem is usually the same as EntityList
        if (outOffsets.m_uEntityList != 0)
            outOffsets.m_uEntitySystem = outOffsets.m_uEntityList;

        // Validate that we got at least some offsets
        return outOffsets.m_uEntityList != 0 && outOffsets.m_uViewMatrix != 0;
    }
    catch (...)
    {
        return false;
    }
}

// Fetch offsets from local cs2-dumper dump (e.g. C:\Users\mas\Downloads\output\offsets.json)
bool COffsetUpdater::FetchFromLocalDump(OffsetData& outOffsets)
{
    try
    {
        // Preferred explicit path from user
        std::vector<std::string> vecPaths =
        {
            "C:\\Users\\mas\\Downloads\\output\\offsets.json",
            ".\\output\\offsets.json",
            "output\\offsets.json"
        };

        for (const auto& path : vecPaths)
        {
            std::ifstream file(path);
            if (!file.is_open())
                continue;

            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            if (ParseOffsetsFromJSON(content, outOffsets))
            {
                // Cache for future runs
                SaveCachedOffsets(outOffsets);
                return true;
            }
        }
    }
    catch (...)
    {
    }

    return false;
}

// Fetch from cs2-dumper API (GitHub)
bool COffsetUpdater::FetchFromCS2Dumper(OffsetData& outOffsets)
{
    // cs2-dumper API endpoint (if available)
    // Alternative: Use raw.githubusercontent.com for direct JSON access
    std::wstring url = L"https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/offsets.json";
    
    std::string response;
    if (!HTTPRequest(url, response))
        return false;

    return ParseOffsetsFromJSON(response, outOffsets);
}

// Fetch from alternative source
bool COffsetUpdater::FetchFromAlternativeSource(OffsetData& outOffsets)
{
    // Alternative source: diabloakar0/cs2-offsets
    std::wstring url = L"https://raw.githubusercontent.com/diabloakar0/cs2-offsets/main/offsets.json";
    
    std::string response;
    if (!HTTPRequest(url, response))
        return false;

    return ParseOffsetsFromJSON(response, outOffsets);
}

// Fetch latest offsets
bool COffsetUpdater::FetchLatestOffsets(OffsetData& outOffsets)
{
    // Try local dump first (fastest & user-provided)
    if (FetchFromLocalDump(outOffsets))
        return true;

    // Try cs2-dumper first
    if (FetchFromCS2Dumper(outOffsets))
    {
        SaveCachedOffsets(outOffsets);
        return true;
    }

    // Fallback to alternative source
    if (FetchFromAlternativeSource(outOffsets))
    {
        SaveCachedOffsets(outOffsets);
        return true;
    }

    return false;
}

// Save offsets to cache file
bool COffsetUpdater::SaveCachedOffsets(const OffsetData& offsets)
{
    try
    {
        nlohmann::json json;
        json["client.dll"]["dwEntityList"] = offsets.m_uEntityList;
        json["client.dll"]["dwViewMatrix"] = offsets.m_uViewMatrix;
        json["client.dll"]["dwLocalPlayerController"] = offsets.m_uLocalPlayerController;
        json["client.dll"]["dwPlantedC4"] = offsets.m_uPlantedC4;
        json["client.dll"]["dwGlobalVars"] = offsets.m_uGlobalVars;
        json["client.dll"]["dwCSGOInput"] = offsets.m_uCSGOInput;
        json["client.dll"]["dwSensitivity"] = offsets.m_uSensitivity;
        json["engine2.dll"]["dwNetworkGameClient"] = offsets.m_uNetworkGameClient;
        json["client.dll"]["dwGameEntitySystem"] = offsets.m_uEntitySystem;

        std::ofstream file(GetCacheFilePath());
        if (file.is_open())
        {
            file << json.dump(4);
            file.close();
            return true;
        }
    }
    catch (...)
    {
    }

    return false;
}

// Load offsets from cached file
bool COffsetUpdater::LoadCachedOffsets(OffsetData& outOffsets)
{
    try
    {
        std::ifstream file(GetCacheFilePath());
        if (!file.is_open())
            return false;

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        return ParseOffsetsFromJSON(content, outOffsets);
    }
    catch (...)
    {
        return false;
    }
}

// Update offsets in Globals.cpp automatically
bool COffsetUpdater::UpdateOffsetsInFile(const OffsetData& offsets)
{
    try
    {
        std::string filePath = "src/Globals.cpp";
        std::ifstream file(filePath);
        if (!file.is_open())
            return false;

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // Replace offset values
        auto replaceOffset = [&](const std::string& name, std::uintptr_t value) {
            std::string searchPattern = name + " = clientModule.m_uBaseAddress + 0x";
            size_t pos = content.find(searchPattern);
            if (pos != std::string::npos)
            {
                size_t endPos = content.find(';', pos);
                if (endPos != std::string::npos)
                {
                    std::string oldValue = content.substr(pos, endPos - pos);
                    std::string newValue = name + " = clientModule.m_uBaseAddress + " + std::format("0x{:X}", value);
                    content.replace(pos, endPos - pos, newValue);
                }
            }
        };

        auto replaceEngineOffset = [&](const std::string& name, std::uintptr_t value) {
            std::string searchPattern = name + " = engine2Module.m_uBaseAddress + 0x";
            size_t pos = content.find(searchPattern);
            if (pos != std::string::npos)
            {
                size_t endPos = content.find(';', pos);
                if (endPos != std::string::npos)
                {
                    std::string oldValue = content.substr(pos, endPos - pos);
                    std::string newValue = name + " = engine2Module.m_uBaseAddress + " + std::format("0x{:X}", value);
                    content.replace(pos, endPos - pos, newValue);
                }
            }
        };

        replaceOffset("g_Globals.m_Offsets.m_uEntityList", offsets.m_uEntityList);
        replaceOffset("g_Globals.m_Offsets.m_uViewMatrix", offsets.m_uViewMatrix);
        replaceOffset("g_Globals.m_Offsets.m_uLocalPlayerController", offsets.m_uLocalPlayerController);
        replaceOffset("g_Globals.m_Offsets.m_uPlantedC4", offsets.m_uPlantedC4);
        replaceOffset("g_Globals.m_Offsets.m_uGlobalVars", offsets.m_uGlobalVars);
        replaceOffset("g_Globals.m_Offsets.m_uCSGOInput", offsets.m_uCSGOInput);
        replaceOffset("g_Globals.m_Offsets.m_uEntitySystem", offsets.m_uEntitySystem);
        replaceOffset("g_Globals.m_Offsets.m_uSensitivity", offsets.m_uSensitivity);
        replaceEngineOffset("g_Globals.m_Offsets.m_uNetworkGameClient", offsets.m_uNetworkGameClient);

        // Update dump date comment
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_s(&tm, &time_t);
        char dateStr[64];
        std::strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M:%S UTC", &tm);
        
        size_t datePos = content.find("// Dump date:");
        if (datePos != std::string::npos)
        {
            size_t dateEnd = content.find('\n', datePos);
            if (dateEnd != std::string::npos)
            {
                content.replace(datePos, dateEnd - datePos, "// Dump date: " + std::string(dateStr) + " (Auto-updated)");
            }
        }

        // Write back to file
        std::ofstream outFile(filePath);
        if (outFile.is_open())
        {
            outFile << content;
            outFile.close();
            return true;
        }
    }
    catch (...)
    {
    }

    return false;
}

// Main function to update offsets
bool COffsetUpdater::UpdateOffsets()
{
    OffsetData offsets;
    
    // Try to fetch latest offsets online
    if (FetchLatestOffsets(offsets))
    {
        // Optionally update Globals.cpp file (commented out by default to avoid overwriting)
        // UpdateOffsetsInFile(offsets);
        return true;
    }
    
    // Fallback to cached offsets
    if (LoadCachedOffsets(offsets))
        return true;

    return false;
}

