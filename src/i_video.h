//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	System specific interface stuff.
//
//-----------------------------------------------------------------------------

#ifndef __I_VIDEO__
#define __I_VIDEO__

#include "doomtype.h"

// Called by D_DoomMain,
// determines the hardware configuration
// and sets up the video mode
void I_InitGraphics (void);

void I_ShutdownGraphics(void);

// Shows a 128x64 one bit per pixel picture: 16 bytes per row,
// most significant bit on the left, 1 = dark pixel.
void I_ShowMono (const byte* bits);

// The grey frame buffer is dithered and shown (v_video.c).
void I_FinishUpdate (void);

// There is no palette on a monochrome LCD: the palette number is turned
// into a brightness flash (v_video.c).
void I_SetPalette (int palette);

#endif
