/* -----------------------------------------------------------------------------
 *
 * Copyright (c) 2014-2019 Alexis Naveros.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * -----------------------------------------------------------------------------
 */


#if !defined(CPUCONF_LONG_BITSHIFT) || !defined(CPUCONF_LONG_BITS)
 #error Preprocessor symbols CPUCONF_LONG_BITSHIFT and CPUCONF_LONG_BITS are undefined!
 #error This header requires cpuconfig.h
#endif

#if !defined(MM_ATOMIC_SUPPORT)
 #warning Compiling mmbitmap without atomic support, it is going to be SLOW.
 #warning This header requires mm.h 
#endif


typedef struct
{
  size_t entrycount;
  size_t mapsize;
#ifdef MM_ATOMIC_SUPPORT
  mmAtomicL *map;
#else
  long *map;
  mtMutex mutex;
#endif
} mmBitMap;

int mmBitMapInit( mmBitMap *bitmap, size_t entrycount, int initvalue );
void mmBitMapReset( mmBitMap *bitmap, int resetvalue );
void mmBitMapResetRange( mmBitMap *bitmap, int minimumentrycount, int resetvalue );
void mmBitMapFree( mmBitMap *bitmap );

int mmBitMapFindSet( mmBitMap *bitmap, size_t entryindex, size_t entryindexlast, size_t *retentryindex );
int mmBitMapFindClear( mmBitMap *bitmap, size_t entryindex, size_t entryindexlast, size_t *retentryindex );


////


/* No atomic locking, single-threaded access */

static inline int mmBitMapDirectGet( mmBitMap *bitmap, size_t entryindex )
{
  int value;
  size_t index, shift;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  value = ( MM_ATOMIC_ACCESS_L( &bitmap->map[index] ) >> shift ) & 0x1;
#else
  value = ( bitmap->map[index] >> shift ) & 0x1;
#endif
  return value;
}

static inline void mmBitMapDirectSet( mmBitMap *bitmap, size_t entryindex )
{
  size_t index, shift;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  MM_ATOMIC_ACCESS_L( &bitmap->map[index] ) |= (long)1 << shift;
#else
  bitmap->map[index] |= (long)1 << shift;
#endif
  return;
}

static inline void mmBitMapDirectClear( mmBitMap *bitmap, size_t entryindex )
{
  size_t index, shift;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  MM_ATOMIC_ACCESS_L( &bitmap->map[index] ) &= ~( (long)1 << shift );
#else
  bitmap->map[index] &= ~( (long)1 << shift );
#endif
  return;
}

static inline int mmBitMapDirectMaskGet( mmBitMap *bitmap, size_t entryindex, long mask )
{
  int value;
  size_t index, shift;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  value = ( MM_ATOMIC_ACCESS_L( &bitmap->map[index] ) >> shift ) & mask;
#else
  value = ( bitmap->map[index] >> shift ) & mask;
#endif
  return value;
}

static inline void mmBitMapDirectMaskSet( mmBitMap *bitmap, size_t entryindex, long value, long mask )
{
  size_t index, shift;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  MM_ATOMIC_ACCESS_L( &bitmap->map[index] ) = ( MM_ATOMIC_ACCESS_L( &bitmap->map[index] ) & ~( mask << shift ) ) | ( value << shift );
#else
  bitmap->map[index] = ( bitmap->map[index] & ~( mask << shift ) ) | ( value << shift );
#endif
  return;
}


////


/* Atomic locking, multi-threaded access */

static inline int mmBitMapGet( mmBitMap *bitmap, size_t entryindex )
{
  int value;
  size_t index, shift;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  value = ( mmAtomicReadL( &bitmap->map[index] ) >> shift ) & 0x1;
#else
  mtMutexLock( &bitmap->mutex );
  value = ( bitmap->map[index] >> shift ) & 0x1;
  mtMutexUnlock( &bitmap->mutex );
#endif
  return value;
}

static inline void mmBitMapSet( mmBitMap *bitmap, size_t entryindex )
{
  size_t index, shift;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
 #ifdef BP_BITMAP_PREWRITE_CHECK
  if( !( mmAtomicReadL( &bitmap->map[index] ) & ( (long)1 << shift ) ) )
    mmAtomicOrL( &bitmap->map[index], (long)1 << shift );
 #else
  mmAtomicOrL( &bitmap->map[index], (long)1 << shift );
 #endif
#else
  mtMutexLock( &bitmap->mutex );
  bitmap->map[index] |= (long)1 << shift;
  mtMutexUnlock( &bitmap->mutex );
#endif
  return;
}

static inline void mmBitMapClear( mmBitMap *bitmap, size_t entryindex )
{
  size_t index, shift;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
 #ifdef BP_BITMAP_PREWRITE_CHECK
  if( mmAtomicReadL( &bitmap->map[index] ) & ( (long)1 << shift ) )
    mmAtomicAndL( &bitmap->map[index], ~( (long)1 << shift ) );
 #else
  mmAtomicAndL( &bitmap->map[index], ~( (long)1 << shift ) );
 #endif
#else
  mtMutexLock( &bitmap->mutex );
  bitmap->map[index] &= ~( (long)1 << shift );
  mtMutexUnlock( &bitmap->mutex );
#endif
  return;
}

static inline int mmBitMapMaskGet( mmBitMap *bitmap, size_t entryindex, long mask )
{
  int value;
  size_t index, shift;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  value = ( mmAtomicReadL( &bitmap->map[index] ) >> shift ) & mask;
#else
  mtMutexLock( &bitmap->mutex );
  value = ( bitmap->map[index] >> shift ) & mask;
  mtMutexUnlock( &bitmap->mutex );
#endif
  return value;
}

static inline void mmBitMapMaskSet( mmBitMap *bitmap, size_t entryindex, long value, long mask )
{
  size_t index, shift;
  long oldvalue, newvalue;
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shift = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  for( ; ; )
  {
    oldvalue = mmAtomicReadL( &bitmap->map[index] );
    newvalue = ( oldvalue & ~( mask << shift ) ) | ( value << shift );
    if( mmAtomicCmpReplaceL( &bitmap->map[index], oldvalue, newvalue ) )
      break;
  }
#else
  mtMutexLock( &bitmap->mutex );
  bitmap->map[index] = ( bitmap->map[index] & ~( mask << shift ) ) | ( value << shift );
  mtMutexUnlock( &bitmap->mutex );
#endif
  return;
}


