#include "RoboCatPCH.h"


#if !_WIN32
	#include <chrono>
	using namespace std::chrono;
#endif

Timing	Timing::sInstance;

namespace
{
#if _WIN32
	LARGE_INTEGER sStartTime = { 0 };
#else
	high_resolution_clock::time_point sStartTime;
#endif
}

Timing::Timing()
{
#if _WIN32
	LARGE_INTEGER perfFreq;
	QueryPerformanceFrequency( &perfFreq );
	mPerfCountDuration = 1.0 / perfFreq.QuadPart;

	QueryPerformanceCounter( &sStartTime );

	mLastFrameStartTime = GetTime();
#else
	sStartTime = high_resolution_clock::now();
#endif
}

void Timing::Update()
{

	double currentTime = GetTime();

    mDeltaTime = ( float ) ( currentTime - mLastFrameStartTime );
	
	mLastFrameStartTime = currentTime;
	mFrameStartTimef = static_cast< float > ( mLastFrameStartTime );

}

double Timing::GetTime() const
{
#if _WIN32
	LARGE_INTEGER curTime, timeSinceStart;
	QueryPerformanceCounter( &curTime );

	timeSinceStart.QuadPart = curTime.QuadPart - sStartTime.QuadPart;

	return timeSinceStart.QuadPart * mPerfCountDuration;
#else
	auto now = high_resolution_clock::now();
	auto ms = duration_cast< milliseconds >( now - sStartTime ).count();
	//a little uncool to then convert into a double just to go back, but oh well.
	return static_cast< double >( ms ) / 1000;
#endif
}