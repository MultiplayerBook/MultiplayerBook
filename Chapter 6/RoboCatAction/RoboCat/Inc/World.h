

/*
* the world tracks all the live game objects. Failry inefficient for now, but not that much of a problem
*/
class World
{

public:

	static void StaticInit();

	static std::unique_ptr< World >		sInstance;

	void AddGameObject( GameObjectPtr inGameObject );
	void RemoveGameObject( GameObjectPtr inGameObject );

	void Update();

	const std::vector< GameObjectPtr >&	GetGameObjects()	const	{ return mGameObjects; }

private:


	World();

	int	GetIndexOfGameObject( GameObjectPtr inGameObject );

	std::vector< GameObjectPtr >	mGameObjects;


};