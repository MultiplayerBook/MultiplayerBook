enum ReplicationAction
{
	RA_Create,
	RA_Update,
	RA_Destroy,
	RA_RPC,
	RA_MAX
};


struct ReplicationCommand
{
public:
	
	ReplicationCommand() {}
	ReplicationCommand( uint32_t inInitialDirtyState ) : mAction( RA_Create ), mDirtyState( inInitialDirtyState ) {}
	
	//if the create is ack'd, we can demote to just an update...
	void HandleCreateAckd()							{ if( mAction == RA_Create ) { mAction = RA_Update; } }
	void AddDirtyState( uint32_t inState )			{ mDirtyState |= inState; }
	void SetDestroy()								{ mAction = RA_Destroy; }
	
	bool				HasDirtyState() const	{ return ( mAction == RA_Destroy ) || ( mDirtyState != 0 ); }
	
	void				SetAction( ReplicationAction inAction )		{ mAction = inAction; }
	ReplicationAction	GetAction()	const							{ return mAction; }
	uint32_t			GetDirtyState() const						{ return mDirtyState; }
	inline void			ClearDirtyState( uint32_t inStateToClear );
	
private:
	
	uint32_t				mDirtyState;
	ReplicationAction		mAction;
};

inline void	 ReplicationCommand::ClearDirtyState( uint32_t inStateToClear )
{
	mDirtyState &= ~inStateToClear;
	
	if( mAction == RA_Destroy )
	{
		mAction = RA_Update;
	}
}
