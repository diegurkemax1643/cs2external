#include "../Includes.h"

// Static member initialization
float CAimbot::m_flSensitivity = DEFAULT_SENSITIVITY;
bool CAimbot::m_bCalibrated = false;
bool CAimbot::m_bCalibrating = false;
std::chrono::steady_clock::time_point CAimbot::m_tLastCalibration = std::chrono::steady_clock::now();
std::chrono::steady_clock::time_point CAimbot::m_tGameStartTime = std::chrono::steady_clock::time_point::min();
bool CAimbot::m_bCrashed = false; // Safety flag to disable aimbot after crash

/**
 * Main aimbot execution
 */
void CAimbot::Run()
{
	// SAFETY: If aimbot crashed before, disable it completely
	if (m_bCrashed)
		return;

	// Wrap entire function in try-catch to prevent crashes
	try
	{
		// Check if aimbot is enabled
		if (!CONFIG_GET(bool, g_Variables.m_AimBot.m_bEnableAimbot))
			return;
		
		// SAFETY: Early validation - check if globals are valid
		if (!g_Globals.m_LocalPlayer.m_pPlayerPawn)
			return;


	// Validate local player - SAFETY: Wrap IsAlive() in try-catch
	if (!g_Globals.m_LocalPlayer.m_pPlayerPawn)
	{
		m_bCalibrated = false;
		m_tGameStartTime = std::chrono::steady_clock::time_point::min();
		return;
	}
	
	bool bIsAlive = false;
	try
	{
		bIsAlive = g_Globals.m_LocalPlayer.m_pPlayerPawn->IsAlive();
	}
	catch (...)
	{
		return;
	}
	
	if (!bIsAlive)
	{
		m_bCalibrated = false;
		m_tGameStartTime = std::chrono::steady_clock::time_point::min();
		return;
	}

		// Check window focus
		if (!g_Memory.IsWindowInForeground(X("Counter-Strike 2")))
			return;

		// Check if aim key is pressed - SAFETY: Wrap in try-catch
		bool bAimKeyPressed = false;
		try
		{
			bAimKeyPressed = IsAimKeyPressed();
		}
	catch (...)
	{
		return;
	}

		if (!bAimKeyPressed)
			return;

		// Skip calibration - not needed, aimbot works without it
		// Mark as calibrated to use default sensitivity
		if (!m_bCalibrated)
		{
			m_flSensitivity = DEFAULT_SENSITIVITY;
			m_bCalibrated = true;
		}

		// Find target (SEH will catch access violations)
		Vector vecTargetPos = Vector(0, 0, 0);
		bool found = FindTarget(vecTargetPos);

	if (!found)
	{
		return;
	}

		// Validate target position before calculating angles
		if (vecTargetPos.IsZero() || std::abs(vecTargetPos.x) > 100000.0f || std::abs(vecTargetPos.y) > 100000.0f || std::abs(vecTargetPos.z) > 100000.0f)
		{
			return;
		}

		// CRITICAL: Validate target is above ground (not aiming at ground)
		// SAFETY: Wrap eye position access in try-catch
		Vector vecLocalEyePos;
		try
		{
			vecLocalEyePos = g_Globals.m_LocalPlayer.m_pPlayerPawn->GetEyePosition();
		}
	catch (...)
	{
		return;
	}
		
	if (vecLocalEyePos.IsZero())
	{
		return;
	}
		
	float flHeightDiff = vecTargetPos.z - vecLocalEyePos.z;
	if (flHeightDiff < -50.0f) // If target is more than 50 units below eye, it's likely ground
	{
		return;
	}

		// NEW APPROACH: Use WorldToScreen to project target to screen, then calculate mouse movement
		// This is the same method ESP uses, so it will aim exactly where the head circle is
		ImVec2 vecScreenTarget;
		try
		{
			if (!Draw::WorldToScreen(vecTargetPos, vecScreenTarget))
			{
				return;
			}
		}
		catch (...)
		{
			return;
		}
		
		// Get screen center (crosshair position)
		int iScreenWidth = GetSystemMetrics(SM_CXSCREEN);
		int iScreenHeight = GetSystemMetrics(SM_CYSCREEN);
		ImVec2 vecScreenCenter(iScreenWidth / 2.0f, iScreenHeight / 2.0f);
		
		// Calculate pixel difference from crosshair to target
		float flDeltaX = vecScreenTarget.x - vecScreenCenter.x;
		float flDeltaY = vecScreenTarget.y - vecScreenCenter.y;
		
		// Convert pixel difference to mouse movement
		// Direct conversion - no sensitivity scaling needed (aimbot works correctly without it)
		// Use direct pixel-to-mouse conversion for all smoothing values
		int iDeltaX = static_cast<int>(std::round(flDeltaX));
		int iDeltaY = static_cast<int>(std::round(flDeltaY));

		// Validate pixel values - allow larger movements
		if (std::abs(iDeltaX) > 10000 || std::abs(iDeltaY) > 10000)
			return;

		// Apply smoothing - SAFETY: Wrap in try-catch
		try
		{
			ApplySmoothing(iDeltaX, iDeltaY);
		}
		catch (...)
		{
			return;
		}

		// CRITICAL: Always move if we have any pixel difference
		// Force minimum movement to ensure aimbot always works
		if (std::abs(flDeltaX) > 0.1f && iDeltaX == 0)
		{
			iDeltaX = (flDeltaX > 0) ? 1 : -1;
		}
		if (std::abs(flDeltaY) > 0.1f && iDeltaY == 0)
		{
			iDeltaY = (flDeltaY > 0) ? 1 : -1;
		}

		// Move mouse - SAFETY: Wrap in try-catch
		if (iDeltaX != 0 || iDeltaY != 0)
		{
			try
			{
				g_Utilities.MouseMove(iDeltaX, iDeltaY);
			}
			catch (...)
			{
				return;
			}
		}
	}
	catch (const std::exception& e)
	{
		m_bCrashed = true; // Disable aimbot after crash
	}
	catch (...)
	{
		m_bCrashed = true; // Disable aimbot after crash
	}
}

/**
 * Find best target
 */
bool CAimbot::FindTarget(Vector& vecTargetPos)
{
	// Wrap in try-catch to prevent crashes
	try
	{
		if (!g_Globals.m_LocalPlayer.m_pPlayerPawn)
			return false;

		// Get local player info - validate before accessing
		if (!g_Globals.m_LocalPlayer.m_pPlayerPawn)
			return false;

		Vector vecLocalEyePos;
		try
		{
			vecLocalEyePos = g_Globals.m_LocalPlayer.m_pPlayerPawn->GetEyePosition();
		}
		catch (...)
		{
			Logger::Log("[Aimbot] Exception getting eye position");
			return false;
		}

		if (vecLocalEyePos.IsZero())
			return false;

		// SAFETY: Wrap team and view angle access in try-catch
		std::uint8_t uLocalTeam = 0;
		QAngle angCurrentView;
		try
		{
			uLocalTeam = g_Globals.m_LocalPlayer.m_pPlayerPawn->m_iTeamNum();
			angCurrentView = g_Interfaces.m_CSGOInput.m_angViewAngle;
			
			// CRITICAL: Validate view angle - if values are invalid, use default
			if (std::abs(angCurrentView.x) > 360.0f || std::abs(angCurrentView.y) > 360.0f || 
				std::isnan(angCurrentView.x) || std::isnan(angCurrentView.y) ||
				std::isinf(angCurrentView.x) || std::isinf(angCurrentView.y))
			{
				Logger::Log("[Aimbot] Invalid view angle detected, using default (0, 0)");
				angCurrentView = QAngle(0.0f, 0.0f, 0.0f);
			}
		}
		catch (...)
		{
			Logger::Log("[Aimbot] Exception getting team or view angle");
			return false;
		}

	// Get settings
	int iFovPixels = CONFIG_GET(int, g_Variables.m_AimBot.m_iAimbotFov);
	if (iFovPixels <= 0)
		iFovPixels = 10000; // Very large FOV to ensure targets are found

		bool bVisibilityCheck = CONFIG_GET(bool, g_Variables.m_AimBot.m_bAimbotVisibilityCheck);
		// SIMPLIFIED: No bone index needed - just use origin + 72 units (same as ESP)
		float flHeadAimOffset = CONFIG_GET(float, g_Variables.m_AimBot.m_flHeadAimOffset);

		// Lock entity list - SAFETY: Wrap in try-catch
		std::shared_lock lock(EntityList::m_mtxEntities);
		
		// SAFETY: Validate entity list pointer and size before accessing
		try
		{
			if (EntityList::m_vecEntities.empty())
			{
				Logger::Log("[Aimbot] Entity list is empty!");
				return false;
			}
		}
		catch (...)
		{
			Logger::Log("[Aimbot] Exception accessing entity list");
			return false;
		}

		// SAFETY: Limit entity list size to prevent crashes from corrupted data
		size_t uMaxEntities = std::min(EntityList::m_vecEntities.size(), size_t(64));
		Logger::Log("[Aimbot] Entity list size: " + std::to_string(EntityList::m_vecEntities.size()) + ", processing max: " + std::to_string(uMaxEntities));
		Logger::Log("[Aimbot] Local team: " + std::to_string((int)uLocalTeam));

	// Prioritize closest enemy on screen (by screen distance from crosshair)
	float flBestScreenDistance = 999999.0f; // Distance in screen pixels from crosshair
	Vector vecBestTarget = Vector(0, 0, 0);
	bool bFound = false;
	int iEntitiesChecked = 0;
	int iEntitiesSkipped = 0;
	
	// Get screen center for distance calculation
	int iScreenWidth = GetSystemMetrics(SM_CXSCREEN);
	int iScreenHeight = GetSystemMetrics(SM_CYSCREEN);
	ImVec2 vecScreenCenter(iScreenWidth / 2.0f, iScreenHeight / 2.0f);

		// Iterate through entities - USE SAME LOGIC AS ESP
		// SAFETY: Wrap entire loop in try-catch to prevent crashes
		// SAFETY: Limit iteration to prevent crashes
		size_t uEntityIndex = 0;
		for (const auto& entity : EntityList::m_vecEntities)
		{
			// SAFETY: Limit number of entities processed
			if (uEntityIndex >= uMaxEntities)
				break;
			uEntityIndex++;
			
			try
			{
				if (entity.m_eType != EEntityType::ENTITY_PLAYER)
					continue;

				iEntitiesChecked++;

				// SAFETY: Validate entity pointer before using
				if (!entity.m_pEntity)
				{
					iEntitiesSkipped++;
					continue;
				}

				CCSPlayerController* pController = reinterpret_cast<CCSPlayerController*>(entity.m_pEntity);
				if (!pController)
				{
					iEntitiesSkipped++;
					continue;
				}

				// CRITICAL: Skip local player controller - check this FIRST
				// SAFETY: Wrap in try-catch to prevent access violations
				bool bIsLocal = false;
				try
				{
					bIsLocal = pController->m_bIsLocalPlayerController();
				}
				catch (...)
				{
					Logger::Log("[Aimbot] Exception checking if local player controller");
					iEntitiesSkipped++;
					continue;
				}

				if (bIsLocal)
				{
					Logger::Log("[Aimbot] Skipped local player controller (m_bIsLocalPlayerController=true)");
					iEntitiesSkipped++;
					continue;
				}

				// Get player pawn - SAFETY: Wrap in try-catch
				C_CSPlayerPawn* pPawn = nullptr;
				try
				{
					pPawn = reinterpret_cast<C_CSPlayerPawn*>(pController->m_hPawn().Get());
				}
				catch (...)
				{
					Logger::Log("[Aimbot] Exception getting pawn from controller");
					iEntitiesSkipped++;
					continue;
				}

				if (!pPawn)
				{
					iEntitiesSkipped++;
					continue;
				}

				// CRITICAL: Check if this is the local player pawn by comparing addresses
				if (g_Globals.m_LocalPlayer.m_pPlayerPawn && pPawn == g_Globals.m_LocalPlayer.m_pPlayerPawn)
				{
					Logger::Log("[Aimbot] Skipped - pawn address matches local player!");
					iEntitiesSkipped++;
					continue;
				}

				// CRITICAL: Also check if controller matches local player controller
				if (g_Globals.m_LocalPlayer.m_pController && pController == g_Globals.m_LocalPlayer.m_pController)
				{
					Logger::Log("[Aimbot] Skipped - controller address matches local player!");
					iEntitiesSkipped++;
					continue;
				}

				// SAFETY: Wrap IsAlive check in try-catch
				bool bIsAlive = false;
				try
				{
					bIsAlive = pPawn->IsAlive();
				}
				catch (...)
				{
					Logger::Log("[Aimbot] Exception checking if pawn is alive");
					iEntitiesSkipped++;
					continue;
				}

				if (!bIsAlive)
				{
					iEntitiesSkipped++;
					continue;
				}

				// Get player team - SAFETY: Wrap in try-catch
				std::uint8_t uPlayerTeam = 0;
				try
				{
					uPlayerTeam = pPawn->m_iTeamNum();
				}
				catch (...)
				{
					Logger::Log("[Aimbot] Exception getting player team");
					iEntitiesSkipped++;
					continue;
				}
				
				// CRITICAL: Use EXACT SAME logic as ESP (line 414 in Main.cpp)
				// ESP uses: if (uPlayerTeam == uLocalTeam || uLocalTeam == 0) continue;
				// This means: skip if same team OR local team is 0
				if (uPlayerTeam == uLocalTeam || uLocalTeam == 0)
				{
					Logger::Log("[Aimbot] Skipped (ESP logic): local=" + std::to_string((int)uLocalTeam) + ", player=" + std::to_string((int)uPlayerTeam));
					iEntitiesSkipped++;
					continue;
				}

				// Log enemy found for debugging
				Logger::Log("[Aimbot] Processing enemy: team=" + std::to_string((int)uPlayerTeam) + ", local=" + std::to_string((int)uLocalTeam));

				// Get position - USE SAME METHOD AS ESP
				// SAFETY: Wrap scene node access in try-catch
				CGameSceneNode* pSceneNode = nullptr;
				Vector vecOrigin;
				try
				{
					Logger::Log("[Aimbot] Getting scene node...");
					pSceneNode = pPawn->m_pGameSceneNode();
					Logger::Log("[Aimbot] Scene node obtained: " + std::to_string(reinterpret_cast<std::uintptr_t>(pSceneNode)));
					
					if (!pSceneNode)
					{
						Logger::Log("[Aimbot] Scene node is null, skipping");
						iEntitiesSkipped++;
						continue;
					}

					Logger::Log("[Aimbot] Reading origin...");
					vecOrigin = pSceneNode->m_vecAbsOrigin();
					Logger::Log("[Aimbot] Origin read: " + std::to_string(vecOrigin.x) + ", " + std::to_string(vecOrigin.y) + ", " + std::to_string(vecOrigin.z));
				}
				catch (...)
				{
					Logger::Log("[Aimbot] Exception getting scene node or origin");
					iEntitiesSkipped++;
					continue;
				}
				
				// Validate origin (same as ESP)
				if (std::abs(vecOrigin.x) > 100000.0f || 
					std::abs(vecOrigin.y) > 100000.0f || 
					std::abs(vecOrigin.z) > 100000.0f)
				{
					iEntitiesSkipped++;
					continue;
				}

				// Use EXACT SAME calculation as ESP head circle fallback (Main.cpp lines 559-564)
				// This ensures aimbot aims exactly where the head circle ESP is drawn
				// NOTE: Using fallback method only (origin + 72) to avoid crashes from bone reading
				Vector vecHead = vecOrigin;
				
				// Check if enemy is crouching and adjust head position accordingly
				float flHeadHeight = 72.0f; // Default standing head height
				try
				{
					CPlayer_MovementServices* pMovementServices = pPawn->m_pMovementServices();
					if (pMovementServices)
					{
						float flDuckAmount = pMovementServices->m_flDuckAmount();
						bool bDucked = pMovementServices->m_bDucked();
						
						// If crouching, interpolate between standing (72) and crouched (~55) head height
						// flDuckAmount ranges from 0.0 (standing) to 1.0 (fully crouched)
						// In CS2, when crouching, the head only goes down by about 15-17 units, not 36
						if (bDucked || flDuckAmount > 0.1f)
						{
							// Interpolate: standing = 72, fully crouched = ~55 (reduced by 17 units)
							flHeadHeight = 72.0f - (flDuckAmount * 17.0f);
						}
						else
						{
							Logger::Log("[Aimbot] Enemy is standing, using default head height: 72.0");
						}
					}
				}
				catch (...)
				{
					Logger::Log("[Aimbot] Exception checking crouch state, using default head height");
					// Use default standing height if we can't read crouch state
				}
				
				vecHead.z += flHeadHeight; // Top of ESP box (approximate head height) - adjusted for crouching
				Logger::Log("[Aimbot] Head position calculated: " + std::to_string(vecHead.x) + ", " + std::to_string(vecHead.y) + ", " + std::to_string(vecHead.z));
				
				// Apply vertical offset to aim slightly lower on head
				// Negative values aim lower, positive values aim higher
				Logger::Log("[Aimbot] Applying head aim offset...");
				// Apply configurable offset
				if (flHeadAimOffset != 0.0f)
				{
					vecHead.z += flHeadAimOffset;
					Logger::Log("[Aimbot] Head aim offset applied: " + std::to_string(flHeadAimOffset));
				}
				// Apply fixed negative offset to aim slightly lower (below the top of head)
				vecHead.z -= 8.0f; // Aim 8 units lower than head top for better accuracy
				Logger::Log("[Aimbot] Fixed lower offset applied: -8.0");

				// CRITICAL: Validate target is above ground level
				Logger::Log("[Aimbot] Validating height difference...");
				Logger::Log("[Aimbot] Local eye pos: " + std::to_string(vecLocalEyePos.x) + ", " + std::to_string(vecLocalEyePos.y) + ", " + std::to_string(vecLocalEyePos.z));
				float flHeightDiff = vecHead.z - vecLocalEyePos.z;
				Logger::Log("[Aimbot] Height diff calculated: " + std::to_string(flHeightDiff));
				if (flHeightDiff < -100.0f) // If target head is more than 100 units below eye, skip
				{
					Logger::Log("[Aimbot] Skipped - target too low (height diff: " + std::to_string(flHeightDiff) + ")");
					iEntitiesSkipped++;
					continue;
				}

				// CRITICAL: Validate this isn't our own position (ESP uses 0.1f check)
				// SAFETY: Check for null pointer before accessing
				Logger::Log("[Aimbot] Validating distance to local player...");
				if (!g_Globals.m_LocalPlayer.m_pPlayerPawn)
				{
					Logger::Log("[Aimbot] Local player pawn is null, skipping");
					iEntitiesSkipped++;
					continue;
				}

				CGameSceneNode* pLocalSceneNode = nullptr;
				try
				{
					Logger::Log("[Aimbot] Getting local scene node...");
					pLocalSceneNode = g_Globals.m_LocalPlayer.m_pPlayerPawn->m_pGameSceneNode();
					Logger::Log("[Aimbot] Local scene node obtained: " + std::to_string(reinterpret_cast<std::uintptr_t>(pLocalSceneNode)));
				}
				catch (...)
				{
					Logger::Log("[Aimbot] Exception getting local scene node");
					iEntitiesSkipped++;
					continue;
				}

				if (!pLocalSceneNode)
				{
					Logger::Log("[Aimbot] Local scene node is null, skipping");
					iEntitiesSkipped++;
					continue;
				}

				// SAFETY: Wrap origin access in try-catch
				Vector vecLocalOrigin;
				try
				{
					Logger::Log("[Aimbot] Reading local origin...");
					vecLocalOrigin = pLocalSceneNode->m_vecAbsOrigin();
					Logger::Log("[Aimbot] Local origin read: " + std::to_string(vecLocalOrigin.x) + ", " + std::to_string(vecLocalOrigin.y) + ", " + std::to_string(vecLocalOrigin.z));
				}
				catch (...)
				{
					Logger::Log("[Aimbot] Exception getting local origin");
					iEntitiesSkipped++;
					continue;
				}

				Logger::Log("[Aimbot] Calculating distance to local player...");
				Vector vecToEnemy = vecHead - vecLocalOrigin;
				float flDistanceToLocal = vecToEnemy.Length();
				Logger::Log("[Aimbot] Distance to local: " + std::to_string(flDistanceToLocal));
				if (flDistanceToLocal < 0.1f) // ESP uses 0.1f - too close, skip
				{
					Logger::Log("[Aimbot] Skipped - too close to local player (distance: " + std::to_string(flDistanceToLocal) + ")");
					iEntitiesSkipped++;
					continue;
				}

				// Distance check - use reasonable limits
				Logger::Log("[Aimbot] Calculating distance to target...");
				float flDistance = vecLocalEyePos.DistTo(vecHead);
				Logger::Log("[Aimbot] Distance to target: " + std::to_string(flDistance));
				if (flDistance > MAX_DISTANCE * 2.0f) // Only check max distance, not min
				{
					Logger::Log("[Aimbot] Target too far: " + std::to_string(flDistance));
					iEntitiesSkipped++;
					continue;
				}

				// Calculate angle to target - SAFETY: Wrap in try-catch
				Logger::Log("[Aimbot] Calculating angle to target...");
				QAngle angTarget;
				try
				{
					Logger::Log("[Aimbot] Calling CalcAngle...");
					angTarget = g_Math.CalcAngle(vecLocalEyePos, vecHead);
					Logger::Log("[Aimbot] Angle calculated: " + std::to_string(angTarget.x) + ", " + std::to_string(angTarget.y));
				}
				catch (...)
				{
					Logger::Log("[Aimbot] Exception calculating angle to target");
					iEntitiesSkipped++;
					continue;
				}
				
				// Calculate FOV distance in degrees (for logging only - FOV check disabled)
				Logger::Log("[Aimbot] Calculating FOV distance...");
				float flFov = 0.0f;
				try
				{
					Logger::Log("[Aimbot] Current view angle: " + std::to_string(angCurrentView.x) + ", " + std::to_string(angCurrentView.y));
					Logger::Log("[Aimbot] Calling GetFovDistance...");
					flFov = GetFovDistance(angCurrentView, angTarget);
					Logger::Log("[Aimbot] FOV distance: " + std::to_string(flFov));
				}
				catch (...)
				{
					Logger::Log("[Aimbot] Exception calculating FOV distance");
					iEntitiesSkipped++;
					continue;
				}
				
				// FOV CHECK DISABLED FOR TESTING - aim at any enemy regardless of position
				// This allows us to test if the aimbot works in general
				/*
				if (flFov > flBestFov)
				{
					Logger::Log("[Aimbot] Target outside FOV: " + std::to_string(flFov) + " > " + std::to_string(flBestFov));
					iEntitiesSkipped++;
					continue;
				}
				*/

				// CRITICAL: Only accept targets that are visible on screen
				// Use WorldToScreen to check if target is actually visible
				ImVec2 vecScreenHead;
				bool bOnScreen = false;
				try
				{
					bOnScreen = Draw::WorldToScreen(vecHead, vecScreenHead);
				}
				catch (...)
				{
					Logger::Log("[Aimbot] Exception checking if target is on screen");
					bOnScreen = false;
				}
				
				if (!bOnScreen)
				{
					Logger::Log("[Aimbot] Target not on screen, skipping");
					iEntitiesSkipped++;
					continue;
				}
				
				// Visibility check - enabled if configured (check after on-screen validation)
				if (bVisibilityCheck)
				{
					// Check if target head is visible (line of sight check)
					bool bIsVisible = false;
					try
					{
						Logger::Log("[Aimbot] Checking visibility: pawn=" + std::to_string(reinterpret_cast<uintptr_t>(pPawn)) + ", eye=" + std::to_string(vecLocalEyePos.x) + "," + std::to_string(vecLocalEyePos.y) + "," + std::to_string(vecLocalEyePos.z) + ", head=" + std::to_string(vecHead.x) + "," + std::to_string(vecHead.y) + "," + std::to_string(vecHead.z));
						bIsVisible = g_Utilities.IsVisible(pPawn, vecLocalEyePos, vecHead);
						Logger::Log("[Aimbot] Visibility check result: " + std::string(bIsVisible ? "VISIBLE" : "NOT VISIBLE"));
					}
					catch (...)
					{
						Logger::Log("[Aimbot] Exception checking visibility");
						bIsVisible = false;
					}
					
					if (!bIsVisible)
					{
						Logger::Log("[Aimbot] Target not visible (visibility check failed), skipping");
						iEntitiesSkipped++;
						continue;
					}
					
					Logger::Log("[Aimbot] Target passed visibility check, continuing...");
				}
				
				// Calculate screen distance from crosshair (center of screen)
				float flScreenDeltaX = vecScreenHead.x - vecScreenCenter.x;
				float flScreenDeltaY = vecScreenHead.y - vecScreenCenter.y;
				float flScreenDistance = std::sqrt(flScreenDeltaX * flScreenDeltaX + flScreenDeltaY * flScreenDeltaY);
				
				// FOV CHECK: Only accept targets within configured FOV (in screen pixels)
				if (flScreenDistance > iFovPixels)
				{
					Logger::Log("[Aimbot] Target outside FOV: " + std::to_string(flScreenDistance) + " > " + std::to_string(iFovPixels));
					iEntitiesSkipped++;
					continue;
				}
				
				// Found valid target - prioritize closest to crosshair on screen (smallest screen distance)
				Logger::Log("[Aimbot] Comparing screen distance: " + std::to_string(flScreenDistance) + " vs best: " + std::to_string(flBestScreenDistance));
				if (flScreenDistance < flBestScreenDistance)
				{
					Logger::Log("[Aimbot] Found valid target! Screen distance: " + std::to_string(flScreenDistance) + ", World distance: " + std::to_string(flDistance) + ", Position: " + std::to_string(vecHead.x) + ", " + std::to_string(vecHead.y) + ", " + std::to_string(vecHead.z));
					Logger::Log("[Aimbot] Setting best target...");
					flBestScreenDistance = flScreenDistance;
					vecBestTarget = vecHead;
					bFound = true;
					Logger::Log("[Aimbot] Best target set successfully");
				}
			}
			catch (const std::exception& e)
			{
				Logger::Log("[Aimbot] Exception processing entity: " + std::string(e.what()));
				iEntitiesSkipped++;
				continue;
			}
			catch (...)
			{
				Logger::Log("[Aimbot] Unknown exception processing entity - skipping");
				iEntitiesSkipped++;
				continue;
			}
		}

		Logger::Log("[Aimbot] Checked: " + std::to_string(iEntitiesChecked) + ", Skipped: " + std::to_string(iEntitiesSkipped) + ", Found: " + std::to_string(bFound ? 1 : 0));

		if (bFound)
		{
			vecTargetPos = vecBestTarget;
			return true;
		}

		return false;
	}
	catch (const std::exception& e)
	{
		Logger::Log("[Aimbot] Exception in FindTarget(): " + std::string(e.what()));
		return false;
	}
	catch (...)
	{
		Logger::Log("[Aimbot] Unknown exception in FindTarget() - caught and handled");
		return false;
	}
}

/**
 * Calculate aim angles
 */
void CAimbot::CalculateAimAngles(const Vector& vecTarget, QAngle& angOut)
{
	// SAFETY: Wrap entire function in try-catch
	try
	{
		if (!g_Globals.m_LocalPlayer.m_pPlayerPawn)
		{
			angOut = QAngle(0, 0, 0);
			return;
		}

		Vector vecLocalEyePos;
		try
		{
			vecLocalEyePos = g_Globals.m_LocalPlayer.m_pPlayerPawn->GetEyePosition();
		}
		catch (...)
		{
			Logger::Log("[Aimbot] Exception getting eye position in CalculateAimAngles");
			angOut = QAngle(0, 0, 0);
			return;
		}
		
		if (vecLocalEyePos.IsZero())
		{
			angOut = QAngle(0, 0, 0);
			return;
		}

		QAngle angCurrent;
		QAngle angDesired;
		try
		{
			angCurrent = g_Interfaces.m_CSGOInput.m_angViewAngle;
			
			// Log the actual view angle for debugging
			Logger::Log("[Aimbot] Raw view angle read: " + std::to_string(angCurrent.x) + ", " + std::to_string(angCurrent.y));
			
			// CRITICAL: Validate view angle - use more lenient checks
			// CS2 view angles should be in -180 to 180 range for yaw, -90 to 90 for pitch
			// But allow larger range to catch actual invalid values
			if (std::abs(angCurrent.x) > 10000.0f || std::abs(angCurrent.y) > 10000.0f ||
				std::isnan(angCurrent.x) || std::isnan(angCurrent.y) ||
				std::isinf(angCurrent.x) || std::isinf(angCurrent.y))
			{
				Logger::Log("[Aimbot] Invalid current view angle detected in CalculateAimAngles (values too extreme), assuming (0, 0)");
				angCurrent = QAngle(0.0f, 0.0f, 0.0f);
			}
			else
			{
				// Normalize the current angle to -180 to 180 range
				angCurrent.x = NormalizeAngle(angCurrent.x);
				angCurrent.y = NormalizeAngle(angCurrent.y);
				Logger::Log("[Aimbot] Normalized current view angle: " + std::to_string(angCurrent.x) + ", " + std::to_string(angCurrent.y));
			}
			
			angDesired = g_Math.CalcAngle(vecLocalEyePos, vecTarget);
		}
		catch (...)
		{
			Logger::Log("[Aimbot] Exception getting view angle, assuming current angle is (0, 0)");
			angCurrent = QAngle(0.0f, 0.0f, 0.0f);
			try
			{
				angDesired = g_Math.CalcAngle(vecLocalEyePos, vecTarget);
			}
			catch (...)
			{
				angOut = QAngle(0, 0, 0);
				return;
			}
		}

		// Calculate delta - SAFETY: Validate angles before normalizing
		if (std::abs(angDesired.x) > 1000000.0f || std::abs(angDesired.y) > 1000000.0f ||
			std::isnan(angDesired.x) || std::isnan(angDesired.y))
		{
			Logger::Log("[Aimbot] Invalid desired angle in CalculateAimAngles");
			angOut = QAngle(0, 0, 0);
			return;
		}
		
		// USER REQUEST: Remove pitch calculation - only aim horizontally (yaw)
		// Calculate only yaw delta to aim at the exact X,Y position of the head ESP
		// Pitch is set to 0 to keep current vertical aim unchanged
		float flDeltaYaw = NormalizeAngle(angDesired.x - angCurrent.x);
		
		// Set pitch delta to 0 - don't change vertical aim
		float flDeltaPitch = 0.0f;

		angOut.x = flDeltaYaw;
		angOut.y = flDeltaPitch; // Always 0 - no pitch adjustment
		angOut.z = 0.0f;
		
		float flHeightDiff = vecTarget.z - vecLocalEyePos.z;
		Logger::Log("[Aimbot] Angle calculation (YAW ONLY): Desired yaw=" + std::to_string(angDesired.x) + ", Current yaw=" + std::to_string(angCurrent.x) + ", Delta yaw=" + std::to_string(flDeltaYaw) + ", Delta pitch=0.0 (disabled), Height diff=" + std::to_string(flHeightDiff));
	}
	catch (...)
	{
		Logger::Log("[Aimbot] Unknown exception in CalculateAimAngles");
		angOut = QAngle(0, 0, 0);
	}
}

/**
 * Convert angles to pixels
 */
void CAimbot::AnglesToPixels(const QAngle& angAim, int& iX, int& iY)
{
	float flSens = m_flSensitivity;
	if (flSens <= 0.0f || flSens > 1.0f || !m_bCalibrated)
	{
		flSens = DEFAULT_SENSITIVITY;
		Logger::Log("[Aimbot] Using default sensitivity: " + std::to_string(flSens));
	}
	else
	{
		Logger::Log("[Aimbot] Using calibrated sensitivity: " + std::to_string(flSens));
	}

	// Convert angles to pixels
	// X (yaw): positive = right, negative = left
	// Y (pitch): In CS2, positive pitch = look up, negative pitch = look down
	// Mouse movement: positive Y = move mouse down (looks up), negative Y = move mouse up (looks down)
	// So we need to invert: if pitch is positive (want to look up), we move mouse down (positive Y)
	float flPixelX = angAim.x / flSens;
	float flPixelY = -angAim.y / flSens; // Inverted: positive pitch (up) -> positive Y (mouse down)
	
	iX = static_cast<int>(std::round(flPixelX));
	iY = static_cast<int>(std::round(flPixelY));
	
	Logger::Log("[Aimbot] Raw pixel calculation: angleX=" + std::to_string(angAim.x) + ", angleY=" + std::to_string(angAim.y) + ", pixelX=" + std::to_string(flPixelX) + ", pixelY=" + std::to_string(flPixelY));
	
	// Clamp values
	if (std::abs(iX) > 10000)
		iX = (iX > 0) ? 10000 : -10000;
	if (std::abs(iY) > 10000)
		iY = (iY > 0) ? 10000 : -10000;
	
	// ALWAYS ensure minimum movement - aimbot must move when key is pressed (yaw only, pitch disabled)
	if (std::abs(angAim.x) > 0.001f && iX == 0)
	{
		iX = (angAim.x > 0) ? 1 : -1;
		Logger::Log("[Aimbot] Applied minimum X movement: " + std::to_string(iX));
	}
	// Pitch is disabled, so iY should always be 0
	if (iY != 0)
	{
		Logger::Log("[Aimbot] WARNING: Y movement should be 0 (pitch disabled), but got: " + std::to_string(iY));
		iY = 0; // Force to 0
	}
}

/**
 * Apply smoothing to movement
 */
void CAimbot::ApplySmoothing(int& iX, int& iY)
{
	int iSmoothness = CONFIG_GET(int, g_Variables.m_AimBot.m_iAimbotSmoothness);
	
	// Validate smoothing value
	if (iSmoothness < 1)
		iSmoothness = 1;
	if (iSmoothness > 100)
		iSmoothness = 100;

	// Smoothing 1 = instant snap (skip smoothing entirely)
	if (iSmoothness == 1)
		return; // No smoothing applied, full speed

	// Inverted smoothing: higher = slower
	// Smoothing 2: flFactor = 0.5 (50% movement)
	// Smoothing 5: flFactor = 0.2 (20% movement)
	// Smoothing 10: flFactor = 0.1 (10% movement)
	// Smoothing 100: flFactor = 0.01 (1% movement)
	float flFactor = 1.0f / static_cast<float>(iSmoothness);
	
	float flSmoothX = static_cast<float>(iX) * flFactor;
	float flSmoothY = static_cast<float>(iY) * flFactor;
	
	iX = static_cast<int>(std::round(flSmoothX));
	iY = static_cast<int>(std::round(flSmoothY));
	
	// Ensure minimum movement after smoothing
	if (std::abs(flSmoothX) > 0.1f && iX == 0)
		iX = (flSmoothX > 0) ? 1 : -1;
	if (std::abs(flSmoothY) > 0.1f && iY == 0)
		iY = (flSmoothY > 0) ? 1 : -1;
}

/**
 * Check if aim key is pressed
 */
bool CAimbot::IsAimKeyPressed()
{
	if (!g_Memory.IsWindowInForeground(X("Counter-Strike 2")))
		return false;

	int iKeyType = CONFIG_GET(int, g_Variables.m_AimBot.m_iAimKeyType);
	
	if (iKeyType == EAimKeyType::AIM_KEY_KEYBOARD)
	{
		int iKey = CONFIG_GET(int, g_Variables.m_AimBot.m_iAimKey);
		if (iKey == 0)
			return false;
		return (GetAsyncKeyState(iKey) & 0x8000) != 0;
	}
	else // Mouse button
	{
		int iMouseButton = CONFIG_GET(int, g_Variables.m_AimBot.m_iAimMouseButton);
		if (iMouseButton == 0)
			return false;
		return (GetAsyncKeyState(iMouseButton) & 0x8000) != 0;
	}
}

/**
 * Check if weapon is melee
 */
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

/**
 * Get bone index from hitbox
 */
int CAimbot::GetBoneIndex(int iHitbox)
{
	switch (iHitbox)
	{
	case EHitBoxes::HITBOX_HEAD:
		return 6;
	case EHitBoxes::HITBOX_NECK:
		return 5;
	case EHitBoxes::HITBOX_CHEST:
	case EHitBoxes::HITBOX_UPPER_CHEST:
		return 4;
	case EHitBoxes::HITBOX_PELVIS:
		return 0;
	default:
		return 6; // Default to head
	}
}

/**
 * Normalize angle
 */
float CAimbot::NormalizeAngle(float flAngle)
{
	// SAFETY: Check for invalid input
	if (std::isnan(flAngle) || std::isinf(flAngle))
	{
		Logger::Log("[Aimbot] Invalid angle in NormalizeAngle: " + std::to_string(flAngle));
		return 0.0f;
	}
	
	// SAFETY: Handle extremely large values
	if (std::abs(flAngle) > 1000000.0f)
	{
		Logger::Log("[Aimbot] Angle too large in NormalizeAngle: " + std::to_string(flAngle));
		return 0.0f;
	}
	
	while (flAngle > 180.0f)
		flAngle -= 360.0f;
	while (flAngle < -180.0f)
		flAngle += 360.0f;
	
	// SAFETY: Validate result
	if (std::isnan(flAngle) || std::isinf(flAngle))
	{
		Logger::Log("[Aimbot] Invalid result in NormalizeAngle");
		return 0.0f;
	}
	
	return flAngle;
}

/**
 * Calculate FOV distance
 */
float CAimbot::GetFovDistance(const QAngle& angCurrent, const QAngle& angTarget)
{
	// SAFETY: Validate inputs before calculation
	if (std::abs(angCurrent.x) > 360.0f || std::abs(angCurrent.y) > 360.0f ||
		std::abs(angTarget.x) > 360.0f || std::abs(angTarget.y) > 360.0f ||
		std::isnan(angCurrent.x) || std::isnan(angCurrent.y) ||
		std::isnan(angTarget.x) || std::isnan(angTarget.y) ||
		std::isinf(angCurrent.x) || std::isinf(angCurrent.y) ||
		std::isinf(angTarget.x) || std::isinf(angTarget.y))
	{
		Logger::Log("[Aimbot] Invalid angles in GetFovDistance, returning 0");
		return 0.0f;
	}
	
	float flDeltaYaw = NormalizeAngle(angTarget.x - angCurrent.x);
	float flDeltaPitch = NormalizeAngle(angTarget.y - angCurrent.y);
	
	// SAFETY: Validate normalized angles
	if (std::isnan(flDeltaYaw) || std::isnan(flDeltaPitch) ||
		std::isinf(flDeltaYaw) || std::isinf(flDeltaPitch))
	{
		Logger::Log("[Aimbot] Invalid normalized angles in GetFovDistance, returning 0");
		return 0.0f;
	}
	
	float flResult = std::sqrt(flDeltaYaw * flDeltaYaw + flDeltaPitch * flDeltaPitch);
	
	// SAFETY: Validate result
	if (std::isnan(flResult) || std::isinf(flResult))
	{
		Logger::Log("[Aimbot] Invalid FOV result, returning 0");
		return 0.0f;
	}
	
	return flResult;
}

/**
 * Calibrate sensitivity (disabled - not needed)
 */
void CAimbot::Calibrate()
{
	// Calibration disabled - just use default sensitivity
	// No mouse movement needed, aimbot works without calibration
	m_flSensitivity = DEFAULT_SENSITIVITY;
	m_bCalibrated = true;
	m_tLastCalibration = std::chrono::steady_clock::now();
}
