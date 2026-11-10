// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	Game completion, final screen animation.
//
//-----------------------------------------------------------------------------



#include <ctype.h>

// Functions.
#include "i_system.h"
#include "m_swap.h"
#include "z_zone.h"
#include "v_video.h"
#include "w_wad.h"
#include "s_sound.h"

// Data.
#include "dstrings.h"
#include "sounds.h"

#include "doomstat.h"
#include "r_state.h"

// ?
//#include "doomstat.h"
//#include "r_local.h"
//#include "f_finale.h"

// Stage of animation:
//  0 = text, 1 = art screen, 2 = character cast
int		finalestage;

int		finalecount;

#define	TEXTSPEED	3
#define	TEXTWAIT	250

static const char e1text[] = E1TEXT;
static const char e2text[] = E2TEXT;
static const char e3text[] = E3TEXT;
static const char e4text[] = E4TEXT;

static const char c1text[] = C1TEXT;
static const char c2text[] = C2TEXT;
static const char c3text[] = C3TEXT;
static const char c4text[] = C4TEXT;
static const char c5text[] = C5TEXT;
static const char c6text[] = C6TEXT;



static const char*	finaletext;

void	F_StartCast (void);
void	F_CastTicker (void);
boolean F_CastResponder (event_t *ev);
void	F_CastDrawer (void);

// A full screen picture, if the WAD has it.
static void F_Page (const char* name)
{
    V_FillRect (0, 0, SCREENWIDTH, SCREENHEIGHT, V_WHITE);
    if (W_CheckNumForName (name) >= 0)
	V_DrawPatch (0, 0, 0, W_CacheLumpName (name, PU_CACHE));
}

//
// F_StartFinale
//
void F_StartFinale (void)
{
    gameaction = ga_nothing;
    gamestate = GS_FINALE;
    viewactive = false;
    automapactive = false;

    // Okay - IWAD dependend stuff.
    // This has been changed severly, and
    //  some stuff might have changed in the process.
    switch ( gamemode )
    {

      // DOOM 1 - E1, E3 or E4, but each nine missions
      case shareware:
      case registered:
      case retail:
      {
	S_ChangeMusic(mus_victor, true);
	
	switch (gameepisode)
	{
	  case 1:finaletext = e1text;
	    break;
	  case 2:finaletext = e2text;
	    break;
	  case 3:finaletext = e3text;
	    break;
	  case 4:finaletext = e4text;
	    break;
	  default:
	    // Ouch.
	    break;
	}
	break;
      }
      
      // DOOM II and missions packs with E1, M34
      case commercial:
      {
	  S_ChangeMusic(mus_read_m, true);

	  switch (gamemap)
	  {
	    case 6:finaletext = c1text;
	      break;
	    case 11:finaletext = c2text;
	      break;
	    case 20:finaletext = c3text;
	      break;
	    case 30:finaletext = c4text;
	      break;
	    case 15:finaletext = c5text;
	      break;
	    case 31:finaletext = c6text;
	      break;
	    default:
	      // Ouch.
	      break;
	  }
	  break;
      }	

   
      // Indeterminate.
      default:
	S_ChangeMusic(mus_read_m, true);
	finaletext = c1text;  // FIXME - other text, music?
	break;
    }
    
    finalestage = 0;
    finalecount = 0;
	
}



boolean F_Responder (event_t *event)
{
    if (finalestage == 2)
	return F_CastResponder (event);
	
    return false;
}


//
// F_Ticker
//
void F_Ticker (void)
{
    int		i;
    
    // check for skipping
    if ( (gamemode == commercial)
      && ( finalecount > 50) )
    {
      // go on to the next level
      for (i=0 ; i<MAXPLAYERS ; i++)
	if (players[i].cmd.buttons)
	  break;
				
      if (i < MAXPLAYERS)
      {	
	if (gamemap == 30)
	  F_StartCast ();
	else
	  gameaction = ga_worlddone;
      }
    }
    
    // advance animation
    finalecount++;
	
    if (finalestage == 2)
    {
	F_CastTicker ();
	return;
    }
	
    if ( gamemode == commercial)
	return;
		
    if (!finalestage && finalecount>strlen (finaletext)*TEXTSPEED + TEXTWAIT)
    {
	finalecount = 0;
	finalestage = 1;
	wipegamestate = -1;		// force a wipe
	if (gameepisode == 3)
	    S_StartMusic (mus_bunny);
    }
}



//
// F_TextWrite
// CasioDoom: the text is word-wrapped to the 31 columns of the screen
// and scrolls up while it is being typed.
//
#include "hu_stuff.h"

#define TEXTCOLS	31
#define TEXTROWS	10

// Lays the first [count] characters out; draws the lines from
// [firstline] on if draw is set. Returns the number of lines used.
static int F_TextLayout (int count, int firstline, boolean draw)
{
    const char*	ch = finaletext;
    int		col = 0;
    int		line = 0;
    int		c;
    int		wordlen;

    while (count-- > 0 && (c = *ch++) != 0)
    {
	if (c == '\n')
	{
	    col = 0;
	    line++;
	    continue;
	}
	if (c == ' ')
	{
	    // does the next word still fit on this line?
	    for (wordlen = 0 ; ch[wordlen] && ch[wordlen] != ' '
		     && ch[wordlen] != '\n' ; wordlen++)
		;
	    if (col + 1 + wordlen > TEXTCOLS)
	    {
		col = 0;
		line++;
		continue;
	    }
	}
	if (col >= TEXTCOLS)
	{
	    col = 0;
	    line++;
	}
	if (draw && line >= firstline && c != ' ')
	    HU_DrawChar (2 + col*HU_CHARW, 2 + (line-firstline)*HU_CHARH, c);
	col++;
    }
    return line + 1;
}

void F_TextWrite (void)
{
    int		count;
    int		lines;
    
    // erase the entire screen to a plain background
    V_FillRect (0, 0, SCREENWIDTH, SCREENHEIGHT, V_WHITE);

    // draw some of the text onto the screen
    count = (finalecount - 10)/TEXTSPEED;
    if (count < 0)
	count = 0;
    lines = F_TextLayout (count, 0, false);
    F_TextLayout (count, lines > TEXTROWS ? lines - TEXTROWS : 0, true);
}

//
// Final DOOM 2 animation
// Casting by id Software.
//   in order of appearance
//
typedef struct
{
    const char		*name;
    mobjtype_t	type;
} castinfo_t;

static const castinfo_t	castorder[] = {
    {CC_ZOMBIE, MT_POSSESSED},
    {CC_SHOTGUN, MT_SHOTGUY},
    {CC_HEAVY, MT_CHAINGUY},
    {CC_IMP, MT_TROOP},
    {CC_DEMON, MT_SERGEANT},
    {CC_LOST, MT_SKULL},
    {CC_CACO, MT_HEAD},
    {CC_HELL, MT_KNIGHT},
    {CC_BARON, MT_BRUISER},
    {CC_ARACH, MT_BABY},
    {CC_PAIN, MT_PAIN},
    {CC_REVEN, MT_UNDEAD},
    {CC_MANCU, MT_FATSO},
    {CC_ARCH, MT_VILE},
    {CC_SPIDER, MT_SPIDER},
    {CC_CYBER, MT_CYBORG},
    {CC_HERO, MT_PLAYER},

    {NULL,0}
};

int		castnum;
int		casttics;
const state_t*	caststate;
boolean		castdeath;
int		castframes;
int		castonmelee;
boolean		castattacking;


//
// F_StartCast
//
extern	gamestate_t     wipegamestate;


void F_StartCast (void)
{
    wipegamestate = -1;		// force a screen wipe
    castnum = 0;
    caststate = &states[mobjinfo[castorder[castnum].type].seestate];
    casttics = caststate->tics;
    castdeath = false;
    finalestage = 2;	
    castframes = 0;
    castonmelee = 0;
    castattacking = false;
    S_ChangeMusic(mus_evil, true);
}


//
// F_CastTicker
//
void F_CastTicker (void)
{
    int		st;
    int		sfx;
	
    if (--casttics > 0)
	return;			// not time to change state yet
		
    if (caststate->tics == -1 || caststate->nextstate == S_NULL)
    {
	// switch from deathstate to next monster
	castnum++;
	castdeath = false;
	if (castorder[castnum].name == NULL)
	    castnum = 0;
	if (mobjinfo[castorder[castnum].type].seesound)
	    S_StartSound (NULL, mobjinfo[castorder[castnum].type].seesound);
	caststate = &states[mobjinfo[castorder[castnum].type].seestate];
	castframes = 0;
    }
    else
    {
	// just advance to next state in animation
	if (caststate == &states[S_PLAY_ATK1])
	    goto stopattack;	// Oh, gross hack!
	st = caststate->nextstate;
	caststate = &states[st];
	castframes++;
	
	// sound hacks....
	switch (st)
	{
	  case S_PLAY_ATK1:	sfx = sfx_dshtgn; break;
	  case S_POSS_ATK2:	sfx = sfx_pistol; break;
	  case S_SPOS_ATK2:	sfx = sfx_shotgn; break;
	  case S_VILE_ATK2:	sfx = sfx_vilatk; break;
	  case S_SKEL_FIST2:	sfx = sfx_skeswg; break;
	  case S_SKEL_FIST4:	sfx = sfx_skepch; break;
	  case S_SKEL_MISS2:	sfx = sfx_skeatk; break;
	  case S_FATT_ATK8:
	  case S_FATT_ATK5:
	  case S_FATT_ATK2:	sfx = sfx_firsht; break;
	  case S_CPOS_ATK2:
	  case S_CPOS_ATK3:
	  case S_CPOS_ATK4:	sfx = sfx_shotgn; break;
	  case S_TROO_ATK3:	sfx = sfx_claw; break;
	  case S_SARG_ATK2:	sfx = sfx_sgtatk; break;
	  case S_BOSS_ATK2:
	  case S_BOS2_ATK2:
	  case S_HEAD_ATK2:	sfx = sfx_firsht; break;
	  case S_SKULL_ATK2:	sfx = sfx_sklatk; break;
	  case S_SPID_ATK2:
	  case S_SPID_ATK3:	sfx = sfx_shotgn; break;
	  case S_BSPI_ATK2:	sfx = sfx_plasma; break;
	  case S_CYBER_ATK2:
	  case S_CYBER_ATK4:
	  case S_CYBER_ATK6:	sfx = sfx_rlaunc; break;
	  case S_PAIN_ATK3:	sfx = sfx_sklatk; break;
	  default: sfx = 0; break;
	}
		
	if (sfx)
	    S_StartSound (NULL, sfx);
    }
	
    if (castframes == 12)
    {
	// go into attack frame
	castattacking = true;
	if (castonmelee)
	    caststate=&states[mobjinfo[castorder[castnum].type].meleestate];
	else
	    caststate=&states[mobjinfo[castorder[castnum].type].missilestate];
	castonmelee ^= 1;
	if (caststate == &states[S_NULL])
	{
	    if (castonmelee)
		caststate=
		    &states[mobjinfo[castorder[castnum].type].meleestate];
	    else
		caststate=
		    &states[mobjinfo[castorder[castnum].type].missilestate];
	}
    }
	
    if (castattacking)
    {
	if (castframes == 24
	    ||	caststate == &states[mobjinfo[castorder[castnum].type].seestate] )
	{
	  stopattack:
	    castattacking = false;
	    castframes = 0;
	    caststate = &states[mobjinfo[castorder[castnum].type].seestate];
	}
    }
	
    casttics = caststate->tics;
    if (casttics == -1)
	casttics = 15;
}


//
// F_CastResponder
//

boolean F_CastResponder (event_t* ev)
{
    if (ev->type != ev_keydown)
	return false;
		
    if (castdeath)
	return true;			// already in dying frames
		
    // go into death frame
    castdeath = true;
    caststate = &states[mobjinfo[castorder[castnum].type].deathstate];
    casttics = caststate->tics;
    castframes = 0;
    castattacking = false;
    if (mobjinfo[castorder[castnum].type].deathsound)
	S_StartSound (NULL, mobjinfo[castorder[castnum].type].deathsound);
	
    return true;
}


//
// F_CastDrawer
//
void F_CastDrawer (void)
{
    const spritedef_t*		sprdef;
    const spriteframe_t*	sprframe;
    int				lump;
    const char*			name = castorder[castnum].name;
    
    // erase the entire screen to a background
    F_Page ("BOSSBACK");

    HU_DrawText ((SCREENWIDTH - (int)strlen (name)*HU_CHARW)/2,
		 SCREENHEIGHT - HU_CHARH - 1, name);
			
    // draw the current frame in the middle of the screen
    sprdef = &sprites[caststate->sprite];
    if ((caststate->frame & FF_FRAMEMASK) >= sprdef->numframes)
	return;
    sprframe = &spriteframes[sprdef->firstframe + (caststate->frame & FF_FRAMEMASK)];
    lump = sprframe->lump[0];
    if (lump == 0xffff)
	return;
			
    V_DrawSprite (SCREENWIDTH/2, SCREENHEIGHT - HU_CHARH - 3,
		  W_CacheLumpNum (lump, PU_CACHE), sprframe->flip & 1);
}


//
// F_BunnyScroll
//
void F_BunnyScroll (void)
{
    int		scrolled;
    char	name[10];
    int		stage;
		
    scrolled = SCREENWIDTH - (finalecount-230)/5;
    if (scrolled > SCREENWIDTH)
	scrolled = SCREENWIDTH;
    if (scrolled < 0)
	scrolled = 0;
		
    V_FillRect (0, 0, SCREENWIDTH, SCREENHEIGHT, V_WHITE);
    if (W_CheckNumForName ("PFUB2") >= 0)
	V_DrawPatch (-scrolled, 0, 0, W_CacheLumpName ("PFUB2", PU_CACHE));
    if (W_CheckNumForName ("PFUB1") >= 0)
	V_DrawPatch (SCREENWIDTH-scrolled, 0, 0, W_CacheLumpName ("PFUB1", PU_CACHE));
	
    if (finalecount < 1130)
	return;
    stage = 0;
    if (finalecount >= 1180)
    {
	stage = (finalecount-1180) / 5;
	if (stage > 6)
	    stage = 6;
    }
	
    sprintf (name,"END%i",stage);
    if (W_CheckNumForName (name) >= 0)
	V_DrawPatch ((SCREENWIDTH-42)/2, (SCREENHEIGHT-20)/2,0, W_CacheLumpName (name,PU_CACHE));
}


//
// F_Drawer
//
void F_Drawer (void)
{
    if (finalestage == 2)
    {
	F_CastDrawer ();
	return;
    }

    if (!finalestage)
	F_TextWrite ();
    else
    {
	switch (gameepisode)
	{
	  case 1:
	    F_Page ("CREDIT");
	    break;
	  case 2:
	    F_Page ("VICTORY2");
	    break;
	  case 3:
	    F_BunnyScroll ();
	    break;
	  case 4:
	    F_Page ("ENDPIC");
	    break;
	}
    }
			
}
