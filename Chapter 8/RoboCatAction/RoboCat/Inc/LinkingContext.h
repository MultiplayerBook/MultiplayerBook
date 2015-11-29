

#ifndef RoboCat_LinkingContext_h
#define RoboCat_LinkingContext_h

class GameObject;

class LinkingContext
{
public:
	
	LinkingContext() : 
	mNextNetworkId( 1 )
	{}

	uint32_t GetNetworkId( GameObject* inGameObject, bool inShouldCreateIfNotFound )
	{
		auto it = mGameObjectToNetworkIdMap.find( inGameObject );
		if( it != mGameObjectToNetworkIdMap.end() )
		{
			return it->second;
		}
		else if( inShouldCreateIfNotFound )
		{
			uint32_t newNetworkId = mNextNetworkId++;
			AddGameObject( inGameObject, newNetworkId );
			return newNetworkId;
		}
		else
		{
			return 0;
		}
	}
	
	GameObject* GetGameObject( uint32_t inNetworkId ) const
	{
		auto it = mNetworkIdToGameObjectMap.find( inNetworkId );
		if( it != mNetworkIdToGameObjectMap.end() )
		{
			return it->second;
		}
		else
		{
			return nullptr;
		}
	}

	void AddGameObject( GameObject* inGameObject, uint32_t inNetworkId )
	{
		mNetworkIdToGameObjectMap[ inNetworkId ] = inGameObject;
		mGameObjectToNetworkIdMap[ inGameObject ] = inNetworkId;
	}
	
	void RemoveGameObject( GameObject *inGameObject )
	{
		uint32_t networkId = mGameObjectToNetworkIdMap[ inGameObject ];
		mGameObjectToNetworkIdMap.erase( inGameObject );
		mNetworkIdToGameObjectMap.erase( networkId );
	}
	
private:
	std::unordered_map< uint32_t, GameObject* > mNetworkIdToGameObjectMap;
	std::unordered_map< const GameObject*, uint32_t > mGameObjectToNetworkIdMap;

	uint32_t mNextNetworkId;
};

#endif
