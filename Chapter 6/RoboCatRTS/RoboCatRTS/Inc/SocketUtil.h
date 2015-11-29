enum SocketAddressFamily
{
	INET = AF_INET,
	INET6 = AF_INET6
};

class SocketUtil
{
public:

	static bool			StaticInit();
	static void			CleanUp();

	static void			ReportError( const char* inOperationDesc );
	static int			GetLastError();

	static int			Select( const vector< TCPSocketPtr >* inReadSet,
								vector< TCPSocketPtr >* outReadSet,
								const vector< TCPSocketPtr >* inWriteSet,
								vector< TCPSocketPtr >* outWriteSet,
								const vector< TCPSocketPtr >* inExceptSet,
								vector< TCPSocketPtr >* outExceptSet );

	static UDPSocketPtr	CreateUDPSocket( SocketAddressFamily inFamily );
	static TCPSocketPtr	CreateTCPSocket( SocketAddressFamily inFamily );

private:

	inline static fd_set* FillSetFromVector( fd_set& outSet, const vector< TCPSocketPtr >* inSockets, int& ioNaxNfds );
	inline static void FillVectorFromSet( vector< TCPSocketPtr >* outSockets, const vector< TCPSocketPtr >* inSockets, const fd_set& inSet );
};