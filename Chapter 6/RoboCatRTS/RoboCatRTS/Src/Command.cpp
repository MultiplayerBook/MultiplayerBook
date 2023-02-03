#include "RoboCatPCH.h"

shared_ptr< Command > Command::StaticReadAndCreate( InputMemoryBitStream& inInputStream )
{
	CommandPtr retVal;
	
	int type = CM_INVALID;
	inInputStream.Read( type );
	uint32_t networkId = 0;
	inInputStream.Read( networkId );
	uint32_t playerId = 0;
	inInputStream.Read( playerId );

	switch ( type )
	{
	case CM_ATTACK:
		retVal = std::make_shared< AttackCommand >();
		retVal->mNetworkId = networkId;
		retVal->mPlayerId = playerId;
		retVal->Read( inInputStream );
		break;
	case CM_MOVE:
		retVal = std::make_shared< MoveCommand >();
		retVal->mNetworkId = networkId;
		retVal->mPlayerId = playerId;
		retVal->Read( inInputStream );
		break;
	default:
		LOG( "Read in an unknown command type??" );
		break;
	}

	return retVal;
}

void Command::Write( OutputMemoryBitStream& inOutputStream )
{
	inOutputStream.Write( mCommandType );
	inOutputStream.Write( mNetworkId );
	inOutputStream.Write( mPlayerId );
}

AttackCommandPtr AttackCommand::StaticCreate( uint32_t inMyNetId, uint32_t inTargetNetId )
{
	AttackCommandPtr retVal;
	GameObjectPtr me = NetworkManager::sInstance->GetGameObject( inMyNetId );
	GameObjectPtr target = NetworkManager::sInstance->GetGameObject( inTargetNetId );
	uint32_t playerId = NetworkManager::sInstance->GetMyPlayerId();
	//can only issue commands to this unit if I own it, and it's a cat,
	//and if the target is a cat that's on a different team
	if ( me && me->GetClassId() == RoboCat::kClassId &&
		me->GetPlayerId() ==  playerId &&
		target && target->GetClassId() == RoboCat::kClassId &&
		target->GetPlayerId() != me->GetPlayerId() )
	{
		retVal = std::make_shared< AttackCommand >();
		retVal->mNetworkId = inMyNetId;
		retVal->mPlayerId = playerId;
		retVal->mTargetNetId = inTargetNetId;
	}
	return retVal;
}

void AttackCommand::Write( OutputMemoryBitStream& inOutputStream )
{
	Command::Write( inOutputStream );
	inOutputStream.Write( mTargetNetId );
}

void AttackCommand::Read( InputMemoryBitStream& inInputStream )
{
	inInputStream.Read( mTargetNetId );
}

void AttackCommand::ProcessCommand()
{
	GameObjectPtr obj = NetworkManager::sInstance->GetGameObject( mNetworkId );
	if ( obj && obj->GetClassId() == RoboCat::kClassId &&
		obj->GetPlayerId() == mPlayerId )
	{
		RoboCat* rc = obj->GetAsCat();
		rc->EnterAttackState( mTargetNetId );
	}
}

MoveCommandPtr MoveCommand::StaticCreate( uint32_t inNetworkId, const Vector3& inTarget )
{
	MoveCommandPtr retVal;
	GameObjectPtr go = NetworkManager::sInstance->GetGameObject( inNetworkId );
	uint32_t playerId = NetworkManager::sInstance->GetMyPlayerId();

	//can only issue commands to this unit if I own it, and it's a cat
	if ( go && go->GetClassId() == RoboCat::kClassId && 
		go->GetPlayerId() == playerId )
	{
		retVal = std::make_shared< MoveCommand >();
		retVal->mNetworkId = inNetworkId;
		retVal->mPlayerId = playerId;
		retVal->mTarget = inTarget;
	}
	return retVal;
}

void MoveCommand::Write( OutputMemoryBitStream& inOutputStream )
{
	Command::Write( inOutputStream );
	inOutputStream.Write( mTarget );
}

void MoveCommand::ProcessCommand()
{
	GameObjectPtr obj = NetworkManager::sInstance->GetGameObject( mNetworkId );
	if ( obj && obj->GetClassId() == RoboCat::kClassId &&
		obj->GetPlayerId() == mPlayerId )
	{
		RoboCat* rc = obj->GetAsCat();
		rc->EnterMovingState( mTarget );
	}
}

void MoveCommand::Read( InputMemoryBitStream& inInputStream )
{
	inInputStream.Read( mTarget );
}
