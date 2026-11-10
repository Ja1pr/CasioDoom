//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// CasioDoom changes: GPL, see LICENSE.
//
// DESCRIPTION:
//	DOOM selection menu, options, episode etc.
//	Sliders and icons. Kinda widget stuff.
//
//  CasioDoom: the menus are plain text in the 4x6 font. They offer what
//  the original did (new game / episode / skill, options, load and save,
//  help, quit) plus a cheat menu, because the cheat codes cannot be typed
//  on a calculator.
//
//-----------------------------------------------------------------------------

#include <string.h>
#include <stdio.h>

#include "doomdef.h"
#include "dstrings.h"

#include "d_main.h"

#include "i_system.h"
#include "i_video.h"
#include "z_zone.h"
#include "v_video.h"
#include "w_wad.h"

#include "r_local.h"

#include "hu_stuff.h"
#include "st_stuff.h"

#include "g_game.h"

#include "m_misc.h"
#include "p_setup.h"

#include "doomstat.h"

#include "m_menu.h"


//
// defaulted values
//
int			mouseSensitivity;       // unused, kept for old saves

// Show messages has default, 0 = off, 1 = on
int			showMessages = 1;
	
// Blocky mode does not exist here.
int			detailLevel;		
// 10 = status bar, 11 = full screen
int			screenblocks = 10;		

boolean			inhelpscreens;
boolean			menuactive;

#define SAVESTRINGSIZE 	24
#define LINEHEIGHT	6
#define NUMSLOTS	6

extern boolean		message_dontfuckwithme;

// 1 = message to be printed
static int		messageToPrint;
// ...and here is the message string!
static const char*	messageString;		
static int		messageLastMenuActive;
// timed message = no input from user
static boolean		messageNeedsInput;     
static void    (*messageRoutine)(int response);

// old save description before edit
static char*		savegamestrings;	// [NUMSLOTS][SAVESTRINGSIZE]

//
// MENU TYPEDEFS
//
typedef struct
{
    // 0 = no cursor here, 1 = ok, 2 = arrows ok
    signed char	status;
    
    const char*	name;
    
    // choice = menu item #.
    // if status = 2,
    //   choice=0:leftarrow,1:rightarrow
    void	(*routine)(int choice);
} menuitem_t;

typedef struct menu_s
{
    short		numitems;	// # of menu items
    const struct menu_s* prevMenu;	// previous menu
    const menuitem_t*	menuitems;	// menu items
    void		(*routine)(void);	// draw routine
    const char*		title;
    short		x;
    short		y;		// x,y of menu
} menu_t;

static short		itemOn;			// menu item skull is on
static short		skullAnimCounter;	// skull animation counter
static short		whichSkull;		// which skull to draw

// current menudef
static const menu_t*	currentMenu;                          
// remembered cursor positions of the menus
static signed char	lastOn[8];

//
// PROTOTYPES
//
static void M_NewGame(int choice);
static void M_Episode(int choice);
static void M_ChooseSkill(int choice);
static void M_LoadGame(int choice);
static void M_SaveGame(int choice);
static void M_Options(int choice);
static void M_EndGame(int choice);
static void M_ReadThis(int choice);
static void M_QuitDOOM(int choice);

static void M_ChangeMessages(int choice);
static void M_SizeDisplay(int choice);
static void M_Brightness(int choice);
static void M_Cheats(int choice);
static void M_DoCheat(int choice);

static void M_FinishReadThis(int choice);
static void M_LoadSelect(int choice);
static void M_SaveSelect(int choice);
static void M_ReadSaveStrings(void);

static void M_DrawMainMenu(void);
static void M_DrawReadThis1(void);
static void M_DrawOptions(void);
static void M_DrawLoad(void);

static void M_SetupNextMenu(const menu_t *menudef);
static void M_StartMessage(const char *string,void (*routine)(int),boolean input);
static void M_ClearMenus (void);

enum { m_main, m_episode, m_skill, m_options, m_cheats, m_read, m_load, m_save };


//
// DOOM MENU
//
static const menuitem_t MainMenu[]=
{
    {1,"NEW GAME",M_NewGame},
    {1,"OPTIONS",M_Options},
    {1,"LOAD GAME",M_LoadGame},
    {1,"SAVE GAME",M_SaveGame},
    {1,"READ THIS!",M_ReadThis},
    {1,"QUIT GAME",M_QuitDOOM}
};

static const menu_t  MainDef =
{
    6, NULL, MainMenu, M_DrawMainMenu, NULL, 46, 17
};


//
// EPISODE SELECT
//
static const menuitem_t EpisodeMenu[]=
{
    {1,"KNEE-DEEP IN THE DEAD", M_Episode},
    {1,"THE SHORES OF HELL", M_Episode},
    {1,"INFERNO", M_Episode},
    {1,"THY FLESH CONSUMED", M_Episode}
};

static const menu_t  EpiDef3 =
{
    3, &MainDef, EpisodeMenu, NULL, "WHICH EPISODE?", 14, 20
};

static const menu_t  EpiDef4 =
{
    4, &MainDef, EpisodeMenu, NULL, "WHICH EPISODE?", 14, 20
};

//
// NEW GAME
//
static const menuitem_t NewGameMenu[]=
{
    {1,"I'M TOO YOUNG TO DIE.",	M_ChooseSkill},
    {1,"HEY, NOT TOO ROUGH.",	M_ChooseSkill},
    {1,"HURT ME PLENTY.",	M_ChooseSkill},
    {1,"ULTRA-VIOLENCE.",	M_ChooseSkill},
    {1,"NIGHTMARE!",		M_ChooseSkill}
};

static const menu_t  NewDef =
{
    5, &MainDef, NewGameMenu, NULL, "CHOOSE SKILL LEVEL:", 14, 18
};


//
// OPTIONS MENU
//
static const menuitem_t OptionsMenu[]=
{
    {1,"END GAME",	M_EndGame},
    {2,"MESSAGES:",	M_ChangeMessages},
    {2,"SCREEN:",	M_SizeDisplay},
    {2,"BRIGHTNESS:",	M_Brightness},
    {1,"CHEATS...",	M_Cheats}
};

static const menu_t  OptionsDef =
{
    5, &MainDef, OptionsMenu, M_DrawOptions, "OPTIONS", 24, 18
};

//
// CHEATS (the codes of the original, as menu items)
//
static const menuitem_t CheatMenu[]=
{
    {1,"GOD MODE       (IDDQD)",	M_DoCheat},
    {1,"WEAPONS + AMMO (IDFA)",	M_DoCheat},
    {1,"...AND KEYS    (IDKFA)",	M_DoCheat},
    {1,"NO CLIPPING    (IDCLIP)",	M_DoCheat},
    {1,"FULL MAP       (IDDT)",	M_DoCheat}
};

static const menu_t  CheatDef =
{
    5, &OptionsDef, CheatMenu, NULL, "CHEATS", 14, 18
};

//
// Read This!
//
static const menuitem_t ReadMenu1[] =
{
    {1,"",M_FinishReadThis}
};

static const menu_t  ReadDef1 =
{
    1, &MainDef, ReadMenu1, M_DrawReadThis1, NULL, 200, 0
};

//
// LOAD / SAVE GAME MENU
//
static const menuitem_t LoadMenu[]=
{
    {1,"", M_LoadSelect},
    {1,"", M_LoadSelect},
    {1,"", M_LoadSelect},
    {1,"", M_LoadSelect},
    {1,"", M_LoadSelect},
    {1,"", M_LoadSelect}
};

static const menu_t  LoadDef =
{
    NUMSLOTS, &MainDef, LoadMenu, M_DrawLoad, "LOAD GAME", 14, 18
};

static const menuitem_t SaveMenu[]=
{
    {1,"", M_SaveSelect},
    {1,"", M_SaveSelect},
    {1,"", M_SaveSelect},
    {1,"", M_SaveSelect},
    {1,"", M_SaveSelect},
    {1,"", M_SaveSelect}
};

static const menu_t  SaveDef =
{
    NUMSLOTS, &MainDef, SaveMenu, M_DrawLoad, "SAVE GAME", 14, 18
};

static int M_MenuIndex (const menu_t* m)
{
    if (m == &MainDef) return m_main;
    if (m == &EpiDef3 || m == &EpiDef4) return m_episode;
    if (m == &NewDef) return m_skill;
    if (m == &OptionsDef) return m_options;
    if (m == &CheatDef) return m_cheats;
    if (m == &LoadDef) return m_load;
    if (m == &SaveDef) return m_save;
    return m_read;
}


//
// M_ReadSaveStrings
//  read the strings from the savegame files
//
static void M_ReadSaveStrings(void)
{
    int             i;
    char    name[32];
	
    if (!savegamestrings)
	savegamestrings = Z_Malloc (NUMSLOTS*SAVESTRINGSIZE, PU_STATIC, 0);

    for (i = 0;i < NUMSLOTS;i++)
    {
	char*	s = savegamestrings + i*SAVESTRINGSIZE;

	G_SaveName (i, name);
	memset (s, 0, SAVESTRINGSIZE);
	if (I_FileOpen (name, false))
	{
	    I_FileRead (s, SAVESTRINGSIZE);
	    I_FileClose ();
	    s[SAVESTRINGSIZE-1] = 0;
	}
    }
}


//
// M_LoadGame & Cie.
//
static void M_DrawLoad(void)
{
    int             i;
	
    for (i = 0;i < NUMSLOTS; i++)
    {
	const char* s = savegamestrings + i*SAVESTRINGSIZE;
	HU_DrawText (currentMenu->x, currentMenu->y+LINEHEIGHT*i,
		     s[0] ? s : EMPTYSTRING);
    }
}


//
// User wants to load this game
//
static void M_LoadSelect(int choice)
{
    if (!savegamestrings[choice*SAVESTRINGSIZE])
	return;
    G_LoadGame (choice);
    M_ClearMenus ();
}

//
// Selected from DOOM menu
//
static void M_LoadGame (int choice)
{
    (void)choice;
    M_ReadSaveStrings();
    M_SetupNextMenu(&LoadDef);
}


//
// M_SaveGame & Cie.
// There is no way to type a name, so the save is named after the level.
//
static void M_SaveSelect(int choice)
{
    char	desc[SAVESTRINGSIZE+16];

    strncpy (desc, HU_LevelName (), SAVESTRINGSIZE-1);
    desc[SAVESTRINGSIZE-1] = 0;
    G_SaveGame (choice, desc);
    M_ClearMenus ();
}

//
// Selected from DOOM menu
//
static void M_SaveGame (int choice)
{
    (void)choice;
    if (!usergame)
    {
	M_StartMessage(SAVEDEAD,NULL,false);
	return;
    }
	
    if (gamestate != GS_LEVEL)
	return;
	
    M_ReadSaveStrings();
    M_SetupNextMenu(&SaveDef);
}


//
// Read This Menus
//
static void M_DrawReadThis1(void)
{
    inhelpscreens = true;
    V_DrawPatch (0,0,0,W_CacheLumpName("HELP1",PU_CACHE));
}


//
// M_DrawMainMenu
//
static void M_DrawMainMenu(void)
{
    const patch_t*	logo;

    if (W_CheckNumForName ("M_DOOM") >= 0)
    {
	logo = W_CacheLumpName("M_DOOM",PU_CACHE);
	V_DrawPatch ((SCREENWIDTH - logo->width)/2, 1, 0, logo);
    }
    else
	HU_DrawText (54, 4, "DOOM");
}


//
// M_NewGame
//
static void M_NewGame(int choice)
{
    (void)choice;
    if ( gamemode == commercial )
	M_SetupNextMenu(&NewDef);
    else if ( gamemode == retail )
	M_SetupNextMenu(&EpiDef4);
    else
	M_SetupNextMenu(&EpiDef3);
}


//
//      M_Episode
//
static int     epi;

static void M_VerifyNightmare(int ch)
{
    if (ch != 'y')
	return;
		
    G_DeferedInitNew(sk_nightmare,epi+1,1);
    M_ClearMenus ();
}

static void M_ChooseSkill(int choice)
{
    if (choice == sk_nightmare)
    {
	M_StartMessage(NIGHTMARE,M_VerifyNightmare,true);
	return;
    }
	
    G_DeferedInitNew(choice,epi+1,1);
    M_ClearMenus ();
}

static void M_Episode(int choice)
{
    if (!P_MapExists (choice+1, 1))
    {
	if (gamemode == shareware)
	    M_StartMessage(SWSTRING,NULL,false);
	else
	    M_StartMessage("THIS EPISODE IS NOT IN\nYOUR DOOM.CWD FILE.\n\nPRESS A KEY.",NULL,false);
	return;
    }
	 
    epi = choice;
    M_SetupNextMenu(&NewDef);
}



//
// M_Options
//
static void M_DrawOptions(void)
{
    static const char digits[5][2] = { "0", "1", "2", "3", "4" };
    int		x = currentMenu->x + 12*HU_CHARW;
    int		y = currentMenu->y;

    HU_DrawText (x, y+LINEHEIGHT*1, showMessages ? "ON" : "OFF");
    HU_DrawText (x, y+LINEHEIGHT*2, screenblocks >= 11 ? "FULL" : "STATUS BAR");
    HU_DrawText (x, y+LINEHEIGHT*3, digits[usegamma % 5]);
}

static void M_Options(int choice)
{
    (void)choice;
    M_SetupNextMenu(&OptionsDef);
}

static void M_Cheats(int choice)
{
    (void)choice;
    if (gamestate != GS_LEVEL || !usergame)
    {
	M_StartMessage("CHEATS ONLY WORK\nDURING A GAME.\n\nPRESS A KEY.",NULL,false);
	return;
    }
    if (gameskill == sk_nightmare)
    {
	M_StartMessage("NO CHEATING IN NIGHTMARE!\n\nPRESS A KEY.",NULL,false);
	return;
    }
    M_SetupNextMenu(&CheatDef);
}

static void M_DoCheat(int choice)
{
    ST_Cheat (choice);
    message_dontfuckwithme = true;
    M_ClearMenus ();
}


//
//      Toggle messages on/off
//
static void M_ChangeMessages(int choice)
{
    (void)choice;
    showMessages = 1 - showMessages;
	
    if (!showMessages)
	players[consoleplayer].message = MSGOFF;
    else
	players[consoleplayer].message = MSGON ;

    message_dontfuckwithme = true;
    M_SaveDefaults ();
}


//
// M_EndGame
//
static void M_EndGameResponse(int ch)
{
    if (ch != 'y')
	return;
		
    M_ClearMenus ();
    D_StartTitle ();
}

static void M_EndGame(int choice)
{
    (void)choice;
    if (!usergame)
	return;
	
    M_StartMessage(ENDGAME,M_EndGameResponse,true);
}


//
// M_ReadThis
//
static void M_ReadThis(int choice)
{
    (void)choice;
    M_SetupNextMenu(&ReadDef1);
}

static void M_FinishReadThis(int choice)
{
    (void)choice;
    M_SetupNextMenu(&MainDef);
}


//
// M_QuitDOOM
//
static void M_QuitResponse(int ch)
{
    if (ch != 'y')
	return;
    M_SaveDefaults ();
    I_Quit ();
}

static void M_QuitDOOM(int choice)
{
    (void)choice;
    M_StartMessage("ARE YOU SURE YOU WANT TO\nQUIT THIS GREAT GAME?\n\n" DOSY,
		   M_QuitResponse,true);
}


static void M_SizeDisplay(int choice)
{
    (void)choice;
    screenblocks = screenblocks >= 11 ? 10 : 11;
    R_SetViewSize (screenblocks, 0);
    M_SaveDefaults ();
}

static void M_Brightness(int choice)
{
    if (choice)
	usegamma = (usegamma + 1) % 5;
    else
	usegamma = (usegamma + 4) % 5;
    M_SaveDefaults ();
}


//
//      Menu Functions
//
static void
M_StartMessage
( const char*	string,
  void		(*routine)(int),
  boolean	input )
{
    messageLastMenuActive = menuactive;
    messageToPrint = 1;
    messageString = string;
    messageRoutine = routine;
    messageNeedsInput = input;
    menuactive = true;
    return;
}


//
// CONTROL PANEL
//

//
// M_Responder
//
boolean M_Responder (event_t* ev)
{
    int             ch;
	
    if (ev->type != ev_keydown)
	return false;
    ch = ev->data1;
    
    // Take care of any messages that need input
    if (messageToPrint)
    {
	// EXE answers yes, EXIT (or anything else) answers no
	if (ch == KEY_ENTER)
	    ch = 'y';
	else if (messageNeedsInput == true)
	    ch = 'n';
		
	menuactive = messageLastMenuActive;
	messageToPrint = 0;
	if (messageRoutine)
	    messageRoutine(ch);
			
	// like the original: answering a question closes the menu
	if (!messageToPrint)
	    menuactive = false;
	return true;
    }
    
    // F-Keys
    if (!menuactive)
	switch(ch)
	{
	  case KEY_MINUS:         // Screen size down
	  case KEY_EQUALS:        // Screen size up
	    if (automapactive || gamestate != GS_LEVEL)
		return false;
	    M_SizeDisplay(0);
	    return true;
				
	  case KEY_F1:            // Help key
	    M_StartControlPanel ();
	    currentMenu = &ReadDef1;
	    itemOn = 0;
	    return true;
				
	  case KEY_F2:            // Save
	    M_StartControlPanel();
	    M_SaveGame(0);
	    return true;
				
	  case KEY_F3:            // Load
	    M_StartControlPanel();
	    M_LoadGame(0);
	    return true;
				
	  case KEY_F11:           // gamma toggle
	    M_Brightness (1);
	    return true;
	}

    
    // Pop-up menu?
    if (!menuactive)
    {
	if (ch == KEY_ESCAPE)
	{
	    M_StartControlPanel ();
	    return true;
	}
	return false;
    }

    
    // Keys usable within menu
    switch (ch)
    {
      case KEY_DOWNARROW:
	do
	{
	    if (itemOn+1 > currentMenu->numitems-1)
		itemOn = 0;
	    else itemOn++;
	} while(currentMenu->menuitems[itemOn].status==0);
	return true;
		
      case KEY_UPARROW:
	do
	{
	    if (!itemOn)
		itemOn = currentMenu->numitems-1;
	    else itemOn--;
	} while(currentMenu->menuitems[itemOn].status==0);
	return true;

      case KEY_LEFTARROW:
	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status == 2)
	    currentMenu->menuitems[itemOn].routine(0);
	return true;
		
      case KEY_RIGHTARROW:
	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status == 2)
	    currentMenu->menuitems[itemOn].routine(1);
	return true;

      case KEY_ENTER:
	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status)
	{
	    lastOn[M_MenuIndex (currentMenu)] = itemOn;
	    if (currentMenu->menuitems[itemOn].status == 2)
		currentMenu->menuitems[itemOn].routine(1);      // right arrow
	    else
		currentMenu->menuitems[itemOn].routine(itemOn);
	}
	return true;
		
      case KEY_ESCAPE:
      case KEY_BACKSPACE:
	lastOn[M_MenuIndex (currentMenu)] = itemOn;
	if (currentMenu->prevMenu)
	{
	    currentMenu = currentMenu->prevMenu;
	    itemOn = lastOn[M_MenuIndex (currentMenu)];
	}
	else
	    M_ClearMenus ();
	return true;
	
      default:
	break;
	
    }

    return true;
}



//
// M_StartControlPanel
//
void M_StartControlPanel (void)
{
    // intro might call this repeatedly
    if (menuactive)
	return;
    
    menuactive = 1;
    currentMenu = &MainDef;         // JDC
    itemOn = lastOn[m_main];        // JDC
}


//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
void M_Drawer (void)
{
    short		i;
    short		x;
    short		y;
    short		lines;
    const char*		s;
    const char*		e;
    int			w;

    inhelpscreens = false;

    
    // Horiz. & Vertically center string and print it.
    if (messageToPrint)
    {
	lines = 1;
	w = 0;
	i = 0;
	for (s = messageString ; *s ; s++)
	{
	    if (*s == '\n')
	    {
		lines++;
		i = 0;
	    }
	    else if (++i > w)
		w = i;
	}
	y = (SCREENHEIGHT - lines*HU_CHARH)/2;
	if (y < 1)
	    y = 1;
	V_FillRect ((SCREENWIDTH - w*HU_CHARW)/2 - 3, y - 3,
		    w*HU_CHARW + 6, lines*HU_CHARH + 5, V_BLACK);
	V_FillRect ((SCREENWIDTH - w*HU_CHARW)/2 - 2, y - 2,
		    w*HU_CHARW + 4, lines*HU_CHARH + 3, V_WHITE);
	for (s = messageString ; *s ; )
	{
	    e = s;
	    while (*e && *e != '\n')
		e++;
	    x = (SCREENWIDTH - (e - s)*HU_CHARW)/2;
	    if (x < 1)
		x = 1;
	    HU_DrawTextN (x, y, s, e - s);
	    y += HU_CHARH;
	    s = *e ? e+1 : e;
	}
	return;
    }

    if (!menuactive)
	return;

    // a plain background makes the small text readable
    if (currentMenu != &ReadDef1)
	V_FillRect (0, 0, SCREENWIDTH, SCREENHEIGHT, V_WHITE);

    if (currentMenu->title)
	HU_DrawText ((SCREENWIDTH - (int)strlen (currentMenu->title)*HU_CHARW)/2,
		     4, currentMenu->title);

    if (currentMenu->routine)
	currentMenu->routine();         // call Draw routine
    
    // DRAW MENU
    x = currentMenu->x;
    y = currentMenu->y;

    for (i=0 ; i<currentMenu->numitems ; i++)
    {
	if (currentMenu->menuitems[i].name[0])
	    HU_DrawText (x, y, currentMenu->menuitems[i].name);
	y += LINEHEIGHT;
    }

    
    // DRAW SKULL
    if (x < SCREENWIDTH)
	V_DrawPatch (x - 8, currentMenu->y + itemOn*LINEHEIGHT - 1, 0,
		     W_CacheLumpName(whichSkull ? "M_SKULL2" : "M_SKULL1",
				     PU_CACHE));

}


//
// M_ClearMenus
//
static void M_ClearMenus (void)
{
    menuactive = 0;
}




//
// M_SetupNextMenu
//
static void M_SetupNextMenu(const menu_t *menudef)
{
    currentMenu = menudef;
    itemOn = lastOn[M_MenuIndex (menudef)];
    if (itemOn >= menudef->numitems)
	itemOn = 0;
}


//
// M_Ticker
//
void M_Ticker (void)
{
    if (--skullAnimCounter <= 0)
    {
	whichSkull ^= 1;
	skullAnimCounter = 8;
    }
}


//
// M_Init
//
void M_Init (void)
{
    currentMenu = &MainDef;
    menuactive = 0;
    itemOn = 0;
    whichSkull = 0;
    skullAnimCounter = 10;
    messageToPrint = 0;
    messageString = NULL;
    messageLastMenuActive = menuactive;
    lastOn[m_skill] = sk_medium;
}
