#pragma once

#include <string>
#include <cstdint>
#include <map>

class COffsetUpdater
{
public:
    struct OffsetData
    {
        std::uintptr_t m_uEntityList = 0U;
        std::uintptr_t m_uViewMatrix = 0U;
        std::uintptr_t m_uLocalPlayerController = 0U;
        std::uintptr_t m_uPlantedC4 = 0U;
        std::uintptr_t m_uGlobalVars = 0U;
        std::uintptr_t m_uCSGOInput = 0U;
        std::uintptr_t m_uNetworkGameClient = 0U;
        std::uintptr_t m_uEntitySystem = 0U;
        std::uintptr_t m_uSensitivity = 0U;
    };

    // Fetch latest offsets from local dump, cs2-dumper API or fallback sources
    static bool FetchLatestOffsets(OffsetData& outOffsets);
    
    // Fetch offsets directly from a local cs2-dumper output folder
    // (e.g. C:\Users\mas\Downloads\output\offsets.json)
    static bool FetchFromLocalDump(OffsetData& outOffsets);
    
    // Update offsets in Globals.cpp automatically
    static bool UpdateOffsetsInFile(const OffsetData& offsets);
    
    // Load offsets from cached file
    static bool LoadCachedOffsets(OffsetData& outOffsets);
    
    // Save offsets to cache file
    static bool SaveCachedOffsets(const OffsetData& offsets);
    
    // Main function to update offsets (tries online first, falls back to cache)
    static bool UpdateOffsets();

private:
    // Fetch from cs2-dumper API
    static bool FetchFromCS2Dumper(OffsetData& outOffsets);
    
    // Fetch from alternative source (diabloakar0/cs2-offsets)
    static bool FetchFromAlternativeSource(OffsetData& outOffsets);
    
    // Parse JSON response and extract offsets
    static bool ParseOffsetsFromJSON(const std::string& jsonResponse, OffsetData& outOffsets);
    
    // HTTP request helper using WinHTTP
    static bool HTTPRequest(const std::wstring& url, std::string& response);
    
    // Convert hex string to uintptr_t
    static std::uintptr_t HexStringToUintptr(const std::string& hexStr);
    
    // Get cache file path
    static std::string GetCacheFilePath();
};

inline COffsetUpdater g_OffsetUpdater;

