
class GamerServices
{
public:
	static bool StaticInit();
	static std::unique_ptr< GamerServices >	sInstance;
	
	//general player functions
	uint64_t GetLocalPlayerId();
	string GetLocalPlayerName();
	string GetRemotePlayerName( uint64_t inPlayerId );

	//lobby functions
	void LobbySearchAsync();
	int GetLobbyNumPlayers( uint64_t inLobbyId );
	uint64_t GetMasterPeerId( uint64_t inLobbyId );
	void GetLobbyPlayerMap( uint64_t inLobbyId, map< uint64_t, string >& outPlayerMap );
	void SetLobbyReady( uint64_t inLobbyId );
	void LeaveLobby( uint64_t inLobbyId );

	//peer-to-peer networking
	bool SendP2PReliable( const OutputMemoryBitStream& inOutputStream, uint64_t inToPlayer );
	bool IsP2PPacketAvailable( uint32_t& outPacketSize );
	uint32_t ReadP2PPacket( void* inToReceive, uint32_t inMaxLength, uint64_t& outFromPlayer );

	//stats
	enum Stat
	{
		#define STAT(a,b) Stat_##a,
		#include "Stats.def"
		#undef STAT
		MAX_STAT
	};

	enum class StatType
	{
		INT,
		FLOAT,
		AVGRATE
	};

	int GetStatInt( Stat inStat );
	float GetStatFloat( Stat inStat );
	float GetStatAvgRate( Stat inStat );

	//for int/float stats, we can just add to the stat and write to the server immediately
	void AddToStat( Stat inStat, int inInc );
	void AddToStat( Stat inStat, float inInc );
	//for average rate stats, we first update the locally cached values
	//then write the average rate for the interval to the server, resetting the local cache
	void UpdateAvgRateLocal( Stat inStat, float inIncValue, float inIncSeconds );
	void WriteAvgRate( Stat inStat );
protected:
	void RetrieveStatsAsync();
public:

	//achievements
	enum Achievement
	{
		#define ACH(a) a,
		#include "Achieve.def"
		#undef ACH
		MAX_ACHIEVEMENT
	};

	bool IsAchievementUnlocked( Achievement inAch );
	void UnlockAchievement( Achievement inAch );

	//leaderboards
	enum Leaderboard
	{
		#define BOARD(a,b,c) LB_##a,
		#include "Leaderboards.def"
		#undef BOARD
		MAX_LEADERBOARD
	};
	
	void UploadLeaderboardScoreAsync( Leaderboard inLead, int inScore );
	void DownloadLeaderboardAsync( Leaderboard inLead );
protected:
	void RetrieveLeaderboardsAsync();
	void FindLeaderboardAsync( Leaderboard inLead );
public:

	void Update();

	~GamerServices();

	struct Impl;

	//debugging
	void DebugResetStats( bool inResetAchieves );
private:
	GamerServices();
	
	std::unique_ptr< Impl > mImpl;
};
