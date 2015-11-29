class WindowManager
{

public:
	
	static bool StaticInit();
	static std::unique_ptr< WindowManager >	sInstance;

	SDL_Window*		GetMainWindow()	const	{ return mMainWindow; }

	~WindowManager();
private:
	WindowManager( SDL_Window* inMainWindow );

	SDL_Window*				mMainWindow;
};