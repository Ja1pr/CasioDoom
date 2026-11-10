//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	Preparation of data for rendering.
//
//  CasioDoom: wall textures are composed and shrunk by the WAD converter;
//  the tables of textures, flats and sprite frames are ready-made lumps.
//  Nothing is generated or cached here, all graphics are drawn straight
//  from the WAD image.
//
//-----------------------------------------------------------------------------

#include <string.h>

#include "i_system.h"
#include "z_zone.h"
#include "m_swap.h"
#include "w_wad.h"
#include "doomdef.h"
#include "r_local.h"
#include "p_local.h"
#include "doomstat.h"
#include "r_sky.h"
#include "r_data.h"

int			numtextures;
int			numflats;
int			gfxshift;
const texinfo_t*	texinfo;
const flatinfo_t*	flatinfo;

// for global animation
short*			flattranslation;
short*			texturetranslation;

const lighttable_t*	colormaps;

// One entry cache: columns are nearly always fetched in runs.
static int		lasttex = -1;
static const cwtexture_t* lasttexdata;


void R_ClearTextureCache (void)
{
    lasttex = -1;
}

static const cwtexture_t* R_TextureData (int tex)
{
    if (tex != lasttex)
    {
	lasttexdata = W_CacheLumpNum (texinfo[tex].lump, PU_CACHE);
	lasttex = tex;
    }
    return lasttexdata;
}

//
// R_GetColumn
// Returns a solid column, (128>>gfxshift) texels high.
//
const byte*
R_GetColumn
( int		tex,
  int		col )
{
    const cwtexture_t*	t = R_TextureData (tex);

    col = (col & (texinfo[tex].flags >> 1)) >> gfxshift;
    return (const byte *)(t + 1) + col * (128 >> gfxshift);
}

//
// R_GetMaskedColumn
// Returns the column as posts, or NULL if the texture has no holes
// (use R_GetColumn then).
//
const column_t*
R_GetMaskedColumn
( int		tex,
  int		col )
{
    const cwtexture_t*		t = R_TextureData (tex);
    const unsigned short*	ofs;

    if (!t->postofs)
	return NULL;
    col = (col & (texinfo[tex].flags >> 1)) >> gfxshift;
    ofs = (const unsigned short *)((const byte *)t + t->postofs);
    return (const column_t *)((const byte *)t + ofs[col]);
}


//
// R_InitData
// Locates all the lumps
//  that will be used by all views
// Must be called after W_Init.
//
void R_InitData (void)
{
    int		i;

    gfxshift = cdinfo->gfxshift;
    numtextures = cdinfo->numtextures;
    numflats = cdinfo->numflats;
    numsprites = cdinfo->numsprites;

    texinfo = W_CacheLumpName ("~TEXINFO", PU_STATIC);
    flatinfo = W_CacheLumpName ("~FLTINFO", PU_STATIC);
    sprites = W_CacheLumpName ("~SPRDEFS", PU_STATIC);
    spriteframes = W_CacheLumpName ("~SPRFRAM", PU_STATIC);
    colormaps = W_CacheLumpName ("COLORMAP", PU_STATIC);

    if (numsprites != NUMSPRITES)
	I_Error ("R_InitData: sprite table mismatch");

    // Create translation tables for global animation.
    texturetranslation = Z_Malloc ((numtextures+1)*sizeof(short), PU_STATIC, 0);
    for (i=0 ; i<numtextures ; i++)
	texturetranslation[i] = i;

    flattranslation = Z_Malloc ((numflats+1)*sizeof(short), PU_STATIC, 0);
    for (i=0 ; i<numflats ; i++)
	flattranslation[i] = i;
}


static boolean R_NameEq (const char* a, const char* name)
{
    int		i;
    char	c;

    for (i=0 ; i<8 ; i++)
    {
	c = name[i];
	if (c >= 'a' && c <= 'z')
	    c -= 32;
	if (a[i] != c)
	    return false;
	if (!c)
	    break;
    }
    return true;
}


//
// R_CheckFlatNumForName
// Retrieval, get a flat number for a flat name. -1 if absent.
//
int R_CheckFlatNumForName (const char* name)
{
    int		i;

    for (i=0 ; i<numflats ; i++)
	if (R_NameEq (flatinfo[i].name, name))
	    return i;
    return -1;
}

int R_FlatNumForName (const char* name)
{
    int		i = R_CheckFlatNumForName (name);

    if (i == -1)
	I_Error ("R_FlatNumForName: %s not found",name);
    return i;
}


//
// R_CheckTextureNumForName
// Check whether texture is available.
// Filter out NoTexture indicator.
//
int	R_CheckTextureNumForName (const char *name)
{
    int		i;

    // "NoTexture" marker.
    if (name[0] == '-')		
	return 0;
		
    for (i=1 ; i<numtextures ; i++)
	if (R_NameEq (texinfo[i].name, name))
	    return i;
    
    return -1;
}


//
// R_TextureNumForName
// Calls R_CheckTextureNumForName,
//  aborts with error message.
//
int	R_TextureNumForName (const char* name)
{
    int		i;
	
    i = R_CheckTextureNumForName (name);

    if (i==-1)
    {
	I_Error ("R_TextureNumForName: %s not found",
		 name);
    }
    return i;
}


//
// R_PrecacheLevel
// Nothing to do: graphics are used in place.
//
void R_PrecacheLevel (void)
{
}
