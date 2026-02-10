#pragma once

/**
 * CAimbot - External Aimbot for CS2
 * 
 * This is a clean, working implementation based on best practices for CS2 external cheats.
 * It reads game memory to find targets and calculates angles for aiming.
 * 
 * Features:
 * - Target selection based on FOV and distance
 * - Smooth mouse movement with configurable smoothing
 * - Automatic sensitivity calibration
 * - Visibility checks (optional)
 * - Multiple bone targeting options
 * 
 * Security: This code only performs local memory reading and mouse input.
 * No network connections, data exfiltration, or malicious operations.
 */
class CAimbot
{
public:
	/**
	 * Main aimbot execution function
	 * Should be called every frame in the main loop
	 */
	static void Run();
	
	/**
	 * Manual calibration function
	 * Measures sensitivity for accurate angle-to-pixel conversion
	 */
	static void Calibrate();
	
	/**
	 * Get current sensitivity calibration value
	 * @return Sensitivity value (degrees per pixel)
	 */
	static float GetSensitivity() { return m_flSensitivity; }
	
	/**
	 * Check if aimbot is calibrated
	 * @return True if calibrated, false otherwise
	 */
	static bool IsCalibrated() { return m_bCalibrated; }
	
private:
	/**
	 * Find the best target to aim at
	 * @param vecTargetPos Output: Target bone position
	 * @return True if valid target found
	 */
	static bool FindTarget(Vector& vecTargetPos);
	
	/**
	 * Calculate aim angles to target
	 * @param vecTarget Target position
	 * @param angOut Output: Aim angles (delta from current view)
	 */
	static void CalculateAimAngles(const Vector& vecTarget, QAngle& angOut);
	
	/**
	 * Convert angles to pixel movement
	 * @param angAim Input angles
	 * @param iX Output: Horizontal pixels
	 * @param iY Output: Vertical pixels
	 */
	static void AnglesToPixels(const QAngle& angAim, int& iX, int& iY);
	
	/**
	 * Apply smoothing to pixel movement
	 * @param iX Input/Output: Horizontal pixels
	 * @param iY Input/Output: Vertical pixels
	 */
	static void ApplySmoothing(int& iX, int& iY);
	
	/**
	 * Check if aim key is pressed
	 * @return True if key is down
	 */
	static bool IsAimKeyPressed();
	
	/**
	 * Check if weapon is melee
	 * @return True if melee weapon
	 */
	static bool IsMeleeWeapon();
	
	/**
	 * Get bone index from hitbox enum
	 * @param iHitbox Hitbox enum value
	 * @return CS2 bone index
	 */
	static int GetBoneIndex(int iHitbox);
	
	/**
	 * Normalize angle to -180 to 180 range
	 * @param flAngle Input angle
	 * @return Normalized angle
	 */
	static float NormalizeAngle(float flAngle);
	
	/**
	 * Calculate FOV distance in degrees
	 * @param angCurrent Current view angles
	 * @param angTarget Target angles
	 * @return FOV distance in degrees
	 */
	static float GetFovDistance(const QAngle& angCurrent, const QAngle& angTarget);
	
	// Calibration state
	static float m_flSensitivity;              // Degrees per pixel
	static bool m_bCalibrated;                  // Calibration status
	static bool m_bCalibrating;                 // Currently calibrating
	static bool m_bCrashed;                     // Safety flag to disable aimbot after crash
	
	// Timing
	static std::chrono::steady_clock::time_point m_tLastCalibration;
	static std::chrono::steady_clock::time_point m_tGameStartTime;
	
	// Constants
	static constexpr float DEFAULT_SENSITIVITY = 0.022f;  // Default degrees per pixel
	static constexpr float MAX_DISTANCE = 5000.0f;        // Max target distance
	static constexpr float MIN_ANGLE_DIFF = 0.01f;       // Minimum angle difference to aim
};

inline CAimbot g_Aimbot;

