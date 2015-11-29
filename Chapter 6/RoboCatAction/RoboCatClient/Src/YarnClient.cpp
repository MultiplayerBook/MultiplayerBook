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
		SetLocation( location );

		Vector3 velocity;
		inInputStream.Read( velocity.mX );
		inInputStream.Read( velocity.mY );
		SetVelocity( velocity );

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
