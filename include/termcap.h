/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	termcap.h
 * Function:	Mini terminal capability database
 **********************************************************************/

struct termcap_entry {
	char *name;
	char *desc;
	char *bold;
	char *colstr_s;
	char *colstr_e;
	char *reset;
	char *fullreset;
	char *cls;
	};

#define TERMCAP { \
   { "xterm", "Xwindows terminal emulator.", \
	"\033[1m", \
	NULL, \
	NULL, \
	"\033[m", \
	"\033[r\033<\033[m\033[H\033[2J\033[?7h\033[?1;3;4;6l", \
	"\033[H\033[2J", }, \
   { "colxterm", "Xwindows terminal emulator supporting colour.", \
	"\033[1m", \
	"\033[", \
	"m", \
	"\033[m", \
	"\033c\033[r\033<\033[m\033[H\033[2J\033[?7h\033[?1;3;4;6l", \
	"\033[H\033[2J", }, \
   { "vt100", "vt100 or compatible terminal.", \
	"\033[1m", \
	NULL, \
	NULL, \
	"\033[m", \
	"\033c\033>\033[?3l\033[?4l\033[?5l\033[?7h\033[?8h", \
	"\033[;H\033[2J", }, \
   { "colvt100", "vt100 or compatible terminal supporting colour.", \
	"\033[1m", \
	"\033[", \
	"m", \
	"\033[m", \
	"\033c\033>\033[?3l\033[?4l\033[?5l\033[?7h\033[?8h", \
	"\033[;H\033[2J", }, \
   { "wyse30", "Wyse Technology Terminal.", \
	"\033)", \
	NULL, \
	NULL, \
	"\033(", \
	"\033c\033*", \
	"\033*", }, \
   { "sun", "Sun workstation console.", \
	"\033[7m", \
	NULL, \
	NULL, \
	"\033[m", \
	"\033[1r", \
	"\014", }, \
   { "broken_colvt100", "Colour vt100 terminal with broken colour.", \
	"\033[1m", \
	"\033[", \
	"m", \
	"\033[22;2;37;40;0m", \
	"\033c\033>\033[?3l\033[?4l\033[?5l\033[?7h\033[?8h", \
	"\033[;H\033[2J", }, \
   { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL } }

