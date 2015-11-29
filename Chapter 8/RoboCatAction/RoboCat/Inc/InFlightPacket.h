class DeliveryNotificationManager;

//in case we decide to change the type of the sequence number to use fewer or more bits
typedef uint16_t	PacketSequenceNumber;

class InFlightPacket
{
public:
	
	InFlightPacket( PacketSequenceNumber inSequenceNumber );
	
	PacketSequenceNumber GetSequenceNumber() const	{ return mSequenceNumber; }
	float				 GetTimeDispatched() const	{ return mTimeDispatched; }
	
	void 				 SetTransmissionData( int inKey, TransmissionDataPtr	inTransmissionData )
	{
		mTransmissionDataMap[ inKey ] = inTransmissionData;
	}
	const TransmissionDataPtr GetTransmissionData( int inKey ) const
	{
		auto it = mTransmissionDataMap.find( inKey );
		return ( it != mTransmissionDataMap.end() ) ? it->second : nullptr;
	}
	
	void			HandleDeliveryFailure( DeliveryNotificationManager* inDeliveryNotificationManager ) const;
	void			HandleDeliverySuccess( DeliveryNotificationManager* inDeliveryNotificationManager ) const;
	
private:
	PacketSequenceNumber	mSequenceNumber;
	float			mTimeDispatched;
	
	unordered_map< int, TransmissionDataPtr >	mTransmissionDataMap;
};