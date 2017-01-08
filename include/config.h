/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	config.h
 * Function:	Main JeamLand configuration file.
 *		You are advised to read the entire file and change
 * 		anything which is not correct for your setup.
 *		The most frequently changed options are near the top
 *		of the file.
 **********************************************************************/

/****
 * The version number.. */
#define VERSION		"1.0.2"
#define ERQD_VERSION	"1.3"
#define INETD_VERSION	"1.6"
#define MAIL_VERSION	"2.3"

/****
 * Set the local name of the talker, otherwise the interhost system will
 * become confused !*/
#define LOCAL_NAME	"Unconfigured"

/****
 * Define where the talker code lives. */
#define TOP_DIR 	"/home/af/jeamland"
#define LIB_PATH	TOP_DIR"/lib"
#define BIN_PATH	TOP_DIR"/bin"

/****
 * Do you want JeamLand to produce a backtrace if it crashes ?
 * Should only be necessary when you have a naff debugger
 * takes up memory/cpu to store the funstack.. but is quite useful
 */
#define CRASH_TRACE

/****
 * The port the talker listens on (can still be overidden with the -p
 * boot option. */
#define DEFAULT_PORT	4242
/* These can be replaced with absolute numerical values if desired..
 * although that would not allow them to be relative to a boot port
 * selected with the -p option */
#define INETD_PORT	(port - 1)
#define CDUDP_PORT	(port - 2)
#define SERVICE_PORT	(port - 3)
#define MAX_SERVICES	7

/****
 * Given as the admin's email address. */
#define OVERSEER_EMAIL	"Jeamland@twikki.demon.co.uk"

/****
 * The filename of the entrance room. */
#define ENTRANCE_ROOM	"_Entry"
 
/****
 * Define if JeamLand cannot work out the correct host name for the machine
 * it should only be necessary on some suns or when you wish to give an alias
 * instead of major name.
 */
/*#define OVERRIDE_HOST_NAME 	"host.domain.name"*/

/****
 * Define if you want a timezone adjustment.
 * In hours. */
/*#define TIMEZONE_ADJUSTMENT	0*/

/****
 * Define if you want the talker to run with a specific umask.
 */
#define RUN_UMASK	077

/****
 * Define if you want the talker to run as a specific user or group.
 * NB: on some systems, this involves making the JeamLand binary setuid and/or
 *     setgid.
 */
/*#define RUN_USER	"jeamland"*/
/*#define RUN_GROUP	"jeamland"*/

/****
 * Force new users to accept a set of conditions ? */
#define REQUIRE_CONDITIONS

/****
 * Some limits. */
#define MAX_USERS	50
#define NAME_LEN	15
#define TITLE_LEN	40
#define PROMPT_LEN	40
#define BOARD_LIMIT	20
#define GRUPE_LIMIT	10
#define GRUPE_EL_LIMIT	15
#define IDLE_LIMIT	(30 * 60)
/* At login prompt.. */
#define LOGIN_TIMEOUT		60
#define PASSWORD_RETRIES	2

/****
 * Some defaults. */
#define AUTOSAVE_FREQUENCY	(15 * 60)
#define LOADAV_INTERVAL		(5 * 60)
#define CLEANUP_FREQUENCY	(10 * 60)
#define DEFAULT_SCREENWIDTH	80
#define DEFAULT_QUOTE_CHAR	'>'
#define DEFAULT_ENTERMSG	"%N arrives from %r."
#define DEFAULT_LEAVEMSG	"%N goes to %r."

/****
 * If ALIAS_EXITS is defined, common directions such as 'n' for 'north'
 * will be expanded. */
#define ALIAS_LIMIT	20
#undef ALIAS_EXITS

/****
 * Execution will abort if EVAL_DEPTH is exceeded. */
#define EVAL_DEPTH	15

/****
 * Recursive grupe expansion will abort if GRUPE_RECURSE_DEPTH is exceeded. */
#define GRUPE_RECURSE_DEPTH	5

/****
 * Define if you want all aliases not containing a $x sequence to be treated
 * as if they end with $*.
 * NB: This behaviour can cause problems with compound aliases
 *     and is not recommended.
 */
#undef ALIAS_AUTO_ADD_ARGS

/****
 * How big should each users history be ?
 */
#define HISTORY_SIZE 20

/****
 * The default prompts.
 * See 'help cookies' for the values which can go in PROMPT
 * NB: ED_PROMPT and MAIL_PROMPT can not currently contain cookies.
 */
#define PROMPT		"%p "
#define ED_PROMPT	": "
#define MAIL_PROMPT	"& "

/****
 * How do you want 'say' messages to look ?
 * the first %s is replaced by the users name, the second by the message.
 * If you use " characters, they must be escaped with a \  */
#define SAY_FORMAT	"%s says '%s'\n"
#define YSAY_FORMAT	"You say '%s'\n"

/****
 * Which aliases should be the 'startup' and 'shutdown' aliases ?
 * These can be undefined if required. */
#define STARTUP_ALIAS 	"_login"
#define SHUTDOWN_ALIAS 	"_logout"

/****
 * If these are defined, then if the room contains an ARRIVE_ALIAS, the user
 * will execute it as they enter the room, and if it contains a DEPART_ALIAS,
 * the user will execute it as they leave.
 */
#define ARRIVE_ALIAS	"_arrive"
#define DEPART_ALIAS	"_depart"

/****
 * How many lines does the pager assume on a page by default ?
 * (Users can change individually with the 'morelen' command) */
#define DEFAULT_MORELEN	22

/****
 * How big are the logs allowed to get before they are renamed to log~
 * Undefine for no cyclic checking.
 * This is in bytes */
#define CYCLIC_LOGS	15000

/****
 * Allow cookies which are unknown ? */
#undef IGNORE_UNKNOWN_COOKIES

/****
 * Purge defines. You can undef any of these which you don't want.
 * Undefining PURGE_HOUR stops all _automatic_ purges as does the
 * existance of a file in etc/ called nopurge. */
/* Midnight */
#define PURGE_HOUR 0
/* How much login time do you need to be considered a non-newbie
 * One day here. */
#define NEWBIE_TIME_ON	(3600 * 24)
#define PURGE_NEWBIE	(20 * 3600 * 24)
#define PURGE_RESIDENT 	(50 * 3600 * 24)
#define PURGE_CITIZEN 	(80 * 3600 * 24)
#define PURGE_WARDEN	(150 * 3600 * 24)
/* Don't purge these two levels.. */
#undef PURGE_CONSUL
#undef PURGE_OVERSEER

/****
 * Do you want all passwords to be checked for security and rejected if
 * considered insecure ? */
#define SECURE_PASSWORDS

/****
 * Do you want statistics logged every so often ?
 * And if so, do you want an automatic shutdown if the resident set size
 * becomes too large ?
 * You can undefine LOG_STATISTICS while leaving the rest intact
 * This will stop the writing to the log file, but still keep updating
 * the information shown by the 'rusage' command.
 * Similarly, you can undefine RSS_MAX
 */
#define LOG_STATISTICS 		"stats"
#define STATISTICS_INTERVAL 	(60 * 15)
#define RSS_MAX			2000000
#define RSS_MAX_DELAY		(60 * 5)

/****
 * Hash soul commands ? - speeds up command parsing and lowers CPU usage
 * at the expense of a small amount of memory.
 * (about 1300 bytes with the distribution feelings database)
 */
#define HASH_FEELINGS

/****
 * Hash normal commands ? see above. */
#define HASH_COMMANDS

/****
 * Hash function aliases ? see above. */
#define HASH_ALIAS_FUNCS

/****
 * Hash jlm functions ? see above. */
#define HASH_JLM_FUNCS

/****
 * Keep audit files ? - recommended unless you are short of diskspace!
 * The example crontab shows a nice method of archiving these */
#define AUDIT_COMMANDS

/****
 * Do you want all connections logged ? */
#define LOG_CONNECTIONS

/****
 * Store stats on command usage ?
 * Increases memory overhead and CPU usage but can be interesting ;)
 */
#define PROFILING

/****
 * Do you want everyone to be automatically validated ?
 * Looks like a nice way to fill up your disk.. */
#undef AUTO_VALIDATE

/****
 * Do you want the talker to send email validation letters to people who
 * change their email address ?
 * Setting this up is complicated, see the file doc/autovalemail
 * This define both enables the autovalemail system and defines the password
 * used at the service port. */
/*#define AUTO_VALEMAIL put_password_here*/

/****
 * Some default messages.. */
#define NEWBIE_TITLE	"is new to "LOCAL_NAME"."
#define VOID_DESC \
	    "You hang among the ether.\n" \
	    "Reality has encountered trouble and so you find yourself\n" \
	    "floating here.\n"
#define NEW_ROOM_DESC \
	"This is a brand spanking new room.\nThere's nothing here!\n\n" \
	"Use 'desc' to set this rooms description.\n" \
	"Use 'name' to set this rooms name.\n"
/* NB: the \r's are important.. this is a raw written message */
#define FULL_MSG \
	"\n\r\n\r" \
	"Sorry, "LOCAL_NAME" is full at present.\n\r" \
	"Please try again later.\n\r"

/****
 * Allow embedded newlines ? (ie.. a \ at the end of a line enables the user
 * to carry on on the next line. */
#define EMBEDDED_NEWLINES

/****
 * Super snoop allows you to snoop users even while you are not on.
 * Everything written to or read from that users socket will be written
 * to a file in snoops/<username>
 * This list of users to snoop is in snoops/snooped and contains one user
 * per line.
 * This option also enables the 'harrassment' command */
#define SUPER_SNOOP

/****
 * If SUPER_SNOOP is defined, the harassment command exists which allows
 * users to start a supersnoop on another user who is harassing them.
 * For the next HARASS_LOG_TIME seconds, the user will be supersnooped */
#define HARASS_LOG_TIME	(15 * 60)

/****
 * Super snoop ALL users ?
 * This would be rather silly.. even with a lot of diskspace. */
#undef SUPERSNOOP_ALL

/****
 * Log information about all mallocs and frees
 * This should not be used for normal running as it increases memory and
 * CPU usage, but should be used if you are going to change any of the code
 * for testing that you have not introduced any memory leaks.
 * The 'dm' command becomes available which writes out the malloc table to
 * log/debug_malloc
 *
 * If you also define DEBUG_MALLOC_FUDGE then the malloc wrapped will check
 * to see if more memory than allocated was used at any point. It does this
 * by allocating more memory than actually required and then filling either
 * side of the memory with some known values which can be checked when the
 * memory block is freed. NB: This works on Linux, it may not work on your
 * machine! */
/*#define DEBUG_MALLOC*/
/*#define DEBUG_MALLOC_FUDGE*/

/****
 * Check various things. This should not be needed for normal operation
 * but I recommend that it is defined if you change any of the source.
 * Does little harm in any case. */
#define DEBUG

/****
 * Keep basic statistics on memory management to be shown with the
 * 'meminfo' command. Leaving this defined will consume slightly more
 * CPU time than otherwise, but this should be negligable and it does
 * provide a good indication of memory leaks. */
#define MALLOC_STATS

/****
 * Keep basic statistics on string buffers to be shown with the 'strbufs'
 * command. Leaving this defined will consume slightly more CPU time than
 * otherwise, but this should be negligable. */
#define STRBUF_STATS

/****
 * Does your system have an rusage call ? (or in the case of a Solaris
 * machine, a times() call.) */
#define RUSAGE

/****
 * Change the process name ? */
#define SETPROCTITLE

/****
 * Nice the process ? (ie. lower its execution priority)
 * Only happens when the talker is booted via the angel. */
#define NICE 5

/****
 * Enable the UDP system ? */
#define UDP_SUPPORT
/* Support the CDUDP system as well ? */
#define CDUDP_SUPPORT

/* You probably don't want these.. unless you're the nosy type ;-) */
#undef DEBUG_UDP
#undef DEBUG_SERVICES

/* These are probably ok - used by the intermud system, these values are taken
 * directly from Mark Lewis' LPMud implementation */
#define REPLY_TIMEOUT	12
#define RETRIES		2
#define PAST_ID_TIMEOUT	120
/* Two days. */
/*#define HOST_TIMEOUT	(24 * 3600 * 2)*/
/* These two are for interhost mail. */
/* 30 minutes and 24 hours.. */
#define MAIL_RETRY	(30 * 60)
#define MAIL_RETRIES	48

/****
 * Don't change these unless you know what they mean! */
#define MAX_ARGV	5
#define ROOT_USER	"root"
#define IPTABLE_SIZE	((MAX_USERS * 3) / 2)
#define MAX_PACKET_LEN	1024
#define FIT_PACKET_LEN	950

