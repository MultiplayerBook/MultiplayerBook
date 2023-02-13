#include "RoboCatPCH.h"
#include <zlib.h>
unique_ptr< NetworkManager > NetworkManager::sInstance;

namespace
{
	const float kTimeBetweenHellos = 1.f;
	const float kStartDelay = 3.0f;
	const int	kSubTurnsPerTurn = 3;
	const int	kMaxPlayerCount = 4;
}

bool NetworkManager::StaticInitAsMasterPeer( uint16_t inPort, const string& inName )
{
	sInstance = std::make_unique< NetworkManager >();

	return sInstance->InitAsMasterPeer( inPort, inName );
}

bool NetworkManager::StaticInitAsPeer( const SocketAddress& inMPAddress, const string& inName )
{
	sInstance = std::make_unique< NetworkManager >();
	return sInstance->InitAsPeer( inMPAddress, inName );
}

NetworkManager::NetworkManager() :
	mBytesSentThisFrame( 0 ),
	mDropPacketChance( 0.f ),
	mSimulatedLatency( 0.f ),
	mBytesReceivedPerSecond( WeightedTimedMovingAverage( 1.f ) ),
	mBytesSentPerSecond(WeightedTimedMovingAverage( 1.f )),
	mPlayerId( 0 ),
	mNewNetworkId( 1 ),
	mIsMasterPeer( false ),
	mState( NMS_Unitialized ),
	mPlayerCount( 0 ),
	mHighestPlayerId( 0 ),
	mTimeOfLastHello( 0.0f ),
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

bool NetworkManager::InitAsMasterPeer( uint16_t inPort, const string& inName )
{
	if( !InitSocket( inPort ) )
	{
		return false;
	}

	//master peer at init means guaranteed first player id
	mPlayerId = 1;
	mHighestPlayerId = mPlayerId;
	mIsMasterPeer = true;
	mPlayerCount = 1;

	//in lobby cause we don't need to ask the master peer (since we are the master peer)
	mState = NMS_Lobby;

	//add myself to the player name map
	mName = inName;
	mPlayerNameMap.emplace( mPlayerId, mName );
	return true;
}

bool NetworkManager::InitAsPeer( const SocketAddress& inMPAddress, const string& inName )
{
	if( !InitSocket( 0 ) )
	{
		return false;
	}

	mMasterPeerAddr = inMPAddress;

	//we're going to have to ask the master peer
	mState = NMS_Hello;

	//set my name
	mName = inName;
	//don't know my player id, so can't add myself to the name map yet

	return true;
}

bool NetworkManager::InitSocket( uint16_t inPort )
{
	mSocket = SocketUtil::CreateUDPSocket( INET );
	SocketAddress ownAddress( INADDR_ANY, inPort );
	mSocket->Bind( ownAddress );

	LOG( "Initializing NetworkManager at port %d", inPort );

	//did we bind okay?
	if( mSocket == nullptr )
	{
		return false;
	}

	if( mSocket->SetNonBlockingMode( true ) != NO_ERROR )
	{
		return false;
	}

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
	case NMS_Hello:
		UpdateSayingHello();
		break;
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

void NetworkManager::UpdateSayingHello( bool inForce )
{
	float time = Timing::sInstance.GetTimef();

	if ( inForce || time > mTimeOfLastHello + kTimeBetweenHellos )
	{
		SendHelloPacket();
		mTimeOfLastHello = time;
	}
}

void NetworkManager::SendHelloPacket()
{
	OutputMemoryBitStream helloPacket;

	helloPacket.Write( kHelloCC );
	helloPacket.Write( mName );

	SendPacket( helloPacket, mMasterPeerAddr );
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

		for ( auto &iter : mPlayerToSocketMap )
		{
			SendPacket( packet, iter.second );
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

void NetworkManager::ProcessPacket( InputMemoryBitStream& inInputStream, const SocketAddress& inFromAddress )
{
	switch ( mState )
	{
	case NMS_Hello:
		ProcessPacketsHello( inInputStream, inFromAddress );
		break;
	case NMS_Lobby:
		ProcessPacketsLobby( inInputStream, inFromAddress );
		break;
	case NMS_Playing:
		ProcessPacketsPlaying( inInputStream, inFromAddress );
		break;
	case NMS_Delay:
		ProcessPacketsDelay( inInputStream, inFromAddress );
		break;
	default:
		break;
	}
}

void NetworkManager::ProcessPacketsHello( InputMemoryBitStream& inInputStream, const SocketAddress& inFromAddress )
{
	//only time we're in hello state is when we are not the master peer and trying to join
	uint32_t	packetType;
	inInputStream.Read( packetType );

	switch ( packetType )
	{
	case kNotMasterPeerCC:
		HandleNotMPPacket( inInputStream );
		break;
	case kWelcomeCC:
		HandleWelcomePacket( inInputStream );
		break;
	case kNotJoinableCC:
	default:
		//couldn't join or something crazy happened, goodbye cruel world
		LOG( "Failed to join game, exiting..." );
		Engine::sInstance->SetShouldKeepRunning( false );
		break;
	}
}

void NetworkManager::HandleNotMPPacket( InputMemoryBitStream& inInputStream )
{
	//this will have the correct master peer address
	mMasterPeerAddr.Read( inInputStream );
	//just force send this immediately
	UpdateSayingHello( true );
}

void NetworkManager::HandleWelcomePacket( InputMemoryBitStream& inInputStream )
{
	//first is my player id
	int playerId;
	inInputStream.Read( playerId );
	UpdateHighestPlayerId( playerId );
	mPlayerId = playerId;
	
	//add me to the name map
	mPlayerNameMap.emplace( mPlayerId, mName );

	//now the player id for the master peer
	//add entries for the master peer
	inInputStream.Read( playerId );
	UpdateHighestPlayerId( playerId );
	mPlayerToSocketMap.emplace( playerId, mMasterPeerAddr );
	mSocketToPlayerMap.emplace( mMasterPeerAddr, playerId );

	//now remaining players
	inInputStream.Read( mPlayerCount );
	SocketAddress socketAddr;
	// there's actually count - 1 entries because the master peer won't have an entry for themselves
	for ( int i = 0; i < mPlayerCount - 1; ++i )
	{
		inInputStream.Read( playerId );
		UpdateHighestPlayerId( playerId );

		socketAddr.Read( inInputStream );

		mPlayerToSocketMap.emplace( playerId, socketAddr );
		mSocketToPlayerMap.emplace( socketAddr, playerId );
	}

	//now player names
	std::string name;
	for ( int i = 0; i < mPlayerCount; ++i )
	{
		inInputStream.Read( playerId );
		inInputStream.Read( name );
		mPlayerNameMap.emplace( playerId, name );
	}

	//now add 1 to the player count cause I've joined
	//(the master peer sends me the old list before adding me)
	mPlayerCount++;

	//now we need to send out an intro packet to every peer in the socket map
	OutputMemoryBitStream outPacket;
	outPacket.Write( kIntroCC );
	outPacket.Write( mPlayerId );
	outPacket.Write( mName );
	for ( auto& iter : mPlayerToSocketMap )
	{
		SendPacket( outPacket, iter.second );
	}

	//I've been welcomed, so I'm in the lobby now
	mState = NMS_Lobby;
}

void NetworkManager::ProcessPacketsLobby( InputMemoryBitStream& inInputStream, const SocketAddress& inFromAddress )
{
	//could be someone saying hello, an introduction, or a start packet
	uint32_t	packetType;
	inInputStream.Read( packetType );

	switch ( packetType )
	{
	case kHelloCC:
		HandleHelloPacket( inInputStream, inFromAddress );
		break;
	case kIntroCC:
		HandleIntroPacket( inInputStream, inFromAddress );
		break;
	case kStartCC:
		HandleStartPacket( inInputStream, inFromAddress );
		break;
	default:
		LOG( "Unexpected packet received in Lobby state. Ignoring." );
		break;
	}
}

void NetworkManager::HandleHelloPacket( InputMemoryBitStream& inInputStream, const SocketAddress& inFromAddress )
{
	//for now, if I already know of this player, throw away the packet
	//this doesn't work if there's packet loss
	if ( mSocketToPlayerMap.find( inFromAddress ) != mSocketToPlayerMap.end() )
	{
		return;
	}

	if ( mPlayerCount >= kMaxPlayerCount )
	{
		//sorry, can't join if full
		OutputMemoryBitStream outPacket;
		outPacket.Write( kNotJoinableCC );
		SendPacket( outPacket, inFromAddress );
		return;
	}

	if ( mIsMasterPeer )
	{
		//it'll only contain the new player's name
		string name;
		inInputStream.Read( name );

		OutputMemoryBitStream outputStream;
		outputStream.Write( kWelcomeCC );
		//we'll assign the next possible player id to this player
		mHighestPlayerId++;
		outputStream.Write( mHighestPlayerId );

		//write our player id
		outputStream.Write( mPlayerId );

		outputStream.Write( mPlayerCount );

		//now write the player to socket map
		for ( auto &iter : mPlayerToSocketMap )
		{
			outputStream.Write( iter.first );
			iter.second.Write( outputStream );
		}

		//and the player names
		for ( auto &iter : mPlayerNameMap )
		{
			outputStream.Write( iter.first );
			outputStream.Write( iter.second );
		}

		SendPacket( outputStream, inFromAddress );

		//increment the player count and add this player to maps
		mPlayerCount++;
		mPlayerToSocketMap.emplace( mHighestPlayerId, inFromAddress );
		mSocketToPlayerMap.emplace( inFromAddress, mHighestPlayerId );
		mPlayerNameMap.emplace( mHighestPlayerId, name );
	}
	else
	{
		//talk to the master peer, not me
		OutputMemoryBitStream outPacket;
		outPacket.Write( kNotMasterPeerCC );
		mMasterPeerAddr.Write( outPacket );
		SendPacket( outPacket, inFromAddress );
	}
}

void NetworkManager::HandleIntroPacket( InputMemoryBitStream& inInputStream, const SocketAddress& inFromAddress )
{
	//master peer can safely ignore the intro packet
	if ( !mIsMasterPeer )
	{
		//just contains player's id and name
		int playerId;
		string name;
		
		inInputStream.Read( playerId );
		inInputStream.Read( name );

		UpdateHighestPlayerId( playerId );

		mPlayerCount++;

		//add the new player to our maps
		mPlayerToSocketMap.emplace( playerId, inFromAddress );
		mSocketToPlayerMap.emplace( inFromAddress, playerId );
		mPlayerNameMap.emplace( playerId, name );
	}
}

void NetworkManager::HandleStartPacket( InputMemoryBitStream& inInputStream, const SocketAddress& inFromAddress )
{
	//make sure this is the master peer, cause we don't want any funny business
	if ( inFromAddress == mMasterPeerAddr )
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

void NetworkManager::ProcessPacketsPlaying( InputMemoryBitStream& inInputStream, const SocketAddress& inFromAddress )
{
	uint32_t	packetType;
	inInputStream.Read( packetType );

	switch ( packetType )
	{
	case kTurnCC:
		HandleTurnPacket( inInputStream, inFromAddress );
		break;
	default:
		//ignore anything else
		break;
	}
}

void NetworkManager::HandleTurnPacket( InputMemoryBitStream& inInputStream, const SocketAddress& inFromAddress )
{
	if ( mSocketToPlayerMap.find( inFromAddress ) != mSocketToPlayerMap.end() )
	{
		int expectedId = mSocketToPlayerMap[ inFromAddress ];

		int turnNum;
		int playerId;
		inInputStream.Read( turnNum );
		inInputStream.Read( playerId );

		if ( playerId != expectedId )
		{
			LOG( "We received turn data for a different player Id...stop trying to cheat!" );
			return;
		}

		TurnData data;
		data.Read( inInputStream );

		mTurnData[ turnNum ].emplace( playerId, data );
	}
}

void NetworkManager::ProcessPacketsDelay( InputMemoryBitStream& inInputStream, const SocketAddress& inFromAddress )
{
	//the only packet we can even consider here is an input one, since we
	//only can only enter delay after we've been playing
	uint32_t	packetType;
	inInputStream.Read( packetType );

	if ( packetType == kTurnCC )
	{
		HandleTurnPacket( inInputStream, inFromAddress );
		//if we're lucky, maybe this was the packet we were waiting on?
		TryAdvanceTurn();
	}
}

void NetworkManager::HandleConnectionReset( const SocketAddress& inFromAddress )
{
	//remove this player from our maps
	if ( mSocketToPlayerMap.find( inFromAddress ) != mSocketToPlayerMap.end() )
	{
		uint32_t playerId = mSocketToPlayerMap[ inFromAddress ];
		mSocketToPlayerMap.erase( inFromAddress );
		mPlayerToSocketMap.erase( playerId );
		mPlayerNameMap.erase( playerId );
		ScoreBoardManager::sInstance->RemoveEntry( playerId );

		mPlayerCount--;

		//if this was the master peer, pick the next player in the string map to be MP
		if ( inFromAddress == mMasterPeerAddr )
		{
			uint32_t newMPId = mPlayerNameMap.begin()->first;
			if ( newMPId == mPlayerId )
			{
				//I'm the new master peer, muahahahah
				mIsMasterPeer = true;
			}
			else
			{
				mMasterPeerAddr = mPlayerToSocketMap[ newMPId ];
			}
		}

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
	//should we just keep a static one?
	char packetMem[ 1500 ];
	int packetSize = sizeof( packetMem );
	InputMemoryBitStream inputStream( packetMem, packetSize * 8 );
	SocketAddress fromAddress;

	//keep reading until we don't have anything to read ( or we hit a max number that we'll process per frame )
	int receivedPackedCount = 0;
	int totalReadByteCount = 0;

	while( receivedPackedCount < kMaxPacketsPerFrameCount )
	{
		int readByteCount = mSocket->ReceiveFrom( packetMem, packetSize, fromAddress );
		if( readByteCount == 0 )
		{
			//nothing to read
			break;
		}
		else if( readByteCount == -WSAECONNRESET )
		{
			//port closed on other end, so DC this person immediately
			HandleConnectionReset( fromAddress );
		}
		else if( readByteCount > 0 )
		{
			inputStream.ResetToCapacity( readByteCount );
			++receivedPackedCount;
			totalReadByteCount += readByteCount;

			//now, should we drop the packet?
			if( RoboMath::GetRandomFloatNonGame() >= mDropPacketChance )
			{
				//we made it
				//shove the packet into the queue and we'll handle it as soon as we should...
				//we'll pretend it wasn't received until simulated latency from now
				//this doesn't sim jitter, for that we would need to.....

				float simulatedReceivedTime = Timing::sInstance.GetTimef() + mSimulatedLatency;
				mPacketQueue.emplace( simulatedReceivedTime, inputStream, fromAddress );
			}
			else
			{
				LOG( "Dropped packet!", 0 );
				//dropped!
			}
		}
		else
		{
			//uhoh, error? exit or just keep going?
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
			ProcessPacket( nextPacket.GetPacketBuffer(), nextPacket.GetFromAddress() );
			mPacketQueue.pop();
		}
		else
		{
			break;
		}
	
	}

}

void NetworkManager::UpdateHighestPlayerId( uint32_t inId )
{
	mHighestPlayerId = std::max( mHighestPlayerId, inId );
}

void NetworkManager::EnterPlayingState()
{
	mState = NMS_Playing;

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
}

void NetworkManager::SpawnCat( uint32_t inPlayerId, const Vector3& inSpawnVec )
{
	RoboCatPtr cat = std::static_pointer_cast< RoboCat >( GameObjectRegistry::sInstance->CreateGameObject( 'RCAT' ) );
	cat->SetColor( ScoreBoardManager::sInstance->GetEntry( inPlayerId )->GetColor() );
	cat->SetPlayerId( inPlayerId );
	cat->SetLocation( Vector3( inSpawnVec.mX, inSpawnVec.mY, 0.0f ) );
	cat->SetRotation( inSpawnVec.mZ );
}

bool NetworkManager::CheckSync( IntToTurnDataMap& inTurnMap )
{
	auto iter = inTurnMap.begin();
	
	uint32_t expectedRand = iter->second.GetRandomValue();
	uint32_t expectedCRC = iter->second.GetCRC();
	
	++iter;
	while ( iter != inTurnMap.end() )
	{
		if ( expectedRand != iter->second.GetRandomValue() )
		{
			LOG( "Random is out of sync for player %u on turn %d", iter->second.GetPlayerId(), mTurnNumber );
			return false;
		}

		if ( expectedCRC != iter->second.GetCRC() )
		{
			LOG( "CRC is out of sync for player %u on turn %d", iter->second.GetPlayerId(), mTurnNumber );
			return false;
		}
		++iter;
	}

	return true;
}

void NetworkManager::SendPacket( const OutputMemoryBitStream& inOutputStream, const SocketAddress& inToAddress )
{
	int sentByteCount = mSocket->SendTo( inOutputStream.GetBufferPtr(), inOutputStream.GetByteLength(), inToAddress );
	if( sentByteCount > 0 )
	{
		mBytesSentThisFrame += sentByteCount;
	}
}

void NetworkManager::TryStartGame()
{
	if ( mState < NMS_Starting && IsMasterPeer() )
	{
		LOG( "Master peer starting the game!" );
		
		//let everyone know
		OutputMemoryBitStream outPacket;
		outPacket.Write( kStartCC );
		
		//select a seed value
		uint32_t seed = RandGen::sInstance->GetRandomUInt32( 0, UINT32_MAX );
		RandGen::sInstance->Seed( seed );
		outPacket.Write( seed );

		for ( auto &iter : mPlayerToSocketMap )
		{
			SendPacket( outPacket, iter.second );
		}

		mTimeToStart = kStartDelay;
		mState = NMS_Starting;
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


NetworkManager::ReceivedPacket::ReceivedPacket( float inReceivedTime, InputMemoryBitStream& ioInputMemoryBitStream, const SocketAddress& inFromAddress ) :
	mReceivedTime( inReceivedTime ),
	mFromAddress( inFromAddress ),
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

	crc = static_cast<uint32_t>(
		crc32( crc, reinterpret_cast<const Bytef*>(crcStream.GetBufferPtr()), crcStream.GetByteLength() ) );
	return crc;
}
