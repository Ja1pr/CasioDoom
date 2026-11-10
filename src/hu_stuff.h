//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:  Head up display
//
//  CasioDoom: one tiny font (3x5 glyphs in 4x6 cells) is used for all
//  text in the game: messages, menus, intermission and finale.
//
//-----------------------------------------------------------------------------

#ifndef __HU_STUFF_H__
#define __HU_STUFF_H__

#include "d_event.h"

//
// Globally visible constants.
//
#define HU_FONTSTART	'!'	// the first font characters
#define HU_FONTEND	'_'	// the last font characters

// Size of a character cell, in pixels.
#define HU_CHARW	4
#define HU_CHARH	6
// Characters that fit on one line.
#define HU_COLUMNS	(SCREENWIDTH/HU_CHARW)

#define HU_MSGTIMEOUT	(4*TICRATE)

//
// HEADS UP TEXT
//

void HU_Init(void);
void HU_Start(void);

boolean HU_Responder(event_t* ev);

void HU_Ticker(void);
void HU_Drawer(void);

// Draws a string (black on a white box). '\n' starts a new line.
void HU_DrawText (int x, int y, const char* string);
// Draws at most [count] characters of one line.
void HU_DrawTextN (int x, int y, const char* string, int count);
// Black on whatever is there (no box).
void HU_DrawChar (int x, int y, int c);

// Name of the level being played, for the automap.
const char* HU_LevelName (void);
const char* HU_LevelNameFor (int episode, int map);

#endif
