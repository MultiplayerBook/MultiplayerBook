#include "RoboCatPCH.h"

void CommandList::AddCommand( CommandPtr inCommand )
{
	mCommands.push_back( inCommand );
}

void CommandList::ProcessCommands( uint32_t inExpectedPlayerId )
{
	for( CommandPtr p : mCommands )
	{
		if ( p->GetPlayerId() == inExpectedPlayerId )
		{
			p->ProcessCommand();
		}
	}
}

void CommandList::Write( OutputMemoryBitStream& inOutputStream )
{
	inOutputStream.Write( GetCount() );
	for( CommandPtr p : mCommands )
	{
		p->Write( inOutputStream );
	}
}

void CommandList::Read( InputMemoryBitStream& inInputStream )
{
	int count;
	inInputStream.Read( count );
	for( int i = 0; i < count; ++i )
	{
		mCommands.push_back( Command::StaticReadAndCreate( inInputStream ) );
	}
}
