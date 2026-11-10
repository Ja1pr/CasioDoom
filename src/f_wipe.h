//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	Mission start screen wipe/melt, special effects.
//	
//-----------------------------------------------------------------------------

#ifndef __F_WIPE_H__
#define __F_WIPE_H__

#include "doomtype.h"

// Remembers the frame on the LCD. Returns false if there is no memory
// for a wipe (it is skipped then).
boolean wipe_StartScreen (void);

// Takes the freshly drawn frame buffer as the picture to melt into.
void wipe_EndScreen (void);

// Advances the melt and shows it. Returns true when finished.
boolean wipe_ScreenWipe (int ticks);

#endif
