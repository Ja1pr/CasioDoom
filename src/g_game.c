//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:  none
//
//-----------------------------------------------------------------------------

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "doomdef.h" 
#include "doomstat.h"

#include "z_zone.h"
#include "f_finale.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_random.h"
#include "i_system.h"

#include "p_setup.h"
#include "p_saveg.h"
#include "p_tick.h"

#include "d_main.h"

#include "wi_stuff.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "am_map.h"

// Needs access to LFB.
#include "v_video.h"

#include "w_wad.h"

#include "p_local.h" 

#include "s_sound.h"

// Data.
#include "dstrings.h"
#include "sounds.h"

// SKY handling - still the wrong place.
#include "r_data.h"
#include "r_sky.h"



#include "g_game.h"


#define SAVESTRINGSIZE	24



boolean	G_CheckDemoStatus (void); 
void	G_ReadDemoTiccmd (ticcmd_t* cmd); 
void	G_PlayerReborn (int player); 
void	G_InitNew (skill_t skill, int episode, int map); 
 
void	G_DoReborn (int playernum); 
 
void	G_DoLoadLevel (void); 
void	G_DoNewGame (void); 
void	G_DoLoadGame (void); 
void	G_DoPlayDemo (void); 
void	G_DoCompleted (void); 
void	G_DoWorldDone (void); 
void	G_DoSaveGame (void); 
 
 
gameaction_t    gameaction; 
gamestate_t     gamestate; 
skill_t         gameskill; 
boolean		respawnmonsters;
// Nightmare / -fast: quicker demons and fireballs (see p_mobj.c)
boolean		fastmonsters;
int             gameepisode; 
int             gamemap; 
 
boolean         paused; 
boolean         sendpause;             	// send a pause event next tic 
boolean         sendsave;             	// send a save event next tic 
boolean         usergame;               // ok to save / end game 
 
boolean         viewactive; 
 
boolean         deathmatch;           	// only if started as net death 
boolean         netgame;                // only true if packets are broadcast 
boolean         playeringame[MAXPLAYERS]; 
player_t        players[MAXPLAYERS]; 
 
int             consoleplayer;          // player taking events and displaying 
int             displayplayer;          // view being displayed 
int             gametic; 
int             levelstarttic;          // gametic at level start 
int             totalkills, totalitems, totalsecret;    // for intermission 
 
boolean         demorecording;		// always false: no recording here
boolean         demoplayback; 
static const byte*	demobuffer;
static const byte*	demo_p;
boolean         singledemo;            	// quit after playing a demo from cmdline 
 
wbstartstruct_t wminfo;               	// parms for world map / intermission 
 
// 
// controls (have defaults) 
// 
#define key_right	KEY_RIGHTARROW
#define key_left	KEY_LEFTARROW
#define key_up		KEY_UPARROW
#define key_down	KEY_DOWNARROW
#define key_strafeleft	','
#define key_straferight	'.'
#define key_fire	KEY_RCTRL
#define key_use		' '
#define key_strafe	KEY_RALT
#define key_speed	KEY_RSHIFT
 
#define MAXPLMOVE		(forwardmove[1]) 
 
static const signed char forwardmove[2] = {0x19, 0x32}; 
static const signed char sidemove[2] = {0x18, 0x28}; 
static const short	angleturn[3] = {640, 1280, 320};	// + slow turn 

#define SLOWTURNTICS	6 
 
#define NUMKEYS		256 

static byte*    gamekeydown; 
static int      turnheld;				// for accelerative turning 
 
int		savegameslot; 
char		savedescription[SAVESTRINGSIZE+8]; 
 
int		bodyqueslot; 
 
 
//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer. 
// If recording a demo, write it out 
// 
void G_BuildTiccmd (ticcmd_t* cmd) 
{ 
    int		i; 
    boolean	strafe;
    int		speed;
    int		tspeed; 
    int		forward;
    int		side;
    
    memset (cmd,0,sizeof(*cmd)); 

    if (!gamekeydown)
	return;
 
    strafe = gamekeydown[key_strafe]; 
    speed = gamekeydown[key_speed];
 
    forward = side = 0;
    
    // use two stage accelerative turning
    // on the keyboard and joystick
    if (gamekeydown[key_right]
	|| gamekeydown[key_left]) 
	turnheld += ticdup; 
    else 
	turnheld = 0; 

    if (turnheld < SLOWTURNTICS) 
	tspeed = 2;             // slow turn 
    else 
	tspeed = speed;
    
    // let movement keys cancel each other out
    if (strafe) 
    { 
	if (gamekeydown[key_right]) 
	    side += sidemove[speed]; 
	if (gamekeydown[key_left]) 
	    side -= sidemove[speed]; 
    } 
    else 
    { 
	if (gamekeydown[key_right]) 
	    cmd->angleturn -= angleturn[tspeed]; 
	if (gamekeydown[key_left]) 
	    cmd->angleturn += angleturn[tspeed]; 
    } 
 
    if (gamekeydown[key_up]) 
	forward += forwardmove[speed]; 
    if (gamekeydown[key_down]) 
	forward -= forwardmove[speed]; 
    if (gamekeydown[key_straferight]) 
	side += sidemove[speed]; 
    if (gamekeydown[key_strafeleft]) 
	side -= sidemove[speed];
    
    // buttons
    if (gamekeydown[key_fire]) 
	cmd->buttons |= BT_ATTACK; 
 
    if (gamekeydown[key_use]) 
	cmd->buttons |= BT_USE;

    // chainsaw overrides 
    for (i=0 ; i<NUMWEAPONS-1 ; i++)        
	if (gamekeydown['1'+i]) 
	{ 
	    cmd->buttons |= BT_CHANGE; 
	    cmd->buttons |= i<<BT_WEAPONSHIFT; 
	    break; 
	}
    
    if (forward > MAXPLMOVE) 
	forward = MAXPLMOVE; 
    else if (forward < -MAXPLMOVE) 
	forward = -MAXPLMOVE; 
    if (side > MAXPLMOVE) 
	side = MAXPLMOVE; 
    else if (side < -MAXPLMOVE) 
	side = -MAXPLMOVE; 
 
    cmd->forwardmove += forward; 
    cmd->sidemove += side;
    
    // special buttons
    if (sendpause) 
    { 
	sendpause = false; 
	cmd->buttons = BT_SPECIAL | BTS_PAUSE; 
    } 
 
    if (sendsave) 
    { 
	sendsave = false; 
	cmd->buttons = BT_SPECIAL | BTS_SAVEGAME | (savegameslot<<BTS_SAVESHIFT); 
    } 
} 
 

//
// The converter only keeps the sky textures it knows are needed;
// fall back to any sky rather than fail.
//
static int G_Sky (const char* name)
{
    int		t = R_CheckTextureNumForName (name);

    if (t < 0)
	t = R_CheckTextureNumForName ("SKY1");
    if (t < 0)
	t = 0;
    return t;
}

static void G_SetSky (void)
{
    static const char skies[4][5] = { "SKY1", "SKY2", "SKY3", "SKY4" };

    if (gamemode == commercial)
	skytexture = G_Sky (gamemap < 12 ? "SKY1" : gamemap < 21 ? "SKY2" : "SKY3");
    else
	skytexture = G_Sky (skies[(gameepisode-1)&3]);
}

//
// G_DoLoadLevel 
//
void G_DoLoadLevel (void) 
{ 
    int             i; 

    // Set the sky map.
    skyflatnum = cdinfo->skyflat;
    G_SetSky ();

    levelstarttic = gametic;        // for time calculation
    
    if (wipegamestate == GS_LEVEL) 
	wipegamestate = -1;             // force a wipe 

    gamestate = GS_LEVEL; 

    for (i=0 ; i<MAXPLAYERS ; i++) 
    { 
	if (playeringame[i] && players[i].playerstate == PST_DEAD) 
	    players[i].playerstate = PST_REBORN; 
	memset (players[i].frags,0,sizeof(players[i].frags)); 
    } 
		 
    P_SetupLevel (gameepisode, gamemap, 0, gameskill);    
    displayplayer = consoleplayer;		// view the guy you are playing    
    gameaction = ga_nothing; 
    
    // clear cmd building stuff
    if (!gamekeydown)
	gamekeydown = Z_Malloc (NUMKEYS, PU_STATIC, 0);
    memset (gamekeydown, 0, NUMKEYS); 
    sendpause = sendsave = paused = false; 
} 
 
 
//
// G_Responder  
// Get info needed to make ticcmd_ts for the players.
// 
boolean G_Responder (event_t* ev) 
{ 
    // any other key pops up menu if in demos
    if (gameaction == ga_nothing && !singledemo && 
	(demoplayback || gamestate == GS_DEMOSCREEN) 
	) 
    { 
	if (ev->type == ev_keydown) 
	{ 
	    M_StartControlPanel (); 
	    return true; 
	} 
	return false; 
    } 
 
    if (gamestate == GS_LEVEL) 
    { 
	if (HU_Responder (ev)) 
	    return true;	// chat ate the event 
	if (ST_Responder (ev)) 
	    return true;	// status window ate it 
	if (AM_Responder (ev)) 
	    return true;	// automap ate it 
    } 
	 
    if (gamestate == GS_FINALE) 
    { 
	if (F_Responder (ev)) 
	    return true;	// finale ate the event 
    } 
	 
    switch (ev->type) 
    { 
      case ev_keydown: 
	if (ev->data1 == KEY_PAUSE) 
	{ 
	    sendpause = true; 
	    return true; 
	} 
	if (ev->data1 <NUMKEYS && gamekeydown) 
	    gamekeydown[ev->data1] = true; 
	return true;    // eat key down events 
 
      case ev_keyup: 
	if (ev->data1 <NUMKEYS && gamekeydown) 
	    gamekeydown[ev->data1] = false; 
	return false;   // always let key up events filter down 
		 
      default: 
	break; 
    } 
 
    return false; 
} 
 
 
 
//
// G_Ticker
// Make ticcmd_ts for the players.
//
void G_Ticker (void) 
{ 
    int		i;
    int		buf; 
    ticcmd_t*	cmd;
    
    // do player reborns if needed
    for (i=0 ; i<MAXPLAYERS ; i++) 
	if (playeringame[i] && players[i].playerstate == PST_REBORN) 
	    G_DoReborn (i);
    
    // do things to change the game state
    while (gameaction != ga_nothing) 
    { 
	switch (gameaction) 
	{ 
	  case ga_loadlevel: 
	    G_DoLoadLevel (); 
	    break; 
	  case ga_newgame: 
	    G_DoNewGame (); 
	    break; 
	  case ga_loadgame: 
	    G_DoLoadGame (); 
	    break; 
	  case ga_savegame: 
	    G_DoSaveGame (); 
	    break; 
	  case ga_playdemo: 
	    G_DoPlayDemo (); 
	    break; 
	  case ga_completed: 
	    G_DoCompleted (); 
	    break; 
	  case ga_victory: 
	    F_StartFinale (); 
	    break; 
	  case ga_worlddone: 
	    G_DoWorldDone (); 
	    break; 
	  case ga_screenshot: 
	    gameaction = ga_nothing; 
	    break; 
	  case ga_nothing: 
	    break; 
	} 
    }
    
    // get commands
    buf = (gametic/ticdup)%BACKUPTICS; 
 
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (playeringame[i]) 
	{ 
	    cmd = &players[i].cmd; 
 
	    memcpy (cmd, &netcmds[i][buf], sizeof(ticcmd_t)); 
 
	    if (demoplayback) 
		G_ReadDemoTiccmd (cmd); 
	}
    }
    
    // check for special buttons
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (playeringame[i]) 
	{ 
	    if (players[i].cmd.buttons & BT_SPECIAL) 
	    { 
		switch (players[i].cmd.buttons & BT_SPECIALMASK) 
		{ 
		  case BTS_PAUSE: 
		    paused ^= 1; 
		    break; 
					 
		  case BTS_SAVEGAME: 
		    savegameslot =  
			(players[i].cmd.buttons & BTS_SAVEMASK)>>BTS_SAVESHIFT; 
		    gameaction = ga_savegame; 
		    break; 
		} 
	    } 
	}
    }
    
    // do main actions
    switch (gamestate) 
    { 
      case GS_LEVEL: 
	P_Ticker (); 
	ST_Ticker (); 
	AM_Ticker (); 
	HU_Ticker ();            
	break; 
	 
      case GS_INTERMISSION: 
	WI_Ticker (); 
	break; 
			 
      case GS_FINALE: 
	F_Ticker (); 
	break; 
 
      case GS_DEMOSCREEN: 
	D_PageTicker (); 
	break; 
    }        
} 
 
 
//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//

//
// G_InitPlayer 
// Called at the start.
// Called by the game initialization functions.
//
void G_InitPlayer (int player) 
{ 
    player_t*	p; 
 
    // set up the saved info         
    p = &players[player]; 
	 
    // clear everything else to defaults 
    G_PlayerReborn (player); 
	 
} 
 
 

//
// G_PlayerFinishLevel
// Can when a player completes a level.
//
void G_PlayerFinishLevel (int player) 
{ 
    player_t*	p; 
	 
    p = &players[player]; 
	 
    memset (p->powers, 0, sizeof (p->powers)); 
    memset (p->cards, 0, sizeof (p->cards)); 
    p->mo->flags &= ~MF_SHADOW;		// cancel invisibility 
    p->extralight = 0;			// cancel gun flashes 
    p->fixedcolormap = 0;		// cancel ir gogles 
    p->damagecount = 0;			// no palette changes 
    p->bonuscount = 0; 
} 
 

//
// G_PlayerReborn
// Called after a player dies 
// almost everything is cleared and initialized 
//
void G_PlayerReborn (int player) 
{ 
    player_t*	p; 
    int		i; 
    int		frags[MAXPLAYERS]; 
    int		killcount;
    int		itemcount;
    int		secretcount; 
	 
    memcpy (frags,players[player].frags,sizeof(frags)); 
    killcount = players[player].killcount; 
    itemcount = players[player].itemcount; 
    secretcount = players[player].secretcount; 
	 
    p = &players[player]; 
    memset (p, 0, sizeof(*p)); 
 
    memcpy (players[player].frags, frags, sizeof(players[player].frags)); 
    players[player].killcount = killcount; 
    players[player].itemcount = itemcount; 
    players[player].secretcount = secretcount; 
 
    p->usedown = p->attackdown = true;	// don't do anything immediately 
    p->playerstate = PST_LIVE;       
    p->health = MAXHEALTH; 
    p->readyweapon = p->pendingweapon = wp_pistol; 
    p->weaponowned[wp_fist] = true; 
    p->weaponowned[wp_pistol] = true; 
    p->ammo[am_clip] = 50; 
	 
    for (i=0 ; i<NUMAMMO ; i++) 
	p->maxammo[i] = maxammo[i]; 
		 
}

//
// G_CheckSpot  
// Returns false if the player cannot be respawned
// at the given mapthing_t spot  
// because something is occupying it 
//

//
// G_DoReborn 
// Single player: reload the level from scratch.
// 
void G_DoReborn (int playernum) 
{ 
    (void)playernum;
    gameaction = ga_loadlevel;  
} 
 
 
void G_ScreenShot (void) 
{ 
} 
 


// DOOM Par Times
static const short pars[4][10] = 
{ 
    {0}, 
    {0,30,75,120,90,165,180,180,30,165}, 
    {0,90,90,90,120,90,360,240,30,170}, 
    {0,90,45,90,150,90,90,165,30,135} 
}; 

// DOOM II Par Times
static const short cpars[32] =
{
    30,90,120,120,90,150,120,120,270,90,	//  1-10
    210,150,150,150,210,150,420,150,210,150,	// 11-20
    240,150,180,150,150,300,330,420,300,180,	// 21-30
    120,30					// 31-32
};
 

//
// G_DoCompleted 
//
boolean		secretexit; 
extern char*	pagename; 
 
void G_ExitLevel (void) 
{ 
    secretexit = false; 
    gameaction = ga_completed; 
} 

// Here's for the german edition.
void G_SecretExitLevel (void) 
{ 
    // IF NO WOLF3D LEVELS, NO SECRET EXIT!
    if ( (gamemode == commercial)
      && !P_MapExists (1, 31))
	secretexit = false;
    else
	secretexit = true; 
    gameaction = ga_completed; 
} 
 
void G_DoCompleted (void) 
{ 
    int             i; 
	 
    gameaction = ga_nothing; 
 
    for (i=0 ; i<MAXPLAYERS ; i++) 
	if (playeringame[i]) 
	    G_PlayerFinishLevel (i);        // take away cards and stuff 
	 
    if (automapactive) 
	AM_Stop (); 
	
    if ( gamemode != commercial)
	switch(gamemap)
	{
	  case 8:
	    gameaction = ga_victory;
	    return;
	  case 9: 
	    for (i=0 ; i<MAXPLAYERS ; i++) 
		players[i].didsecret = true; 
	    break;
	}
		
//#if 0  Hmmm - why?
    if ( (gamemap == 8)
	 && (gamemode != commercial) ) 
    {
	// victory 
	gameaction = ga_victory; 
	return; 
    } 
	 
    if ( (gamemap == 9)
	 && (gamemode != commercial) ) 
    {
	// exit secret level 
	for (i=0 ; i<MAXPLAYERS ; i++) 
	    players[i].didsecret = true; 
    } 
//#endif
    
	 
    wminfo.didsecret = players[consoleplayer].didsecret; 
    wminfo.epsd = gameepisode -1; 
    wminfo.last = gamemap -1;
    
    // wminfo.next is 0 biased, unlike gamemap
    if ( gamemode == commercial)
    {
	if (secretexit)
	    switch(gamemap)
	    {
	      case 15: wminfo.next = 30; break;
	      case 31: wminfo.next = 31; break;
	    }
	else
	    switch(gamemap)
	    {
	      case 31:
	      case 32: wminfo.next = 15; break;
	      default: wminfo.next = gamemap;
	    }
    }
    else
    {
	if (secretexit) 
	    wminfo.next = 8; 	// go to secret level 
	else if (gamemap == 9) 
	{
	    // returning from secret level 
	    switch (gameepisode) 
	    { 
	      case 1: 
		wminfo.next = 3; 
		break; 
	      case 2: 
		wminfo.next = 5; 
		break; 
	      case 3: 
		wminfo.next = 6; 
		break; 
	      case 4:
		wminfo.next = 2;
		break;
	    }                
	} 
	else 
	    wminfo.next = gamemap;          // go to next level 
    }
		 
    wminfo.maxkills = totalkills; 
    wminfo.maxitems = totalitems; 
    wminfo.maxsecret = totalsecret; 
    wminfo.maxfrags = 0; 
    if ( gamemode == commercial )
	wminfo.partime = 35*cpars[gamemap-1]; 
    else
	wminfo.partime = 35*pars[gameepisode][gamemap]; 
    wminfo.pnum = consoleplayer; 
 
    for (i=0 ; i<MAXPLAYERS ; i++) 
    { 
	wminfo.plyr[i].in = playeringame[i]; 
	wminfo.plyr[i].skills = players[i].killcount; 
	wminfo.plyr[i].sitems = players[i].itemcount; 
	wminfo.plyr[i].ssecret = players[i].secretcount; 
	wminfo.plyr[i].stime = leveltime; 
	memcpy (wminfo.plyr[i].frags, players[i].frags 
		, sizeof(wminfo.plyr[i].frags)); 
    } 
 
    gamestate = GS_INTERMISSION; 
    viewactive = false; 
    automapactive = false; 
 
	
    WI_Start (&wminfo); 
} 


//
// G_WorldDone 
//
void G_WorldDone (void) 
{ 
    gameaction = ga_worlddone; 

    if (secretexit) 
	players[consoleplayer].didsecret = true; 

    if ( gamemode == commercial )
    {
	switch (gamemap)
	{
	  case 15:
	  case 31:
	    if (!secretexit)
		break;
	  case 6:
	  case 11:
	  case 20:
	  case 30:
	    F_StartFinale ();
	    break;
	}
    }
} 
 
void G_DoWorldDone (void) 
{        
    gamemap = wminfo.next+1; 
    if (!P_MapExists (gameepisode, gamemap))
    {
	// A partial DOOM.CWD: the next level was not converted.
	gameaction = ga_nothing; 
	D_StartTitle ();
	return;
    }
    gamestate = GS_LEVEL; 
    G_DoLoadLevel (); 
    gameaction = ga_nothing; 
    viewactive = true; 
} 
 


//
// G_InitFromSavegame
// Can be called by the startup code or the menu task. 
//
void G_SaveName (int slot, char* name)
{
    sprintf (name, "DOOMSAV%d.DSG", slot);
}

void G_LoadGame (int slot) 
{ 
    savegameslot = slot;
    gameaction = ga_loadgame; 
} 
 
#define VERSIONSIZE		16 

static void G_SaveVersion (char* vcheck)
{
    // the layout of the structures in the file depends on the build
    memset (vcheck,0,VERSIONSIZE); 
    sprintf (vcheck,"cd%i.%i.%i.%i",CWAD_VERSION, (int)sizeof(mobj_t),
	     (int)sizeof(player_t), (int)sizeof(sector_t)); 
}

void G_DoLoadGame (void) 
{ 
    int		a,b,c; 
    char	name[32];
    char	vcheck[VERSIONSIZE]; 
    char	header[SAVESTRINGSIZE+VERSIONSIZE]; 
	 
    gameaction = ga_nothing; 
	 
    G_SaveName (savegameslot, name);
    if (!I_FileOpen (name, false))
	return;
    SV_Begin ();
    SV_Read (header, sizeof(header));
    
    // skip the description field 
    G_SaveVersion (vcheck);
    if (memcmp (header + SAVESTRINGSIZE, vcheck, VERSIONSIZE)) 
    {
	SV_End (false);
	I_FileClose ();
	players[consoleplayer].message = "SAVE IS FROM ANOTHER VERSION";
	return;				// bad version 
    }
			 
    gameskill = SV_ReadByte (); 
    gameepisode = SV_ReadByte (); 
    gamemap = SV_ReadByte (); 

    if (!P_MapExists (gameepisode, gamemap))
    {
	SV_End (false);
	I_FileClose ();
	return;
    }

    // load a base level 
    demoplayback = false;
    respawnparm = fastparm = nomonsters = false;
    G_InitNew (gameskill, gameepisode, gamemap); 
 
    // get the times 
    a = SV_ReadByte (); 
    b = SV_ReadByte (); 
    c = SV_ReadByte (); 
    leveltime = (a<<16) + (b<<8) + c; 
	 
    // dearchive all the modifications
    P_UnArchivePlayers (); 
    P_UnArchiveWorld (); 
    P_UnArchiveThinkers (); 
    P_UnArchiveSpecials (); 
 
    a = SV_ReadByte ();
    b = SV_End (false);
    I_FileClose ();
    if (a != 0x1d || !b) 
	I_Error ("Bad savegame");
} 
 

//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string 
//
void
G_SaveGame
( int	slot,
  const char*	description ) 
{ 
    savegameslot = slot; 
    strncpy (savedescription, description, SAVESTRINGSIZE); 
    savedescription[SAVESTRINGSIZE-1] = 0;
    sendsave = true; 
} 
 
void G_DoSaveGame (void) 
{ 
    char	name[32]; 
    char	header[SAVESTRINGSIZE+VERSIONSIZE]; 
    boolean	ok;
	
    gameaction = ga_nothing; 
    G_SaveName (savegameslot, name);
    if (!I_FileOpen (name, true))
    {
	players[consoleplayer].message = "CANNOT WRITE SAVE FILE";
	return;
    }
    SV_Begin ();
	 
    memset (header, 0, sizeof(header));
    memcpy (header, savedescription, SAVESTRINGSIZE); 
    G_SaveVersion (header + SAVESTRINGSIZE);
    SV_Write (header, sizeof(header));
	 
    SV_WriteByte (gameskill); 
    SV_WriteByte (gameepisode); 
    SV_WriteByte (gamemap); 
    SV_WriteByte (leveltime>>16); 
    SV_WriteByte (leveltime>>8); 
    SV_WriteByte (leveltime); 
 
    P_ArchivePlayers (); 
    P_ArchiveWorld (); 
    P_ArchiveThinkers (); 
    P_ArchiveSpecials (); 
	 
    SV_WriteByte (0x1d);		// consistancy marker 
    ok = SV_End (true);
    I_FileClose ();

    savedescription[0] = 0;		 
	 
    players[consoleplayer].message = ok ? GGSAVED : "SAVE FAILED (STORAGE FULL?)";
} 
 

//
// G_InitNew
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set. 
//
static skill_t	d_skill; 
static int     d_episode; 
static int     d_map; 
 
void
G_DeferedInitNew
( skill_t	skill,
  int		episode,
  int		map) 
{ 
    d_skill = skill; 
    d_episode = episode; 
    d_map = map; 
    gameaction = ga_newgame; 
} 


void G_DoNewGame (void) 
{
    demoplayback = false; 
    netgame = false;
    deathmatch = false;
    respawnparm = false;
    fastparm = false;
    nomonsters = false;
    consoleplayer = 0;
    G_InitNew (d_skill, d_episode, d_map); 
    gameaction = ga_nothing; 
} 


void
G_InitNew
( skill_t	skill,
  int		episode,
  int		map ) 
{ 
    int             i; 
	 
    paused = false; 

    if (skill > sk_nightmare) 
	skill = sk_nightmare;

    if (episode < 1)
      episode = 1; 
    if (episode > 4)
      episode = 4;

    if (map < 1) 
	map = 1;
    
    if ( (map > 9)
	 && ( gamemode != commercial) )
      map = 9; 
		 
    M_ClearRandom (); 
	 
    if (skill == sk_nightmare || respawnparm )
	respawnmonsters = true;
    else
	respawnmonsters = false;
		
    // The original patched the state and mobjinfo tables here; they are
    // in ROM now, so P_SetMobjState / P_SpawnMissile look at this flag.
    fastmonsters = fastparm || skill == sk_nightmare;
			 
    // force players to be initialized upon first level load         
    for (i=0 ; i<MAXPLAYERS ; i++) 
	players[i].playerstate = PST_REBORN; 
 
    usergame = true;                // will be set false if a demo 
    paused = false; 
    demoplayback = false; 
    automapactive = false; 
    viewactive = true; 
    gameepisode = episode; 
    gamemap = map; 
    gameskill = skill; 
 
    G_DoLoadLevel (); 
} 
 

//
// DEMO PLAYBACK 
// 
#define DEMOMARKER		0x80
#define DEMOVERSION		109


void G_ReadDemoTiccmd (ticcmd_t* cmd) 
{ 
    if (*demo_p == DEMOMARKER) 
    {
	// end of demo data stream 
	G_CheckDemoStatus (); 
	return; 
    } 
    cmd->forwardmove = ((signed char)*demo_p++); 
    cmd->sidemove = ((signed char)*demo_p++); 
    cmd->angleturn = ((unsigned char)*demo_p++)<<8; 
    cmd->buttons = (unsigned char)*demo_p++; 
} 


//
// G_PlayDemo 
//

static const char*	defdemoname; 
 
void G_DeferedPlayDemo (const char* name) 
{ 
    defdemoname = name; 
    gameaction = ga_playdemo; 
} 
 
void G_DoPlayDemo (void) 
{ 
    skill_t skill; 
    int             episode, map; 
    boolean	    multi;
	 
    gameaction = ga_nothing; 
    if (W_CheckNumForName (defdemoname) < 0)
    {
	D_AdvanceDemo ();
	return;
    }
    demobuffer = demo_p = W_CacheLumpName (defdemoname, PU_STATIC); 
    if ( *demo_p++ != DEMOVERSION)
    {
	W_ReleaseLump (demobuffer);
	D_AdvanceDemo ();
	return;
    }
    
    skill = *demo_p++; 
    episode = *demo_p++; 
    map = *demo_p++; 
    deathmatch = *demo_p++;
    respawnparm = *demo_p++;
    fastparm = *demo_p++;
    nomonsters = *demo_p++;
    consoleplayer = *demo_p++;
	
    // the file always has four "player present" bytes
    multi = demo_p[1] || demo_p[2] || demo_p[3] || consoleplayer
	|| deathmatch;
    demo_p += 4;
    consoleplayer = 0;
    deathmatch = false;
    if (multi || !P_MapExists (episode, map))
    {
	// multiplayer demos cannot be shown
	respawnparm = fastparm = nomonsters = false;
	W_ReleaseLump (demobuffer);
	D_AdvanceDemo ();
	return;
    }

    G_InitNew (skill, episode, map); 

    usergame = false; 
    demoplayback = true; 
} 


/* 
=================== 
= 
= G_CheckDemoStatus 
= 
= Called after a death or level completion to allow demos to be cleaned up 
= Returns true if a new demo loop action will take place 
=================== 
*/ 
 
boolean G_CheckDemoStatus (void) 
{ 
    if (demoplayback) 
    { 
	if (singledemo) 
	    I_Quit (); 
			 
	W_ReleaseLump (demobuffer);
	demoplayback = false; 
	netgame = false;
	deathmatch = false;
	respawnparm = false;
	fastparm = false;
	nomonsters = false;
	consoleplayer = 0;
	D_AdvanceDemo (); 
	return true; 
    } 
 
    return false; 
} 
