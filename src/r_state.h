// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
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
//	Refresh/render internal state variables (global).
//
//-----------------------------------------------------------------------------


#ifndef __R_STATE__
#define __R_STATE__

// Need data structure definitions.
#include "d_player.h"
#include "r_data.h"






//
// Refresh internal data structures,
//  for rendering.
//

// needed for texture pegging
extern const texinfo_t*	texinfo;
extern const flatinfo_t*	flatinfo;
extern int		numtextures;
extern int		numflats;
extern int		gfxshift;
#define textureheight(t)	((fixed_t)texinfo[t].height << FRACBITS)

// needed for pre rendering (fracs)





extern const lighttable_t*	colormaps;

extern int		viewwidth;
extern int		scaledviewwidth;
extern int		viewheight;

extern int		firstflat;

// for global animation
extern short*		flattranslation;	
extern short*		texturetranslation;	


// Sprite....
extern int		firstspritelump;
extern int		lastspritelump;
extern int		numspritelumps;



//
// Lookup tables for map data.
//
extern int		numsprites;
extern const spritedef_t*	sprites;
extern const spriteframe_t*	spriteframes;

extern int		numvertexes;
extern const vertex_t*	vertexes;

extern int		numsegs;
extern const seg_t*	segs;

extern int		numsectors;
extern sector_t*	sectors;
extern const mapsec_t*	mapsecs;
extern const unsigned short* seclines;

extern int		numsubsectors;
extern const subsector_t*	subsectors;

extern int		numnodes;
extern const node_t*	nodes;

extern int		numlines;
extern const line_t*	lines;
extern linedata_t*	linedata;

extern int		numsides;
extern const side_t*	sides;
extern byte*		sideswitched;


//
// POV data.
//
extern fixed_t		viewx;
extern fixed_t		viewy;
extern fixed_t		viewz;

extern angle_t		viewangle;
extern player_t*	viewplayer;


// ?
extern angle_t		clipangle;

extern const unsigned char viewangletox[FINEANGLES/2];
extern const angle_t	xtoviewangle[SCREENWIDTH+1];
//extern fixed_t		finetangent[FINEANGLES/2];

extern fixed_t		rw_distance;
extern angle_t		rw_normalangle;



// angle to line origin
extern int		rw_angle1;

// Segs count?
extern int		sscount;

extern visplane_t*	floorplane;
extern visplane_t*	ceilingplane;



//
// Accessors for the pointer-free level structures.
//
#define LN_V1(l)	(&vertexes[(l)->v1])
#define LN_V2(l)	(&vertexes[(l)->v2])
#define LN_FRONT(l)	((l)->frontsector < 0 ? (sector_t *)0 : &sectors[(l)->frontsector])
#define LN_BACK(l)	((l)->backsector < 0 ? (sector_t *)0 : &sectors[(l)->backsector])
#define LN_DATA(l)	(linedata[(l) - lines])
#define LN_SPECIAL(l)	(LN_DATA(l).special)
#define SG_V1(s)	(&vertexes[(s)->v1])
#define SG_V2(s)	(&vertexes[(s)->v2])
#define SG_FRONT(s)	(&sectors[(s)->frontsector])
#define SG_BACK(s)	((s)->backsector < 0 ? (sector_t *)0 : &sectors[(s)->backsector])
#define SG_SIDE(s)	(&sides[(s)->sidedef])
#define SG_LINE(s)	(&lines[(s)->linedef])
#define SS_SECTOR(ss)	(&sectors[(ss)->sector])
#define SD_SECTOR(sd)	(&sectors[(sd)->sector])
#define SEC_ROM(sec)	(&mapsecs[(sec) - sectors])
#define SEC_TAG(sec)	(SEC_ROM(sec)->tag)
#define SEC_LINECOUNT(sec) (SEC_ROM(sec)->linecount)
#define SEC_LINE(sec,i)	(&lines[seclines[SEC_ROM(sec)->firstline + (i)]])

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
