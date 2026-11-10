//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	Do all the WAD I/O, get map description,
//	set up initial state and misc. LUTs.
//
//  CasioDoom: the converter has already built every table, so "loading"
//  a level only means finding its lumps and creating the small RAM side
//  (sectors, line flags, block links).
//
//-----------------------------------------------------------------------------

#include <string.h>
#include <stdio.h>

#include "z_zone.h"
#include "m_swap.h"
#include "m_bbox.h"
#include "g_game.h"
#include "i_system.h"
#include "w_wad.h"
#include "doomdef.h"
#include "p_local.h"
#include "s_sound.h"
#include "doomstat.h"

void	P_SpawnMapThing (const mapthing_t* mthing, int index);

//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//
int			numvertexes;
const vertex_t*		vertexes;

int			numsegs;
const seg_t*		segs;

int			numsectors;
sector_t*		sectors;
const mapsec_t*		mapsecs;
const unsigned short*	seclines;

int			numsubsectors;
const subsector_t*	subsectors;

int			numnodes;
const node_t*		nodes;

int			numlines;
const line_t*		lines;
linedata_t*		linedata;

int			numsides;
const side_t*		sides;
// One bit per side: its switch texture is in the "pressed" state.
byte*			sideswitched;

int			nummapthings;
const mapthing_t*	mapthings;

// BLOCKMAP
// Created from axis aligned bounding box
// of the map, a rectangular array of
// blocks of size ...
// Used to speed up collision detection
// by spatial subdivision in 2D.
//
// Blockmap size.
int		bmapwidth;
int		bmapheight;	// size in mapblocks
const short*	blockmap;	// int for larger maps
// offsets in blockmap are from here
const short*	blockmaplump;		
// origin of block map
fixed_t		bmaporgx;
fixed_t		bmaporgy;
// for thing chains
mobj_t**	blocklinks;		

// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//  LineOf Sight calculation.
// Without special effect, this could be
//  used as a PVS lookup as well.
//
const byte*	rejectmatrix;

// Maintain single and multi player starting spots.
mapthing_t	playerstarts[MAXPLAYERS];


static const void* P_MapLump (const char* mapname, const char* suffix, int recsize, int* count)
{
    char	name[12];
    int		lump;

    sprintf (name, "%s%s", mapname, suffix);
    lump = W_GetNumForName (name);
    if (count)
	*count = W_LumpLength (lump) / recsize;
    return W_CacheLumpNum (lump, PU_LEVEL);
}


//
// P_LoadSectors
//
static void P_LoadSectors (void)
{
    int			i;
    const mapsec_t*	ms;
    sector_t*		ss;
	
    sectors = Z_Malloc (numsectors*sizeof(sector_t),PU_LEVEL,0);	
    memset (sectors, 0, numsectors*sizeof(sector_t));
    
    ms = mapsecs;
    ss = sectors;
    for (i=0 ; i<numsectors ; i++, ss++, ms++)
    {
	ss->floorheight = ms->floorheight<<FRACBITS;
	ss->ceilingheight = ms->ceilingheight<<FRACBITS;
	ss->floorpic = ms->floorpic;
	ss->ceilingpic = ms->ceilingpic;
	ss->lightlevel = ms->lightlevel;
	ss->special = ms->special;
	ss->thinglist = NULL;
    }
}


//
// P_LoadThings
//
static void P_LoadThings (void)
{
    int			i;
    const mapthing_t*	mt;
    boolean		spawn;

    mt = mapthings;
    for (i=0 ; i<nummapthings ; i++, mt++)
    {
	spawn = true;

	// Do not spawn cool, new monsters if !commercial
	if ( gamemode != commercial)
	{
	    switch(mt->type)
	    {
	      case 68:	// Arachnotron
	      case 64:	// Archvile
	      case 88:	// Boss Brain
	      case 89:	// Boss Shooter
	      case 69:	// Hell Knight
	      case 67:	// Mancubus
	      case 71:	// Pain Elemental
	      case 65:	// Former Human Commando
	      case 66:	// Revenant
	      case 84:	// Wolf SS
		spawn = false;
		break;
	    }
	}
	if (spawn == false)
	    break;

	P_SpawnMapThing (mt, i);
    }
}


//
// P_LoadLineData
//
static void P_LoadLineData (void)
{
    int			i;

    linedata = Z_Malloc (numlines*sizeof(linedata_t),PU_LEVEL,0);	
    memset (linedata, 0, numlines*sizeof(linedata_t));
    for (i=0 ; i<numlines ; i++)
	linedata[i].special = lines[i].special0;

    i = (numsides + 7) / 8;
    sideswitched = Z_Malloc (i,PU_LEVEL,0);
    memset (sideswitched, 0, i);
}


//
// P_LoadBlockMap
//
static void P_LoadBlockMap (const char* mapname)
{
    int		count;
	
    blockmaplump = P_MapLump (mapname, "BLK", 2, NULL);
    blockmap = blockmaplump+4;
		
    bmaporgx = blockmaplump[0]<<FRACBITS;
    bmaporgy = blockmaplump[1]<<FRACBITS;
    bmapwidth = blockmaplump[2];
    bmapheight = blockmaplump[3];
	
    // clear out mobj chains
    count = sizeof(*blocklinks)* bmapwidth*bmapheight;
    blocklinks = Z_Malloc (count,PU_LEVEL, 0);
    memset (blocklinks, 0, count);
}


//
// P_MapName
//
void P_MapName (int episode, int map, char* lumpname)
{
    if ( gamemode == commercial)
	sprintf (lumpname,"MAP%02i", map);
    else
    {
	lumpname[0] = 'E';
	lumpname[1] = '0' + episode;
	lumpname[2] = 'M';
	lumpname[3] = '0' + map;
	lumpname[4] = 0;
    }
}

boolean P_MapExists (int episode, int map)
{
    char	lumpname[12];

    P_MapName (episode, map, lumpname);
    strcat (lumpname, "VTX");
    return W_CheckNumForName (lumpname) >= 0;
}


//
// P_SetupLevel
//
void
P_SetupLevel
( int		episode,
  int		map,
  int		playermask,
  skill_t	skill)
{
    int		i;
    char	lumpname[9];
	
    totalkills = totalitems = totalsecret = wminfo.maxfrags = 0;
    wminfo.partime = 180;
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	players[i].killcount = players[i].secretcount 
	    = players[i].itemcount = 0;
    }

    // Initial height of PointOfView
    // will be set by player think.
    players[consoleplayer].viewz = 1; 

    Z_FreeTags (PU_LEVEL, PU_PURGELEVEL-1);

    P_InitThinkers ();
	   
    // find map name
    P_MapName (episode, map, lumpname);
    if (!P_MapExists (episode, map))
	I_Error ("Level %s is not in DOOM.CWD", lumpname);

    leveltime = 0;
	
    // note: most of this is ordered so that the RAM side comes first
    vertexes = P_MapLump (lumpname, "VTX", sizeof(vertex_t), &numvertexes);
    mapsecs = P_MapLump (lumpname, "SEC", sizeof(mapsec_t), &numsectors);
    seclines = P_MapLump (lumpname, "SLN", 2, NULL);
    sides = P_MapLump (lumpname, "SID", sizeof(side_t), &numsides);
    lines = P_MapLump (lumpname, "LIN", sizeof(line_t), &numlines);
    subsectors = P_MapLump (lumpname, "SSC", sizeof(subsector_t), &numsubsectors);
    nodes = P_MapLump (lumpname, "NOD", sizeof(node_t), &numnodes);
    segs = P_MapLump (lumpname, "SEG", sizeof(seg_t), &numsegs);
    rejectmatrix = P_MapLump (lumpname, "REJ", 1, NULL);
    mapthings = P_MapLump (lumpname, "THG", sizeof(mapthing_t), &nummapthings);

    P_LoadBlockMap (lumpname);
    P_LoadSectors ();
    P_LoadLineData ();

    bodyqueslot = 0;
    P_LoadThings ();
    
    // clear special respawning que
    iquehead = iquetail = 0;		
	
    // set up world state
    P_SpawnSpecials ();
}


//
// P_Init
//
void P_Init (void)
{
    P_InitIntercepts ();
    P_InitSwitchList ();
    P_InitPicAnims ();
    R_InitSprites ();
}
