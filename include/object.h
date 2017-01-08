/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	object.h
 * Function:
 **********************************************************************/

#define OT_EMPTY	0x0
#define OT_USER		0x1
#define OT_ROOM		0x2
#define OT_JLM		0x4

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
	unsigned long num_contains;
	struct object *contains, *next_contains;
	struct object *super;
	struct object *next;

	struct sent *export, *inv;
	};

