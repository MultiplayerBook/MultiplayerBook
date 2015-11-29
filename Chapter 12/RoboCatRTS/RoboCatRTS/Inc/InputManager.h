
class InputManager
{
public:


	static void StaticInit();
	static unique_ptr< InputManager >	sInstance;

	void HandleInput( EInputAction inInputAction, int inKeyCode );
	void HandleMouseClick( int32_t inX, int32_t inY, uint8_t button );

	CommandList&	GetCommandList()		{ return mCommandList; }
	void			ClearCommandList()		{ mCommandList.Clear(); }
	
	void			Update();
	uint32_t		GetSelectedNetId()	{ return mSelectedNetId; }

private:
	void			GenerateRightClickCommand( const Vector3& inWorldPos );
	InputManager();

	
	CommandList		mCommandList;
	uint32_t		mSelectedNetId;
};