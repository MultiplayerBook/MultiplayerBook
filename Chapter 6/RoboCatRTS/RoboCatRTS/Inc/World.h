

/*
* the world tracks all the live game objects. Failry inefficient for now, but not that much of a problem
*/

//zoom hardcoded at 100
const float kWorldZoomFactor = 100.0f;
const float kWorldWidth = 12.8f;
const float kWorldHeight = 7.2f;

class World
{

public:
	static void StaticInit();

	static std::unique_ptr< World >		sInstance;

	void AddGameObject( GameObjectPtr inGameObject );
	void RemoveGameObject(GameObjectPtr inGameObject);

	void Update( float inDeltaTime );

	const std::vector< GameObjectPtr >&	GetGameObjects()	const	{ return mGameObjects; }

	uint32_t TrySelectGameObject( const Vector3& inSelectLoc );

private:


	World();

	std::vector< GameObjectPtr >	mGameObjects;
};