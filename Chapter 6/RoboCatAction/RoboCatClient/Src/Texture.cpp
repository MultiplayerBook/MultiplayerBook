#include <RoboCatClientPCH.h>

Texture::Texture( uint32_t inWidth, uint32_t inHeight, SDL_Texture* inTexture ) :
	mWidth( inWidth ),
	mHeight( inHeight ),
	mTexture( inTexture )
{
}

Texture::~Texture()
{
	SDL_DestroyTexture( mTexture );
}