#pragma once

enum EVisualsModifiers : std::uint8_t
{
	VISUALS_IGNORE_TEAMMATES = 0U,
	VISUALS_ONLY_WHEN_VISIBLE,
	VISUALS_MAX
};

enum EAimKeyType : std::uint8_t
{
	AIM_KEY_KEYBOARD = 0U,
	AIM_KEY_MOUSE
};

enum ELinesEspOrigin : std::uint8_t
{
	LINES_ESP_ORIGIN_TOP = 0U,
	LINES_ESP_ORIGIN_BOTTOM,
	LINES_ESP_ORIGIN_CROSSHAIR
};

class CVariables
{
public:
	struct GuiVariables_t
	{
		CONFIG_ADD_VARIABLE(int, m_iMenuKey, VK_INSERT);
		CONFIG_ADD_VARIABLE(int, m_iUnloadKey, VK_DELETE);
		
		CONFIG_ADD_VARIABLE(bool, m_bExcludeFromDesktopCapture, false);
	}; GuiVariables_t m_Gui;

	struct AimBotVariables_t
	{
		CONFIG_ADD_VARIABLE(bool, m_bEnableAimbot, false);
		CONFIG_ADD_VARIABLE(int, m_iAimbotFov, 300); // FOV in pixels (default 300)
		CONFIG_ADD_VARIABLE(int, m_iAimbotSmoothness, 5);
		CONFIG_ADD_VARIABLE(bool, m_bAimbotVisibilityCheck, true);
		CONFIG_ADD_VARIABLE(int, m_iAimbotBoneIndex, EHitBoxes::HITBOX_HEAD);
		CONFIG_ADD_VARIABLE(bool, m_bShowFov, true);
		CONFIG_ADD_VARIABLE(int, m_iAimKey, VK_LMENU); // Left Alt
		CONFIG_ADD_VARIABLE(int, m_iAimMouseButton, VK_XBUTTON1);
		CONFIG_ADD_VARIABLE(int, m_iAimKeyType, EAimKeyType::AIM_KEY_KEYBOARD);
	}; AimBotVariables_t m_AimBot;
	
	struct TriggerBotVariables_t
	{
		CONFIG_ADD_VARIABLE(bool, m_bEnableTriggerbot, false);
		CONFIG_ADD_VARIABLE(int, m_iTriggerBotHotKey, VK_LMENU); // Left Alt
		CONFIG_ADD_VARIABLE(int, m_iTriggerBotDelay, 5); // Delay in milliseconds
		CONFIG_ADD_VARIABLE(float, m_flTriggerBotMaxVelocity, 18.0f); // Maximum velocity threshold
	}; TriggerBotVariables_t m_TriggerBot;

	struct PlayerVisualsVariables_t
	{
		CONFIG_ADD_VARIABLE(bool, m_bEnableVisuals, true);
		CONFIG_ADD_VARIABLE_VECTOR(bool, EVisualsModifiers::VISUALS_MAX, m_vecVisualsModifiers, false);
		CONFIG_ADD_VARIABLE(bool, m_bShowTeammates, false); // Show ESP on teammates
		
		// ESP Types
		CONFIG_ADD_VARIABLE(bool, m_bBoxEspEnabled, true);
		CONFIG_ADD_VARIABLE(bool, m_bKirkEspEnabled, false);
		CONFIG_ADD_VARIABLE(bool, m_bSkeletonEspEnabled, true);
		CONFIG_ADD_VARIABLE(bool, m_bWireframeEspEnabled, false);
		CONFIG_ADD_VARIABLE(bool, m_bIndicatorEspEnabled, false);
		CONFIG_ADD_VARIABLE(bool, m_bHealthBarEnabled, true);
		CONFIG_ADD_VARIABLE(bool, m_bNameEnabled, true);
		CONFIG_ADD_VARIABLE(bool, m_bSelfEspEnabled, false);
		CONFIG_ADD_VARIABLE(bool, m_bCorneredBoxEnabled, false);
		CONFIG_ADD_VARIABLE(bool, m_bBoxFillEnabled, false);
		CONFIG_ADD_VARIABLE(bool, m_bOverrideHealthColorEnabled, false);
		CONFIG_ADD_VARIABLE(bool, m_bHeadCircleEnabled, false);
		CONFIG_ADD_VARIABLE(bool, m_bBombInfoEnabled, false);
		CONFIG_ADD_VARIABLE(bool, m_bBombLocationEnabled, false);
		CONFIG_ADD_VARIABLE(bool, m_bDroppedWeaponsEnabled, false);
		CONFIG_ADD_VARIABLE(bool, m_bDroppedGrenadesEnabled, false);
		CONFIG_ADD_VARIABLE(bool, m_bForceIconsEnabled, false);
		CONFIG_ADD_VARIABLE(bool, m_bEnablePreview, false);
		
		// ESP Lines
		CONFIG_ADD_VARIABLE(bool, m_bLinesEspEnabled, false);
		CONFIG_ADD_VARIABLE(int, m_iLinesEspOrigin, ELinesEspOrigin::LINES_ESP_ORIGIN_CROSSHAIR);
		
		// ESP Colors
		CONFIG_ADD_VARIABLE(Color, m_colESPBox, Color(255, 255, 80, 255)); // Default ESP color
		CONFIG_ADD_VARIABLE(Color, m_colESPLine, Color(255, 255, 255, 255)); // White line color
		CONFIG_ADD_VARIABLE(Color, m_colESPText, Color(255, 255, 255, 255)); // White text color
		CONFIG_ADD_VARIABLE(Color, m_colSkeletonEsp, Color(80, 180, 255, 255)); // Skeleton color
		CONFIG_ADD_VARIABLE(Color, m_colLinesEsp, Color(255, 255, 80, 255)); // Lines ESP color
		
		// ESP Gradient
		CONFIG_ADD_VARIABLE(bool, m_bBoxGradientEnabled, false);
		CONFIG_ADD_VARIABLE(bool, m_bBoxGradientFilledEnabled, false);
		CONFIG_ADD_VARIABLE(bool, m_bBoxGradientOutlineEnabled, false);
		CONFIG_ADD_VARIABLE(Color, m_colBoxGradient1, Color(67, 181, 129, 255)); // #43B581
		CONFIG_ADD_VARIABLE(Color, m_colBoxGradient2, Color(181, 67, 129, 255)); // #B54381
		
		// ESP Settings
		CONFIG_ADD_VARIABLE(int, m_iEspTransparency, 100); // 0-100, where 100 = fully opaque
		CONFIG_ADD_VARIABLE(int, m_iEspThickness, 1); // Line thickness
		
	}; PlayerVisualsVariables_t m_PlayerVisuals;

	struct MinimapEspVariables_t
	{
		CONFIG_ADD_VARIABLE(bool, m_bMinimapEspEnabled, false);
		CONFIG_ADD_VARIABLE(int, m_iMinimapSize, 200); // Size in pixels
		CONFIG_ADD_VARIABLE(int, m_iMinimapRange, 2000); // Range in game units
		CONFIG_ADD_VARIABLE(int, m_iMinimapX, 10); // X position on screen
		CONFIG_ADD_VARIABLE(int, m_iMinimapY, 10); // Y position on screen
		CONFIG_ADD_VARIABLE(Color, m_colMinimapEnemy, Color(255, 0, 0, 255)); // Red for enemies
		CONFIG_ADD_VARIABLE(Color, m_colMinimapTeam, Color(0, 255, 0, 255)); // Green for teammates
		CONFIG_ADD_VARIABLE(bool, m_bMinimapShowPlayerDirection, true);
		CONFIG_ADD_VARIABLE(float, m_flMinimapRotationAdjustment, 0.0f); // Rotation adjustment in degrees
	}; MinimapEspVariables_t m_MinimapEsp;

	struct WidgetVariables_t
	{
		// Widget Toggles
		CONFIG_ADD_VARIABLE(bool, m_bFpsWidgetEnabled, true);
		CONFIG_ADD_VARIABLE(bool, m_bBombTimerWidgetEnabled, true);
		CONFIG_ADD_VARIABLE(bool, m_bPlayerCountWidgetEnabled, false);
		CONFIG_ADD_VARIABLE(bool, m_bHealthArmorWidgetEnabled, false);
		CONFIG_ADD_VARIABLE(bool, m_bWeaponAmmoWidgetEnabled, false);
		CONFIG_ADD_VARIABLE(bool, m_bSpectatorListWidgetEnabled, false);
		
		// Widget Positions
		CONFIG_ADD_VARIABLE(float, m_flFpsWidgetX, 10.0f);
		CONFIG_ADD_VARIABLE(float, m_flFpsWidgetY, 10.0f);
		CONFIG_ADD_VARIABLE(float, m_flBombTimerWidgetX, 10.0f);
		CONFIG_ADD_VARIABLE(float, m_flBombTimerWidgetY, 40.0f);
		CONFIG_ADD_VARIABLE(float, m_flIndicatorEspWidgetX, 10.0f);
		CONFIG_ADD_VARIABLE(float, m_flIndicatorEspWidgetY, 70.0f);
		CONFIG_ADD_VARIABLE(float, m_flPlayerCountWidgetX, 10.0f);
		CONFIG_ADD_VARIABLE(float, m_flPlayerCountWidgetY, 100.0f);
		CONFIG_ADD_VARIABLE(float, m_flHealthArmorWidgetX, 10.0f);
		CONFIG_ADD_VARIABLE(float, m_flHealthArmorWidgetY, 130.0f);
		CONFIG_ADD_VARIABLE(float, m_flWeaponAmmoWidgetX, 10.0f);
		CONFIG_ADD_VARIABLE(float, m_flWeaponAmmoWidgetY, 160.0f);
		CONFIG_ADD_VARIABLE(float, m_flSpectatorListWidgetX, 10.0f);
		CONFIG_ADD_VARIABLE(float, m_flSpectatorListWidgetY, 190.0f);
	}; WidgetVariables_t m_Widgets;

	struct WebServerVariables_t
	{
		CONFIG_ADD_VARIABLE(bool, m_bWebServerEnabled, true);
		CONFIG_ADD_VARIABLE(int, m_iWebServerPort, 8080);
		CONFIG_ADD_VARIABLE(int, m_iWebServerUpdateInterval, 1000); // Update interval in milliseconds
		CONFIG_ADD_VARIABLE(bool, m_bWebServerTunnelEnabled, false); // Cloudflared tunnel enabled
	}; WebServerVariables_t m_WebServer;

};
inline CVariables g_Variables;