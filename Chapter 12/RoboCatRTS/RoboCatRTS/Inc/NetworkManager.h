class NetworkManager
{
public:
	//contains data for a particular turn
	static const uint32_t	kTurnCC = 'TURN';
	//notification used to ready up
	static const uint32_t	kReadyCC = 'REDY';
	//notifies peers that the game will be starting soon
	static const uint32_t	kStartCC = 'STRT';
	//used to ping a peer when in delay
	static const uint32_t	kDelayCC = 'DELY';
	static const int		kMaxPacketsPerFrameCount = 10;

	enum NetworkManagerState
	{
		NMS_Unitialized,
		NMS_Searching,
		NMS_Lobby,
		NMS_Ready,
		//everything above this should be the pre-game/lobby/connection
		NMS_Starting,
		NMS_Playing,
		NMS_Delay,
	};

	static unique_ptr< NetworkManager >	sInstance;
	static bool	StaticInit();

	NetworkManager();
	~NetworkManager();

	void	ProcessIncomingPackets();
	
	void	SendOutgoingPackets();
	void	UpdateDelay();
private:
	void	UpdateSayingHello( bool inForce = false );
	void	SendHelloPacket();
	void	UpdateStarting();
	void	UpdateSendTurnPacket();
	void	TryAdvanceTurn();
public:

	void	ProcessPacket( InputMemoryBitStream& inInputStream, uint64_t inFromPlayer );
private:
	void	ProcessPacketsLobby( InputMemoryBitStream& inInputStream, uint64_t inFromPlayer );
	void	ProcessPacketsReady( InputMemoryBitStream& inInputStream, uint64_t inFromPlayer );
	void	HandleReadyPacket( InputMemoryBitStream& inInputStream, uint64_t inFromPlayer );
	void	SendReadyPacketsToPeers();
	void	HandleStartPacket( InputMemoryBitStream& inInputStream, uint64_t inFromPlayer );
	void	ProcessPacketsPlaying( InputMemoryBitStream& inInputStream, uint64_t inFromPlayer );
	void	HandleTurnPacket( InputMemoryBitStream& inInputStream, uint64_t inFromPlayer );
	void	ProcessPacketsDelay( InputMemoryBitStream& inInputStream, uint64_t inFromPlayer );
	void	TryStartGame();
public:
	void	HandleConnectionReset( uint64_t inFromPlayer );

	void	SendPacket( const OutputMemoryBitStream& inOutputStream, uint64_t inToPlayer );

	void	EnterLobby( uint64_t inLobbyId );
	void	UpdateLobbyPlayers();
	void	TryReadyGame();

			const WeightedTimedMovingAverage& GetBytesReceivedPerSecond()	const	{ return mBytesReceivedPerSecond; }
			const WeightedTimedMovingAverage& GetBytesSentPerSecond()		const	{ return mBytesSentPerSecond; }
	void	SetDropPacketChance( float inChance )	{ mDropPacketChance = inChance; }
	float	GetDropPacketChance() const				{ return mDropPacketChance; }
	void	SetSimulatedLatency( float inLatency )	{ mSimulatedLatency = inLatency; }
	float	GetSimulatedLatency() const				{ return mSimulatedLatency; }

	bool	IsMasterPeer() const { return mIsMasterPeer; }
	float	GetTimeToStart() const { return mTimeToStart; }
	
	GameObjectPtr	GetGameObject( uint32_t inNetworkId ) const;
	GameObjectPtr	RegisterAndReturn( GameObject* inGameObject );
	void			UnregisterGameObject( GameObject* inGameObject );

	NetworkManagerState GetState() const { return mState; }
	int		GetReadyCount() const { return mReadyCount; }
	int		GetPlayerCount() const { return mPlayerCount; }
	int		GetTurnNumber() const { return mTurnNumber; }
	int		GetSubTurnNumber() const { return mSubTurnNumber; }
	uint64_t GetMyPlayerId() const { return mPlayerId; }
	bool	IsPlayerInGame( uint64_t inPlayerId );
private:
	void	AddToNetworkIdToGameObjectMap( GameObjectPtr inGameObject );
	void	RemoveFromNetworkIdToGameObjectMap( GameObjectPtr inGameObject );
	void	RegisterGameObject( GameObjectPtr inGameObject );
	uint32_t GetNewNetworkId();
	uint32_t ComputeGlobalCRC();

	bool	Init();
	
	class ReceivedPacket
	{
	public:
		ReceivedPacket( float inReceivedTime, InputMemoryBitStream& inInputMemoryBitStream, uint64_t inFromPlayer );

		uint64_t				GetFromPlayer()		const	{ return mFromPlayer; }
		float					GetReceivedTime()	const	{ return mReceivedTime; }
		InputMemoryBitStream&	GetPacketBuffer()			{ return mPacketBuffer; }

	private:
			
		float					mReceivedTime;
		InputMemoryBitStream	mPacketBuffer;
		uint64_t				mFromPlayer;

	};

	void	UpdateBytesSentLastFrame();
	void	ReadIncomingPacketsIntoQueue();
	void	ProcessQueuedPackets();

	void	EnterPlayingState();
	void	SpawnCat( uint64_t inPlayerId, const Vector3& inSpawnVec );

	void	CheckForAchievements();

	//these should stay ordered!
	typedef map< uint64_t, string > Int64ToStrMap;
	typedef map< uint64_t, TurnData > Int64ToTurnDataMap;
	typedef map< uint32_t, GameObjectPtr > IntToGameObjectMap;

	bool	CheckSync( Int64ToTurnDataMap& inTurnMap );

	queue< ReceivedPacket, list< ReceivedPacket > >	mPacketQueue;

	IntToGameObjectMap			mNetworkIdToGameObjectMap;
	Int64ToStrMap				mPlayerNameMap;

	//this stores all of our turn information for every turn since game start
	vector< Int64ToTurnDataMap >	mTurnData;

	WeightedTimedMovingAverage	mBytesReceivedPerSecond;
	WeightedTimedMovingAverage	mBytesSentPerSecond;
	NetworkManagerState			mState;

	int							mBytesSentThisFrame;
	std::string		mName;

	float			mDropPacketChance;
	float			mSimulatedLatency;

	float			mDelayHeartbeat;
	float			mTimeToStart;

	int				mPlayerCount;
	int				mReadyCount;
	
	uint32_t		mNewNetworkId;
	uint64_t		mPlayerId;
	uint64_t		mLobbyId;
	uint64_t		mMasterPeerId;

	int				mTurnNumber;
	int				mSubTurnNumber;
	bool			mIsMasterPeer;
};
