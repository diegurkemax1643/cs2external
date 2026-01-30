#pragma once

class CAimbot
{
public:
	static void Run();
	static void Calibrate();
	static void AutoCalibrateOnGameStart(); // Auto-calibrate when joining game
	static double GetAnglePerPixel() { return m_flAnglePerPixel; }
	
private:
	static bool GetAimTarget(Vector& vecTargetPosition, std::uintptr_t& uTargetAddress);
	static void GetAimAngles(const Vector& vecTargetPosition, QAngle& angAimAngles);
	static void GetAimPixels(const QAngle& angAimAngles, int& iDeltaX, int& iDeltaY);
	static double CalibrationMeasureAnglePerPixel(int iDeltaPixels);
	static bool IsAimKeyDown();
	static bool IsMeleeWeapon();
	
	static double m_flAnglePerPixel;
	static bool m_bIsCalibrated;
	static bool m_bCalibrationInProgress;
	static std::chrono::steady_clock::time_point m_tLastCalibration;
	static std::chrono::steady_clock::time_point m_tWindowFocusRegainTime;
	static std::chrono::steady_clock::time_point m_tGameStartTime;
	static bool m_bLastWindowActiveState;
	static bool m_bHasStartedCalibration;
};

inline CAimbot g_Aimbot;

