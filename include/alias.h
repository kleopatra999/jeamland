/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	alias.h
 * Function:
 **********************************************************************/

struct alias {
	char 		*key;
	char		*fob;
	struct alias 	*next;
	     };
