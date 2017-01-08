/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	board.h
 * Function:
 **********************************************************************/

#define B_ANON		0x1
#define B_NEWM		0x2
#define B_CRYPTED	0x4

#define M_MSG_READ	0x1
#define M_DELETED	0x2
#define M_TOMAIL	0x4	/* Used by the M command in the mailer */
#define M_CRYPTED	0x8

struct message {
	int flags;
	char *text;
	char *subject;
	char *poster;
	char *cc;	/* Used by the mailer */
	time_t date;
	struct message *next;
	};

struct board {
	char *fname;
	char *archive;
	char *followup;
	int num;
	int limit;
	int flags;
	char *read_grupe, *write_grupe;
	int lastread;
	struct message *messages, *last_msg;
	};

