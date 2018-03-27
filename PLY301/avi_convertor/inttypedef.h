#ifndef _INTTYPEDEF_H_
#define _INTTYPEDEF_H_

typedef unsigned char	uint8_t;
typedef unsigned short	uint16_t;
typedef unsigned int	uint32_t;
typedef unsigned long long  uint64_t;
typedef signed char		int8_t;
typedef signed short	int16_t;
typedef signed int		int32_t;
typedef signed long long	int64_t;

#   define AV_RL16(x)                           \
    ((((const uint8_t*)(x))[1] << 8) |          \
      ((const uint8_t*)(x))[0])
#   define AV_WL16(p, d) do {                   \
        ((uint8_t*)(p))[0] = (d);               \
        ((uint8_t*)(p))[1] = (d)>>8;            \
    } while(0)

#   define AV_RB32(x)                           \
    ((((const uint8_t*)(x))[0] << 24) |         \
     (((const uint8_t*)(x))[1] << 16) |         \
     (((const uint8_t*)(x))[2] <<  8) |         \
      ((const uint8_t*)(x))[3])
#   define AV_RL32(x)                           \
    ((((const uint8_t*)(x))[3] << 24) |         \
     (((const uint8_t*)(x))[2] << 16) |         \
     (((const uint8_t*)(x))[1] <<  8) |         \
      ((const uint8_t*)(x))[0])

static inline UINT16 GetWLE( void const * _p )
{
    UINT8 * p = (UINT8 *)_p;
    return ( ((UINT16)p[1] << 8) | p[0] );
}
static inline UINT32 GetDWLE( void const * _p )
{
    UINT8 * p = (UINT8 *)_p;
    return ( ((UINT32)p[3] << 24) | ((UINT32)p[2] << 16)
              | ((UINT32)p[1] << 8) | p[0] );
}
static inline UINT64 GetQWLE( void const * _p )
{
    UINT8 * p = (UINT8 *)_p;
    return ( ((UINT64)p[7] << 56) | ((UINT64)p[6] << 48)
              | ((UINT64)p[5] << 40) | ((UINT64)p[4] << 32)
              | ((UINT64)p[3] << 24) | ((UINT64)p[2] << 16)
              | ((UINT64)p[1] << 8) | p[0] );
}

#define SetWLE( p, v ) _SetWLE( (UINT8*)p, v)
static inline void _SetWLE( UINT8 *p, UINT16 i_dw )
{
    p[1] = ( i_dw >>  8 )&0xff;
    p[0] = ( i_dw       )&0xff;
}

#define SetDWLE( p, v ) _SetDWLE( (UINT8*)p, v)
static inline void _SetDWLE( UINT8 *p, UINT32 i_dw )
{
    p[3] = ( i_dw >> 24 )&0xff;
    p[2] = ( i_dw >> 16 )&0xff;
    p[1] = ( i_dw >>  8 )&0xff;
    p[0] = ( i_dw       )&0xff;
}

static inline UINT16 U16_AT( void const * _p )
{
    UINT8 * p = (UINT8 *)_p;
    return ( ((UINT16)p[0] << 8) | p[1] );
}
static inline UINT32 U32_AT( void const * _p )
{
    UINT8 * p = (UINT8 *)_p;
    return ( ((UINT32)p[0] << 24) | ((UINT32)p[1] << 16)
              | ((UINT32)p[2] << 8) | p[3] );
}
static inline UINT64 U64_AT( void const * _p )
{
    UINT8 * p = (UINT8 *)_p;
    return ( ((UINT64)p[0] << 56) | ((UINT64)p[1] << 48)
              | ((UINT64)p[2] << 40) | ((UINT64)p[3] << 32)
              | ((UINT64)p[4] << 24) | ((UINT64)p[5] << 16)
              | ((UINT64)p[6] << 8) | p[7] );
}

#define GetWBE( p )     U16_AT( p )
#define GetDWBE( p )    U32_AT( p )
#define GetQWBE( p )    U64_AT( p )

#endif
