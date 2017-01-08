/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	system.h
 * Function:	Global system flags / datatypes.
 *		Definition of svalue.
 **********************************************************************/

#define SYS_SHUTDWN	0x1
#define SYS_SHOWLOAD	0x2
#define SYS_LOG		0x4
#define SYS_NOERQD	0x8
#define SYS_NOPROCTITLE	0x10
#define SYS_BACKGROUND	0x20
#define SYS_EMPTY	0x40
#define SYS_CHECK_HELPS 0x80
#define SYS_NOPING	0x100
#define SYS_KILLOLD	0x200
#define SYS_CONSOLE	0x400
#define SYS_NOIPC	0x800
#define SYS_NOUDP	0x1000
#define SYS_NOSERVICE	0x2000
#define SYS_EVENT	0x4000

enum dump_mode { DUMP_CAT = 0, DUMP_TAIL, DUMP_HEAD, DUMP_MORE };
enum valid_mode { VALID_READ = 0, VALID_WRITE };

#define NULL_INPUT_TO	(void (*)(struct user *, char *))NULL

struct ipentry {
	unsigned long addr;
	char *name;
	};

struct vector;

union u {
	char *string;
	long number;
	struct vector *vec;
	void *pointer;
	void (*fpointer)();
	};

#define T_EMPTY		0
#define T_NUMBER	1
#define T_STRING	2
#define T_POINTER	3
#define T_VECTOR	4
#define T_FPOINTER	5

struct svalue {
	int type;
	union u u;
	};

