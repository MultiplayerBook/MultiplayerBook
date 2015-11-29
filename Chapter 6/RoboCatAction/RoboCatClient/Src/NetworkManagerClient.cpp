#include <RoboCatClientPCH.h>

NetworkManagerClient*	NetworkManagerClient::sInstance;

namespace
{
	const float kTimeBetweenHellos = 1.f;
	const float kTimeBetweenInputPackets = 0.033f;
}

NetworkManagerClient::NetworkManagerClient() :
	mState( NCS_Uninitialized ),
	mLastRoundTripTime( 0.f )
{
}

void NetworkManagerClient::StaticInit( const SocketAddress& inServerAddress, const string& inName )
{
	sInstance = new NetworkManagerClient();
	return sInstance->Init( inServerAddress, inName );
}

void NetworkManagerClient::Init( const SocketAddress& inServerAddress, const string& inName )
{
	NetworkManager::Init( 0 );

	mServerAddress = inServerAddress;
	mState = NCS_SayingHello;
	mTimeOfLastHello = 0.f;
	mName = inName;

	mAvgRoundTripTime = WeightedTimedMovingAverage( 1.f );
}

void NetworkManagerClient::ProcessPacket( InputMemoryBitStream& inInputStream, const SocketAddress& inFromAddress )
{
	uint32_t	packetType;
	inInputStream.Read( packetType );
	switch( packetType )
	{
	case kWelcomeCC:
		HandleWelcomePacket( inInputStream );
		break;
	case kStateCC:
		HandleStatePacket( inInputStream );
		break;
	}
}


void NetworkManagerClient::SendOutgoingPackets()
{
	switch( mState )
	{
	case NCS_SayingHello:
		UpdateSayingHello();
		break;
	case NCS_Welcomed:
		UpdateSendingInputPacket();
		break;
	}
}

void NetworkManagerClient::UpdateSayingHello()
{
	float time = Timing::sInstance.GetTimef();

	if( time > mTimeOfLastHello + kTimeBetweenHellos )
	{
		SendHelloPacket();
		mTimeOfLastHello = time;
	}
}

void NetworkManagerClient::SendHelloPacket()
{
	OutputMemoryBitStream helloPacket; 

	helloPacket.Write( kHelloCC );
	helloPacket.Write( mName );

	SendPacket( helloPacket, mServerAddress );
}

void NetworkManagerClient::HandleWelcomePacket( InputMemoryBitStream& inInputStream )
{
	if( mState == NCS_SayingHello )
	{
		//if we got a player id, we've been welcomed!
		int playerId;
		inInputStream.Read( playerId );
		mPlayerId = playerId;
		mState = NCS_Welcomed;
		LOG( "'%s' was welcomed on client as player %d", mName.c_str(), mPlayerId );
	}
}



void NetworkManagerClient::HandleStatePacket( InputMemoryBitStream& inInputStream )
{
	if( mState == NCS_Welcomed )
	{
		ReadLastMoveProcessedOnServerTimestamp( inInputStream );

		//old
		//HandleGameObjectState( inPacketBuffer );
		HandleScoreBoardState( inInputStream );

		//tell the replication manager to handle the rest...
		mReplicationManagerClient.Read( inInputStream );
	}
}

void NetworkManagerClient::ReadLastMoveProcessedOnServerTimestamp( InputMemoryBitStream& inInputStream )
{
	bool isTimestampDirty;
	inInputStream.Read( isTimestampDirty );
	if( isTimestampDirty )
	{
		inInputStream.Read( mLastMoveProcessedByServerTimestamp );

		float rtt = Timing::sInstance.GetFrameStartTime() - mLastMoveProcessedByServerTimestamp;
		mLastRoundTripTime = rtt;
		mAvgRoundTripTime.Update( rtt );

		InputManager::sInstance->GetMoveList().RemovedProcessedMoves( mLastMoveProcessedByServerTimestamp );

	}
}

void NetworkManagerClient::HandleGameObjectState( InputMemoryBitStream& inInputStream )
{
	//copy the mNetworkIdToGameObjectMap so that anything that doesn't get an updated can be destroyed...
	IntToGameObjectMap	objectsToDestroy = mNetworkIdToGameObjectMap;

	int stateCount;
	inInputStream.Read( stateCount );
	if( stateCount > 0 )
	{
		for( int stateIndex = 0; stateIndex < stateCount; ++stateIndex )
		{
			int networkId;
			uint32_t fourCC;

			inInputStream.Read( networkId );
			inInputStream.Read( fourCC );
			GameObjectPtr go;
			auto itGO = mNetworkIdToGameObjectMap.find( networkId );
			//didn't find it, better create it!
			if( itGO == mNetworkIdToGameObjectMap.end() )
			{
				go = GameObjectRegistry::sInstance->CreateGameObject( fourCC );
				go->SetNetworkId( networkId );
				AddToNetworkIdToGameObjectMap( go );
			}
			else
			{
				//found it
				go = itGO->second;
			}

			//now we can update into it
			go->Read( inInputStream );
			objectsToDestroy.erase( networkId );
		}
	}

	//anything left gets the axe
	DestroyGameObjectsInMap( objectsToDestroy );
}

void NetworkManagerClient::HandleScoreBoardState( InputMemoryBitStream& inInputStream )
{
	ScoreBoardManager::sInstance->Read( inInputStream );
}
 
void NetworkManagerClient::DestroyGameObjectsInMap( const IntToGameObjectMap& inObjectsToDestroy )
{
	for( auto& pair: inObjectsToDestroy )
	{
		pair.second->SetDoesWantToDie( true );
		//and remove from our map!
		mNetworkIdToGameObjectMap.erase( pair.first );
	}

	
}


void NetworkManagerClient::UpdateSendingInputPacket()
{
	float time = Timing::sInstance.GetTimef();

	if( time > mTimeOfLastInputPacket + kTimeBetweenInputPackets )
	{
		SendInputPacket();
		mTimeOfLastInputPacket = time;
	}
}

void NetworkManagerClient::SendInputPacket()
{
	//only send if there's any input to send!
	MoveList& moveList = InputManager::sInstance->GetMoveList();

	if( moveList.HasMoves() )
	{
		OutputMemoryBitStream inputPacket; 
		inputPacket.Write( kInputCC );

		//we only want to send the last three moves
		int moveCount = moveList.GetMoveCount();
		int startIndex = moveCount > 3 ? moveCount - 3 - 1 : 0;
		inputPacket.Write( moveCount - startIndex, 2 );
		for( int i = startIndex; i < moveCount; ++i )
		{
			moveList[i].Write( inputPacket );
		}

		SendPacket( inputPacket, mServerAddress );
		moveList.Clear();
	}
}