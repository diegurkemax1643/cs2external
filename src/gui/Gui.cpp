#include "../Includes.h"

void Gui::Initialize(unsigned int uFontFlags)
{
    // create fonts
    ImGuiIO& io = ImGui::GetIO();

    ImGui::StyleColorsDark();
    
    // Customize style to match C# project design
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 18.0f;
    style.FrameRounding = 8.0f;
    style.GrabRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(4.0f, 3.0f);
    style.ItemSpacing = ImVec2(4.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    style.IndentSpacing = 21.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;
    style.WindowBorderSize = 0.0f;
    
    // Set colors to match C# design
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.094f, 0.102f, 0.125f, 0.98f); // #181B20 with 0.98 opacity
    colors[ImGuiCol_ChildBg] = ImVec4(0.102f, 0.114f, 0.133f, 1.0f); // #1A1D22
    colors[ImGuiCol_FrameBg] = ImVec4(0.137f, 0.149f, 0.169f, 1.0f); // #23262B
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.137f, 0.149f, 0.169f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.137f, 0.149f, 0.169f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.137f, 0.149f, 0.169f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.765f, 1.0f, 1.0f); // #00C3FF
    colors[ImGuiCol_ButtonActive] = ImVec4(0.0f, 0.765f, 1.0f, 1.0f);
    colors[ImGuiCol_Text] = ImVec4(0.945f, 0.945f, 0.945f, 1.0f); // #F1F1F1
    colors[ImGuiCol_TextDisabled] = ImVec4(0.69f, 0.722f, 0.757f, 1.0f); // #B0B8C1
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.094f, 0.102f, 0.106f, 1.0f); // #181A1B
    colors[ImGuiCol_SliderGrab] = ImVec4(0.29f, 0.62f, 1.0f, 1.0f); // #4A9EFF
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.29f, 0.62f, 1.0f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 0.765f, 1.0f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.137f, 0.149f, 0.169f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.137f, 0.149f, 0.169f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.137f, 0.149f, 0.169f, 1.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.137f, 0.149f, 0.169f, 1.0f);
    
    // Performance optimizations
    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;
    style.Alpha = 1.0f;

    ImFontConfig imVerdanaConfig = { };
    imVerdanaConfig.FontBuilderFlags = ImGuiFreeTypeBuilderFlags::ImGuiFreeTypeBuilderFlags_LightHinting | ImGuiFreeTypeBuilderFlags::ImGuiFreeTypeBuilderFlags_Bold;

    Fonts::Default = io.Fonts->AddFontFromFileTTF(X("C:\\Windows\\Fonts\\Verdana.ttf"), 20, &imVerdanaConfig, io.Fonts->GetGlyphRangesCyrillic());
    Fonts::ESP = io.Fonts->AddFontFromFileTTF(X("C:\\Windows\\Fonts\\Verdana.ttf"), 10, &imVerdanaConfig, io.Fonts->GetGlyphRangesCyrillic());

    m_bInitialized = ImGuiFreeType::BuildFontAtlas(io.Fonts, uFontFlags);
}

void Gui::Update(ImGuiIO& io)
{
    io.MouseDrawCursor = m_bOpen;
    if (m_bOpen)
    {
        POINT p;
        if (GetCursorPos(&p))
        {
            // set imgui mouse position
            io.MousePos = ImVec2(static_cast<float>(p.x), static_cast<float>(p.y));
        }
    }
    
    // Handle keybind input
    if (Gui::m_bWaitingForAimKey)
    {
        if (Gui::m_bKeyInputDebounce)
        {
            Gui::m_bKeyInputDebounce = false;
            return;
        }
        
        // Check mouse buttons
        for (int i = 0; i < 5; i++)
        {
            if (ImGui::IsMouseDown(i))
            {
                int vk = 0;
                switch (i)
                {
                case 0: vk = VK_LBUTTON; break;
                case 1: vk = VK_RBUTTON; break;
                case 2: vk = VK_MBUTTON; break;
                case 3: vk = VK_XBUTTON1; break;
                case 4: vk = VK_XBUTTON2; break;
                }
                
                CONFIG_GET(int, g_Variables.m_AimBot.m_iAimMouseButton) = vk;
                CONFIG_GET(int, g_Variables.m_AimBot.m_iAimKeyType) = EAimKeyType::AIM_KEY_MOUSE;
                Gui::m_bWaitingForAimKey = false;
                return;
            }
        }
        
        // Check keyboard keys
        for (int i = VK_BACK; i <= VK_RMENU; i++)
        {
            if (ImGui::IsKeyDown(i))
            {
                CONFIG_GET(int, g_Variables.m_AimBot.m_iAimKey) = i;
                CONFIG_GET(int, g_Variables.m_AimBot.m_iAimKeyType) = EAimKeyType::AIM_KEY_KEYBOARD;
                Gui::m_bWaitingForAimKey = false;
                return;
            }
        }
    }
}

static void RenderSidebar()
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();
    
    // Sidebar background
    drawList->AddRectFilled(windowPos, ImVec2(windowPos.x + 180.0f, windowPos.y + windowSize.y), 
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.094f, 0.102f, 0.106f, 1.0f)));
    
    ImGui::SetCursorPosY(26.0f);
    ImGui::SetCursorPosX(18.0f);
    
    // Logo - "max.gg" style
    ImVec4 logoColor = ImVec4(0.0f, 0.765f, 1.0f, 1.0f); // #00C3FF
    ImVec4 outlineColor = ImVec4(0.0f, 0.45f, 0.65f, 1.0f);
    
    ImGui::SetWindowFontScale(1.8f);
    ImVec2 textPos = ImGui::GetCursorScreenPos();
    
    // Draw outline
    ImGui::PushStyleColor(ImGuiCol_Text, outlineColor);
    for (int offsetX = -2; offsetX <= 2; offsetX++)
    {
        for (int offsetY = -2; offsetY <= 2; offsetY++)
        {
            if (offsetX != 0 || offsetY != 0)
            {
                ImGui::SetCursorScreenPos(ImVec2(textPos.x + offsetX, textPos.y + offsetY));
                ImGui::Text("max.gg");
            }
        }
    }
    ImGui::PopStyleColor();
    
    // Draw main text
    ImGui::SetCursorScreenPos(textPos);
    ImGui::PushStyleColor(ImGuiCol_Text, logoColor);
    ImGui::Text("max.gg");
    ImGui::SetCursorScreenPos(ImVec2(textPos.x + 1, textPos.y));
    ImGui::Text("max.gg");
    ImGui::SetCursorScreenPos(ImVec2(textPos.x, textPos.y + 1));
    ImGui::Text("max.gg");
    ImGui::SetCursorScreenPos(ImVec2(textPos.x + 1, textPos.y + 1));
    ImGui::Text("max.gg");
    ImGui::PopStyleColor();
    
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SetCursorPosY(64.0f);
    ImGui::Spacing();
    
    // Navigation buttons
    auto RenderSidebarButton = [](const char* label, int index) {
        bool isSelected = Tabs::m_iCurrentTab == index;
        ImVec4 buttonColor = isSelected ? ImVec4(0.137f, 0.149f, 0.169f, 1.0f) : ImVec4(0, 0, 0, 0);
        ImVec4 textColor = (isSelected || ImGui::IsItemHovered()) ? ImVec4(0.0f, 0.765f, 1.0f, 1.0f) : ImVec4(0.69f, 0.722f, 0.757f, 1.0f);
        
        ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.137f, 0.149f, 0.169f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.137f, 0.149f, 0.169f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        
        ImGui::SetCursorPosX(8.0f);
        if (ImGui::Button(label, ImVec2(164, 40)))
        {
            Tabs::m_iCurrentTab = index;
        }
        
        ImGui::PopStyleColor(4);
        ImGui::Spacing();
    };
    
    RenderSidebarButton("Aimbot", 0);
    RenderSidebarButton("Visuals", 1);
    RenderSidebarButton("Miscellaneous", 2);
    RenderSidebarButton("Web Menu", 3);
    RenderSidebarButton("Configs/Global", 4);
    
    // Bottom branding
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 80.0f);
    ImGui::SetCursorPosX(24.0f);
    
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.765f, 1.0f, 1.0f));
    ImGui::Text("max.gg");
    ImGui::PopStyleColor();
    
    ImGui::SetCursorPosX(24.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.69f, 0.722f, 0.757f, 1.0f));
    ImGui::Text("herrdoerfler -19751 Days Left");
    ImGui::PopStyleColor();
}

static void RenderAimbotPanel()
{
    ImGui::SetCursorPos(ImVec2(28, 28));
    
    // Tab buttons
    if (ImGui::Button("Aim", ImVec2(120, 36)))
        Gui::m_iAimbotSubTab = 0;
    ImGui::SameLine(0, 8);
    if (ImGui::Button("TriggerBot", ImVec2(120, 36)))
        Gui::m_iAimbotSubTab = 1;
    
    ImGui::Spacing();
    ImGui::Spacing();
    
    if (Gui::m_iAimbotSubTab == 0)
    {
        ImGui::Text("Aimbot Settings");
        ImGui::Spacing();
        
        // Enable checkbox
        bool aimbotEnabled = CONFIG_GET(bool, g_Variables.m_AimBot.m_bEnableAimbot);
        if (ImGui::Checkbox("Enable", &aimbotEnabled))
        {
            CONFIG_GET(bool, g_Variables.m_AimBot.m_bEnableAimbot) = aimbotEnabled;
        }
        
        ImGui::SameLine();
        
        // Keybind button
        static char keyTextBuffer[64] = "[...]";
        if (Gui::m_bWaitingForAimKey)
        {
            strcpy_s(keyTextBuffer, "[...]");
        }
        else
        {
            int keyIndex = (CONFIG_GET(int, g_Variables.m_AimBot.m_iAimKeyType) == EAimKeyType::AIM_KEY_KEYBOARD) ?
                CONFIG_GET(int, g_Variables.m_AimBot.m_iAimKey) :
                CONFIG_GET(int, g_Variables.m_AimBot.m_iAimMouseButton);
            
            if (keyIndex >= 0 && keyIndex < 256)
            {
                std::string keyStr = std::string("[") + KeyNames[keyIndex] + "]";
                strcpy_s(keyTextBuffer, keyStr.c_str());
            }
            else
            {
                strcpy_s(keyTextBuffer, "[-]");
            }
        }
        
        if (ImGui::Button(keyTextBuffer))
        {
            Gui::m_bWaitingForAimKey = true;
            Gui::m_bKeyInputDebounce = true;
        }
        
        ImGui::Spacing();
        
        // Vis Check
        bool visCheck = CONFIG_GET(bool, g_Variables.m_AimBot.m_bAimbotVisibilityCheck);
        if (ImGui::Checkbox("Vis Check [bSpotted]", &visCheck))
        {
            CONFIG_GET(bool, g_Variables.m_AimBot.m_bAimbotVisibilityCheck) = visCheck;
        }
        
        ImGui::Spacing();
        
        // Show FOV
        bool showFov = CONFIG_GET(bool, g_Variables.m_AimBot.m_bShowFov);
        if (ImGui::Checkbox("Show FoV", &showFov))
        {
            CONFIG_GET(bool, g_Variables.m_AimBot.m_bShowFov) = showFov;
        }
        
        ImGui::Spacing();
        
        // Hitbox dropdown
        ImGui::Text("Hitbox");
        ImGui::SameLine();
        const char* boneOptions[] = { "Head", "Neck", "Chest", "Stomach", "Pelvis" };
        int boneIndex = CONFIG_GET(int, g_Variables.m_AimBot.m_iAimbotBoneIndex);
        if (ImGui::Combo("##Hitbox", &boneIndex, boneOptions, IM_ARRAYSIZE(boneOptions)))
        {
            CONFIG_GET(int, g_Variables.m_AimBot.m_iAimbotBoneIndex) = boneIndex;
        }
        
        ImGui::Spacing();
        
        // FOV Slider
        ImGui::Text("FoV");
        float fov = static_cast<float>(CONFIG_GET(int, g_Variables.m_AimBot.m_iAimbotFov));
        if (ImGui::SliderFloat("##FoV", &fov, 1.0f, 1500.0f, "%.0f"))
        {
            CONFIG_GET(int, g_Variables.m_AimBot.m_iAimbotFov) = static_cast<int>(fov);
        }
        
        ImGui::Spacing();
        
        // Smoothness Slider
        ImGui::Text("Smooth");
        float smoothness = static_cast<float>(CONFIG_GET(int, g_Variables.m_AimBot.m_iAimbotSmoothness));
        if (ImGui::SliderFloat("##Smooth", &smoothness, 1.0f, 100.0f, "%.0f"))
        {
            CONFIG_GET(int, g_Variables.m_AimBot.m_iAimbotSmoothness) = static_cast<int>(smoothness);
        }
    }
    else
    {
        ImGui::Text("TriggerBot Settings");
        ImGui::Spacing();
        
        // Enable TriggerBot
        bool triggerBotEnabled = CONFIG_GET(bool, g_Variables.m_TriggerBot.m_bEnableTriggerbot);
        if (ImGui::Checkbox("Enable TriggerBot", &triggerBotEnabled))
        {
            CONFIG_GET(bool, g_Variables.m_TriggerBot.m_bEnableTriggerbot) = triggerBotEnabled;
        }
        
        ImGui::Spacing();
        
        // Hotkey Selection
        ImGui::Text("Hotkey");
        const char* triggerBotHotkeyOptions[] = { "Left Alt", "Right Alt", "Left Shift", "Right Shift", "Left Ctrl", "Right Ctrl" };
        int hotkeyIndex = 0;
        int currentHotkey = CONFIG_GET(int, g_Variables.m_TriggerBot.m_iTriggerBotHotKey);
        if (currentHotkey == VK_LMENU) hotkeyIndex = 0;
        else if (currentHotkey == VK_RMENU) hotkeyIndex = 1;
        else if (currentHotkey == VK_LSHIFT) hotkeyIndex = 2;
        else if (currentHotkey == VK_RSHIFT) hotkeyIndex = 3;
        else if (currentHotkey == VK_LCONTROL) hotkeyIndex = 4;
        else if (currentHotkey == VK_RCONTROL) hotkeyIndex = 5;
        
        if (ImGui::Combo("##Hotkey", &hotkeyIndex, triggerBotHotkeyOptions, IM_ARRAYSIZE(triggerBotHotkeyOptions)))
        {
            int keys[] = { VK_LMENU, VK_RMENU, VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL };
            CONFIG_GET(int, g_Variables.m_TriggerBot.m_iTriggerBotHotKey) = keys[hotkeyIndex];
        }
        
        ImGui::Spacing();
        
        // Delay Slider
        ImGui::Text("Delay (ms)");
        float delay = static_cast<float>(CONFIG_GET(int, g_Variables.m_TriggerBot.m_iTriggerBotDelay));
        if (ImGui::SliderFloat("##Delay", &delay, 0.0f, 100.0f, "%.0f"))
        {
            CONFIG_GET(int, g_Variables.m_TriggerBot.m_iTriggerBotDelay) = static_cast<int>(delay);
        }
        
        ImGui::Spacing();
        
        // Velocity Slider
        ImGui::Text("Max Velocity");
        float velocity = CONFIG_GET(float, g_Variables.m_TriggerBot.m_flTriggerBotMaxVelocity);
        if (ImGui::SliderFloat("##Velocity", &velocity, 0.0f, 100.0f, "%.1f"))
        {
            CONFIG_GET(float, g_Variables.m_TriggerBot.m_flTriggerBotMaxVelocity) = velocity;
        }
    }
}

static void RenderESPPreview(ImDrawList* drawList, ImVec2 previewPos, ImVec2 previewSize)
{
    // Preview area background
    ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
    drawList->AddRectFilled(previewPos, ImVec2(previewPos.x + previewSize.x, previewPos.y + previewSize.y), bgColor);
    drawList->AddRect(previewPos, ImVec2(previewPos.x + previewSize.x, previewPos.y + previewSize.y), 
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f)), 0.0f, 0, 1.0f);
    
    // Center of preview area
    ImVec2 center = ImVec2(previewPos.x + previewSize.x * 0.5f, previewPos.y + previewSize.y * 0.5f);
    
    // Sample player box dimensions
    float boxHeight = 120.0f;
    float boxWidth = boxHeight * 0.5f;
    ImVec2 boxMin = ImVec2(center.x - boxWidth, center.y - boxHeight * 0.5f);
    ImVec2 boxMax = ImVec2(center.x + boxWidth, center.y + boxHeight * 0.5f);
    ImVec2 boxHead = ImVec2(center.x, boxMin.y);
    
    // Get colors and settings
    Color colBox = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colESPBox);
    Color colText = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colESPText);
    Color colSkeleton = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colSkeletonEsp);
    Color colLinesEsp = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colLinesEsp);
    int iThickness = CONFIG_GET(int, g_Variables.m_PlayerVisuals.m_iEspThickness);
    float flThickness = std::clamp(static_cast<float>(iThickness), 1.0f, 10.0f);
    
    // Apply transparency
    int iTransparency = CONFIG_GET(int, g_Variables.m_PlayerVisuals.m_iEspTransparency);
    float flAlpha = std::clamp(static_cast<float>(iTransparency) / 100.0f, 0.0f, 1.0f);
    uint8_t uAlpha = static_cast<uint8_t>(std::clamp(static_cast<int>(colBox.a() * flAlpha), 0, 255));
    colBox.Set(colBox.r(), colBox.g(), colBox.b(), uAlpha);
    
    // Draw Box ESP
    if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bBoxEspEnabled))
    {
        bool bCorneredBox = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bCorneredBoxEnabled);
        bool bBoxFill = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bBoxFillEnabled);
        
        if (bBoxFill)
        {
            Color colFill = colBox;
            uint8_t uFillAlpha = static_cast<uint8_t>(std::clamp(static_cast<int>(colFill.a() * 0.3f), 0, 255));
            colFill.Set(colFill.r(), colFill.g(), colFill.b(), uFillAlpha);
            drawList->AddRectFilled(boxMin, boxMax, colFill.GetU32());
        }
        
        if (bCorneredBox)
        {
            float flCornerLength = 20.0f;
            ImU32 colBoxU32 = colBox.GetU32();
            
            // Top-left corner
            drawList->AddLine(ImVec2(boxMin.x, boxMin.y), ImVec2(boxMin.x + flCornerLength, boxMin.y), colBoxU32, flThickness);
            drawList->AddLine(ImVec2(boxMin.x, boxMin.y), ImVec2(boxMin.x, boxMin.y + flCornerLength), colBoxU32, flThickness);
            
            // Top-right corner
            drawList->AddLine(ImVec2(boxMax.x, boxMin.y), ImVec2(boxMax.x - flCornerLength, boxMin.y), colBoxU32, flThickness);
            drawList->AddLine(ImVec2(boxMax.x, boxMin.y), ImVec2(boxMax.x, boxMin.y + flCornerLength), colBoxU32, flThickness);
            
            // Bottom-left corner
            drawList->AddLine(ImVec2(boxMin.x, boxMax.y), ImVec2(boxMin.x + flCornerLength, boxMax.y), colBoxU32, flThickness);
            drawList->AddLine(ImVec2(boxMin.x, boxMax.y), ImVec2(boxMin.x, boxMax.y - flCornerLength), colBoxU32, flThickness);
            
            // Bottom-right corner
            drawList->AddLine(ImVec2(boxMax.x, boxMax.y), ImVec2(boxMax.x - flCornerLength, boxMax.y), colBoxU32, flThickness);
            drawList->AddLine(ImVec2(boxMax.x, boxMax.y), ImVec2(boxMax.x, boxMax.y - flCornerLength), colBoxU32, flThickness);
        }
        else
        {
            drawList->AddRect(boxMin, boxMax, colBox.GetU32(), 0.0f, 0, flThickness);
        }
    }
    
    // Draw Health Bar
    if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bHealthBarEnabled))
    {
        float flBarWidth = 4.0f;
        float flHealthPercent = 0.75f; // 75% health for preview
        float flBarHeight = boxHeight * flHealthPercent;
        float flBarX = boxMin.x - 10.0f;
        float flBarY = boxMax.y - flBarHeight;
        
        Color colHealth;
        if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bOverrideHealthColorEnabled))
        {
            colHealth = colBox;
        }
        else
        {
            int iRed = static_cast<int>(255.0f * (1.0f - flHealthPercent));
            int iGreen = static_cast<int>(255.0f * flHealthPercent);
            colHealth = Color(std::clamp(iRed, 0, 255), std::clamp(iGreen, 0, 255), 0, 255);
        }
        
        drawList->AddRectFilled(ImVec2(flBarX, flBarY), ImVec2(flBarX + flBarWidth, boxMax.y), colHealth.GetU32());
        drawList->AddRect(ImVec2(flBarX, flBarY), ImVec2(flBarX + flBarWidth, boxMax.y), IM_COL32(0, 0, 0, 255), 0.0f, 0, 1.0f);
    }
    
    // Draw Name
    if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bNameEnabled))
    {
        if (Fonts::ESP != nullptr)
        {
            ImVec2 vecNamePos = ImVec2(boxHead.x, boxHead.y - 20.0f);
            const char* previewName = "Preview Player";
            ImVec2 textSize = Fonts::ESP->CalcTextSizeA(10.0f, FLT_MAX, 0.0f, previewName);
            vecNamePos.x -= textSize.x * 0.5f; // Center the text
            
            // Use Draw::AddDrawListText for proper text rendering with outline
            Draw::AddDrawListText(drawList, Fonts::ESP, 10.0f, vecNamePos, previewName, 
                colText.GetU32(), DRAW_TEXT_OUTLINE, IM_COL32(0, 0, 0, 255));
        }
    }
    
    // Draw Head Circle
    if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bHeadCircleEnabled))
    {
        float flHeadRadius = 15.0f;
        drawList->AddCircle(boxHead, flHeadRadius, colBox.GetU32(), 32, flThickness);
    }
    
    // Draw Skeleton ESP
    if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bSkeletonEspEnabled))
    {
        ImU32 colSkeletonU32 = colSkeleton.GetU32();
        
        // Simplified skeleton for preview
        ImVec2 head = boxHead;
        ImVec2 neck = ImVec2(center.x, head.y + 15.0f);
        ImVec2 spine1 = ImVec2(center.x, neck.y + 20.0f);
        ImVec2 spine2 = ImVec2(center.x, spine1.y + 20.0f);
        ImVec2 pelvis = ImVec2(center.x, spine2.y + 20.0f);
        
        ImVec2 armUpperL = ImVec2(center.x - 25.0f, spine1.y);
        ImVec2 armLowerL = ImVec2(center.x - 35.0f, armUpperL.y + 20.0f);
        ImVec2 handL = ImVec2(center.x - 40.0f, armLowerL.y + 15.0f);
        
        ImVec2 armUpperR = ImVec2(center.x + 25.0f, spine1.y);
        ImVec2 armLowerR = ImVec2(center.x + 35.0f, armUpperR.y + 20.0f);
        ImVec2 handR = ImVec2(center.x + 40.0f, armLowerR.y + 15.0f);
        
        ImVec2 legUpperL = ImVec2(center.x - 15.0f, pelvis.y);
        ImVec2 legLowerL = ImVec2(center.x - 20.0f, legUpperL.y + 25.0f);
        ImVec2 ankleL = ImVec2(center.x - 22.0f, legLowerL.y + 20.0f);
        
        ImVec2 legUpperR = ImVec2(center.x + 15.0f, pelvis.y);
        ImVec2 legLowerR = ImVec2(center.x + 20.0f, legUpperR.y + 25.0f);
        ImVec2 ankleR = ImVec2(center.x + 22.0f, legLowerR.y + 20.0f);
        
        // Spine chain
        drawList->AddLine(head, neck, colSkeletonU32, flThickness);
        drawList->AddLine(neck, spine1, colSkeletonU32, flThickness);
        drawList->AddLine(spine1, spine2, colSkeletonU32, flThickness);
        drawList->AddLine(spine2, pelvis, colSkeletonU32, flThickness);
        
        // Left arm chain
        drawList->AddLine(spine1, armUpperL, colSkeletonU32, flThickness);
        drawList->AddLine(armUpperL, armLowerL, colSkeletonU32, flThickness);
        drawList->AddLine(armLowerL, handL, colSkeletonU32, flThickness);
        
        // Right arm chain
        drawList->AddLine(spine1, armUpperR, colSkeletonU32, flThickness);
        drawList->AddLine(armUpperR, armLowerR, colSkeletonU32, flThickness);
        drawList->AddLine(armLowerR, handR, colSkeletonU32, flThickness);
        
        // Left leg chain
        drawList->AddLine(pelvis, legUpperL, colSkeletonU32, flThickness);
        drawList->AddLine(legUpperL, legLowerL, colSkeletonU32, flThickness);
        drawList->AddLine(legLowerL, ankleL, colSkeletonU32, flThickness);
        
        // Right leg chain
        drawList->AddLine(pelvis, legUpperR, colSkeletonU32, flThickness);
        drawList->AddLine(legUpperR, legLowerR, colSkeletonU32, flThickness);
        drawList->AddLine(legLowerR, ankleR, colSkeletonU32, flThickness);
    }
    
    // Draw Lines ESP
    if (CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bLinesEspEnabled))
    {
        ImVec2 vecOriginPoint;
        int iLinesOrigin = CONFIG_GET(int, g_Variables.m_PlayerVisuals.m_iLinesEspOrigin);
        ImVec2 screenSize = previewSize;
        
        switch (iLinesOrigin)
        {
        case ELinesEspOrigin::LINES_ESP_ORIGIN_TOP:
            vecOriginPoint = ImVec2(previewPos.x + screenSize.x * 0.5f, previewPos.y);
            break;
        case ELinesEspOrigin::LINES_ESP_ORIGIN_BOTTOM:
            vecOriginPoint = ImVec2(previewPos.x + screenSize.x * 0.5f, previewPos.y + screenSize.y);
            break;
        case ELinesEspOrigin::LINES_ESP_ORIGIN_CROSSHAIR:
        default:
            vecOriginPoint = ImVec2(previewPos.x + screenSize.x * 0.5f, previewPos.y + screenSize.y * 0.5f);
            break;
        }
        
        drawList->AddLine(vecOriginPoint, center, colLinesEsp.GetU32(), flThickness);
    }
}

static void RenderVisualsPanel()
{
    ImGui::SetCursorPos(ImVec2(28, 28));
    
    // Tab buttons
    if (ImGui::Button("General", ImVec2(120, 36)))
        Gui::m_iVisualsSubTab = 0;
    ImGui::SameLine(0, 8);
    if (ImGui::Button("Colors", ImVec2(120, 36)))
        Gui::m_iVisualsSubTab = 1;
    
    ImGui::Spacing();
    ImGui::Spacing();
    
    if (Gui::m_iVisualsSubTab == 0)
    {
        ImGui::Text("Players");
        ImGui::Spacing();
        
        // Enable Preview
        bool enablePreview = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bEnablePreview);
        if (ImGui::Checkbox("Enable Preview", &enablePreview))
        {
            CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bEnablePreview) = enablePreview;
        }
        
        // Render preview if enabled
        if (enablePreview)
        {
            ImGui::Spacing();
            ImGui::Text("Preview:");
            ImGui::Spacing();
            
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 previewPos = ImGui::GetCursorScreenPos();
            ImVec2 previewSize = ImVec2(400.0f, 300.0f);
            
            // Reserve space for preview
            ImGui::InvisibleButton("##PreviewArea", previewSize);
            
            // Render the preview
            RenderESPPreview(drawList, previewPos, previewSize);
            
            ImGui::Spacing();
        }
        
        ImGui::Spacing();
        
        // ESP toggles
        bool espEnabled = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bEnableVisuals);
        if (ImGui::Checkbox("Enable", &espEnabled))
        {
            CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bEnableVisuals) = espEnabled;
        }
        
        bool showTeammates = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bShowTeammates);
        if (ImGui::Checkbox("Show Teammates", &showTeammates))
        {
            CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bShowTeammates) = showTeammates;
        }
        
        bool nameEnabled = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bNameEnabled);
        if (ImGui::Checkbox("Name", &nameEnabled))
        {
            CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bNameEnabled) = nameEnabled;
        }
        
        bool boxEnabled = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bBoxEspEnabled);
        if (ImGui::Checkbox("Box", &boxEnabled))
        {
            CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bBoxEspEnabled) = boxEnabled;
        }
        
        bool kirkEspEnabled = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bKirkEspEnabled);
        if (ImGui::Checkbox("Kirk ESP", &kirkEspEnabled))
        {
            CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bKirkEspEnabled) = kirkEspEnabled;
        }
        
        bool corneredBox = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bCorneredBoxEnabled);
        if (ImGui::Checkbox("Cornered Box", &corneredBox))
        {
            CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bCorneredBoxEnabled) = corneredBox;
        }
        
        bool boxFill = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bBoxFillEnabled);
        if (ImGui::Checkbox("Box Fill", &boxFill))
        {
            CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bBoxFillEnabled) = boxFill;
        }
        
        bool healthBar = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bHealthBarEnabled);
        if (ImGui::Checkbox("Health", &healthBar))
        {
            CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bHealthBarEnabled) = healthBar;
        }
        
        bool overrideHealthColor = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bOverrideHealthColorEnabled);
        if (ImGui::Checkbox("Override Health Color", &overrideHealthColor))
        {
            CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bOverrideHealthColorEnabled) = overrideHealthColor;
        }
        
        bool skeleton = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bSkeletonEspEnabled);
        if (ImGui::Checkbox("Skeleton", &skeleton))
        {
            CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bSkeletonEspEnabled) = skeleton;
        }
        
        bool headCircle = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bHeadCircleEnabled);
        if (ImGui::Checkbox("Head Circle", &headCircle))
        {
            CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bHeadCircleEnabled) = headCircle;
        }
        
        bool linesEsp = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bLinesEspEnabled);
        if (ImGui::Checkbox("Lines ESP", &linesEsp))
        {
            CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bLinesEspEnabled) = linesEsp;
        }
        
        bool minimapEsp = CONFIG_GET(bool, g_Variables.m_MinimapEsp.m_bMinimapEspEnabled);
        if (ImGui::Checkbox("Minimap ESP", &minimapEsp))
        {
            CONFIG_GET(bool, g_Variables.m_MinimapEsp.m_bMinimapEspEnabled) = minimapEsp;
        }
        
        ImGui::SameLine();
        if (ImGui::SmallButton("?"))
        {
            // Show minimap settings dialog (would need to implement)
        }
    }
    else
    {
        ImGui::Text("ESP Colors");
        ImGui::Spacing();
        
        // ESP Color
        ImGui::Text("ESP Color");
        Color espColor = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colESPBox);
        float espCol[3] = { espColor.rBase(), espColor.gBase(), espColor.bBase() };
        if (ImGui::ColorEdit3("##EspColor", espCol))
        {
            espColor.Set(espCol[0], espCol[1], espCol[2], 1.0f);
            CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colESPBox) = espColor;
        }
        
        ImGui::Spacing();
        
        // Skeleton Color
        ImGui::Text("Skeleton Color");
        Color skeletonColor = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colSkeletonEsp);
        float skelCol[3] = { skeletonColor.rBase(), skeletonColor.gBase(), skeletonColor.bBase() };
        if (ImGui::ColorEdit3("##SkeletonColor", skelCol))
        {
            skeletonColor.Set(skelCol[0], skelCol[1], skelCol[2], 1.0f);
            CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colSkeletonEsp) = skeletonColor;
        }
        
        ImGui::Spacing();
        ImGui::Text("Box Gradient Colors");
        ImGui::Spacing();
        
        // Gradient Color 1
        ImGui::Text("Gradient Color 1");
        Color grad1 = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colBoxGradient1);
        float grad1Col[3] = { grad1.rBase(), grad1.gBase(), grad1.bBase() };
        if (ImGui::ColorEdit3("##Gradient1", grad1Col))
        {
            grad1.Set(grad1Col[0], grad1Col[1], grad1Col[2], 1.0f);
            CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colBoxGradient1) = grad1;
        }
        
        ImGui::Spacing();
        
        // Gradient Color 2
        ImGui::Text("Gradient Color 2");
        Color grad2 = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colBoxGradient2);
        float grad2Col[3] = { grad2.rBase(), grad2.gBase(), grad2.bBase() };
        if (ImGui::ColorEdit3("##Gradient2", grad2Col))
        {
            grad2.Set(grad2Col[0], grad2Col[1], grad2Col[2], 1.0f);
            CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colBoxGradient2) = grad2;
        }
        
        ImGui::Spacing();
        ImGui::Text("Lines ESP");
        ImGui::Spacing();
        
        // Lines Color
        ImGui::Text("Lines Color");
        Color linesColor = CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colLinesEsp);
        float linesCol[3] = { linesColor.rBase(), linesColor.gBase(), linesColor.bBase() };
        if (ImGui::ColorEdit3("##LinesColor", linesCol))
        {
            linesColor.Set(linesCol[0], linesCol[1], linesCol[2], 1.0f);
            CONFIG_GET(Color, g_Variables.m_PlayerVisuals.m_colLinesEsp) = linesColor;
        }
        
        ImGui::Spacing();
        
        // Lines Origin
        ImGui::Text("Lines Origin");
        const char* linesEspOriginOptions[] = { "Top", "Bottom", "Crosshair" };
        int linesOrigin = CONFIG_GET(int, g_Variables.m_PlayerVisuals.m_iLinesEspOrigin);
        if (ImGui::Combo("##LinesOrigin", &linesOrigin, linesEspOriginOptions, IM_ARRAYSIZE(linesEspOriginOptions)))
        {
            CONFIG_GET(int, g_Variables.m_PlayerVisuals.m_iLinesEspOrigin) = linesOrigin;
        }
    }
}

static void RenderMiscellaneousPanel()
{
    ImGui::SetCursorPos(ImVec2(28, 28));
    
    ImGui::Text("Misc Settings");
    ImGui::Spacing();
    
    // Tab Bar for Settings and Widgets
    if (ImGui::BeginTabBar("MiscTabs"))
    {
        // Settings Tab
        if (ImGui::BeginTabItem("Settings"))
        {
            // ESP Thickness
            ImGui::Text("ESP Thickness");
            float thickness = static_cast<float>(CONFIG_GET(int, g_Variables.m_PlayerVisuals.m_iEspThickness));
            if (ImGui::SliderFloat("##Thickness", &thickness, 1.0f, 10.0f, "%.0fpx"))
            {
                CONFIG_GET(int, g_Variables.m_PlayerVisuals.m_iEspThickness) = static_cast<int>(thickness);
            }
            ImGui::TextDisabled("Adjust line thickness for all ESP features");
            ImGui::Spacing();
            
            // Wireframe ESP
            if (ImGui::CollapsingHeader("Wireframe ESP"))
            {
                bool wireframe = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bWireframeEspEnabled);
                if (ImGui::Checkbox("##WireframeESP", &wireframe))
                {
                    CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bWireframeEspEnabled) = wireframe;
                }
                ImGui::SameLine();
                ImGui::Text("Wireframe ESP");
                ImGui::TextDisabled("Draw wireframe boxes around enemies");
            }
            
            // Self ESP
            if (ImGui::CollapsingHeader("Self ESP"))
            {
                bool selfEsp = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bSelfEspEnabled);
                if (ImGui::Checkbox("Self ESP", &selfEsp))
                {
                    CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bSelfEspEnabled) = selfEsp;
                }
                ImGui::TextDisabled("Draw wireframe on your own character");
            }
            
            ImGui::EndTabItem();
        }
        
        // Widgets Tab
        if (ImGui::BeginTabItem("Widgets"))
        {
            ImGui::Text("Widget Settings");
            ImGui::TextDisabled("Toggle widgets that display information on screen");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            // FPS Widget
            if (ImGui::CollapsingHeader("FPS Widget"))
            {
                bool fpsWidget = CONFIG_GET(bool, g_Variables.m_Widgets.m_bFpsWidgetEnabled);
                if (ImGui::Checkbox("Enable FPS Widget", &fpsWidget))
                {
                    CONFIG_GET(bool, g_Variables.m_Widgets.m_bFpsWidgetEnabled) = fpsWidget;
                }
                ImGui::TextDisabled("Display current FPS and smoothness");
                
                if (fpsWidget)
                {
                    ImGui::Spacing();
                    ImGui::Text("Position:");
                    float fpsX = CONFIG_GET(float, g_Variables.m_Widgets.m_flFpsWidgetX);
                    float fpsY = CONFIG_GET(float, g_Variables.m_Widgets.m_flFpsWidgetY);
                    if (ImGui::InputFloat("X##FPSX", &fpsX))
                    {
                        CONFIG_GET(float, g_Variables.m_Widgets.m_flFpsWidgetX) = std::max(0.0f, fpsX);
                    }
                    ImGui::SameLine();
                    if (ImGui::InputFloat("Y##FPSY", &fpsY))
                    {
                        CONFIG_GET(float, g_Variables.m_Widgets.m_flFpsWidgetY) = std::max(0.0f, fpsY);
                    }
                }
            }
            
            // Bomb Timer Widget
            if (ImGui::CollapsingHeader("Bomb Timer Widget"))
            {
                bool bombWidget = CONFIG_GET(bool, g_Variables.m_Widgets.m_bBombTimerWidgetEnabled);
                if (ImGui::Checkbox("Enable Bomb Timer Widget", &bombWidget))
                {
                    CONFIG_GET(bool, g_Variables.m_Widgets.m_bBombTimerWidgetEnabled) = bombWidget;
                }
                ImGui::TextDisabled("Display bomb timer and defuse time");
                
                if (bombWidget)
                {
                    ImGui::Spacing();
                    ImGui::Text("Position:");
                    float bombX = CONFIG_GET(float, g_Variables.m_Widgets.m_flBombTimerWidgetX);
                    float bombY = CONFIG_GET(float, g_Variables.m_Widgets.m_flBombTimerWidgetY);
                    if (ImGui::InputFloat("X##BombX", &bombX))
                    {
                        CONFIG_GET(float, g_Variables.m_Widgets.m_flBombTimerWidgetX) = std::max(0.0f, bombX);
                    }
                    ImGui::SameLine();
                    if (ImGui::InputFloat("Y##BombY", &bombY))
                    {
                        CONFIG_GET(float, g_Variables.m_Widgets.m_flBombTimerWidgetY) = std::max(0.0f, bombY);
                    }
                }
            }
            
            // Indicator ESP Widget
            if (ImGui::CollapsingHeader("Indicator ESP Widget"))
            {
                bool indicator = CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bIndicatorEspEnabled);
                if (ImGui::Checkbox("Enable Indicator ESP Widget", &indicator))
                {
                    CONFIG_GET(bool, g_Variables.m_PlayerVisuals.m_bIndicatorEspEnabled) = indicator;
                }
                ImGui::TextDisabled("Show notification when enemies are behind you");
                
                if (indicator)
                {
                    ImGui::Spacing();
                    ImGui::Text("Position:");
                    float indX = CONFIG_GET(float, g_Variables.m_Widgets.m_flIndicatorEspWidgetX);
                    float indY = CONFIG_GET(float, g_Variables.m_Widgets.m_flIndicatorEspWidgetY);
                    if (ImGui::InputFloat("X##IndX", &indX))
                    {
                        CONFIG_GET(float, g_Variables.m_Widgets.m_flIndicatorEspWidgetX) = std::max(0.0f, indX);
                    }
                    ImGui::SameLine();
                    if (ImGui::InputFloat("Y##IndY", &indY))
                    {
                        CONFIG_GET(float, g_Variables.m_Widgets.m_flIndicatorEspWidgetY) = std::max(0.0f, indY);
                    }
                }
            }
            
            // Player Count Widget
            if (ImGui::CollapsingHeader("Player Count Widget"))
            {
                bool playerCountWidget = CONFIG_GET(bool, g_Variables.m_Widgets.m_bPlayerCountWidgetEnabled);
                if (ImGui::Checkbox("Enable Player Count Widget", &playerCountWidget))
                {
                    CONFIG_GET(bool, g_Variables.m_Widgets.m_bPlayerCountWidgetEnabled) = playerCountWidget;
                }
                ImGui::TextDisabled("Display alive enemy and teammate counts");
                
                if (playerCountWidget)
                {
                    ImGui::Spacing();
                    ImGui::Text("Position:");
                    float pcX = CONFIG_GET(float, g_Variables.m_Widgets.m_flPlayerCountWidgetX);
                    float pcY = CONFIG_GET(float, g_Variables.m_Widgets.m_flPlayerCountWidgetY);
                    if (ImGui::InputFloat("X##PCX", &pcX))
                    {
                        CONFIG_GET(float, g_Variables.m_Widgets.m_flPlayerCountWidgetX) = std::max(0.0f, pcX);
                    }
                    ImGui::SameLine();
                    if (ImGui::InputFloat("Y##PCY", &pcY))
                    {
                        CONFIG_GET(float, g_Variables.m_Widgets.m_flPlayerCountWidgetY) = std::max(0.0f, pcY);
                    }
                }
            }
            
            // Health & Armor Widget
            if (ImGui::CollapsingHeader("Health & Armor Widget"))
            {
                bool healthArmorWidget = CONFIG_GET(bool, g_Variables.m_Widgets.m_bHealthArmorWidgetEnabled);
                if (ImGui::Checkbox("Enable Health & Armor Widget", &healthArmorWidget))
                {
                    CONFIG_GET(bool, g_Variables.m_Widgets.m_bHealthArmorWidgetEnabled) = healthArmorWidget;
                }
                ImGui::TextDisabled("Display your current health and armor");
                
                if (healthArmorWidget)
                {
                    ImGui::Spacing();
                    ImGui::Text("Position:");
                    float haX = CONFIG_GET(float, g_Variables.m_Widgets.m_flHealthArmorWidgetX);
                    float haY = CONFIG_GET(float, g_Variables.m_Widgets.m_flHealthArmorWidgetY);
                    if (ImGui::InputFloat("X##HAX", &haX))
                    {
                        CONFIG_GET(float, g_Variables.m_Widgets.m_flHealthArmorWidgetX) = std::max(0.0f, haX);
                    }
                    ImGui::SameLine();
                    if (ImGui::InputFloat("Y##HAY", &haY))
                    {
                        CONFIG_GET(float, g_Variables.m_Widgets.m_flHealthArmorWidgetY) = std::max(0.0f, haY);
                    }
                }
            }
            
            // Weapon & Ammo Widget
            if (ImGui::CollapsingHeader("Weapon & Ammo Widget"))
            {
                bool weaponAmmoWidget = CONFIG_GET(bool, g_Variables.m_Widgets.m_bWeaponAmmoWidgetEnabled);
                if (ImGui::Checkbox("Enable Weapon & Ammo Widget", &weaponAmmoWidget))
                {
                    CONFIG_GET(bool, g_Variables.m_Widgets.m_bWeaponAmmoWidgetEnabled) = weaponAmmoWidget;
                }
                ImGui::TextDisabled("Display current weapon name and ammo count");
                
                if (weaponAmmoWidget)
                {
                    ImGui::Spacing();
                    ImGui::Text("Position:");
                    float waX = CONFIG_GET(float, g_Variables.m_Widgets.m_flWeaponAmmoWidgetX);
                    float waY = CONFIG_GET(float, g_Variables.m_Widgets.m_flWeaponAmmoWidgetY);
                    if (ImGui::InputFloat("X##WAX", &waX))
                    {
                        CONFIG_GET(float, g_Variables.m_Widgets.m_flWeaponAmmoWidgetX) = std::max(0.0f, waX);
                    }
                    ImGui::SameLine();
                    if (ImGui::InputFloat("Y##WAY", &waY))
                    {
                        CONFIG_GET(float, g_Variables.m_Widgets.m_flWeaponAmmoWidgetY) = std::max(0.0f, waY);
                    }
                }
            }
            
            // Spectator List Widget
            if (ImGui::CollapsingHeader("Spectator List Widget"))
            {
                bool spectatorListWidget = CONFIG_GET(bool, g_Variables.m_Widgets.m_bSpectatorListWidgetEnabled);
                if (ImGui::Checkbox("Enable Spectator List Widget", &spectatorListWidget))
                {
                    CONFIG_GET(bool, g_Variables.m_Widgets.m_bSpectatorListWidgetEnabled) = spectatorListWidget;
                }
                ImGui::TextDisabled("Display list of players spectating you");
                
                if (spectatorListWidget)
                {
                    ImGui::Spacing();
                    ImGui::Text("Position:");
                    float specX = CONFIG_GET(float, g_Variables.m_Widgets.m_flSpectatorListWidgetX);
                    float specY = CONFIG_GET(float, g_Variables.m_Widgets.m_flSpectatorListWidgetY);
                    if (ImGui::InputFloat("X##SpecX", &specX))
                    {
                        CONFIG_GET(float, g_Variables.m_Widgets.m_flSpectatorListWidgetX) = std::max(0.0f, specX);
                    }
                    ImGui::SameLine();
                    if (ImGui::InputFloat("Y##SpecY", &specY))
                    {
                        CONFIG_GET(float, g_Variables.m_Widgets.m_flSpectatorListWidgetY) = std::max(0.0f, specY);
                    }
                }
            }
            
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }
    
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Exit Button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
    
    if (ImGui::Button("Exit", ImVec2(200, 40)))
    {
        Config::Save(X("default.json"));
        g_Globals.m_bIsUnloading = true;
        ExitProcess(0);
    }
    
    ImGui::PopStyleColor(3);
    ImGui::TextDisabled("Exit the application (settings are saved automatically)");
}

static void RenderWebMenuPanel()
{
    ImGui::SetCursorPos(ImVec2(28, 28));
    
    ImGui::Text("Web Menu Settings");
    ImGui::Spacing();
    
    if (ImGui::CollapsingHeader("Web Server"))
    {
        bool webServerEnabled = CONFIG_GET(bool, g_Variables.m_WebServer.m_bWebServerEnabled);
        if (ImGui::Checkbox("Enable Web Server", &webServerEnabled))
        {
            CONFIG_GET(bool, g_Variables.m_WebServer.m_bWebServerEnabled) = webServerEnabled;
        }
        
        ImGui::TextDisabled("Start a local web server to view enemy information in a browser");
        ImGui::Spacing();
        
        // Port
        ImGui::Text("Port");
        float port = static_cast<float>(CONFIG_GET(int, g_Variables.m_WebServer.m_iWebServerPort));
        if (ImGui::SliderFloat("##Port", &port, 1024.0f, 65535.0f, "%.0f"))
        {
            CONFIG_GET(int, g_Variables.m_WebServer.m_iWebServerPort) = static_cast<int>(port);
        }
        ImGui::TextDisabled("Port number for the web server (default: 8080)");
        ImGui::Spacing();
        
        // Update Interval
        ImGui::Text("Update Interval (ms)");
        float interval = static_cast<float>(CONFIG_GET(int, g_Variables.m_WebServer.m_iWebServerUpdateInterval));
        if (ImGui::SliderFloat("##Interval", &interval, 1.0f, 500.0f, "%.0fms"))
        {
            CONFIG_GET(int, g_Variables.m_WebServer.m_iWebServerUpdateInterval) = std::max(1, std::min(500, static_cast<int>(interval)));
        }
        ImGui::TextDisabled("How often the web page updates enemy data (default: 1000ms = 1 second)");
        ImGui::Spacing();
        
        // Tunnel
        ImGui::Text("Cloudflared Tunnel");
        bool tunnelEnabled = CONFIG_GET(bool, g_Variables.m_WebServer.m_bWebServerTunnelEnabled);
        if (ImGui::Checkbox("Enable Cloudflared Tunnel (Public URL without port forwarding)", &tunnelEnabled))
        {
            CONFIG_GET(bool, g_Variables.m_WebServer.m_bWebServerTunnelEnabled) = tunnelEnabled;
        }
        ImGui::TextDisabled("Creates a public URL accessible from anywhere using Cloudflare tunnel. Requires cloudflared installed.");
    }
}

static void RenderConfigPanel()
{
    ImGui::SetCursorPos(ImVec2(28, 28));
    
    ImGui::Text("Config Management");
    ImGui::Spacing();
    
    static std::string strConfigFile;
    ImGui::Text("Config Name:");
    ImGui::InputText("##ConfigName", &strConfigFile);
    
    if (ImGui::Button("Save", ImVec2(120, 36)))
    {
        Config::Save(strConfigFile);
        strConfigFile.clear();
        Config::Refresh();
    }
    ImGui::SameLine(0, 8);
    if (ImGui::Button("Load", ImVec2(120, 36)))
    {
        Config::Load(strConfigFile);
        Config::Refresh();
    }
    
    ImGui::Spacing();
    ImGui::Text("Available Configs:");
    ImGui::BeginChild("ConfigList", ImVec2(-1, 100), false);
    
    for (size_t uIndex = 0U; uIndex < Config::m_vecFileNames.size(); uIndex++)
    {
        if (ImGui::Selectable(Config::m_vecFileNames.at(uIndex).c_str(), false))
        {
            strConfigFile = Config::m_vecFileNames.at(uIndex);
        }
    }
    
    ImGui::EndChild();
    
    ImGui::Spacing();
    if (ImGui::Button("Load Selected", ImVec2(180, 36)))
    {
        if (!strConfigFile.empty())
        {
            Config::Load(strConfigFile);
        }
    }
    ImGui::SameLine(0, 8);
    if (ImGui::Button("Delete", ImVec2(120, 36)))
    {
        if (!strConfigFile.empty())
        {
            Config::Remove(strConfigFile);
            Config::Refresh();
            strConfigFile.clear();
        }
    }
}

void Gui::Render()
{
    if (!m_bInitialized)
        return;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* pForeGroundDrawList = ImGui::GetForegroundDrawList();

    // update our stuff first
    Gui::Update(io);

    ImGui::SetNextWindowSize(ImVec2(924, 700), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar;
    
    if (ImGui::Begin("Menu", nullptr, windowFlags))
    {
        // Main container with rounded background
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Draw rounded background
        drawList->AddRectFilled(windowPos, windowPos + windowSize, 
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.137f, 0.157f, 0.165f, 1.0f)), 18.0f);
        
        // Sidebar + Content layout
        ImGui::BeginChild("Sidebar", ImVec2(180, -1), false);
        RenderSidebar();
        ImGui::EndChild();
        
        ImGui::SameLine();
        
        ImGui::BeginChild("Content", ImVec2(-1, -1), false);
        
        // Content background
        ImVec2 contentPos = ImGui::GetWindowPos();
        ImVec2 contentSize = ImGui::GetWindowSize();
        drawList->AddRectFilled(contentPos, contentPos + contentSize, 
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.102f, 0.114f, 0.133f, 1.0f)));
        
        switch (Tabs::m_iCurrentTab)
        {
        case 0:
            RenderAimbotPanel();
            break;
        case 1:
            RenderVisualsPanel();
            break;
        case 2:
            RenderMiscellaneousPanel();
            break;
        case 3:
            RenderWebMenuPanel();
            break;
        case 4:
            RenderConfigPanel();
            break;
        }
        
        ImGui::EndChild();
        
        ImGui::End();
    }
}
