#include "RoboCatPCH.h"

unique_ptr< InputManager >	InputManager::sInstance;

void InputManager::StaticInit()
{
	sInstance.reset( new InputManager() );
}

void InputManager::HandleInput( EInputAction inInputAction, int inKeyCode )
{
	switch( inKeyCode )
	{
	case '+':
	case '=':
		{
			float latency = NetworkManager::sInstance->GetSimulatedLatency();
			latency += 0.1f;
			if( latency > 0.5f )
			{
				latency = 0.5f;
			}
			NetworkManager::sInstance->SetSimulatedLatency( latency );
			break;
		}
	case '-':
		{
			float latency = NetworkManager::sInstance->GetSimulatedLatency();
			latency -= 0.1f;
			if( latency < 0.0f )
			{
				latency = 0.0f;
			}
			NetworkManager::sInstance->SetSimulatedLatency( latency );
			break;
		}
	case SDLK_RETURN:
		//start the game!
		NetworkManager::sInstance->TryStartGame();
		break;
	default:
		break;
	}

}


void InputManager::HandleMouseClick( int32_t inX, int32_t inY, uint8_t button )
{
	float worldX = inX / kWorldZoomFactor - kWorldWidth / 2.0f;
	float worldY = inY / kWorldZoomFactor - kWorldHeight / 2.0f;
	switch( button )
	{
	case SDL_BUTTON_LEFT:
		mSelectedNetId = World::sInstance->TrySelectGameObject( Vector3( worldX, worldY, 0.0f ) );
		break;
	case SDL_BUTTON_RIGHT:
		GenerateRightClickCommand( Vector3( worldX, worldY, 0.0f ) );
		break;
	default:
		break;
	}
}

void InputManager::GenerateRightClickCommand( const Vector3& inWorldPos )
{
	if ( mSelectedNetId > 0 )
	{
		//need to figure out if this is an attack or a moveto command
		uint32_t targetId = World::sInstance->TrySelectGameObject( inWorldPos );

		CommandPtr cmd;
		if ( targetId > 0 )
		{
			cmd = AttackCommand::StaticCreate( mSelectedNetId, targetId );
		}

		//fallback in case the attack command was invalid
		if ( !cmd )
		{
			cmd = MoveCommand::StaticCreate( mSelectedNetId, inWorldPos );
		}

		if ( cmd )
		{
			mCommandList.AddCommand( cmd );
		}
	}
}

InputManager::InputManager() :
mSelectedNetId( 0 )
{

}

void InputManager::Update()
{
	//if we wanted keyboard polling, etc, could do it here
	//but just using SDL event system so no need
}
