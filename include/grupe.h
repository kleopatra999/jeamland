/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	grupe.h
 * Function:
 **********************************************************************/

struct grupe_el {
	char *name;
	long flags;
	struct grupe *grp;
	struct grupe_el *next;
	};

struct grupe {
	char *gid;
	long flags;
	struct grupe_el *el;
	struct grupe *next;
	};

