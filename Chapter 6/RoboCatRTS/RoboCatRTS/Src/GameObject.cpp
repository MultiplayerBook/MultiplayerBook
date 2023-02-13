#include "RoboCatPCH.h"

GameObject::GameObject() :
	mIndexInWorld( -1 ),
	mCollisionRadius( 0.5f ),
	mDoesWantToDie( false ),
	mRotation( 0.f ),
	mNetworkId( 0 ),
	mPlayerId( 0 ),
	mColor( Colors::White ),
	mScale( 1.0f )
{
}

GameObject::~GameObject()
{
}

void GameObject::Update( float inDeltaTime )
{
	//object don't do anything by default...	
}


void GameObject::HandleDying()
{
	NetworkManager::sInstance->UnregisterGameObject( this );
}

Vector3 GameObject::GetForwardVector()	const
{
	//should we cache this when you turn?
	return Vector3( sinf( mRotation ), -cosf( mRotation ), 0.f );
}

void GameObject::SetNetworkId( uint32_t inNetworkId )
{ 
	//this doesn't put you in the map or remove you from it
	mNetworkId = inNetworkId; 

}

bool GameObject::TrySelect( const Vector3& inLocation )
{
	Vector3 diff = mLocation - inLocation;
	float dist = diff.Length2D();
	if ( dist <= mCollisionRadius )
	{
		return true;
	}

	return false;
}

void GameObject::SetRotation( float inRotation )
{ 
	//should we normalize using fmodf?
	mRotation = inRotation;
}
