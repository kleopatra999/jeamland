/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	object.h
 * Function:
 **********************************************************************/

#define T_USER	0x1
#define T_ROOM	0x2
#define T_JLM	0x4

union m {
	struct user *user;
	struct room *room;
	struct jlm *jlm;
	};

struct object {
	unsigned long id;
	unsigned short type;
	time_t create_time;
	union m m;
	struct object *contains, *next_contains;
	struct object *super;
	struct object *next;

	struct sent *export, *inv;
	};

