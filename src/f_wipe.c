//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	Mission begin melt/wipe screen special effect.
//
//  CasioDoom: the melt works on the 1 bit pictures that go to the LCD, so
//  it needs two 1 KB buffers instead of two full screens.
//
//-----------------------------------------------------------------------------

#include <string.h>

#include "z_zone.h"
#include "i_video.h"
#include "v_video.h"
#include "m_random.h"

#include "doomdef.h"

#include "f_wipe.h"

#define ROWBYTES	(SCREENWIDTH/8)

static byte*		wipe_scr_start;
static byte*		wipe_scr_end;
static signed char*	wipe_y;
static boolean		go;


boolean wipe_StartScreen (void)
{
    byte*	mem;

    mem = Z_TryMalloc (2*ROWBYTES*SCREENHEIGHT + SCREENWIDTH, PU_STATIC, 0);
    if (!mem)
	return false;
    wipe_scr_start = mem;
    wipe_scr_end = mem + ROWBYTES*SCREENHEIGHT;
    wipe_y = (signed char *)(wipe_scr_end + ROWBYTES*SCREENHEIGHT);
    memcpy (wipe_scr_start, monoscreen, ROWBYTES*SCREENHEIGHT);
    go = false;
    return true;
}

void wipe_EndScreen (void)
{
    V_Dither (wipe_scr_end);
}

static void wipe_initMelt (void)
{
    int		i;
    int		r;

    // setup initial column positions
    // (y<0 => not ready to scroll yet)
    wipe_y[0] = -(M_Random()%8);
    for (i=1 ; i<SCREENWIDTH ; i++)
    {
	r = (M_Random()%3) - 1;
	wipe_y[i] = wipe_y[i-1] + r;
	if (wipe_y[i] > 0)
	    wipe_y[i] = 0;
	else if (wipe_y[i] == -8)
	    wipe_y[i] = -7;
    }
}

boolean wipe_ScreenWipe (int ticks)
{
    int		i;
    int		x;
    int		y;
    int		dy;
    int		sy;
    int		mask;
    boolean	done = true;
    byte*	dest;

    // initial stuff
    if (!go)
    {
	go = true;
	wipe_initMelt ();
    }

    while (ticks--)
    {
	for (i=0 ; i<SCREENWIDTH ; i++)
	{
	    if (wipe_y[i] < 0)
	    {
		wipe_y[i]++;
		done = false;
	    }
	    else if (wipe_y[i] < SCREENHEIGHT)
	    {
		dy = (wipe_y[i] < 6) ? wipe_y[i]+1 : 3;
		if (wipe_y[i]+dy >= SCREENHEIGHT)
		    dy = SCREENHEIGHT - wipe_y[i];
		wipe_y[i] += dy;
		done = false;
	    }
	}
    }

    // compose: the new picture above the column's y, the old one,
    // moved down, below it
    dest = monoscreen;
    memcpy (dest, wipe_scr_end, ROWBYTES*SCREENHEIGHT);
    for (x=0 ; x<SCREENWIDTH ; x++)
    {
	sy = wipe_y[x] < 0 ? 0 : wipe_y[x];
	mask = 0x80 >> (x & 7);
	for (y=sy ; y<SCREENHEIGHT ; y++)
	{
	    if (wipe_scr_start[(y-sy)*ROWBYTES + (x>>3)] & mask)
		dest[y*ROWBYTES + (x>>3)] |= mask;
	    else
		dest[y*ROWBYTES + (x>>3)] &= ~mask;
	}
    }
    I_ShowMono (monoscreen);

    // final stuff
    if (done)
    {
	go = false;
	Z_Free (wipe_scr_start);
    }

    return done;
}
