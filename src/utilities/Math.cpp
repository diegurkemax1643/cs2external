#include "../Includes.h"

float CMath::GetFOV( QAngle angViewAngle, QAngle angAimAngle )
{
	Vector vAim = angViewAngle.ToVec();
	Vector vAng = angAimAngle.ToVec();
	// Fix: Use product of lengths, not LengthSqr
	float dot = vAim.DotProduct( vAng );
	float len = vAim.Length( ) * vAng.Length( );
	if ( len == 0.0f ) return 0.0f;
	float fov = M_RAD2DEG( acosf( std::clamp( dot / len, -1.0f, 1.0f ) ) );
	return fov;
}

QAngle CMath::CalcAngle( const Vector& vecStart, const Vector& vecEnd )
{
	Vector vecDelta = vecEnd - vecStart;
	QAngle angView = vecDelta.ToAngles();
	angView.Normalize( );
	return angView;
}