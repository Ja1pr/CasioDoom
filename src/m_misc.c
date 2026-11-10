//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	Default Config File: the three settings that exist on the
//	calculator are kept in a tiny binary file.
//
//-----------------------------------------------------------------------------

#include "doomdef.h"
#include "i_system.h"
#include "v_video.h"
#include "m_misc.h"

#define CONFIGFILE	"DOOM.CFG"
#define CONFIGMAGIC	0xCD

extern int	showMessages;
extern int	screenblocks;

void M_SaveDefaults (void)
{
    byte	cfg[4];

    cfg[0] = CONFIGMAGIC;
    cfg[1] = showMessages;
    cfg[2] = screenblocks;
    cfg[3] = usegamma;
    if (I_FileOpen (CONFIGFILE, true))
    {
	I_FileWrite (cfg, 4);
	I_FileClose ();
    }
}

void M_LoadDefaults (void)
{
    byte	cfg[4];

    if (!I_FileOpen (CONFIGFILE, false))
	return;
    if (I_FileRead (cfg, 4) == 4 && cfg[0] == CONFIGMAGIC)
    {
	showMessages = cfg[1] != 0;
	screenblocks = cfg[2] >= 11 ? 11 : 10;
	usegamma = cfg[3] % 5;
    }
    I_FileClose ();
}
