#include "RoboCatPCH.h"
#include <time.h>
#include <SDL.h>

std::unique_ptr< Engine >	Engine::sInstance = nullptr;


Engine::Engine() :
mShouldKeepRunning( true )
{

}

bool Engine::StaticInit()
{
	RandGen::StaticInit();

	GameObjectRegistry::StaticInit();


	World::StaticInit();

	ScoreBoardManager::StaticInit();

	// need to initialize Steam before SDL so the overlay works
	if( !GamerServices::StaticInit() )
	{
		return false;
	}

	SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO );

	if( !WindowManager::StaticInit() )
	{
		return false;
	}

	if( !GraphicsDriver::StaticInit( WindowManager::sInstance->GetMainWindow() ) )
	{
		return false;
	}

	TextureManager::StaticInit();
	RenderManager::StaticInit();
	InputManager::StaticInit();

	HUD::StaticInit();


	sInstance.reset( new Engine() );

 	GameObjectRegistry::sInstance->RegisterCreationFunction( 'RCAT', RoboCat::StaticCreate );
 	GameObjectRegistry::sInstance->RegisterCreationFunction( 'YARN', Yarn::StaticCreate );
 
	NetworkManager::StaticInit();

	return true;
}

Engine::~Engine()
{

	SDL_Quit();
}

int Engine::Run()
{
	return DoRunLoop();
}

void Engine::HandleEvent( SDL_Event* inEvent )
{
	switch( inEvent->type )
	{
	case SDL_KEYDOWN:
		InputManager::sInstance->HandleInput( EIA_Pressed, inEvent->key.keysym.sym );
		break;
	case SDL_KEYUP:
		InputManager::sInstance->HandleInput( EIA_Released, inEvent->key.keysym.sym );
		break;
	case SDL_MOUSEBUTTONDOWN:
		InputManager::sInstance->HandleMouseClick( inEvent->button.x, inEvent->button.y, inEvent->button.button );
		break;
	default:
		break;
	}
}

int Engine::DoRunLoop()
{
	// Main message loop
	bool quit = false;
	SDL_Event event;
	memset( &event, 0, sizeof( SDL_Event ) );

	while( !quit && mShouldKeepRunning )
	{
		if ( SDL_PollEvent( &event ) )
		{
			if ( event.type == SDL_QUIT ||
				( event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE ) )
			{
				quit = true;
			}
			else
			{
				HandleEvent( &event );
			}
		}
		else
		{
			Timing::sInstance.Update();

			DoFrame();
		}
	}

	return event.type;
}

void Engine::DoFrame()
{
	InputManager::sInstance->Update();

	GamerServices::sInstance->Update();

	if ( NetworkManager::sInstance->GetState() != NetworkManager::NMS_Delay )
	{
		float delta = Timing::sInstance.GetDeltaTime();
		World::sInstance->Update( delta );
		NetworkManager::sInstance->ProcessIncomingPackets();
		NetworkManager::sInstance->SendOutgoingPackets();
	}
	else
	{
		NetworkManager::sInstance->UpdateDelay();
	}

	RenderManager::sInstance->Render();
}	
