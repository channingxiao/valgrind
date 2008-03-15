/*
  This file is part of drd, a data race detector.

  Copyright (C) 2006-2008 Bart Van Assche
  bart.vanassche@gmail.com

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
  02111-1307, USA.

  The GNU General Public License is contained in the file COPYING.
*/


#ifndef __DRD_BITMAP3_H
#define __DRD_BITMAP3_H


#include "pub_tool_oset.h"


/*
  Bitmap representation. A bitmap is a data structure in which two bits are
  reserved per 32 bit address: one bit that indicates that the data at the
  specified address has been read, and one bit that indicates that the data has
  been written to.
*/


/* Macro definitions. */

#define ADDR0_BITS 16

#define ADDR0_COUNT ((UWord)1 << ADDR0_BITS)

#define ADDR0_MASK (ADDR0_COUNT - 1)

#define SPLIT_ADDRESS(a)            \
  UWord a##0 = ((a) & ADDR0_MASK);  \
  UWord a##1 = ((a) >> ADDR0_BITS);

// Assumption: sizeof(Addr) == sizeof(UWord).
#define MAKE_ADDRESS(a1, a0)  \
  (Addr)(((UWord)(a1) << (ADDR0_BITS)) | ((UWord)(a0)))

#define BITS_PER_UWORD (8UL*sizeof(UWord))
#if defined(VGA_x86) || defined(VGA_ppc32)
#define BITS_PER_BITS_PER_UWORD 5
#elif defined(VGA_amd64) || defined(VGA_ppc64)
#define BITS_PER_BITS_PER_UWORD 6
#else
#error Unknown platform.
#endif

#define BITMAP1_UWORD_COUNT (ADDR0_COUNT >> BITS_PER_BITS_PER_UWORD)

/* Highest bits of an address that fit into the same UWord of bm0[]. */
#define UWORD_MSB(a) ((a) & ~(BITS_PER_UWORD - 1))

/* Lowest bits of an address that fit into the same UWord of bm0[]. */
#define UWORD_LSB(a) ((a) & (BITS_PER_UWORD - 1))

/* Highest address that fits in the same UWord as a. */
#define UWORD_HIGHEST_ADDRESS(a) ((a) | (BITS_PER_UWORD - 1))


// Local constants.

static ULong s_bitmap2_creation_count;


/* Lowest level, corresponding to the lowest ADDR0_BITS of an address. */
struct bitmap1
{
  UWord bm0_r[BITMAP1_UWORD_COUNT];
  UWord bm0_w[BITMAP1_UWORD_COUNT];
};

static __inline__ UWord bm0_mask(const Addr a)
{
  return ((UWord)1 << UWORD_LSB(a));
}

static __inline__ void bm0_set(UWord* bm0, const Addr a)
{
  //tl_assert(a < ADDR0_COUNT);
  bm0[a >> BITS_PER_BITS_PER_UWORD] |= (UWord)1 << UWORD_LSB(a);
}

/** Set all of the addresses in range [ a1 .. a1 + size [ in bitmap bm0. */
static __inline__ void bm0_set_range(UWord* bm0,
                                     const Addr a1, const SizeT size)
{
#if 0
  tl_assert(a1 < ADDR0_COUNT);
  tl_assert(size > 0);
  tl_assert(a1 + size <= ADDR0_COUNT);
  tl_assert(UWORD_MSB(a1) == UWORD_MSB(a1 + size - 1));
#endif
  bm0[a1 >> BITS_PER_BITS_PER_UWORD]
    |= (((UWord)1 << size) - 1) << UWORD_LSB(a1);
}

static __inline__ void bm0_clear(UWord* bm0, const Addr a)
{
  //tl_assert(a < ADDR0_COUNT);
  bm0[a >> BITS_PER_BITS_PER_UWORD] &= ~((UWord)1 << UWORD_LSB(a));
}

static __inline__ UWord bm0_is_set(const UWord* bm0, const Addr a)
{
  //tl_assert(a < ADDR0_COUNT);
  return (bm0[a >> BITS_PER_BITS_PER_UWORD] & ((UWord)1 << UWORD_LSB(a)));
}

/** Return true if any of the bits [ a1 .. a1+size [ are set in bm0. */
static __inline__ UWord bm0_is_any_set(const UWord* bm0,
                                       const Addr a1, const SizeT size)
{
#if 0
  tl_assert(a1 < ADDR0_COUNT);
  tl_assert(size > 0);
  tl_assert(a1 + size <= ADDR0_COUNT);
  tl_assert(UWORD_MSB(a1) == UWORD_MSB(a1 + size - 1));
#endif
  return (bm0[a1 >> BITS_PER_BITS_PER_UWORD]
          & ((((UWord)1 << size) - 1) << UWORD_LSB(a1)));
}

struct bitmap2
{
  Addr           addr; ///< address >> ADDR0_BITS
  struct bitmap1 bm1;
};

/* Complete bitmap. */
struct bitmap
{
  Addr            last_lookup_a1;
  struct bitmap2* last_lookup_result;
  OSet*           oset;
};

static __inline__
struct bitmap2* bm_lookup(const struct bitmap* const bm, const Addr a)
{
  struct bitmap2* result;
  const UWord a1 = a >> ADDR0_BITS;
  if (a1 == bm->last_lookup_a1)
  {
    return bm->last_lookup_result;
  }
  result = VG_(OSetGen_Lookup)(bm->oset,&a1);
  if (result)
  {
    ((struct bitmap*)bm)->last_lookup_a1     = a1;
    ((struct bitmap*)bm)->last_lookup_result = result;
  }
  return result;
}

static __inline__
struct bitmap2* bm2_insert(const struct bitmap* const bm,
                           const UWord a1)
{
  struct bitmap2* const node = VG_(OSetGen_AllocNode)(bm->oset, sizeof(*node));
  node->addr = a1;
  VG_(memset)(&node->bm1, 0, sizeof(node->bm1));
  VG_(OSetGen_Insert)(bm->oset, node);
  
  ((struct bitmap*)bm)->last_lookup_a1     = a1;
  ((struct bitmap*)bm)->last_lookup_result = node;
  
  s_bitmap2_creation_count++;
  
  return node;
}

static __inline__
struct bitmap2* bm2_lookup_or_insert(const struct bitmap* const bm,
                                     const UWord a1)
{
  struct bitmap2* p2;

  if (a1 == bm->last_lookup_a1)
  {
    return bm->last_lookup_result;
  }

  p2 = VG_(OSetGen_Lookup)(bm->oset, &a1);
  if (p2 == 0)
  {
    p2 = bm2_insert(bm, a1);
  }
  ((struct bitmap*)bm)->last_lookup_a1     = a1;
  ((struct bitmap*)bm)->last_lookup_result = p2;
  return p2;
}


#endif /* __DRD_BITMAP3_H */
