//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	Calculator WAD ("CDWD") access.
//
//  The file is never loaded.  The platform layer tells us where its
//  pieces lie in memory (on the calculator: in the flash chip), and lumps
//  are used in place.  Only a lump that happens to straddle two
//  non-adjacent pieces of the file is copied to the zone heap.
//
//-----------------------------------------------------------------------------

#ifndef __W_WAD__
#define __W_WAD__

#include "doomtype.h"

#define CWAD_VERSION	3

typedef struct
{
    char		magic[4];	// "CDWD"
    unsigned int	endian;		// 0x01020304 in native byte order
    unsigned int	numlumps;
    unsigned int	dirofs;
} wadheader_t;

typedef struct
{
    char		name[8];
    unsigned int	filepos;
    unsigned int	size;
} lumpinfo_t;

// Lump "~CDINFO"
typedef struct
{
    unsigned short	version;
    unsigned char	gamemode;
    unsigned char	gfxshift;
    unsigned short	numtextures;
    unsigned short	numflats;
    unsigned short	numsprites;
    unsigned short	numspriteframes;
    unsigned short	nummaps;
    unsigned short	skyflat;
} cdinfo_t;

// One contiguous piece of the WAD file, as seen in memory.
typedef struct
{
    const byte*		base;
    unsigned int	ofs;	// offset in the file
    unsigned int	len;
} wadfrag_t;

extern	int		numlumps;
extern	const cdinfo_t*	cdinfo;

// frags must be sorted by offset and cover the whole file.
void    W_Init (const wadfrag_t* frags, int numfrags);

int	W_CheckNumForName (const char* name);
int	W_GetNumForName (const char* name);

int	W_LumpLength (int lump);
void    W_ReadLump (int lump, void *dest);

const void*	W_CacheLumpNum (int lump, int tag);
const void*	W_CacheLumpName (const char* name, int tag);

// Frees the heap copy of a lump, if it had to be copied.
void	W_ReleaseLump (const void* ptr);

#endif
