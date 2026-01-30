#include "Includes.h"

FILE* m_pConsoleStream = nullptr;
std::ofstream m_ofsFile{};

bool ConsoleAttach(const char* szConsoleTitle)
{
    if (!AllocConsole())
        return false;

    AttachConsole(ATTACH_PARENT_PROCESS);

    FILE* fp;
    freopen_s(&fp, X("CONIN$"), X("r"), stdin);
    freopen_s(&fp, X("CONOUT$"), X("w"), stdout);
    freopen_s(&fp, X("CONOUT$"), X("w"), stderr);

    if (!SetConsoleTitleA(szConsoleTitle))
        return false;

    return true;
}

bool DetachConsole()
{
    FILE* fp;
    freopen_s(&fp, X("NUL"), X("r"), stdin);
    freopen_s(&fp, X("NUL"), X("w"), stdout);
    freopen_s(&fp, X("NUL"), X("w"), stderr);

    // Konsole freigeben
    return FreeConsole() != 0;
}

void SetThreadPriorityWrapper()
{
    // Get the handle to your program's thread
    HANDLE hThread = GetCurrentThread();
    if (hThread)
    {
        int nThreadPriority = GetThreadPriority(hThread);
        if (nThreadPriority == THREAD_PRIORITY_ERROR_RETURN) // error
            throw std::runtime_error(X("failed to get thread priority"));

        if (nThreadPriority == THREAD_PRIORITY_HIGHEST) // already set
        {
            // close the handle
            CloseHandle(hThread);
            return;
        }

        // set the thread priority to high
        SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST);
        // close the handle
        CloseHandle(hThread);
    }
    else // error
        throw std::runtime_error(X("failed to set thread priority"));
}

void EntityThread()
{
    // set thread priority
    SetThreadPriorityWrapper();

    while (!g_Globals.m_bIsUnloading)
    {
        if (!g_Utilities.IsInGame())
        {
            // sleep to conserve system resources
            g_Utilities.Sleep(3000.f);
			continue;
		}

        // lock entity list
        EntityList::m_mtxEntities.lock();

        // update entities
        EntityList::UpdateEntities();

        // unlock entity list
        EntityList::m_mtxEntities.unlock();

        // sleep in-between ticks
        g_Utilities.Sleep(INTERVAL_PER_TICK * 1000.0f);
    }
}

void RenderThread()
{
    // set thread priority
    SetThreadPriorityWrapper();

    while (!g_Globals.m_bIsUnloading)
    {
        // clear data from previous call
        Draw::ClearDrawData();

        if (!g_Memory.IsWindowInForeground(X("Counter-Strike 2")))
        {
            // "safe" way of clearing
            Draw::SwapDrawData();

            g_Utilities.Sleep(1000.0f);
            continue;
        }

        // lock entities
        std::unique_lock lockEntityGuard(EntityList::m_mtxEntities);

        // copy entities into separate container
        std::vector<EntityObject_t> vecEntities;
        vecEntities.assign(EntityList::m_vecEntities.begin(), EntityList::m_vecEntities.end());

        // unlock entities
        lockEntityGuard.unlock();
        lockEntityGuard.release();

        if (Window::m_bInitialized)
        {        
            // run ESP
            for (EntityObject_t& object : vecEntities)
            {
                switch (object.m_eType)
                {
                    case EEntityType::ENTITY_PLAYER:
                    {
                        CCSPlayerController* pController = reinterpret_cast<CCSPlayerController*>(object.m_pEntity);
                        if (!pController || pController->m_bIsLocalPlayerController())
                            continue;

                        C_CSPlayerPawn* pPawn = reinterpret_cast<C_CSPlayerPawn*>(pController->m_hPawn().Get());
                        if (!pPawn || !pPawn->IsAlive())
                            continue;
                        
                        // Check if we should show teammates
                        bool bShowTeammates = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bShowTeammates);
                        if (!bShowTeammates && g_Globals.m_LocalPlayer.m_pPlayerPawn)
                        {
                            std::uint8_t localTeam = g_Globals.m_LocalPlayer.m_pPlayerPawn->m_iTeamNum();
                            std::uint8_t playerTeam = pPawn->m_iTeamNum();
                            if (localTeam == playerTeam && localTeam != 0) // Same team and not unassigned
                                continue;
                        }

                        // esp
                        if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bEnableVisuals))
                        {
                            try
                            {
                                // Safety checks
                                if (!pPawn || !pController)
                                    continue;
                                    
                                CGameSceneNode* pSceneNode = pPawn->m_pGameSceneNode();
                                if (!pSceneNode)
                                    continue;
                                
                                // Get player position
                                Vector vecOrigin = pSceneNode->m_vecAbsOrigin();
                                
                                // Validate origin
                                if (vecOrigin.x == 0.0f && vecOrigin.y == 0.0f && vecOrigin.z == 0.0f)
                                    continue;
                                
                                // Get head position - use bone cache for accurate head position if available
                                Vector vecHead = vecOrigin;
                                bool bUseBoneHead = false;
                                
                                // Try to get actual head bone position (bone index 6 = head)
                                BoneData_t* pBoneCache = pSceneNode->m_pBoneCache();
                                if (pBoneCache)
                                {
                                    std::uintptr_t uHeadBoneAddress = reinterpret_cast<std::uintptr_t>(pBoneCache) + (6 * 0x20);
                                    Vector vecHeadBone = g_Memory.ReadMemory<Vector>(uHeadBoneAddress);
                                    if (!vecHeadBone.IsZero() && 
                                        std::abs(vecHeadBone.x) < 50000.0f && 
                                        std::abs(vecHeadBone.y) < 50000.0f && 
                                        std::abs(vecHeadBone.z) < 50000.0f)
                                    {
                                        vecHead = vecHeadBone;
                                        bUseBoneHead = true;
                                    }
                                }
                                
                                // Fallback: approximate head position (add 72 units for full head height)
                                if (!bUseBoneHead)
                                {
                                    vecHead = vecOrigin;
                                    vecHead.z += 72.0f; // Increased from 64 to reach top of head
                                }
                                
                                // Convert to screen coordinates
                                ImVec2 vecScreenOrigin, vecScreenHead;
                                if (!Draw::WorldToScreen(vecOrigin, vecScreenOrigin) || !Draw::WorldToScreen(vecHead, vecScreenHead))
                                    continue;
                                
                                // Validate screen coordinates
                                ImVec2 vecDisplaySize = ImGui::GetIO().DisplaySize;
                                if (vecDisplaySize.x <= 0.0f || vecDisplaySize.y <= 0.0f)
                                    continue;
                                    
                                if (vecScreenOrigin.x < 0 || vecScreenOrigin.x > vecDisplaySize.x || 
                                    vecScreenOrigin.y < 0 || vecScreenOrigin.y > vecDisplaySize.y ||
                                    vecScreenHead.x < 0 || vecScreenHead.x > vecDisplaySize.x || 
                                    vecScreenHead.y < 0 || vecScreenHead.y > vecDisplaySize.y)
                                    continue;
                                
                                // Calculate box dimensions based on actual player height
                                float flHeight = vecScreenOrigin.y - vecScreenHead.y;
                                if (flHeight <= 0.0f || flHeight > vecDisplaySize.y)
                                    continue;
                                
                                // Ensure height doesn't exceed reasonable player size (clamp to max ~200 pixels)
                                // This prevents the box from being bigger than the player
                                float flMaxHeight = vecDisplaySize.y * 0.3f; // Max 30% of screen height
                                flHeight = std::min(flHeight, flMaxHeight);
                                    
                                float flWidth = flHeight * 0.5f; // Box width is half of height
                                
                                // Ensure width doesn't exceed reasonable player width
                                float flMaxWidth = vecDisplaySize.x * 0.2f; // Max 20% of screen width
                                flWidth = std::min(flWidth, flMaxWidth);
                                
                                // Validate width
                                if (flWidth <= 0.0f || flWidth > vecDisplaySize.x)
                                    continue;
                                
                                // Calculate box corners (same size for both normal box and Kirk ESP)
                                // The box already scales naturally with distance due to perspective
                                // Box goes from top of head to bottom of feet
                                ImVec2 vecMin = ImVec2(vecScreenHead.x - flWidth, vecScreenHead.y);
                                ImVec2 vecMax = ImVec2(vecScreenHead.x + flWidth, vecScreenOrigin.y);
                                
                                // Get colors from config
                                Color colBox = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colESPBox);
                                Color colLine = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colESPLine);
                                Color colText = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colESPText);
                                Color colSkeleton = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colSkeletonEsp);
                                Color colLinesEsp = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colLinesEsp);
                                
                                // Get ESP settings
                                int iThickness = CONFIG_GET(int, g_Variables.m_PlayerVisuals.m_iEspThickness);
                                float flThickness = std::clamp(static_cast<float>(iThickness), 1.0f, 10.0f);
                                
                                // Apply transparency
                                int iTransparency = CONFIG_GET(int, g_Variables.m_PlayerVisuals.m_iEspTransparency);
                                float flAlpha = std::clamp(static_cast<float>(iTransparency) / 100.0f, 0.0f, 1.0f);
                                uint8_t uAlpha = static_cast<uint8_t>(std::clamp(static_cast<int>(colBox.a() * flAlpha), 0, 255));
                                colBox.Set(colBox.r(), colBox.g(), colBox.b(), uAlpha);
                                
                                // Draw Kirk ESP (image box) or normal Box ESP
                                if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bKirkEspEnabled))
                                {
                                    // Load kirk texture if not already loaded (static to persist across frames)
                                    static ImTextureID pKirkTexture = nullptr;
                                    static bool bTextureLoaded = false;
                                    static bool bTextureLoadAttempted = false;
                                    
                                    if (!bTextureLoadAttempted && Window::m_pDevice)
                                    {
                                        // Get executable directory
                                        char szExePath[MAX_PATH];
                                        GetModuleFileNameA(nullptr, szExePath, MAX_PATH);
                                        std::string sExeDir = szExePath;
                                        size_t uLastSlash = sExeDir.find_last_of("\\/");
                                        if (uLastSlash != std::string::npos)
                                            sExeDir = sExeDir.substr(0, uLastSlash + 1);
                                        
                                        // Try to load the image from executable directory
                                        std::string sKirkPng = sExeDir + "kirk.png";
                                        std::string sKirkJpg = sExeDir + "kirk.jpg";
                                        
                                        pKirkTexture = g_Utilities.LoadImageTexture(sKirkPng.c_str());
                                        if (!pKirkTexture)
                                            pKirkTexture = g_Utilities.LoadImageTexture(sKirkJpg.c_str());
                                        
                                        bTextureLoadAttempted = true;
                                        bTextureLoaded = (pKirkTexture != nullptr);
                                    }
                                    
                                    if (pKirkTexture)
                                    {
                                        // Draw the image as the box
                                        // Use white color with alpha from colBox to show the image properly
                                        Color colImage;
                                        colImage.Set(255, 255, 255, colBox.a());
                                        Draw::AddImage(pKirkTexture, vecMin, vecMax, colImage, 0.0f);
                                    }
                                    else
                                    {
                                        // Fallback to normal box if image not found
                                        Draw::AddRect(vecMin, vecMax, colBox, DRAW_RECT_OUTLINE, Color(0, 0, 0, 255), 0.0f, flThickness);
                                    }
                                }
                                else if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bBoxEspEnabled))
                                {
                                    bool bCorneredBox = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bCorneredBoxEnabled);
                                    bool bBoxFill = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bBoxFillEnabled);
                                    
                                    if (bBoxFill)
                                    {
                                        Color colFill = colBox;
                                        uint8_t uFillAlpha = static_cast<uint8_t>(std::clamp(static_cast<int>(colFill.a() * 0.3f), 0, 255));
                                        colFill.Set(colFill.r(), colFill.g(), colFill.b(), uFillAlpha);
                                        Draw::AddRect(vecMin, vecMax, colFill, DRAW_RECT_FILLED, Color(0, 0, 0, 0), 0.0f, 0.0f);
                                    }
                                    
                                    if (bCorneredBox)
                                    {
                                        // Draw cornered box
                                        float flCornerLength = 20.0f;
                                        
                                        // Top-left corner
                                        Draw::AddLine(ImVec2(vecMin.x, vecMin.y), ImVec2(vecMin.x + flCornerLength, vecMin.y), colBox, flThickness);
                                        Draw::AddLine(ImVec2(vecMin.x, vecMin.y), ImVec2(vecMin.x, vecMin.y + flCornerLength), colBox, flThickness);
                                        
                                        // Top-right corner
                                        Draw::AddLine(ImVec2(vecMax.x, vecMin.y), ImVec2(vecMax.x - flCornerLength, vecMin.y), colBox, flThickness);
                                        Draw::AddLine(ImVec2(vecMax.x, vecMin.y), ImVec2(vecMax.x, vecMin.y + flCornerLength), colBox, flThickness);
                                        
                                        // Bottom-left corner
                                        Draw::AddLine(ImVec2(vecMin.x, vecMax.y), ImVec2(vecMin.x + flCornerLength, vecMax.y), colBox, flThickness);
                                        Draw::AddLine(ImVec2(vecMin.x, vecMax.y), ImVec2(vecMin.x, vecMax.y - flCornerLength), colBox, flThickness);
                                        
                                        // Bottom-right corner
                                        Draw::AddLine(ImVec2(vecMax.x, vecMax.y), ImVec2(vecMax.x - flCornerLength, vecMax.y), colBox, flThickness);
                                        Draw::AddLine(ImVec2(vecMax.x, vecMax.y), ImVec2(vecMax.x, vecMax.y - flCornerLength), colBox, flThickness);
                                    }
                                    else
                                    {
                                        // Draw normal box
                                        Draw::AddRect(vecMin, vecMax, colBox, DRAW_RECT_OUTLINE, Color(0, 0, 0, 255), 0.0f, flThickness);
                                    }
                                }
                                
                                // Draw Health Bar
                                if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bHealthBarEnabled))
                                {
                                    try
                                    {
                                        int iHealth = pPawn->m_iHealth();
                                        int iMaxHealth = pPawn->m_iMaxHealth();
                                        if (iMaxHealth > 0 && iHealth >= 0)
                                        {
                                            float flHealthPercent = std::clamp(static_cast<float>(iHealth) / static_cast<float>(iMaxHealth), 0.0f, 1.0f);
                                            
                                            float flBarWidth = 4.0f;
                                            float flBarHeight = flHeight * flHealthPercent;
                                            float flBarX = vecMin.x - 10.0f;
                                            float flBarY = vecMax.y - flBarHeight;
                                            
                                            // Validate bar position
                                            if (flBarY >= vecMin.y && flBarY <= vecMax.y && flBarX >= 0 && flBarX < vecDisplaySize.x)
                                            {
                                                // Health color (green to red gradient)
                                                Color colHealth;
                                                if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bOverrideHealthColorEnabled))
                                                {
                                                    colHealth = colBox;
                                                }
                                                else
                                                {
                                                    // Green to red based on health
                                                    int iRed = static_cast<int>(255.0f * (1.0f - flHealthPercent));
                                                    int iGreen = static_cast<int>(255.0f * flHealthPercent);
                                                    colHealth = Color(std::clamp(iRed, 0, 255), std::clamp(iGreen, 0, 255), 0, 255);
                                                }
                                                
                                                Draw::AddRect(ImVec2(flBarX, flBarY), ImVec2(flBarX + flBarWidth, vecMax.y), colHealth, DRAW_RECT_FILLED, Color(0, 0, 0, 255), 0.0f, 0.0f);
                                                Draw::AddRect(ImVec2(flBarX, flBarY), ImVec2(flBarX + flBarWidth, vecMax.y), Color(0, 0, 0, 255), DRAW_RECT_OUTLINE, Color(0, 0, 0, 0), 0.0f, 1.0f);
                                            }
                                        }
                                    }
                                    catch (...)
                                    {
                                        // Skip health bar if there's an error
                                    }
                                }
                                
                                // Draw Name
                                if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bNameEnabled))
                                {
                                    try
                                    {
                                        std::string strPlayerName = pController->m_strSanitizedPlayerName();
                                        if (!strPlayerName.empty() && strPlayerName.length() < 128)
                                        {
                                            ImVec2 vecNamePos = ImVec2(vecScreenHead.x, vecScreenHead.y - 20.0f);
                                            if (vecNamePos.y >= 0 && vecNamePos.y < vecDisplaySize.y)
                                            {
                                                if (Fonts::ESP != nullptr)
                                                {
                                                    Draw::AddText(Fonts::ESP, 10.0f, vecNamePos, strPlayerName, colText, DRAW_TEXT_OUTLINE, Color(0, 0, 0, 255));
                                                }
                                            }
                                        }
                                    }
                                    catch (...)
                                    {
                                        // Skip name if there's an error
                                    }
                                }
                                
                                // Draw Head Circle
                                if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bHeadCircleEnabled))
                                {
                                    float flHeadRadius = 15.0f;
                                    Draw::AddCircle(vecScreenHead, flHeadRadius, colBox, 32, DRAW_CIRCLE_OUTLINE, Color(0, 0, 0, 255), flThickness);
                                }
                                
                                // Draw Skeleton ESP
                                if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bSkeletonEspEnabled))
                                {
                                    try
                                    {
                                        // Get bone cache from gameSceneNode
                                        // CGameSceneNode has m_pBoneCache at offset 0x1F0
                                        BoneData_t* pBoneCache = pSceneNode->m_pBoneCache();
                                        
                                        if (!pBoneCache || pBoneCache == nullptr)
                                            continue;
                                        
                                        // CS2 bone indices
                                        const int BONE_HEAD = 6;
                                        const int BONE_NECK = 5;
                                        const int BONE_SPINE_1 = 4;
                                        const int BONE_SPINE_2 = 2;
                                        const int BONE_PELVIS = 0;
                                        const int BONE_ARM_UPPER_L = 8;
                                        const int BONE_ARM_LOWER_L = 9;
                                        const int BONE_HAND_L = 10;
                                        const int BONE_ARM_UPPER_R = 13;
                                        const int BONE_ARM_LOWER_R = 14;
                                        const int BONE_HAND_R = 15;
                                        const int BONE_LEG_UPPER_L = 22;
                                        const int BONE_LEG_LOWER_L = 23;
                                        const int BONE_ANKLE_L = 24;
                                        const int BONE_LEG_UPPER_R = 25;
                                        const int BONE_LEG_LOWER_R = 26;
                                        const int BONE_ANKLE_R = 27;
                                        
                                        // Helper function to get bone position
                                        // BoneData_t structure: Vector m_vecPosition (offset 0x0), float m_flScale (offset 0xC), Quaternion (offset 0x10)
                                        // Each bone is sizeof(BoneData_t) = 32 bytes (0x20)
                                        auto GetBonePosition = [&](int boneIndex) -> Vector {
                                            if (boneIndex < 0 || boneIndex >= 128)
                                                return Vector(0, 0, 0);
                                            
                                            try
                                            {
                                                // Calculate bone address: boneCache + (boneIndex * sizeof(BoneData_t))
                                                // sizeof(BoneData_t) = Vector(12) + float(4) + Quaternion(16) = 32 bytes = 0x20
                                                std::uintptr_t uBoneAddress = reinterpret_cast<std::uintptr_t>(pBoneCache) + (boneIndex * 0x20);
                                                
                                                // Read bone position directly (first member of BoneData_t)
                                                Vector vecBonePos = g_Memory.ReadMemory<Vector>(uBoneAddress);
                                                
                                                // Validate: check if position is reasonable
                                                if (std::abs(vecBonePos.x) > 50000.0f || std::abs(vecBonePos.y) > 50000.0f || std::abs(vecBonePos.z) > 50000.0f)
                                                    return Vector(0, 0, 0);
                                                
                                                return vecBonePos;
                                            }
                                            catch (...)
                                            {
                                                return Vector(0, 0, 0);
                                            }
                                        };
                                        
                                        // Get bone positions
                                        Vector vecHead = GetBonePosition(BONE_HEAD);
                                        Vector vecNeck = GetBonePosition(BONE_NECK);
                                        Vector vecSpine1 = GetBonePosition(BONE_SPINE_1);
                                        Vector vecSpine2 = GetBonePosition(BONE_SPINE_2);
                                        Vector vecPelvis = GetBonePosition(BONE_PELVIS);
                                        Vector vecArmUpperL = GetBonePosition(BONE_ARM_UPPER_L);
                                        Vector vecArmLowerL = GetBonePosition(BONE_ARM_LOWER_L);
                                        Vector vecHandL = GetBonePosition(BONE_HAND_L);
                                        Vector vecArmUpperR = GetBonePosition(BONE_ARM_UPPER_R);
                                        Vector vecArmLowerR = GetBonePosition(BONE_ARM_LOWER_R);
                                        Vector vecHandR = GetBonePosition(BONE_HAND_R);
                                        Vector vecLegUpperL = GetBonePosition(BONE_LEG_UPPER_L);
                                        Vector vecLegLowerL = GetBonePosition(BONE_LEG_LOWER_L);
                                        Vector vecAnkleL = GetBonePosition(BONE_ANKLE_L);
                                        Vector vecLegUpperR = GetBonePosition(BONE_LEG_UPPER_R);
                                        Vector vecLegLowerR = GetBonePosition(BONE_LEG_LOWER_R);
                                        Vector vecAnkleR = GetBonePosition(BONE_ANKLE_R);
                                        
                                        // Convert to screen coordinates and draw
                                        auto DrawBoneLine = [&](Vector vecBone1, Vector vecBone2) {
                                            // Validate bones are not zero
                                            if (vecBone1.x == 0.0f && vecBone1.y == 0.0f && vecBone1.z == 0.0f)
                                                return;
                                            if (vecBone2.x == 0.0f && vecBone2.y == 0.0f && vecBone2.z == 0.0f)
                                                return;
                                                
                                            ImVec2 vecScreen1, vecScreen2;
                                            if (Draw::WorldToScreen(vecBone1, vecScreen1) && Draw::WorldToScreen(vecBone2, vecScreen2))
                                            {
                                                Draw::AddLine(vecScreen1, vecScreen2, colSkeleton, flThickness);
                                            }
                                        };
                                        
                                        // Draw skeleton connections (matching C# BoneConnections)
                                        // Spine chain
                                        DrawBoneLine(vecHead, vecNeck);
                                        DrawBoneLine(vecNeck, vecSpine1);
                                        DrawBoneLine(vecSpine1, vecSpine2);
                                        DrawBoneLine(vecSpine2, vecPelvis);
                                        
                                        // Left arm chain
                                        DrawBoneLine(vecSpine1, vecArmUpperL);
                                        DrawBoneLine(vecArmUpperL, vecArmLowerL);
                                        DrawBoneLine(vecArmLowerL, vecHandL);
                                        
                                        // Right arm chain
                                        DrawBoneLine(vecSpine1, vecArmUpperR);
                                        DrawBoneLine(vecArmUpperR, vecArmLowerR);
                                        DrawBoneLine(vecArmLowerR, vecHandR);
                                        
                                        // Left leg chain
                                        DrawBoneLine(vecPelvis, vecLegUpperL);
                                        DrawBoneLine(vecLegUpperL, vecLegLowerL);
                                        DrawBoneLine(vecLegLowerL, vecAnkleL);
                                        
                                        // Right leg chain
                                        DrawBoneLine(vecPelvis, vecLegUpperR);
                                        DrawBoneLine(vecLegUpperR, vecLegLowerR);
                                        DrawBoneLine(vecLegLowerR, vecAnkleR);
                                    }
                                    catch (...)
                                    {
                                        // Skip skeleton if there's an error
                                    }
                                }
                                
                                // Draw Lines ESP
                                if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bLinesEspEnabled))
                                {
                                    ImVec2 vecOriginPoint;
                                    int iLinesOrigin = CONFIG_GET(int, g_Variables.m_PlayerVisuals.m_iLinesEspOrigin);
                                    
                                    switch (iLinesOrigin)
                                    {
                                    case ELinesEspOrigin::LINES_ESP_ORIGIN_TOP:
                                        vecOriginPoint = ImVec2(vecDisplaySize.x * 0.5f, 0.0f);
                                        break;
                                    case ELinesEspOrigin::LINES_ESP_ORIGIN_BOTTOM:
                                        vecOriginPoint = ImVec2(vecDisplaySize.x * 0.5f, vecDisplaySize.y);
                                        break;
                                    case ELinesEspOrigin::LINES_ESP_ORIGIN_CROSSHAIR:
                                    default:
                                        vecOriginPoint = ImVec2(vecDisplaySize.x * 0.5f, vecDisplaySize.y * 0.5f);
                                        break;
                                    }
                                    
                                    Draw::AddLine(vecOriginPoint, vecScreenOrigin, colLinesEsp, flThickness);
                                }
                            }
                            catch (...)
                            {
                                // Skip ESP rendering for this player if there's an error
                                continue;
                            }
                        }

                        break;
                    }

                }
            }
            
            // Draw Aimbot FOV Circle
            if (CONFIG_GET(bool, g_Variables.m_AimBot.m_bEnableAimbot) && 
                CONFIG_GET(bool, g_Variables.m_AimBot.m_bShowFov))
            {
                ImVec2 vecDisplaySize = ImGui::GetIO().DisplaySize;
                ImVec2 vecCenter = ImVec2(vecDisplaySize.x * 0.5f, vecDisplaySize.y * 0.5f);
                
                int iFovPixels = CONFIG_GET(int, g_Variables.m_AimBot.m_iAimbotFov);
                float flFovRadius = static_cast<float>(iFovPixels);
                
                if (flFovRadius > 0.0f && flFovRadius < vecDisplaySize.x && flFovRadius < vecDisplaySize.y)
                {
                    Color colFov = Color(255, 255, 255, 150); // White with transparency
                    Draw::AddCircle(vecCenter, flFovRadius, colFov, 64, DRAW_CIRCLE_OUTLINE, Color(0, 0, 0, 255), 1.0f);
                }
            }
        }

        // swap given data to safe container
        Draw::SwapDrawData();
    }
}

void MapParserThread()
{
    // set thread priority
    SetThreadPriorityWrapper();

    while (!g_Globals.m_bIsUnloading)
    {
        if (!g_Utilities.IsInGame())
        {
            // sleep to conserve system resources
            g_Utilities.Sleep(3000.f);
            continue;
        }

        std::string strMapName = g_Memory.ReadMemoryString(g_Interfaces.m_GlobalVars.m_uMapNameShort);
        if (strMapName.empty())
        {
            g_Utilities.Sleep(3000.f);
            continue;
        }

        g_MapParser.VerifyMapNameHash(strMapName);
        
        // sleep to conserve system resources
        g_Utilities.Sleep(3000.f);
    }
}

void TickThread()
{
    // set thread priority
    SetThreadPriorityWrapper();

    while (!g_Globals.m_bIsUnloading)
    {
        if (!g_Utilities.IsInGame())
        {
            // sleep to conserve system resources
            g_Utilities.Sleep(3000.f);
            continue;
        }

        if (!g_Memory.IsWindowInForeground(X("Counter-Strike 2")) || Gui::m_bOpen)
        {
            g_Utilities.Sleep(1000.0f);
            continue;
        }
        
        // Always run auto-calibration check (even if aimbot is disabled)
        // This ensures calibration happens when joining a game
        if (g_Utilities.IsInGame() && g_Globals.m_LocalPlayer.m_pPlayerPawn && g_Globals.m_LocalPlayer.m_pPlayerPawn->IsAlive())
        {
            g_Aimbot.AutoCalibrateOnGameStart();
        }
        
        if (CONFIG_GET(bool, g_Variables.m_AimBot.m_bEnableAimbot))
        {
            g_Aimbot.Run();
        }

        if (CONFIG_GET(bool, g_Variables.m_TriggerBot.m_bEnableTriggerbot))
        {
			// run triggerbot here
        }

        // sleep to conserve system resources
        g_Utilities.Sleep(INTERVAL_PER_TICK * 1000.0f);
    }
}

void UpdateThread()
{
    // set thread priority
    SetThreadPriorityWrapper();

    while (!g_Globals.m_bIsUnloading)
    {
        // update globals
        g_Globals.Update();
        // update interfaces
        g_Interfaces.Update();

        // sleep to conserve system resources
        g_Utilities.Sleep(1);
    }
}

__forceinline void CreateThreads()
{
    std::thread(&EntityThread).detach();
    std::thread(&RenderThread).detach();
    std::thread(&MapParserThread).detach();
    std::thread(&TickThread).detach();
    std::thread(&UpdateThread).detach();
}

bool MainLoop(LPVOID lpParameter)
{
    // Clear console screen - harmless operation
    system(X("cls"));

    #ifndef _DEBUG
    DetachConsole();
    #endif // !_DEBUG

    try
    {
        // initialize memory
        g_Memory.Initialize(X("cs2.exe"));

        // check if last module is loaded
        while (g_Memory.GetModule(NAVSYSTEM_DLL).m_uBaseAddress == 0U)
        {
			std::cout << X("Looking for navsystem.dll") << std::endl;
            g_Utilities.Sleep(500.0f);
        }

        // setup config system
        Config::Setup(X("default.json"));
        // initialize schema system
        SchemaSystem::Setup();

        // create our window
        if (!Window::m_bInitialized)
            Window::Create();

        // set window process priority
        SetPriorityClass(g_Globals.m_Instance, HIGH_PRIORITY_CLASS);
        // set program priority
        SetPriorityClass(g_Globals.m_hDll, HIGH_PRIORITY_CLASS);
		// set program priority
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

        // create all our threads
        CreateThreads();

        while (!g_Globals.m_bIsUnloading)
        {
            if (!Window::Render())
               return false;
        }
    }
    catch (const std::exception& ex)
    {
        #ifdef _DEBUG
        _RPT0(_CRT_ERROR, ex.what());
        #else
        // unload
        FreeLibraryAndExitThread(static_cast<HMODULE>(lpParameter), EXIT_FAILURE);
        #endif
    }

    return EXIT_SUCCESS;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPreviousInstance, LPSTR pArgs, int iCmdShow)
{
    ConsoleAttach(X("External Base"));
   
    g_Globals.m_hDll = hInstance;

    if (!MainLoop(hInstance))
    {
        // release handle
        g_Memory.~CMemory();

        // destroy context
        if (Window::m_bInitialized)
            Window::Destroy();
    }

    return EXIT_SUCCESS;
}