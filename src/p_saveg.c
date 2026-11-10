//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	Archiving: SaveGame I/O.
//
//  CasioDoom: there is no RAM for a save buffer, so the game is streamed
//  to the file through a small buffer. Structures are written as they are
//  in memory, with pointers replaced by indices.
//
//-----------------------------------------------------------------------------

#include <string.h>

#include "i_system.h"
#include "z_zone.h"
#include "p_local.h"

// State.
#include "doomstat.h"
#include "r_state.h"
#include "p_saveg.h"

#define SVBUFSIZE	256

static byte*	svbuf;
static int	svpos;
static int	svlen;
static boolean	sverror;


//
// Stream helpers
//
void SV_Begin (void)
{
    svbuf = Z_Malloc (SVBUFSIZE, PU_STATIC, 0);
    svpos = svlen = 0;
    sverror = false;
}

static void SV_Flush (void)
{
    if (svpos && I_FileWrite (svbuf, svpos) != svpos)
	sverror = true;
    svpos = 0;
}

// Returns false if anything went wrong.
boolean SV_End (boolean writing)
{
    if (writing)
	SV_Flush ();
    Z_Free (svbuf);
    return !sverror;
}

void SV_Write (const void* data, int len)
{
    const byte*	p = data;
    int		n;

    while (len)
    {
	n = SVBUFSIZE - svpos;
	if (n > len)
	    n = len;
	memcpy (svbuf + svpos, p, n);
	svpos += n;
	p += n;
	len -= n;
	if (svpos == SVBUFSIZE)
	    SV_Flush ();
    }
}

void SV_Read (void* data, int len)
{
    byte*	p = data;
    int		n;

    while (len)
    {
	if (svpos == svlen)
	{
	    svlen = I_FileRead (svbuf, SVBUFSIZE);
	    svpos = 0;
	    if (svlen <= 0)
	    {
		svlen = 0;
		sverror = true;
		memset (p, 0, len);
		return;
	    }
	}
	n = svlen - svpos;
	if (n > len)
	    n = len;
	memcpy (p, svbuf + svpos, n);
	svpos += n;
	p += n;
	len -= n;
    }
}

void SV_WriteByte (int v)
{
    byte	b = v;
    SV_Write (&b, 1);
}

int SV_ReadByte (void)
{
    byte	b;
    SV_Read (&b, 1);
    return b;
}

static void SV_WriteInt (int v)
{
    SV_Write (&v, sizeof(v));
}

static int SV_ReadInt (void)
{
    int		v;
    SV_Read (&v, sizeof(v));
    return v;
}


//
// P_ArchivePlayers
//
void P_ArchivePlayers (void)
{
    int		i;
    int		j;
    player_t	p;
		
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (!playeringame[i])
	    continue;
	
	p = players[i];
	for (j=0 ; j<NUMPSPRITES ; j++)
	{
	    if (p.psprites[j].state)
	    {
		p.psprites[j].state 
		    = (state_t *)(size_t)(p.psprites[j].state-states);
	    }
	}
	SV_Write (&p, sizeof(p));
    }
}



//
// P_UnArchivePlayers
//
void P_UnArchivePlayers (void)
{
    int		i;
    int		j;
	
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (!playeringame[i])
	    continue;
	
	SV_Read (&players[i], sizeof(player_t));
	
	// will be set when unarc thinker
	players[i].mo = NULL;	
	players[i].message = NULL;
	players[i].attacker = NULL;

	for (j=0 ; j<NUMPSPRITES ; j++)
	{
	    if (players[i]. psprites[j].state)
	    {
		players[i]. psprites[j].state 
		    = &states[ (size_t)players[i].psprites[j].state ];
	    }
	}
    }
}


//
// P_ArchiveWorld
//
void P_ArchiveWorld (void)
{
    int		i;

    SV_WriteInt (numsectors);
    SV_WriteInt (numlines);
    SV_WriteInt (numsides);
    SV_Write (sectors, numsectors*sizeof(sector_t));
    SV_Write (linedata, numlines*sizeof(linedata_t));
    SV_Write (sideswitched, (numsides+7)/8);

    // buttons that are still counting down
    for (i=0 ; i<MAXBUTTONS ; i++)
    {
	SV_WriteInt (buttonlist[i].btimer
		     ? (int)(buttonlist[i].line - lines) : -1);
	SV_WriteInt (buttonlist[i].btimer);
	SV_WriteInt (buttonlist[i].restore);
    }
}



//
// P_UnArchiveWorld
//
void P_UnArchiveWorld (void)
{
    int		i;
    int		line;

    if (SV_ReadInt () != numsectors
	|| SV_ReadInt () != numlines
	|| SV_ReadInt () != numsides)
	I_Error ("Savegame is from a different DOOM.CWD");

    SV_Read (sectors, numsectors*sizeof(sector_t));
    for (i=0 ; i<numsectors ; i++)
    {
	sectors[i].specialdata = 0;
	sectors[i].soundtarget = 0;
	sectors[i].thinglist = 0;
	sectors[i].validcount = 0;
    }
    SV_Read (linedata, numlines*sizeof(linedata_t));
    for (i=0 ; i<numlines ; i++)
	linedata[i].validcount = 0;
    SV_Read (sideswitched, (numsides+7)/8);

    for (i=0 ; i<MAXBUTTONS ; i++)
    {
	line = SV_ReadInt ();
	buttonlist[i].btimer = SV_ReadInt ();
	buttonlist[i].restore = SV_ReadInt ();
	if (line < 0 || line >= numlines)
	    buttonlist[i].btimer = 0;
	else
	    buttonlist[i].line = &lines[line];
    }
}





//
// Thinkers
//
typedef enum
{
    tc_end,
    tc_mobj,
    tc_static

} thinkerclass_t;



//
// P_ArchiveThinkers
//
void P_ArchiveThinkers (void)
{
    thinker_t*		th;
    mobj_t		mobj;
	
    // save off the current thinkers
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
	if (th->function.acp1 == (actionf_p1)P_StaticThinker)
	{
	    SV_WriteByte (tc_static);
	    memcpy (&mobj, th, MOBJ_STATICSIZE);
	    mobj.state = (state_t *)(size_t)(mobj.state - states);
	    SV_Write (&mobj, MOBJ_STATICSIZE);
	    continue;
	}

	if (th->function.acp1 == (actionf_p1)P_MobjThinker)
	{
	    SV_WriteByte (tc_mobj);
	    mobj = *(mobj_t *)th;
	    mobj.state = (state_t *)(size_t)(mobj.state - states);
	    
	    if (mobj.player)
		mobj.player = (player_t *)(size_t)((mobj.player-players) + 1);
	    SV_Write (&mobj, sizeof(mobj));
	    continue;
	}
		
	// I_Error ("P_ArchiveThinkers: Unknown thinker function");
    }

    // add a terminating marker
    SV_WriteByte (tc_end);
}



//
// P_UnArchiveThinkers
//
void P_UnArchiveThinkers (void)
{
    byte		tclass;
    thinker_t*		currentthinker;
    thinker_t*		next;
    mobj_t*		mobj;
    
    // remove all the current thinkers
    currentthinker = thinkercap.next;
    while (currentthinker != &thinkercap)
    {
	next = currentthinker->next;
	
	if (P_IsMobjThinker (currentthinker))
	    P_RemoveMobj ((mobj_t *)currentthinker);
	else
	    Z_Free (currentthinker);

	currentthinker = next;
    }
    P_InitThinkers ();
    
    // read in saved thinkers
    while (1)
    {
	tclass = SV_ReadByte ();
	switch (tclass)
	{
	  case tc_end:
	    return; 	// end of list
			
	  case tc_mobj:
	    mobj = Z_Malloc (sizeof(*mobj), PU_LEVEL, NULL);
	    SV_Read (mobj, sizeof(*mobj));
	    mobj->state = &states[(size_t)mobj->state];
	    mobj->target = NULL;
	    mobj->tracer = NULL;
	    if (mobj->player)
	    {
		mobj->player = &players[(size_t)mobj->player-1];
		mobj->player->mo = mobj;
	    }
	    P_SetThingPosition (mobj);
	    mobj->info = &mobjinfo[mobj->type];
	    mobj->floorz = SS_SECTOR(mobj->subsector)->floorheight;
	    mobj->ceilingz = SS_SECTOR(mobj->subsector)->ceilingheight;
	    mobj->thinker.function.acp1 = (actionf_p1)P_MobjThinker;
	    P_AddThinker (&mobj->thinker);
	    break;
			
	  case tc_static:
	    mobj = Z_Malloc (MOBJ_STATICSIZE, PU_LEVEL, NULL);
	    SV_Read (mobj, MOBJ_STATICSIZE);
	    mobj->state = &states[(size_t)mobj->state];
	    P_SetThingPosition (mobj);
	    mobj->info = &mobjinfo[mobj->type];
	    mobj->thinker.function.acp1 = (actionf_p1)P_StaticThinker;
	    P_AddThinker (&mobj->thinker);
	    break;
			
	  default:
	    I_Error ("Bad savegame (thinker %i)",tclass);
	}
	
    }

}


//
// P_ArchiveSpecials
//
enum
{
    tc_ceiling,
    tc_door,
    tc_floor,
    tc_plat,
    tc_flash,
    tc_strobe,
    tc_glow,
    tc_fire,
    tc_endspecials

} specials_e;	

// All of these structures start with a thinker followed by fields, one
// of which is the sector pointer; it is stored as an index.
#define ARCHIVE(type, var, tc) \
    { \
	type var = *(type *)th; \
	SV_WriteByte (tc); \
	var.sector = (sector_t *)(size_t)(var.sector - sectors); \
	SV_Write (&var, sizeof(var)); \
    }

#define UNARCHIVE(type, var, func) \
    type* var = Z_Malloc (sizeof(type), PU_LEVSPEC, NULL); \
    SV_Read (var, sizeof(type)); \
    var->sector = &sectors[(size_t)var->sector]; \
    var->thinker.function.acp1 = (actionf_p1)func; \
    P_AddThinker (&var->thinker);


//
// Things to handle:
//
// T_MoveCeiling, (ceiling_t: sector_t * swizzle), - active list
// T_VerticalDoor, (vldoor_t: sector_t * swizzle),
// T_MoveFloor, (floormove_t: sector_t * swizzle),
// T_LightFlash, (lightflash_t: sector_t * swizzle),
// T_StrobeFlash, (strobe_t: sector_t *),
// T_Glow, (glow_t: sector_t *),
// T_FireFlicker, (fireflicker_t: sector_t *),
// T_PlatRaise, (plat_t: sector_t *), - active list
//
void P_ArchiveSpecials (void)
{
    thinker_t*		th;
    int			i;
	
    // save off the current thinkers
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
	if (th->function.acv == (actionf_v)NULL)
	{
	    // an in-stasis ceiling or plat has no thinker function;
	    // the flag byte after the class tells them apart on load
	    for (i = 0; i < MAXCEILINGS;i++)
		if (activeceilings[i] == (ceiling_t *)th)
		    break;
	    
	    if (i<MAXCEILINGS)
	    {
		ARCHIVE (ceiling_t, ceiling, tc_ceiling);
		SV_WriteByte (0);
		continue;
	    }

	    for (i = 0; i < MAXPLATS;i++)
		if (activeplats[i] == (plat_t *)th)
		    break;

	    if (i<MAXPLATS)
	    {
		ARCHIVE (plat_t, plat, tc_plat);
		SV_WriteByte (0);
	    }
	    continue;
	}
			
	if (th->function.acp1 == (actionf_p1)T_MoveCeiling)
	{
	    ARCHIVE (ceiling_t, ceiling, tc_ceiling);
	    SV_WriteByte (1);
	    continue;
	}
			
	if (th->function.acp1 == (actionf_p1)T_VerticalDoor)
	{
	    ARCHIVE (vldoor_t, door, tc_door);
	    continue;
	}
			
	if (th->function.acp1 == (actionf_p1)T_MoveFloor)
	{
	    ARCHIVE (floormove_t, floor, tc_floor);
	    continue;
	}
			
	if (th->function.acp1 == (actionf_p1)T_PlatRaise)
	{
	    ARCHIVE (plat_t, plat, tc_plat);
	    SV_WriteByte (1);
	    continue;
	}
			
	if (th->function.acp1 == (actionf_p1)T_LightFlash)
	{
	    ARCHIVE (lightflash_t, flash, tc_flash);
	    continue;
	}
			
	if (th->function.acp1 == (actionf_p1)T_StrobeFlash)
	{
	    ARCHIVE (strobe_t, strobe, tc_strobe);
	    continue;
	}
			
	if (th->function.acp1 == (actionf_p1)T_Glow)
	{
	    ARCHIVE (glow_t, glow, tc_glow);
	    continue;
	}

	if (th->function.acp1 == (actionf_p1)T_FireFlicker)
	{
	    ARCHIVE (fireflicker_t, fire, tc_fire);
	    continue;
	}
    }
	
    // add a terminating marker
    SV_WriteByte (tc_endspecials);

}


//
// P_UnArchiveSpecials
//
void P_UnArchiveSpecials (void)
{
    byte		tclass;
	
    // read in saved thinkers
    while (1)
    {
	tclass = SV_ReadByte ();
	switch (tclass)
	{
	  case tc_endspecials:
	    return;	// end of list
			
	  case tc_ceiling:
	  {
	    UNARCHIVE (ceiling_t, ceiling, T_MoveCeiling);
	    ceiling->sector->specialdata = ceiling;
	    if (!SV_ReadByte ())
		ceiling->thinker.function.acv = (actionf_v)NULL;
	    P_AddActiveCeiling(ceiling);
	    break;
	  }
				
	  case tc_door:
	  {
	    UNARCHIVE (vldoor_t, door, T_VerticalDoor);
	    door->sector->specialdata = door;
	    break;
	  }
				
	  case tc_floor:
	  {
	    UNARCHIVE (floormove_t, floor, T_MoveFloor);
	    floor->sector->specialdata = floor;
	    break;
	  }
				
	  case tc_plat:
	  {
	    UNARCHIVE (plat_t, plat, T_PlatRaise);
	    plat->sector->specialdata = plat;
	    if (!SV_ReadByte ())
		plat->thinker.function.acv = (actionf_v)NULL;
	    P_AddActivePlat(plat);
	    break;
	  }
				
	  case tc_flash:
	  {
	    UNARCHIVE (lightflash_t, flash, T_LightFlash);
	    break;
	  }
				
	  case tc_strobe:
	  {
	    UNARCHIVE (strobe_t, strobe, T_StrobeFlash);
	    break;
	  }
				
	  case tc_glow:
	  {
	    UNARCHIVE (glow_t, glow, T_Glow);
	    break;
	  }

	  case tc_fire:
	  {
	    UNARCHIVE (fireflicker_t, fire, T_FireFlicker);
	    break;
	  }
				
	  default:
	    I_Error ("Bad savegame (special %i)",tclass);
	}
	
    }

}
