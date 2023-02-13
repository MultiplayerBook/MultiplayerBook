#include "RoboCatPCH.h"

//TODO: check these, I have no idea if this is the best way to do it...
//(definitely doesn't work from cross-platform standpoint)
void SocketAddress::Read( InputMemoryBitStream& inInputStream )
{
	//read the address and port
	inInputStream.Read( GetIP4Ref() );
	inInputStream.Read( GetAsSockAddrIn()->sin_port );
}

void SocketAddress::Write( OutputMemoryBitStream& inOutputStream )
{
	//write the address and port
	inOutputStream.Write( GetIP4Ref() );
	inOutputStream.Write( GetAsSockAddrIn()->sin_port );
}

string	SocketAddress::ToString() const
{
#if _WIN32
	const sockaddr_in* s = GetAsSockAddrIn();
	char destinationBuffer[ 128 ];
	InetNtop( s->sin_family, const_cast< in_addr* >( &s->sin_addr ), destinationBuffer, sizeof( destinationBuffer ) );
	return StringUtils::Sprintf( "%s:%d",
								destinationBuffer,
								ntohs( s->sin_port ) );
#else
	//not implement on mac for now...
	return string( "not implemented on mac for now" );
#endif
}

