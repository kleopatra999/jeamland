/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	user.h
 * Function:
 **********************************************************************/

#define BUFFER_SIZE	1024
#define NUM_JOBS	6

/* User flags */
#define	U_SOCK_CLOSING		0x1
#define U_INHIBIT_ALIASES	0x2
#define	U_NO_ECHO		0x4
#define U_LOGGING_IN		0x8
#define U_NOESCAPE		0x10
#define U_AFK			0x20
#define U_CONSOLE		0x40
#define U_INED			0x80
#define U_UNBUFFERED_TEXT	0x100
#define U_IN_MAILER		0x200
#define U_LOOP_PASSWD		0x400
#define U_INHIBIT_QUIT		0x800
#define U_SKIP_SENTENCES	0x1000
#define U_QUIET			0x2000
#define U_DEBUG_ALIAS		0x4000
#define U_ALIAS_FB		0x8000
#define U_COLOURED		0x10000
#define U_SUDO			0x20000

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
#define U_IACEOR		0x1000
#define U_SLY_LOGIN		0x2000
#define U_NO_ECHOBACK		0x4000
#define U_SHUTDOWN_NOKICK	0x8000
#define U_IACGA			0x10000
#define U_BEEP_LOGIN		0x20000
#define U_NO_IDLEOUT		0x40000
#define U_NOEMAIL		0x80000

/* Earmuffs flags */
#define EAR_SHOUTS		0x1
#define EAR_CHIME		0x2
#define EAR_NOTIFY		0x4
#define EAR_TCP			0x8
#define EAR_UDP			0x10
#define EAR_RNCLIENT		0x20
#define EAR_I3			0x40

/* Ed & Mail options */
#define U_EDOPT_AUTOLIST	0x1
#define U_EDOPT_RULER		0x2
#define U_EDOPT_INFO		0x4
#define U_MAILOPT_NEW_ONLY	0x8
#define U_EDOPT_LEFT_RULER	0x10
#define U_EDOPT_REECHO		0x20
#define U_EDOPT_WRAP		0x40
#define U_MAILOPT_SIG		0x80
#define U_EDOPT_SIG		0x100
#define U_EDOPT_PROMPT		0x200

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
#define A_GALIASES	L_CONSUL
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
	unsigned long	version;	/* Version of talker last saved on */
	char 		*name;		/* Name currently in use */
	char 		*capname;	/* Properly capitalised name */
	char 		*rlname;	/* Real name */
	char 		*passwd;	/* Encrypted password */
	char		*email;		/* Email address */
	char		*url;		/* Homepage URL */
	char		*title;		/* Title */
	char 		*prompt;	/* Current prompt */
	char 		*plan;		/* Plan message */
	char		*sig;		/* .Sig message */
	char		*startloc;	/* Filename of start location */
	char 		*validator;	/* Who validated this user */
	char		*last_ip;	/* Where last login was from */
	char		*comment;	/* Administrator's comment on user */
	char		*leavemsg;	/* Message used when leaving a room */
	char		*entermsg;	/* Message used when entering a room */
	int		level;		/* User level */
	int 		gender;		/* Gender (See G_xxx above) */
	unsigned long	saveflags;	/* Saveable flags */
	unsigned int	medopts;	/* More and Ed Options */
	unsigned int	terminal;	/* Terminal type */
	unsigned int	screenwidth;	/* Width of screen for linewrap */
	unsigned int	morelen;	/* Page length for more */
	unsigned int	earmuffs;	/* Earmuff values */
	unsigned int	bad_logins;	/* No of previous bad logins */
	unsigned int	postings;	/* Number of messages posted. */
	unsigned int	num_logins;	/* Number of times logged in */
	int		tz_adjust;	/* Adjustment to local time. */
	time_t		last_login;	/* Time of last login */
	time_t		time_on;	/* Total time on (seconds) */
	time_t		hibernate;	/* 'Hibernate until' time */
	time_t		last_watch;	/* Last time watch was caught up */
	int		alias_lim;	/* Alias limit */
	char		quote_char;	/* Character user when quoting text */
	struct alias 	*alias;		/* List of aliases */
	struct umbs	*umbs;		/* Subscribed mbs list */
	struct grupe	*grupes;	/* Private grupes */
	struct strlist	*watch;		/* Watched files */
	struct strlist	*chan_earmuff;	/* Channel earmuff values */
	struct mapping	*attr;		/* Colour attribute flags */
	struct mapping	*uattr;		/* Per user attribute flags */

	/* Variables not saved in the user file */
	struct vector	*history;	/* User's command history */
	int		history_ptr;	/* Pointer into history */
	char		*reply_to;	/* Last user 'tell' received from */
	char 		*mbs_current;	/* Current board under mbs */
	unsigned long	flags;		/* Temporary flags */
	time_t		login_time;	/* Time logged in */
	time_t		last_command;	/* Time of last command */
	int		alias_indent;	/* Used with 'alias debug' */
	struct stack	alias_stack;	/* Stack used during alias parsing */
	int		last_note;	/* Last note read on board */

	/* Temporary variables */
	struct stack	stack;		/* Temporary variable stack */
	struct stack	atexit;		/* Atexit function stack */
	struct ed_buffer *ed;		/* Editor buffer */
	struct more_buf *more;		/* Pager buffer */
	struct board 	*mailbox;	/* Mailbox */

	/* Socket variables */
	int		col;		/* Used by the line-wrapper */
	int		snoop_fd;	/* Super snoop file fd */
	char		*uname;		/* Remote username */
	struct tcpsock	socket;		/* TCP Socket */

	/* Job control - sched command */
	struct {
#define JOB_USED		0x1
#define JOB_QUIET		0x2
#define JOB_INHIBIT_QUIT	0x4
		int event;
		int flags;
		} jobs[NUM_JOBS];	/* Running jobs list */

	/* Volatile pointers */
	struct object	*ob;		/* Object linked to user */
	struct room	*super;		/* User's environment */
	void 		(*input_to)(struct user *, char *);
					/* Function for redirected input */
	struct user	*snooping;	/* Who this user is snooping */
	struct user	*snooped_by;	/* Who is snooping this user */
	struct user 	*next;		/* Next user in chain */
	    };

