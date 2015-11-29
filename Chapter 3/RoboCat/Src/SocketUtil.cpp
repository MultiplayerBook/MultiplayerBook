#include "RoboCatPCH.h"



bool SocketUtil::StaticInit()
{
#if _WIN32
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if ( iResult != NO_ERROR )
	{
		ReportError ("Starting Up");
		return false;
	}
#endif
	return true;
}

void SocketUtil::CleanUp()
{
#if _WIN32
	WSACleanup();
#endif
}


void SocketUtil::ReportError( const char* inOperationDesc )
{
#if _WIN32
	LPVOID lpMsgBuf;
	DWORD errorNum = GetLastError();
	
	FormatMessage(
				  FORMAT_MESSAGE_ALLOCATE_BUFFER |
				  FORMAT_MESSAGE_FROM_SYSTEM |
				  FORMAT_MESSAGE_IGNORE_INSERTS,
				  NULL,
				  errorNum,
				  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				  (LPTSTR) &lpMsgBuf,
				  0, NULL );
	
	
	LOG( "Error %s: %d- %s", inOperationDesc, errorNum, lpMsgBuf );
#else
	LOG( "Error: %hs", inOperationDesc );
#endif
}

int SocketUtil::GetLastError()
{
#if _WIN32
	return WSAGetLastError();
#else
	return errno;
#endif
	
}

UDPSocketPtr SocketUtil::CreateUDPSocket( SocketAddressFamily inFamily )
{
	SOCKET s = socket( inFamily, SOCK_DGRAM, IPPROTO_UDP );
	
	if( s != INVALID_SOCKET )
	{
		return UDPSocketPtr( new UDPSocket( s ) );
	}
	else
	{
		ReportError( "SocketUtil::CreateUDPSocket" );
		return nullptr;
	}
}

TCPSocketPtr SocketUtil::CreateTCPSocket( SocketAddressFamily inFamily )
{
	SOCKET s = socket( inFamily, SOCK_STREAM, IPPROTO_TCP );
	
	if( s != INVALID_SOCKET )
	{
		return TCPSocketPtr( new TCPSocket( s ) );
	}
	else
	{
		ReportError( "SocketUtil::CreateTCPSocket" );
		return nullptr;
	}
}

fd_set* SocketUtil::FillSetFromVector( fd_set& outSet, const vector< TCPSocketPtr >* inSockets, int& ioNaxNfds )
{
	if( inSockets )
	{
		FD_ZERO( &outSet );
		
		for( const TCPSocketPtr& socket : *inSockets )
		{
			FD_SET( socket->mSocket, &outSet );
#if !_WIN32
			ioNaxNfds = std::max( ioNaxNfds, socket->mSocket );
#endif
		}
		return &outSet;
	}
	else
	{
		return nullptr;
	}
}

void SocketUtil::FillVectorFromSet( vector< TCPSocketPtr >* outSockets, const vector< TCPSocketPtr >* inSockets, const fd_set& inSet )
{
	if( inSockets && outSockets )
	{
		outSockets->clear();
		for( const TCPSocketPtr& socket : *inSockets )
		{
			if( FD_ISSET( socket->mSocket, &inSet ) )
			{
				outSockets->push_back( socket );
			}
		}
	}
}

int SocketUtil::Select( const vector< TCPSocketPtr >* inReadSet,
					   vector< TCPSocketPtr >* outReadSet,
					   const vector< TCPSocketPtr >* inWriteSet,
					   vector< TCPSocketPtr >* outWriteSet,
					   const vector< TCPSocketPtr >* inExceptSet,
					   vector< TCPSocketPtr >* outExceptSet )
{
	//build up some sets from our vectors
	fd_set read, write, except;
	
	int nfds = 0;
	
	fd_set *readPtr = FillSetFromVector( read, inReadSet, nfds );
	fd_set *writePtr = FillSetFromVector( read, inWriteSet, nfds );
	fd_set *exceptPtr = FillSetFromVector( read, inExceptSet, nfds );
	
	int toRet = select( nfds + 1, readPtr, writePtr, exceptPtr, nullptr );
	
	if( toRet > 0 )
	{
		FillVectorFromSet( outReadSet, inReadSet, read );
		FillVectorFromSet( outWriteSet, inWriteSet, write );
		FillVectorFromSet( outExceptSet, inExceptSet, except );
	}
	return toRet;
}
