//-----------------------------------------------------------------------------
//
// CasioDoom - CASIO fx-9860GIII (and compatible SH4 models) system layer,
// built on the gint kernel.
//
// The interesting part is I_MapWad(): the calculator only has about
// 100 KB of RAM for an add-in, far too little to load a level. DOOM.CWD
// is therefore never loaded. Its sectors are located in the flash chip,
// which is visible in the address space, and the game reads all level
// and graphics data straight from there.
//
// This file is GPL, see LICENSE.
//
//-----------------------------------------------------------------------------

#include <gint/gint.h>
#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/timer.h>
#include <gint/bfile.h>
#include <gint/kmalloc.h>
#include <gint/hardware.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../doomdef.h"
#include "../doomstat.h"
#include "../d_main.h"
#include "../i_system.h"
#include "../i_video.h"
#include "../z_zone.h"
#include "../m_argv.h"

// gint and DOOM both use KEY_ names; these are DOOM's (doomdef.h)
// under names that cannot clash.
enum
{
    DK_RIGHT = 0xae, DK_LEFT = 0xac, DK_UP = 0xad, DK_DOWN = 0xaf,
    DK_ESCAPE = 27, DK_ENTER = 13, DK_TAB = 9,
    DK_F2 = 0x80+0x3c, DK_F3 = 0x80+0x3d, DK_F11 = 0x80+0x57,
    DK_BACKSPACE = 127, DK_PAUSE = 0xff,
    DK_FIRE = 0x80+0x1d, DK_STRAFE = 0x80+0x38, DK_RUN = 0x80+0x36
};

#define WADNAME		"DOOM.CWD"
#define SECTOR		512
#define READCHUNK	4096
#define MAXFRAGS	64

// How much of the system heap is left to the OS (BFile needs some).
#define OSHEAP_RESERVE	(12*1024)
// ...and of gint's own arena.
#define URAM_RESERVE	1536


//
// TIME
//
static volatile int	tics;
static int		tictimer = -1;

static int I_TimerCallback (void)
{
    tics++;
    return TIMER_CONTINUE;
}

int I_GetTime (void)
{
    return tics;
}

void I_Init (void)
{
    tictimer = timer_configure (TIMER_ANY, 1000000/TICRATE,
				GINT_CALL (I_TimerCallback));
    if (tictimer >= 0)
	timer_start (tictimer);
}


//
// MEMORY
//
void I_ZoneAreas (void)
{
    size_t	size;
    void*	p;

    // What is left of the add-in's own RAM area.
    size = 0;
    p = kmalloc_max (&size, "_uram");
    if (p && size > URAM_RESERVE + 2048)
    {
	size -= URAM_RESERVE;
	p = krealloc (p, size);
	if (p)
	    Z_AddArea (p, size);
    }

    // The biggest block the system heap will give us.
    for (size = 512*1024 ; size >= 8*1024 ; size -= 4*1024)
    {
	p = kmalloc (size, "_os");
	if (p)
	{
	    if (size > OSHEAP_RESERVE + 8*1024)
	    {
		kfree (p);
		size -= OSHEAP_RESERVE;
		p = kmalloc (size, "_os");
	    }
	    if (p)
		Z_AddArea (p, size);
	    break;
	}
    }
}


//
// ERRORS / EXIT
//
static void I_Shutdown (void)
{
    if (tictimer >= 0)
	timer_stop (tictimer);
    tictimer = -1;
}

void I_Quit (void)
{
    I_Shutdown ();
    exit (0);
}

void I_Error (const char *error, ...)
{
    static char	msg[160];
    va_list	argptr;
    char*	s;
    char*	e;
    int		y = 9;
    int		n;

    va_start (argptr,error);
    vsnprintf (msg, sizeof(msg), error, argptr);
    va_end (argptr);

    dclear (C_WHITE);
    dtext (1, 0, C_BLACK, "DOOM error:");
    // 21 characters of the 5x7 system font fit on a line
    for (s = msg ; *s && y < 56 ; y += 8)
    {
	for (n = 0, e = s ; *e && *e != '\n' && n < 21 ; e++, n++)
	    ;
	dtext_opt (1, y, C_BLACK, C_NONE, DTEXT_LEFT, DTEXT_TOP, s, n);
	s = (*e == '\n') ? e+1 : e;
    }
    dtext (1, 57, C_BLACK, "[EXIT] to leave");
    dupdate ();

    clearevents ();
    while (getkey ().key != KEY_EXIT)
	;
    I_Shutdown ();
    exit (1);
}


//
// FILES (through the OS: every call needs a "world switch")
//
static uint16_t		fpath[32];
static int		ffd = -1;
static boolean		fwriting;
static void*		fbuf;
static int		flen;
static int		fwhence;

static void I_MakePath (const char* name)
{
    static const char	prefix[] = "\\\\fls0\\";
    int			i = 0;
    const char*		p;

    for (p = prefix ; *p ; p++)
	fpath[i++] = *p;
    for (p = name ; *p && i < 31 ; p++)
	fpath[i++] = *p;
    fpath[i] = 0;
}

static int os_open (void)
{
    int		size = 0;

    if (fwriting)
    {
	BFile_Remove (fpath);
	if (BFile_Create (fpath, BFile_File, &size) < 0)
	    return -1;
	return BFile_Open (fpath, BFile_WriteOnly);
    }
    return BFile_Open (fpath, BFile_ReadOnly);
}

static int os_read (void)
{
    return BFile_Read (ffd, fbuf, flen, fwhence);
}

static int os_write (void)
{
    return BFile_Write (ffd, fbuf, flen);
}

static int os_close (void)
{
    return BFile_Close (ffd);
}

static int os_size (void)
{
    return BFile_Size (ffd);
}

boolean I_FileOpen (const char* name, boolean write)
{
    I_MakePath (name);
    fwriting = write;
    ffd = gint_world_switch (GINT_CALL (os_open));
    return ffd >= 0;
}

int I_FileRead (void* buf, int len)
{
    int		n;

    if (ffd < 0)
	return 0;
    fbuf = buf;
    flen = len;
    fwhence = -1;
    n = gint_world_switch (GINT_CALL (os_read));
    return n < 0 ? 0 : n;
}

int I_FileWrite (const void* buf, int len)
{
    static byte	even[2];
    int		rc;

    if (ffd < 0 || len <= 0)
	return 0;
    // BFile only writes an even number of bytes at a time
    fbuf = (void *)buf;
    flen = len & ~1;
    rc = 0;
    if (flen)
	rc = gint_world_switch (GINT_CALL (os_write));
    if (rc >= 0 && (len & 1))
    {
	even[0] = ((const byte *)buf)[len-1];
	even[1] = 0;
	fbuf = even;
	flen = 2;
	rc = gint_world_switch (GINT_CALL (os_write));
    }
    return rc < 0 ? 0 : len;
}

void I_FileClose (void)
{
    if (ffd >= 0)
	gint_world_switch (GINT_CALL (os_close));
    ffd = -1;
}


//
// THE WAD IN FLASH
//
static const byte*	flashstart;
static const byte*	flashend;

// Looks for a sector with the given contents.
static const byte* I_FindSector (const byte* data, int len, const byte* from)
{
    const byte*		p;
    unsigned int	key;
    int			pass;

    memcpy (&key, data, 4);

    // from the hint to the end, then from the start to the hint
    for (pass = 0 ; pass < 2 ; pass++)
    {
	const byte*	a = pass ? flashstart : from;
	const byte*	b = pass ? from : flashend;

	for (p = a ; p + len <= b ; p += SECTOR)
	    if (*(const unsigned int *)p == key && !memcmp (p, data, len))
		return p;
    }
    return NULL;
}

static void I_Progress (const char* text, int done, int total)
{
    dclear (C_WHITE);
    dtext (1, 10, C_BLACK, "DOOM");
    dtext (1, 24, C_BLACK, text);
    drect_border (10, 40, 117, 48, C_WHITE, 1, C_BLACK);
    if (total > 0)
	drect (12, 42, 12 + (int)((long long)done * 103 / total), 46, C_BLACK);
    dupdate ();
}

const wadfrag_t* I_MapWad (int* count)
{
    wadfrag_t*		frags;
    byte*		buf;
    int			nfrags = 0;
    int			size;
    int			ofs;
    int			n;
    int			pos;
    int			len;
    const byte*		addr;
    const byte*		expect = NULL;

    // P1 (cached) view of the flash chip
    flashstart = (const byte *)0x80000000;
    flashend = flashstart + gint[HWROM];
    if (gint[HWROM] < (1 << 20) || gint[HWROM] > (64 << 20))
	flashend = flashstart + (8 << 20);

    if (!I_FileOpen (WADNAME, false))
	I_Error (WADNAME " not found.\nCopy it to the\nstorage memory.");
    size = gint_world_switch (GINT_CALL (os_size));
    if (size < 64)
	I_Error (WADNAME " is empty.");

    frags = Z_Malloc (MAXFRAGS*sizeof(wadfrag_t), PU_STATIC, 0);
    buf = Z_Malloc (READCHUNK, PU_STATIC, 0);

    for (ofs = 0 ; ofs < size ; ofs += n)
    {
	if (!(ofs & 0xffff))
	    I_Progress ("Locating " WADNAME "...", ofs, size);

	fbuf = buf;
	flen = size - ofs < READCHUNK ? size - ofs : READCHUNK;
	fwhence = ofs;
	n = gint_world_switch (GINT_CALL (os_read));
	if (n <= 0)
	    I_Error ("Cannot read " WADNAME);

	for (pos = 0 ; pos < n ; pos += SECTOR)
	{
	    len = n - pos < SECTOR ? n - pos : SECTOR;

	    // usually the file just continues
	    if (expect && expect + len <= flashend
		&& !memcmp (expect, buf + pos, len))
		addr = expect;
	    else
		addr = I_FindSector (buf + pos, len,
				     expect ? expect : flashstart);
	    if (!addr)
		I_Error ("Cannot find " WADNAME "\nin flash memory\n(offset %d).",
			 ofs + pos);

	    if (nfrags && addr == expect)
		frags[nfrags-1].len += len;
	    else
	    {
		if (nfrags == MAXFRAGS)
		    I_Error ("Storage memory is too\nfragmented. Optimize\n"
			     "it and copy " WADNAME "\nagain.");
		frags[nfrags].base = addr;
		frags[nfrags].ofs = ofs + pos;
		frags[nfrags].len = len;
		nfrags++;
	    }
	    expect = addr + len;
	}
    }
    I_FileClose ();
    Z_Free (buf);

    I_Progress ("Starting...", 1, 1);
    *count = nfrags;
    return frags;
}


//
// VIDEO
//
void I_InitGraphics (void)
{
}

void I_ShutdownGraphics (void)
{
}

void I_ShowMono (const byte* bits)
{
    // gint's video RAM has the very same layout: 128x64, 1 bit per
    // pixel, most significant bit first, 1 = dark.
    memcpy (gint_vram, bits, SCREENWIDTH*SCREENHEIGHT/8);
    dupdate ();
}


//
// INPUT
//
static int I_TranslateKey (int key)
{
    switch (key)
    {
      case KEY_UP:	return DK_UP;
      case KEY_DOWN:	return DK_DOWN;
      case KEY_LEFT:	return DK_LEFT;
      case KEY_RIGHT:	return DK_RIGHT;
      case KEY_SHIFT:	return DK_FIRE;
      case KEY_ALPHA:	return ' ';		// use / open
      case KEY_OPTN:	return DK_STRAFE;
      case KEY_VARS:	return DK_RUN;
      case KEY_F1:	return DK_TAB;		// automap
      case KEY_F2:	return DK_F2;		// save
      case KEY_F3:	return DK_F3;		// load
      case KEY_F4:	return DK_F11;		// brightness
      case KEY_F5:	return ',';		// step left
      case KEY_F6:	return '.';		// step right
      case KEY_EXE:	return DK_ENTER;
      case KEY_EXIT:	return DK_ESCAPE;
      case KEY_DEL:	return DK_BACKSPACE;
      case KEY_ACON:	return DK_PAUSE;
      case KEY_0:	return '0';
      case KEY_1:	return '1';
      case KEY_2:	return '2';
      case KEY_3:	return '3';
      case KEY_4:	return '4';
      case KEY_5:	return '5';
      case KEY_6:	return '6';
      case KEY_7:	return '7';
      case KEY_8:	return '8';
      case KEY_9:	return '9';
      case KEY_ADD:	return '=';		// zoom in / screen size
      case KEY_SUB:	return '-';		// zoom out / screen size
      case KEY_XOT:	return 'f';		// automap: follow mode
      case KEY_LOG:	return 'g';		// automap: grid
      case KEY_LN:	return 'm';		// automap: mark
      case KEY_SIN:	return 'c';		// automap: clear marks
    }
    return 0;
}

void I_StartFrame (void)
{
}

void I_StartTic (void)
{
    key_event_t	kev;
    event_t	ev;

    while ((kev = pollevent ()).type != KEYEV_NONE)
    {
	if (kev.type != KEYEV_DOWN && kev.type != KEYEV_UP)
	    continue;

	// [MENU] goes to the calculator's main menu, as usual
	if (kev.key == KEY_MENU)
	{
	    if (kev.type == KEYEV_DOWN)
	    {
		gint_osmenu ();
		clearevents ();
	    }
	    continue;
	}

	ev.data1 = I_TranslateKey (kev.key);
	if (!ev.data1)
	    continue;
	ev.type = kev.type == KEYEV_DOWN ? ev_keydown : ev_keyup;
	ev.data2 = ev.data3 = 0;
	D_PostEvent (&ev);
    }
}


//
// MAIN
//
int main (void)
{
    myargc = 0;
    myargv = NULL;
    D_DoomMain ();
    return 0;
}
