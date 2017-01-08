/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	room.h
 * Function:
 **********************************************************************/

#define R_LOCKED	0x1
#define R_BOARD		0x2

struct exit {
	char *name;
	char *file;
	struct exit *next;
	};

struct alias;

struct room {
	char *name;
	char *fname;
	char *owner;
	char *long_desc;
	char *lock_grupe;
	int flags;
	int inhibit_cleanup;
	int alias_lim;

	struct alias *alias;
	struct object *ob;
	struct exit *exit;
	struct board *board;
	struct room *next;
	};

