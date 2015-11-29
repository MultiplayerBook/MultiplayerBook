#include <RoboCatClientPCH.h>

std::unique_ptr< HUD >	HUD::sInstance;


HUD::HUD() :
mScoreBoardOrigin( 50.f, 60.f, 0.0f ),
mBandwidthOrigin( 50.f, 10.f, 0.0f ),
mRoundTripTimeOrigin( 50.f, 10.f, 0.0f ),
mScoreOffset( 0.f, 50.f, 0.0f ),
mHealthOffset( 1000, 10.f, 0.0f ),
mHealth( 0 )
{
	TTF_Init();
	mFont = TTF_OpenFont( "../Assets/Carlito-Regular.TTF", 36 );
	if( mFont == nullptr )
	{
		SDL_LogError( SDL_LOG_CATEGORY_ERROR, "Failed to load font." );
	}
}


void HUD::StaticInit()
{
	sInstance.reset( new HUD() );
}

void HUD::Render()
{
	//RenderBandWidth();
	RenderRoundTripTime();
	RenderScoreBoard();
	RenderHealth();
}

void HUD::RenderHealth()
{
	if( mHealth > 0 )
	{
		string healthString = StringUtils::Sprintf( "Health %d", mHealth );
		RenderText( healthString, mHealthOffset, Colors::Red );
	}
}

void HUD::RenderBandWidth()
{
	string bandwidth = StringUtils::Sprintf( "In %d  Bps, Out %d Bps",
												static_cast< int >( NetworkManagerClient::sInstance->GetBytesReceivedPerSecond().GetValue() ),
												static_cast< int >( NetworkManagerClient::sInstance->GetBytesSentPerSecond().GetValue() ) );
	RenderText( bandwidth, mBandwidthOrigin, Colors::White );
}

void HUD::RenderRoundTripTime()
{
	float rttMS = NetworkManagerClient::sInstance->GetAvgRoundTripTime().GetValue() * 1000.f;

	string roundTripTime = StringUtils::Sprintf( "RTT %d ms", ( int ) rttMS  );
	RenderText( roundTripTime, mRoundTripTimeOrigin, Colors::White );
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
