//
//  ByteSwap.h
//  RoboCat
//
//  Created by Joshua Glazer on 4/20/14.
//  Copyright (c) 2014 com.JoshuaGlazer.Book. All rights reserved.
//

#ifndef RoboCat_ByteSwap_h
#define RoboCat_ByteSwap_h

inline uint16_t ByteSwap2( uint16_t inData )
{
	return ( inData >> 8 ) | ( inData << 8 );
}

inline uint32_t ByteSwap4( uint32_t inData )
{
	return  ( ( inData >> 24 ) & 0x000000ff ) |
			( ( inData >>  8 ) & 0x0000ff00 ) |
			( ( inData <<  8 ) & 0x00ff0000 ) |
			( ( inData << 24 ) & 0xff000000 );
}

inline uint64_t ByteSwap8( uint64_t inData )
{
	return  ( ( inData >> 56 ) & 0x00000000000000ff ) |
			( ( inData >> 40 ) & 0x000000000000ff00 ) |
			( ( inData >> 24 ) & 0x0000000000ff0000 ) |
			( ( inData >>  8 ) & 0x00000000ff000000 ) |
			( ( inData <<  8 ) & 0x000000ff00000000 ) |
			( ( inData << 24 ) & 0x0000ff0000000000 ) |
			( ( inData << 40 ) & 0x00ff000000000000 ) |
			( ( inData << 56 ) & 0xff00000000000000 );
}


template < typename tFrom, typename tTo >
class TypeAliaser
{
public:
	TypeAliaser( tFrom inFromValue ) :
		mAsFromType( inFromValue ) {}
	tTo& Get() { return mAsToType; }
	
	union
	{
		tFrom 	mAsFromType;
		tTo		mAsToType;
	};
};


template <typename T, size_t tSize > class ByteSwapper;

//specialize for 1...
template <typename T>
class ByteSwapper< T, 1 >
{
public:
	T Swap( T inData ) const
	{
		return inData;
	}
};


//specialize for 2...
template <typename T>
class ByteSwapper< T, 2 >
{
public:
	T Swap( T inData ) const
	{
		uint16_t result =
			ByteSwap2( TypeAliaser< T, uint16_t >( inData ).Get() );
		return TypeAliaser< uint16_t, T >( result ).Get();
	}
};

//specialize for 4...
template <typename T>
class ByteSwapper< T, 4 >
{
public:
	T Swap( T inData ) const
	{
		uint32_t result =
			ByteSwap4( TypeAliaser< T, uint32_t >( inData ).Get() );
		return TypeAliaser< uint32_t, T >( result ).Get();
	}
};


//specialize for 8...
template <typename T>
class ByteSwapper< T, 8 >
{
public:
	T Swap( T inData ) const
	{
		uint64_t result =
			ByteSwap8( TypeAliaser< T, uint64_t >( inData ).Get() );
		return TypeAliaser< uint64_t, T >( result ).Get();
	}
};

template < typename T >
T ByteSwap( T inData )
{
	return ByteSwapper< T, sizeof( T ) >().Swap( inData );
}

inline void TestByteSwap()
{
	int32_t test = 0x12345678;
	float floatTest = 1.f;
	
	printf( "swapped 0x%x is 0x%x\n", test, ByteSwap( test ) );
	printf( "swapped %f is %f\n", floatTest, ByteSwap( floatTest ) );
	printf( "swapped 0x%x is 0x%x\n", TypeAliaser< float, uint32_t >( floatTest ).Get(), TypeAliaser< float, uint32_t >( ByteSwap( floatTest ) ).Get() );
}

#endif
