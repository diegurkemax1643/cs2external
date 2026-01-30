#include "../Includes.h"

double CAimbot::m_flAnglePerPixel = 0.0;
bool CAimbot::m_bIsCalibrated = false;
bool CAimbot::m_bCalibrationInProgress = false;
std::chrono::steady_clock::time_point CAimbot::m_tLastCalibration = std::chrono::steady_clock::now();
std::chrono::steady_clock::time_point CAimbot::m_tWindowFocusRegainTime = std::chrono::steady_clock::time_point::min();
std::chrono::steady_clock::time_point CAimbot::m_tGameStartTime = std::chrono::steady_clock::time_point::min();
bool CAimbot::m_bLastWindowActiveState = false;
bool CAimbot::m_bHasStartedCalibration = false;

constexpr int CALIBRATION_RECHECK_INTERVAL_MINUTES = 5;
constexpr int CALIBRATION_DELAY_AFTER_FOCUS_MS = 500;
constexpr int CALIBRATION_DELAY_AFTER_GAME_START_MS = 2000;

void CAimbot::Run()
{
	if (!g_Globals.m_LocalPlayer.m_pPlayerPawn || !g_Globals.m_LocalPlayer.m_pPlayerPawn->IsAlive())
	{
		if (m_bHasStartedCalibration)
		{
			m_bHasStartedCalibration = false;
			m_bIsCalibrated = false;
			m_tGameStartTime = std::chrono::steady_clock::time_point::min();
		}
		return;
	}

	// Auto-calibrate on game start
	AutoCalibrateOnGameStart();

	// Don't run aimbot if calibration is in progress
	if (m_bCalibrationInProgress)
		return;

	if (IsMeleeWeapon())
		return;

	// Check window focus
	if (!g_Memory.IsWindowInForeground(X("Counter-Strike 2")))
	{
		m_bLastWindowActiveState = false;
		return;
	}
	m_bLastWindowActiveState = true;

	// Check if aim key is pressed
	if (!IsAimKeyDown())
		return;

	// Get target
	Vector vecTargetPosition = Vector(0, 0, 0);
	std::uintptr_t uTargetAddress = 0;
	if (!GetAimTarget(vecTargetPosition, uTargetAddress))
		return;

	// Calculate aim angles
	QAngle angAimAngles;
	GetAimAngles(vecTargetPosition, angAimAngles);

	// Convert to pixels
	int iDeltaX = 0, iDeltaY = 0;
	GetAimPixels(angAimAngles, iDeltaX, iDeltaY);

	// Validate values
	if (std::abs(iDeltaX) > 10000 || std::abs(iDeltaY) > 10000)
		return;

	// Apply smoothing
	int iSmoothness = CONFIG_GET(int, g_Variables.m_AimBot.m_iAimbotSmoothness);
	if (iSmoothness > 1 && iSmoothness <= 100)
	{
		iDeltaX = iDeltaX / iSmoothness;
		iDeltaY = iDeltaY / iSmoothness;
	}
	else if (iSmoothness == 0)
	{
		// No smoothing - use raw values
	}

	// Move mouse - ALWAYS move if we have a target and key is pressed
	// Even if values are small, move the mouse
	if (iDeltaX != 0 || iDeltaY != 0)
	{
		// Ensure we always move at least 1 pixel if there's any angle difference
		if (std::abs(angAimAngles.x) > 0.01f && iDeltaX == 0)
			iDeltaX = (angAimAngles.x > 0) ? 1 : -1;
		if (std::abs(angAimAngles.y) > 0.01f && iDeltaY == 0)
			iDeltaY = (angAimAngles.y > 0) ? -1 : 1; // Inverted for pitch
			
		g_Utilities.MouseMove(iDeltaX, iDeltaY);
	}
}

bool CAimbot::GetAimTarget(Vector& vecTargetPosition, std::uintptr_t& uTargetAddress)
{
	if (!g_Globals.m_LocalPlayer.m_pPlayerPawn)
		return false;

	float flMinPixelDist = std::numeric_limits<float>::max();
	Vector vecBestTarget = Vector(0, 0, 0);
	std::uintptr_t uBestAddress = 0;
	bool bTargetFound = false;

	// Lock entity list
	std::shared_lock lock(EntityList::m_mtxEntities);

	if (EntityList::m_vecEntities.empty())
		return false;

	// Get local player info
	Vector vecLocalEyePos = g_Globals.m_LocalPlayer.m_pPlayerPawn->GetEyePosition();
	if (vecLocalEyePos.IsZero())
		return false;

	std::uint8_t uLocalTeam = g_Globals.m_LocalPlayer.m_pPlayerPawn->m_iTeamNum();
	QAngle angCurrentView = g_Interfaces.m_CSGOInput.m_angViewAngle;

	// Get bone index from config
	int iBoneIndex = CONFIG_GET(int, g_Variables.m_AimBot.m_iAimbotBoneIndex);
	int iCS2BoneIndex = 6; // Default to head
	switch (iBoneIndex)
	{
	case EHitBoxes::HITBOX_HEAD:
		iCS2BoneIndex = 6;
		break;
	case EHitBoxes::HITBOX_NECK:
		iCS2BoneIndex = 5;
		break;
	case EHitBoxes::HITBOX_CHEST:
	case EHitBoxes::HITBOX_UPPER_CHEST:
		iCS2BoneIndex = 4;
		break;
	case EHitBoxes::HITBOX_PELVIS:
		iCS2BoneIndex = 0;
		break;
	default:
		iCS2BoneIndex = 6;
		break;
	}

	// Iterate through entities
	for (const auto& entity : EntityList::m_vecEntities)
	{
		if (entity.m_eType != EEntityType::ENTITY_PLAYER)
			continue;

		CCSPlayerController* pController = reinterpret_cast<CCSPlayerController*>(entity.m_pEntity);
		if (!pController || pController->m_bIsLocalPlayerController())
			continue;

		C_CSPlayerPawn* pPawn = reinterpret_cast<C_CSPlayerPawn*>(pController->m_hPawn().Get());
		if (!pPawn || !pPawn->IsAlive())
			continue;

		// Team check
		std::uint8_t uPlayerTeam = pPawn->m_iTeamNum();
		if (uLocalTeam == uPlayerTeam && uLocalTeam != 0)
			continue;

		// Skip visibility check - aim at all enemies for testing
		// This ensures the aimbot will work even if visibility check fails

		// Get bone position
		CGameSceneNode* pSceneNode = pPawn->m_pGameSceneNode();
		if (!pSceneNode)
			continue;

		BoneData_t* pBoneCache = pSceneNode->m_pBoneCache();
		if (!pBoneCache)
			continue;

		// Calculate bone address
		std::uintptr_t uBoneAddress = reinterpret_cast<std::uintptr_t>(pBoneCache) + (iCS2BoneIndex * 0x20);
		
		if (uBoneAddress == 0 || uBoneAddress < reinterpret_cast<std::uintptr_t>(pBoneCache))
			continue;
		
		Vector vecBonePos = g_Memory.ReadMemory<Vector>(uBoneAddress);

		if (vecBonePos.IsZero())
			continue;
			
		if (std::abs(vecBonePos.x) > 50000.0f || 
			std::abs(vecBonePos.y) > 50000.0f || 
			std::abs(vecBonePos.z) > 50000.0f)
			continue;
		
		float flDistance = vecLocalEyePos.DistTo(vecBonePos);
		if (flDistance > 5000.0f)
			continue;

		// Calculate angle to target
		QAngle angTargetAngle = g_Math.CalcAngle(vecLocalEyePos, vecBonePos);
		
		// Calculate angle delta
		float flDeltaYaw = angTargetAngle.x - angCurrentView.x;
		float flDeltaPitch = angTargetAngle.y - angCurrentView.y;
		
		// Normalize
		while (flDeltaYaw > 180.0f) flDeltaYaw -= 360.0f;
		while (flDeltaYaw < -180.0f) flDeltaYaw += 360.0f;
		while (flDeltaPitch > 180.0f) flDeltaPitch -= 360.0f;
		while (flDeltaPitch < -180.0f) flDeltaPitch += 360.0f;
		
		// Calculate FOV distance in pixels
		float flFovPixels = 0.0f;
		if (m_flAnglePerPixel > 0.0 && m_flAnglePerPixel < 1.0)
		{
			float flAngleDiff = std::sqrt(flDeltaYaw * flDeltaYaw + flDeltaPitch * flDeltaPitch);
			flFovPixels = flAngleDiff / m_flAnglePerPixel;
		}
		else
		{
			// Use simple FOV calculation
			float flFov = g_Math.GetFOV(angCurrentView, angTargetAngle);
			flFovPixels = flFov * 12.0f; // Rough estimate
		}
		
		// Check if within FOV (but use a very large default if FOV is too small)
		int iFovPixels = CONFIG_GET(int, g_Variables.m_AimBot.m_iAimbotFov);
		if (iFovPixels < 50) // If FOV is too small, use a reasonable default
			iFovPixels = 500;
		if (flFovPixels > static_cast<float>(iFovPixels))
			continue;

		// Find closest target
		if (flFovPixels < flMinPixelDist && flFovPixels >= 0.0f)
		{
			flMinPixelDist = flFovPixels;
			vecBestTarget = vecBonePos;
			uBestAddress = reinterpret_cast<std::uintptr_t>(pPawn);
			bTargetFound = true;
		}
	}

	if (bTargetFound)
	{
		vecTargetPosition = vecBestTarget;
		uTargetAddress = uBestAddress;
	}

	return bTargetFound;
}

void CAimbot::GetAimAngles(const Vector& vecTargetPosition, QAngle& angAimAngles)
{
	if (!g_Globals.m_LocalPlayer.m_pPlayerPawn)
	{
		angAimAngles = QAngle(0, 0, 0);
		return;
	}

	Vector vecLocalEyePos = g_Globals.m_LocalPlayer.m_pPlayerPawn->GetEyePosition();
	if (vecLocalEyePos.IsZero())
	{
		angAimAngles = QAngle(0, 0, 0);
		return;
	}

	QAngle angCurrentView = g_Interfaces.m_CSGOInput.m_angViewAngle;
	QAngle angDesiredAngle = g_Math.CalcAngle(vecLocalEyePos, vecTargetPosition);

	// Calculate delta angles
	float flDeltaYaw = angDesiredAngle.x - angCurrentView.x;
	float flDeltaPitch = angDesiredAngle.y - angCurrentView.y;

	// Normalize angles
	while (flDeltaYaw > 180.0f)
		flDeltaYaw -= 360.0f;
	while (flDeltaYaw < -180.0f)
		flDeltaYaw += 360.0f;
	
	while (flDeltaPitch > 180.0f)
		flDeltaPitch -= 360.0f;
	while (flDeltaPitch < -180.0f)
		flDeltaPitch += 360.0f;

	angAimAngles.x = flDeltaYaw;
	angAimAngles.y = flDeltaPitch;
	angAimAngles.z = 0.0f;
}

void CAimbot::GetAimPixels(const QAngle& angAimAngles, int& iDeltaX, int& iDeltaY)
{
	// Use a simple, direct conversion
	// For CS2, a rough estimate: 1 degree ≈ 10-15 pixels depending on FOV
	// We'll use a conservative estimate that should work for most setups
	double flAnglePerPixel = m_flAnglePerPixel;
	
	// If not calibrated or invalid, use a simple default
	if (flAnglePerPixel <= 0.0 || flAnglePerPixel > 1.0)
	{
		// Default: approximately 0.022 degrees per pixel (works for 90 FOV at 1920x1080)
		// This is a reasonable default that should work
		flAnglePerPixel = 0.022;
	}

	// Convert angles (in degrees) to pixels
	// X axis (yaw): positive = right, negative = left
	// Y axis (pitch): positive = up, negative = down (inverted for screen)
	// Multiply by a factor to ensure movement is significant
	iDeltaX = static_cast<int>(std::round(angAimAngles.x / flAnglePerPixel));
	iDeltaY = static_cast<int>(std::round(-angAimAngles.y / flAnglePerPixel)); // Negative Y because screen Y is inverted
	
	// Ensure minimum movement if angle is significant
	if (std::abs(angAimAngles.x) > 0.1f && std::abs(iDeltaX) < 1)
		iDeltaX = (iDeltaX >= 0) ? 1 : -1;
	if (std::abs(angAimAngles.y) > 0.1f && std::abs(iDeltaY) < 1)
		iDeltaY = (iDeltaY >= 0) ? 1 : -1;
}

void CAimbot::AutoCalibrateOnGameStart()
{
	if (!g_Globals.m_LocalPlayer.m_pPlayerPawn || !g_Globals.m_LocalPlayer.m_pPlayerPawn->IsAlive())
		return;

	if (!g_Memory.IsWindowInForeground(X("Counter-Strike 2")))
		return;

	if (m_bIsCalibrated && !m_bCalibrationInProgress)
		return;

	auto tNow = std::chrono::steady_clock::now();
	
	if (m_tGameStartTime == std::chrono::steady_clock::time_point::min())
	{
		m_tGameStartTime = tNow;
		m_bHasStartedCalibration = false;
		return;
	}

	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tNow - m_tGameStartTime);
	if (elapsed.count() < CALIBRATION_DELAY_AFTER_GAME_START_MS)
		return;

	if (!m_bCalibrationInProgress && !m_bHasStartedCalibration)
	{
		m_bCalibrationInProgress = true;
		m_bHasStartedCalibration = true;
		
		Calibrate();
		
		m_bCalibrationInProgress = false;
	}
}

void CAimbot::Calibrate()
{
	if (!g_Globals.m_LocalPlayer.m_pPlayerPawn)
		return;

	if (!g_Memory.IsWindowInForeground(X("Counter-Strike 2")))
		return;

	std::vector<double> vecMeasurements;
	vecMeasurements.reserve(5);

	vecMeasurements.push_back(CalibrationMeasureAnglePerPixel(50));
	vecMeasurements.push_back(CalibrationMeasureAnglePerPixel(-50));
	vecMeasurements.push_back(CalibrationMeasureAnglePerPixel(75));
	vecMeasurements.push_back(CalibrationMeasureAnglePerPixel(-75));
	vecMeasurements.push_back(CalibrationMeasureAnglePerPixel(100));

	double flSum = 0.0;
	int iCount = 0;
	for (double flMeasurement : vecMeasurements)
	{
		if (flMeasurement > 0.0 && flMeasurement < 1.0)
		{
			flSum += flMeasurement;
			iCount++;
		}
	}

	if (iCount > 0)
	{
		m_flAnglePerPixel = flSum / static_cast<double>(iCount);
		m_bIsCalibrated = true;
	}
	else
	{
		// Use default
		m_flAnglePerPixel = 0.022;
		m_bIsCalibrated = true;
	}
}

double CAimbot::CalibrationMeasureAnglePerPixel(int iDeltaPixels)
{
	if (!g_Globals.m_LocalPlayer.m_pPlayerPawn)
		return 0.0;

	g_Utilities.Sleep(100.0f);

	QAngle angStartView = g_Interfaces.m_CSGOInput.m_angViewAngle;
	float flStartYaw = angStartView.x;
	
	g_Utilities.MouseMove(iDeltaPixels, 0);
	g_Utilities.Sleep(150.0f);

	QAngle angEndView = g_Interfaces.m_CSGOInput.m_angViewAngle;
	float flEndYaw = angEndView.x;

	float flAngleDiff = flEndYaw - flStartYaw;
	
	while (flAngleDiff > 180.0f)
		flAngleDiff -= 360.0f;
	while (flAngleDiff < -180.0f)
		flAngleDiff += 360.0f;
	
	flAngleDiff = std::abs(flAngleDiff);

	if (std::abs(iDeltaPixels) > 0 && flAngleDiff > 0.1f)
	{
		double flResult = flAngleDiff / std::abs(iDeltaPixels);
		
		if (flResult > 0.001 && flResult < 0.1)
			return flResult;
	}

	return 0.0;
}

bool CAimbot::IsAimKeyDown()
{
	if (!g_Memory.IsWindowInForeground(X("Counter-Strike 2")))
		return false;

	int iKeyType = CONFIG_GET(int, g_Variables.m_AimBot.m_iAimKeyType);
	
	if (iKeyType == EAimKeyType::AIM_KEY_KEYBOARD)
	{
		int iKey = CONFIG_GET(int, g_Variables.m_AimBot.m_iAimKey);
		if (iKey == 0)
			return false;
		SHORT keyState = GetAsyncKeyState(iKey);
		return (keyState & 0x8000) != 0;
	}
	else // Mouse button
	{
		int iMouseButton = CONFIG_GET(int, g_Variables.m_AimBot.m_iAimMouseButton);
		if (iMouseButton == 0)
			return false;
		SHORT keyState = GetAsyncKeyState(iMouseButton);
		return (keyState & 0x8000) != 0;
	}
}

bool CAimbot::IsMeleeWeapon()
{
	if (!g_Globals.m_LocalPlayer.m_pPlayerPawn)
		return false;

	CPlayer_WeaponServices* pWeaponServices = g_Globals.m_LocalPlayer.m_pPlayerPawn->m_pWeaponServices();
	if (!pWeaponServices)
		return false;

	C_BasePlayerWeapon* pWeapon = pWeaponServices->m_hActiveWeapon().Get();
	if (!pWeapon)
		return false;

	CCSWeaponBaseVData* pWeaponData = pWeapon->GetWeaponBaseVData();
	if (!pWeaponData)
		return false;

	CSWeaponType eWeaponType = pWeaponData->m_WeaponType();
	return (eWeaponType == CSWeaponType::WEAPONTYPE_KNIFE || 
			eWeaponType == CSWeaponType::WEAPONTYPE_TASER);
}
