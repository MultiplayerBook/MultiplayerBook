#include "RoboCatPCH.h"

std::unique_ptr< HUD >	HUD::sInstance = nullptr;


HUD::HUD() :
mScoreBoardOrigin( 30.f, 60.f, 0.0f ),
mBandwidthOrigin( 30.f, 10.f, 0.0f ),
mRoundTripTimeOrigin( 50.f, 10.f, 0.0f ),
mScoreOffset( 0.f, 50.f, 0.0f ),
mHealthOffset( 1000, 10.f, 0.0f ),
mTimeOrigin( 500.0f, 320.0f, 0.0f ),
mHealth( 0 )
{
	TTF_Init();
	mFont = TTF_OpenFont( "../Assets/Carlito-Regular.TTF", 36 );
	if( mFont == nullptr )
	{
		SDL_LogError( SDL_LOG_CATEGORY_ERROR, "Failed to load font." );
	}
}


void HUD::RenderTurnNumber()
{
	string str = StringUtils::Sprintf( "Turn %d:%d", NetworkManager::sInstance->GetTurnNumber(),
		NetworkManager::sInstance->GetSubTurnNumber() );
	RenderText( str, mBandwidthOrigin, Colors::Blue );
}

void HUD::StaticInit()
{
	sInstance.reset( new HUD() );
}

void HUD::Render()
{
	//RenderBandWidth();
	RenderTurnNumber();
	RenderScoreBoard();
	RenderHealth();
	RenderCountdown();
}

void HUD::RenderHealth()
{
	if( mHealth > 0 )
	{
		string healthString = StringUtils::Sprintf( "Health %d", mHealth );
		RenderText( healthString, mHealthOffset, Colors::Red );
	}
}

void HUD::RenderCountdown()
{
	NetworkManager::NetworkManagerState state = NetworkManager::sInstance->GetState();

	switch ( state )
	{
	case NetworkManager::NMS_Starting:
		{
			float timeToStart = NetworkManager::sInstance->GetTimeToStart();
			if ( timeToStart > 0.0f )
			{
				string timeStr = StringUtils::Sprintf( "STARTING IN %.2f", timeToStart );
				RenderText( timeStr, mTimeOrigin, Colors::Red );
			}
		}
		break;
	case NetworkManager::NMS_Lobby:
		{
			string str = StringUtils::Sprintf( "%d/4 players in lobby", NetworkManager::sInstance->GetPlayerCount() );
			RenderText( str, mTimeOrigin, Colors::Blue );
		}
		break;
	case NetworkManager::NMS_Delay:
		RenderText( "Waiting on other players...", mTimeOrigin, Colors::Red );
		break;
	default:
		break;
	}
}

void HUD::RenderBandWidth()
{
	string bandwidth = StringUtils::Sprintf( "In %d  Bps, Out %d Bps",
												static_cast< int >( NetworkManager::sInstance->GetBytesReceivedPerSecond().GetValue() ),
												static_cast< int >( NetworkManager::sInstance->GetBytesSentPerSecond().GetValue() ) );
	RenderText( bandwidth, mBandwidthOrigin, Colors::White );
}

void HUD::RenderScoreBoard()
{
	const vector< ScoreBoardManager::Entry >& entries = ScoreBoardManager::sInstance->GetEntries();
	Vector3 offset = mScoreBoardOrigin;
	
	for( const auto& entry: entries )
	{
		RenderText( entry.GetFormattedNameScore(), offset, entry.GetColor() );
		offset.mX += mScoreOffset.mX;
		offset.mY += mScoreOffset.mY;
	}

}

void HUD::RenderText( const string& inStr, const Vector3& origin, const Vector3& inColor )
{
	// Convert the color
	SDL_Color color;
	color.r = static_cast<Uint8>( inColor.mX * 255 );
	color.g = static_cast<Uint8>( inColor.mY * 255 );
	color.b = static_cast<Uint8>( inColor.mZ * 255 );
	color.a = 255;

	// Draw to surface and create a texture
	SDL_Surface* surface = TTF_RenderText_Blended( mFont, inStr.c_str(), color );
	SDL_Texture* texture = SDL_CreateTextureFromSurface( GraphicsDriver::sInstance->GetRenderer(), surface );

	// Setup the rect for the texture
	SDL_Rect dstRect;
	dstRect.x = static_cast<int>( origin.mX );
	dstRect.y = static_cast<int>( origin.mY );
	SDL_QueryTexture( texture, nullptr, nullptr, &dstRect.w, &dstRect.h );

	// Draw the texture
	SDL_RenderCopy( GraphicsDriver::sInstance->GetRenderer(), texture, nullptr, &dstRect );

	// Destroy the surface/texture
	SDL_DestroyTexture( texture );
	SDL_FreeSurface( surface );
}
