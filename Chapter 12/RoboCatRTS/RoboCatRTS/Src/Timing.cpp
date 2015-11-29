#include "RoboCatPCH.h"

float kDesiredFrameTime = 0.03333333f;
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

	//frame lock at 30fps
	while( mDeltaTime < kDesiredFrameTime )
	{
		currentTime = GetTime();

		mDeltaTime = (float)( currentTime - mLastFrameStartTime );
	}

	//set the delta time to the desired frame time, to try to account
	//for potential slight fluctuations that may occur when exiting the loop
	//this also will handle the frame time not going crazy if spammed with events
	mDeltaTime = kDesiredFrameTime;
	
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