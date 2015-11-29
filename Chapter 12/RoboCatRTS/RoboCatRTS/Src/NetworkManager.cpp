#include "RoboCatPCH.h"
#include <zlib.h>
unique_ptr< NetworkManager > NetworkManager::sInstance;

namespace
{
	const float kTimeBetweenDelayHeartbeat = 1.f;
	const float kStartDelay = 3.0f;
	const int	kSubTurnsPerTurn = 3;
	const int	kMaxPlayerCount = 4;
}

bool NetworkManager::StaticInit()
{
	sInstance = std::make_unique< NetworkManager >();

	return sInstance->Init();
}

NetworkManager::NetworkManager() :
	mBytesSentThisFrame( 0 ),
	mDropPacketChance( 0.f ),
	mSimulatedLatency( 0.f ),
	mBytesReceivedPerSecond( WeightedTimedMovingAverage( 1.f ) ),
	mBytesSentPerSecond(WeightedTimedMovingAverage( 1.f )),
	mPlayerId( 0 ),
	mLobbyId( 0 ),
	mNewNetworkId( 1 ),
	mIsMasterPeer( false ),
	mState( NMS_Unitialized ),
	mPlayerCount( 0 ),
	mReadyCount( 0 ),
	mDelayHeartbeat( kTimeBetweenDelayHeartbeat ),
	mTimeToStart( -1.0f ),
	//we always start on turn -2 b/c we need 2 frames before we can actually play
	mTurnNumber( -2 ),
	mSubTurnNumber( 0 )
{
	//this is enough for a 166 minute game...
	//so let's avoid realloc/copies and just construct all the empty maps, too
	mTurnData.resize( 100000 );
}

NetworkManager::~NetworkManager()
{
}

bool NetworkManager::Init()
{
	//set my player info from steam
	mPlayerId = GamerServices::sInstance->GetLocalPlayerId();
	mName = GamerServices::sInstance->GetLocalPlayerName();

	//begin the search for a lobby
	mState = NMS_Searching;
	GamerServices::sInstance->LobbySearchAsync();

	return true;
}

void NetworkManager::ProcessIncomingPackets()
{
	ReadIncomingPacketsIntoQueue();

	ProcessQueuedPackets();

	UpdateBytesSentLastFrame();

}

void NetworkManager::SendOutgoingPackets()
{
	switch ( mState )
	{
	case NMS_Starting:
		UpdateStarting();
		break;
	case NMS_Playing:
		UpdateSendTurnPacket();
		break;
	default:
		break;
	}
}

void NetworkManager::UpdateDelay()
{
	//first process incoming packets, in case that removes us from delay
	NetworkManager::sInstance->ProcessIncomingPackets();

	if( mState == NMS_Delay )
	{
		mDelayHeartbeat -= Timing::sInstance.GetDeltaTime();
		if( mDelayHeartbeat <= 0.0f )
		{
			mDelayHeartbeat = kTimeBetweenDelayHeartbeat;
		}

		//find out who's missing and send them a heartbeat
		unordered_set< uint64_t > playerSet;
		for( auto& iter : mPlayerNameMap )
		{
			playerSet.emplace( iter.first );
		}

		Int64ToTurnDataMap& turnData = mTurnData[ mTurnNumber + 1 ];
		for( auto& iter : turnData )
		{
			playerSet.erase( iter.first );
		}

		OutputMemoryBitStream packet;
		packet.Write( kDelayCC );
		//whoever's left is who's missing
		for( auto& iter : playerSet )
		{
			SendPacket( packet, iter );
		}
	}
}

void NetworkManager::UpdateStarting()
{
	mTimeToStart -= Timing::sInstance.GetDeltaTime();
	if ( mTimeToStart <= 0.0f )
	{
		EnterPlayingState();
	}
}

void NetworkManager::UpdateSendTurnPacket()
{
	mSubTurnNumber++;
	if ( mSubTurnNumber == kSubTurnsPerTurn )
	{
		//create our turn data
		TurnData data(mPlayerId, RandGen::sInstance->GetRandomUInt32(0, UINT32_MAX),
			ComputeGlobalCRC(), InputManager::sInstance->GetCommandList());

		//we need to send a turn packet to all of our peers
		OutputMemoryBitStream packet;
		packet.Write( kTurnCC );
		//we're sending data for 2 turns from now
		packet.Write( mTurnNumber + 2 );
		packet.Write( mPlayerId );
		data.Write( packet );

 		for ( auto &iter : mPlayerNameMap )
 		{
			if( iter.first != mPlayerId )
			{
				SendPacket( packet, iter.first );
			}
 		}

		//save our turn data for turn + 2
		mTurnData[ mTurnNumber + 2 ].emplace( mPlayerId, data );
		InputManager::sInstance->ClearCommandList();

		if ( mTurnNumber >= 0 )
		{
			TryAdvanceTurn();
		}
		else
		{
			//a negative turn means there's no possible commands yet
			mTurnNumber++;
			mSubTurnNumber = 0;
		}
	}
}


void NetworkManager::TryAdvanceTurn()
{
	//only advance the turn IF we received the data for everyone
	if ( mTurnData[ mTurnNumber + 1 ].size() == mPlayerCount )
	{
		if ( mState == NMS_Delay )
		{
			//throw away any input accrued during delay
			InputManager::sInstance->ClearCommandList();
			mState = NMS_Playing;
			//wait 100ms to give the slow peer a chance to catch up
			SDL_Delay( 100 );
		}

		mTurnNumber++;
		mSubTurnNumber = 0;

		if ( CheckSync( mTurnData[ mTurnNumber ] ) )
		{
			//process all the moves for this turn
			for ( auto& iter : mTurnData[ mTurnNumber ] )
			{
				iter.second.GetCommandList().ProcessCommands( iter.first );
			}

			//since we're still in sync, let's check for achievements
			CheckForAchievements();
		}
		else
		{
			//for simplicity, just kill the game if it desyncs
			LOG( "DESYNC: Game over man, game over." );
			Engine::sInstance->SetShouldKeepRunning( false );
		}
	}
	else
	{
		//don't have all player's turn data, we have to delay :(
		mState = NMS_Delay;
		LOG( "Going into delay state, don't have all the info for turn %d", mTurnNumber + 1);
	}
}

void NetworkManager::ProcessPacket( InputMemoryBitStream& inInputStream, uint64_t inFromPlayer )
{
	switch ( mState )
	{
	case NMS_Lobby:
		ProcessPacketsLobby( inInputStream, inFromPlayer );
		break;
	case NMS_Ready:
		ProcessPacketsReady( inInputStream, inFromPlayer );
		break;
	case NMS_Starting:
		//if I'm starting and get a packet, treat this as playing
		ProcessPacketsPlaying( inInputStream, inFromPlayer );
		break;
	case NMS_Playing:
		ProcessPacketsPlaying( inInputStream, inFromPlayer );
		break;
	case NMS_Delay:
		ProcessPacketsDelay( inInputStream, inFromPlayer );
		break;
	default:
		break;
	}
}

void NetworkManager::ProcessPacketsLobby( InputMemoryBitStream& inInputStream, uint64_t inFromPlayer )
{
	//should only be a ready packet
	uint32_t	packetType;
	inInputStream.Read( packetType );

	switch ( packetType )
	{
	case kReadyCC:
		HandleReadyPacket( inInputStream, inFromPlayer );
		break;
	default:
		//ignore anything else
		break;
	}
}

void NetworkManager::ProcessPacketsReady( InputMemoryBitStream& inInputStream, uint64_t inFromPlayer )
{
	//could be another ready packet or a start packet
	uint32_t	packetType;
	inInputStream.Read( packetType );

	switch( packetType )
	{
	case kReadyCC:
		HandleReadyPacket( inInputStream, inFromPlayer );
		break;
	case kStartCC:
		HandleStartPacket( inInputStream, inFromPlayer );
		break;
	default:
		//ignore anything else
		break;
	}
}

void NetworkManager::HandleReadyPacket( InputMemoryBitStream& inInputStream, uint64_t inFromPlayer )
{
	//if this is my first ready packet, I need to let everyone else know I'm ready
	if( mReadyCount == 0 )
	{
		SendReadyPacketsToPeers();
		//I'm ready now also, so an extra increment here
		mReadyCount++;
		mState = NMS_Ready;
	}

	mReadyCount++;

	TryStartGame();
}

void NetworkManager::SendReadyPacketsToPeers()
{
	OutputMemoryBitStream outPacket;
	outPacket.Write( kReadyCC );
	for( auto& iter : mPlayerNameMap )
	{
		if( iter.first != mPlayerId )
		{
			SendPacket( outPacket, iter.first );
		}
	}
}

void NetworkManager::HandleStartPacket( InputMemoryBitStream& inInputStream, uint64_t inFromPlayer )
{
	//make sure this is the master peer, cause we don't want any funny business
	if ( inFromPlayer == mMasterPeerId )
	{
		LOG( "Got the orders to go!" );
		//get the rng seed
		uint32_t seed;
		inInputStream.Read( seed );
		RandGen::sInstance->Seed( seed );
		//for now, assume that we're one frame off, but ideally we would RTT to adjust
		//the time to start, based on latency/jitter
		mState = NMS_Starting;
		mTimeToStart = kStartDelay - Timing::sInstance.GetDeltaTime();
	}
}

void NetworkManager::ProcessPacketsPlaying( InputMemoryBitStream& inInputStream, uint64_t inFromPlayer )
{
	uint32_t	packetType;
	inInputStream.Read( packetType );

	switch ( packetType )
	{
	case kTurnCC:
		HandleTurnPacket( inInputStream, inFromPlayer );
		break;
	default:
		//ignore anything else
		break;
	}
}

void NetworkManager::HandleTurnPacket( InputMemoryBitStream& inInputStream, uint64_t inFromPlayer )
{
	int turnNum;
	uint64_t playerId;
	inInputStream.Read( turnNum );
	inInputStream.Read( playerId );

	if ( playerId != inFromPlayer )
	{
		LOG( "We received turn data for a different player Id...stop trying to cheat!" );
		return;
	}

	TurnData data;
	data.Read( inInputStream );

	mTurnData[ turnNum ].emplace( playerId, data );
}

void NetworkManager::ProcessPacketsDelay( InputMemoryBitStream& inInputStream, uint64_t inFromPlayer )
{
	//the only packet we can even consider here is an input one, since we
	//only can only enter delay after we've been playing
	uint32_t	packetType;
	inInputStream.Read( packetType );

	if ( packetType == kTurnCC )
	{
		HandleTurnPacket( inInputStream, inFromPlayer );
		//if we're lucky, maybe this was the packet we were waiting on?
		TryAdvanceTurn();
	}
}

void NetworkManager::HandleConnectionReset( uint64_t inFromPlayer )
{
	//remove this player from our maps
	if( mPlayerNameMap.find( inFromPlayer ) != mPlayerNameMap.end() )
	{		
		LOG( "Player %llu disconnected", inFromPlayer );
		mPlayerNameMap.erase( inFromPlayer );
		ScoreBoardManager::sInstance->RemoveEntry( inFromPlayer );

		mPlayerCount--;

		//if we were in delay, then let's see if we can continue now that this player DC'd?
		if ( mState == NMS_Delay )
		{
			TryAdvanceTurn();
		}
	}
}

void NetworkManager::ReadIncomingPacketsIntoQueue()
{
	//should we just keep a static one?
	char packetMem[ 1500 ];
	size_t packetSize = sizeof( packetMem );
	uint32_t incomingSize = 0;
	InputMemoryBitStream inputStream( packetMem, packetSize * 8 );
	uint64_t fromPlayer;

	//keep reading until we don't have anything to read ( or we hit a max number that we'll process per frame )
	int receivedPackedCount = 0;
	int totalReadByteCount = 0;

	while( GamerServices::sInstance->IsP2PPacketAvailable( incomingSize ) && 
			receivedPackedCount < kMaxPacketsPerFrameCount )
	{
		if( incomingSize <= packetSize )
		{
			uint32_t readByteCount = GamerServices::sInstance->ReadP2PPacket( packetMem, packetSize, fromPlayer );
			if( readByteCount > 0 )
			{
				inputStream.ResetToCapacity( readByteCount );
				++receivedPackedCount;
				totalReadByteCount += readByteCount;

				//shove the packet into the queue and we'll handle it as soon as we should...
				//we'll pretend it wasn't received until simulated latency from now
				//this doesn't sim jitter, for that we would need to.....
				float simulatedReceivedTime = Timing::sInstance.GetTimef() + mSimulatedLatency;

				mPacketQueue.emplace( simulatedReceivedTime, inputStream, fromPlayer );
			}
		}
	}

	if( totalReadByteCount > 0 )
	{
		mBytesReceivedPerSecond.UpdatePerSecond( static_cast< float >( totalReadByteCount ) );
	}
}

void NetworkManager::ProcessQueuedPackets()
{
	//look at the front packet...
	while( !mPacketQueue.empty() )
	{
		ReceivedPacket& nextPacket = mPacketQueue.front();
		if( Timing::sInstance.GetTimef() > nextPacket.GetReceivedTime() )
		{
			ProcessPacket( nextPacket.GetPacketBuffer(), nextPacket.GetFromPlayer() );
			mPacketQueue.pop();
		}
		else
		{
			break;
		}
	
	}

}

void NetworkManager::EnterPlayingState()
{
	mState = NMS_Playing;

	//leave the lobby now that we're playing
	GamerServices::sInstance->LeaveLobby( mLobbyId );

	//create scoreboard entry for each player
	for ( auto& iter : mPlayerNameMap )
	{
		ScoreBoardManager::sInstance->AddEntry( iter.first, iter.second );
		//everyone gets a score of 3 cause 3 cats
		ScoreBoardManager::sInstance->GetEntry( iter.first )->SetScore( 3 );
	}

	//spawn a cat for each player

	float halfWidth = kWorldWidth / 2.0f;
	float halfHeight = kWorldHeight / 2.0f;

	// ( pos.x, pos.y, rot )
	std::array<Vector3, 4> spawnLocs = {
		Vector3( -halfWidth + halfWidth / 5, -halfHeight + halfHeight / 5, 2.35f ), // UP-LEFT
		Vector3( -halfWidth + halfWidth / 5, halfHeight - halfHeight / 4, -5.49f ), // DOWN-LEFT
		Vector3( halfWidth - halfWidth / 5, halfHeight - halfHeight / 4, -0.78f ), // DOWN-RIGHT
		Vector3( halfWidth - halfWidth / 5, -halfHeight + halfHeight / 4, -2.35f ), // UP-RIGHT
	};

	//use this to randomize location of cats
	std::array<int, 4> indices = { 0, 1, 2, 3 };
	std::shuffle( indices.begin(), indices.end(), RandGen::sInstance->GetGeneratorRef() );

	const float kCatOffset = 1.0f;

	int i = 0;
	for ( auto& iter : mPlayerNameMap )
	{
		Vector3 spawnVec = spawnLocs[ indices[ i ] ];
		//spawn 3 cats per player
		SpawnCat( iter.first, spawnVec );
		if ( spawnVec.mX > 0.0f )
		{
			SpawnCat( iter.first, Vector3( spawnVec.mX - kCatOffset, spawnVec.mY, spawnVec.mZ ) );
		}
		else
		{
			SpawnCat( iter.first, Vector3( spawnVec.mX + kCatOffset, spawnVec.mY, spawnVec.mZ ) );
		}

		if ( spawnVec.mY > 0.0f )
		{
			SpawnCat( iter.first, Vector3( spawnVec.mX, spawnVec.mY - kCatOffset, spawnVec.mZ ) );
		}
		else
		{
			SpawnCat( iter.first, Vector3( spawnVec.mX, spawnVec.mY + kCatOffset, spawnVec.mZ ) );
		}
		i++;
	}

	//Increment games played stat
	GamerServices::sInstance->AddToStat( GamerServices::Stat_NumGames, 1 );
}

void NetworkManager::SpawnCat( uint64_t inPlayerId, const Vector3& inSpawnVec )
{
	RoboCatPtr cat = std::static_pointer_cast< RoboCat >( GameObjectRegistry::sInstance->CreateGameObject( 'RCAT' ) );
	cat->SetColor( ScoreBoardManager::sInstance->GetEntry( inPlayerId )->GetColor() );
	cat->SetPlayerId( inPlayerId );
	cat->SetLocation( Vector3( inSpawnVec.mX, inSpawnVec.mY, 0.0f ) );
	cat->SetRotation( inSpawnVec.mZ );
}

void NetworkManager::CheckForAchievements()
{
	for( int i = 0; i < GamerServices::MAX_ACHIEVEMENT; ++i )
	{
		GamerServices::Achievement ach = static_cast< GamerServices::Achievement >( i );
		switch( ach )
		{
		case GamerServices::ACH_WIN_ONE_GAME:
			if( GamerServices::sInstance->GetStatInt( GamerServices::Stat_NumWins ) > 0 )
			{
				GamerServices::sInstance->UnlockAchievement( ach );
			}
			break;
		case GamerServices::ACH_WIN_100_GAMES:
			if( GamerServices::sInstance->GetStatInt( GamerServices::Stat_NumWins ) >= 100 )
			{
				GamerServices::sInstance->UnlockAchievement( ach );
			}
			break;
		case GamerServices::ACH_TRAVEL_FAR_ACCUM:
		case GamerServices::ACH_TRAVEL_FAR_SINGLE:
		default:
			//nothing for these
			break;
		}
	}
}

bool NetworkManager::CheckSync( Int64ToTurnDataMap& inTurnMap )
{
	auto iter = inTurnMap.begin();
	
	uint32_t expectedRand = iter->second.GetRandomValue();
	uint32_t expectedCRC = iter->second.GetCRC();
	
	++iter;
	while ( iter != inTurnMap.end() )
	{
		if ( expectedRand != iter->second.GetRandomValue() )
		{
			LOG( "Random is out of sync for player %llu on turn %d", iter->second.GetPlayerId(), mTurnNumber );
			return false;
		}

		if ( expectedCRC != iter->second.GetCRC() )
		{
			LOG( "CRC is out of sync for player %llu on turn %d", iter->second.GetPlayerId(), mTurnNumber );
			return false;
		}
		++iter;
	}

	return true;
}

void NetworkManager::SendPacket( const OutputMemoryBitStream& inOutputStream, uint64_t inToPlayer )
{
	GamerServices::sInstance->SendP2PReliable( inOutputStream, inToPlayer );
}

void NetworkManager::EnterLobby( uint64_t inLobbyId )
{
	mLobbyId = inLobbyId;
	mState = NMS_Lobby;
	UpdateLobbyPlayers();
}

void NetworkManager::UpdateLobbyPlayers()
{
	//we only want to update player counts in lobby before we're starting
	if( mState < NMS_Starting )
	{
		mPlayerCount = GamerServices::sInstance->GetLobbyNumPlayers( mLobbyId );
		mMasterPeerId = GamerServices::sInstance->GetMasterPeerId( mLobbyId );
		//am I the master peer now?
		if( mMasterPeerId == mPlayerId )
		{
			mIsMasterPeer = true;
		}

		GamerServices::sInstance->GetLobbyPlayerMap( mLobbyId, mPlayerNameMap );

		//this might allow us to start
		TryStartGame();
	}
}

void NetworkManager::TryStartGame()
{
	if ( mState == NMS_Ready && IsMasterPeer() && mPlayerCount == mReadyCount )
	{
		LOG( "Starting!" );
		//let everyone know
		OutputMemoryBitStream outPacket;
		outPacket.Write( kStartCC );
		
		//select a seed value
		uint32_t seed = RandGen::sInstance->GetRandomUInt32( 0, UINT32_MAX );
		RandGen::sInstance->Seed( seed );
		outPacket.Write( seed );

		for ( auto &iter : mPlayerNameMap )
		{
			if( iter.first != mPlayerId )
			{
				SendPacket( outPacket, iter.first );
			}
		}

		mTimeToStart = kStartDelay;
		mState = NMS_Starting;
	}
}

void NetworkManager::TryReadyGame()
{
	if( mState == NMS_Lobby && IsMasterPeer() )
	{
		LOG( "Master peer readying up!" );
		//let the gamer services know we're readying up
		GamerServices::sInstance->SetLobbyReady( mLobbyId );

		SendReadyPacketsToPeers();

		mReadyCount = 1;
		mState = NMS_Ready;

		//we might be ready to start
		TryStartGame();
	}
}

void NetworkManager::UpdateBytesSentLastFrame()
{
	if( mBytesSentThisFrame > 0 )
	{
		mBytesSentPerSecond.UpdatePerSecond( static_cast< float >( mBytesSentThisFrame ) );

		mBytesSentThisFrame = 0;
	}

}


NetworkManager::ReceivedPacket::ReceivedPacket( float inReceivedTime, InputMemoryBitStream& ioInputMemoryBitStream, uint64_t inFromPlayer ) :
	mReceivedTime( inReceivedTime ),
	mFromPlayer( inFromPlayer ),
	mPacketBuffer( ioInputMemoryBitStream )
{
}


GameObjectPtr NetworkManager::GetGameObject( uint32_t inNetworkId ) const
{
	auto gameObjectIt = mNetworkIdToGameObjectMap.find( inNetworkId );
	if ( gameObjectIt != mNetworkIdToGameObjectMap.end() )
	{
		return gameObjectIt->second;
	}
	else
	{
		return GameObjectPtr();
	}
}

GameObjectPtr NetworkManager::RegisterAndReturn( GameObject* inGameObject )
{
	GameObjectPtr toRet( inGameObject );
	RegisterGameObject( toRet );
	return toRet;
}

void NetworkManager::UnregisterGameObject( GameObject* inGameObject )
{
	int networkId = inGameObject->GetNetworkId();
	auto iter = mNetworkIdToGameObjectMap.find( networkId );
	if ( iter != mNetworkIdToGameObjectMap.end() )
	{
		mNetworkIdToGameObjectMap.erase( iter );
	}
}

bool NetworkManager::IsPlayerInGame( uint64_t inPlayerId )
{
	if( mPlayerNameMap.find( inPlayerId ) != mPlayerNameMap.end() )
	{
		return true;
	}
	else
	{
		return false;
	}
}

void NetworkManager::AddToNetworkIdToGameObjectMap( GameObjectPtr inGameObject )
{
	mNetworkIdToGameObjectMap[ inGameObject->GetNetworkId() ] = inGameObject;
}

void NetworkManager::RemoveFromNetworkIdToGameObjectMap( GameObjectPtr inGameObject )
{
	mNetworkIdToGameObjectMap.erase( inGameObject->GetNetworkId() );
}

void NetworkManager::RegisterGameObject( GameObjectPtr inGameObject )
{
	//assign network id
	int newNetworkId = GetNewNetworkId();
	inGameObject->SetNetworkId( newNetworkId );

	//add mapping from network id to game object
	mNetworkIdToGameObjectMap[ newNetworkId ] = inGameObject;
}

uint32_t NetworkManager::GetNewNetworkId()
{
	uint32_t toRet = mNewNetworkId++;
	if ( mNewNetworkId < toRet )
	{
		LOG( "Network ID Wrap Around!!! You've been playing way too long...", 0 );
	}

	return toRet;
}

uint32_t NetworkManager::ComputeGlobalCRC()
{
	//save into bit stream to reduce CRC calls
	OutputMemoryBitStream crcStream;

	uint32_t crc = static_cast<uint32_t>( crc32( 0, Z_NULL, 0 ) );

	for ( auto& iter : mNetworkIdToGameObjectMap )
	{
		iter.second->WriteForCRC( crcStream );
	}

	crc = static_cast<uint32_t>( crc32( crc, reinterpret_cast<const Bytef*>(crcStream.GetBufferPtr()), crcStream.GetByteLength() ) );
	return crc;
}
