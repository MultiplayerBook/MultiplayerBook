#include <RoboCatPCH.h>

bool Move::Write( OutputMemoryBitStream& inOutputStream ) const
{
	mInputState.Write( inOutputStream );
	inOutputStream.Write( mTimestamp );

	return true;
}

bool Move::Read( InputMemoryBitStream& inInputStream )
{
	mInputState.Read( inInputStream );
	inInputStream.Read( mTimestamp );

	return true;
}
