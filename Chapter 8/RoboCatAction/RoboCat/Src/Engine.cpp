#include <RoboCatPCH.h>
#include <time.h>
#include <SDL.h>

std::unique_ptr< Engine >	Engine::sInstance;


Engine::Engine() :
mShouldKeepRunning( true )
{
	SocketUtil::StaticInit();

	srand( static_cast< uint32_t >( time( nullptr ) ) );
	
	GameObjectRegistry::StaticInit();


	World::StaticInit();

	ScoreBoardManager::StaticInit();

	SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO );
}

Engine::~Engine()
{
	SocketUtil::CleanUp();

	SDL_Quit();
}




int Engine::Run()
{
	return DoRunLoop();
}

void Engine::HandleEvent( SDL_Event* inEvent )
{
	// Default implementation does nothing, up to derived classes to handle them, if they so choose
	( void )inEvent;
}

int Engine::DoRunLoop()
{
	// Main message loop
	bool quit = false;
	SDL_Event event;
	memset( &event, 0, sizeof( SDL_Event ) );

	while( !quit && mShouldKeepRunning )
	{
		if( SDL_PollEvent( &event ) )
		{
			if( event.type == SDL_QUIT )
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
	World::sInstance->Update();
}

	
