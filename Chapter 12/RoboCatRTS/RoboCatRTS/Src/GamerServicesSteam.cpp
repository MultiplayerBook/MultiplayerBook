#include "RoboCatPCH.h"
#include "steam_api.h"

std::unique_ptr< GamerServices > GamerServices::sInstance = nullptr;
const char* kGameName = "robocatrts";

struct StatData
{
	const char* Name;
	GamerServices::StatType Type;

	int IntStat = 0;
	float FloatStat = 0.0f;
	struct
	{
		float SessionValue = 0.0f;
		float SessionLength = 0.0f;
	} AvgRateStat;

	StatData( const char* inName, GamerServices::StatType inType ) :
		Name(inName),
		Type(inType)
	{ }
};

struct AchieveData
{
	const char* Name;
	bool Unlocked = false;

	AchieveData( const char* inName ):
		Name( inName )
	{ }
};

struct LeaderboardData
{
	const char* Name;
	ELeaderboardSortMethod SortMethod;
	ELeaderboardDisplayType DisplayType;
	SteamLeaderboard_t Handle = 0;

	LeaderboardData( const char* inName, ELeaderboardSortMethod inMethod, 
		ELeaderboardDisplayType inDisplay ) :
		Name( inName ),
		SortMethod( inMethod ),
		DisplayType( inDisplay )
	{ }
};

struct GamerServices::Impl
{
	Impl();

	//Call result when we get a list of lobbies
	CCallResult<Impl, LobbyMatchList_t> mLobbyMatchListResult;
	void OnLobbyMatchListCallback( LobbyMatchList_t* inCallback, bool inIOFailure );
	//Call result when a lobby is created
	CCallResult<Impl, LobbyCreated_t> mLobbyCreateResult;
	void OnLobbyCreateCallback( LobbyCreated_t* inCallback, bool inIOFailure );
	//Call result when we enter a lobby
	CCallResult<Impl, LobbyEnter_t> mLobbyEnteredResult;
	void OnLobbyEnteredCallback( LobbyEnter_t* inCallback, bool inIOFailure );
	//Call result when a leaderboard is found (or created)
	CCallResult<Impl, LeaderboardFindResult_t> mLeaderFindResult;
	void OnLeaderFindCallback( LeaderboardFindResult_t* inCallback, bool inIOFailure );
	//Call results when a leaderboard score is uploaded
	CCallResult<Impl, LeaderboardScoreUploaded_t> mLeaderUploadResult;
	void OnLeaderUploadCallback( LeaderboardScoreUploaded_t* inCallback, bool inIOFailure );
	//Call results when a leaderboard is downloaded
	CCallResult<Impl, LeaderboardScoresDownloaded_t> mLeaderDownloadResult;
	void OnLeaderDownloadCallback( LeaderboardScoresDownloaded_t* inCallback, bool inIOFailure );

	//Callback when a user leaves/enters lobby
	STEAM_CALLBACK( Impl, OnLobbyChatUpdate, LobbyChatUpdate_t, mChatDataUpdateCallback );
	//Callback when a P2P connection is attempted
	STEAM_CALLBACK( Impl, OnP2PSessionRequest, P2PSessionRequest_t, mSessionRequestCallback );
	//Callback if the P2P connection fails
	STEAM_CALLBACK( Impl, OnP2PSessionFail, P2PSessionConnectFail_t, mSessionFailCallback );
	//Callback when stats are received from server
	STEAM_CALLBACK( Impl, OnStatsReceived, UserStatsReceived_t, mStatsReceived );

	std::array< StatData, MAX_STAT > mStatArray;
	std::array< AchieveData, MAX_ACHIEVEMENT > mAchieveArray;
	std::array< LeaderboardData, MAX_LEADERBOARD > mLeaderArray;
	uint64_t mGameId;
	CSteamID mLobbyId;
	int mCurrentLeaderFind;

	bool mAreStatsReady;
	bool mAreLeadersReady;
};

GamerServices::Impl::Impl() :
	//use an x-macro to construct every element of the stat/achievement arrays
	mStatArray( {
		#define STAT(a,b) StatData(#a, StatType::b),
		#include "../Inc/Stats.def"
		#undef STAT	
	} ),
	mAchieveArray( {
		#define ACH(a) AchieveData(#a),
		#include "../Inc/Achieve.def"
		#undef ACH
	} ),
	mLeaderArray( {
		#define BOARD(a,b,c) LeaderboardData(#a, k_ELeaderboardSortMethod##b, k_ELeaderboardDisplayType##c),
		#include "../Inc/Leaderboards.def"
		#undef BOARD
	} ),

	mChatDataUpdateCallback( this, &Impl::OnLobbyChatUpdate ),
	mSessionRequestCallback( this, &Impl::OnP2PSessionRequest ),
	mSessionFailCallback( this, &Impl::OnP2PSessionFail ),
	mStatsReceived( this, &Impl::OnStatsReceived ),
	mGameId( SteamUtils()->GetAppID() ),
	mCurrentLeaderFind( -1 ),
	mAreStatsReady( false ),
	mAreLeadersReady( false )
{

}

void GamerServices::Impl::OnLobbyMatchListCallback( LobbyMatchList_t* inCallback, bool inIOFailure )
{
	if( inIOFailure )
	{
		return;
	}

	//if we have available lobbies, enter the first one
	if( inCallback->m_nLobbiesMatching > 0 )
	{
		mLobbyId = SteamMatchmaking()->GetLobbyByIndex( 0 );
		SteamAPICall_t call = SteamMatchmaking()->JoinLobby( mLobbyId );
		mLobbyEnteredResult.Set( call, this, &Impl::OnLobbyEnteredCallback );
	}
	else
	{
		//need to make our own lobby
		SteamAPICall_t call = SteamMatchmaking()->CreateLobby( k_ELobbyTypePublic, 4 );
		mLobbyCreateResult.Set( call, this, &Impl::OnLobbyCreateCallback );
	}
}

void GamerServices::Impl::OnLobbyCreateCallback( LobbyCreated_t* inCallback, bool inIOFailure )
{
	if( inCallback->m_eResult == k_EResultOK && !inIOFailure )
	{
		mLobbyId = inCallback->m_ulSteamIDLobby;
		//set our game and joinable so others can find this lobby
		SteamMatchmaking()->SetLobbyData( mLobbyId, "game", kGameName );

		NetworkManager::sInstance->EnterLobby( mLobbyId.ConvertToUint64() );
	}
}

void GamerServices::Impl::OnLobbyEnteredCallback( LobbyEnter_t* inCallback, bool inIOFailure )
{
	if( inIOFailure )
	{
		return;
	}

	if( inCallback->m_EChatRoomEnterResponse == k_EChatRoomEnterResponseSuccess )
	{
		mLobbyId = inCallback->m_ulSteamIDLobby;
		NetworkManager::sInstance->EnterLobby( mLobbyId.ConvertToUint64() );
	}
	else
	{
		//failed to join, just quit
		LOG( "Failed to join lobby" );
		Engine::sInstance->SetShouldKeepRunning( false );
	}
}

void GamerServices::Impl::OnLeaderFindCallback( LeaderboardFindResult_t* inCallback, bool inIOFailure )
{
	if( !inIOFailure && inCallback->m_bLeaderboardFound )
	{
		mLeaderArray[ mCurrentLeaderFind ].Handle = inCallback->m_hSteamLeaderboard;

		//load the next one
		mCurrentLeaderFind++;
		if( mCurrentLeaderFind != MAX_LEADERBOARD )
		{
			GamerServices::sInstance->FindLeaderboardAsync( static_cast< Leaderboard >( mCurrentLeaderFind ) );
		}
		else
		{
			mAreLeadersReady = true;
		}
	}
}

void GamerServices::Impl::OnLeaderUploadCallback( LeaderboardScoreUploaded_t* inCallback, 
	bool inIOFailure )
{
	if( !inIOFailure && inCallback->m_bSuccess && inCallback->m_bScoreChanged )
	{
		//we could do something here if we for example wanted to show
		//a new "high score" or some such
	}
}

void GamerServices::Impl::OnLeaderDownloadCallback( 
	LeaderboardScoresDownloaded_t* inCallback, bool inIOFailure )
{
	if( !inIOFailure )
	{
		LeaderboardEntry_t entry;

		//this is where we would hook into displaying the leaderboard
		//for now, we'll just output it to the console
		for( int i = 0; i < inCallback->m_cEntryCount; ++i )
		{
			SteamUserStats()->GetDownloadedLeaderboardEntry( inCallback->m_hSteamLeaderboardEntries, 
				i, &entry, nullptr, 0 );

			LOG( "%d. %s = %d", entry.m_nGlobalRank,
				SteamFriends()->GetFriendPersonaName( entry.m_steamIDUser ),
				entry.m_nScore );
		}
	}
}

void GamerServices::Impl::OnLobbyChatUpdate( LobbyChatUpdate_t* inCallback )
{
	//this gets called every time a user enters or exits the lobby
	if( mLobbyId == inCallback->m_ulSteamIDLobby )
	{
		NetworkManager::sInstance->UpdateLobbyPlayers();
	}
}

void GamerServices::Impl::OnP2PSessionRequest( P2PSessionRequest_t* inCallback )
{
	CSteamID playerId = inCallback->m_steamIDRemote;
	if( NetworkManager::sInstance->IsPlayerInGame( playerId.ConvertToUint64() ) )
	{
		SteamNetworking()->AcceptP2PSessionWithUser( playerId );
	}
}

void GamerServices::Impl::OnP2PSessionFail( P2PSessionConnectFail_t* inCallback )
{
	//we've lost this player, so let the network manager know
	NetworkManager::sInstance->HandleConnectionReset( inCallback->m_steamIDRemote.ConvertToUint64() );
}

void GamerServices::Impl::OnStatsReceived( UserStatsReceived_t* inCallback )
{
	LOG( "Stats loaded from server..." );
	mAreStatsReady = true;

	if( inCallback->m_nGameID == mGameId &&
		inCallback->m_eResult == k_EResultOK )
	{
		//load stats
		for( int i = 0; i < MAX_STAT; ++i )
		{
			StatData& stat = mStatArray[ i ];
			if( stat.Type == StatType::INT )
			{
				SteamUserStats()->GetStat( stat.Name, &stat.IntStat );
				LOG( "Stat %s = %d", stat.Name, stat.IntStat );
			}
			else
			{
				//when we get average rate, we still only get one float
				SteamUserStats()->GetStat( stat.Name, &stat.FloatStat );
				LOG( "Stat %s = %f", stat.Name, stat.FloatStat );
			}
		}

		//load achievements
		for( int i = 0; i < MAX_ACHIEVEMENT; ++i )
		{
			AchieveData& ach = mAchieveArray[ i ];
			SteamUserStats()->GetAchievement( ach.Name, &ach.Unlocked );
			LOG( "Achievement %s = %d", ach.Name, ach.Unlocked );
		}
	}
}

uint64_t GamerServices::GetLocalPlayerId()
{
	CSteamID myID = SteamUser()->GetSteamID();
	return myID.ConvertToUint64();
}

string GamerServices::GetLocalPlayerName()
{
	return string( SteamFriends()->GetPersonaName() );
}

string GamerServices::GetRemotePlayerName( uint64_t inPlayerId )
{
	return string( SteamFriends()->GetFriendPersonaName( inPlayerId ) );
}

void GamerServices::LobbySearchAsync()
{
	//make sure it's Robo Cat RTS!
	SteamMatchmaking()->AddRequestLobbyListStringFilter( "game", kGameName, k_ELobbyComparisonEqual );
	//only need one result
	SteamMatchmaking()->AddRequestLobbyListResultCountFilter( 1 );
	
	SteamAPICall_t call = SteamMatchmaking()->RequestLobbyList();
	mImpl->mLobbyMatchListResult.Set( call, mImpl.get(), &Impl::OnLobbyMatchListCallback );
}

int GamerServices::GetLobbyNumPlayers( uint64_t inLobbyId )
{
	return SteamMatchmaking()->GetNumLobbyMembers( inLobbyId );
}

uint64_t GamerServices::GetMasterPeerId( uint64_t inLobbyId )
{
	CSteamID masterId = SteamMatchmaking()->GetLobbyOwner( inLobbyId );
	return masterId.ConvertToUint64();
}

void GamerServices::GetLobbyPlayerMap( uint64_t inLobbyId, map< uint64_t, string >& outPlayerMap )
{
	CSteamID myId = GetLocalPlayerId();
	outPlayerMap.clear();
	int count = GetLobbyNumPlayers( inLobbyId );
	for( int i = 0; i < count; ++i )
	{
		CSteamID playerId = SteamMatchmaking()->GetLobbyMemberByIndex( inLobbyId, i );
		if( playerId == myId )
		{
			outPlayerMap.emplace( playerId.ConvertToUint64(), GetLocalPlayerName() );
		}
		else
		{
			outPlayerMap.emplace( playerId.ConvertToUint64(), GetRemotePlayerName( playerId.ConvertToUint64() ) );
		}
	}
}

void GamerServices::SetLobbyReady( uint64_t inLobbyId )
{
	SteamMatchmaking()->SetLobbyJoinable( inLobbyId, false);
}

void GamerServices::LeaveLobby( uint64_t inLobbyId )
{
	SteamMatchmaking()->LeaveLobby( inLobbyId );
}

bool GamerServices::SendP2PReliable( const OutputMemoryBitStream& inOutputStream, uint64_t inToPlayer )
{
	return SteamNetworking()->SendP2PPacket( inToPlayer, inOutputStream.GetBufferPtr(),
												inOutputStream.GetByteLength(), k_EP2PSendReliable );
}

bool GamerServices::IsP2PPacketAvailable( uint32_t& outPacketSize )
{
	return SteamNetworking()->IsP2PPacketAvailable( &outPacketSize );
}

uint32_t GamerServices::ReadP2PPacket( void* inToReceive, uint32_t inMaxLength, uint64_t& outFromPlayer )
{
	uint32_t packetSize;
	CSteamID fromId;
	SteamNetworking()->ReadP2PPacket( inToReceive, inMaxLength, &packetSize, &fromId );
	outFromPlayer = fromId.ConvertToUint64();
	return packetSize;
}

void GamerServices::RetrieveStatsAsync()
{
	SteamUserStats()->RequestCurrentStats();
}

int GamerServices::GetStatInt( Stat inStat )
{
	if( !mImpl->mAreStatsReady )
	{
		LOG( "Stats ERROR: Stats not ready yet" );
		return -1;
	}

	StatData& stat = mImpl->mStatArray[ inStat ];

	if( stat.Type != StatType::INT )
	{
		LOG( "Stats ERROR: %s is not an integer stat", stat.Name );
		return -1;
	}

	return stat.IntStat;
}

float GamerServices::GetStatFloat( Stat inStat )
{
	if( !mImpl->mAreStatsReady )
	{
		LOG( "Stats ERROR: Stats not ready yet" );
		return -1.f;
	}

	StatData& stat = mImpl->mStatArray[ inStat ];

	if( stat.Type != StatType::FLOAT )
	{
		LOG( "Stats ERROR: %s is not a float stat", stat.Name );
		return -1.f;
	}

	return stat.FloatStat;
}

float GamerServices::GetStatAvgRate( Stat inStat )
{
	if( !mImpl->mAreStatsReady )
	{
		LOG( "Stats ERROR: Stats not ready yet" );
		return -1.f;
	}

	StatData& stat = mImpl->mStatArray[ inStat ];

	if( stat.Type != StatType::AVGRATE )
	{
		LOG( "Stats ERROR: %s is not an average rate stat", stat.Name );
		return -1.f;
	}

	return stat.FloatStat;
}

void GamerServices::AddToStat( Stat inStat, int inInc )
{
	if( !mImpl->mAreStatsReady )
	{
		LOG( "Stats ERROR: Stats not ready yet" );
		return;
	}

	StatData& stat = mImpl->mStatArray[ inStat ];

	if( stat.Type != StatType::INT )
	{
		LOG( "Stats ERROR: %s is not an integer stat", stat.Name );
		return;
	}

	stat.IntStat += inInc;
	SteamUserStats()->SetStat( stat.Name, stat.IntStat );
}

void GamerServices::AddToStat( Stat inStat, float inInc )
{
	if( !mImpl->mAreStatsReady )
	{
		LOG( "Stats ERROR: Stats not ready yet" );
		return;
	}

	StatData& stat = mImpl->mStatArray[ inStat ];

	if( stat.Type != StatType::FLOAT )
	{
		LOG( "Stats ERROR: %s is not a float stat", stat.Name );
		return;
	}

	stat.FloatStat += inInc;
	SteamUserStats()->SetStat( stat.Name, stat.FloatStat );
}

void GamerServices::UpdateAvgRateLocal( Stat inStat, float inIncValue, float inIncSeconds )
{
	if( !mImpl->mAreStatsReady )
	{
		LOG( "Stats ERROR: Stats not ready yet" );
		return;
	}

	StatData& stat = mImpl->mStatArray[ inStat ];

	if( stat.Type != StatType::AVGRATE )
	{
		LOG( "Stats ERROR: %s is not an average rate stat", stat.Name );
		return;
	}

	stat.AvgRateStat.SessionValue += inIncValue;
	stat.AvgRateStat.SessionLength += inIncSeconds;
}

void GamerServices::WriteAvgRate( Stat inStat )
{
	if( !mImpl->mAreStatsReady )
	{
		LOG( "Stats ERROR: Stats not ready yet" );
		return;
	}

	StatData& stat = mImpl->mStatArray[ inStat ];

	if( stat.Type != StatType::AVGRATE )
	{
		LOG( "Stats ERROR: %s is not an average rate stat", stat.Name );
		return;
	}

	SteamUserStats()->UpdateAvgRateStat( stat.Name,
		stat.AvgRateStat.SessionValue,
		stat.AvgRateStat.SessionLength );

	//reset the local range
	stat.AvgRateStat.SessionValue = 0.0f;
	stat.AvgRateStat.SessionLength = 0.0f;

	//grab the new float representing the average
	SteamUserStats()->GetStat( stat.Name, &stat.FloatStat );
}

bool GamerServices::IsAchievementUnlocked( Achievement inAch )
{
	if( !mImpl->mAreStatsReady )
	{
		LOG( "Achievements ERROR: Stats not ready yet" );
		return false;
	}

	return mImpl->mAchieveArray[ inAch ].Unlocked;
}

void GamerServices::UnlockAchievement( Achievement inAch )
{
	if( !mImpl->mAreStatsReady )
	{
		LOG( "Achievements ERROR: Stats not ready yet" );
		return;
	}

	AchieveData& ach = mImpl->mAchieveArray[ inAch ];

	//ignore if already unlocked
	if( ach.Unlocked )
	{
		return;
	}

	SteamUserStats()->SetAchievement( ach.Name );
	ach.Unlocked = true;

	LOG( "Unlocking achievement %s", ach.Name );
}

void GamerServices::UploadLeaderboardScoreAsync( Leaderboard inLead, int inScore )
{
	if( !mImpl->mAreLeadersReady )
	{
		LOG( "Leaderboards ERROR: Leaderboards not ready yet" );
		return;
	}

	SteamAPICall_t call = SteamUserStats()->UploadLeaderboardScore( mImpl->mLeaderArray[ inLead ].Handle,
		k_ELeaderboardUploadScoreMethodKeepBest, inScore, NULL, 0 );

	mImpl->mLeaderUploadResult.Set( call, mImpl.get(), &Impl::OnLeaderUploadCallback );
}

void GamerServices::DownloadLeaderboardAsync( Leaderboard inLead )
{
	if( !mImpl->mAreLeadersReady )
	{
		LOG( "Leaderboards ERROR: Leaderboards not ready yet" );
		return;
	}

	SteamAPICall_t call = SteamUserStats()->DownloadLeaderboardEntries( mImpl->mLeaderArray[ inLead ].Handle,
		k_ELeaderboardDataRequestGlobalAroundUser, 4, 5 );

	mImpl->mLeaderDownloadResult.Set( call, mImpl.get(), &Impl::OnLeaderDownloadCallback );
}

void GamerServices::RetrieveLeaderboardsAsync()
{
	//since we can only query one leaderboard at a time,
	//we'll just grab the first for now
	FindLeaderboardAsync( static_cast< Leaderboard >( 0 ) );
}

void GamerServices::FindLeaderboardAsync( Leaderboard inLead )
{
	mImpl->mCurrentLeaderFind = inLead;

	LeaderboardData& lead = mImpl->mLeaderArray[ inLead ];
	
	SteamAPICall_t call = SteamUserStats()->FindOrCreateLeaderboard( lead.Name,
		lead.SortMethod, lead.DisplayType );

	mImpl->mLeaderFindResult.Set( call, mImpl.get(), &Impl::OnLeaderFindCallback );
}

void GamerServices::Update()
{
	//without this, callbacks will never fire
	SteamAPI_RunCallbacks();
}

GamerServices::~GamerServices()
{
	//Disabled calling shutdown b/c it seems to crash on Mac
	//SteamAPI_Shutdown();
}

void SteamAPIDebugTextHook( int nSeverity, const char *pchDebugText )
{
	LOG( pchDebugText );
}

bool GamerServices::StaticInit()
{
	bool retVal = SteamAPI_Init();
	if( !retVal )
	{
		LOG( "Failed to initialize Steam. Make sure you are running a Steam client." );
	}
	else
	{
		sInstance.reset( new GamerServices() );
	}

	return retVal;
}

GamerServices::GamerServices()
{
	mImpl = std::make_unique<Impl>();

	SteamClient()->SetWarningMessageHook( &SteamAPIDebugTextHook );

	//get the current stats from the server
	RetrieveStatsAsync();

	//and the leaderboards
	RetrieveLeaderboardsAsync();
}

void GamerServices::DebugResetStats( bool inResetAchieves )
{
	if( !mImpl->mAreStatsReady )
	{
		LOG( "Stats ERROR: Stats not ready yet" );
		return;
	}

	LOG( "Stats resetting..." );
	//wipe our in-memory copy
	for( int i = 0; i < MAX_STAT; ++i )
	{
		StatData& stat = mImpl->mStatArray[ i ];
		stat.IntStat = 0;
		stat.FloatStat = 0.0f;
		stat.AvgRateStat.SessionValue = 0.0f;
		stat.AvgRateStat.SessionLength = 0.0f;
	}

	//tell steam to reset
	SteamUserStats()->ResetAllStats( inResetAchieves );
}
