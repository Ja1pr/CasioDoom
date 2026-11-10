//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//      Zone Memory Allocation, perhaps NeXT ObjectiveC inspired.
//	Remark: this was the only stuff that, according
//	 to John Carmack, might have been useful for
//	 Quake.
//
//  CasioDoom: several separate memory areas can be used together, because
//  the calculator has no single big block of free RAM.
//
//-----------------------------------------------------------------------------

#ifndef __Z_ZONE__
#define __Z_ZONE__

//
// ZONE MEMORY
// PU - purge tags.
// Tags < 100 are not overwritten until freed.
#define PU_STATIC		1	// static entire execution time
#define PU_LEVEL		50	// static until level exited
#define PU_LEVSPEC		51      // a special thinker in a level
// Tags >= 100 are purgable whenever needed.
#define PU_PURGELEVEL	100
#define PU_CACHE		101

void	Z_Init (void);
// Hands a memory area to the allocator (called by Z_Init via I_ZoneAreas).
void	Z_AddArea (void* base, int size);
void*	Z_Malloc (int size, int tag, void *ptr);
// Like Z_Malloc, but returns NULL instead of failing.
void*	Z_TryMalloc (int size, int tag, void *ptr);
void    Z_Free (void *ptr);
void    Z_FreeTags (int lowtag, int hightag);
void    Z_CheckHeap (void);
void    Z_ChangeTag2 (void *ptr, int tag);
int     Z_FreeMemory (void);
int     Z_LargestFree (void);
int     Z_TotalMemory (void);

#define Z_ChangeTag(p,t) Z_ChangeTag2(p,t)

#endif
