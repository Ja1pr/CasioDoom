//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	Single player game loop timing.
//	What is left of DOOM's network game networking core.
//
//-----------------------------------------------------------------------------

#include "m_menu.h"
#include "i_system.h"
#include "i_video.h"
#include "g_game.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
#include "d_net.h"

//
// NETWORKING
//
// gametic is the tic about to (or currently being) run
// maketic is the tick that hasn't had control made for it yet
//
ticcmd_t        netcmds[MAXPLAYERS][BACKUPTICS];
int         	maketic;
int		ticdup = 1;

void D_ProcessEvents (void); 
void G_BuildTiccmd (ticcmd_t *cmd); 
void D_DoAdvanceDemo (void);
extern boolean advancedemo;

static int	gametime;

//
// NetUpdate
// Builds ticcmds for console player
//
void NetUpdate (void)
{
    int             nowtime;
    int             newtics;
    int		    i;
    
    // check time
    nowtime = I_GetTime ();
    newtics = nowtime - gametime;
    gametime = nowtime;
	
    if (newtics <= 0) 	// nothing new to update
	return;

    // build new ticcmds for console player
    for (i=0 ; i<newtics ; i++)
    {
	I_StartTic ();
	D_ProcessEvents ();
	if (maketic - gametic >= BACKUPTICS/2-1)
	    break;          // can't hold any more
	
	G_BuildTiccmd (&netcmds[consoleplayer][maketic%BACKUPTICS]);
	maketic++;
    }
}


//
// D_CheckNetGame
// Works out player numbers among the net participants
//
void D_CheckNetGame (void)
{
    int		i;

    netgame = false;
    deathmatch = false;
    consoleplayer = displayplayer = 0;
    for (i=0 ; i<MAXPLAYERS ; i++)
	playeringame[i] = (i == 0);
    maketic = 0;
    gametime = I_GetTime ();
}


//
// TryRunTics
//
void TryRunTics (void)
{
    int		counts;

    NetUpdate ();
    counts = maketic - gametic;

    // wait for at least one new tic
    while (counts < 1)
    {
	NetUpdate ();
	counts = maketic - gametic;
    }

    // run the tics
    while (counts--)
    {
	if (advancedemo)
	    D_DoAdvanceDemo ();
	M_Ticker ();
	G_Ticker ();
	gametic++;
    }
}
