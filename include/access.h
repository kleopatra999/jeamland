/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	access.h
 * Function:
 **********************************************************************/

enum siteban_level { BANNED_INV = -1, BANNED_ALL = 1, BANNED_ABA, BANNED_NEW,
		     BANNED_MAX };

struct banned_site {

	unsigned long	addr;
	unsigned long	netmask;

	char *reason;
	enum siteban_level level;

	struct banned_site *next;
	};

