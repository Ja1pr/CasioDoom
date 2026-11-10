// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//      Refresh/rendering module, shared data struct definitions.
//
//  CasioDoom: the static part of a level (vertexes, lines, sides, segs,
//  subsectors, nodes, sector templates) is NOT loaded into RAM.  The WAD
//  converter stores it in exactly the layouts below and the structures
//  are used in place, straight from flash.  They therefore contain no
//  pointers, only indices, and everything that changes during play lives
//  in the small RAM-side structures sector_t and linedata_t.
//
//-----------------------------------------------------------------------------

#ifndef __R_DEFS__
#define __R_DEFS__

#include "doomdef.h"
#include "m_fixed.h"
#include "d_think.h"
#include "p_mobj.h"

#define SIL_NONE		0
#define SIL_BOTTOM		1
#define SIL_TOP			2
#define SIL_BOTH		3

#define MAXDRAWSEGS		96

// Clip values (screen rows) fit in a signed byte on a 64 line display.
typedef signed char	clip_t;

//
// ROM: vertex (8 bytes)
//
typedef struct
{
    fixed_t	x;
    fixed_t	y;
} vertex_t;

//
// ROM: sector template (28 bytes).
//
typedef struct
{
    short	floorheight;
    short	ceilingheight;
    short	floorpic;
    short	ceilingpic;
    short	lightlevel;
    short	special;
    short	tag;
    short	linecount;
    int		firstline;	// index into the sector line list
    short	blockbox[4];	// mapblock bounding box for height changes
} mapsec_t;

//
// RAM: the living part of a sector (32 bytes).
//
typedef	struct
{
    fixed_t	floorheight;
    fixed_t	ceilingheight;
    short	floorpic;
    short	ceilingpic;
    short	lightlevel;
    short	special;

    // if == validcount, already checked
    unsigned short validcount;
    // 0 = untraversed, 1,2 = sndlines -1
    unsigned char soundtraversed;
    unsigned char pad;

    // thing that made a sound (or null)
    mobj_t*	soundtarget;
    // list of mobjs in sector
    mobj_t*	thinglist;
    // thinker_t for reversable actions
    void*	specialdata;
} sector_t;

//
// ROM: SideDef (16 bytes)
//
typedef struct
{
    fixed_t	textureoffset;
    fixed_t	rowoffset;
    short	toptexture;
    short	bottomtexture;
    short	midtexture;
    short	sector;
} side_t;

typedef enum
{
    ST_HORIZONTAL,
    ST_VERTICAL,
    ST_POSITIVE,
    ST_NEGATIVE
} slopetype_t;

//
// ROM: LineDef (44 bytes)
//
typedef struct line_s
{
    // Precalculated v2 - v1 for side checking.
    fixed_t	dx;
    fixed_t	dy;
    // Bounding box, for the extent of the LineDef.
    fixed_t	bbox[4];
    // Vertices, from v1 to v2.
    short	v1;
    short	v2;
    short	flags;
    short	special0;	// initial special, see linedata_t
    short	tag;
    // sidenum[1] will be -1 if one sided
    short	sidenum[2];
    // Front and back sector (index, -1 if none)
    short	frontsector;
    short	backsector;
    // To aid move clipping.
    unsigned char slopetype;
    unsigned char pad;
} line_t;

//
// RAM: what changes on a line (4 bytes)
//
typedef struct
{
    unsigned short validcount;
    unsigned char  special;
    unsigned char  mapped;	// seen on the automap
} linedata_t;

//
// ROM: SubSector (8 bytes)
//
typedef struct subsector_s
{
    short		sector;
    unsigned short	numlines;
    unsigned short	firstline;
    short		pad;
} subsector_t;

//
// ROM: LineSeg (20 bytes)
//
typedef struct
{
    short	v1;
    short	v2;
    short	sidedef;
    short	linedef;
    short	frontsector;
    short	backsector;	// -1 for one sided lines
    fixed_t	offset;
    angle_t	angle;
} seg_t;

//
// ROM: BSP node (52 bytes)
//
typedef struct
{
    // Partition line.
    fixed_t	x;
    fixed_t	y;
    fixed_t	dx;
    fixed_t	dy;
    // Bounding box for each child.
    fixed_t	bbox[2][4];
    // If NF_SUBSECTOR its a subsector.
    unsigned short children[2];
} node_t;

// posts are runs of non masked source pixels
typedef struct
{
    byte		topdelta;	// -1 is the last post in a column
    byte		length; 	// length data bytes follows
} post_t;

// column_t is a list of 0 or more post_t, (byte)-1 terminated
typedef post_t	column_t;

// Now a 64 entry grey remapping table.
typedef byte	lighttable_t;	

//
// ?
//
typedef struct drawseg_s
{
    const seg_t*	curline;
    short		x1;
    short		x2;

    fixed_t		scale1;
    fixed_t		scale2;
    fixed_t		scalestep;

    // 0=none, 1=bottom, 2=top, 3=both
    int			silhouette;

    // do not clip sprites above this
    fixed_t		bsilheight;
    // do not clip sprites below this
    fixed_t		tsilheight;
    
    // Pointers to lists for sprite clipping,
    //  all three adjusted so [x1] is first value.
    clip_t*		sprtopclip;		
    clip_t*		sprbottomclip;	
    short*		maskedtexturecol;
} drawseg_t;

//
// Calculator patch: like a DOOM patch, but with 16 bit column offsets.
// For sprites the pixel data is reduced by 2^gfxshift while leftoffset,
// topoffset and worldwidth stay in map units.
//
typedef struct 
{ 
    short		width;		// columns stored
    short		height;		// rows stored
    short		leftoffset;	// pixels to the left of origin 
    short		topoffset;	// pixels below the origin 
    short		worldwidth;	// width in map units (sprites)
    short		pad;
    unsigned short	columnofs[8];	// only [width] used
} patch_t;

#define PATCH_COLUMN(p,c) \
    ((const column_t *)((const byte *)(p) + (p)->columnofs[c]))

//
// Calculator wall texture lump: raw columns (always 128>>gfxshift texels
// high), optionally followed by a post version for see-through textures.
//
typedef struct
{
    unsigned short	width;		// reduced columns
    unsigned short	postofs;	// 0, or offset of ushort[width] table
} cwtexture_t;

// ROM: texture directory entry
typedef struct
{
    char		name[8];
    unsigned short	width;
    unsigned short	height;
    unsigned short	lump;
    unsigned short	flags;
} texinfo_t;

// ROM: flat directory entry
typedef struct
{
    char		name[8];
    unsigned short	lump;
    unsigned short	pad;
} flatinfo_t;

// A vissprite_t is a thing
//  that will be drawn during a refresh.
typedef struct vissprite_s
{
    // Doubly linked list.
    struct vissprite_s*	prev;
    struct vissprite_s*	next;
    
    short		x1;
    short		x2;

    // for line side calculation
    fixed_t		gx;
    fixed_t		gy;		

    // global bottom / top for silhouette clipping
    fixed_t		gz;
    fixed_t		gzt;

    // horizontal position of x1
    fixed_t		startfrac;
    
    fixed_t		scale;
    
    // negative if flipped
    fixed_t		xiscale;	

    fixed_t		texturemid;
    int			patch;

    // for color translation and shadow draw,
    //  maxbright frames as well
    const lighttable_t*	colormap;
   
    int			mobjflags;
    
} vissprite_t;

//	
// ROM: sprite frame (18 bytes) and sprite definition (4 bytes).
//
typedef struct
{
    // If zero, use lump[0] for all views.
    byte		rotate;
    // bit r set: flip rotation r
    byte		flip;
    // Lump to use for view angles 0-7 (0xffff = missing).
    unsigned short	lump[8];
} spriteframe_t;

typedef struct
{
    unsigned short	numframes;
    unsigned short	firstframe;
} spritedef_t;

//
// Now what is a visplane, anyway?
// 
typedef struct
{
  fixed_t		height;
  short			picnum;
  short			lightlevel;
  short			minx;
  short			maxx;
  
  // leave pads for [minx-1]/[maxx+1]
  byte		pad1;
  // Here lies the rub for all
  //  dynamic resize/change of resolution.
  byte		top[SCREENWIDTH];
  byte		pad2;
  byte		pad3;
  // See above.
  byte		bottom[SCREENWIDTH];
  byte		pad4;

} visplane_t;

#endif
