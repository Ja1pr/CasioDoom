//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	Status bar code.
//	Does the face/direction indicator animatin.
//	Does palette indicators as well (red pain/berserk, bright pickup)
//
//  CasioDoom: a 128x10 status bar, redrawn completely every frame:
//
//    AMMO | HEALTH% | weapons 2-7 | face | ARMOR% | keys
//
//  The cheat codes cannot be typed on a calculator; they are offered by
//  the options menu instead (ST_Cheat).
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "i_system.h"
#include "i_video.h"
#include "z_zone.h"
#include "m_random.h"
#include "w_wad.h"

#include "doomdef.h"

#include "g_game.h"

#include "st_stuff.h"

#include "p_local.h"
#include "p_inter.h"

#include "am_map.h"

#include "v_video.h"

// State.
#include "doomstat.h"

// Data.
#include "dstrings.h"

//
// STATUS BAR DATA
//

// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS		1
#define STARTBONUSPALS		9
#define NUMREDPALS			8
#define NUMBONUSPALS		4
// Radiation suit, green shift.
#define RADIATIONPAL		13

// Number of status faces.
#define ST_NUMPAINFACES		5
#define ST_NUMSTRAIGHTFACES	3
#define ST_NUMTURNFACES		2
#define ST_NUMSPECIALFACES		3

#define ST_FACESTRIDE \
          (ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES)

#define ST_NUMEXTRAFACES		2

#define ST_NUMFACES \
          (ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES)

#define ST_TURNOFFSET		(ST_NUMSTRAIGHTFACES)
#define ST_OUCHOFFSET		(ST_TURNOFFSET + ST_NUMTURNFACES)
#define ST_EVILGRINOFFSET		(ST_OUCHOFFSET + 1)
#define ST_RAMPAGEOFFSET		(ST_EVILGRINOFFSET + 1)
#define ST_GODFACE			(ST_NUMPAINFACES*ST_FACESTRIDE)
#define ST_DEADFACE			(ST_GODFACE+1)

#define ST_EVILGRINCOUNT		(2*TICRATE)
#define ST_STRAIGHTFACECOUNT	(TICRATE/2)
#define ST_TURNCOUNT		(1*TICRATE)
#define ST_OUCHCOUNT		(1*TICRATE)
#define ST_RAMPAGEDELAY		(2*TICRATE)

#define ST_MUCHPAIN			20

// Layout, in pixels.
#define ST_AMMOX		1
#define ST_HEALTHX		19
#define ST_ARMSX		42
#define ST_FACEX		68
#define ST_ARMORX		82
#define ST_KEYX			105
#define ST_NUMY			(ST_Y+2)
#define ST_BIGW			5


// main player in game
static player_t*	plyr; 

// used for timing
static unsigned int	st_clock;

// lump numbers of the graphics
static short		lu_bar;
static short		lu_tallnum;	// 0-9, minus, percent follow
static short		lu_tallminus;
static short		lu_tallpercent;
static short		lu_shortnum;
static short		lu_graynum;
static short		lu_keys;
static short*		faces;		// [ST_NUMFACES], -1 = missing

 // used to use appopriately pained face
static int	st_oldhealth = -1;

// used for evil grin
static boolean	oldweaponsowned[NUMWEAPONS]; 

 // count until face changes
static int	st_facecount = 0;

// current face index, used by w_faces
static int	st_faceindex = 0;

// a random number per tick
static int	st_randomnumber;  

static int	st_palette = 0;

// The status bar is not shown in the full screen view.
boolean		st_statusbaron;


//
// STATUS BAR CODE
//

// The cheats of the original, started from the options menu.
void ST_Cheat (int cheat)
{
    int		i;

    if (!plyr || gamestate != GS_LEVEL || plyr->playerstate != PST_LIVE)
	return;
    // "Can't cheat in nightmare"
    if (gameskill == sk_nightmare || demoplayback)
	return;

    switch (cheat)
    {
      case ST_CHEAT_GOD:	// 'dqd' cheat for toggleable god mode
	plyr->cheats ^= CF_GODMODE;
	if (plyr->cheats & CF_GODMODE)
	{
	  if (plyr->mo)
	    plyr->mo->health = 100;
	  
	  plyr->health = 100;
	  plyr->message = STSTR_DQDON;
	}
	else 
	  plyr->message = STSTR_DQDOFF;
	break;

      case ST_CHEAT_FA:		// 'fa' cheat for killer fucking arsenal
      case ST_CHEAT_KFA:	// 'kfa' cheat for key full ammo
	plyr->armorpoints = 200;
	plyr->armortype = 2;
	
	for (i=0;i<NUMWEAPONS;i++)
	  plyr->weaponowned[i] = true;

	// the shareware game has no plasma rifle / BFG, Doom 1 no SSG
	if (gamemode == shareware)
	  plyr->weaponowned[wp_plasma] = plyr->weaponowned[wp_bfg] = false;
	if (gamemode != commercial)
	  plyr->weaponowned[wp_supershotgun] = false;
	
	for (i=0;i<NUMAMMO;i++)
	  plyr->ammo[i] = plyr->maxammo[i];
	
	if (cheat == ST_CHEAT_KFA)
	{
	    for (i=0;i<NUMCARDS;i++)
		plyr->cards[i] = true;
	    plyr->message = STSTR_KFAADDED;
	}
	else
	    plyr->message = STSTR_FAADDED;
	break;

      case ST_CHEAT_NOCLIP:	// Simplified, accepting both "noclip" and "idspispopd".
	plyr->cheats ^= CF_NOCLIP;
	
	if (plyr->cheats & CF_NOCLIP)
	  plyr->message = STSTR_NCON;
	else
	  plyr->message = STSTR_NCOFF;
	break;

      case ST_CHEAT_MAP:	// 'dt': full automap
	AM_Cheat ();
	break;
    }
}

// Respond to keyboard input events: nothing left to do.
boolean
ST_Responder (event_t* ev)
{
  (void)ev;
  return false;
}



int ST_calcPainOffset(void)
{
    int		health;
    static int	lastcalc;
    static int	oldhealth = -1;
    
    health = plyr->health > 100 ? 100 : plyr->health;

    if (health != oldhealth)
    {
	lastcalc = ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
	oldhealth = health;
    }
    return lastcalc;
}


//
// This is a not-very-pretty routine which handles
//  the face states and their timing.
// the precedence of expressions is:
//  dead > evil grin > turned head > straight ahead
//
void ST_updateFaceWidget(void)
{
    int		i;
    angle_t	badguyangle;
    angle_t	diffang;
    static int	lastattackdown = -1;
    static int	priority = 0;
    boolean	doevilgrin;

    if (priority < 10)
    {
	// dead
	if (!plyr->health)
	{
	    priority = 9;
	    st_faceindex = ST_DEADFACE;
	    st_facecount = 1;
	}
    }

    if (priority < 9)
    {
	if (plyr->bonuscount)
	{
	    // picking up bonus
	    doevilgrin = false;

	    for (i=0;i<NUMWEAPONS;i++)
	    {
		if (oldweaponsowned[i] != plyr->weaponowned[i])
		{
		    doevilgrin = true;
		    oldweaponsowned[i] = plyr->weaponowned[i];
		}
	    }
	    if (doevilgrin) 
	    {
		// evil grin if just picked up weapon
		priority = 8;
		st_facecount = ST_EVILGRINCOUNT;
		st_faceindex = ST_calcPainOffset() + ST_EVILGRINOFFSET;
	    }
	}

    }
  
    if (priority < 8)
    {
	if (plyr->damagecount
	    && plyr->attacker
	    && plyr->attacker != plyr->mo)
	{
	    // being attacked
	    priority = 7;
	    
	    if (plyr->health - st_oldhealth > ST_MUCHPAIN)
	    {
		st_facecount = ST_TURNCOUNT;
		st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
	    }
	    else
	    {
		badguyangle = R_PointToAngle2(plyr->mo->x,
					      plyr->mo->y,
					      plyr->attacker->x,
					      plyr->attacker->y);
		
		if (badguyangle > plyr->mo->angle)
		{
		    // whether right or left
		    diffang = badguyangle - plyr->mo->angle;
		    i = diffang > ANG180; 
		}
		else
		{
		    // whether left or right
		    diffang = plyr->mo->angle - badguyangle;
		    i = diffang <= ANG180; 
		} // confusing, aint it?

		
		st_facecount = ST_TURNCOUNT;
		st_faceindex = ST_calcPainOffset();
		
		if (diffang < ANG45)
		{
		    // head-on    
		    st_faceindex += ST_RAMPAGEOFFSET;
		}
		else if (i)
		{
		    // turn face right
		    st_faceindex += ST_TURNOFFSET;
		}
		else
		{
		    // turn face left
		    st_faceindex += ST_TURNOFFSET+1;
		}
	    }
	}
    }
  
    if (priority < 7)
    {
	// getting hurt because of your own damn stupidity
	if (plyr->damagecount)
	{
	    if (plyr->health - st_oldhealth > ST_MUCHPAIN)
	    {
		priority = 7;
		st_facecount = ST_TURNCOUNT;
		st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
	    }
	    else
	    {
		priority = 6;
		st_facecount = ST_TURNCOUNT;
		st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
	    }

	}

    }
  
    if (priority < 6)
    {
	// rapid firing
	if (plyr->attackdown)
	{
	    if (lastattackdown==-1)
		lastattackdown = ST_RAMPAGEDELAY;
	    else if (!--lastattackdown)
	    {
		priority = 5;
		st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
		st_facecount = 1;
		lastattackdown = 1;
	    }
	}
	else
	    lastattackdown = -1;

    }
  
    if (priority < 5)
    {
	// invulnerability
	if ((plyr->cheats & CF_GODMODE)
	    || plyr->powers[pw_invulnerability])
	{
	    priority = 4;

	    st_faceindex = ST_GODFACE;
	    st_facecount = 1;

	}

    }

    // look left or look right if the facecount has timed out
    if (!st_facecount)
    {
	st_faceindex = ST_calcPainOffset() + (st_randomnumber % 3);
	st_facecount = ST_STRAIGHTFACECOUNT;
	priority = 0;
    }

    st_facecount--;

}

void ST_updateWidgets(void)
{
    // refresh everything if this is him coming back to life
    ST_updateFaceWidget();

    // used by the w_armsbg widget
    st_statusbaron = !(viewheight == SCREENHEIGHT) || automapactive;

}

void ST_Ticker (void)
{
    st_clock++;
    st_randomnumber = M_Random();
    ST_updateWidgets();
    st_oldhealth = plyr->health;
}

void ST_doPaletteStuff(void)
{

    int		palette;
    int		cnt;
    int		bzc;

    cnt = plyr->damagecount;

    if (plyr->powers[pw_strength])
    {
	// slowly fade the berzerk out
  	bzc = 12 - (plyr->powers[pw_strength]>>6);

	if (bzc > cnt)
	    cnt = bzc;
    }
	
    if (cnt)
    {
	palette = (cnt+7)>>3;
	
	if (palette >= NUMREDPALS)
	    palette = NUMREDPALS-1;

	palette += STARTREDPALS;
    }

    else if (plyr->bonuscount)
    {
	palette = (plyr->bonuscount+7)>>3;

	if (palette >= NUMBONUSPALS)
	    palette = NUMBONUSPALS-1;

	palette += STARTBONUSPALS;
    }

    else if ( plyr->powers[pw_ironfeet] > 4*32
	      || plyr->powers[pw_ironfeet]&8)
	palette = RADIATIONPAL;
    else
	palette = 0;

    if (palette != st_palette)
    {
	st_palette = palette;
	I_SetPalette (palette);
    }

}


// Right aligned number, up to three digits.
static void ST_DrawNum (int x, int num, boolean percent)
{
    int		pos = x + 2*ST_BIGW;
    boolean	neg = num < 0;

    if (percent)
	V_DrawPatch (x + 3*ST_BIGW, ST_NUMY, 0,
		     W_CacheLumpNum (lu_tallpercent, PU_CACHE));
    if (neg)
	num = -num;
    if (num > 999)
	num = 999;
    do
    {
	V_DrawPatch (pos, ST_NUMY, 0,
		     W_CacheLumpNum (lu_tallnum + num % 10, PU_CACHE));
	pos -= ST_BIGW;
	num /= 10;
    } while (num && pos >= x);
    if (neg && pos >= x)
	V_DrawPatch (pos, ST_NUMY, 0, W_CacheLumpNum (lu_tallminus, PU_CACHE));
}

void ST_drawWidgets(void)
{
    int		i;
    int		ammo;

    V_DrawPatch (0, ST_Y, 0, W_CacheLumpNum (lu_bar, PU_CACHE));

    // ready weapon ammo
    ammo = weaponinfo[plyr->readyweapon].ammo;
    if (ammo != am_noammo)
	ST_DrawNum (ST_AMMOX, plyr->ammo[ammo], false);

    ST_DrawNum (ST_HEALTHX, plyr->health, true);
    ST_DrawNum (ST_ARMORX, plyr->armorpoints, true);

    // weapons owned: 2 3 4 5 6 7
    for (i=0 ; i<6 ; i++)
	V_DrawPatch (ST_ARMSX + 4*i, ST_Y+3, 0,
		     W_CacheLumpNum ((plyr->weaponowned[i+1] ? lu_shortnum
				      : lu_graynum) + i + 2, PU_CACHE));

    // face
    if (faces[st_faceindex] >= 0)
	V_DrawPatch (ST_FACEX, ST_Y+1, 0,
		     W_CacheLumpNum (faces[st_faceindex], PU_CACHE));

    // keys: blue, yellow, red from top to bottom; skulls look different
    for (i=0 ; i<3 ; i++)
    {
	if (plyr->cards[i+3])
	    V_DrawPatch (ST_KEYX, ST_Y+2+3*i, 0,
			 W_CacheLumpNum (lu_keys + i + 3, PU_CACHE));
	else if (plyr->cards[i])
	    V_DrawPatch (ST_KEYX, ST_Y+2+3*i, 0,
			 W_CacheLumpNum (lu_keys + i, PU_CACHE));
    }
}

void ST_Drawer (void)
{
    // Do red-/gold-shifts from damage/items
    ST_doPaletteStuff();

    st_statusbaron = !(viewheight == SCREENHEIGHT) || automapactive;
    if (st_statusbaron)
	ST_drawWidgets ();
}

static int ST_FaceLump (const char* name)
{
    return W_CheckNumForName (name);
}

void ST_loadGraphics(void)
{

    int		i;
    int		j;
    int		facenum;
    
    char	namebuf[9];

    // The number lumps are consecutive in the sorted directory.
    lu_bar = W_GetNumForName ("STBAR");
    lu_tallnum = W_GetNumForName ("STTNUM0");
    lu_tallminus = W_GetNumForName ("STTMINUS");
    lu_tallpercent = W_GetNumForName ("STTPRCNT");
    lu_shortnum = W_GetNumForName ("STYSNUM0");
    lu_graynum = W_GetNumForName ("STGNUM0");
    lu_keys = W_GetNumForName ("STKEYS0");

    // face states
    faces = Z_Malloc (ST_NUMFACES*sizeof(short), PU_STATIC, 0);
    facenum = 0;
    for (i=0;i<ST_NUMPAINFACES;i++)
    {
	for (j=0;j<ST_NUMSTRAIGHTFACES;j++)
	{
	    sprintf(namebuf, "STFST%d%d", i, j);
	    faces[facenum++] = ST_FaceLump (namebuf);
	}
	sprintf(namebuf, "STFTR%d0", i);	// turn right
	faces[facenum++] = ST_FaceLump (namebuf);
	sprintf(namebuf, "STFTL%d0", i);	// turn left
	faces[facenum++] = ST_FaceLump (namebuf);
	sprintf(namebuf, "STFOUCH%d", i);	// ouch!
	faces[facenum++] = ST_FaceLump (namebuf);
	sprintf(namebuf, "STFEVL%d", i);	// evil grin ;)
	faces[facenum++] = ST_FaceLump (namebuf);
	sprintf(namebuf, "STFKILL%d", i);	// pissed off
	faces[facenum++] = ST_FaceLump (namebuf);
    }
    faces[facenum++] = ST_FaceLump ("STFGOD0");
    faces[facenum++] = ST_FaceLump ("STFDEAD0");
}

void ST_initData(void)
{

    int		i;

    plyr = &players[consoleplayer];

    st_clock = 0;
    st_statusbaron = true;

    st_faceindex = 0;
    st_palette = -1;

    st_oldhealth = -1;

    for (i=0;i<NUMWEAPONS;i++)
	oldweaponsowned[i] = plyr->weaponowned[i];
}

void ST_Start (void)
{
    ST_initData();
}

void ST_Stop (void)
{
    I_SetPalette (0);
}

void ST_Init (void)
{
    ST_loadGraphics();
}
