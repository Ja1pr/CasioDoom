//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	Gamma correction LUT.
//	Functions to draw patches (by post) directly to screen.
//	Functions to blit a block to the screen.
//
//  CasioDoom: one 128x64 frame buffer holding 64 grey levels, which is
//  ordered-dithered to the 1 bit LCD when shown.
//
//-----------------------------------------------------------------------------

#ifndef __V_VIDEO__
#define __V_VIDEO__

#include "doomtype.h"
#include "doomdef.h"

// Needed because we are refering to patches.
#include "r_data.h"

//
// VIDEO
//

#define CENTERY			(SCREENHEIGHT/2)

#define V_BLACK			0
#define V_WHITE			63

// Only screen 0 exists.
extern	byte*		screens[1];

// The last frame that was shown, 1 bit per pixel.
extern	byte*		monoscreen;

// Brightness setting, 0-4.
extern	int	usegamma;

// Allocates buffer screens, call before R_Init.
void V_Init (void);

// Draws a patch, clipped to the screen. The scrn argument is ignored.
void
V_DrawPatch
( int		x,
  int		y,
  int		scrn,
  const patch_t*	patch);

#define V_DrawPatchDirect V_DrawPatch

void
V_DrawPatchFlipped
( int		x,
  int		y,
  int		scrn,
  const patch_t*	patch );

void
V_DrawSprite
( int		x,
  int		y,
  const patch_t*	patch,
  boolean	flipped );

void
V_FillRect
( int		x,
  int		y,
  int		w,
  int		h,
  int		c );

// Converts the grey frame buffer to 1 bit per pixel (1024 bytes).
void V_Dither (byte* dest);

#define V_MarkRect(x,y,w,h)	((void)0)

#endif
