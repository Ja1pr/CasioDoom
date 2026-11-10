//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:  Heads-up displays
//
//-----------------------------------------------------------------------------

#include <string.h>

#include "doomdef.h"

#include "z_zone.h"

#include "hu_stuff.h"
#include "w_wad.h"
#include "v_video.h"

#include "doomstat.h"

// Data.
#include "dstrings.h"


static player_t*	plr;
static int		fontlump;	// lump of '!'; the others follow

static const char*	message;	// currently shown
static const char*	lastmessage;
static int		message_counter;

extern int		showMessages;
extern boolean		automapactive;

boolean			message_dontfuckwithme;


//
// Builtin map names.
// The actual names can be found in DStrings.h.
//

static const char* const mapnames[] =	// DOOM shareware/registered/retail (Ultimate) names.
{

    HUSTR_E1M1,
    HUSTR_E1M2,
    HUSTR_E1M3,
    HUSTR_E1M4,
    HUSTR_E1M5,
    HUSTR_E1M6,
    HUSTR_E1M7,
    HUSTR_E1M8,
    HUSTR_E1M9,

    HUSTR_E2M1,
    HUSTR_E2M2,
    HUSTR_E2M3,
    HUSTR_E2M4,
    HUSTR_E2M5,
    HUSTR_E2M6,
    HUSTR_E2M7,
    HUSTR_E2M8,
    HUSTR_E2M9,

    HUSTR_E3M1,
    HUSTR_E3M2,
    HUSTR_E3M3,
    HUSTR_E3M4,
    HUSTR_E3M5,
    HUSTR_E3M6,
    HUSTR_E3M7,
    HUSTR_E3M8,
    HUSTR_E3M9,

    HUSTR_E4M1,
    HUSTR_E4M2,
    HUSTR_E4M3,
    HUSTR_E4M4,
    HUSTR_E4M5,
    HUSTR_E4M6,
    HUSTR_E4M7,
    HUSTR_E4M8,
    HUSTR_E4M9
};

static const char* const mapnames2[] =	// DOOM 2 map names.
{
    HUSTR_1,
    HUSTR_2,
    HUSTR_3,
    HUSTR_4,
    HUSTR_5,
    HUSTR_6,
    HUSTR_7,
    HUSTR_8,
    HUSTR_9,
    HUSTR_10,
    HUSTR_11,
	
    HUSTR_12,
    HUSTR_13,
    HUSTR_14,
    HUSTR_15,
    HUSTR_16,
    HUSTR_17,
    HUSTR_18,
    HUSTR_19,
    HUSTR_20,
	
    HUSTR_21,
    HUSTR_22,
    HUSTR_23,
    HUSTR_24,
    HUSTR_25,
    HUSTR_26,
    HUSTR_27,
    HUSTR_28,
    HUSTR_29,
    HUSTR_30,
    HUSTR_31,
    HUSTR_32
};


const char* HU_LevelNameFor (int episode, int map)
{
    if (gamemode == commercial)
    {
	if (map >= 1 && map <= 32)
	    return mapnames2[map-1];
    }
    else if (episode >= 1 && episode <= 4
	     && map >= 1 && map <= 9)
	return mapnames[(episode-1)*9+map-1];
    return "NEW LEVEL";
}

const char* HU_LevelName (void)
{
    return HU_LevelNameFor (gameepisode, gamemap);
}


void HU_Init(void)
{
    // The font lumps STCFN033..STCFN095 are consecutive in the
    // (sorted) directory.
    fontlump = W_GetNumForName ("STCFN033");
}


//
// Text drawing
//
void HU_DrawChar (int x, int y, int c)
{
    if (c >= 'a' && c <= 'z')
	c -= 32;
    if (c < HU_FONTSTART || c > HU_FONTEND)
	return;
    V_DrawPatch (x, y, 0, W_CacheLumpNum (fontlump + c - HU_FONTSTART, PU_CACHE));
}

void HU_DrawTextN (int x, int y, const char* string, int count)
{
    int		c;

    if (count <= 0)
	return;
    // one white line above and to the left keeps the text readable
    V_FillRect (x-1, y-1, count*HU_CHARW+1, HU_CHARH+1, V_WHITE);
    while (count-- && (c = *string++) != 0)
    {
	HU_DrawChar (x, y, c);
	x += HU_CHARW;
    }
}

void HU_DrawText (int x, int y, const char* string)
{
    const char*	end;

    while (*string)
    {
	end = string;
	while (*end && *end != '\n')
	    end++;
	HU_DrawTextN (x, y, string, end - string);
	y += HU_CHARH;
	string = end;
	if (*string == '\n')
	    string++;
    }
}


void HU_Start(void)
{
    plr = &players[consoleplayer];
    message = NULL;
    message_counter = 0;
    message_dontfuckwithme = false;
}

void HU_Drawer(void)
{
    const char*	s;
    int		len;
    int		y;
    int		n;

    if (message)
    {
	// word wrap the message over at most two lines
	s = message;
	y = 1;
	for (n=0 ; n<2 && *s ; n++)
	{
	    len = strlen (s);
	    if (len > HU_COLUMNS-1)
	    {
		len = HU_COLUMNS-1;
		while (len > 8 && s[len] != ' ')
		    len--;
	    }
	    HU_DrawTextN (1, y, s, len);
	    s += len;
	    while (*s == ' ')
		s++;
	    y += HU_CHARH;
	}
    }
    if (automapactive)
    {
	s = HU_LevelName ();
	len = strlen (s);
	if (len > HU_COLUMNS-1)
	    len = HU_COLUMNS-1;
	HU_DrawTextN (1, viewheight - HU_CHARH, s, len);
    }
}

void HU_Ticker(void)
{
    // tick down message counter if message is up
    if (message_counter && !--message_counter)
	message = NULL;

    if (showMessages || message_dontfuckwithme)
    {
	// display message if necessary
	if (plr->message)
	{
	    message = lastmessage = plr->message;
	    plr->message = 0;
	    message_counter = HU_MSGTIMEOUT;
	    message_dontfuckwithme = 0;
	}
    } // else message_on = false;
    else
	plr->message = 0;
}

boolean HU_Responder(event_t *ev)
{
    // EXE brings the last message back
    if (ev->type == ev_keydown && ev->data1 == KEY_ENTER && lastmessage)
    {
	message = lastmessage;
	message_counter = HU_MSGTIMEOUT;
	return true;
    }
    return false;
}
