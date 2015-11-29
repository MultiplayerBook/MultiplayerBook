#include<RoboCatPCH.h>

GameObject::GameObject() :
	mIndexInWorld( -1 ),
	mCollisionRadius( 0.5f ),
	mDoesWantToDie( false ),
	mRotation( 0.f ),
	mNetworkId( 0 ),
	mColor( Colors::White ),
	mScale( 1.0f )
{
}

void GameObject::Update()
{
	//object don't do anything by default...	
}


Vector3 GameObject::GetForwardVector()	const
{
	//should we cache this when you turn?
	return Vector3( sinf( mRotation ), -cosf( mRotation ), 0.f );
}

void GameObject::SetNetworkId( int inNetworkId )
{ 
	//this doesn't put you in the map or remove you from it
	mNetworkId = inNetworkId; 

}

void GameObject::SetRotation( float inRotation )
{ 
	//should we normalize using fmodf?
	mRotation = inRotation;
}
