/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// console.c

#ifdef NeXT
#include <libc.h>
#endif
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <fcntl.h>
#include "quakedef.h"

#include <time.h> // JPG - needed for console log

int 		con_linewidth;

float		con_cursorspeed = 4;

// JPG - upped CON_TEXTSIZE from 16384 to 65536
#define		CON_TEXTSIZE	65536

qboolean 	con_forcedup;		// because no entities to refresh

int			con_totallines;		// total lines in console scrollback
int			con_backscroll;		// lines up from bottom to display
int			con_current;		// where next message will be printed
int			con_x;				// offset in current line for next print
char		*con_text=0;

cvar_t		pq_confilter = {"pq_confilter", "0"};	// JPG 1.05 - filter out the "you got" messages
cvar_t		pq_timestamp = {"pq_timestamp", "0"};	// JPG 1.05 - timestamp player binds during a match
cvar_t		pq_removecr = {"pq_removecr", "1"};		// JPG 3.20 - remove \r from console output

#define	NUM_CON_TIMES 4
float		con_times[NUM_CON_TIMES];	// realtime time the line was generated
								// for transparent notify lines

int			con_vislines;

qboolean	con_debuglog;

#define		MAXCMDLINE	256
extern	char	key_lines[32][MAXCMDLINE];
extern	int		key_linepos;

qboolean	con_initialized;

char logfilename[128];	// JPG - support for different filenames

viddef_t	vid;


/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	if (con_text)
		Q_memset (con_text, ' ', CON_TEXTSIZE);
}


/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	int		i;

	for (i=0 ; i<NUM_CON_TIMES ; i++)
		con_times[i] = 0;
}


/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char	tbuf[CON_TEXTSIZE];

	width = (vid.width >> 3) - 2;

	if (width == con_linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = 38;
		con_linewidth = width;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		Q_memset (con_text, ' ', CON_TEXTSIZE);
	}
	else
	{
		oldwidth = con_linewidth;
		con_linewidth = width;
		oldtotallines = con_totallines;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		numlines = oldtotallines;

		if (con_totallines < numlines)
			numlines = con_totallines;

		numchars = oldwidth;

		if (con_linewidth < numchars)
			numchars = con_linewidth;

		Q_memcpy (tbuf, con_text, CON_TEXTSIZE);
		Q_memset (con_text, ' ', CON_TEXTSIZE);

		for (i=0 ; i<numlines ; i++)
		{
			for (j=0 ; j<numchars ; j++)
			{
				con_text[(con_totallines - 1 - i) * con_linewidth + j] =
						tbuf[((con_current - i + oldtotallines) %
							  oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con_backscroll = 0;
	con_current = con_totallines - 1;
}


/*
================
Con_Init
================
*/
void Con_Init (void)
{
#define MAXGAMEDIRLEN	1000
	char	temp[MAXGAMEDIRLEN+1];
	// char	*t2 = "/qconsole.log"; // JPG - don't need this
	char	*ch;	// JPG - added this
	int		fd, n;	// JPG - added these
    time_t  ltime;		// JPG - for console log file

	con_debuglog = COM_CheckParm("-condebug");

	if (con_debuglog)
	{
		// JPG - check for different file name
		if ((con_debuglog < com_argc - 1) && (*com_argv[con_debuglog+1] != '-') && (*com_argv[con_debuglog+1] != '+'))
		{
			if ((ch = strchr(com_argv[con_debuglog+1], '%')) && (ch[1] == 'd'))
			{
				n = 0;
				do
				{
					n = n + 1;
					sprintf(logfilename, com_argv[con_debuglog+1], n);
					strcat(logfilename, ".log");
					sprintf(temp, "%s/%s", com_gamedir, logfilename);
					fd = open(temp, O_CREAT | O_EXCL | O_WRONLY, 0666);
				}
				while (fd == -1);
				close(fd);
			}
			else
				sprintf(logfilename, "%s.log", com_argv[con_debuglog+1]);
		}
		else
			strcpy(logfilename, "qconsole.log");

		// JPG - changed t2 to logfilename
		if (strlen (com_gamedir) < (MAXGAMEDIRLEN - strlen (logfilename)))
		{
			sprintf (temp, "%s/%s", com_gamedir, logfilename); // JPG - added the '/'
			unlink (temp);
		}

		// JPG - print initial message
		Con_Printf("Log file initialized.\n");
		Con_Printf("%s/%s\n", com_gamedir, logfilename);
		time( &ltime );
		Con_Printf( "%s\n", ctime( &ltime ) );
	}

	con_text = Hunk_AllocName (CON_TEXTSIZE, "context");
	Q_memset (con_text, ' ', CON_TEXTSIZE);
	con_linewidth = -1;
	Con_CheckResize ();

	Con_Printf ("Console initialized.\n");

//
// register our commands
//
	Cvar_RegisterVariable (&pq_confilter);	// JPG 1.05 - make "you got" messages temporary
	Cvar_RegisterVariable (&pq_timestamp);	// JPG 1.05 - timestamp player binds during a match
	Cvar_RegisterVariable (&pq_removecr);	// JPG 3.20 - timestamp player binds during a match

	Cmd_AddCommand ("clear", Con_Clear_f);
	con_initialized = true;
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (void)
{
	con_x = 0;
	con_current++;
	Q_memset (&con_text[(con_current%con_totallines)*con_linewidth], ' ', con_linewidth);

	// JPG - fix backscroll
	if (con_backscroll)
		con_backscroll++;
}

#define DIGIT(x) ((x) >= '0' && (x) <= '9')

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the notify window will pop up.
================
*/
void Con_Print (char *txt)
{
	int		y;
	int		c, l;
	static int	cr;
	int		mask;

	static int fixline = 0;

	//con_backscroll = 0;  // JPG - half of a fix for an annoying problem

	// JPG 1.05 - make the "You got" messages temporary
	if (pq_confilter.value)
		fixline |= !strcmp(txt, "You got armor\n") ||
				   !strcmp(txt, "You receive ") ||
				   !strcmp(txt, "You got the ") ||
				   !strcmp(txt, "no weapon.\n") ||
				   !strcmp(txt, "not enough ammo.\n");

	if (txt[0] == 1)
	{
		mask = 128;		// go to colored text
	// play talk wav
		txt++;

		// JPG 1.05 - timestamp player binds during a match (unless the bind already has a time in it)
		if ((!cls.netcon || cls.netcon->mod != MOD_PROQUAKE || *txt == '(') &&		// JPG 3.30 - fixed old bug hit by some servers
			!con_x && pq_timestamp.value && (cl.minutes || cl.seconds) && cl.seconds < 128)
		{
			int minutes, seconds, match_time, msg_time;
			char buff[16];
			char *ch;

			if (cl.match_pause_time)
				match_time = ceil(60.0 * cl.minutes + cl.seconds - (cl.match_pause_time - cl.last_match_time));
			else
				match_time = ceil(60.0 * cl.minutes + cl.seconds - (cl.time - cl.last_match_time));
			minutes = match_time / 60;
			seconds = match_time - 60 * minutes;

			msg_time = -10;
			ch = txt + 2;
			if (ch[0] && ch[1] && ch[2])
			{
				while (ch[2])
				{
					if (ch[0] == ':' && DIGIT(ch[1]) && DIGIT(ch[2]) && DIGIT(ch[-1]))
					{
						msg_time = 60 * (ch[-1] - '0') + 10 * (ch[1] - '0') + (ch[2] - '0');
						if (DIGIT(ch[-2]))
							msg_time += 600 * (ch[-2] - '0');
						break;
					}
					ch++;
				}
			}
			if (msg_time < match_time - 2 || msg_time > match_time + 2)
			{
				if (pq_timestamp.value == 1)
					sprintf(buff, "%d%c%02d", minutes, 'X' + 128, seconds);
				else
					sprintf(buff, "%02d", seconds);

				if (cr)
				{
					con_current--;
					cr = false;
				}
				Con_Linefeed ();
				if (con_current >= 0)
					con_times[con_current % NUM_CON_TIMES] = realtime;

				y = con_current % con_totallines;
				for (ch = buff ; *ch ; ch++)
					con_text[y*con_linewidth+con_x++] = *ch - 30;
				con_text[y*con_linewidth+con_x++] = ' ';
			}
		}
	}
	else if (txt[0] == 2)
	{
		mask = 128;		// go to colored text
		txt++;
	}
	else
		mask = 0;

	while ( (c = *txt) )
	{
	// count word length
		for (l=0 ; l< con_linewidth ; l++)
			if ( txt[l] <= ' ')
				break;

	// word wrap
		if (l != con_linewidth && (con_x + l > con_linewidth) )
			con_x = 0;

		txt++;

		if (cr)
		{
			con_current--;
			cr = false;
		}


		if (!con_x)
		{
			Con_Linefeed ();
		// mark time for transparent overlay
			if (con_current >= 0)
				con_times[con_current % NUM_CON_TIMES] = realtime;
		}

		switch (c)
		{
		case '\n':
			con_x = 0;
			cr = fixline;	// JPG 1.05 - make the "you got" messages temporary
			fixline = 0;	// JPG
			break;

		case '\r':
			if (pq_removecr.value)	// JPG 3.20 - optionally remove '\r'
				c += 128;
			else
			{
				con_x = 0;
				cr = 1;
				break;
			}

		default:	// display character and advance
			y = con_current % con_totallines;
			con_text[y*con_linewidth+con_x] = c | mask;
			con_x++;
			if (con_x >= con_linewidth)
				con_x = 0;
			break;
		}

	}
}


// JPG - increased this from 4096 to 16384 and moved it up here
// See http://www.inside3d.com/qip/q1/bugs.htm, NVidia 5.16 drivers can cause crash
#define	MAXPRINTMSG	16384

/* JPG - took *file out of the argument list and used logfilename instead
================
Con_DebugLog
================
*/
void Con_DebugLog( /* char *file, */ char *fmt, ...)
{
    va_list argptr;
    static char data[MAXPRINTMSG];	// JPG 3.02 - changed from 1024 to MAXPRINTMSG
    int fd;

    va_start(argptr, fmt);
    vsprintf(data, fmt, argptr);
    va_end(argptr);
    fd = open(va("%s/%s", com_gamedir, logfilename), O_WRONLY | O_CREAT | O_APPEND, 0666);
    write(fd, data, strlen(data));
    close(fd);
}


/*
================
Con_Printf

Handles cursor positioning, line wrapping, etc
================
*/
// FIXME: make a buffer size safe vsprintf?
void Con_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	static qboolean	inupdate;

	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);

// also echo to debugging console
	Sys_Printf ("%s", msg);	// also echo to debugging console

// log all messages to file
	if (con_debuglog)
		Con_DebugLog( /* va("%s/qconsole.log",com_gamedir), */ "%s", msg);  // JPG - got rid of filename

	if (!con_initialized)
		return;
}


/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void Con_DPrintf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	if (!developer.value)
		return;			// don't confuse non-developers with techie stuff...

	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);

	Con_Printf ("%s", msg);
}


/*
==================
Con_SafePrintf

Okay to call even when the screen can't be updated
==================
*/
void Con_SafePrintf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[1024];

	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);

	Con_Printf ("%s", msg);
}
