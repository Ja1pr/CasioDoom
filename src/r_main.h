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
//	System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __R_MAIN__
#define __R_MAIN__

#include "d_player.h"
#include "r_data.h"




//
// POV related.
//
extern fixed_t		viewcos;
extern fixed_t		viewsin;

extern int		viewwidth;
extern int		viewheight;
extern int		viewwindowx;
extern int		viewwindowy;



extern int		centerx;
extern int		centery;

extern fixed_t		centerxfrac;
extern fixed_t		centeryfrac;
extern fixed_t		projection;

extern unsigned short	validcount;
void R_NextValidCount (void);

extern int		linecount;
extern int		loopcount;


//
// Lighting LUT.
// Used for z-depth cuing per column/row,
//  and other lighting effects (sector ambient, flash).
//

// Lighting constants.
// Now why not 32 levels here?
#define LIGHTLEVELS	        16
#define LIGHTSEGSHIFT	         4

#define MAXLIGHTSCALE		48
#define LIGHTSCALESHIFT		12
#define MAXLIGHTZ	       128
#define LIGHTZSHIFT		20

#define NUMCOLORMAPS		32

// Number of bytes in one colormap (64 grey levels).
#define COLORMAPSIZE		64

// Distance light tables are replaced by small formulas:
// "startmap" is the colormap of a light level at distance 0.
#define R_StartMap(lightnum)	((LIGHTLEVELS-1-(lightnum))*2*NUMCOLORMAPS/LIGHTLEVELS)

extern const unsigned char zlighttab[MAXLIGHTZ];
extern const fixed_t	distscale[SCREENWIDTH];

static inline const lighttable_t* R_ColormapLevel (int level)
{
    extern const lighttable_t* colormaps;
    if (level < 0)
	level = 0;
    if (level >= NUMCOLORMAPS)
	level = NUMCOLORMAPS-1;
    return colormaps + level*COLORMAPSIZE;
}

// was scalelight[lightnum][index]
#define R_ScaleLight(startmap,index)     R_ColormapLevel ((startmap) - (index)*BASE_WIDTH/SCREENWIDTH/2)
// was zlight[lightnum][index]
#define R_ZLight(startmap,index)     R_ColormapLevel ((startmap) - zlighttab[index])

extern int		extralight;
extern const lighttable_t*	fixedcolormap;


// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.


// Blocky/low detail mode.
//B remove this?
//  0 = high, 1 = low
#define detailshift 0


//
// Function pointers to switch refresh/drawing functions.
// Used to select shadow mode etc.
//
extern void		(*colfunc) (void);
extern void		(*basecolfunc) (void);
extern void		(*fuzzcolfunc) (void);
// No shadow effects on floors.
extern void		(*spanfunc) (void);


//
// Utility functions.
int
R_PointOnSide
( fixed_t	x,
  fixed_t	y,
  const node_t*	node );

int
R_PointOnSegSide
( fixed_t	x,
  fixed_t	y,
  const seg_t*	line );

angle_t
R_PointToAngle
( fixed_t	x,
  fixed_t	y );

angle_t
R_PointToAngle2
( fixed_t	x1,
  fixed_t	y1,
  fixed_t	x2,
  fixed_t	y2 );

fixed_t
R_PointToDist
( fixed_t	x,
  fixed_t	y );


fixed_t R_ScaleFromGlobalAngle (angle_t visangle);

const subsector_t*
R_PointInSubsector
( fixed_t	x,
  fixed_t	y );

void
R_AddPointToBox
( int		x,
  int		y,
  fixed_t*	box );



//
// REFRESH - the actual rendering functions.
//

// Called by G_Drawer.
void R_RenderPlayerView (player_t *player);

// Called by startup code.
void R_Init (void);

// Called by M_Responder.
void R_SetViewSize (int blocks, int detail);

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
