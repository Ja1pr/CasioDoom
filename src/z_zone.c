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
 
// CasioDoom: 16 byte block header. The size (a multiple of 4, below
// 16 MB) and the tag share one word.
typedef struct memblock_s
{
    unsigned int	sizetag;	// size << 8 | tag
    void**		user;	// NULL if a free block
    struct memblock_s*	next;
    struct memblock_s*	prev;
} memblock_t;

#define BSIZE(b)	((int)((b)->sizetag >> 8))
#define BTAG(b)		((int)((b)->sizetag & 0xff))
#define BSET(b,size,tag) ((b)->sizetag = ((unsigned int)(size) << 8) | (tag))

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
    BSET (&zone->blocklist, 0, PU_STATIC);
    zone->rover = block;
	
    block->prev = block->next = &zone->blocklist;

    // NULL indicates a free block.
    block->user = NULL;
    BSET (block, zone->size - sizeof(memzone_t), 0);

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

    if (!block->user)
	I_Error ("Z_Free: block is not allocated");
		
    if (block->user > (void **)0x100)
    {
	// smaller values are not pointers
	// Note: OS-dependend?
	
	// clear the user's mark
	*block->user = 0;
    }

    // mark as free
    block->user = NULL;	
    BSET (block, BSIZE (block), 0);
	
    other = block->prev;

    if (!other->user)
    {
	// merge with previous free block
	BSET (other, BSIZE (other) + BSIZE (block), 0);
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
	BSET (block, BSIZE (block) + BSIZE (other), 0);
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
	    if (BTAG (rover) < PU_PURGELEVEL)
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
    } while (base->user || BSIZE (base) < size);

    
    // found a block big enough
    extra = BSIZE (base) - size;
    
    if (extra >  MINFRAGMENT)
    {
	// there will be a free fragment after the allocated block
	newblock = (memblock_t *) ((byte *)base + size );
	BSET (newblock, extra, 0);
	
	// NULL indicates free block.
	newblock->user = NULL;	
	newblock->prev = base;
	newblock->next = base->next;
	newblock->next->prev = newblock;

	base->next = newblock;
	BSET (base, size, 0);
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
    BSET (base, BSIZE (base), tag);

    // next allocation will start looking here
    mainzone->rover = base->next;	
    
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
	
	if (BTAG (block) >= lowtag && BTAG (block) <= hightag)
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
	
	if ( (byte *)block + BSIZE (block) != (byte *)block->next)
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

    if (!block->user)
	I_Error ("Z_ChangeTag: block is not allocated");

    if (tag >= PU_PURGELEVEL && block->user < (void **)0x100)
	I_Error ("Z_ChangeTag: an owner is required for purgable blocks");

    BSET (block, BSIZE (block), tag);
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
	if (!block->user || BTAG (block) >= PU_PURGELEVEL)
	    free += BSIZE (block);
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
	if (!block->user && BSIZE (block) > best)
	    best = BSIZE (block);
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
