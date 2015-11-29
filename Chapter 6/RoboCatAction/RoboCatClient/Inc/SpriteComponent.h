class SpriteComponent
{
public:

	SpriteComponent( GameObject* inGameObject );
	~SpriteComponent();

	virtual void		Draw( const SDL_Rect& inViewTransform );

			void		SetTexture( TexturePtr inTexture )			{ mTexture = inTexture; }

			Vector3		GetOrigin()					const			{ return mOrigin; }
			void		SetOrigin( const Vector3& inOrigin )		{ mOrigin = inOrigin; }


private:

	Vector3											mOrigin;

	TexturePtr										mTexture;

	//don't want circular reference...
	GameObject*										mGameObject;
};

typedef shared_ptr< SpriteComponent >	SpriteComponentPtr;