/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	alias.h
 * Function:
 **********************************************************************/

struct alias {
	char 		*key;
	char		*fob;
	struct alias 	*next;
	     };
