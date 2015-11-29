#include <RoboCatPCH.h>


std::unique_ptr< GameObjectRegistry >	GameObjectRegistry::sInstance;

void GameObjectRegistry::StaticInit()
{
	sInstance.reset( new GameObjectRegistry() );
}

GameObjectRegistry::GameObjectRegistry()
{
}

void GameObjectRegistry::RegisterCreationFunction( uint32_t inFourCCName, GameObjectCreationFunc inCreationFunction )
{
	mNameToGameObjectCreationFunctionMap[ inFourCCName ] = inCreationFunction;
}

GameObjectPtr GameObjectRegistry::CreateGameObject( uint32_t inFourCCName )
{
	//no error checking- if the name isn't there, exception!
	GameObjectCreationFunc creationFunc = mNameToGameObjectCreationFunctionMap[ inFourCCName ];

	GameObjectPtr gameObject = creationFunc();

	//should the registry depend on the world? this might be a little weird
	//perhaps you should ask the world to spawn things? for now it will be like this
	World::sInstance->AddGameObject( gameObject );

	return gameObject;
}