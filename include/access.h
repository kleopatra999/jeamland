/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	access.h
 * Function:
 **********************************************************************/
#define BANNED_ALL	1
#define BANNED_ABA	2
#define BANNED_NEW	3

struct banned_site {
	char *host;
	char *reason;
	int level;
	struct banned_site *next;
	};

