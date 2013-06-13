//-------------------------------------------------------------------------------------
// filters.h
//  
// Utility header with helpers for implementing image filters
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#if defined(_MSC_VER) && (_MSC_VER > 1000)
#pragma once
#endif

#ifdef USE_XNAMATH
#include <xnamath.h>
#else
#include <directxmath.h>
#include <directxpackedvector.h>
#endif

namespace DirectX
{

//-------------------------------------------------------------------------------------
// Box filtering helpers
//-------------------------------------------------------------------------------------

XMGLOBALCONST XMVECTORF32 g_boxScale = { 0.25f, 0.25f, 0.25f, 0.25f };
XMGLOBALCONST XMVECTORF32 g_boxScale3D = { 0.125f, 0.125f, 0.125f, 0.125f };

#define AVERAGE4( res, p0, p1, p2, p3 ) \
{ \
    XMVECTOR v = XMVectorAdd( (p0), (p1) ); \
    v = XMVectorAdd( v, (p2) ); \
    v = XMVectorAdd( v, (p3) ); \
    res = XMVectorMultiply( v, g_boxScale ); \
}

#define AVERAGE8( res, p0, p1, p2, p3, p4, p5, p6, p7) \
{ \
    XMVECTOR v = XMVectorAdd( (p0), (p1) ); \
    v = XMVectorAdd( v, (p2) ); \
    v = XMVectorAdd( v, (p3) ); \
    v = XMVectorAdd( v, (p4) ); \
    v = XMVectorAdd( v, (p5) ); \
    v = XMVectorAdd( v, (p6) ); \
    v = XMVectorAdd( v, (p7) ); \
    res = XMVectorMultiply( v, g_boxScale3D ); \
}


//-------------------------------------------------------------------------------------
// Linear filtering helpers
//-------------------------------------------------------------------------------------

struct LinearFilter
{
    size_t  u0;
    float   weight0;
    size_t  u1;
    float   weight1;
};

inline void _CreateLinearFilter( _In_ size_t source, _In_ size_t dest, _In_ bool wrap, _Out_writes_(dest) LinearFilter* lf )
{
    assert( source > 0 );
    assert( dest > 0 );
    assert( lf != 0 );

    float scale = float(source) / float(dest);

    // Mirror is the same case as clamp for linear

    for( size_t u = 0; u < dest; ++u )
    {
        float srcB = ( float(u) + 0.5f ) * scale + 0.5f;

        ptrdiff_t isrcB = ptrdiff_t(srcB);
        ptrdiff_t isrcA = isrcB - 1;
        
        if ( isrcA < 0 )
        {
            isrcA = ( wrap ) ? ( source - 1) : 0;
        }

        if ( size_t(isrcB) >= source )
        {
            isrcB = ( wrap ) ? 0 : ( source - 1);
        }

        float weight = 1.0f + float(isrcB) - srcB;

        auto& entry = lf[ u ];
        entry.u0 = size_t(isrcA);
        entry.weight0 = weight;

        entry.u1 = size_t(isrcB);
        entry.weight1 = 1.0f - weight;
    }
}

#define BILINEAR_INTERPOLATE( res, x, y, r0, r1 ) \
    res = ( y.weight0 * ( (r0)[ x.u0 ] * x.weight0 + (r0)[ x.u1 ] * x.weight1 ) ) \
          + ( y.weight1 * ( (r1)[ x.u0 ] * x.weight0 + (r1)[ x.u1 ] * x.weight1 ) )

#define TRILINEAR_INTERPOLATE( res, x, y, z, r0, r1, r2, r3 ) \
    res = ( z.weight0 * ( ( y.weight0 * ( (r0)[ x.u0 ] * x.weight0 + (r0)[ x.u1 ] * x.weight1 ) ) \
                          + ( y.weight1 * ( (r1)[ x.u0 ] * x.weight0 + (r1)[ x.u1 ] * x.weight1 ) ) ) ) \
          + ( z.weight1 * ( ( y.weight0 * ( (r2)[ x.u0 ] * x.weight0 + (r2)[ x.u1 ] * x.weight1 ) ) \
                             + ( y.weight1 * ( (r3)[ x.u0 ] * x.weight0 + (r3)[ x.u1 ] * x.weight1 ) ) ) )


//-------------------------------------------------------------------------------------
// Cubic filtering helpers
//-------------------------------------------------------------------------------------

XMGLOBALCONST XMVECTORF32 g_cubicThird = { 1.f/3.f, 1.f/3.f, 1.f/3.f, 1.f/3.f }; 
XMGLOBALCONST XMVECTORF32 g_cubicSixth = { 1.f/6.f, 1.f/6.f, 1.f/6.f, 1.f/6.f }; 
XMGLOBALCONST XMVECTORF32 g_cubicHalf = { 1.f/2.f, 1.f/2.f, 1.f/2.f, 1.f/2.f };

inline ptrdiff_t bounduvw( ptrdiff_t u, ptrdiff_t maxu, bool wrap, bool mirror )
{
    if ( wrap )
    {
        if ( u < 0 )
        {
            u = maxu + u + 1;
        }
        else if ( u > maxu )
        {
            u = u - maxu - 1;
        }
    }
    else if ( mirror )
    {
        if ( u < 0 )
        {
            u = ( -u ) - 1;
        }
        else if ( u > maxu )
        {
            u = maxu - (u - maxu - 1);
        }
    }

    // Handles clamp, but also a safety factor for degenerate images for wrap/mirror
    u = std::min<ptrdiff_t>( u, maxu );
    u = std::max<ptrdiff_t>( u, 0 );

    return u;
}

struct CubicFilter
{
    size_t  u0;
    size_t  u1;
    size_t  u2;
    size_t  u3;
    float   x;
};

inline void _CreateCubicFilter( _In_ size_t source, _In_ size_t dest, _In_ bool wrap, bool mirror, _Out_writes_(dest) CubicFilter* cf )
{
    assert( source > 0 );
    assert( dest > 0 );
    assert( cf != 0 );

    float scale = float(source) / float(dest);

    for( size_t u = 0; u < dest; ++u )
    {
        float srcB = ( float(u) + 0.5f ) * scale - 0.5f;

        ptrdiff_t isrcB = bounduvw( ptrdiff_t(srcB), source - 1, wrap, mirror );
        ptrdiff_t isrcA = bounduvw( isrcB - 1, source - 1, wrap, mirror );
        ptrdiff_t isrcC = bounduvw( isrcB + 1, source - 1, wrap, mirror );
        ptrdiff_t isrcD = bounduvw( isrcB + 2, source - 1, wrap, mirror );

        auto& entry = cf[ u ];
        entry.u0 = size_t(isrcA);
        entry.u1 = size_t(isrcB);
        entry.u2 = size_t(isrcC);
        entry.u3 = size_t(isrcD);

        float x = srcB - float(isrcB);
        entry.x = x;
    }
}

#define CUBIC_INTERPOLATE( res, dx, p0, p1, p2, p3 ) \
{ \
    XMVECTOR a0 = (p1); \
    XMVECTOR d0 = (p0) - a0; \
    XMVECTOR d2 = (p2) - a0; \
    XMVECTOR d3 = (p3) - a0; \
    XMVECTOR a1 = d2 - g_cubicThird*d0 - g_cubicSixth*d3; \
    XMVECTOR a2 = g_cubicHalf*d0 + g_cubicHalf*d2; \
    XMVECTOR a3 = g_cubicSixth*d3 - g_cubicSixth*d0 - g_cubicHalf*d2;  \
    XMVECTOR vdx = XMVectorReplicate( dx ); \
    XMVECTOR vdx2 = vdx * vdx; \
    XMVECTOR vdx3 = vdx2 * vdx; \
    res = a0 + a1*vdx + a2*vdx2 + a3*vdx3; \
}

}; // namespace