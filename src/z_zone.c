//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	Zone Memory Allocation. Neat.
//
//-----------------------------------------------------------------------------

#include <string.h>

#include "z_zone.h"
#include "i_system.h"
#include "doomdef.h"

//
// ZONE MEMORY ALLOCATION
//
// There is never any space between memblocks,
//  and there will never be two contiguous free memblocks.
// The rover can be left pointing at a non-empty block.
//
// It is of no value to free a cachable block,
//  because it will get overwritten automatically if needed.
// 
 
#define ZONEID	0x1d4a

typedef struct memblock_s
{
    int			size;	// including the header and possibly tiny fragments
    void**		user;	// NULL if a free block
    struct memblock_s*	next;
    struct memblock_s*	prev;
    short		tag;	// purgelevel
    short		id;	// should be ZONEID
} memblock_t;

typedef struct
{
    // total bytes malloced, including header
    int		size;

    // start / end cap for linked list
    memblock_t	blocklist;
    
    memblock_t*	rover;
    
} memzone_t;

#define MAXZONES	4
#define MINFRAGMENT	32

static memzone_t*	zones[MAXZONES];
static int		numzones;


void Z_AddArea (void* base, int size)
{
    memblock_t*	block;
    memzone_t*	zone;

    if (numzones == MAXZONES || size < 1024)
	return;

    zone = (memzone_t *)base;
    zone->size = size & ~3;

    // set the entire zone to one free block
    zone->blocklist.next =
	zone->blocklist.prev =
	block = (memblock_t *)( (byte *)zone + sizeof(memzone_t) );

    zone->blocklist.user = (void *)zone;
    zone->blocklist.tag = PU_STATIC;
    zone->rover = block;
	
    block->prev = block->next = &zone->blocklist;

    // NULL indicates a free block.
    block->user = NULL;
    block->size = zone->size - sizeof(memzone_t);

    zones[numzones++] = zone;
}


//
// Z_Init
//
void Z_Init (void)
{
    numzones = 0;
    I_ZoneAreas ();
    if (!numzones)
	I_Error ("Z_Init: no memory");
}


//
// Z_Free
//
void Z_Free (void* ptr)
{
    memblock_t*		block;
    memblock_t*		other;
    int			i;
	
    block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));

    if (block->id != ZONEID)
	I_Error ("Z_Free: freed a pointer without ZONEID");
		
    if (block->user > (void **)0x100)
    {
	// smaller values are not pointers
	// Note: OS-dependend?
	
	// clear the user's mark
	*block->user = 0;
    }

    // mark as free
    block->user = NULL;	
    block->tag = 0;
    block->id = 0;
	
    other = block->prev;

    if (!other->user)
    {
	// merge with previous free block
	other->size += block->size;
	other->next = block->next;
	other->next->prev = other;

	for (i=0 ; i<numzones ; i++)
	    if (block == zones[i]->rover)
		zones[i]->rover = other;

	block = other;
    }
	
    other = block->next;
    if (!other->user)
    {
	// merge the next free block onto the end
	block->size += other->size;
	block->next = other->next;
	block->next->prev = block;

	for (i=0 ; i<numzones ; i++)
	    if (other == zones[i]->rover)
		zones[i]->rover = block;
    }
}


//
// Tries to allocate in one zone.
//
static void* Z_ZoneMalloc (memzone_t* mainzone, int size, int tag, void* user)
{
    int		extra;
    memblock_t*	start;
    memblock_t* rover;
    memblock_t* newblock;
    memblock_t*	base;

    // scan through the block list,
    // looking for the first free block
    // of sufficient size,
    // throwing out any purgable blocks along the way.

    // if there is a free block behind the rover,
    //  back up over them
    base = mainzone->rover;
    
    if (!base->prev->user)
	base = base->prev;
	
    rover = base;
    start = base->prev;
	
    do
    {
	if (rover == start)
	{
	    // scanned all the way around the list
	    return NULL;
	}
	
	if (rover->user)
	{
	    if (rover->tag < PU_PURGELEVEL)
	    {
		// hit a block that can't be purged,
		//  so move base past it
		base = rover = rover->next;
	    }
	    else
	    {
		// free the rover block (adding the size to base)

		// the rover can be the base block
		base = base->prev;
		Z_Free ((byte *)rover+sizeof(memblock_t));
		base = base->next;
		rover = base->next;
	    }
	}
	else
	    rover = rover->next;
    } while (base->user || base->size < size);

    
    // found a block big enough
    extra = base->size - size;
    
    if (extra >  MINFRAGMENT)
    {
	// there will be a free fragment after the allocated block
	newblock = (memblock_t *) ((byte *)base + size );
	newblock->size = extra;
	
	// NULL indicates free block.
	newblock->user = NULL;	
	newblock->tag = 0;
	newblock->id = 0;
	newblock->prev = base;
	newblock->next = base->next;
	newblock->next->prev = newblock;

	base->next = newblock;
	base->size = size;
    }
	
    if (user)
    {
	// mark as an in use block
	base->user = user;			
	*(void **)user = (void *) ((byte *)base + sizeof(memblock_t));
    }
    else
    {
	if (tag >= PU_PURGELEVEL)
	    I_Error ("Z_Malloc: an owner is required for purgable blocks");

	// mark as in use, but unowned	
	base->user = (void *)2;		
    }
    base->tag = tag;

    // next allocation will start looking here
    mainzone->rover = base->next;	
	
    base->id = ZONEID;
    
    return (void *) ((byte *)base + sizeof(memblock_t));
}


//
// Z_Malloc
// You can pass a NULL user if the tag is < PU_PURGELEVEL.
//
void* Z_TryMalloc (int size, int tag, void* user)
{
    int		i;
    void*	p;

    size = (size + 3) & ~3;
    
    // account for size of block header
    size += sizeof(memblock_t);

    // Level data goes to the first area that has room; everything
    // else prefers the last one.  That keeps long-lived blocks apart.
    if (tag >= PU_LEVEL)
    {
	for (i=0 ; i<numzones ; i++)
	    if ( (p = Z_ZoneMalloc (zones[i], size, tag, user)) )
		return p;
    }
    else
    {
	for (i=numzones-1 ; i>=0 ; i--)
	    if ( (p = Z_ZoneMalloc (zones[i], size, tag, user)) )
		return p;
    }
    return NULL;
}

void* Z_Malloc (int size, int tag, void* user)
{
    void*	p = Z_TryMalloc (size, tag, user);

    if (!p)
	I_Error ("Out of memory!\n(%i bytes needed, %i free)",
		 size, Z_FreeMemory ());
    return p;
}


//
// Z_FreeTags
//
void Z_FreeTags (int lowtag, int hightag)
{
    memblock_t*	block;
    memblock_t*	next;
    int		i;
	
    for (i=0 ; i<numzones ; i++)
    for (block = zones[i]->blocklist.next ;
	 block != &zones[i]->blocklist ;
	 block = next)
    {
	// get link before freeing
	next = block->next;

	// free block?
	if (!block->user)
	    continue;
	
	if (block->tag >= lowtag && block->tag <= hightag)
	    Z_Free ( (byte *)block+sizeof(memblock_t));
    }
}


//
// Z_CheckHeap
//
void Z_CheckHeap (void)
{
    memblock_t*	block;
    int		i;
	
    for (i=0 ; i<numzones ; i++)
    for (block = zones[i]->blocklist.next ; ; block = block->next)
    {
	if (block->next == &zones[i]->blocklist)
	{
	    // all blocks have been hit
	    break;
	}
	
	if ( (byte *)block + block->size != (byte *)block->next)
	    I_Error ("Z_CheckHeap: block size does not touch the next block\n");

	if ( block->next->prev != block)
	    I_Error ("Z_CheckHeap: next block doesn't have proper back link\n");

	if (!block->user && !block->next->user)
	    I_Error ("Z_CheckHeap: two consecutive free blocks\n");
    }
}


//
// Z_ChangeTag
//
void Z_ChangeTag2 (void* ptr, int tag)
{
    memblock_t*	block;
	
    block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));

    if (block->id != ZONEID)
	I_Error ("Z_ChangeTag: freed a pointer without ZONEID");

    if (tag >= PU_PURGELEVEL && block->user < (void **)0x100)
	I_Error ("Z_ChangeTag: an owner is required for purgable blocks");

    block->tag = tag;
}


//
// Z_FreeMemory: free + purgable bytes.
//
int Z_FreeMemory (void)
{
    memblock_t*		block;
    int			free;
    int			i;
	
    free = 0;
    
    for (i=0 ; i<numzones ; i++)
    for (block = zones[i]->blocklist.next ;
	 block != &zones[i]->blocklist;
	 block = block->next)
    {
	if (!block->user || block->tag >= PU_PURGELEVEL)
	    free += block->size;
    }
    return free;
}

int Z_LargestFree (void)
{
    memblock_t*		block;
    int			best = 0;
    int			i;

    for (i=0 ; i<numzones ; i++)
    for (block = zones[i]->blocklist.next ;
	 block != &zones[i]->blocklist;
	 block = block->next)
    {
	if (!block->user && block->size > best)
	    best = block->size;
    }
    return best - (int)sizeof(memblock_t);
}

int Z_TotalMemory (void)
{
    int		i;
    int		total = 0;

    for (i=0 ; i<numzones ; i++)
	total += zones[i]->size;
    return total;
}
