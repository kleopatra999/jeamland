/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	config.h
 * Function:	Main JeamLand configuration file.
 *		You are advised to read the entire file and change
 * 		anything which is not correct for your setup.
 *		The most frequently changed options are near the top
 *		of the file.
 **********************************************************************/

/****
 * The version number..
 */
#define VERSION		"1.2.2"
#define JL_REL_DATE	"Fri Oct 03 16:27:54 1997"
#define SH_JL_REL_DATE	"03/10/1997"
#define ERQD_VERSION	"2.0"
#define INETD_VERSION	"2.0"
#define MAIL_VERSION	"2.0"

/****
 * Set the local name of the talker, otherwise the interhost system will
 * become confused !
 */
#define LOCAL_NAME	"Unconfigured"

/****
 * Define where the talker code lives.
 */
#define TOP_DIR		"/home/talker/jeamland"
#define LIB_PATH	TOP_DIR"/lib"
#define BIN_PATH	TOP_DIR"/bin"

/****
 * Do you want JeamLand to produce a backtrace if it crashes ?
 * Takes up memory/cpu to store the funstack.. but is very useful
 */
#define CRASH_TRACE

/****
 * The port the talker listens on (can still be overidden with the -p
 * boot option.)
 */
#define DEFAULT_PORT	4242
/* These can be replaced with absolute numerical values if desired..
 * although that would not allow them to be relative to a boot port
 * selected with the -p option
 */
#define INETD_PORT		(port - 1)
#define CDUDP_PORT		(port - 2)
#define SERVICE_PORT		(port - 3)
#define MAX_SERVICES		5
#define REMOTE_NOTIFY
/* If you change this one, you will need to alter the rnclient source code */
#define NOTIFY_PORT		(port - 4)
#define MAX_REMOTE_NOTIFY	20
#define REMOTE_NOTIFY_TIMEOUT	600
/* Try sending 3 packets (1 + 2 retransmissions) at 3 second intervals */
#define RNCLIENT_RETRANSMIT_COUNT	2
#define RNCLIENT_RETRANSMIT_DELAY	3

/****
 * Given as the admin's email address.
 */
#define OVERSEER_EMAIL	"Jeamland@twikki.demon.co.uk"

/****
 * Entrance rooms.
 * You may have more than one entrance room as long as they all share the
 * same filename prefix, defined below.
 */
#define ENTRANCE_PREFIX	"_Entry"
/* Define this to allocate rooms in a round robin fashion. */
#undef ENTRY_ALLOC_RROBIN
/* Define this to allocate rooms based on the number of users in each.
 * The entrance rooms will be searched until one is found with less than
 * this number of users. If there is no such room, then the room with the
 * lowest user count is used.
 */
#define ENTRY_ALLOC_MAX	20
 
/****
 * Define if JeamLand cannot work out the correct host name for the machine.
 * This should only be necessary on some suns or when you wish to give an
 * alias instead of major name.
 */
/*#define OVERRIDE_HOST_NAME 	"host.name.here"*/

/****
 * Define if you want a timezone adjustment.
 * In hours.
 */
/*#define TIMEZONE_ADJUSTMENT	0*/

/****
 * Define if you want the talker to run with a specific umask.
 * (This is the paranoid setting.)
 */
#define RUN_UMASK	077

/****
 * Define this if you want JeamLand to attempt to change its file descriptor
 * limit. Very useful on solaris, where the default limit is 32.
 */
/*#define SETFDLIMIT	1024*/

/****
 * Force new users to accept a set of conditions ?
 */
#define REQUIRE_CONDITIONS

/****
 * Tune new users out of intermud channels by default ?
 * This helps to cut confusion!
 */
#define NEWBIE_TUNEOUT

/****
 * Some limits.
 */
/* This is the maximum user limit. It is only enforced by the talker software
 * and the host operating system file descriptor limit is the ultimate barrier,
 * but see the SETFDLIMIT define above */
#define MAX_USERS		50
#define NAME_LEN		15
#define TITLE_LEN		40
#define PROMPT_LEN		80
#define GRUPE_LIMIT		10
#define GRUPE_EL_LIMIT		15
/* Idle time allowed per user level
 * i.e. resident gets IDLE LIMIT, citizen gets twice that etc. etc. */
#define IDLE_LIMIT		(30 * 60)
/* If defined, minimum amount of idle time before afk is automatically
 * engaged.. */
#define IDLE_AUTO_AFK		(20 * 60)
#define USER_ATTR_LIMIT		10
/* At login prompt.. */
#define LOGIN_TIMEOUT		60
#define PASSWORD_RETRIES	2
/* In editor */
#define MAX_MAIL_LINES		1000
#define MAX_BOARD_LINES		500
/* This is in bytes. */
#define MAX_EDIT		200000

/****
 * Some defaults.
 * The odd time delays are to stop events happening simultaneously
 * too often.
 */
#define AUTOSAVE_FREQUENCY	(9 * 60)
#define LOADAV_INTERVAL		(5 * 60)
#define CLEANUP_FREQUENCY	(11 * 60)
#define DEFAULT_SCREENWIDTH	80
#define DEFAULT_MORELEN		22
#define DEFAULT_QUOTE_CHAR	'>'
#define DEFAULT_ENTERMSG	"%N arrives from %r."
#define DEFAULT_LEAVEMSG	"%N goes to %r."
#define BOARD_LIMIT		20
#define ALIAS_LIMIT		30

/****
 * Execution will abort if EVAL_DEPTH is exceeded.
 * If you have many complex aliases, you may need to increase this.
 */
#define EVAL_DEPTH	20

/****
 * Recursive grupe expansion will abort if GRUPE_RECURSE_DEPTH is exceeded.
 * This should be enough!
 */
#define GRUPE_RECURSE_DEPTH	10

/****
 * How big should each user's history be ?
 */
#define HISTORY_SIZE 20

/****
 * The default prompts.
 * See lib/help/cookies for the values which can go in PROMPT
 * NB: ED_PROMPT and MAIL_PROMPT can not currently contain cookies.
 */
#define PROMPT		"%p "
/* Cyan prompt.. */
#define COLOUR_PROMPT	"%cc.%p %cz"
#define ED_PROMPT	": "
#define MAIL_PROMPT	"& "

/****
 * How do you want 'say' messages to look ?
 * the first %s is replaced by the users name, the second by the message.
 * If you use " or % characters, they must be escaped with a \ 
 */
#define SAY_FORMAT	"%s says '%s'\n"
#define YSAY_FORMAT	"You say '%s'\n"

/****
 * Which aliases should be executed when a user logs in / out.
 * These can be undefined if required.
 */
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
 * How big are the logs allowed to get before they are renamed to log~
 * Undefine for no cyclic checking.
 * This is in bytes - if low on disk space, you will want to lower this! ;-)
 */
#define CYCLIC_LOGS	30000

/****
 * Allow cookies which are unknown ?
 */
#define IGNORE_UNKNOWN_COOKIES

/****
 * Purge defines. You can undef any of these which you don't want.
 * Undefining PURGE_HOUR stops all _automatic_ purges as does the
 * existance of a file in etc/ called nopurge.
 * NB: This should probably be moved to an external cron process which calls
 *     the service port to do the actual user deletion.
 */
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
 * considered insecure ?
 * A lot of people consider this too restrictive.
 */
#define SECURE_PASSWORDS

/****
 * Do you want the talker server to be able to act as a cron server.
 * If you do not have local access to "cron" then this may be useful to you.
 * Please see doc/cron for more details.
 */
#undef JL_CRON

/****
 * Do you want statistics logged every so often ?
 * And if so, do you want an automatic shutdown if the resident set size
 * becomes too large ?
 * You can undefine LOG_STATISTICS while leaving the rest intact
 * This will stop the writing to the log file, but still keep updating
 * the information shown by the 'rusage' command.
 * Similarly, you can undefine RSS_MAX
 */
/*#define LOG_STATISTICS 		"stats"*/
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
 * Hash normal commands ? see above.
 */
#define HASH_COMMANDS

/****
 * Hash function aliases ? see above.
 */
#define HASH_ALIAS_FUNCS

/****
 * Hash jlm functions ? see above.
 */
#define HASH_JLM_FUNCS

/****
 * Keep audit files ? - recommended unless you are short of diskspace!
 * These files will be placed in lib/secure/audit
 * The example crontab shows a method of archiving these
 */
#define AUDIT_COMMANDS

/****
 * Do you want all connections logged ?
 */
#define LOG_CONNECTIONS

/****
 * Keep a history of local channels ?
 * Some people believe this to be a breach of privacy..
 */
#define LOCAL_CHANHIST

/****
 * Store stats on command usage ?
 * Increases memory overhead and CPU usage slightly but can be interesting ;)
 */
#undef PROFILING

/****
 * Do you want everyone to be automatically validated ?
 * Looks like a nice way to fill up your disk..
 */
#undef AUTO_VALIDATE

/****
 * Do you want to require new users to register via email ? If you define
 * this, they will only be allowed to log on as 'guest'.
 */
#undef REQUIRE_REGISTRATION

/****
 * The following enable extra features of the service port.
 * In order to fully utilise these features, you will have to set up
 * an email processor which can direct email to a script.
 * Both of these defines also set the password which is required to use
 * the service.
 */

/*
 * Enable email-validation via the service port ?
 * See scripts/valemail
 */
/*#define AUTO_VALEMAIL "put_password_here"*/

/*
 * Enable incoming email via the service port ?
 * See scripts/xemail
 */
/*#define SERV_EMAIL "put_password_here"*/

/****
 * Some default messages..
 */
#define NEWBIE_TITLE	"is new to "LOCAL_NAME"."

#define VOID_DESC \
	    "You hang among the ether.\n" \
	    "Reality has encountered trouble and so you find yourself\n" \
	    "floating here.\n"

#define NEW_ROOM_DESC \
	"This is a brand spanking new room.\nThere's nothing here!\n\n" \
	"Use 'desc' to set this room's description.\n" \
	"Use 'name' to set this room's name.\n"

/* NB: the \r's are important.. this is a raw written message */
#define FULL_MSG \
	"\n\r\n\r" \
	"Sorry, "LOCAL_NAME" is full at present.\n\r" \
	"Please try again later.\n\r"

/****
 * Allow embedded newlines ? (ie.. a \ at the end of a line enables the user
 * to carry on on the next line.
 */
#define EMBEDDED_NEWLINES

/****
 * Super snoop allows you to snoop users even while you are not on.
 * Everything written to or read from that users socket will be written
 * to a file in lib/secure/snoops/<username>
 * This list of users to snoop is in lib/secure/snoops/snooped and contains
 * one user per line.
 * This option also enables the 'harrassment' command
 */
#define SUPER_SNOOP

/****
 * If SUPER_SNOOP is defined, the harassment command exists which allows
 * users to start a supersnoop on another user who is harassing them.
 * For the next HARASS_LOG_TIME seconds, the user will be supersnooped */
#define HARASS_LOG_TIME	(15 * 60)

/****
 * Super snoop ALL users ?
 * Another good way to fill up diskspace.
 */
#undef SUPERSNOOP_ALL

/****
 * Log information about all mallocs and frees
 * This should not be used for normal running as it increases memory and
 * CPU usage a LOT, but should be used if you are going to change any of the
 * code and want to check that you have not introduced any memory leaks.
 * The 'dm' command becomes available which writes out the malloc table to
 * log/debug_malloc.
 *
 * If you also define DEBUG_MALLOC_FUDGE then the malloc wrapper will check
 * to see if more memory than allocated was used at any point. It does this
 * by allocating more memory than actually required and then filling either
 * side of the memory with some known values which can be checked when the
 * memory block is freed.
 */
/*#define DEBUG_MALLOC*/
/*#define DEBUG_MALLOC_FUDGE*/

/****
 * Check various things. This should not be needed for normal operation
 * but I recommend that it is defined if you change any of the source.
 * Does little harm in any case.
 */
#define DEBUG

/****
 * Keep basic statistics on memory management to be shown with the
 * 'meminfo' command. Leaving this defined will consume slightly more
 * CPU time than otherwise, but this should be negligable and it does
 * provide a good indication of memory leaks.
 */
#define MALLOC_STATS

/****
 * Keep basic statistics on string buffers to be shown with the 'strbufs'
 * command. Leaving this defined will consume slightly more CPU time than
 * otherwise, but this should be negligable.
 */
#define STRBUF_STATS

/****
 * Does your system have an rusage call ? (or in the case of a Solaris
 * machine, a times() call.)
 */
#define RUSAGE

/****
 * Does your system have a socketpair() call ? ... if not, or if it does
 * not work properly, define this.
 * If you have any problems with jlms, try defining this.
 */
#undef BROKEN_SOCKETPAIR

/****
 * Do you want syslog messages written to console if in console mode ?
 */
#define CONSOLE_SYSLOG

/****
 * Change the process name ?
 */
#define SETPROCTITLE

/****
 * Nice the process ? (ie. lower its execution priority)
 * Only happens when the talker is booted via the angel.
 */
#define NICE 5

/****
 * Enable the UDP system ?
 */
#define INETD_SUPPORT

/****
 * Keep a history of each channel ?
 */
#define INETD_CHANHIST
/* These are probably ok - these values are taken more or less from
 * Mark Lewis' LPMud implementation */
#define REPLY_TIMEOUT	12
#define RETRIES		2
#define PAST_ID_TIMEOUT	120
#define MAX_PACKET_LEN	1024
#define FIT_PACKET_LEN	950
/* These two are for interhost mail. */
/* 30 minutes and 48 hours.. */
#define MAIL_RETRY	(30 * 60)
#define MAIL_RETRIES	96

/****
 * Enable the CDUDP system ?
 */
#define CDUDP_SUPPORT
/****
 * Keep a history of each channel ?
 */
#define CDUDP_CHANHIST

/****
 * Enable the Intermud-III system ?
 */
#define IMUD3_SUPPORT

/****
 * Hash the imud3 host list ? (* This is no use as far as user commands go,
 * but speeds up processing of incoming packets somewhat *)
 */
#define HASH_I3_HOSTS

/****
 * The IMUD3 status string. You should probably stick to one of the following.
 * (from the RFC)
 */
#define IMUD3_STATUS "driver development"
/*#define IMUD3_STATUS "mudlib development"*/
/*#define IMUD3_STATUS "beta testing"*/
/*#define IMUD3_STATUS "open to public"*/
/*#define IMUD3_STATUS "restricted access"*/

/****
 * Interval at which hosts file is stored to disk.. This is a big and complex
 * file and should probably only be saved once every 15 minutes or so.
 * I chose 16 minutes by default to avoid this happening at the same time
 * as the loadav event..
 */
#define I3_AUTOSAVE	960

/****
 * Keep a history of messages on each i3 channel ?
 */
#define I3_CHANHIST

/* You probably don't want these.. unless you're the nosy type ;-) */
#undef DEBUG_UDP
#undef DEBUG_IMUD3
#undef DEBUG_RNCLIENT

/****
 * Be wary of changing these..
 */
#define MAX_ARGV	5
#define ROOT_USER	"root"
#define IPTABLE_SIZE	((MAX_USERS * 3) / 2)
#define MAX_INET_ADDR	50	/* *bop winzlo* */

