#include "RoboCatPCH.h"

void TurnData::Write( OutputMemoryBitStream& inOutputStream )
{
	inOutputStream.Write( mPlayerId );
	inOutputStream.Write( mRandomValue );
	inOutputStream.Write( mCRC );
	mCommandList.Write( inOutputStream );
}

void TurnData::Read( InputMemoryBitStream& inInputStream )
{
	inInputStream.Read( mPlayerId );
	inInputStream.Read( mRandomValue );
	inInputStream.Read( mCRC );
	mCommandList.Read( inInputStream );
}
