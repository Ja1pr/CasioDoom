//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	The actual span/column drawing functions.
//	Here find the main potential for optimization,
//	 e.g. inline assembly, different algorithms.
//
//  CasioDoom: the view always starts at the top left corner of the
//  128 pixel wide frame buffer, textures are reduced by 2^gfxshift.
//
//-----------------------------------------------------------------------------

#include "doomdef.h"

#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"

#include "r_local.h"

// Needs access to LFB (guess what).
#include "v_video.h"

// State.
#include "doomstat.h"


//
// All drawing to the view buffer is accomplished in this file.
// The other refresh files only know about ccordinates,
//  not the architecture of the frame buffer.
//

int		viewwidth;
int		scaledviewwidth;
int		viewheight;
int		viewwindowx;
int		viewwindowy; 


//
// R_DrawColumn
// Source is the top of the column to scale.
//
const lighttable_t*	dc_colormap; 
int			dc_x; 
int			dc_yl; 
int			dc_yh; 
fixed_t			dc_iscale; 
fixed_t			dc_texturemid;

// first pixel in a column (possibly virtual) 
const byte*		dc_source;		


//
// A column is a vertical slice/span from a wall texture that,
//  given the DOOM style restrictions on the view orientation,
//  will always have constant z depth.
// Thus a special case loop for very fast rendering can
//  be used. It has also been used with Wolfenstein 3D.
// 
void R_DrawColumn (void) 
{ 
    int			count; 
    byte*		dest; 
    fixed_t		frac;
    fixed_t		fracstep;	 
    int			mask;
    const byte*		source = dc_source;
    const lighttable_t*	colormap = dc_colormap;
 
    count = dc_yh - dc_yl; 

    // Zero length, column does not exceed a pixel.
    if (count < 0) 
	return; 
				 
    if ((unsigned)dc_x >= SCREENWIDTH
	|| dc_yl < 0
	|| dc_yh >= SCREENHEIGHT) 
	return;

    dest = screens[0] + dc_yl*SCREENWIDTH + dc_x;  

    // Determine scaling,
    //  which is the only mapping to be done.
    fracstep = dc_iscale >> gfxshift; 
    frac = (dc_texturemid + (dc_yl-centery)*dc_iscale) >> gfxshift; 
    mask = (128 >> gfxshift) - 1;

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    do 
    {
	// Re-map color indices from wall texture column
	//  using a lighting/special effects LUT.
	*dest = colormap[source[(frac>>FRACBITS)&mask]];
	
	dest += SCREENWIDTH; 
	frac += fracstep;
	
    } while (count--); 
} 


//
// Spectre/Invisibility.
//
#define FUZZTABLE		50 

static const signed char fuzzoffset[FUZZTABLE] =
{
    1,-1,1,-1,1,1,-1,
    1,1,-1,1,1,1,-1,
    1,1,1,-1,-1,-1,-1,
    1,-1,-1,1,1,1,1,-1,
    1,-1,1,1,-1,-1,1,
    1,-1,-1,-1,-1,1,1,
    1,1,-1,1,1,-1,1 
}; 

static int	fuzzpos = 0; 


//
// Framebuffer postprocessing.
// Creates a fuzzy image by copying pixels
//  from adjacent ones to left and right.
// Used with an all black colormap, this
//  could create the SHADOW effect,
//  i.e. spectres and invisible players.
//
void R_DrawFuzzColumn (void) 
{ 
    int			count; 
    byte*		dest; 

    // Adjust borders. Low... 
    if (!dc_yl) 
	dc_yl = 1;

    // .. and high.
    if (dc_yh >= viewheight-1) 
	dc_yh = viewheight - 2; 
		 
    count = dc_yh - dc_yl; 

    // Zero length.
    if (count < 0) 
	return; 

    if ((unsigned)dc_x >= SCREENWIDTH
	|| dc_yl < 0 || dc_yh >= SCREENHEIGHT)
	return;

    dest = screens[0] + dc_yl*SCREENWIDTH + dc_x;

    // Looks like an attempt at dithering,
    //  using the colormap #6 (of 0-31, a bit
    //  brighter than average).
    do 
    {
	// Lookup framebuffer, and retrieve
	//  a pixel that is either one column
	//  left or right of the current one.
	// Add index from colormap to index.
	*dest = colormaps[6*COLORMAPSIZE
			  +dest[fuzzoffset[fuzzpos]*SCREENWIDTH]]; 

	// Clamp table lookup index.
	if (++fuzzpos == FUZZTABLE) 
	    fuzzpos = 0;
	
	dest += SCREENWIDTH;
    } while (count--); 
} 
 

//
// R_DrawSpan 
// With DOOM style restrictions on view orientation,
//  the floors and ceilings consist of horizontal slices
//  or spans with constant z depth.
// However, rotation around the world z axis is possible,
//  thus this mapping, while simpler and faster than
//  perspective correct texture mapping, has to traverse
//  the texture at an angle in all but a few cases.
// In consequence, flats are not stored by column (like walls),
//  and the inner loop has to step in texture space u and v.
//
int			ds_y; 
int			ds_x1; 
int			ds_x2;

const lighttable_t*	ds_colormap; 

fixed_t			ds_xfrac; 
fixed_t			ds_yfrac; 
fixed_t			ds_xstep; 
fixed_t			ds_ystep;

// start of a (64>>gfxshift) square tile image 
const byte*		ds_source;	


//
// Draws the actual span.
void R_DrawSpan (void) 
{ 
    fixed_t		xfrac;
    fixed_t		yfrac; 
    byte*		dest; 
    int			count;
    int			spot; 
    int			bits = 6 - gfxshift;
    int			mask = (1 << bits) - 1;
    int			shift = FRACBITS + gfxshift;
    const byte*		source = ds_source;
    const lighttable_t*	colormap = ds_colormap;
	 
    if (ds_x2 < ds_x1
	|| ds_x1<0
	|| ds_x2>=SCREENWIDTH  
	|| (unsigned)ds_y>=SCREENHEIGHT)
	return;
    
    xfrac = ds_xfrac; 
    yfrac = ds_yfrac; 
	 
    dest = screens[0] + ds_y*SCREENWIDTH + ds_x1;

    // We do not check for zero spans here?
    count = ds_x2 - ds_x1; 

    do 
    {
	// Current texture index in u,v.
	spot = (((yfrac>>shift)&mask)<<bits) + ((xfrac>>shift)&mask);

	// Lookup pixel from flat texture tile,
	//  re-index using light/colormap.
	*dest++ = colormap[source[spot]];

	// Next step in u,v.
	xfrac += ds_xstep; 
	yfrac += ds_ystep;
	
    } while (count--); 
} 
