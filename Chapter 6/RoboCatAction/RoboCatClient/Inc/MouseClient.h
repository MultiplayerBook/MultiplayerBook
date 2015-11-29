class MouseClient : public Mouse
{
public:
	static	GameObjectPtr	StaticCreate()		{ return GameObjectPtr( new MouseClient() ); }

protected:
	MouseClient();

private:

	SpriteComponentPtr	mSpriteComponent;
};