/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	master.h
 * Function:
 **********************************************************************/

struct db_entry {
	char *user;
	char *setter;
	int level;
	struct db_entry *next;
	};

#ifdef AUTO_VALEMAIL
struct valemail {
	char *user;
	char *id;
	char *email;
	time_t date;
	struct valemail *next;
	};
#endif

