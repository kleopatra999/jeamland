/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	termcap.h
 * Function:	Mini terminal capability database
 **********************************************************************/

struct termcap_entry {
	char *name;

	char *bold;
	char *yellow;
	char *red;

	char *reset;
	char *cls;
	};

#define TERMCAP { \
   { "xterm",\
	"\033[1m", \
	"\033[1m", \
	"\033[1m", \
	"\033[m", \
	"\033[H\033[2J", }, \
   { "vt100", \
	"\033[1m", \
	"\033[1m", \
	"\033[1m", \
	"\033[m", \
	"\033[;H\033[2J", }, \
   { "colour_vt100", \
	"\033[1m", \
	"\033[1;33m", \
	"\033[1;31m", \
	"\033[2;37;0m", \
	"\033[;H\033[2J", }, \
   { "tvi912", \
	"\033l", \
	"\033l", \
	"\033l", \
	"\033m", \
	"\032", }, \
   { "sun", \
	"\033[1m", \
	"\033[1;33H", \
	"\033[1;31H", \
	"\033[2;37;0m", \
	"\014", }, \
   { NULL, NULL, NULL, NULL, NULL, NULL } }

