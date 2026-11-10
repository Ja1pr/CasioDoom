//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	Intermission screens.
//
//  CasioDoom: single player only. The tally (kills / items / secrets /
//  time / par) and the "you are here" map of the original are kept, but
//  drawn as text in the small font over the scaled-down pictures.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>

#include "z_zone.h"
#include "m_random.h"
#include "i_system.h"
#include "w_wad.h"
#include "g_game.h"
#include "r_local.h"
#include "s_sound.h"
#include "doomstat.h"
#include "v_video.h"
#include "hu_stuff.h"

#include "wi_stuff.h"

//
// Data needed to add patches to full screen intermission pics.
// Patches are statistics messages, and animations.
// Loads of by-pixel layout and placement, offsets etc.
//

// GLOBAL LOCATIONS
#define NUMEPISODES	4
#define NUMMAPS		9

// in tics
#define SHOWNEXTLOCDELAY	4

typedef struct
{
    unsigned char	x;
    unsigned char	y;
} point_t;

// Positions of the level nodes on the 128x64 episode maps
// (the originals scaled by 0.4 / 0.32).
static const point_t lnodes[NUMEPISODES-1][NUMMAPS] =
{
    // Episode 0 World Map
    {
	{ 74, 52 },	// location of level 0 (CJ)
	{ 59, 46 },	// location of level 1 (CJ)
	{ 28, 39 },	// location of level 2 (CJ)
	{ 84, 33 },	// location of level 3 (CJ)
	{ 46, 28 },	// location of level 4 (CJ)
	{ 66, 18 },	// location of level 5 (CJ)
	{ 28, 18 },	// location of level 6 (CJ)
	{ 54, 9 },	// location of level 7 (CJ)
	{ 28, 8 }	// location of level 8 (CJ)
    },

    // Episode 1 World Map should go here
    {
	{ 102, 8 },	// location of level 0 (CJ)
	{ 39, 16 },	// location of level 1 (CJ)
	{ 75, 20 },	// location of level 2 (CJ)
	{ 51, 25 },	// location of level 3 (CJ)
	{ 86, 29 },	// location of level 4 (CJ)
	{ 53, 42 },	// location of level 5 (CJ)
	{ 83, 44 },	// location of level 6 (CJ)
	{ 59, 45 },	// location of level 7 (CJ)
	{ 94, 51 }	// location of level 8 (CJ)
    },

    // Episode 2 World Map should go here
    {
	{ 62, 54 },	// location of level 0 (CJ)
	{ 19, 49 },	// location of level 1 (CJ)
	{ 70, 30 },	// location of level 2 (CJ)
	{ 106, 24 },	// location of level 3 (CJ)
	{ 52, 15 },	// location of level 4 (CJ)
	{ 112, 7 },	// location of level 5 (CJ)
	{ 79, 15 },	// location of level 6 (CJ)
	{ 56, 8 },	// location of level 7 (CJ)
	{ 112, 44 }	// location of level 8 (CJ)
    }

};


//
// GENERAL DATA
//

// States for single-player
#define SP_KILLS		0
#define SP_ITEMS		2
#define SP_SECRET		4
#define SP_FRAGS		6 
#define SP_TIME			8 
#define SP_PAR			ST_TIME

#define SP_PAUSE		1

// in seconds
#define SHOWNEXTLOCDELAY	4

// used to accelerate or skip a stage
static int		acceleratestage;

// wbs->pnum
static int		me;

 // specifies current state
static stateenum_t	state;

// contains information passed into intermission
static wbstartstruct_t*	wbs;

static wbplayerstruct_t* plrs;  // wbs->plyr[]

// used for general timing
static int 		cnt;  

// used for timing of background animation
static int 		bcnt;

static int		cnt_kills;
static int		cnt_items;
static int		cnt_secret;
static int		cnt_time;
static int		cnt_par;
static int		cnt_pause;

static int		sp_state;

static boolean		snl_pointeron = false;


//
// CODE
//

// slam background
static void WI_slamBackground(void)
{
    char	name[16];

    if (gamemode == commercial || wbs->epsd > 2)
	strcpy (name, "INTERPIC");
    else 
	sprintf (name, "WIMAP%d", wbs->epsd);

    V_FillRect (0, 0, SCREENWIDTH, SCREENHEIGHT, V_WHITE);
    if (W_CheckNumForName (name) >= 0)
	V_DrawPatch (0, 0, 0, W_CacheLumpName (name, PU_CACHE));
}

// The ticker is used to detect keys
//  because of timing issues in netgames.
boolean WI_Responder(event_t* ev)
{
    (void)ev;
    return false;
}

static void WI_Centered (int y, const char* s)
{
    int		len = strlen (s);

    if (len > HU_COLUMNS-1)
	len = HU_COLUMNS-1;
    HU_DrawTextN ((SCREENWIDTH - len*HU_CHARW)/2, y, s, len);
}

// Draws "<Levelname> Finished!"
static void WI_drawLF(void)
{
    WI_Centered (2, HU_LevelNameFor (wbs->epsd+1, wbs->last+1));
    WI_Centered (2+HU_CHARH, "FINISHED");
}

// Draws "Entering <LevelName>"
static void WI_drawEL(void)
{
    WI_Centered (2, "ENTERING");
    WI_Centered (2+HU_CHARH, HU_LevelNameFor (wbs->epsd+1, wbs->next+1));
}

static void WI_drawOnLnode (int n, const char* lump)
{
    if (W_CheckNumForName (lump) < 0)
	return;
    V_DrawPatch (lnodes[wbs->epsd][n].x - 2, lnodes[wbs->epsd][n].y - 2, 0,
		 W_CacheLumpName (lump, PU_CACHE));
}

static void WI_End(void)
{
}

static void WI_initNoState(void)
{
    state = NoState;
    acceleratestage = 0;
    cnt = 10;
}

static void WI_updateNoState(void) {

    if (!--cnt)
    {
	WI_End();
	G_WorldDone();
    }

}

static void WI_initShowNextLoc(void)
{
    state = ShowNextLoc;
    acceleratestage = 0;
    cnt = SHOWNEXTLOCDELAY * TICRATE;
}

static void WI_updateShowNextLoc(void)
{
    if (!--cnt || acceleratestage)
	WI_initNoState();
    else
	snl_pointeron = (cnt & 31) < 20;
}

static void WI_drawShowNextLoc(void)
{
    int		i;
    int		last;

    WI_slamBackground();

    if ( gamemode != commercial && wbs->epsd <= 2)
    {
	last = (wbs->last == 8) ? wbs->next - 1 : wbs->last;

	// draw a splat on taken cities.
	for (i=0 ; i<=last ; i++)
	    WI_drawOnLnode (i, "WISPLAT");

	// splat the secret level?
	if (wbs->didsecret)
	    WI_drawOnLnode (8, "WISPLAT");

	// draw flashing ptr
	if (snl_pointeron && wbs->next < NUMMAPS)
	    WI_drawOnLnode (wbs->next, "WIURH0"); 
    }

    // draws which level you are entering..
    if ( (gamemode != commercial)
	 || wbs->next != 30)
	WI_drawEL();  

}

static void WI_drawNoState(void)
{
    snl_pointeron = true;
    WI_drawShowNextLoc();
}

static void WI_initStats(void)
{
    state = StatCount;
    acceleratestage = 0;
    sp_state = 1;
    cnt_kills = cnt_items = cnt_secret = -1;
    cnt_time = cnt_par = -1;
    cnt_pause = TICRATE;
}

static void WI_updateStats(void)
{
    if (acceleratestage && sp_state != 10)
    {
	acceleratestage = 0;
	cnt_kills = (plrs[me].skills * 100) / wbs->maxkills;
	cnt_items = (plrs[me].sitems * 100) / wbs->maxitems;
	cnt_secret = (plrs[me].ssecret * 100) / wbs->maxsecret;
	cnt_time = plrs[me].stime / TICRATE;
	cnt_par = wbs->partime / TICRATE;
	sp_state = 10;
    }

    if (sp_state == 2)
    {
	cnt_kills += 2;

	if (cnt_kills >= (plrs[me].skills * 100) / wbs->maxkills)
	{
	    cnt_kills = (plrs[me].skills * 100) / wbs->maxkills;
	    sp_state++;
	}
    }
    else if (sp_state == 4)
    {
	cnt_items += 2;

	if (cnt_items >= (plrs[me].sitems * 100) / wbs->maxitems)
	{
	    cnt_items = (plrs[me].sitems * 100) / wbs->maxitems;
	    sp_state++;
	}
    }
    else if (sp_state == 6)
    {
	cnt_secret += 2;

	if (cnt_secret >= (plrs[me].ssecret * 100) / wbs->maxsecret)
	{
	    cnt_secret = (plrs[me].ssecret * 100) / wbs->maxsecret;
	    sp_state++;
	}
    }

    else if (sp_state == 8)
    {
	cnt_time += 3;

	if (cnt_time >= plrs[me].stime / TICRATE)
	    cnt_time = plrs[me].stime / TICRATE;

	cnt_par += 3;

	if (cnt_par >= wbs->partime / TICRATE)
	{
	    cnt_par = wbs->partime / TICRATE;

	    if (cnt_time >= plrs[me].stime / TICRATE)
		sp_state++;
	}
    }
    else if (sp_state == 10)
    {
	if (acceleratestage)
	{
	    if (gamemode == commercial)
		WI_initNoState();
	    else
		WI_initShowNextLoc();
	}
    }
    else if (sp_state & 1)
    {
	if (!--cnt_pause)
	{
	    sp_state++;
	    cnt_pause = TICRATE;
	}
    }

}

static void WI_drawStat (int y, const char* label, int value, boolean time)
{
    char	buf[40];

    if (value < 0)
	sprintf (buf, "%s", label);
    else if (time)
    {
	if (value >= 3600)
	    sprintf (buf, "%s  SUCKS", label);
	else
	    sprintf (buf, "%s  %d:%02d", label, value/60, value%60);
    }
    else
	sprintf (buf, "%s  %3d%%", label, value);
    HU_DrawText (26, y, buf);
}

static void WI_drawStats(void)
{
    WI_slamBackground();
    WI_drawLF();

    WI_drawStat (18, "KILLS ", cnt_kills, false);
    WI_drawStat (25, "ITEMS ", cnt_items, false);
    WI_drawStat (32, "SECRET", cnt_secret, false);
    WI_drawStat (43, "TIME  ", cnt_time, true);
    if (wbs->epsd < 3)
	WI_drawStat (50, "PAR   ", cnt_par, true);
}

static void WI_checkForAccelerate(void)
{
    int   i;
    player_t  *player;

    // check for button presses to skip delays
    for (i=0, player = players ; i<MAXPLAYERS ; i++, player++)
    {
	if (playeringame[i])
	{
	    if (player->cmd.buttons & BT_ATTACK)
	    {
		if (!player->attackdown)
		    acceleratestage = 1;
		player->attackdown = true;
	    }
	    else
		player->attackdown = false;
	    if (player->cmd.buttons & BT_USE)
	    {
		if (!player->usedown)
		    acceleratestage = 1;
		player->usedown = true;
	    }
	    else
		player->usedown = false;
	}
    }
}



// Updates stuff each tick
void WI_Ticker(void)
{
    // counter for general background animation
    bcnt++;  

    WI_checkForAccelerate();

    switch (state)
    {
      case StatCount:
	WI_updateStats();
	break;
	
      case ShowNextLoc:
	WI_updateShowNextLoc();
	break;
	
      case NoState:
	WI_updateNoState();
	break;
    }

}

void WI_Drawer (void)
{
    switch (state)
    {
      case StatCount:
	WI_drawStats();
	break;
	
      case ShowNextLoc:
	WI_drawShowNextLoc();
	break;
	
      case NoState:
	WI_drawNoState();
	break;
    }
}


static void WI_initVariables(wbstartstruct_t* wbstartstruct)
{

    wbs = wbstartstruct;

    acceleratestage = 0;
    cnt = bcnt = 0;
    me = wbs->pnum;
    plrs = wbs->plyr;

    if (!wbs->maxkills)
	wbs->maxkills = 1;

    if (!wbs->maxitems)
	wbs->maxitems = 1;

    if (!wbs->maxsecret)
	wbs->maxsecret = 1;

    if ( gamemode != retail )
      if (wbs->epsd > 2)
	wbs->epsd -= 3;
}

void WI_Start(wbstartstruct_t* wbstartstruct)
{

    WI_initVariables(wbstartstruct);
    WI_initStats();
}
