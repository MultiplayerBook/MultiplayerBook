#include <RoboCatClientPCH.h>

YarnClient::YarnClient()
{
	mSpriteComponent.reset( new SpriteComponent( this ) );
	mSpriteComponent->SetTexture( TextureManager::sInstance->GetTexture( "yarn" ) );
}


void YarnClient::Read( InputMemoryBitStream& inInputStream )
{
	bool stateBit;

	inInputStream.Read( stateBit );
	if( stateBit )
	{
		Vector3 location;
		inInputStream.Read( location.mX );
		inInputStream.Read( location.mY );


		Vector3 velocity;
		inInputStream.Read( velocity.mX );
		inInputStream.Read( velocity.mY );
		SetVelocity( velocity );

		//dead reckon ahead by rtt, since this was spawned a while ago!
		SetLocation( location + velocity * NetworkManagerClient::sInstance->GetRoundTripTime() );


		float rotation;
		inInputStream.Read( rotation );
		SetRotation( rotation );
	}


	inInputStream.Read( stateBit );
	if( stateBit )
	{	
		Vector3 color;
		inInputStream.Read( color );
		SetColor( color );
	}

	inInputStream.Read( stateBit );
	if( stateBit )
	{	
		inInputStream.Read( mPlayerId, 8 );
	}

}

//you look like you hit a cat on the client, so disappear ( whether server registered or not
bool YarnClient::HandleCollisionWithCat( RoboCat* inCat )
{
	if( GetPlayerId() != inCat->GetPlayerId() )
	{
		RenderManager::sInstance->RemoveComponent( mSpriteComponent.get() );
	}
	return false;
}
