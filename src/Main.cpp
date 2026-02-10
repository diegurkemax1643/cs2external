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

        // Update at higher frequency (128 tick = ~7.8ms) to reduce ESP lag while maintaining performance
        // This is twice the game's tick rate, providing smoother ESP without being too expensive
        g_Utilities.Sleep((INTERVAL_PER_TICK * 0.5f) * 1000.0f);
    }
}

// Widget rendering function - renders draggable widgets
void RenderWidgets()
{
    if (!g_Memory.IsWindowInForeground(X("Counter-Strike 2")))
        return;

    ImDrawList* pDrawList = ImGui::GetBackgroundDrawList();
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 vecDisplaySize = io.DisplaySize;
    bool bMenuOpen = Gui::m_bOpen; // Only draggable when menu is open
    
    // Static variables for dragging state (shared across all widgets)
    static bool bDragging = false;
    static const char* szDraggingWidget = nullptr;
    static ImVec2 vecDragOffset(0, 0);
    static ImVec2 vecDragStartPos(0, 0);
    
    // Helper function to render a widget
    auto RenderWidget = [&](const char* szName, float& flX, float& flY, float flWidth, float flHeight, std::function<void(ImVec2, ImVec2)> renderContent)
    {
        ImVec2 vecPos(flX, flY);
        ImVec2 vecSize(flWidth, flHeight);
        ImVec2 vecMin = vecPos;
        ImVec2 vecMax = vecPos + vecSize;
        
        // Dragging logic (only when menu is open)
        if (bMenuOpen)
        {
            ImVec2 vecMousePos = io.MousePos;
            bool bHovered = vecMousePos.x >= vecMin.x && vecMousePos.x <= vecMax.x &&
                           vecMousePos.y >= vecMin.y && vecMousePos.y <= vecMax.y;
            
            // Start dragging
            if (bHovered && io.MouseDown[0] && !bDragging)
            {
                bDragging = true;
                szDraggingWidget = szName;
                vecDragOffset = vecMousePos - vecMin;
            }
            
            // Continue dragging
            if (bDragging && szDraggingWidget == szName)
            {
                if (io.MouseDown[0])
                {
                    vecPos = vecMousePos - vecDragOffset;
                    // Clamp to screen bounds
                    vecPos.x = std::max(0.0f, std::min(vecPos.x, vecDisplaySize.x - vecSize.x));
                    vecPos.y = std::max(0.0f, std::min(vecPos.y, vecDisplaySize.y - vecSize.y));
                    flX = vecPos.x;
                    flY = vecPos.y;
                    vecMin = vecPos;
                    vecMax = vecPos + vecSize;
                }
                else
                {
                    // Stop dragging
                    bDragging = false;
                    szDraggingWidget = nullptr;
                }
            }
        }
        else if (!bMenuOpen && bDragging && szDraggingWidget == szName)
        {
            // Stop dragging if menu closes
            bDragging = false;
            szDraggingWidget = nullptr;
        }
        
        // Black transparent rounded background
        ImU32 colBackground = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.7f)); // Black with 70% opacity
        pDrawList->AddRectFilled(vecMin, vecMax, colBackground, 8.0f); // 8px rounded corners
        
        // Border (highlight when dragging)
        ImU32 colBorder = (bDragging && szDraggingWidget == szName) ? 
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.765f, 1.0f, 0.8f)) : // Cyan when dragging
            ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.3f)); // White border with 30% opacity
        pDrawList->AddRect(vecMin, vecMax, colBorder, 8.0f, 0, 1.5f);
        
        // Render widget content
        renderContent(vecMin, vecMax);
    };
    
    // FPS Widget
    if (CONFIG_GET(bool, g_Variables.m_Widgets.m_bFpsWidgetEnabled))
    {
        float& flX = CONFIG_GET(float, g_Variables.m_Widgets.m_flFpsWidgetX);
        float& flY = CONFIG_GET(float, g_Variables.m_Widgets.m_flFpsWidgetY);
        
        RenderWidget("FPS", flX, flY, 120.0f, 40.0f, [&](ImVec2 vecMin, ImVec2 vecMax)
        {
            // Calculate FPS
            static float flLastTime = 0.0f;
            static int iFrameCount = 0;
            static float flFPS = 0.0f;
            
            float flCurrentTime = ImGui::GetTime();
            iFrameCount++;
            
            if (flCurrentTime - flLastTime >= 1.0f)
            {
                flFPS = static_cast<float>(iFrameCount) / (flCurrentTime - flLastTime);
                iFrameCount = 0;
                flLastTime = flCurrentTime;
            }
            
            // Draw FPS text
            ImVec2 vecTextPos(vecMin.x + 10.0f, vecMin.y + 12.0f);
            char szFpsText[32];
            sprintf_s(szFpsText, "FPS: %.0f", flFPS);
            pDrawList->AddText(vecTextPos, IM_COL32(255, 255, 255, 255), szFpsText);
        });
    }
    
    // Bomb Timer Widget
    if (CONFIG_GET(bool, g_Variables.m_Widgets.m_bBombTimerWidgetEnabled))
    {
        float& flX = CONFIG_GET(float, g_Variables.m_Widgets.m_flBombTimerWidgetX);
        float& flY = CONFIG_GET(float, g_Variables.m_Widgets.m_flBombTimerWidgetY);
        
        RenderWidget("BombTimer", flX, flY, 180.0f, 60.0f, [&](ImVec2 vecMin, ImVec2 vecMax)
        {
            // Get bomb entity if planted
            std::string strBombText = "Bomb: Not Planted";
            ImU32 colText = IM_COL32(255, 255, 255, 255);
            
            // Check if bomb is planted using offset
            if (g_Globals.m_Offsets.m_uPlantedC4 != 0)
            {
                std::uintptr_t uPlantedC4Address = g_Memory.ReadMemory<std::uintptr_t>(g_Globals.m_Offsets.m_uPlantedC4);
                if (uPlantedC4Address != 0)
                {
                    strBombText = "Bomb: Planted";
                    colText = IM_COL32(255, 0, 0, 255); // Red when planted
                }
            }
            
            ImVec2 vecTextPos(vecMin.x + 10.0f, vecMin.y + 15.0f);
            pDrawList->AddText(vecTextPos, colText, strBombText.c_str());
        });
    }
    
    // Player Count Widget
    if (CONFIG_GET(bool, g_Variables.m_Widgets.m_bPlayerCountWidgetEnabled))
    {
        float& flX = CONFIG_GET(float, g_Variables.m_Widgets.m_flPlayerCountWidgetX);
        float& flY = CONFIG_GET(float, g_Variables.m_Widgets.m_flPlayerCountWidgetY);
        
        RenderWidget("PlayerCount", flX, flY, 150.0f, 50.0f, [&](ImVec2 vecMin, ImVec2 vecMax)
        {
            // Count players
            std::shared_lock lock(EntityList::m_mtxEntities);
            int iEnemyCount = 0;
            int iTeamCount = 0;
            
            if (g_Globals.m_LocalPlayer.m_pPlayerPawn)
            {
                std::uint8_t uLocalTeam = g_Globals.m_LocalPlayer.m_pPlayerPawn->m_iTeamNum();
                
                for (const auto& entity : EntityList::m_vecEntities)
                {
                    if (entity.m_eType == EEntityType::ENTITY_PLAYER)
                    {
                        CCSPlayerController* pController = reinterpret_cast<CCSPlayerController*>(entity.m_pEntity);
                        if (!pController || pController->m_bIsLocalPlayerController())
                            continue;
                            
                        C_CSPlayerPawn* pPawn = reinterpret_cast<C_CSPlayerPawn*>(pController->m_hPawn().Get());
                        if (!pPawn || !pPawn->IsAlive())
                            continue;
                            
                        std::uint8_t uPlayerTeam = pPawn->m_iTeamNum();
                        if (uPlayerTeam == uLocalTeam && uLocalTeam != 0)
                            iTeamCount++;
                        else if (uPlayerTeam != uLocalTeam)
                            iEnemyCount++;
                    }
                }
            }
            
            char szPlayerText[64];
            sprintf_s(szPlayerText, "Enemies: %d\nTeam: %d", iEnemyCount, iTeamCount);
            
            ImVec2 vecTextPos(vecMin.x + 10.0f, vecMin.y + 12.0f);
            pDrawList->AddText(vecTextPos, IM_COL32(255, 255, 255, 255), szPlayerText);
        });
    }
    
    // Health & Armor Widget
    if (CONFIG_GET(bool, g_Variables.m_Widgets.m_bHealthArmorWidgetEnabled))
    {
        float& flX = CONFIG_GET(float, g_Variables.m_Widgets.m_flHealthArmorWidgetX);
        float& flY = CONFIG_GET(float, g_Variables.m_Widgets.m_flHealthArmorWidgetY);
        
        RenderWidget("HealthArmor", flX, flY, 150.0f, 50.0f, [&](ImVec2 vecMin, ImVec2 vecMax)
        {
            if (g_Globals.m_LocalPlayer.m_pPlayerPawn)
            {
                int iHealth = g_Globals.m_LocalPlayer.m_pPlayerPawn->m_iHealth();
                int iArmor = g_Globals.m_LocalPlayer.m_pPlayerPawn->m_ArmorValue();
                
                char szHealthText[64];
                sprintf_s(szHealthText, "HP: %d\nArmor: %d", iHealth, iArmor);
                
                ImU32 colHealth = (iHealth > 50) ? IM_COL32(0, 255, 0, 255) : 
                                  (iHealth > 25) ? IM_COL32(255, 255, 0, 255) : 
                                  IM_COL32(255, 0, 0, 255);
                
                ImVec2 vecTextPos(vecMin.x + 10.0f, vecMin.y + 12.0f);
                pDrawList->AddText(vecTextPos, colHealth, szHealthText);
            }
            else
            {
                ImVec2 vecTextPos(vecMin.x + 10.0f, vecMin.y + 12.0f);
                pDrawList->AddText(vecTextPos, IM_COL32(255, 255, 255, 255), "Not in game");
            }
        });
    }
    
    // Weapon & Ammo Widget
    if (CONFIG_GET(bool, g_Variables.m_Widgets.m_bWeaponAmmoWidgetEnabled))
    {
        float& flX = CONFIG_GET(float, g_Variables.m_Widgets.m_flWeaponAmmoWidgetX);
        float& flY = CONFIG_GET(float, g_Variables.m_Widgets.m_flWeaponAmmoWidgetY);
        
        RenderWidget("WeaponAmmo", flX, flY, 150.0f, 50.0f, [&](ImVec2 vecMin, ImVec2 vecMax)
        {
            if (g_Globals.m_LocalPlayer.m_pPlayerPawn)
            {
                CPlayer_WeaponServices* pWeaponServices = g_Globals.m_LocalPlayer.m_pPlayerPawn->m_pWeaponServices();
                if (pWeaponServices)
                {
                    C_BasePlayerWeapon* pWeapon = pWeaponServices->m_hActiveWeapon().Get();
                    if (pWeapon)
                    {
                        int iClip1 = pWeapon->m_iClip1();
                        // Get reserve ammo
                        int iReserveAmmo = pWeapon->m_pReserveAmmo();
                        
                        char szAmmoText[64];
                        sprintf_s(szAmmoText, "Ammo: %d/%d", iClip1, iReserveAmmo);
                        
                        ImVec2 vecTextPos(vecMin.x + 10.0f, vecMin.y + 12.0f);
                        pDrawList->AddText(vecTextPos, IM_COL32(255, 255, 255, 255), szAmmoText);
                    }
                    else
                    {
                        ImVec2 vecTextPos(vecMin.x + 10.0f, vecMin.y + 12.0f);
                        pDrawList->AddText(vecTextPos, IM_COL32(255, 255, 255, 255), "No weapon");
                    }
                }
            }
            else
            {
                ImVec2 vecTextPos(vecMin.x + 10.0f, vecMin.y + 12.0f);
                pDrawList->AddText(vecTextPos, IM_COL32(255, 255, 255, 255), "Not in game");
            }
        });
    }
    
    // Spectator List Widget
    if (CONFIG_GET(bool, g_Variables.m_Widgets.m_bSpectatorListWidgetEnabled))
    {
        float& flX = CONFIG_GET(float, g_Variables.m_Widgets.m_flSpectatorListWidgetX);
        float& flY = CONFIG_GET(float, g_Variables.m_Widgets.m_flSpectatorListWidgetY);
        
        RenderWidget("SpectatorList", flX, flY, 200.0f, 100.0f, [&](ImVec2 vecMin, ImVec2 vecMax)
        {
            // Count spectators (simplified - you may need to implement actual spectator detection)
            ImVec2 vecTextPos(vecMin.x + 10.0f, vecMin.y + 12.0f);
            pDrawList->AddText(vecTextPos, IM_COL32(255, 255, 255, 255), "Spectators: 0");
        });
    }
    
    // Behind Enemy Indicator Widget
    if (CONFIG_GET(bool, g_Variables.m_Widgets.m_bBehindEnemyIndicatorWidgetEnabled))
    {
        float& flX = CONFIG_GET(float, g_Variables.m_Widgets.m_flBehindEnemyIndicatorWidgetX);
        float& flY = CONFIG_GET(float, g_Variables.m_Widgets.m_flBehindEnemyIndicatorWidgetY);
        
        // Default to top-right if X is 0 (auto-position)
        // Once user drags it, X will be non-zero and this won't recalculate
        if (flX == 0.0f && vecDisplaySize.x > 0.0f)
        {
            flX = vecDisplaySize.x - 150.0f; // Position from right edge
            if (flY == 10.0f) // Only set Y if it's still at default
                flY = 10.0f; // Top of screen
        }
        
        RenderWidget("BehindEnemyIndicator", flX, flY, 120.0f, 40.0f, [&](ImVec2 vecMin, ImVec2 vecMax)
        {
            int iBehindEnemyCount = 0;
            
            if (g_Globals.m_LocalPlayer.m_pPlayerPawn && g_Globals.m_LocalPlayer.m_pPlayerPawn->IsAlive())
            {
                // Get local player position and view angles
                CGameSceneNode* pLocalSceneNode = g_Globals.m_LocalPlayer.m_pPlayerPawn->m_pGameSceneNode();
                if (pLocalSceneNode)
                {
                    Vector vecLocalOrigin = pLocalSceneNode->m_vecAbsOrigin();
                    QAngle angViewAngle = g_Interfaces.m_CSGOInput.m_angViewAngle;
                    
                    // Get forward direction from view angles
                    Vector vecForward;
                    angViewAngle.ToDirections(&vecForward, nullptr, nullptr);
                    
                    // Get local player team
                    std::uint8_t uLocalTeam = g_Globals.m_LocalPlayer.m_pPlayerPawn->m_iTeamNum();
                    
                    // Check all entities
                    std::shared_lock lock(EntityList::m_mtxEntities);
                    for (const auto& entity : EntityList::m_vecEntities)
                    {
                        if (entity.m_eType == EEntityType::ENTITY_PLAYER)
                        {
                            CCSPlayerController* pController = reinterpret_cast<CCSPlayerController*>(entity.m_pEntity);
                            if (!pController || pController->m_bIsLocalPlayerController())
                                continue;
                            
                            C_CSPlayerPawn* pPawn = reinterpret_cast<C_CSPlayerPawn*>(pController->m_hPawn().Get());
                            if (!pPawn || !pPawn->IsAlive())
                                continue;
                            
                            // Check if enemy (different team)
                            std::uint8_t uPlayerTeam = pPawn->m_iTeamNum();
                            if (uPlayerTeam == uLocalTeam || uLocalTeam == 0)
                                continue;
                            
                            // Get enemy position
                            CGameSceneNode* pEnemySceneNode = pPawn->m_pGameSceneNode();
                            if (!pEnemySceneNode)
                                continue;
                            
                            Vector vecEnemyOrigin = pEnemySceneNode->m_vecAbsOrigin();
                            
                            // Check if enemy is behind the player
                            Vector vecToEnemy = vecEnemyOrigin - vecLocalOrigin;
                            float flDistance = vecToEnemy.Length();
                            
                            if (flDistance < 0.1f) // Too close, skip
                                continue;
                            
                            // Normalize direction to enemy
                            vecToEnemy = vecToEnemy / flDistance;
                            
                            // Calculate dot product: positive = enemy in front, negative = enemy behind
                            float flDotProduct = vecForward.DotProduct(vecToEnemy);
                            
                            // Check if enemy is behind player (negative dot product means opposite direction)
                            if (flDotProduct < -0.1f) // Behind player
                            {
                                iBehindEnemyCount++;
                            }
                        }
                    }
                }
            }
            
            // Draw count text
            char szCountText[32];
            sprintf_s(szCountText, "Behind: %d", iBehindEnemyCount);
            
            ImVec2 vecTextPos(vecMin.x + 10.0f, vecMin.y + 12.0f);
            ImU32 colText = (iBehindEnemyCount > 0) ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 255, 255, 255); // Red if enemies behind, white if none
            pDrawList->AddText(vecTextPos, colText, szCountText);
        });
    }
}

void RenderThread()
{
    // set thread priority
    SetThreadPriorityWrapper();

    // Frame limiting for render thread
    constexpr float flRenderTargetFPS = 240.0f; // Match 240Hz monitor
    constexpr float flRenderFrameTime = 1000.0f / flRenderTargetFPS;
    auto flLastRenderTime = std::chrono::high_resolution_clock::now();

    while (!g_Globals.m_bIsUnloading)
    {
        // Frame limiting - don't render faster than target FPS
        auto flCurrentTime = std::chrono::high_resolution_clock::now();
        auto flElapsed = std::chrono::duration_cast<std::chrono::microseconds>(flCurrentTime - flLastRenderTime).count() / 1000.0f;
        
        if (flElapsed < flRenderFrameTime)
        {
            float flSleepTime = flRenderFrameTime - flElapsed;
            if (flSleepTime > 0.0f)
                g_Utilities.Sleep(flSleepTime);
            continue;
        }
        
        flLastRenderTime = std::chrono::high_resolution_clock::now();
        
        // clear data from previous call
        Draw::ClearDrawData();

        if (!g_Memory.IsWindowInForeground(X("Counter-Strike 2")))
        {
            // "safe" way of clearing
            Draw::SwapDrawData();

            g_Utilities.Sleep(1000.0f);
            continue;
        }

        // Update view matrix right before rendering for accurate world-to-screen (lightweight operation)
        // This ensures the view matrix is fresh without the expensive entity update
        if (g_Utilities.IsInGame())
        {
            g_Globals.m_matViewMatrix = g_Memory.ReadMemory<ViewMatrix_t>(g_Globals.m_Offsets.m_uViewMatrix);
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
                                
                                // Validate origin - check for invalid positions
                                // Reject if exactly zero OR if values are unreasonably large (invalid memory read)
                                if ((vecOrigin.x == 0.0f && vecOrigin.y == 0.0f && vecOrigin.z == 0.0f) ||
                                    std::abs(vecOrigin.x) > 100000.0f || 
                                    std::abs(vecOrigin.y) > 100000.0f || 
                                    std::abs(vecOrigin.z) > 100000.0f)
                                    continue;
                                
                                // Get head position - use bone cache for accurate head position if available
                                Vector vecHead = vecOrigin;
                                bool bUseBoneHead = false;
                                
                                // Try to get actual head bone position (bone index 6 = head)
                                BoneData_t* pBoneCache = pSceneNode->m_pBoneCache();
                                if (pBoneCache)
                                {
                                    try
                                    {
                                        std::uintptr_t uHeadBoneAddress = reinterpret_cast<std::uintptr_t>(pBoneCache) + (6 * sizeof(BoneData_t));
                                        BoneData_t headBoneData = g_Memory.ReadMemory<BoneData_t>(uHeadBoneAddress);
                                        Vector vecHeadBone = headBoneData.m_vecPosition;
                                        if (!vecHeadBone.IsZero() && 
                                            std::abs(vecHeadBone.x) < 50000.0f && 
                                            std::abs(vecHeadBone.y) < 50000.0f && 
                                            std::abs(vecHeadBone.z) < 50000.0f)
                                        {
                                            vecHead = vecHeadBone;
                                            bUseBoneHead = true;
                                        }
                                    }
                                    catch (...)
                                    {
                                        // Fallback to approximate head position
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
                            
                            // Validate screen coordinates
                            if (vecDisplaySize.x <= 0.0f || vecDisplaySize.y <= 0.0f)
                                continue;
                            
                            // Relaxed validation: allow ESP to render if at least part of the player is on screen
                            // Check if either origin or head is on screen (allows partial visibility)
                            bool bOriginOnScreen = (vecScreenOrigin.x >= -100.0f && vecScreenOrigin.x <= vecDisplaySize.x + 100.0f &&
                                                   vecScreenOrigin.y >= -100.0f && vecScreenOrigin.y <= vecDisplaySize.y + 100.0f);
                            bool bHeadOnScreen = (vecScreenHead.x >= -100.0f && vecScreenHead.x <= vecDisplaySize.x + 100.0f &&
                                                 vecScreenHead.y >= -100.0f && vecScreenHead.y <= vecDisplaySize.y + 100.0f);
                            
                            // Skip only if both are completely off-screen
                            if (!bOriginOnScreen && !bHeadOnScreen)
                                continue;
                            
                            // Calculate box dimensions based on actual player size on screen
                            // Box should match player's visual size (scales with distance naturally)
                            float flHeight = vecScreenOrigin.y - vecScreenHead.y;
                            if (flHeight <= 0.0f || flHeight > vecDisplaySize.y)
                                continue; // Skip if height calculation fails
                            
                            // Make box width proportional to height (narrow rectangle)
                            float flWidth = flHeight * 0.4f; // Width is 40% of height
                            
                            // Calculate box corners - box goes from top of head to bottom of feet
                            ImVec2 vecMin = ImVec2(vecScreenHead.x - flWidth * 0.5f, vecScreenHead.y);
                            ImVec2 vecMax = ImVec2(vecScreenHead.x + flWidth * 0.5f, vecScreenOrigin.y);
                            
                            // Get colors from config
                            Color colBoxBase = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colESPBox);
                            Color colLine = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colESPLine);
                            Color colText = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colESPText);
                            Color colSkeleton = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colSkeletonEsp);
                            Color colLinesEsp = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colLinesEsp);
                            
                            // Get glow colors for glow outline ESP
                            Color colGlowEnemy = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colGlowEnemy);
                            Color colGlowTeam = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colGlowTeam);
                            
                            // Visibility check for color change (only for local player's view)
                            bool bIsVisible = false;
                            if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bVisibilityIndicatorEnabled))
                            {
                                try
                                {
                                    // Check if local player can see this enemy
                                    if (g_Globals.m_LocalPlayer.m_pPlayerPawn)
                                    {
                                        Vector vecLocalEyePos = g_Globals.m_LocalPlayer.m_pPlayerPawn->GetEyePosition();
                                        
                                        // Check spotted mask directly for local player
                                        EntitySpottedState_t spottedState = pPawn->m_entitySpottedState();
                                        int localIndex = g_Globals.m_LocalPlayer.m_pPlayerPawn->GetRefEHandle().GetEntryIndex();
                                        
                                        // Check if local player has spotted this enemy using the bitmask
                                        // The mask array has 2 uint32_t values (64 bits total)
                                        if (localIndex >= 0 && localIndex < 64)
                                        {
                                            int maskIndex = localIndex / 32;
                                            int bitIndex = localIndex % 32;
                                            if (maskIndex < 2)
                                            {
                                                bIsVisible = (spottedState.m_bSpottedByMask[maskIndex] & (1U << bitIndex)) != 0;
                                            }
                                        }
                                        
                                        // If map parser is available, also do a raycast check for more accuracy
                                        if (g_MapParser.m_bSetup)
                                        {
                                            bool bMapVisible = g_MapParser.IsVisible(vecLocalEyePos, vecHead);
                                            // Use map check if available (more accurate)
                                            bIsVisible = bMapVisible;
                                        }
                                        
                                        // Update ESP color based on visibility
                                        if (bIsVisible)
                                        {
                                            colBoxBase = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colVisibleEnemy);
                                        }
                                        else
                                        {
                                            colBoxBase = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colHiddenEnemy);
                                        }
                                    }
                                }
                                catch (...)
                                {
                                    // Keep default color if visibility check fails
                                }
                            }
                            
                            // Get ESP settings
                            int iThickness = CONFIG_GET(int, g_Variables.m_PlayerVisuals.m_iEspThickness);
                            float flThickness = std::clamp(static_cast<float>(iThickness), 1.0f, 10.0f);
                            
                            // Apply transparency
                            int iTransparency = CONFIG_GET(int, g_Variables.m_PlayerVisuals.m_iEspTransparency);
                            float flAlpha = std::clamp(static_cast<float>(iTransparency) / 100.0f, 0.0f, 1.0f);
                            uint8_t uAlpha = static_cast<uint8_t>(std::clamp(static_cast<int>(colBoxBase.a() * flAlpha), 0, 255));
                            Color colBox = colBoxBase;
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
                                        // Draw cornered box with proportional corner length
                                        // Corner length should be proportional to box size to maintain shape
                                        float flCornerLength = std::min(flWidth, flHeight) * 0.3f; // 30% of smaller dimension
                                        flCornerLength = std::max(flCornerLength, 6.0f); // Minimum 6px to ensure visibility
                                        
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
                                
                                // Draw Chams ESP (bone-based filled player outline visible through walls)
                                if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bChamsEspEnabled))
                                {
                                    try
                                    {
                                        // Determine if player is visible or hidden
                                        bool bIsVisible = false;
                                        if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bVisibilityIndicatorEnabled))
                                        {
                                            try
                                            {
                                                if (g_Globals.m_LocalPlayer.m_pPlayerPawn)
                                                {
                                                    Vector vecLocalEyePos = g_Globals.m_LocalPlayer.m_pPlayerPawn->GetEyePosition();
                                                    
                                                    EntitySpottedState_t spottedState = pPawn->m_entitySpottedState();
                                                    int localIndex = g_Globals.m_LocalPlayer.m_pPlayerPawn->GetRefEHandle().GetEntryIndex();
                                                    
                                                    if (localIndex >= 0 && localIndex < 64)
                                                    {
                                                        int maskIndex = localIndex / 32;
                                                        int bitIndex = localIndex % 32;
                                                        if (maskIndex < 2)
                                                        {
                                                            bIsVisible = (spottedState.m_bSpottedByMask[maskIndex] & (1U << bitIndex)) != 0;
                                                        }
                                                    }
                                                    
                                                    if (g_MapParser.m_bSetup)
                                                    {
                                                        bool bMapVisible = g_MapParser.IsVisible(vecLocalEyePos, vecHead);
                                                        bIsVisible = bMapVisible;
                                                    }
                                                }
                                            }
                                            catch (...)
                                            {
                                                // Keep default visibility state
                                            }
                                        }
                                        
                                        // Check if we should only show chams when player is behind walls
                                        bool bOnlyThroughWalls = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bChamsOnlyThroughWalls);
                                        bool bShouldShowChams = !bOnlyThroughWalls || !bIsVisible;
                                        
                                        if (bShouldShowChams)
                                        {
                                            // Get chams colors based on visibility
                                            Color colChams = bIsVisible ? 
                                                CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colChamsVisible) : 
                                                CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colChamsHidden);
                                            
                                            // Apply opacity setting
                                            float flOpacity = CONFIG_GET(float, g_Variables.m_PlayerVisuals.m_flChamsOpacity);
                                            flOpacity = std::clamp(flOpacity, 0.0f, 1.0f);
                                            uint8_t uChamsAlpha = static_cast<uint8_t>(std::clamp(static_cast<int>(colChams.a() * flOpacity), 0, 255));
                                            colChams.Set(colChams.r(), colChams.g(), colChams.b(), uChamsAlpha);
                                            
                                            // EFFICIENT BONE READING: Using dwBoneMatrix and m_pStudioHdr method
                                            std::uintptr_t uPawnAddr = reinterpret_cast<std::uintptr_t>(pPawn);
                                            
                                            // Read bone matrix array once
                                            std::array<Matrix3x4_t, 128> boneMatrix = {};
                                            bool bBoneMatrixRead = false;
                                            
                                            try {
                                                // Read bone matrix - try direct offset first (as per user's method)
                                                // If that fails, try reading as pointer
                                                std::uintptr_t uBoneMatrixAddr = uPawnAddr + C_CSPlayerPawn::dwBoneMatrix;
                                                
                                                // First try: read directly from offset (user's method: entity + offsets::dwBoneMatrix)
                                                try {
                                                    boneMatrix = g_Memory.ReadMemory<std::array<Matrix3x4_t, 128>>(uBoneMatrixAddr);
                                                    // Validate first bone position is reasonable (X, Y, Z from matrix columns)
                                                    const auto& firstBone = boneMatrix[0];
                                                    Vector testPos(firstBone[0][3], firstBone[1][3], firstBone[2][3]);
                                                    if (!testPos.IsZero() && std::abs(testPos.x) < 50000.0f && 
                                                        std::abs(testPos.y) < 50000.0f && std::abs(testPos.z) < 50000.0f)
                                                    {
                                                        bBoneMatrixRead = true;
                                                    }
                                                }
                                                catch (...) {}
                                                
                                                // Second try: read as pointer (if direct read failed or invalid)
                                                if (!bBoneMatrixRead)
                                                {
                                                    try {
                                                        std::uintptr_t uBoneMatrixPtr = g_Memory.ReadMemory<std::uintptr_t>(uBoneMatrixAddr);
                                                        if (uBoneMatrixPtr > 0x10000 && uBoneMatrixPtr < 0x7FFFFFFFFFFF)
                                                        {
                                                            boneMatrix = g_Memory.ReadMemory<std::array<Matrix3x4_t, 128>>(uBoneMatrixPtr);
                                                            const auto& firstBone = boneMatrix[0];
                                                            Vector testPos(firstBone[0][3], firstBone[1][3], firstBone[2][3]);
                                                            if (!testPos.IsZero() && std::abs(testPos.x) < 50000.0f && 
                                                                std::abs(testPos.y) < 50000.0f && std::abs(testPos.z) < 50000.0f)
                                                            {
                                                                bBoneMatrixRead = true;
                                                            }
                                                        }
                                                    }
                                                    catch (...) {}
                                                }
                                            }
                                            catch (...) {}
                                            
                                            // Get bone position from matrix array
                                            auto GetBonePos = [&](const std::array<Matrix3x4_t, 128>& bonematrix, int idx) -> Vector {
                                                if (idx < 0 || idx >= 128)
                                                    return Vector(0, 0, 0);
                                                
                                                try {
                                                    const auto& bone = bonematrix.at(idx);
                                                    return Vector(bone[0][3], bone[1][3], bone[2][3]);
                                                }
                                                catch (...) {}
                                                
                                                return Vector(0, 0, 0);
                                            };
                                            
                                            // Get StudioHdr pointer
                                            std::uintptr_t dwStudioHdr = 0;
                                            studiohdr_t pStudioHdr = {};
                                            bool bStudioHdrRead = false;
                                            
                                            try {
                                                dwStudioHdr = g_Memory.ReadMemory<std::uintptr_t>(uPawnAddr + C_CSPlayerPawn::m_pStudioHdr);
                                                if (dwStudioHdr > 0x10000 && dwStudioHdr < 0x7FFFFFFFFFFF)
                                                {
                                                    pStudioHdr = g_Memory.ReadMemory<studiohdr_t>(dwStudioHdr);
                                                    if (pStudioHdr.numbones > 0 && pStudioHdr.numbones <= 128)
                                                    {
                                                        bStudioHdrRead = true;
                                                    }
                                                }
                                            }
                                            catch (...) {}
                                            
                                            // Draw filled chams using bone connections from studiohdr
                                            if (bBoneMatrixRead && bStudioHdrRead)
                                            {
                                                std::vector<ImVec2> vecChamsPolygon;
                                                
                                                // Iterate through all bones and draw connections
                                                for (int i = 0; i < pStudioHdr.numbones; i++)
                                                {
                                                    try {
                                                        uintptr_t dwBone = dwStudioHdr + pStudioHdr.boneindex + (i * sizeof(mstudiobone_t));
                                                        mstudiobone_t bone = g_Memory.ReadMemory<mstudiobone_t>(dwBone);
                                                        
                                                        // Check if bone is valid and has a parent
                                                        if (dwBone && (bone.flags & 0x00000100) && (bone.parent != -1))
                                                        {
                                                            Vector vChild = GetBonePos(boneMatrix, i);
                                                            Vector vParent = GetBonePos(boneMatrix, bone.parent);
                                                            
                                                            ImVec2 svChild, svParent;
                                                            if (Draw::WorldToScreen(vChild, svChild) && Draw::WorldToScreen(vParent, svParent))
                                                            {
                                                                // Add points to form a polygon for filled chams
                                                                vecChamsPolygon.push_back(svChild);
                                                                vecChamsPolygon.push_back(svParent);
                                                            }
                                                        }
                                                    }
                                                    catch (...) {}
                                                }
                                                
                                                // Draw filled polygon if we have enough points
                                                if (vecChamsPolygon.size() >= 3)
                                                {
                                                    // Draw filled chams
                                                    Draw::AddPolygon(vecChamsPolygon, colChams, DRAW_POLYGON_FILLED, Color(0, 0, 0, 0), true, 0.0f);
                                                    
                                                    // Draw outline
                                                    Color colChamsOutline = colChams;
                                                    colChamsOutline.Set(colChamsOutline.r(), colChamsOutline.g(), colChamsOutline.b(), 
                                                        static_cast<uint8_t>(std::min(255, static_cast<int>(uChamsAlpha * 1.3f))));
                                                    Draw::AddPolygon(vecChamsPolygon, colChamsOutline, DRAW_POLYGON_OUTLINE, Color(0, 0, 0, 0), false, 1.5f);
                                                }
                                            }
                                            else
                                            {
                                                // Fallback: Use box coordinates if bone reading fails
                                                std::vector<ImVec2> vecBoxChams;
                                                vecBoxChams.push_back(ImVec2(vecMin.x, vecMin.y));
                                                vecBoxChams.push_back(ImVec2(vecMax.x, vecMin.y));
                                                vecBoxChams.push_back(ImVec2(vecMax.x, vecMax.y));
                                                vecBoxChams.push_back(ImVec2(vecMin.x, vecMax.y));
                                                
                                                Draw::AddPolygon(vecBoxChams, colChams, DRAW_POLYGON_FILLED, Color(0, 0, 0, 0), true, 0.0f);
                                                
                                                Color colChamsOutline = colChams;
                                                colChamsOutline.Set(colChamsOutline.r(), colChamsOutline.g(), colChamsOutline.b(), 
                                                    static_cast<uint8_t>(std::min(255, static_cast<int>(uChamsAlpha * 1.3f))));
                                                Draw::AddPolygon(vecBoxChams, colChamsOutline, DRAW_POLYGON_OUTLINE, Color(0, 0, 0, 0), false, 1.5f);
                                            }
                                        }
                                    }
                                    catch (...)
                                    {
                                        // Skip chams rendering if there's an error
                                    }
                                }
                                
                                // Draw Filled Body ESP (uses bone positions to create exact body outline and fill it)
                                if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bBodyFilledEsp))
                                {
                                    try
                                    {
                                        Color colBodyFill = colBox;
                                        // Use higher alpha for better visibility (but not too high to see through)
                                        uint8_t uBodyFillAlpha = static_cast<uint8_t>(std::clamp(static_cast<int>(colBodyFill.a() * 0.5f), 0, 255));
                                        colBodyFill.Set(colBodyFill.r(), colBodyFill.g(), colBodyFill.b(), uBodyFillAlpha);
                                        
                                        // Get bone positions to create exact body outline (same method as skeleton ESP)
                                        CGameSceneNode* pSceneNode = pPawn->m_pGameSceneNode();
                                        if (pSceneNode)
                                        {
                                            BoneData_t* pBoneCache = pSceneNode->m_pBoneCache();
                                            if (pBoneCache)
                                            {
                                                // Read entire BoneData_t structure, not just position
                                                auto GetBonePos = [&](int boneIdx) -> Vector {
                                                    if (boneIdx < 0 || boneIdx >= 64) return Vector(0,0,0);
                                                    std::uintptr_t uBoneAddr = reinterpret_cast<std::uintptr_t>(pBoneCache) + (boneIdx * sizeof(BoneData_t));
                                                    try
                                                    {
                                                        BoneData_t boneData = g_Memory.ReadMemory<BoneData_t>(uBoneAddr);
                                                        return boneData.m_vecPosition;
                                                    }
                                                    catch (...)
                                                    {
                                                        return Vector(0,0,0);
                                                    }
                                                };
                                                
                                                // Use the same bone indices as skeleton ESP (which already works)
                                                enum BoneIndex {
                                                    pelvis = 0,
                                                    spine_2 = 3,
                                                    spine_1 = 4,
                                                    neck_0 = 5,
                                                    head = 6,
                                                    arm_upper_L = 13,
                                                    arm_lower_L = 14,
                                                    hand_L = 15,
                                                    arm_upper_R = 8,
                                                    arm_lower_R = 9,
                                                    hand_R = 10,
                                                    leg_upper_L = 23,
                                                    leg_lower_L = 24,
                                                    ankle_L = 25,
                                                    leg_upper_R = 30,
                                                    leg_lower_R = 31,
                                                    ankle_R = 32
                                                };
                                                
                                                // Get bone positions and convert to screen coordinates
                                                // We'll create a polygon that follows the body outline
                                                std::vector<ImVec2> vecBodyOutline;
                                                
                                                // Helper function to add bone to outline if valid
                                                auto AddBoneToOutline = [&](int boneIdx) -> bool {
                                                    Vector vecBone3D = GetBonePos(boneIdx);
                                                    if (!vecBone3D.IsZero())
                                                    {
                                                        ImVec2 vecBoneScreen;
                                                        if (Draw::WorldToScreen(vecBone3D, vecBoneScreen))
                                                        {
                                                            vecBodyOutline.push_back(vecBoneScreen);
                                                            return true;
                                                        }
                                                    }
                                                    return false;
                                                };
                                                
                                                // Build body outline polygon in clockwise order (for proper filling)
                                                // Top: head
                                                if (!AddBoneToOutline(head))
                                                    continue; // Need head for valid outline
                                                
                                                // Left side: head -> left shoulder -> left elbow -> left hand
                                                AddBoneToOutline(arm_upper_L);
                                                AddBoneToOutline(arm_lower_L);
                                                AddBoneToOutline(hand_L);
                                                
                                                // Bottom: left hand -> left ankle -> right ankle -> right hand
                                                AddBoneToOutline(ankle_L);
                                                AddBoneToOutline(ankle_R);
                                                AddBoneToOutline(hand_R);
                                                
                                                // Right side: right hand -> right elbow -> right shoulder -> back to head
                                                AddBoneToOutline(arm_lower_R);
                                                AddBoneToOutline(arm_upper_R);
                                                
                                                // If we have enough points (at least 4), draw filled polygon
                                                if (vecBodyOutline.size() >= 4)
                                                {
                                                    // Ensure polygon is closed (add first point at end if needed)
                                                    if (vecBodyOutline.size() > 0)
                                                    {
                                                        // Check if already closed (first and last point are same)
                                                        float flDistX = std::abs(vecBodyOutline[0].x - vecBodyOutline.back().x);
                                                        float flDistY = std::abs(vecBodyOutline[0].y - vecBodyOutline.back().y);
                                                        if (flDistX > 1.0f || flDistY > 1.0f)
                                                        {
                                                            // Not closed, add first point at end
                                                            vecBodyOutline.push_back(vecBodyOutline[0]);
                                                        }
                                                    }
                                                    
                                                    // Draw filled polygon representing exact body outline
                                                    Draw::AddPolygon(vecBodyOutline, colBodyFill, DRAW_POLYGON_FILLED, Color(0, 0, 0, 0), true, 0.0f);
                                                }
                                            }
                                        }
                                    }
                                    catch (...)
                                    {
                                        // Silently fail if bone reading fails
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
                                float flTextOffset = 0.0f;
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
                                                    flTextOffset += 15.0f;
                                                }
                                            }
                                        }
                                    }
                                    catch (...)
                                    {
                                        // Skip name if there's an error
                                    }
                                }
                                
                                // Draw Distance
                                if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bDistanceEspEnabled))
                                {
                                    try
                                    {
                                        if (g_Globals.m_LocalPlayer.m_pPlayerPawn)
                                        {
                                            Vector vecLocalOrigin = g_Globals.m_LocalPlayer.m_pPlayerPawn->m_pGameSceneNode()->m_vecAbsOrigin();
                                            Vector vecEnemyOrigin = vecOrigin;
                                            Vector vecDistance = vecEnemyOrigin - vecLocalOrigin;
                                            float flDistance = vecDistance.Length();
                                            
                                            std::string strDistance = std::format("{:.0f}m", flDistance);
                                            ImVec2 vecDistancePos = ImVec2(vecScreenHead.x, vecScreenHead.y - 20.0f - flTextOffset);
                                            if (vecDistancePos.y >= 0 && vecDistancePos.y < vecDisplaySize.y)
                                            {
                                                if (Fonts::ESP != nullptr)
                                                {
                                                    Draw::AddText(Fonts::ESP, 10.0f, vecDistancePos, strDistance, colText, DRAW_TEXT_OUTLINE, Color(0, 0, 0, 255));
                                                    flTextOffset += 15.0f;
                                                }
                                            }
                                        }
                                    }
                                    catch (...)
                                    {
                                        // Skip distance if there's an error
                                    }
                                }
                                
                                // Draw Flashed Indicator
                                if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bFlashedEspEnabled))
                                {
                                    try
                                    {
                                        // Check if player is flashed by checking flash duration
                                        float flFlashDuration = pPawn->m_flFlashDuration();
                                        if (flFlashDuration > 0.0f)
                                        {
                                            // Draw "flashed" text at player's feet (origin position)
                                            ImVec2 vecFlashedPos = ImVec2(vecScreenOrigin.x, vecScreenOrigin.y + 5.0f); // Slightly below feet
                                            if (vecFlashedPos.y >= 0 && vecFlashedPos.y < vecDisplaySize.y)
                                            {
                                                if (Fonts::ESP != nullptr)
                                                {
                                                    // Use yellow/orange color for flashed indicator
                                                    Color colFlashed = Color(255, 255, 0, 255); // Yellow
                                                    Draw::AddText(Fonts::ESP, 10.0f, vecFlashedPos, "flashed", colFlashed, DRAW_TEXT_OUTLINE, Color(0, 0, 0, 255));
                                                }
                                            }
                                        }
                                    }
                                    catch (...)
                                    {
                                        // Skip flashed indicator if there's an error
                                    }
                                }
                                
                                // Draw Weapon
                                if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bWeaponEspEnabled))
                                {
                                    try
                                    {
                                        CCSPlayer_WeaponServices* pWeaponServices = pPawn->m_pWeaponServices();
                                        if (pWeaponServices)
                                        {
                                            C_BasePlayerWeapon* pWeapon = pWeaponServices->m_hActiveWeapon().Get();
                                            if (pWeapon)
                                            {
                                                std::string strWeaponName = pPawn->m_strActiveWeaponName();
                                                if (!strWeaponName.empty())
                                                {
                                                    // Clean up weapon name (remove "weapon_" prefix if present)
                                                    if (strWeaponName.find("weapon_") == 0)
                                                        strWeaponName = strWeaponName.substr(7);
                                                    
                                                    // Capitalize first letter
                                                    if (!strWeaponName.empty())
                                                        strWeaponName[0] = std::toupper(strWeaponName[0]);
                                                    
                                                    ImVec2 vecWeaponPos = ImVec2(vecScreenHead.x, vecScreenHead.y - 20.0f - flTextOffset);
                                                    if (vecWeaponPos.y >= 0 && vecWeaponPos.y < vecDisplaySize.y)
                                                    {
                                                        if (Fonts::ESP != nullptr)
                                                        {
                                                            Draw::AddText(Fonts::ESP, 10.0f, vecWeaponPos, strWeaponName, colText, DRAW_TEXT_OUTLINE, Color(0, 0, 0, 255));
                                                            flTextOffset += 15.0f;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    catch (...)
                                    {
                                        // Skip weapon if there's an error
                                    }
                                }
                                
                                // Draw Angle Lines (show direction player is looking)
                                if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bAngleLinesEnabled))
                                {
                                    try
                                    {
                                        // Get player's view angle
                                        QAngle angEyeAngles = pPawn->m_angEyeAngles();
                                        
                                        // Convert angle to forward direction vector
                                        Vector vecForward;
                                        angEyeAngles.ToDirections(&vecForward, nullptr, nullptr);
                                        
                                        // Calculate line end point in 3D space (50 units forward from head)
                                        float flLineLength = 50.0f;
                                        Vector vecLineEnd3D = vecHead + (vecForward * flLineLength);
                                        
                                        // Convert to screen coordinates
                                        ImVec2 vecLineEndScreen;
                                        if (Draw::WorldToScreen(vecLineEnd3D, vecLineEndScreen))
                                        {
                                            Color colAngleLine = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colLinesEsp);
                                            Draw::AddLine(vecScreenHead, vecLineEndScreen, colAngleLine, 2.0f);
                                        }
                                    }
                                    catch (...)
                                    {
                                        // Skip angle lines if there's an error
                                    }
                                }
                                
                                // Draw Head Circle (fixed small size, doesn't scale with distance)
                                if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bHeadCircleEnabled))
                                {
                                    // Fixed small radius - always the same size regardless of distance
                                    float flHeadRadius = 8.0f; // Small fixed size
                                    Draw::AddCircle(vecScreenHead, flHeadRadius, colBox, 32, DRAW_CIRCLE_OUTLINE, Color(0, 0, 0, 255), flThickness);
                                }
                                
                                // ============================================================================
                                // ADVANCED BONE DEBUGGING FUNCTIONS
                                // ============================================================================
                                
                                // DiagnoseBoneCache - Scans ALL possible offsets automatically
                                auto DiagnoseBoneCache = [&](CGameSceneNode* pSceneNode) -> void {
                                    if (!pSceneNode)
                                        return;
                                        
                                    std::uintptr_t uSceneNodeAddr = reinterpret_cast<std::uintptr_t>(pSceneNode);
                                    
                                    // Bone cache diagnosis (logging removed)
                                    const std::uintptr_t cacheOffsets[] = { 
                                        0x1E0, 0x1F0, 0x200, 0x210, 0x220, 0x230, 0x240, 0x250,
                                        0x260, 0x270, 0x280, 0x290, 0x2A0, 0x2B0, 0x2C0, 0x2D0,
                                        0x300, 0x320, 0x340, 0x360, 0x380, 0x3A0, 0x3C0, 0x3E0,
                                        0x400, 0x420, 0x440, 0x460, 0x480, 0x4A0, 0x4C0, 0x4E0,
                                        0x500, 0x520, 0x540, 0x560, 0x580, 0x5A0, 0x5C0, 0x5E0
                                    };
                                    
                                    for (const auto& offset : cacheOffsets)
                                    {
                                        try {
                                            std::uintptr_t uPotentialCache = g_Memory.ReadMemory<std::uintptr_t>(uSceneNodeAddr + offset);
                                            
                                            if (uPotentialCache > 0x10000 && uPotentialCache < 0x7FFFFFFFFFFF)
                                            {
                                                const std::size_t strides[] = { 0x20, 0x30, 0x40, 0x48, 0x50, 0x60 };
                                                
                                                for (const auto& stride : strides)
                                                {
                                                    try {
                                                        std::uintptr_t uHeadBone = uPotentialCache + (6 * stride);
                                                        Matrix3x4_t matrix = g_Memory.ReadMemory<Matrix3x4_t>(uHeadBone);
                                                        Vector pos(matrix[0][3], matrix[1][3], matrix[2][3]);
                                                        
                                                        if (!pos.IsZero() && 
                                                            std::abs(pos.x) < 50000.0f && 
                                                            std::abs(pos.y) < 50000.0f && 
                                                            std::abs(pos.z) < 50000.0f)
                                                        {
                                                            // Valid bone cache found (logging removed)
                                                        }
                                                    }
                                                    catch (...) {}
                                                }
                                            }
                                        }
                                        catch (...) {}
                                    }
                                    
                                };
                                
                                // GetBonePos_ModelState - Alternative via CModelState
                                auto GetBonePos_ModelState = [&](CGameSceneNode* pSceneNode, int boneIdx) -> Vector {
                                    if (!pSceneNode || boneIdx < 0 || boneIdx >= 64)
                                        return Vector(0, 0, 0);
                                    
                                    try {
                                        std::uintptr_t uSceneNodeAddr = reinterpret_cast<std::uintptr_t>(pSceneNode);
                                        const std::uintptr_t modelStateOffsets[] = { 0x160, 0x170, 0x180, 0x190 };
                                        
                                        for (const auto& msOffset : modelStateOffsets)
                                        {
                                            try {
                                                std::uintptr_t uModelState = g_Memory.ReadMemory<std::uintptr_t>(uSceneNodeAddr + msOffset);
                                                
                                                if (uModelState > 0x10000)
                                                {
                                                    const std::uintptr_t boneArrayOffsets[] = { 0x80, 0xA0, 0xC0, 0xE0 };
                                                    
                                                    for (const auto& baOffset : boneArrayOffsets)
                                                    {
                                                        try {
                                                            std::uintptr_t uBoneArray = g_Memory.ReadMemory<std::uintptr_t>(uModelState + baOffset);
                                                            
                                                            if (uBoneArray > 0x10000)
                                                            {
                                                                std::uintptr_t uBoneAddr = uBoneArray + (boneIdx * 0x30);
                                                                Matrix3x4_t matrix = g_Memory.ReadMemory<Matrix3x4_t>(uBoneAddr);
                                                                Vector pos(matrix[0][3], matrix[1][3], matrix[2][3]);
                                                                
                                                                if (!pos.IsZero() && 
                                                                    std::abs(pos.x) < 50000.0f && 
                                                                    std::abs(pos.y) < 50000.0f && 
                                                                    std::abs(pos.z) < 50000.0f)
                                                                {
                                                                    return pos;
                                                                }
                                                            }
                                                        }
                                                        catch (...) {}
                                                    }
                                                }
                                            }
                                            catch (...) {}
                                        }
                                    }
                                    catch (...) {}
                                    
                                    return Vector(0, 0, 0);
                                };
                                
                                // GetHitboxPosition - Hitbox-based fallback (most reliable)
                                auto GetHitboxPosition = [&](C_CSPlayerPawn* pPawn, EHitBoxes hitbox) -> Vector {
                                    if (!pPawn || hitbox < 0 || hitbox >= EHitBoxes::HITBOX_MAX)
                                        return Vector(0, 0, 0);
                                    
                                    try {
                                        std::uintptr_t uPawnAddr = reinterpret_cast<std::uintptr_t>(pPawn);
                                        CGameSceneNode* pSceneNode = pPawn->m_pGameSceneNode();
                                        if (!pSceneNode)
                                            return Vector(0, 0, 0);
                                        
                                        // Approximate hitbox positions based on origin (fallback method)
                                        Vector vecOrigin = pSceneNode->m_vecAbsOrigin();
                                        
                                        switch (hitbox)
                                        {
                                        case EHitBoxes::HITBOX_HEAD:
                                            return vecOrigin + Vector(0, 0, 72.0f);
                                        case EHitBoxes::HITBOX_NECK:
                                            return vecOrigin + Vector(0, 0, 65.0f);
                                        case EHitBoxes::HITBOX_CHEST:
                                        case EHitBoxes::HITBOX_UPPER_CHEST:
                                            return vecOrigin + Vector(0, 0, 50.0f);
                                        case EHitBoxes::HITBOX_PELVIS:
                                            return vecOrigin + Vector(0, 0, 10.0f);
                                        default:
                                            return vecOrigin;
                                        }
                                    }
                                    catch (...) {}
                                    
                                    return Vector(0, 0, 0);
                                };
                                
                                // ============================================================================
                                
                                // Draw Skeleton ESP
                                if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bSkeletonEspEnabled))
                                {
                                    try
                                    {
                                        // Enum für die wichtigsten Bones (ggf. anpassen)
                                        enum BoneIndex {
                                            pelvis = 0,
                                            spine_2 = 3,
                                            spine_1 = 4,
                                            neck_0 = 5,
                                            head = 6,
                                            arm_upper_L = 13,
                                            arm_lower_L = 14,
                                            hand_L = 15,
                                            arm_upper_R = 8,
                                            arm_lower_R = 9,
                                            hand_R = 10,
                                            leg_upper_L = 23,
                                            leg_lower_L = 24,
                                            ankle_L = 25,
                                            leg_upper_R = 30,
                                            leg_lower_R = 31,
                                            ankle_R = 32
                                        };
                                        constexpr std::pair<int, int> boneConnections[] = {
                                            {head, neck_0},
                                            {neck_0, spine_1},
                                            {spine_1, spine_2},
                                            {spine_2, pelvis},
                                            {spine_1, arm_upper_L},
                                            {arm_upper_L, arm_lower_L},
                                            {arm_lower_L, hand_L},
                                            {spine_1, arm_upper_R},
                                            {arm_upper_R, arm_lower_R},
                                            {arm_lower_R, hand_R},
                                            {pelvis, leg_upper_L},
                                            {leg_upper_L, leg_lower_L},
                                            {leg_lower_L, ankle_L},
                                            {pelvis, leg_upper_R},
                                            {leg_upper_R, leg_lower_R},
                                            {leg_lower_R, ankle_R}
                                        };
                                        // EFFICIENT BONE READING: Using dwBoneMatrix and m_pStudioHdr method
                                        std::uintptr_t uPawnAddr = reinterpret_cast<std::uintptr_t>(pPawn);
                                        
                                        // Read bone matrix array once
                                        std::array<Matrix3x4_t, 128> boneMatrix = {};
                                        bool bBoneMatrixRead = false;
                                        
                                        try {
                                            // Read bone matrix - try direct offset first (as per user's method)
                                            // If that fails, try reading as pointer
                                            std::uintptr_t uBoneMatrixAddr = uPawnAddr + C_CSPlayerPawn::dwBoneMatrix;
                                            
                                            // First try: read directly from offset (user's method: entity + offsets::dwBoneMatrix)
                                            try {
                                                boneMatrix = g_Memory.ReadMemory<std::array<Matrix3x4_t, 128>>(uBoneMatrixAddr);
                                                // Validate first bone position is reasonable (X, Y, Z from matrix columns)
                                                const auto& firstBone = boneMatrix[0];
                                                Vector testPos(firstBone[0][3], firstBone[1][3], firstBone[2][3]);
                                                if (!testPos.IsZero() && std::abs(testPos.x) < 50000.0f && 
                                                    std::abs(testPos.y) < 50000.0f && std::abs(testPos.z) < 50000.0f)
                                                {
                                                    bBoneMatrixRead = true;
                                                }
                                            }
                                            catch (...) {}
                                            
                                            // Second try: read as pointer (if direct read failed or invalid)
                                            if (!bBoneMatrixRead)
                                            {
                                                try {
                                                    std::uintptr_t uBoneMatrixPtr = g_Memory.ReadMemory<std::uintptr_t>(uBoneMatrixAddr);
                                                    if (uBoneMatrixPtr > 0x10000 && uBoneMatrixPtr < 0x7FFFFFFFFFFF)
                                                    {
                                                        boneMatrix = g_Memory.ReadMemory<std::array<Matrix3x4_t, 128>>(uBoneMatrixPtr);
                                                        const auto& firstBone = boneMatrix[0];
                                                        Vector testPos(firstBone[0][3], firstBone[1][3], firstBone[2][3]);
                                                        if (!testPos.IsZero() && std::abs(testPos.x) < 50000.0f && 
                                                            std::abs(testPos.y) < 50000.0f && std::abs(testPos.z) < 50000.0f)
                                                        {
                                                            bBoneMatrixRead = true;
                                                        }
                                                    }
                                                }
                                                catch (...) {}
                                            }
                                        }
                                        catch (...) {}
                                        
                                        // Get bone position from matrix array
                                        auto GetBonePos = [&](const std::array<Matrix3x4_t, 128>& bonematrix, int idx) -> Vector {
                                            if (idx < 0 || idx >= 128)
                                                return Vector(0, 0, 0);
                                            
                                            try {
                                                const auto& bone = bonematrix.at(idx);
                                                return Vector(bone[0][3], bone[1][3], bone[2][3]);
                                            }
                                            catch (...) {}
                                            
                                            return Vector(0, 0, 0);
                                        };
                                        
                                        // Get StudioHdr pointer
                                        std::uintptr_t dwStudioHdr = 0;
                                        studiohdr_t pStudioHdr = {};
                                        bool bStudioHdrRead = false;
                                        
                                        try {
                                            dwStudioHdr = g_Memory.ReadMemory<std::uintptr_t>(uPawnAddr + C_CSPlayerPawn::m_pStudioHdr);
                                            if (dwStudioHdr > 0x10000 && dwStudioHdr < 0x7FFFFFFFFFFF)
                                            {
                                                pStudioHdr = g_Memory.ReadMemory<studiohdr_t>(dwStudioHdr);
                                                if (pStudioHdr.numbones > 0 && pStudioHdr.numbones <= 128)
                                                {
                                                    bStudioHdrRead = true;
                                                }
                                            }
                                        }
                                        catch (...) {}
                                        
                                        // Draw skeleton using bone connections from studiohdr
                                        if (bBoneMatrixRead && bStudioHdrRead)
                                        {
                                            int iLinesDrawn = 0;
                                            
                                            // Iterate through all bones and draw connections
                                            for (int i = 0; i < pStudioHdr.numbones; i++)
                                            {
                                                try {
                                                    uintptr_t dwBone = dwStudioHdr + pStudioHdr.boneindex + (i * sizeof(mstudiobone_t));
                                                    mstudiobone_t bone = g_Memory.ReadMemory<mstudiobone_t>(dwBone);
                                                    
                                                    // Check if bone is valid and has a parent
                                                    if (dwBone && (bone.flags & 0x00000100) && (bone.parent != -1))
                                                    {
                                                        Vector vChild = GetBonePos(boneMatrix, i);
                                                        Vector vParent = GetBonePos(boneMatrix, bone.parent);
                                                        
                                                        ImVec2 svChild, svParent;
                                                        if (Draw::WorldToScreen(vChild, svChild) && Draw::WorldToScreen(vParent, svParent))
                                                        {
                                                            Draw::AddLine(svChild, svParent, colSkeleton, 2.0f);
                                                            iLinesDrawn++;
                                                        }
                                                    }
                                                }
                                                catch (...) {}
                                            }
                                        }
                                    }
                                    catch (...)
                                    {
                                        // Exception in skeleton drawing (logging removed)
                                    }
                                }
                                
                                // Glow Outline ESP removed - only real in-game glow is used (matches C# implementation)
                                
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

                    case EEntityType::ENTITY_CHICKEN:
                    {
                        // Chicken ESP
                        if (CONFIG_GET(bool, g_Variables.m_FunEsp.m_bChickenEspEnabled))
                        {
                            try
                            {
                                C_BaseEntity* pChicken = object.m_pEntity;
                                if (!pChicken)
                                    break;

                                CGameSceneNode* pSceneNode = pChicken->m_pGameSceneNode();
                                if (!pSceneNode)
                                    break;

                                Vector vecOrigin = pSceneNode->m_vecAbsOrigin();
                                
                                // Validate origin
                                if (std::abs(vecOrigin.x) > 100000.0f || 
                                    std::abs(vecOrigin.y) > 100000.0f || 
                                    std::abs(vecOrigin.z) > 100000.0f)
                                    break;

                                // Convert to screen coordinates
                                ImVec2 vecScreenOrigin, vecScreenTop;
                                Vector vecTop = vecOrigin;
                                vecTop.z += 20.0f; // Chicken height approximation
                                
                                if (!Draw::WorldToScreen(vecOrigin, vecScreenOrigin) || !Draw::WorldToScreen(vecTop, vecScreenTop))
                                    break;

                                // Get display size
                                ImVec2 vecDisplaySize = ImGui::GetIO().DisplaySize;

                                // Check if chicken is on screen
                                bool bOnScreen = (vecScreenOrigin.x >= -100.0f && vecScreenOrigin.x <= vecDisplaySize.x + 100.0f &&
                                                   vecScreenOrigin.y >= -100.0f && vecScreenOrigin.y <= vecDisplaySize.y + 100.0f);
                                
                                if (!bOnScreen)
                                    break;

                                // Calculate box dimensions
                                float flHeight = vecScreenOrigin.y - vecScreenTop.y;
                                float flWidth = flHeight * 0.6f; // Chicken is roughly 0.6:1 width to height ratio

                                // Calculate box corners
                                ImVec2 vecMin = ImVec2(vecScreenTop.x - flWidth * 0.5f, vecScreenTop.y);
                                ImVec2 vecMax = ImVec2(vecScreenTop.x + flWidth * 0.5f, vecScreenOrigin.y);

                                // Get chicken ESP settings
                                Color colChicken = CONFIG_GET(Color, g_Variables.m_FunEsp.m_colChickenEsp);
                                int iThickness = CONFIG_GET(int, g_Variables.m_FunEsp.m_iChickenEspThickness);

                                // Draw box around chicken
                                Draw::AddRect(vecMin, vecMax, colChicken, DRAW_RECT_OUTLINE, Color(0, 0, 0, 255), 0.0f, static_cast<float>(iThickness));
                            }
                            catch (...)
                            {
                                // Skip chicken ESP if there's an error
                                break;
                            }
                        }

                        break;
                    }

                    case EEntityType::ENTITY_BOMB:
                    {
                        // Planted Bomb ESP
                        if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bBombEspEnabled) && 
                            CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bBombEspShowPlanted))
                        {
                            try
                            {
                                C_PlantedC4* pBomb = reinterpret_cast<C_PlantedC4*>(object.m_pEntity);
                                if (!pBomb)
                                    break;

                                // Check if bomb has exploded or been defused
                                if (pBomb->m_bHasExploded() || pBomb->m_bBombDefused())
                                    break;

                                CGameSceneNode* pSceneNode = pBomb->m_pGameSceneNode();
                                if (!pSceneNode)
                                    break;

                                Vector vecOrigin = pSceneNode->m_vecAbsOrigin();
                                
                                // Validate origin
                                if ((vecOrigin.x == 0.0f && vecOrigin.y == 0.0f && vecOrigin.z == 0.0f) ||
                                    std::abs(vecOrigin.x) > 100000.0f || 
                                    std::abs(vecOrigin.y) > 100000.0f || 
                                    std::abs(vecOrigin.z) > 100000.0f)
                                    break;

                                // Convert to screen coordinates
                                ImVec2 vecScreenOrigin, vecScreenTop;
                                Vector vecTop = vecOrigin;
                                vecTop.z += 15.0f; // Bomb height approximation
                                
                                if (!Draw::WorldToScreen(vecOrigin, vecScreenOrigin) || !Draw::WorldToScreen(vecTop, vecScreenTop))
                                    break;

                                // Get display size
                                ImVec2 vecDisplaySize = ImGui::GetIO().DisplaySize;

                                // Check if bomb is on screen
                                bool bOnScreen = (vecScreenOrigin.x >= -100.0f && vecScreenOrigin.x <= vecDisplaySize.x + 100.0f &&
                                                   vecScreenOrigin.y >= -100.0f && vecScreenOrigin.y <= vecDisplaySize.y + 100.0f);
                                
                                if (!bOnScreen)
                                    break;

                                // Calculate box dimensions
                                float flHeight = vecScreenOrigin.y - vecScreenTop.y;
                                if (flHeight <= 0.0f)
                                    flHeight = 30.0f; // Default height if calculation fails
                                float flWidth = flHeight * 0.8f; // Bomb is roughly square

                                // Calculate box corners
                                ImVec2 vecMin = ImVec2(vecScreenTop.x - flWidth * 0.5f, vecScreenTop.y);
                                ImVec2 vecMax = ImVec2(vecScreenTop.x + flWidth * 0.5f, vecScreenOrigin.y);

                                // Get bomb ESP settings
                                Color colBomb = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colBombEsp);
                                int iThickness = CONFIG_GET(int, g_Variables.m_PlayerVisuals.m_iEspThickness);
                                float flThickness = std::clamp(static_cast<float>(iThickness), 1.0f, 10.0f);

                                // Apply transparency
                                int iTransparency = CONFIG_GET(int, g_Variables.m_PlayerVisuals.m_iEspTransparency);
                                float flAlpha = std::clamp(static_cast<float>(iTransparency) / 100.0f, 0.0f, 1.0f);
                                uint8_t uAlpha = static_cast<uint8_t>(std::clamp(static_cast<int>(colBomb.a() * flAlpha), 0, 255));
                                colBomb.Set(colBomb.r(), colBomb.g(), colBomb.b(), uAlpha);

                                // Draw box around bomb
                                Draw::AddRect(vecMin, vecMax, colBomb, DRAW_RECT_OUTLINE, Color(0, 0, 0, 255), 0.0f, flThickness);
                            }
                            catch (...)
                            {
                                // Skip bomb ESP if there's an error
                                break;
                            }
                        }

                        break;
                    }

                    case EEntityType::ENTITY_DROPPED_BOMB:
                    {
                        // Dropped Bomb ESP
                        if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bBombEspEnabled) && 
                            CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bBombEspShowDropped))
                        {
                            try
                            {
                                C_BasePlayerWeapon* pWeapon = reinterpret_cast<C_BasePlayerWeapon*>(object.m_pEntity);
                                if (!pWeapon)
                                    break;

                                CGameSceneNode* pSceneNode = pWeapon->m_pGameSceneNode();
                                if (!pSceneNode)
                                    break;

                                Vector vecOrigin = pSceneNode->m_vecAbsOrigin();
                                
                                // Validate origin
                                if ((vecOrigin.x == 0.0f && vecOrigin.y == 0.0f && vecOrigin.z == 0.0f) ||
                                    std::abs(vecOrigin.x) > 100000.0f || 
                                    std::abs(vecOrigin.y) > 100000.0f || 
                                    std::abs(vecOrigin.z) > 100000.0f)
                                    break;

                                // Convert to screen coordinates
                                ImVec2 vecScreenOrigin, vecScreenTop;
                                Vector vecTop = vecOrigin;
                                vecTop.z += 10.0f; // Dropped bomb height approximation
                                
                                if (!Draw::WorldToScreen(vecOrigin, vecScreenOrigin) || !Draw::WorldToScreen(vecTop, vecScreenTop))
                                    break;

                                // Get display size
                                ImVec2 vecDisplaySize = ImGui::GetIO().DisplaySize;

                                // Check if bomb is on screen
                                bool bOnScreen = (vecScreenOrigin.x >= -100.0f && vecScreenOrigin.x <= vecDisplaySize.x + 100.0f &&
                                                   vecScreenOrigin.y >= -100.0f && vecScreenOrigin.y <= vecDisplaySize.y + 100.0f);
                                
                                if (!bOnScreen)
                                    break;

                                // Calculate box dimensions
                                float flHeight = vecScreenOrigin.y - vecScreenTop.y;
                                if (flHeight <= 0.0f)
                                    flHeight = 25.0f; // Default height if calculation fails
                                float flWidth = flHeight * 0.8f; // Bomb is roughly square

                                // Calculate box corners
                                ImVec2 vecMin = ImVec2(vecScreenTop.x - flWidth * 0.5f, vecScreenTop.y);
                                ImVec2 vecMax = ImVec2(vecScreenTop.x + flWidth * 0.5f, vecScreenOrigin.y);

                                // Get bomb ESP settings
                                Color colBomb = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colBombEsp);
                                int iThickness = CONFIG_GET(int, g_Variables.m_PlayerVisuals.m_iEspThickness);
                                float flThickness = std::clamp(static_cast<float>(iThickness), 1.0f, 10.0f);

                                // Apply transparency
                                int iTransparency = CONFIG_GET(int, g_Variables.m_PlayerVisuals.m_iEspTransparency);
                                float flAlpha = std::clamp(static_cast<float>(iTransparency) / 100.0f, 0.0f, 1.0f);
                                uint8_t uAlpha = static_cast<uint8_t>(std::clamp(static_cast<int>(colBomb.a() * flAlpha), 0, 255));
                                colBomb.Set(colBomb.r(), colBomb.g(), colBomb.b(), uAlpha);

                                // Draw box around dropped bomb
                                Draw::AddRect(vecMin, vecMax, colBomb, DRAW_RECT_OUTLINE, Color(0, 0, 0, 255), 0.0f, flThickness);
                            }
                            catch (...)
                            {
                                // Skip bomb ESP if there's an error
                                break;
                            }
                        }

                        break;
                    }

                    case EEntityType::ENTITY_GRENADE:
                    {
                        // Grenade ESP
                        if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bGrenadeEspEnabled))
                        {
                            try
                            {
                                C_BaseEntity* pGrenade = object.m_pEntity;
                                if (!pGrenade)
                                    break;

                                CGameSceneNode* pSceneNode = pGrenade->m_pGameSceneNode();
                                if (!pSceneNode)
                                    break;

                                Vector vecOrigin = pSceneNode->m_vecAbsOrigin();
                                
                                // Validate origin
                                if ((vecOrigin.x == 0.0f && vecOrigin.y == 0.0f && vecOrigin.z == 0.0f) ||
                                    std::abs(vecOrigin.x) > 100000.0f || 
                                    std::abs(vecOrigin.y) > 100000.0f || 
                                    std::abs(vecOrigin.z) > 100000.0f)
                                    break;

                                // Convert to screen coordinates
                                ImVec2 vecScreenOrigin, vecScreenTop;
                                Vector vecTop = vecOrigin;
                                vecTop.z += 5.0f; // Grenade height approximation
                                
                                if (!Draw::WorldToScreen(vecOrigin, vecScreenOrigin) || !Draw::WorldToScreen(vecTop, vecScreenTop))
                                    break;

                                // Get display size
                                ImVec2 vecDisplaySize = ImGui::GetIO().DisplaySize;

                                // Check if grenade is on screen
                                bool bOnScreen = (vecScreenOrigin.x >= -100.0f && vecScreenOrigin.x <= vecDisplaySize.x + 100.0f &&
                                                   vecScreenOrigin.y >= -100.0f && vecScreenOrigin.y <= vecDisplaySize.y + 100.0f);
                                
                                if (!bOnScreen)
                                    break;

                                // Calculate box dimensions
                                float flHeight = vecScreenOrigin.y - vecScreenTop.y;
                                if (flHeight <= 0.0f)
                                    flHeight = 20.0f; // Default height if calculation fails
                                float flWidth = flHeight * 0.8f; // Grenade is roughly square

                                // Calculate box corners
                                ImVec2 vecMin = ImVec2(vecScreenTop.x - flWidth * 0.5f, vecScreenTop.y);
                                ImVec2 vecMax = ImVec2(vecScreenTop.x + flWidth * 0.5f, vecScreenOrigin.y);

                                // Get grenade ESP settings
                                Color colGrenade = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colGrenadeEsp);
                                int iThickness = CONFIG_GET(int, g_Variables.m_PlayerVisuals.m_iEspThickness);
                                float flThickness = std::clamp(static_cast<float>(iThickness), 1.0f, 10.0f);

                                // Apply transparency
                                int iTransparency = CONFIG_GET(int, g_Variables.m_PlayerVisuals.m_iEspTransparency);
                                float flAlpha = std::clamp(static_cast<float>(iTransparency) / 100.0f, 0.0f, 1.0f);
                                uint8_t uAlpha = static_cast<uint8_t>(std::clamp(static_cast<int>(colGrenade.a() * flAlpha), 0, 255));
                                colGrenade.Set(colGrenade.r(), colGrenade.g(), colGrenade.b(), uAlpha);

                                // Draw box around grenade
                                Draw::AddRect(vecMin, vecMax, colGrenade, DRAW_RECT_OUTLINE, Color(0, 0, 0, 255), 0.0f, flThickness);
                                
                                // Draw grenade name
                                std::string strGrenadeName = pGrenade->GetSchemaName();
                                if (strGrenadeName.find("Flashbang") != std::string::npos)
                                    strGrenadeName = "Flash";
                                else if (strGrenadeName.find("HEGrenade") != std::string::npos)
                                    strGrenadeName = "HE";
                                else if (strGrenadeName.find("Smoke") != std::string::npos)
                                    strGrenadeName = "Smoke";
                                else if (strGrenadeName.find("Molotov") != std::string::npos)
                                    strGrenadeName = "Molotov";
                                else if (strGrenadeName.find("Decoy") != std::string::npos)
                                    strGrenadeName = "Decoy";
                                else
                                    strGrenadeName = "Grenade";
                                
                                ImVec2 vecGrenadeTextPos = ImVec2(vecScreenTop.x, vecScreenTop.y - 15.0f);
                                if (vecGrenadeTextPos.y >= 0 && vecGrenadeTextPos.y < vecDisplaySize.y && Fonts::ESP != nullptr)
                                {
                                    Draw::AddText(Fonts::ESP, 9.0f, vecGrenadeTextPos, strGrenadeName, colGrenade, DRAW_TEXT_OUTLINE, Color(0, 0, 0, 255));
                                }
                            }
                            catch (...)
                            {
                                // Skip grenade ESP if there's an error
                                break;
                            }
                        }

                        break;
                    }

                }
            }
            
            // Draw Radar/Minimap - Safe implementation based on common external CS2 patterns
            if (CONFIG_GET(bool, g_Variables.m_MinimapEsp.m_bMinimapEspEnabled))
            {
                try
                {
                    // Validate local player exists and is alive
                    if (!g_Globals.m_LocalPlayer.m_pPlayerPawn || !g_Globals.m_LocalPlayer.m_pPlayerPawn->IsAlive())
                    {
                        // Skip radar if local player is invalid
                    }
                    else
                    {
                        // Get radar settings
                        int iRadarSize = CONFIG_GET(int, g_Variables.m_MinimapEsp.m_iMinimapSize);
                        int iRadarRange = CONFIG_GET(int, g_Variables.m_MinimapEsp.m_iMinimapRange);
                        int iRadarX = CONFIG_GET(int, g_Variables.m_MinimapEsp.m_iMinimapX);
                        int iRadarY = CONFIG_GET(int, g_Variables.m_MinimapEsp.m_iMinimapY);
                        float flRotationAdjust = CONFIG_GET(float, g_Variables.m_MinimapEsp.m_flMinimapRotationAdjustment);
                        
                        // Clamp values to prevent crashes
                        iRadarSize = std::clamp(iRadarSize, 100, 500);
                        iRadarRange = std::clamp(iRadarRange, 500, 5000);
                        iRadarX = std::max(0, iRadarX);
                        iRadarY = std::max(0, iRadarY);
                        
                        // Get display size
                        ImVec2 vecDisplaySize = ImGui::GetIO().DisplaySize;
                        
                        // Validate radar position is on screen
                        if (iRadarX + iRadarSize > static_cast<int>(vecDisplaySize.x) || 
                            iRadarY + iRadarSize > static_cast<int>(vecDisplaySize.y))
                        {
                            // Skip if radar would be off-screen
                        }
                        else
                        {
                            // Calculate radar center
                            float flRadarCenterX = static_cast<float>(iRadarX) + static_cast<float>(iRadarSize) * 0.5f;
                            float flRadarCenterY = static_cast<float>(iRadarY) + static_cast<float>(iRadarSize) * 0.5f;
                            ImVec2 vecRadarCenter = ImVec2(flRadarCenterX, flRadarCenterY);
                            float flRadarRadius = static_cast<float>(iRadarSize) * 0.5f;
                            
                            // Draw radar background circle
                            Color colBackground = Color(20, 20, 20, 200);
                            Draw::AddCircle(vecRadarCenter, flRadarRadius, colBackground, 64, DRAW_CIRCLE_FILLED, Color(0, 0, 0, 255), 2.0f);
                            
                            // Draw radar border
                            Color colBorder = Color(100, 100, 100, 255);
                            Draw::AddCircle(vecRadarCenter, flRadarRadius, colBorder, 64, DRAW_CIRCLE_OUTLINE, Color(0, 0, 0, 255), 2.0f);
                            
                            // Draw center crosshair (local player position)
                            Color colCenter = Color(255, 255, 255, 255);
                            float flCrosshairSize = 4.0f;
                            Draw::AddLine(
                                ImVec2(flRadarCenterX - flCrosshairSize, flRadarCenterY),
                                ImVec2(flRadarCenterX + flCrosshairSize, flRadarCenterY),
                                colCenter, 1.0f
                            );
                            Draw::AddLine(
                                ImVec2(flRadarCenterX, flRadarCenterY - flCrosshairSize),
                                ImVec2(flRadarCenterX, flRadarCenterY + flCrosshairSize),
                                colCenter, 1.0f
                            );
                            
                            // Draw direction indicator (shows which way you're looking) if rotation is enabled
                            bool bRotateWithView = CONFIG_GET(bool, g_Variables.m_MinimapEsp.m_bMinimapRotateWithView);
                            if (bRotateWithView && CONFIG_GET(bool, g_Variables.m_MinimapEsp.m_bMinimapShowPlayerDirection))
                            {
                                // Draw a line from center pointing in the direction we're looking
                                // Since radar rotates, "forward" is always at the top
                                float flIndicatorLength = flRadarRadius * 0.7f;
                                ImVec2 vecIndicatorEnd = ImVec2(flRadarCenterX, flRadarCenterY - flIndicatorLength);
                                Color colIndicator = Color(0, 255, 255, 255); // Cyan color for direction
                                Draw::AddLine(
                                    ImVec2(flRadarCenterX, flRadarCenterY),
                                    vecIndicatorEnd,
                                    colIndicator, 2.0f
                                );
                                // Draw arrow head
                                float flArrowSize = 3.0f;
                                Draw::AddLine(
                                    vecIndicatorEnd,
                                    ImVec2(vecIndicatorEnd.x - flArrowSize, vecIndicatorEnd.y + flArrowSize),
                                    colIndicator, 2.0f
                                );
                                Draw::AddLine(
                                    vecIndicatorEnd,
                                    ImVec2(vecIndicatorEnd.x + flArrowSize, vecIndicatorEnd.y + flArrowSize),
                                    colIndicator, 2.0f
                                );
                            }
                            
                            // Get local player position and view angle
                            CGameSceneNode* pLocalSceneNode = nullptr;
                            try
                            {
                                pLocalSceneNode = g_Globals.m_LocalPlayer.m_pPlayerPawn->m_pGameSceneNode();
                            }
                            catch (...)
                            {
                                // Skip radar if we can't access scene node
                                pLocalSceneNode = nullptr;
                            }
                            
                            if (pLocalSceneNode)
                            {
                                try
                            {
                                Vector vecLocalOrigin = pLocalSceneNode->m_vecAbsOrigin();
                                
                                // Validate local origin
                                if (std::abs(vecLocalOrigin.x) < 100000.0f && 
                                    std::abs(vecLocalOrigin.y) < 100000.0f && 
                                    std::abs(vecLocalOrigin.z) < 100000.0f)
                                    {
                                        // Validate radar range to prevent division by zero
                                        if (iRadarRange > 0)
                                        {
                                            // Get rotation lock setting
                                            bool bRotateWithView = CONFIG_GET(bool, g_Variables.m_MinimapEsp.m_bMinimapRotateWithView);
                                    
                                    // Get local player team
                                    std::uint8_t uLocalTeam = g_Globals.m_LocalPlayer.m_pPlayerPawn->m_iTeamNum();
                                    
                                    // Get colors
                                    Color colEnemy = CONFIG_GET(Color, g_Variables.m_MinimapEsp.m_colMinimapEnemy);
                                    Color colTeam = CONFIG_GET(Color, g_Variables.m_MinimapEsp.m_colMinimapTeam);
                                    bool bShowTeammates = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bShowTeammates);
                                    
                                    // Draw entities on radar
                                    try
                                    {
                                        std::shared_lock lock(EntityList::m_mtxEntities);
                                        
                                        if (!EntityList::m_vecEntities.empty())
                                        {
                                            for (const auto& entity : EntityList::m_vecEntities)
                                            {
                                                try
                                                {
                                                    if (entity.m_eType != EEntityType::ENTITY_PLAYER)
                                                        continue;
                                                    
                                                    CCSPlayerController* pController = reinterpret_cast<CCSPlayerController*>(entity.m_pEntity);
                                                    if (!pController)
                                                        continue;
                                                    
                                                    if (pController->m_bIsLocalPlayerController())
                                                        continue;
                                                    
                                                    C_CSPlayerPawn* pPawn = reinterpret_cast<C_CSPlayerPawn*>(pController->m_hPawn().Get());
                                                    if (!pPawn)
                                                        continue;
                                                    
                                                    if (!pPawn->IsAlive())
                                                        continue;
                                                    
                                                    // Get entity position
                                                    CGameSceneNode* pEntitySceneNode = pPawn->m_pGameSceneNode();
                                                    if (!pEntitySceneNode)
                                                        continue;
                                                    
                                                    Vector vecEntityOrigin = pEntitySceneNode->m_vecAbsOrigin();
                                                    
                                                    // Validate entity origin
                                                    if (std::abs(vecEntityOrigin.x) > 100000.0f || 
                                                        std::abs(vecEntityOrigin.y) > 100000.0f || 
                                                        std::abs(vecEntityOrigin.z) > 100000.0f)
                                                        continue;
                                                    
                                                    // Calculate relative position (delta from local player)
                                                    Vector vecDelta = vecEntityOrigin - vecLocalOrigin;
                                                    
                                                    // Calculate 2D distance (ignore Z for radar)
                                                    float flDistance2D = std::sqrt(vecDelta.x * vecDelta.x + vecDelta.y * vecDelta.y);
                                                    
                                                    // Skip if too far
                                                    if (flDistance2D > static_cast<float>(iRadarRange))
                                                        continue;
                                                    
                                                    // Calculate radar position using vector math (more reliable than angle conversion)
                                                    // This avoids coordinate system conversion issues
                                                    
                                                    float flRadarX = 0.0f;
                                                    float flRadarY = 0.0f;
                                                    
                                                    // Always use rotation if enabled (force rotation to work)
                                                    if (bRotateWithView)
                                                    {
                                                        // Get view angle and calculate rotation - SAFE: Wrap in try-catch
                                                        try
                                                        {
                                                            QAngle angViewAngle = g_Interfaces.m_CSGOInput.m_angViewAngle;
                                                            
                                                            // Get forward and right vectors from view angle
                                                            Vector vecForward, vecRight;
                                                            angViewAngle.ToDirections(&vecForward, &vecRight, nullptr);
                                                            
                                                            // Normalize vectors (they should already be normalized, but be safe)
                                                            float flForwardLen = vecForward.Length2D();
                                                            float flRightLen = vecRight.Length2D();
                                                            if (flForwardLen > 0.001f && flRightLen > 0.001f)
                                                            {
                                                                vecForward.x /= flForwardLen;
                                                                vecForward.y /= flForwardLen;
                                                                vecRight.x /= flRightLen;
                                                                vecRight.y /= flRightLen;
                                                                
                                                                // Project entity delta onto forward/right plane
                                                                // This gives us the relative position in view space
                                                                // Dot product: forward component = delta · forward, right component = delta · right
                                                                float flForwardComponent = (vecDelta.x * vecForward.x) + (vecDelta.y * vecForward.y);
                                                                float flRightComponent = (vecDelta.x * vecRight.x) + (vecDelta.y * vecRight.y);
                                                                
                                                                // Apply rotation adjustment if any
                                                                if (std::abs(flRotationAdjust) > 0.001f)
                                                                {
                                                                    float flAdjustRad = M_DEG2RAD(flRotationAdjust);
                                                                    float flCosAdjust = std::cos(flAdjustRad);
                                                                    float flSinAdjust = std::sin(flAdjustRad);
                                                                    float flNewForward = flForwardComponent * flCosAdjust - flRightComponent * flSinAdjust;
                                                                    float flNewRight = flForwardComponent * flSinAdjust + flRightComponent * flCosAdjust;
                                                                    flForwardComponent = flNewForward;
                                                                    flRightComponent = flNewRight;
                                                                }
                                                                
                                                                // Convert to radar coordinates
                                                                // Forward = up on radar (negative Y in screen coords because screen Y increases downward)
                                                                // Right = right on radar (positive X in screen coords)
                                                                // In CS2: vecForward points where we're looking, vecRight points to our right
                                                                // On radar: top = forward (where we're looking), right = right side
                                                                // FIX: Invert X to fix mirroring - if enemy is to our right, they should appear on the right side of radar
                                                                flRadarX = -flRightComponent;  // Right component (inverted: positive right = right on radar)
                                                                flRadarY = -flForwardComponent; // Forward component (inverted: positive forward = top on radar)
                                                            }
                                                            else
                                                            {
                                                                // Invalid vectors, use world coordinates instead
                                                                flRadarX = vecDelta.x;
                                                                flRadarY = -vecDelta.y;
                                                            }
                                                        }
                                                        catch (...)
                                                        {
                                                            // Failed to get directions, use world coordinates instead
                                                            flRadarX = vecDelta.x;
                                                            flRadarY = -vecDelta.y;
                                                        }
                                                    }
                                                    else
                                                    {
                                                        // Radar locked to north - use world coordinates directly
                                                        // North (+Y) = up on radar, East (+X) = right on radar
                                                        flRadarX = vecDelta.x;
                                                        flRadarY = -vecDelta.y; // Negative because screen Y increases downward
                                                        
                                                        // Apply rotation adjustment if any
                                                        if (std::abs(flRotationAdjust) > 0.001f)
                                                        {
                                                            float flAdjustRad = M_DEG2RAD(flRotationAdjust);
                                                            float flCosAdjust = std::cos(flAdjustRad);
                                                            float flSinAdjust = std::sin(flAdjustRad);
                                                            float flNewX = flRadarX * flCosAdjust - flRadarY * flSinAdjust;
                                                            float flNewY = flRadarX * flSinAdjust + flRadarY * flCosAdjust;
                                                            flRadarX = flNewX;
                                                            flRadarY = flNewY;
                                                        }
                                                    }
                                                    
                                                    // Scale to radar size (normalize to radar range, then scale to radius)
                                                    // SAFE: Validate iRadarRange to prevent division by zero
                                                    if (iRadarRange > 0)
                                                    {
                                                    float flScale = flRadarRadius / static_cast<float>(iRadarRange);
                                                    flRadarX *= flScale;
                                                    flRadarY *= flScale;
                                                    }
                                                    else
                                                    {
                                                        continue; // Skip if range is invalid
                                                    }
                                                    
                                                    // Clamp to radar circle
                                                    float flDistanceFromCenter = std::sqrt(flRadarX * flRadarX + flRadarY * flRadarY);
                                                    if (flDistanceFromCenter > flRadarRadius && flDistanceFromCenter > 0.001f)
                                                    {
                                                        // Clamp to edge of radar - SAFE: Check for division by zero
                                                        float flClampFactor = flRadarRadius / flDistanceFromCenter;
                                                        flRadarX *= flClampFactor;
                                                        flRadarY *= flClampFactor;
                                                    }
                                                    
                                                    // Convert to screen coordinates (Y is inverted for screen)
                                                    float flScreenX = flRadarCenterX + flRadarX;
                                                    float flScreenY = flRadarCenterY - flRadarY; // Invert Y
                                                    
                                                    // Validate screen coordinates
                                                    if (flScreenX < 0.0f || flScreenX > vecDisplaySize.x ||
                                                        flScreenY < 0.0f || flScreenY > vecDisplaySize.y)
                                                        continue;
                                                    
                                                    // Get team and determine color
                                                    std::uint8_t uEntityTeam = pPawn->m_iTeamNum();
                                                    Color colDot = colEnemy; // Default to enemy color
                                                    
                                                    if (uEntityTeam == uLocalTeam && uLocalTeam != 0)
                                                    {
                                                        if (bShowTeammates)
                                                            colDot = colTeam;
                                                        else
                                                            continue; // Skip teammates if not showing them
                                                    }
                                                    
                                                    // Draw entity dot on radar
                                                    float flDotSize = 3.0f;
                                                    Draw::AddCircle(ImVec2(flScreenX, flScreenY), flDotSize, colDot, 8, DRAW_CIRCLE_FILLED, Color(0, 0, 0, 255), 1.0f);
                                                    
                                                    // Draw direction indicator if enabled
                                                    if (CONFIG_GET(bool, g_Variables.m_MinimapEsp.m_bMinimapShowPlayerDirection))
                                                    {
                                                        // Get entity view angle for direction
                                                        QAngle angEntityView = g_Interfaces.m_CSGOInput.m_angViewAngle; // Fallback to local view
                                                        
                                                        // Try to get entity's actual view angle if available
                                                        // For now, we'll skip direction indicators to keep it simple and safe
                                                    }
                                                }
                                                catch (...)
                                                {
                                                    // Skip this entity if there's an error
                                                    continue;
                                                }
                                            }
                                        }
                                    }
                                            catch (...)
                                            {
                                                // Skip entity drawing if there's an error
                                            }
                                        }
                                    }
                                }
                                catch (...)
                                {
                                    // Skip radar if there's an error accessing local player data
                                }
                            }
                        }
                    }
                }
                catch (...)
                {
                    // Skip radar if there's an error
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
        
        // Aimbot handles calibration automatically in Run()
        if (CONFIG_GET(bool, g_Variables.m_AimBot.m_bEnableAimbot))
        {
            g_Aimbot.Run();
        }

        if (CONFIG_GET(bool, g_Variables.m_TriggerBot.m_bEnableTriggerbot))
        {
			// run triggerbot here
        }

        // Glow ESP - Enhanced with color and intensity support
        // Note: Glow must be applied every frame as the game may reset it
        if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bGlowEspEnabled))
        {
            try
            {
                // Get local player team for filtering
                std::uint8_t uLocalTeam = 0;
                if (g_Globals.m_LocalPlayer.m_pPlayerPawn)
                {
                    uLocalTeam = g_Globals.m_LocalPlayer.m_pPlayerPawn->m_iTeamNum();
                }
                
                bool bShowTeammates = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bShowTeammates);
                Color colGlowEnemy = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colGlowEnemy);
                Color colGlowTeam = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colGlowTeam);
                float flGlowIntensity = CONFIG_GET(float, g_Variables.m_PlayerVisuals.m_flGlowIntensity);
                
                // Clamp intensity to valid range
                flGlowIntensity = std::clamp(flGlowIntensity, 0.0f, 20.0f);
                
                std::shared_lock lock(EntityList::m_mtxEntities);
                for (const auto& entity : EntityList::m_vecEntities)
                {
                    try
                    {
                        if (entity.m_eType != EEntityType::ENTITY_PLAYER)
                            continue;
                        
                        CCSPlayerController* pController = reinterpret_cast<CCSPlayerController*>(entity.m_pEntity);
                        if (!pController)
                            continue;
                        
                        if (pController->m_bIsLocalPlayerController())
                            continue;
                        
                        C_CSPlayerPawn* pPawn = reinterpret_cast<C_CSPlayerPawn*>(pController->m_hPawn().Get());
                        if (!pPawn)
                            continue;
                        
                        if (!pPawn->IsAlive())
                            continue;
                        
                        std::uint8_t uPlayerTeam = pPawn->m_iTeamNum();
                        
                        // Determine if we should glow this player
                        bool bShouldGlow = false;
                        Color colGlow = colGlowEnemy; // Default to enemy color
                        
                        if (uPlayerTeam == uLocalTeam && uLocalTeam != 0)
                        {
                            // Teammate
                            if (bShowTeammates)
                            {
                                bShouldGlow = true;
                                colGlow = colGlowTeam;
                            }
                        }
                        else
                        {
                            // Enemy
                            bShouldGlow = true;
                            colGlow = colGlowEnemy;
                        }
                        
                        if (!bShouldGlow)
                            continue;
                        
                        // Apply real in-game glow using CGlowProperty structure (exactly like C# version)
                        // CGlowProperty is located at C_BaseModelEntity + 0xCC0 (from latest cs2-dumper)
                        std::uintptr_t uPawnAddress = reinterpret_cast<std::uintptr_t>(pPawn);
                        
                        // Validate address is reasonable (should be in game memory range)
                        if (uPawnAddress > 0x10000 && uPawnAddress < 0x7FFFFFFFFFFF)
                        {
                            // C_BaseModelEntity::m_Glow offset (from latest cs2-dumper client_dll.hpp)
                            constexpr std::uintptr_t uGlowPropertyOffset = 0xCC0;
                            
                            // CGlowProperty structure offsets (from latest cs2-dumper client_dll.hpp)
                            constexpr std::uintptr_t uGlowColorOverrideOffset = 0x40; // Color m_glowColorOverride
                            constexpr std::uintptr_t uGlowingOffset = 0x51; // bool m_bGlowing
                            
                            // Get CGlowProperty address (C_BaseModelEntity + 0xCC0)
                            std::uintptr_t uGlowPropertyAddress = uPawnAddress + uGlowPropertyOffset;
                            
                            // Apply intensity to color (0-100 scale, convert to 0-1 for alpha)
                            float flIntensity = std::clamp(flGlowIntensity / 100.0f, 0.0f, 1.0f);
                            float flR = colGlow.rBase() / 255.0f;
                            float flG = colGlow.gBase() / 255.0f;
                            float flB = colGlow.bBase() / 255.0f;
                            float flA = std::max(0.3f, flIntensity); // Minimum 30% for visibility (like C#)
                            
                            // Write color override (Color structure: 4 floats - RGBA)
                            // Exactly like C# version: only write m_glowColorOverride and m_bGlowing
                            g_Memory.WriteMemory<float>(uGlowPropertyAddress + uGlowColorOverrideOffset + 0x0, flR);
                            g_Memory.WriteMemory<float>(uGlowPropertyAddress + uGlowColorOverrideOffset + 0x4, flG);
                            g_Memory.WriteMemory<float>(uGlowPropertyAddress + uGlowColorOverrideOffset + 0x8, flB);
                            g_Memory.WriteMemory<float>(uGlowPropertyAddress + uGlowColorOverrideOffset + 0xC, flA);
                            
                            // Enable glowing
                            g_Memory.WriteMemory<bool>(uGlowPropertyAddress + uGlowingOffset, true);
                        }
                    }
                    catch (...)
                    {
                        // Skip this entity if there's an error
                        continue;
                    }
                }
            }
            catch (...)
            {
                // Skip glow if there's an error
            }
        }
        else
        {
            // Disable glow for all players when glow ESP is disabled
            try
            {
                std::shared_lock lock(EntityList::m_mtxEntities);
                for (const auto& entity : EntityList::m_vecEntities)
                {
                    try
                    {
                        if (entity.m_eType != EEntityType::ENTITY_PLAYER)
                            continue;
                        
                        CCSPlayerController* pController = reinterpret_cast<CCSPlayerController*>(entity.m_pEntity);
                        if (!pController)
                            continue;
                        
                        C_CSPlayerPawn* pPawn = reinterpret_cast<C_CSPlayerPawn*>(pController->m_hPawn().Get());
                        if (!pPawn)
                            continue;
                        
                        // Disable glow using CGlowProperty structure
                        try
                        {
                            std::uintptr_t uPawnAddress = reinterpret_cast<std::uintptr_t>(pPawn);
                            
                            // Validate address is reasonable
                            if (uPawnAddress > 0x10000 && uPawnAddress < 0x7FFFFFFFFFFF)
                            {
                                // Get CGlowProperty offset from C_BaseModelEntity (0xCC0)
                                static std::uintptr_t uGlowPropertyOffset = 0xCC0;
                                constexpr std::uintptr_t uGlowingOffset = 0x51; // bool m_bGlowing
                                
                                // Get CGlowProperty address
                                std::uintptr_t uGlowPropertyAddress = uPawnAddress + uGlowPropertyOffset;
                                
                                // Disable glow by setting m_bGlowing to false
                                g_Memory.WriteMemory<bool>(uGlowPropertyAddress + uGlowingOffset, false);
                            }
                        }
                        catch (...)
                        {
                            // Skip if write fails
                        }
                    }
                    catch (...)
                    {
                        continue;
                    }
                }
            }
            catch (...)
            {
                // Skip if error
            }
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

        // Set default aim key to 'D' (VK_D = 0x44)
        CONFIG_GET(int, g_Variables.m_AimBot.m_iAimKey) = 0x44; // D key
        CONFIG_GET(int, g_Variables.m_AimBot.m_iAimKeyType) = EAimKeyType::AIM_KEY_KEYBOARD;

        // create all our threads
        CreateThreads();

        // Frame limiting for smoother performance
        constexpr float flTargetFPS = 240.0f; // Target 240 FPS for 240Hz monitor
        constexpr float flFrameTime = 1000.0f / flTargetFPS; // ~4.17ms per frame
        auto flLastFrameTime = std::chrono::high_resolution_clock::now();
        
        while (!g_Globals.m_bIsUnloading)
        {
            if (!Window::Render())
               return false;
            
            // Frame limiting - sleep if we're rendering too fast
            auto flCurrentTime = std::chrono::high_resolution_clock::now();
            auto flElapsed = std::chrono::duration_cast<std::chrono::microseconds>(flCurrentTime - flLastFrameTime).count() / 1000.0f;
            
            if (flElapsed < flFrameTime)
            {
                float flSleepTime = flFrameTime - flElapsed;
                if (flSleepTime > 0.0f)
                    g_Utilities.Sleep(flSleepTime);
            }
            
            flLastFrameTime = std::chrono::high_resolution_clock::now();
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