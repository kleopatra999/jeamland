/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	backend.c
 * Function:	The main function and everything that doesn't belong
 *		elsewhere
 **********************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>
#include <netinet/in.h>
#include <grp.h>
#include <pwd.h>
#include <errno.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "jeamland.h"

#ifdef SETFDLIMIT
#include <sys/resource.h>
#endif

int 		port;
int 		sysflags;
char		*bin_path,
		*runas_username;
time_t		start_time, current_time;
int		loads = 0,
		logons = 0,
		peak_users = 0,
		rooms_created = 0;

int		shutdown_id = -1;
float		event_av = 0.0, command_av = 0.0;
extern float 	out_bps, in_bps;

float		utime_av[3] = { 0.0, 0.0, 0.0 },
		stime_av[3] = { 0.0, 0.0, 0.0 };

struct utsname system_uname;
unsigned long version_code = 0;

char ident[] = "@(#) Jeamland (c) Andy Fiddaman 1993-97.";

extern struct user *users;
extern struct room *rooms;
extern struct jlm *jlms;

#ifndef FREEBSD
extern int sys_nerr;
extern char *sys_errlist[];
extern int errno;
#endif

extern void stop_erqd(void);
void save_all_users(void);
void sig_shutdown(struct event *);

#ifdef STATISTICS_INTERVAL
void
update_statistics(struct event *ev)
{
        static long outime = 0, ostime = 0, orss = 0;
        long utime, stime, rss;

        set_times(&utime, &stime, &rss);
	/* Update the global CPU usage averages.. */
	/* Not worth a memcpy.. */
	utime_av[2] = utime_av[1];
	utime_av[1] = utime_av[0];
	utime_av[0] = (utime - outime) * 3.6 / STATISTICS_INTERVAL;
	stime_av[2] = stime_av[1];
	stime_av[1] = stime_av[0];
	stime_av[0] = (stime - ostime) * 3.6 / STATISTICS_INTERVAL;

#ifdef LOG_STATISTICS
        log_file(LOG_STATISTICS, "\n"
            "Users: %2d, %.2f cmds/s,      %.2f events/s\n"
            "           %.2f bytes out/s, %.2f bytes in/s\n"
            "Utime: %6ld (%6ld), Stime %6ld (%6ld), Rss %6ld (%6ld).\n",
            count_users((struct user *)NULL), command_av, event_av,
            out_bps, in_bps, utime, utime - outime, stime, stime - ostime,
            rss, rss - orss);
#endif /* LOG_STATISTICS */

        outime = utime;
        ostime = stime;
        orss = rss;
#ifdef MAX_RSS
	if (rss > MAX_RSS && shutdown_id != -1)
	{
		struct event *ev;
		int warn;

		notify_level(L_VISITOR, "The memory is almost used up!\n"
		    "Automatic shutdown initiated.");
		log_file("shutdown", "Automatic server shutdown started [%d].",
            	    MAX_RSS_DELAY);
        	write_all("Server will shut down in %s.\n",
		    conv_time(MAX_RSS_DELAY));

		warn = MAX_RSS_DELAY / 10;
		if (warn < 1)
			warn = 1;
		ev = create_event();
		push_number(&ev->stack, MAX_RSS_DELAY);
		shutdown_id = add_event(ev, sig_shutdown, warn,
		    "auto shutdown");
	}
#endif
        add_event(create_event(), update_statistics, STATISTICS_INTERVAL,
            "stats");
}
#endif /* STATISTICS_INTERVAL */

void
update_averages(struct event *ev)
{
	static time_t last_time = 0;
	time_t tmd = current_time - last_time;
	extern int bytes_out, bytes_in, bytes_outt, bytes_int;
	extern int ubytes_out, ubytes_in, ubytes_outt, ubytes_int;
	extern int event_count, command_count;

	/* 'Load' Averages.. */
	event_av = (float)event_count / (float)tmd;
	command_av = (float)command_count / (float)tmd;
	event_count = command_count = 0;

	/* Netstatistics */
	bytes_in += bytes_int;
	bytes_out += bytes_outt;
	ubytes_in += ubytes_int;
	ubytes_out += ubytes_outt;
	out_bps = (float)(bytes_outt + ubytes_outt) / (float)tmd;
	in_bps = (float)(bytes_int + ubytes_int) / (float)tmd;
	bytes_outt = bytes_int = ubytes_outt = ubytes_int = 0;

	last_time = current_time;
	add_event(create_event(), update_averages, LOADAV_INTERVAL, "loadav");
}

const char *
perror_text()
{
	if (errno < sys_nerr)
		return sys_errlist[errno];
	return "Unknown error.";
}

void
log_perror(char *fmt, ...)
{
	char buf[BUFFER_SIZE];
	va_list argp;

	va_start(argp, fmt);
	vsprintf(buf, fmt, argp);
	va_end(argp);

	log_file("perror", "%s: %s", buf, perror_text());
}

void
fatal(char *fmt, ...)
{
	char buf[BUFFER_SIZE];
	int i;
	va_list argp;

	FUN_START("fatal");

	va_start(argp, fmt);
	vsprintf(buf, fmt, argp);
	va_end(argp);

	FUN_ARG(buf);

	log_file("syslog", "Fatal error!");
	log_file("fatal", "%s", buf);
	fprintf(stderr, "%s\n", buf);
	notify_level(L_CONSUL, "[ FATAL: %s ]", buf);

#ifdef CRASH_TRACE
	backtrace("fatal", 0);
#endif

	/* First, closedown the jlms.. 
	 * No hurry, we're deciding to crash. */
	stop_erqd();
	closedown_jlms();
	for (i = 0; i < 30 && jlms != (struct jlm *)NULL; i++)
	{
		cleanup_jlms();
		my_sleep(1);
	}

	abort();	/* Create core dump */
}

/*
 * Overseer level gets unlimited idle time.
 * Other levels get (IDLE_LIMIT * level) seconds.
 */
static void
handle_idlers(struct event *ev)
{
	struct user *p;
	int next;

	next = IDLE_LIMIT;
	for (p = users->next; p != (struct user *)NULL; p = p->next)
	{
		int level, idle, n;

		if (!IN_GAME(p) || p->level >= L_OVERSEER ||
		    (p->saveflags & U_NO_IDLEOUT))
			continue;

		level = p->level > 0 ? p->level : 1;
		idle = (int)(current_time - p->last_command);

		if (idle >= IDLE_LIMIT * level)
		{
			save_user(p, 1, 0);
			disconnect_user(p, 1);
			write_socket(p, "You have been idle too long.\n");
			notify_levelabu(p, p->level,
			    "[ %s is disconnected for being idle too long. ]",
			    p->capname);
			if (!(p->saveflags & U_INVIS))
				write_roomabu(p,
			    	    "%s is disconnected for being idle "
				    "too long.\n", p->name);
		}
		else if ((n = IDLE_LIMIT * level - idle) < next)
			next = n;
	}
	/* Set up the next event for the next time a user will be at their
	 * idle limit. */
	add_event(create_event(), handle_idlers, next, "idle");
}

#ifdef IDLE_AUTO_AFK
static void
handle_autoafk(struct event *ev)
{
	struct user *p;
	int next;

	next = IDLE_AUTO_AFK;
	for (p = users->next; p != (struct user *)NULL; p = p->next)
	{
		int idle, n;

		if (!IN_GAME(p) || p->input_to != NULL_INPUT_TO)
			continue;

		idle = (int)(current_time - p->last_command);

		if (idle > IDLE_AUTO_AFK)
		{
			extern void f_afk(struct user *, int, char **);
			char *argv[2];

			argv[1] = "Automatic idle protect";
			f_afk(p, 2, argv);
		}
		else if ((n = IDLE_AUTO_AFK - idle) < next)
			next = n;
	}
	/* Set up the next event for the next time a user will be at their
	 * idle limit. */
	add_event(create_event(), handle_autoafk, next, "autoafk");
}
#endif

static void
autosave(struct event *e)
{
	extern void save_all_users(void);

	save_all_users();
	add_event(create_event(), autosave, AUTOSAVE_FREQUENCY, "autosave");
}

static void
clean_up(struct event *e)
{
	struct room *r, *s;

#if defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT)
	if (!(sysflags & SYS_EMPTY))
	{
#ifdef CDUDP_SUPPORT
		extern struct host *cdudp_hosts;
#endif
#ifdef INETD_SUPPORT
		extern struct host *hosts;
		save_hosts(hosts, F_INETD_HOSTS);
#endif
#ifdef CDUDP_SUPPORT
		save_hosts(cdudp_hosts, F_CDUDP_HOSTS);
#endif
	}
#endif

	for (r = rooms->next; r != (struct room *)NULL; r = s)
	{
		s = r->next;

		if (!r->inhibit_cleanup &&
		    current_time - r->ob->create_time > 300 &&
		    r->ob->contains == (struct object *)NULL)
			destroy_room(r);
	}

	add_event(create_event(), clean_up, CLEANUP_FREQUENCY, "cleanup");
}

static void setup_hour_notify(void);

static void
notify_hour(struct event *e)
{

	notify_level_wrt_flags(L_VISITOR, EAR_CHIME,
	    "\nAnother hour passes....\n");
	setup_hour_notify();
}

static void
setup_hour_notify()
{
	int i = 3600 - (int)current_time % 3600;

	add_event(create_event(), notify_hour, i, "chime");
}

time_t
calc_time_until(int h, int m)
{
	struct tm *t;

	if ((t = localtime(&current_time)) == (struct tm *)NULL)
	{
		log_file("error",
		    "Calc_time_until: Could not get time of day.");
		return -1;
	}

	return
	    (((h > t->tm_hour || (h == t->tm_hour && m > t->tm_min)) ? h :
	    h + 24) - t->tm_hour) * 3600 + (m - t->tm_min) * 60 - t->tm_sec;
}

#ifdef JL_CRON

static void handle_cron(struct event *);

static void
init_cron_event(char *p, int h, int m, int dom, int dow)
{
	struct event *ev;

	ev = create_event();
	push_number(&ev->stack, dom);
	push_number(&ev->stack, dow);
	push_number(&ev->stack, h);
	push_number(&ev->stack, m);
	push_malloced_string(&ev->stack, p);
	add_event(ev, handle_cron, (int)calc_time_until(h, m), "cron");
}

static void
handle_cron(struct event *ev)
{
	struct tm *t;

	FUN_START("handle_cron");

	if ((t = localtime(&current_time)) == (struct tm *)NULL)
	{
		log_file("error", "CRON: Could not get time of day.");
		/* Kill this event.. */
		FUN_END;
		return;
	}

	/* Check that we are on a correct dom/dow for this event. */
	if (((ev->stack.sp - 4)->u.number == -1 ||
	    (ev->stack.sp - 4)->u.number == t->tm_mday) &&
	    ((ev->stack.sp - 3)->u.number == -1 ||
	    (ev->stack.sp - 3)->u.number == t->tm_wday))
	{
		/* Run this command in background, then it can take as long
		 * as it likes.. */
		switch (fork())
		{
		    case 0:
		    {
			char cmd[BUFFER_SIZE];

			sprintf(cmd, "%s/cron/%s", BIN_PATH,
			    ev->stack.sp->u.string);
			system(cmd);
			/* Beware of flushing buffers.. */
			_exit(0);
		    }
		    case -1:
			notify_level(L_OVERSEER,
			    "[ Cron execution failed: could not fork. ]");
			break;
		    default:	/* Parent */
			notify_level(L_OVERSEER, "[ Cron executing: %s ]",
			    ev->stack.sp->u.string);
			log_file("cron", "EXEC: %s", ev->stack.sp->u.string);
			break;
		}
	}

	FUN_LINE;

	/* Restart event */
	init_cron_event(ev->stack.sp->u.string,
	    (ev->stack.sp - 2)->u.number,
	    (ev->stack.sp - 1)->u.number,
	    (ev->stack.sp - 4)->u.number,
	    (ev->stack.sp - 3)->u.number);
	/* Reuse the string on the event stack to avoid having to make
	 * another copy for the repeat event. */
	dec_stack(&ev->stack);
	FUN_END;
}

void
init_cron()
{
	char buf[BUFFER_SIZE];
	int h, m, dom, dow, line;
	FILE *fp;
	char *p;

	if ((fp = fopen(F_CRON, "r")) == (FILE *)NULL)
	{
		log_file("syslog", "Crontab not found.");
		return;
	}

	line = 0;

	while (fgets(buf, sizeof(buf), fp) != (char *)NULL)
	{
		line++;

		if (ISCOMMENT(buf))
			continue;

		if ((p = strchr(buf, '\n')) != (char *)NULL)
			*p = '\0';

		if ((p = strchr(buf, ',')) == (char *)NULL)
		{
			log_file("error", "Crontab line %d: no ,", line);
			continue;
		}

		*p++ = '\0';

		if (sscanf(buf, "%d:%d:%d:%d", &h, &m, &dom, &dow) != 4)
		{
			log_file("error", "Crontab line %d: bad time", line);
			continue;
		}

		if (dom != -1 && (dom < 1 || dom > 31))
		{
			log_file("error",
			    "Crontab line %d: bad month day, %d.", dom);
			continue;
		}

		if (dow != -1 && (dow < 0 || dow > 6))
		{
			log_file("error",
			    "Crontab line %d: bad week day, %d.", dow);
			continue;
		}

		/* Security check. */
		if (strpbrk(p, "./") != (char *)NULL)
		{
			log_file("error", "Crontab line %d: bad command",
			    line);
			continue;
		}

		log_file("cron", "INIT: %02d:%02d Mday: %2d, Wday %d :\n"
		    "      %s",
		    h, m, dom, dow, p);

		init_cron_event(string_copy(p, "init_cron"), h, m, dom, dow);
	}
	fclose(fp);
}
#endif /* JL_CRON */

void
sig_shutdown(struct event *ev)
{
	struct event *e = create_event();
	int num = ev->stack.sp->u.number;

	num -= ev->delay;
	if (num >= ev->delay)
	{
		write_all("Server shutting down in %s.\n", conv_time(num));
		push_number(&e->stack, num);
		shutdown_id = add_event(e, sig_shutdown, ev->delay, "shutdown");
		return;
	}
	else if (num > 0)
	{
		push_number(&e->stack, 0);
		shutdown_id = add_event(e, sig_shutdown, num, "shutdown");
		return;
	}
	sysflags |= SYS_SHUTDWN;
}

void
tidy_shutdown(int sig)
{
	char *s;

	signal(sig, SIG_DFL);

	switch (sig)
	{
	    case SIGINT:
		s = "SIGINT";
		break;

	    case SIGTERM:
		s = "SIGTERM";
		/* Want to shutdown quickly so don't bother saving
		 * host files. */
		sysflags |= SYS_EMPTY;
		break;

	    default:
		fatal("Unknown signal in tidy_shutdown");
	}

	/* Shut down.. */
	sysflags |= SYS_SHUTDWN;

	log_file("shutdown", "%s received.", s);
	log_file("syslog", "%s received.", s);

	write_all("\nShutdown request received!\n");
}

char *
type_name(struct svalue *s)
{
	switch (s->type)
	{
	    case T_NUMBER:
		return "long";
	    case T_UNUMBER:
		return "ulong";
	    case T_STRING:
		return "string";
	    case T_EMPTY:
		return "empty";
	    case T_VECTOR:
		return "array";
	    case T_POINTER:
		return "pointer";
	    case T_MAPPING:
		return "mapping";
	    case T_FPOINTER:
		return "fpointer";
	    default:
		return "unknown";
	}
}

void
print_svalue(struct user *p, struct svalue *s)
{
	char *tmp = svalue_to_string(s);
	write_socket(p, "(%s)%s", type_name(s), tmp);
	xfree(tmp);
}

int
equal_svalue(struct svalue *sv, struct svalue *sv2)
{
	if (sv == sv2)
		return 1;
	if (sv->type != sv2->type)
		return 0;
	switch (sv->type)
	{
	    case T_STRING:
		return !strcmp(sv->u.string, sv2->u.string);
	    case T_NUMBER:
		return sv->u.number == sv2->u.number;
	    case T_UNUMBER:
		return sv->u.unumber == sv2->u.unumber;
	    case T_POINTER:
		return sv->u.pointer == sv2->u.pointer;
	    case T_FPOINTER:
		return sv->u.fpointer == sv2->u.fpointer;
	    case T_VECTOR:
	    {
		int i;

		if (sv->u.vec == sv2->u.vec)
			return 1;
		if (sv->u.vec->size != sv2->u.vec->size)
			return 0;
		for (i = sv->u.vec->size; i--; )
			if (!equal_svalue(&sv->u.vec->items[i],
			    &sv2->u.vec->items[i]))
				return 0;
		return 1;
	    }
	    case T_MAPPING:
		log_file("error", "equal_svalue(mappings) - please report");
		return 0;
	    default:
		fatal("Bad type in equal_svalue: %d.", sv->type);
	}
	return 0;
}

void
free_svalue(struct svalue *sv)
{
	switch (sv->type)
	{
	    case T_STRING:
		FREE(sv->u.string);
	    case T_EMPTY:
	    case T_NUMBER:
	    case T_UNUMBER:
	    case T_POINTER:
	    case T_FPOINTER:
		break;
	    case T_VECTOR:
		/* Possible need to check for recursion.. */
		free_vector(sv->u.vec);
		break;
	    case T_MAPPING:
		free_mapping(sv->u.map);
		break;
	    default:
		fatal("Bad type in free_svalue(); received %d", sv->type);
	}
	sv->type = T_EMPTY;
}

void
update_current_time()
{
	current_time = time((time_t *)NULL);
#ifdef TIMEZONE_ADJUSTMENT
	current_time += 3600 * TIMEZONE_ADJUSTMENT;
#endif
}

void
background_process()
{
	log_file("syslog", "Backgrounding.");
	switch ((int)fork())
	{
	    case -1:
		log_perror("fork");
		fatal("can't fork");
	    case 0:
		break;
	    default:
		exit(0);
	}
}

void
float_exception(int sig)
{
	signal(sig, float_exception);
	log_file("error", "Floating point exception occured!");
}

static void
set_version_code()
{
	char *c, *d, *str;
	int x, y, z;

	version_code = 0x0;

	str = c = string_copy(VERSION, "version copy");
	if ((d = strchr(c, '.')) == (char *)NULL)
		return;
	*d++ = '\0';
	x = atoi(c);
	c = d;
	if ((d = strchr(c, '.')) == (char *)NULL)
		return;
	*d++ = '\0';
	y = atoi(c);
	z = atoi(d);
	version_code = JL_VERSION_CODE(x, y, z);
	xfree(str);
}

#ifdef SETFDLIMIT
static void
setfdlim()
{
	struct rlimit rm;
	int l = SETFDLIMIT, o;

	if (getrlimit(RLIMIT_NOFILE, &rm) == -1)
	{
		log_perror("getrlimit");
		return;
	}

	o = rm.rlim_cur;

	log_file("syslog", "FDLIM: Current limit is %d/%d.", rm.rlim_cur,
	    rm.rlim_max);

	if (l == rm.rlim_cur)
	{
		log_file("syslog", "FDLIM: Limit is already %d.", l);
		return;
	}

	if (l > rm.rlim_max)
	{
		log_file("syslog", "FDLIM: New limit > System limit.");
		log_file("syslog", "FDLIM: System limit used (%d).",
		    rm.rlim_max);
		l = rm.rlim_max;
	}

	rm.rlim_cur = l;

	if (setrlimit(RLIMIT_NOFILE, &rm) == -1)
	{
		log_perror("setrlimit");
		return;
	}
	log_file("syslog", "FDLIM: Changed limit: %d -> %d (Max: %d)",
	    o, l, rm.rlim_max);
}
#endif /* SETFDLIMIT */

int
main(int argc, char **argv)
{
	char *cp;
	FILE *fp;
	struct passwd *pwd;
	struct user *p, *p_next;
#ifdef RUN_UMASK
	int oumask;
#endif
	char lib_path[MAXPATHLEN + 1];
	char pidfile[30];
	int i;

	/* Hmm.. wonder if I have enough prototypes here... */

	/* Initialisation functions */
	extern void malloc_init(void);
	extern void initialise_events(void);
	extern void initialise_ip_table(void);
	extern void init_doa_stack(void);
	extern void init_services(void);
	extern void init_erq_table(void);
	extern void init_user_list(void);
	extern void init_room_list(void);
	extern void init_start_rooms(void);
#ifdef REMOTE_NOTIFY
	extern void init_rnotify(void);
#endif

	/* Load functions */
	extern void load_feelings(void);
	extern void load_mastersave(void);
	extern void load_banned_hosts(void);
	extern void load_hosts(int);
	extern void preload_jlms(void);
	extern void restore_galiases(void);

	/* Check functions */
	extern void check_cmd_table(void);
	extern void check_help_files(void);

	/* Boot time events. */
	extern void start_erqd(void);
	extern void prepare_ipc(void);
	extern void remove_ipc(void);
	extern void start_purge_event(void);

	/* Run time events. */
	extern void process_sockets(void);
	extern void new_user(int, int);
	extern void handle_event(void);

	/* Now misc config dependant functions */
#ifdef DEBUG_MALLOC
	extern void dump_malloc_table_sig(int);
#endif
#ifdef HASH_COMMANDS
	extern void hash_commands(void);
#endif
#ifdef HASH_ALIAS_FUNCS
	extern void hash_alias_funcs(void);
#endif
#ifdef HASH_JLM_FUNCS
	extern void hash_jlm_funcs(void);
#endif
#ifdef INETD_SUPPORT
	extern void initialise_inetd(void);
	extern void ping_all(void);
	extern struct host *hosts;
#endif /* INETD_SUPPORT */
#ifdef CDUDP_SUPPORT
	extern struct host *cdudp_hosts;
	extern void ping_all_cdudp_event(struct event *);
#endif /* CDUDP_SUPPORT */
#ifdef AUTO_VALEMAIL
	extern void restore_valemail(void);
#endif
	char **Argv = argv;

#ifdef CRASH_TRACE
	init_crash();
#endif
	/* This must come after the init_crash() ! */
	FUN_START("main");

#ifdef SIGTTOU
	/* Important for the console mode. */
	signal(SIGTTOU, SIG_IGN);
#endif
#ifdef SIGFPE
	signal(SIGFPE, float_exception);
#endif

	malloc_init();

	update_current_time();

	port = DEFAULT_PORT;
	sysflags = 0;
	strcpy(lib_path, LIB_PATH);
	bin_path = string_copy(BIN_PATH, "*bin path");

	/* There should always be a / directory! 
	 * If this fails, assume a problem with chdir() itself.
	 */
	if (chdir("/"))
		fatal("Cannot chdir !");

	argc--, argv++;
	while (argc > 0 && *argv[0] == '-') 
	{
		for (cp = &argv[0][1]; *cp; cp++) 
			switch (*cp) 
	      		{
			    case '3':
				sysflags |= SYS_NOI3;
				break;

			    case 'b':
				if (*++cp == '\0')
					log_file("syslog",
					    "No argument given to -b");
				else
				{
					while (isspace(*cp))
						cp++;
					COPY(bin_path, cp, "*bin path");
					log_file("syslog",
					    "Using alternate bin "
					    "directory, [%s]", bin_path);
				}
				goto nextopt;

			    case 'B':
				sysflags |= SYS_BACKGROUND;
				sysflags &= ~SYS_CONSOLE;
				/* Fall through */

			    case 'L':
				sysflags |= SYS_LOG;
				break;

			    case 'c':
				sysflags |= SYS_SHOWLOAD;
				break;

			    case 'd':
				sysflags |= SYS_NOERQD;
				break;

			    case 'e':
				sysflags |= SYS_EMPTY;
				break;

	   		    case 'H':
				if (sysflags & SYS_CHECK_HELPS)
				{	/* Double call, set other things */
					sysflags |= SYS_EMPTY;
					sysflags |= SYS_NOPING;
					sysflags |= SYS_NOERQD;
					sysflags |= SYS_NOIPC;
					sysflags |= SYS_NOUDP;
					sysflags |= SYS_NOSERVICE;
					sysflags |= SYS_NONOTIFY;
					sysflags |= SYS_SHUTDWN;
				}
				else
					sysflags |= SYS_CHECK_HELPS;
				break;

			    case 'i':
				sysflags |= SYS_NOIPC;
				break;

			    case 'k':
				sysflags |= SYS_KILLOLD;
				break;

			    case 'l':
				if (*++cp == '\0')
					log_file("syslog",
					    "No argument given to -l");
				else
				{
					while (isspace(*cp))
						cp++;
					strcpy(lib_path, cp);
					log_file("syslog",
			    	    	    "Using alternate lib "
					    "directory, [%s]", lib_path);
				}
				goto nextopt;

			    case 'n':
				sysflags |= SYS_NONOTIFY;
				break;

			    case 'P':
				sysflags |= SYS_NOPROCTITLE;
				break;

			    case 'p':
				port = atoi(++cp);
				if (port < 1024 || port > 65535)
				{
					log_file("syslog",
					    "Bad port number %d\n", port);
					log_file("syslog", "Using %d\n",
					    DEFAULT_PORT);
					port = DEFAULT_PORT;
				}
				goto nextopt;

			    case 'S':
				sysflags |= SYS_NOERQD;
				sysflags |= SYS_NOPING;
				sysflags |= SYS_NOIPC;
				sysflags |= SYS_NOUDP;
				sysflags |= SYS_NOSERVICE;
				sysflags |= SYS_NONOTIFY;
				sysflags |= SYS_LOG;
				sysflags |= SYS_NOI3;
				/* Fall through */

			    case 'C':
				sysflags |= SYS_CONSOLE;
				sysflags &= ~SYS_BACKGROUND;
				break;

			    case 's':
				sysflags |= SYS_NOPING;
				break;

			    case 't':
				sysflags |= SYS_NOSERVICE;
				break;

			    case 'u':
				sysflags |= SYS_NOUDP;
				break;

			    case 'x':
				sysflags |= SYS_SHUTDWN;
				break;

			    case 'h':
				printf("Jeamland boot options:\n"
#ifdef IMUD3_SUPPORT
				    "\t3\tBoot without starting Imud3.\n"
#endif
				    "\tb\tBoot using alternate bin dir.\n"
				    "\tB\tBackground self (implies -L too).\n"
				    "\tc\tShow loads in syslog.\n"
				    "\tC\tBoot allowing console login.\n"
				    "\td\tBoot without starting erqd.\n"
				    "\te\tBoot empty (for debugging)\n"
				    "\th\tThis help page.\n"
				    "\tH\tCheck that all help files exist.\n"
				    "\ti\tBoot without setting up ipc.\n"
				    "\tk\tKill and replace running copy.\n"
				    "\tl\tBoot using alternate lib dir.\n"
				    "\tL\tWrite to log files instead of "
				         "screen.\n"
#ifdef REMOTE_NOTIFY
				    "\tn\tDisable remote notifications.\n"
#endif
				    "\tp\tBoot to a specific port.\n"
				    "\tP\tDont change proctitle.\n"
#if defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT)
				    "\ts\tBoot without sending startup "
				         "pings.\n"
#endif
				    "\tS\tBoot to single user mode.\n"
#ifdef SERVICE_PORT
				    "\tt\tBoot without starting the TCP "
					"service port.\n"
#endif
#if defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT) || defined(REMOTE_NOTIFY)
				    "\tu\tBoot without starting UDP "
					"services.\n"
#endif
				    "\tx\tShutdown after boot.\n");
				exit(0);

			    default:
				fprintf(stderr,
				    "%s: Unknown flag -%c ignored.\n", *Argv,
				    *cp);
				break;
			}
nextopt:
		argc--, argv++;
	}

	if (chdir(lib_path))
	{
		log_perror("chdir");
		fatal("Bad lib directory [%s].", lib_path);
	}

#ifdef SETPROCTITLE
	/* Kludge, but does the job */
	if (!(sysflags & SYS_NOPROCTITLE) && strncmp(Argv[0], "-=>", 3))
        {
                char *tmp = (char *)malloc(70);
                sprintf(tmp, "-=> %s: port %d <=-", LOCAL_NAME, port);
                Argv[0] = tmp;
                execv("../bin/jeamland", Argv);
		log_perror("execv");
		log_file("syslog", "Could not set proctitle.");
		free(tmp);
		/* Carry on.. it's not serious ;-) */
        }
#endif

	set_version_code();
	log_file("syslog", "Booting JeamLand %s (%#x)", VERSION, version_code);

	sprintf(pidfile, F_PID".%d", port);

	if (sysflags & SYS_KILLOLD)
	{
		int pid, args;
		if ((fp = fopen(pidfile, "r")) != (FILE *)NULL)
		{
			if ((args = fscanf(fp, "%d", &pid)) && args != EOF)
			{
				log_file("syslog",
				    "Shutting down running copy.");
				if (kill(pid, SIGTERM) == -1)
					log_perror("Kill");
				else
					my_sleep(5);
			}
			fclose(fp);
		}
	}

#ifdef LOCAL_NAME
	if (!strcasecmp(LOCAL_NAME, "unconfigured"))
#endif
		fatal("You must set a local name!");

	if ((pwd = getpwuid(getuid())) != (struct passwd *)NULL)
		runas_username = string_copy(pwd->pw_name, "*runas_username");
	else
		runas_username = (char *)NULL;

#ifdef RUN_UMASK
	oumask = umask(RUN_UMASK & 0777);
	log_file("syslog", "Umask set to %o (was %o).", RUN_UMASK & 0777,
	    oumask);
#endif

#ifdef SETFDLIMIT
	setfdlim();
#endif

	log_file("syslog", "Loading mastersave.");
	load_mastersave();
	log_file("syslog", "Checking command table.");
	check_cmd_table();
#ifdef HASH_COMMANDS
	log_file("syslog", "Hashing command table.");
	hash_commands();
#endif
#ifdef HASH_ALIAS_FUNCS
	log_file("syslog", "Hashing alias functions.");
	hash_alias_funcs();
#endif
#ifdef HASH_JLM_FUNCS
	log_file("syslog", "Hashing jlm functions.");
	hash_jlm_funcs();
#endif
	if (sysflags & SYS_CHECK_HELPS)
	{
		log_file("syslog", "Checking help files.");
		check_help_files();
	}

	if (sysflags & SYS_BACKGROUND)
		background_process();

	log_file("syslog", "Initialising lists.");
	init_user_list();
	init_room_list();

	log_file("syslog", "Retrieving system information.");
	uname(&system_uname);

	log_file("syslog", "Initialising doa stack.");
	init_doa_stack();

	log_file("syslog", "Initialising events.");
	initialise_events();

	log_file("syslog", "Initialising start rooms.");
	init_start_rooms();

#ifdef INETD_SUPPORT
	initialise_inetd();
#endif

#ifdef IMUD3_SUPPORT
	i3_init();
#endif

	if (!(sysflags & SYS_EMPTY))
	{
#ifdef INETD_SUPPORT
		log_file("syslog", "Loading inetd hosts.");
		load_hosts(0);
#endif
#ifdef CDUDP_SUPPORT
		log_file("syslog", "Loading cdudp hosts.");
		load_hosts(1);
#endif
#ifdef IMUD3_SUPPORT
		if (!(sysflags & SYS_NOI3))
		{
			log_file("syslog", "Loading imud3 database.");
			i3_load();
		}
#endif
		log_file("syslog", "Loading global aliases.");
		restore_galiases();
		log_file("syslog", "Loading banned hosts.");
		load_banned_hosts();
		log_file("syslog", "Loading feelings.");
		load_feelings();
		log_file("syslog", "Loading mbs.");
		load_mbs();
#ifdef AUTO_VALEMAIL
		log_file("syslog", "Loading valemail database.");
		restore_valemail();
#endif
		log_file("syslog", "Initialising JLM's");
		preload_jlms();
	}

	if (!(sysflags & SYS_NOERQD))
	{
		log_file("syslog", "Starting erqd.");
		start_erqd();
	}

	start_time = current_time;

	log_file("syslog", "Initialising ip table.");
	initialise_ip_table();

#ifdef SERVICE_PORT
	log_file("syslog", "Initialising services.");
	init_services();
#endif

#ifdef REMOTE_NOTIFY
	log_file("syslog", "Initialising remote notification table.");
	init_rnotify();
#endif

	log_file("syslog", "Initialising erq table.");
	init_erq_table();

	log_file("syslog", "System: %s %s %s (%s).",
	    system_uname.sysname, system_uname.release,
	    system_uname.version, system_uname.machine);

	/* Seed the random number generator */
	srand((unsigned)current_time);

	prepare_ipc();

	/* Store our pid in the pidfile. */
	if ((fp = fopen(pidfile, "w")) != (FILE *)NULL)
	{
		fprintf(fp, "%ld\n", (long)getpid());
		fclose(fp);
	}

#ifdef IMUD3_SUPPORT
	if (!(sysflags & (SYS_EMPTY | SYS_NOI3)))
		i3_event_connect((struct event *)NULL);
#endif

#ifdef JL_CRON
	log_file("syslog", "Initialising cron jobs.");
	update_current_time();
	init_cron();
#endif

	log_file("syslog", "Starting miscellaneous events.");
	handle_idlers((struct event *)NULL);
#ifdef IDLE_AUTO_AFK
	handle_autoafk((struct event *)NULL);
#endif
	add_event(create_event(), autosave, AUTOSAVE_FREQUENCY, "autosave");
	add_event(create_event(), clean_up, CLEANUP_FREQUENCY, "cleanup");
	update_averages((struct event *)NULL);
#ifdef STATISTICS_INTERVAL
        add_event(create_event(), update_statistics, STATISTICS_INTERVAL,
            "stats");
#endif

	update_current_time();
	start_purge_event();
	setup_hour_notify();

	if (!(sysflags & (SYS_NOPING | SYS_NOUDP)))
	{
#ifdef INETD_SUPPORT
		ping_all();
#endif
#ifdef CDUDP_SUPPORT
		/* Wait until the INETD pings have finished before sending
		 * these */
		add_event(create_event(), ping_all_cdudp_event,
		    (RETRIES + 1) * REPLY_TIMEOUT + 5, "cdudp ping");
#endif
	}

#ifdef DEBUG_MALLOC
	signal(SIGUSR1, dump_malloc_table_sig);
#endif /* DEBUG_MALLOC */

	signal(SIGTERM, tidy_shutdown);
	signal(SIGINT, tidy_shutdown);

#ifdef SIGUSR2
	signal(SIGUSR2, SIG_IGN);
#endif

	if (sysflags & SYS_CONSOLE)
		new_user(fileno(stdin), 1);

	/* Here is the /massive/ (*ahem*) main loop... */
	while (!(sysflags & SYS_SHUTDWN))
	{
#if defined(DEBUG) && defined(CRASH_TRACE)
		check_fundepth(1);
#endif
		if (sysflags & SYS_EVENT)
			handle_event();
		process_sockets();
	}

	update_current_time();

	log_file("shudown", "Shutdown after %s",
	    conv_time(current_time - start_time));

	write_all("\n"
	    "---------------------------\n"
	    "SERVER SHUTDOWN IN PROGRESS\n"
	    "---------------------------\n"
	    );

	/* Kick off all but admin with the nokick flag set... */
	notify_level(L_WARDEN, "[ SHUTDOWN: Closing down user sockets. ]");
	for (p = users->next; p != (struct user *)NULL; p = p_next)
	{
		p_next = p->next;

		notify_level(L_WARDEN, "[ SHUTDOWN: Saving %s. ]",
		    p->capname);
		save_user(p, 1, 0);

		/* Turn console echo back on ! */
		if ((p->flags & U_CONSOLE) && (p->flags & U_NO_ECHO))
			echo(p);

		if (p->level >= L_WARDEN && (p->saveflags & U_SHUTDOWN_NOKICK))
		{
			if (IN_GAME(p))
				lastlog(p);
			continue;
		}

		if (IN_GAME(p))
			notify_level(L_WARDEN,
			    "[ SHUTDOWN: Removed %s. ]", p->capname);

		/* We are not going back into the main loop, call close_socket
		 * directly but must set the SOCK_CLOSING flag first
		 */
		p->flags |= U_SOCK_CLOSING;
		close_socket(p);
	}

	notify_level(L_WARDEN, "[ SHUTDOWN: Shutting down rnotify clients. ]");
	closedown_rnotify();

	notify_level(L_WARDEN, "[ SHUTDOWN: Shutting down erqd. ]");
	stop_erqd();
	if (!(sysflags & SYS_EMPTY))
	{
		/* We don't want to overwrite our hosts files with the blank
		 * tables.
		 * This comes from experience... oh yes! */
#ifdef INETD_SUPPORT
		notify_level(L_WARDEN,
		    "[ SHUTDOWN: Saving intermud hosts. ]");
		save_hosts(hosts, F_INETD_HOSTS);
#endif
#ifdef CDUDP_SUPPORT
		save_hosts(cdudp_hosts, F_CDUDP_HOSTS);
#endif
#ifdef IMUD3_SUPPORT
		i3_save();
#endif
	}

	FUN_END;
	
	notify_level(L_WARDEN, "[ SHUTDOWN: Removing ipc. ]");
	log_file("syslog", "Removing ipc.");
	remove_ipc();

#ifdef IMUD3_SUPPORT
	if (i3_shutdown() == 1)
	{
		log_file("syslog", "I3: Router disconnect");
		notify_level(L_WARDEN,
		    "[ SHUTDOWN: Disconnected from i3 router. ]");
	}
	else
		log_file("syslog", "I3: Not connected to router");
#endif

	cleanup_jlms();
	notify_level(L_WARDEN, "[ SHUTDOWN: Closing down jlms. ]");
	log_file("syslog", "Removing jlms.");
	closedown_jlms();
	/* Give the jlms time to clean up.. 
	 * This used to be implemented using sleep() but some systems
	 * object to mixing alarm() and sleep() calls
	 */
	notify_level(L_WARDEN, "[ SHUTDOWN: Waiting for jlms to exit. ]");
	/* Hang around for ten seconds at most */
	for (i = 0; i < 10 && jlms != (struct jlm *)NULL; i++)
	{
		cleanup_jlms();
		my_sleep(1);
	}
	notify_level(L_WARDEN, "[ SHUTDOWN: Complete. ]");
	log_file("syslog", "Exiting.\n");
	return 0;
}

