//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	Handles calculator WAD file directory and lump access.
//
//-----------------------------------------------------------------------------

#include <string.h>

#include "doomtype.h"
#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"

int			numlumps;
const cdinfo_t*		cdinfo;

static const wadfrag_t*	frags;
static int		numfrags;
static unsigned int	dirofs;

// Lumps that had to be copied because they cross a gap in the file.
#define MAXCOPIES	24
static struct
{
    void*	ptr;
    short	lump;
} copies[MAXCOPIES];


// Finds the fragment holding a file offset.
static const wadfrag_t* W_Frag (unsigned int ofs)
{
    int		lo = 0;
    int		hi = numfrags - 1;
    int		mid;

    while (lo < hi)
    {
	mid = (lo + hi + 1) >> 1;
	if (frags[mid].ofs <= ofs)
	    lo = mid;
	else
	    hi = mid - 1;
    }
    return &frags[lo];
}

// Pointer to [len] contiguous bytes at a file offset, or NULL.
static const byte* W_Ptr (unsigned int ofs, unsigned int len)
{
    const wadfrag_t*	f = W_Frag (ofs);

    if (ofs < f->ofs || ofs + len > f->ofs + f->len)
	return NULL;
    return f->base + (ofs - f->ofs);
}

static void W_Copy (unsigned int ofs, byte* dest, unsigned int len)
{
    const wadfrag_t*	f;
    unsigned int	n;

    while (len)
    {
	f = W_Frag (ofs);
	if (ofs < f->ofs || ofs >= f->ofs + f->len)
	    I_Error ("W_Copy: hole in WAD at %u", ofs);
	n = f->ofs + f->len - ofs;
	if (n > len)
	    n = len;
	memcpy (dest, f->base + (ofs - f->ofs), n);
	dest += n;
	ofs += n;
	len -= n;
    }
}

// Directory entries are 16 bytes at 16-aligned offsets, so they never
// straddle a fragment boundary.
static const lumpinfo_t* W_Info (int lump)
{
    const byte*	p = W_Ptr (dirofs + 16 * (unsigned)lump, 16);

    if (!p)
	I_Error ("W_Info: bad directory");
    return (const lumpinfo_t *)p;
}


void W_Init (const wadfrag_t* f, int n)
{
    const wadheader_t*	h;
    int			i;

    frags = f;
    numfrags = n;

    h = (const wadheader_t *)W_Ptr (0, sizeof(*h));
    if (!h || memcmp (h->magic, "CDWD", 4))
	I_Error ("DOOM.CWD is not a calculator WAD.\n"
		 "Convert it with wad2calc.py");
    if (h->endian != 0x01020304)
	I_Error ("DOOM.CWD has the wrong byte order.\n"
		 "Run wad2calc.py again.");
    numlumps = h->numlumps;
    dirofs = h->dirofs;

    for (i=0 ; i<MAXCOPIES ; i++)
	copies[i].ptr = NULL;

    cdinfo = W_CacheLumpName ("~CDINFO", PU_STATIC);
    if (cdinfo->version != CWAD_VERSION)
	I_Error ("DOOM.CWD: version %d, need %d.\n"
		 "Run wad2calc.py again.", cdinfo->version, CWAD_VERSION);
}


//
// W_CheckNumForName
// Returns -1 if name not found.  The directory is sorted: binary search.
//
int W_CheckNumForName (const char* name)
{
    char	key[8];
    int		i;
    int		lo, hi, mid, c;

    for (i=0 ; i<8 && name[i] ; i++)
    {
	key[i] = name[i];
	if (key[i] >= 'a' && key[i] <= 'z')
	    key[i] -= 32;
    }
    for ( ; i<8 ; i++)
	key[i] = 0;

    lo = 0;
    hi = numlumps - 1;
    while (lo <= hi)
    {
	mid = (lo + hi) >> 1;
	c = memcmp (key, W_Info(mid)->name, 8);
	if (!c)
	    return mid;
	if (c < 0)
	    hi = mid - 1;
	else
	    lo = mid + 1;
    }
    return -1;
}


//
// W_GetNumForName
// Calls W_CheckNumForName, but bombs out if not found.
//
int W_GetNumForName (const char* name)
{
    int	i;

    i = W_CheckNumForName (name);
    
    if (i == -1)
      I_Error ("W_GetNumForName: %s not found!", name);
      
    return i;
}


int W_LumpLength (int lump)
{
    if ((unsigned)lump >= (unsigned)numlumps)
	I_Error ("W_LumpLength: %i >= numlumps",lump);

    return W_Info(lump)->size;
}


void W_ReadLump (int lump, void* dest)
{
    const lumpinfo_t*	l;

    if ((unsigned)lump >= (unsigned)numlumps)
	I_Error ("W_ReadLump: %i >= numlumps",lump);
    l = W_Info (lump);
    W_Copy (l->filepos, dest, l->size);
}


//
// W_CacheLumpNum
// Returns a pointer straight into the WAD image whenever possible.
//
const void* W_CacheLumpNum (int lump, int tag)
{
    const lumpinfo_t*	l;
    const byte*		p;
    int			i;
    int			slot;

    if ((unsigned)lump >= (unsigned)numlumps)
	I_Error ("W_CacheLumpNum: %i >= numlumps",lump);

    l = W_Info (lump);
    p = W_Ptr (l->filepos, l->size);
    if (p)
	return p;

    // The lump is split between two places: keep a copy on the heap.
    slot = -1;
    for (i=0 ; i<MAXCOPIES ; i++)
    {
	if (copies[i].ptr && copies[i].lump == lump)
	{
	    Z_ChangeTag (copies[i].ptr, tag);
	    return copies[i].ptr;
	}
	if (!copies[i].ptr)
	    slot = i;
    }
    if (slot < 0)
	I_Error ("Storage memory is too fragmented.\n"
		 "Optimize it and copy DOOM.CWD again.");
    copies[slot].lump = lump;
    Z_Malloc (l->size, tag, &copies[slot].ptr);
    W_Copy (l->filepos, copies[slot].ptr, l->size);
    return copies[slot].ptr;
}


const void* W_CacheLumpName (const char* name, int tag)
{
    return W_CacheLumpNum (W_GetNumForName(name), tag);
}
