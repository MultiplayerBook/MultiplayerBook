
#define CLASS_IDENTIFICATION( inCode, inClass ) \
enum { kClassId = inCode }; \
virtual uint32_t GetClassId() const { return kClassId; } \
static GameObject* CreateInstance() { return static_cast< GameObject* >( new inClass() ); } \

class GameObject
{
public:

	CLASS_IDENTIFICATION( 'GOBJ', GameObject )

	GameObject();
	virtual ~GameObject();

	virtual	RoboCat*	GetAsCat()	{ return nullptr; }

	//return whether to keep processing collision
	virtual bool	HandleCollisionWithCat( RoboCat* inCat ) { ( void ) inCat; return true; }

	virtual void	Update( float inDeltaTime );

	virtual void	HandleDying();

			void	SetIndexInWorld( int inIndex )						{ mIndexInWorld = inIndex; }
			int		GetIndexInWorld()				const				{ return mIndexInWorld; }

			void	SetRotation( float inRotation );
			float	GetRotation()					const				{ return mRotation; }

			void	SetScale( float inScale )							{ mScale = inScale; }
			float	GetScale()						const				{ return mScale; }


	const Vector3&		GetLocation()				const				{ return mLocation; }
			void		SetLocation( const Vector3& inLocation )		{ mLocation = inLocation; }

			float		GetCollisionRadius()		const				{ return mCollisionRadius; }
			void		SetCollisionRadius( float inRadius )			{ mCollisionRadius = inRadius; }

			Vector3		GetForwardVector()			const;


			void		SetColor( const Vector3& inColor )					{ mColor = inColor; }
	const Vector3&		GetColor()					const				{ return mColor; }

			bool		DoesWantToDie()				const				{ return mDoesWantToDie; }
			void		SetDoesWantToDie( bool inWants )				{ mDoesWantToDie = inWants; }

			uint32_t	GetNetworkId()				const				{ return mNetworkId; }
			void		SetNetworkId( uint32_t inNetworkId );

			void		SetPlayerId( uint32_t inPlayerId )			{ mPlayerId = inPlayerId; }
			uint32_t	GetPlayerId()						const 	{ return mPlayerId; }

	//no default implementation because we don't know what's relevant for each object necessarily
	virtual void		WriteForCRC( OutputMemoryBitStream& inStream )	{ ( void )inStream; }
	virtual bool		TrySelect( const Vector3& inLocation );
protected:


	Vector3											mLocation;
	Vector3											mColor;
	
	float											mCollisionRadius;


	float											mRotation;
	float											mScale;
	int												mIndexInWorld;

	uint32_t										mNetworkId;
	uint32_t										mPlayerId;

	bool											mDoesWantToDie;
	bool											mSelected;
};

typedef shared_ptr< GameObject >	GameObjectPtr;