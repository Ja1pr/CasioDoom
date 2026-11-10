//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	Functions to draw patches (by post) directly to screen,
//	and the conversion of the grey frame buffer to the 1 bit LCD.
//
//-----------------------------------------------------------------------------

#include <string.h>

#include "i_system.h"
#include "i_video.h"
#include "r_local.h"

#include "doomdef.h"
#include "doomdata.h"

#include "m_swap.h"
#include "z_zone.h"

#include "v_video.h"


byte*		screens[1];
byte*		monoscreen;

int		usegamma = 2;

// 8x8 ordered dither matrix, scaled to 0..62 so that grey 0 is always
// black and grey 63 is always white.
static const byte bayer[64] =
{
     0, 31,  7, 39,  1, 33,  9, 41,
    47, 15, 55, 23, 49, 17, 57, 25,
    11, 43,  3, 35, 13, 45,  5, 37,
    59, 27, 51, 19, 61, 29, 53, 21,
     2, 34, 10, 42,  0, 32,  8, 40,
    50, 18, 58, 26, 48, 16, 56, 24,
    14, 46,  6, 38, 12, 44,  4, 36,
    62, 30, 54, 22, 60, 28, 52, 20
};

// Grey remapping applied when dithering: brightness setting plus the
// damage / bonus flash that replaces DOOM's palette tricks.
static byte	greylut[64];
static int	lutgamma = -1;
static int	lutpalette = -1;
static int	curpalette;


#define BLACKPOINT	3

static void V_BuildLut (void)
{
    int		i;
    int		g;
    int		flash;
    int		hi;
    static const byte whitepoint[5] = { 58, 50, 43, 36, 30 };

    // palettes 1-8: pain (red), 9-12: bonus, 13: radiation suit
    if (curpalette >= 1 && curpalette <= 8)
	flash = -5 * curpalette;
    else if (curpalette >= 9 && curpalette <= 12)
	flash = 4 * (curpalette - 8);
    else if (curpalette == 13)
	flash = 6;
    else
	flash = 0;

    // DOOM is a dark game and a 1 bit LCD has no shades to spare, so the
    // grey range that actually occurs is stretched to full contrast.
    // The brightness setting moves the white point.
    hi = whitepoint[usegamma % 5];
    for (i=0 ; i<64 ; i++)
    {
	g = (i - BLACKPOINT) * 63 / (hi - BLACKPOINT);
	if (g < 0)
	    g = 0;
	if (g > 63)
	    g = 63;
	// lift the midtones
	g += g * (63 - g) / 126;
	g += flash;
	if (flash < 0 && g < 0)
	    g = i / 4;
	if (g < 0)
	    g = 0;
	if (g > 63)
	    g = 63;
	greylut[i] = g;
    }
    lutgamma = usegamma;
    lutpalette = curpalette;
}


void I_SetPalette (int palette)
{
    curpalette = palette;
}


//
// V_Dither
//
void V_Dither (byte* dest)
{
    const byte*		src = screens[0];
    const byte*		thr;
    int			x;
    int			y;
    int			b;
    unsigned		bits;

    if (lutgamma != usegamma || lutpalette != curpalette)
	V_BuildLut ();

    for (y=0 ; y<SCREENHEIGHT ; y++)
    {
	thr = &bayer[(y & 7) << 3];
	for (x=0 ; x<SCREENWIDTH ; x+=8)
	{
	    bits = 0;
	    for (b=0 ; b<8 ; b++)
		bits = (bits << 1) | (greylut[src[b] & 63] <= thr[b]);
	    *dest++ = bits;
	    src += 8;
	}
    }
}


//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{
    V_Dither (monoscreen);
    I_ShowMono (monoscreen);
}


//
// V_DrawPatch
// Masks a column based masked pic to the screen. 
//
static void
V_DrawPatchCol
( int		x,
  int		y,
  const patch_t*	patch,
  int		col )
{
    const column_t*	column;
    const byte*		source;
    byte*		dest;
    int			count;
    int			top;

    if ((unsigned)x >= SCREENWIDTH)
	return;

    column = PATCH_COLUMN (patch, col);

    // step through the posts in a column 
    while (column->topdelta != 0xff ) 
    { 
	source = (const byte *)column + 3; 
	top = y + column->topdelta;
	count = column->length; 
	if (top < 0)
	{
	    count += top;
	    source -= top;
	    top = 0;
	}
	if (top + count > SCREENHEIGHT)
	    count = SCREENHEIGHT - top;
	dest = screens[0] + top*SCREENWIDTH + x;
	while (count-- > 0) 
	{ 
	    *dest = *source++; 
	    dest += SCREENWIDTH; 
	} 
	column = (const column_t *)(  (const byte *)column + column->length 
				    + 4 ); 
    } 
}

void
V_DrawPatch
( int		x,
  int		y,
  int		scrn,
  const patch_t*	patch ) 
{ 
    int		col; 
    int		w; 
	 
    (void)scrn;
    y -= patch->topoffset; 
    x -= patch->leftoffset; 

    w = patch->width; 
    for (col=0 ; col<w ; col++)
	V_DrawPatchCol (x+col, y, patch, col);
} 
 
//
// V_DrawPatchFlipped 
// Masks a column based masked pic to the screen.
// Flips horizontally, e.g. to mirror face.
//
void
V_DrawPatchFlipped
( int		x,
  int		y,
  int		scrn,
  const patch_t*	patch ) 
{ 
    int		col; 
    int		w; 
	 
    (void)scrn;
    y -= patch->topoffset; 
    x -= patch->leftoffset; 

    w = patch->width; 
    for (col=0 ; col<w ; col++)
	V_DrawPatchCol (x+col, y, patch, w-1-col);
} 


//
// V_DrawSprite
// Draws a sprite patch (whose offsets are in map units while its
// pixels are reduced by 2^gfxshift) at its reduced size.
//
void
V_DrawSprite
( int		x,
  int		y,
  const patch_t*	patch,
  boolean	flipped ) 
{ 
    int		col; 
    int		w = patch->width; 
	 
    y -= patch->topoffset >> gfxshift; 
    if (flipped)
	x -= (patch->worldwidth - patch->leftoffset) >> gfxshift;
    else
	x -= patch->leftoffset >> gfxshift; 

    for (col=0 ; col<w ; col++)
	V_DrawPatchCol (x+col, y, patch, flipped ? w-1-col : col);
} 


//
// V_FillRect
//
void
V_FillRect
( int		x,
  int		y,
  int		w,
  int		h,
  int		c )
{
    byte*	dest;

    if (x < 0)
    {
	w += x;
	x = 0;
    }
    if (y < 0)
    {
	h += y;
	y = 0;
    }
    if (x + w > SCREENWIDTH)
	w = SCREENWIDTH - x;
    if (y + h > SCREENHEIGHT)
	h = SCREENHEIGHT - y;
    if (w <= 0)
	return;
    dest = screens[0] + y*SCREENWIDTH + x;
    for ( ; h > 0 ; h--)
    {
	memset (dest, c, w);
	dest += SCREENWIDTH;
    }
}


//
// V_Init
// 
void V_Init (void) 
{ 
    screens[0] = Z_Malloc (SCREENWIDTH*SCREENHEIGHT, PU_STATIC, 0);
    memset (screens[0], V_WHITE, SCREENWIDTH*SCREENHEIGHT);
    monoscreen = Z_Malloc (SCREENWIDTH*SCREENHEIGHT/8, PU_STATIC, 0);
    memset (monoscreen, 0, SCREENWIDTH*SCREENHEIGHT/8);
}
