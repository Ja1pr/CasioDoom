//-----------------------------------------------------------------------------
//
// CasioDoom - PC test harness.
//
// Runs the very same engine as the calculator, head-less: time is virtual
// (one tic per displayed frame), keys come from a script and frames can be
// dumped as PGM/PBM pictures.  It also enforces a calculator-sized heap.
//
//   doomhost -wad DOOM.CWD [-heap BYTES] [-frames N] [-dump DIR]
//            [-every N] [-keys "T:KEY ..."] [-frag N] [-warp E M] [-skill S]
//
//  -keys: space separated "tic:key" (press) / "tic:-key" (release) pairs,
//  key = number or one of up down left right fire use enter esc tab y
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "../doomdef.h"
#include "../doomstat.h"
#include "../d_main.h"
#include "../i_system.h"
#include "../i_video.h"
#include "../z_zone.h"
#include "../v_video.h"
#include "../m_argv.h"

static int	hosttime;
static int	maxframes = 200;
static int	dumpevery = 35;
static const char* dumpdir;
static int	framecount;

#define MAXKEYS 512
static struct { int tic; int key; int down; } keyscript[MAXKEYS];
static int	numkeys, nextkey;

int		host_peakused;


void I_Init (void)
{
}

void I_ZoneAreas (void)
{
    int		size = 100*1024;
    int		p = M_CheckParm ("-heap");
    int		first;

    if (p && p < myargc-1)
	size = atoi (myargv[p+1]);
    // two areas, like the calculator (user RAM + system heap)
    first = size / 4;
    Z_AddArea (malloc (first), first);
    Z_AddArea (malloc (size - first), size - first);
}

const wadfrag_t* I_MapWad (int* count)
{
    static wadfrag_t	frags[64];
    const char*		name = "DOOM.CWD";
    int			p = M_CheckParm ("-wad");
    int			nfrag = 1;
    FILE*		f;
    long		size;
    int			i;
    unsigned		ofs, len, piece;

    if (p && p < myargc-1)
	name = myargv[p+1];
    p = M_CheckParm ("-frag");
    if (p && p < myargc-1)
	nfrag = atoi (myargv[p+1]);
    if (nfrag < 1 || nfrag > 64)
	nfrag = 1;

    f = fopen (name, "rb");
    if (!f)
	I_Error ("cannot open %s", name);
    fseek (f, 0, SEEK_END);
    size = ftell (f);
    fseek (f, 0, SEEK_SET);

    // split into pieces whose sizes are multiples of 512 bytes, each in
    // its own allocation, to mimic a fragmented flash file system
    piece = ((size / nfrag) + 511) & ~511u;
    ofs = 0;
    for (i=0 ; ofs < (unsigned)size ; i++)
    {
	byte*	mem;
	len = size - ofs < piece ? size - ofs : piece;
	mem = malloc (len);
	if (fread (mem, 1, len, f) != len)
	    I_Error ("read error");
	frags[i].base = mem;
	frags[i].ofs = ofs;
	frags[i].len = len;
	ofs += len;
    }
    fclose (f);
    *count = i;
    return frags;
}

int I_GetTime (void)
{
    static int	polls;

    // virtual time: one tic per frame, and it also moves on when the
    // game is just waiting for the clock
    if (++polls > 8)
    {
	polls = 0;
	hosttime++;
    }
    return hosttime;
}

static int keyname (const char* s)
{
    if (!strcmp (s, "up")) return KEY_UPARROW;
    if (!strcmp (s, "down")) return KEY_DOWNARROW;
    if (!strcmp (s, "left")) return KEY_LEFTARROW;
    if (!strcmp (s, "right")) return KEY_RIGHTARROW;
    if (!strcmp (s, "fire")) return KEY_RCTRL;
    if (!strcmp (s, "use")) return ' ';
    if (!strcmp (s, "enter")) return KEY_ENTER;
    if (!strcmp (s, "esc")) return KEY_ESCAPE;
    if (!strcmp (s, "tab")) return KEY_TAB;
    if (!strcmp (s, "run")) return KEY_RSHIFT;
    if (!strcmp (s, "strafe")) return KEY_RALT;
    if (s[0] == 'f' && s[1] >= '1' && s[1] <= '9') return KEY_F1 + s[1] - '1';
    if (s[0] >= '0' && s[0] <= '9' && s[1]) return atoi (s);
    return s[0];
}

static void parsekeys (const char* s)
{
    char	buf[32];
    int		tic, n;

    while (*s && numkeys < MAXKEYS)
    {
	if (sscanf (s, "%d:%31s%n", &tic, buf, &n) < 2)
	    break;
	s += n;
	while (*s == ' ')
	    s++;
	keyscript[numkeys].tic = tic;
	keyscript[numkeys].down = buf[0] != '-';
	keyscript[numkeys].key = keyname (buf[0] == '-' ? buf+1 : buf);
	numkeys++;
    }
}

void I_StartFrame (void)
{
}

void G_ExitLevel (void);
void F_StartFinale (void);
static int	exitat = -1;
static int	finaleat = -1;

void I_StartTic (void)
{
    event_t	ev;

    // test hooks: -exitlevel TIC, -finale TIC
    if (hosttime == exitat)
    {
	exitat = -1;
	G_ExitLevel ();
    }
    if (hosttime == finaleat)
    {
	finaleat = -1;
	F_StartFinale ();
    }

    while (nextkey < numkeys && keyscript[nextkey].tic <= hosttime)
    {
	ev.type = keyscript[nextkey].down ? ev_keydown : ev_keyup;
	ev.data1 = keyscript[nextkey].key;
	ev.data2 = ev.data3 = 0;
	D_PostEvent (&ev);
	nextkey++;
    }
}

void I_Quit (void)
{
    printf ("I_Quit after %d frames\n", framecount);
    exit (0);
}

void I_Error (const char *error, ...)
{
    va_list	argptr;

    va_start (argptr,error);
    fprintf (stderr, "Error: ");
    vfprintf (stderr,error,argptr);
    fprintf (stderr, "\n");
    va_end (argptr);
    exit (1);
}

static FILE*	savefile;

boolean I_FileOpen (const char* name, boolean write)
{
    savefile = fopen (name, write ? "wb" : "rb");
    return savefile != NULL;
}

int I_FileRead (void* buf, int len)
{
    return fread (buf, 1, len, savefile);
}

int I_FileWrite (const void* buf, int len)
{
    return fwrite (buf, 1, len, savefile);
}

void I_FileClose (void)
{
    if (savefile)
	fclose (savefile);
    savefile = NULL;
}

void I_InitGraphics (void)
{
}

void I_ShutdownGraphics (void)
{
}

#include "../p_local.h"
#include "../r_local.h"
#include "../m_random.h"
// What the level needs on the 32 bit calculator (sizes from sh-elf-gcc).
static void host_stats (void)
{
    thinker_t*	th;
    int		mobjs = 0, statics = 0, others = 0;
    int		est;

    for (th = thinkercap.next ; th && th != &thinkercap ; th = th->next)
    {
	if (th->function.acp1 == (actionf_p1)P_MobjThinker)
	    mobjs++;
	else if (th->function.acp1 == (actionf_p1)P_StaticThinker)
	    statics++;
	else
	    others++;
    }
    // sizes on the calculator: mobj_t 120, static part 88, block header 16
    est = mobjs*(120+16) + statics*(88+16) + others*(48+16) + numsectors*32
	+ numlines*4 + numsides/8 + bmapwidth*bmapheight*4;
    printf ("state: gametic %d leveltime %d rnd %d player %d %d %d health %d kills %d\n",
	    gametic, leveltime, P_Random (), players[0].mo ? players[0].mo->x : 0,
	    players[0].mo ? players[0].mo->y : 0, players[0].mo ? players[0].mo->angle >> 16 : 0,
	    players[0].health, players[0].killcount);
    printf ("level E%dM%d: %d sectors, %d lines, blockmap %dx%d, %d mobjs + %d "
	    "static things, %d specials -> about %d KB of level RAM on the "
	    "calculator\n",
	    gameepisode, gamemap, numsectors, numlines, bmapwidth,
	    bmapheight, mobjs, statics, others, (est + 1023) / 1024);
}

void I_ShowMono (const byte* bits)
{
    char	name[256];
    FILE*	f;
    int		used;

    used = Z_TotalMemory () - Z_FreeMemory ();
    if (used > host_peakused)
	host_peakused = used;

    if (dumpdir && (framecount % dumpevery) == 0)
    {
	sprintf (name, "%s/f%05d.pbm", dumpdir, framecount);
	f = fopen (name, "wb");
	if (f)
	{
	    fprintf (f, "P4\n128 64\n");
	    fwrite (bits, 1, 1024, f);
	    fclose (f);
	}
	sprintf (name, "%s/f%05d.pgm", dumpdir, framecount);
	f = fopen (name, "wb");
	if (f)
	{
	    int i;
	    fprintf (f, "P5\n128 64\n63\n");
	    for (i=0 ; i<SCREENWIDTH*SCREENHEIGHT ; i++)
		fputc (screens[0][i] > 63 ? 63 : screens[0][i], f);
	    fclose (f);
	}
    }
    framecount++;
    hosttime++;
    if (framecount >= maxframes)
    {
	if (gamestate == GS_LEVEL)
	    host_stats ();
	printf ("done: %d frames, heap peak %d of %d bytes, now free %d\n",
		framecount, host_peakused, Z_TotalMemory (), Z_FreeMemory ());
	exit (0);
    }
}

int main (int argc, char** argv)
{
    int		p;

    myargc = argc;
    myargv = argv;
    p = M_CheckParm ("-frames");
    if (p && p < argc-1)
	maxframes = atoi (argv[p+1]);
    p = M_CheckParm ("-every");
    if (p && p < argc-1)
	dumpevery = atoi (argv[p+1]);
    p = M_CheckParm ("-dump");
    if (p && p < argc-1)
	dumpdir = argv[p+1];
    p = M_CheckParm ("-exitlevel");
    if (p && p < argc-1)
	exitat = atoi (argv[p+1]);
    p = M_CheckParm ("-finale");
    if (p && p < argc-1)
	finaleat = atoi (argv[p+1]);
    p = M_CheckParm ("-keys");
    if (p && p < argc-1)
	parsekeys (argv[p+1]);

    D_DoomMain ();
    return 0;
}
