#include "RoboCatPCH.h"

const float kMaxLifeSpan = 0.25f;
const float kMuzzleSpeed = 4.0f;

Yarn::Yarn() :
	mVelocity( Vector3::Zero )
{
	SetScale( GetScale() * 0.25f );
	SetCollisionRadius( 0.125f );
	mSpriteComponent = std::make_shared<SpriteComponent>( this );
	mSpriteComponent->SetTexture( TextureManager::sInstance->GetTexture( "yarn" ) );
}

void Yarn::InitFromShooter( GameObjectPtr inShooter, GameObjectPtr inTarget )
{
	SetPlayerId( inShooter->GetPlayerId() );

	Vector3 forward = inTarget->GetLocation() - inShooter->GetLocation();
	SetVelocity( forward * kMuzzleSpeed );
	SetLocation( inShooter->GetLocation() );

	mTargetCat = inTarget;
}

void Yarn::Update( float inDeltaTime )
{
	SetLocation( GetLocation() + mVelocity * inDeltaTime );
	
	mLifeSpan += inDeltaTime;
	if ( mLifeSpan > kMaxLifeSpan )
	{
		//on death, do damage to the target cat
		SetDoesWantToDie( true );
		if ( mTargetCat && !mTargetCat->DoesWantToDie() )
		{
			mTargetCat->GetAsCat()->TakeDamage( 1 );
		}
	}
}
