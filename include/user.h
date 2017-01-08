/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	user.h
 * Function:
 **********************************************************************/

#define MAX_INET_ADDR	30
#define BUFFER_SIZE	1024
#define USER_BUFSIZE	2048
#define EMAIL_ADDR_LEN	50
#define NUM_JOBS	6

/* User flags */
#define	U_SOCK_CLOSING		0x1
#define U_SOCK_QUITTING		0x2
#define	U_NO_ECHO		0x4
#define U_LOGGING_IN		0x8
#define U_NOESCAPE		0x10
#define U_AFK			0x20
#define U_CONSOLE		0x40
#define U_INED			0x80
#define U_UNBUFFERED_TEXT	0x100
#define U_IN_MAILER		0x200
#define U_LOOP_PASSWD		0x400
#define U_EOR_OK		0x800
#define U_SKIP_SENTENCES	0x1000
#define U_QUIET			0x2000
#define U_DEBUG_ALIAS		0x4000
#define U_ALIAS_FB		0x8000
#define U_INHIBIT_QUIT		0x10000

/* User saveable flags */
#define U_FPC			0x1
#define U_EMAIL_PRIVATE		0x2
#define U_INVIS			0x4
#define U_ANSI			0x8
#define U_SHOUT_CURSE		0x10
#define U_NO_PURGE		0x20
#define U_BEEP_CURSE		0x40
#define U_EXTRA_NL		0x80
#define U_CONVERSE_MODE		0x100
#define U_EMAIL_VALID		0x200
#define U_BRIEF_MODE		0x400
#define U_NOSNOOP		0x800
#define U_TUSH			0x1000
#define U_SLY_LOGIN		0x2000
#define U_NO_ECHOBACK		0x4000
#define U_SHUTDOWN_NOKICK	0x8000
/*#define U_UNUSED		0x10000*/
#define U_BEEP_LOGIN		0x20000
#define U_NO_IDLEOUT		0x40000

/* Earmuffs flags */
#define EAR_INTERMUD		0x1
#define EAR_SHOUTS		0x2
#define EAR_CHIME		0x4
#define EAR_NOTIFY		0x8
#define EAR_CDUDP		0x10
#define EAR_TCP			0x20
#define EAR_UDP			0x40
#define EAR_DCHAT		0x80
#define EAR_JLCHAN		0x100
#define EAR_INTERADMIN		0x200
#define EAR_INTERCODE		0x400

/* Ed & Mail options */
#define U_EDOPT_AUTOLIST	0x1
#define U_EDOPT_RULER		0x2
#define U_EDOPT_INFO		0x4
#define U_MAILOPT_NEW_ONLY	0x8
#define U_EDOPT_LEFT_RULER	0x10

#define LEVEL_NAMES { \
	"VISITOR", \
	"RESIDENT", \
	"CITIZEN", \
	"WARDEN", \
	"CONSUL", \
	"OVERSEER" }

#define L_VISITOR		0
#define L_RESIDENT		1
#define L_CITIZEN		2
#define L_WARDEN		3
#define L_CONSUL		4
#define L_OVERSEER		5
#define L_MAX			L_OVERSEER

/* Access Levels - should extend this list some day..*/
#define A_SEE_FNAME	L_WARDEN
#define A_SEE_IP	L_WARDEN
#define A_SEE_HEMAIL	L_WARDEN
#define A_SEE_EXINFO	L_WARDEN
#define A_SEE_LASTMOD	L_CONSUL
#define A_SEE_UNAME	L_CONSUL
#define A_MBS_ADMIN	L_CONSUL
#define A_NO_SUICIDE	L_WARDEN
#define A_GRUPE_ADMIN	L_CONSUL
#define A_JLM_OPTS	L_OVERSEER

#define GENDER_TABLE { \
	"M", "male", "he", "his", "him", "himself", \
	"F", "female", "she", "her", "her", "herself", \
	"N", "neuter", "it", "its", "it", "itself", \
	"P", "plural", "they", "their", "them", "themselves", \
	}

#define G_LETTER	0
#define G_ABSOLUTE	1
#define G_PRONOUN	2
#define G_POSSESSIVE	3
#define G_OBJECTIVE	4
#define G_SELF		5
#define G_INDICES	6

#define G_UNSET		-1
#define G_MALE		0
#define G_FEMALE	1
#define G_NEUTER	2
#define G_PLURAL	3
#define G_MAX		4

/* Finger source flags */
#define FINGER_LOCAL	0
#define FINGER_INETD	1
#define FINGER_SERVICE	2

struct user {
	/* Variables saved in the user file */
	char 		*name;		/* Name currently in use */
	char 		*capname;	/* Properly capitalised name */
	char 		*rlname;	/* Real name */
	char 		*passwd;
	char		*email;
	char		*url;
	char		*title;
	char 		*prompt;
	char 		*plan;
	char		*startloc;	/* Filename of start location */
	char 		*validator;
	char		*last_ip;	/* Where last login was from */
	char		*comment;	/* Administrator's comment on user */
	char		*leavemsg;
	char		*entermsg;
	int		level;
	int 		gender;
	unsigned long	saveflags;
	int		medopts;
	int		terminal;	/* Terminal type */
	int		screenwidth;
	int		morelen;
	int		earmuffs;
	int		bad_logins;	/* No of previous bad logins */
	int		postings;	/* Number of messages posted. */
	int		num_logins;
	time_t		last_login;
	time_t		time_on;
	time_t		hibernate;
	time_t		last_watch;
	int		alias_lim;
	int		alias_indent;
	char		quote_char;
	struct alias 	*alias;
	struct stack	alias_stack;
	struct umbs	*umbs;
	struct grupe	*grupes;
	struct strlist	*watch;

	/* Variables not saved in the user file */
	struct vector	*history;
	int		history_ptr;
	char		*reply_to;
	char 		*mbs_current;
	unsigned long	flags;
	time_t		login_time;
	time_t		last_command;

	/* Temporary variables */
	int		sudo;
	struct stack	stack;
	struct stack	atexit;
	struct ed_buffer *ed;
	struct more_buf *more;
	struct board 	*mailbox;

	/* Socket variables */
	int		col;		/* Used by the line-wrapper */
	int 		fd;		/* File descriptor */
	int		snoop_fd;	/* Super snoop file fd */
	struct sockaddr_in addr;	/* Remote internet address */
	char		*uname;		/* Remote username */
	int		lport;		/* Local tcp port */
	int		rport;		/* Remote tcp port */
	int		sendq, recvq;	/* Count of sent and received bytes */
	char		*input_buffer;	/* Um... ;-) */
	int		ib_offset;	/* Offset into input buffer */

	/* Telnet code handling */
	struct {
		enum { TS_IDLE, TS_IAC, TS_WILL, TS_WONT, TS_DO, TS_DONT
			} state;
#define TN_EXPECT_ECHO	0x1
#define TN_EXPECT_EOR	0x2
		int expect;
		} telnet;

	/* Job control - sched command */
	struct {
#define JOB_USED		0x1
#define JOB_QUIET		0x2
#define JOB_INHIBIT_QUIT	0x4
		int event;
		int flags;
		} jobs[NUM_JOBS];

	/* Volatile pointers */
	struct object	*ob;
	struct room	*super;
	void 		(*input_to)(struct user *, char *);
	struct user	*snooping;
	struct user	*snooped_by;
	struct user 	*next;
	    };

