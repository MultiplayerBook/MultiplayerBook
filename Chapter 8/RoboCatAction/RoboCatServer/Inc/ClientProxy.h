class ClientProxy
{
public:

	ClientProxy( const SocketAddress& inSocketAddress, const string& inName, int inPlayerId );

	const	SocketAddress&	GetSocketAddress()	const	{ return mSocketAddress; }
			int				GetPlayerId()		const	{ return mPlayerId; }
	const	string&		GetName()			const	{ return mName; }

			void			SetInputState( const InputState& inInputState )		{ mInputState = inInputState; }
	const	InputState&		GetInputState()		const	{ return mInputState; }

			void			UpdateLastPacketTime();
			float			GetLastPacketFromClientTime()	const	{ return mLastPacketFromClientTime; }

			DeliveryNotificationManager&	GetDeliveryNotificationManager()	{ return mDeliveryNotificationManager; }
			ReplicationManagerServer&		GetReplicationManagerServer()		{ return mReplicationManagerServer; }

			const	MoveList&				GetUnprocessedMoveList() const		{ return mUnprocessedMoveList; }
					MoveList&				GetUnprocessedMoveList()			{ return mUnprocessedMoveList; }

			void	SetIsLastMoveTimestampDirty( bool inIsDirty )				{ mIsLastMoveTimestampDirty = inIsDirty; }
			bool	IsLastMoveTimestampDirty()						const		{ return mIsLastMoveTimestampDirty; }

			void	HandleCatDied();
			void	RespawnCatIfNecessary();

private:

	DeliveryNotificationManager	mDeliveryNotificationManager;
	ReplicationManagerServer	mReplicationManagerServer;

	SocketAddress	mSocketAddress;
	string			mName;
	int				mPlayerId;

	//going away!
	InputState		mInputState;

	float			mLastPacketFromClientTime;
	float			mTimeToRespawn;

	MoveList		mUnprocessedMoveList;
	bool			mIsLastMoveTimestampDirty;



};

typedef shared_ptr< ClientProxy >	ClientProxyPtr;