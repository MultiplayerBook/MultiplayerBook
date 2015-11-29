#include "RoboCatPCH.h"

namespace
{
	const float kDelayBeforeAckTimeout = 0.5f;
}

DeliveryNotificationManager::DeliveryNotificationManager( bool inShouldSendAcks, bool inShouldProcessAcks ) :
mNextOutgoingSequenceNumber( 0 ),
mNextExpectedSequenceNumber( 0 ),
//everybody starts at 0...
mShouldSendAcks( inShouldSendAcks ),
mShouldProcessAcks( inShouldProcessAcks ),
mDeliveredPacketCount( 0 ),
mDroppedPacketCount( 0 ),
mDispatchedPacketCount( 0 )
{
}


//we're going away- log how well we did...
DeliveryNotificationManager::~DeliveryNotificationManager()
{
	LOG( "DNM destructor. Delivery rate %d%%, Drop rate %d%%",
		( 100 * mDeliveredPacketCount ) / mDispatchedPacketCount,
		( 100 * mDroppedPacketCount ) / mDispatchedPacketCount );
}



InFlightPacket* DeliveryNotificationManager::WriteSequenceNumber( OutputMemoryBitStream& inOutputStream )
{
	//write the sequence number, but also create an inflight packet for this...
	PacketSequenceNumber sequenceNumber = mNextOutgoingSequenceNumber++;
	inOutputStream.Write( sequenceNumber );

	++mDispatchedPacketCount;

	if( mShouldProcessAcks )
	{
		mInFlightPackets.emplace_back( sequenceNumber );

		return &mInFlightPackets.back();
	}
	else
	{
		return nullptr;
	}
}

void DeliveryNotificationManager::WriteAckData( OutputMemoryBitStream& inOutputStream )
{
	//we usually will only have one packet to ack
	//so we'll follow that with a 0 bit if that's the case
	//however, if we have more than 1, we'll make that 1 bit a 1 and then write 8 bits of how many packets
	//we could do some statistical analysis to determine if this is the best strategy but we'll use it for now

	//do we have any pending acks?
	//if so, write a 1 bit and write the first range
	//otherwise, write 0 bit
	bool hasAcks = ( mPendingAcks.size() > 0 );

	inOutputStream.Write( hasAcks );
	if( hasAcks )
	{
		//note, we could write all the acks
		mPendingAcks.front().Write( inOutputStream );
		mPendingAcks.pop_front();
	}
}



//returns wether to drop the packet- if sequence number is too low!
bool DeliveryNotificationManager::ProcessSequenceNumber( InputMemoryBitStream& inInputStream )
{
	PacketSequenceNumber	sequenceNumber;

	inInputStream.Read( sequenceNumber );
	if( sequenceNumber == mNextExpectedSequenceNumber )
	{
		mNextExpectedSequenceNumber = sequenceNumber + 1;
		//is this what we expect? great, let's add an ack to our pending list
		if( mShouldSendAcks )
		{
			AddPendingAck( sequenceNumber );
		}
		//and let's continue processing this packet...
		return true;
	}
	//is the sequence number less than our current expected sequence? silently drop it.
	//if this is due to wrapping around, we might fail to ack some packets that we should ack, but they'll get resent, so it's not a big deal
	//note that we don't have to re-ack it because our system doesn't reuse sequence numbers
	else if( sequenceNumber < mNextExpectedSequenceNumber )
	{
		return false;
	}
	else if( sequenceNumber > mNextExpectedSequenceNumber )
	{
		//we missed a lot of packets!
		//so our next expected packet comes after this one...
		mNextExpectedSequenceNumber = sequenceNumber + 1;
		//we should nack the missing packets..this will happen automatically inside AddPendingAck because
		//we're adding an unconsequitive ack
		//and then we can ack this and process it
		if( mShouldSendAcks )
		{
			AddPendingAck( sequenceNumber );
		}
		return true;
	}

	//drop packet if we couldn't even read sequence number!
	return false;
}


//in each packet we can ack a range
//anything in flight before the range will be considered nackd by the other side immediately
void DeliveryNotificationManager::ProcessAcks( InputMemoryBitStream& inInputStream )
{

	bool hasAcks;
	inInputStream.Read( hasAcks );
	if( hasAcks )
	{
		AckRange ackRange;
		ackRange.Read( inInputStream );

		//for each InfilghtPacket with a sequence number less than the start, handle delivery failure...
		PacketSequenceNumber nextAckdSequenceNumber = ackRange.GetStart();
		uint32_t onePastAckdSequenceNumber = nextAckdSequenceNumber + ackRange.GetCount();
		while( nextAckdSequenceNumber < onePastAckdSequenceNumber && !mInFlightPackets.empty() )
		{
			const auto& nextInFlightPacket = mInFlightPackets.front();
			//if the packet has a lower sequence number, we didn't get an ack for it, so it probably wasn't delivered
			PacketSequenceNumber nextInFlightPacketSequenceNumber = nextInFlightPacket.GetSequenceNumber();
			if( nextInFlightPacketSequenceNumber < nextAckdSequenceNumber )
			{
				//copy this so we can remove it before handling the failure- we don't want to find it when checking for state
				auto copyOfInFlightPacket = nextInFlightPacket;
				mInFlightPackets.pop_front();
				HandlePacketDeliveryFailure( copyOfInFlightPacket );
			}
			else if( nextInFlightPacketSequenceNumber == nextAckdSequenceNumber )
			{
				HandlePacketDeliverySuccess( nextInFlightPacket );
				//received!
				mInFlightPackets.pop_front();
				//decrement count, advance nextAckdSequenceNumber
				++nextAckdSequenceNumber;
			}
			else if( nextInFlightPacketSequenceNumber > nextAckdSequenceNumber )
			{
				//we've already ackd some packets in here.
				//keep this packet in flight, but keep going through the ack...
				++nextAckdSequenceNumber;
			}
		}
	}
}

void DeliveryNotificationManager::ProcessTimedOutPackets()
{
	float timeoutTime = Timing::sInstance.GetTimef() - kDelayBeforeAckTimeout;

	while( !mInFlightPackets.empty() )
	{
		const auto& nextInFlightPacket = mInFlightPackets.front();

		//was this packet dispatched before the current time minus the timeout duration?
		if( nextInFlightPacket.GetTimeDispatched() < timeoutTime )
		{
			//it failed! let us know about that
			HandlePacketDeliveryFailure( nextInFlightPacket );
			mInFlightPackets.pop_front();
		}
		else
		{
			//it wasn't, and packets are all in order by time here, so we know we don't have to check farther
			break;
		}
	}
}

void DeliveryNotificationManager::AddPendingAck( PacketSequenceNumber inSequenceNumber )
{
	//if you don't have a range yet, or you can't correctly extend the final range with the sequence number,
	//start a new range
	if( mPendingAcks.size() == 0 || !mPendingAcks.back().ExtendIfShould( inSequenceNumber ) )
	{
		mPendingAcks.emplace_back( inSequenceNumber );
	}
}


void DeliveryNotificationManager::HandlePacketDeliveryFailure( const InFlightPacket& inFlightPacket )
{
	++mDroppedPacketCount;
	inFlightPacket.HandleDeliveryFailure( this );

}


void DeliveryNotificationManager::HandlePacketDeliverySuccess( const InFlightPacket& inFlightPacket )
{
	++mDeliveredPacketCount;
	inFlightPacket.HandleDeliverySuccess( this );
}
