#include "RoboCatPCH.h"

std::unique_ptr< WindowManager >	WindowManager::sInstance = nullptr;

bool WindowManager::StaticInit()
{
	int screenW = static_cast<int>( std::ceilf( kWorldZoomFactor * kWorldWidth ) );
	int screenH = static_cast<int>( std::ceilf( kWorldZoomFactor * kWorldHeight ) );
	SDL_Window* wnd = SDL_CreateWindow( "Robo Cat RTS", 100, 100, screenW, screenH, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE );
	
	if (wnd == nullptr)
	{
		SDL_LogError( SDL_LOG_CATEGORY_ERROR, "Failed to create window." );
		return false;
	}

 	sInstance.reset( new WindowManager( wnd ) );

	return true;
}


WindowManager::WindowManager( SDL_Window* inMainWindow )
{
	mMainWindow = inMainWindow;	

}

WindowManager::~WindowManager()
{
	SDL_DestroyWindow( mMainWindow );
}
