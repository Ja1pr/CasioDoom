//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	Tic handling. The network code is gone: one calculator, one player.
//
//-----------------------------------------------------------------------------

#ifndef __D_NET__
#define __D_NET__

#include "d_player.h"

// Tic commands are buffered here between building and running them.
#define BACKUPTICS		12

// Create any new ticcmds
void NetUpdate (void);

// Nothing to quit, kept for the callers.
#define D_QuitNetGame()		((void)0)

//? how many ticks to run?
void TryRunTics (void);

void D_CheckNetGame (void);

#endif
