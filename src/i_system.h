//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	System specific interface stuff.
//
//-----------------------------------------------------------------------------

#ifndef __I_SYSTEM__
#define __I_SYSTEM__

#include "d_ticcmd.h"
#include "d_event.h"
#include "w_wad.h"

// Called by DoomMain.
void I_Init (void);

// Called by Z_Init: gives every usable memory area to Z_AddArea().
void I_ZoneAreas (void);

// Locates the WAD file in memory. Returns the fragment list.
const wadfrag_t* I_MapWad (int* count);

// Called by D_DoomLoop,
// returns current time in tics.
int I_GetTime (void);

//
// Called by D_DoomLoop,
// called before processing any tics in a frame
// (just after displaying a frame).
// Time consuming syncronous operations
// are performed here (joystick reading).
// Can call D_PostEvent.
//
void I_StartFrame (void);

//
// Called by D_DoomLoop,
// called before processing each tic in a frame.
// Quick syncronous operations are performed here.
// Can call D_PostEvent.
void I_StartTic (void);

// Called by M_Responder when quit is selected.
// Clean exit, displays sell blurb.
void I_Quit (void);

void I_Error (const char *error, ...);

//
// Save files. Only one file can be open at a time.
// I_FileOpen returns false on failure.
//
boolean	I_FileOpen (const char* name, boolean write);
int	I_FileRead (void* buf, int len);
int	I_FileWrite (const void* buf, int len);
void	I_FileClose (void);

#endif
