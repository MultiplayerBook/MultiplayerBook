class Yarn : public GameObject
{
public:

	CLASS_IDENTIFICATION( 'YARN', GameObject )

	static	GameObjectPtr StaticCreate() { return NetworkManager::sInstance->RegisterAndReturn( new Yarn() ); }

	void			SetVelocity( const Vector3& inVelocity )	{ mVelocity = inVelocity; }
	const Vector3&	GetVelocity() const					{ return mVelocity; }

	void		InitFromShooter( GameObjectPtr inShooter, GameObjectPtr inTarget );

	virtual void Update( float inDeltaTime ) override;

	Yarn();

protected:
	SpriteComponentPtr	mSpriteComponent;
	Vector3		mVelocity;
	GameObjectPtr mShooterCat;
	GameObjectPtr mTargetCat;

	float		mLifeSpan;
};

typedef shared_ptr< Yarn >	YarnPtr;