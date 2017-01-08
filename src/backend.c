/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
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
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <netinet/in.h>
#include <grp.h>
#include <pwd.h>
#include <errno.h>
#include <sys/utsname.h>
#include "jeamland.h"

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

char ident[] = "@(#) Jeamland (c) Andy Fiddaman 1994-96.";

extern struct user *users;
extern struct room *rooms;
extern int sys_nerr;
extern char *sys_errlist[];
extern int errno;
extern char *currently_executing;

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
		    "Automatic shutdown initiated.\n");
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
	extern int event_count, command_count;

	/* 'Load' Averages.. */
	event_av = (float)event_count / (float)tmd;
	command_av = (float)command_count / (float)tmd;
	event_count = command_count = 0;

	/* Netstatistics */
	bytes_in += bytes_int;
	bytes_out += bytes_outt;
	out_bps = (float)bytes_outt / (float)tmd;
	in_bps = (float)bytes_int / (float)tmd;
	bytes_outt = bytes_int = 0;

	last_time = current_time;
	add_event(create_event(), update_averages, LOADAV_INTERVAL, "loadav");
}

char *
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

	log_file("syslog", "%s: %s", buf, perror_text());
}

void
fatal(char *fmt, ...)
{
	char buf[BUFFER_SIZE];
	va_list argp;

	va_start(argp, fmt);
	vsprintf(buf, fmt, argp);
	va_end(argp);

	log_file("syslog", "Fatal error!");
	log_file("fatal", "%s", buf);
	fprintf(stderr, "%s\n", buf);
	if (currently_executing != (char *)NULL)
	{
		notify_level(L_CONSUL, "[ FATAL: Whilst executing: %s ]\n",
		    currently_executing);
		log_file("fatal", "Whilst executing %s", currently_executing);
	}
	else
	{
		log_file("fatal", "No current execution!\n");
		notify_level(L_CONSUL, "[ FATAL: No current execution! ]\n");
	}
	notify_level(L_CONSUL, "[ FATAL: %s ]\n", buf);
#ifdef CRASH_TRACE
	backtrace(0);
#endif
	abort();	/* Create core dump */
}

/*
 * Overseer level gets unlimited idle time.
 * Other levels get (IDLE_LIMIT * level) seconds.
 */
void
handle_idlers(struct event *ev)
{
	struct user *p;
	int next;

	next = IDLE_LIMIT;
	for (p = users->next; p != (struct user *)NULL; p = p->next)
	{
		int level, idle, n;

		if (p->level >= L_OVERSEER || !IN_GAME(p) ||
		    (p->saveflags & U_NO_IDLEOUT))
			continue;

		level = p->level > 0 ? p->level : 1;
		idle = (int)(current_time - p->last_command);

		if (idle >= IDLE_LIMIT * level)
		{
			write_socket(p, "You have been idle too long.\n");
			notify_levelabu(p, p->level,
			    "[ %s is disconnected for being idle too long. ]\n",
			    p->capname);
			if (!(p->saveflags & U_INVIS))
				write_roomabu(p,
			    	    "%s is disconnected for being idle "
				    "too long.\n", p->name);
			save_user(p, 1, 0);
			p->flags |= U_SOCK_QUITTING;
		}
		else if ((n = IDLE_LIMIT * level - idle) < next)
			next = n;
	}
	/* Set up the next event for the next time a user will be at their
	 * idle limit. */
	add_event(create_event(), handle_idlers, next, "idle");
}

void
autosave(struct event *e)
{
	extern void save_all_users(void);

	save_all_users();
	add_event(create_event(), autosave, AUTOSAVE_FREQUENCY, "autosave");
}

void
clean_up(struct event *e)
{
	struct room *r, *s;

#ifdef UDP_SUPPORT
	if (!(sysflags & SYS_EMPTY))
	{
		extern void save_hosts(struct host *, char *);
		extern struct host *hosts;
#ifdef CDUDP_SUPPORT
		extern struct host *cdudp_hosts;

		save_hosts(cdudp_hosts, F_CDUDP_HOSTS);
#endif /* CDUDP_SUPPORT */
		save_hosts(hosts, F_INETD_HOSTS);
	}
#endif /* UDP_SUPPORT */

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

void
notify_hour(struct event *e)
{
	void setup_hour_notify(void);

	notify_level_wrt_flags(L_VISITOR, EAR_CHIME,
	    "\nAnother hour passes....\n\n");
	setup_hour_notify();
}

void
setup_hour_notify()
{
	int i = 3600 - (int)current_time % 3600;

	add_event(create_event(), notify_hour, i, "chime");
}

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
	/* SIGTERM received. Either machine is shutting down or else
	 * we are being killed by another copy.
	 * In either case, shut down NOW! */
	sysflags |= SYS_SHUTDWN;
	write_all("\nShutdown request received!\n");
	log_file("shutdown", "SIGTERM received.");
	log_file("syslog", "SIGTERM received.");
}

char *
type_name(struct svalue *s)
{
	switch(s->type)
	{
	    case T_NUMBER:
		return "int";
	    case T_STRING:
		return "string";
	    case T_EMPTY:
		return "empty";
	    case T_VECTOR:
		return "array";
	    case T_POINTER:
		return "pointer";
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
	switch(sv->type)
	{
	    case T_STRING:
		return !strcmp(sv->u.string, sv2->u.string);
	    case T_NUMBER:
		return sv->u.number == sv2->u.number;
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
	    default:
		fatal("Bad type in equal_svalue: %d.", sv->type);
	}
	return 0;
}

void
free_svalue(struct svalue *sv)
{
	switch(sv->type)
	{
	    case T_STRING:
		FREE(sv->u.string);
	    case T_EMPTY:
	    case T_NUMBER:
	    case T_POINTER:
	    case T_FPOINTER:
		break;
	    case T_VECTOR:
		free_vector(sv->u.vec);
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
	switch((int)fork())
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

int
main(int argc, char **argv)
{
	char *cp;
	FILE *fp;
	struct passwd *pwd;
	struct user *p, *p_next;
#ifdef RUN_GROUP
	struct group *grp;
#endif
#ifdef RUN_UMASK
	int oumask;
#endif
	char lib_path[MAXPATHLEN + 1];
	char pidfile[30];

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

	/* Load functions */
	extern void load_feelings(void);
	extern void load_mastersave(void);
	extern void load_banned_hosts(void);
	extern void load_hosts(int);
	extern void preload_jlms(void);

	/* Check functions */
	extern void check_cmd_table(void);
	extern void check_help_files(void);

	/* Boot time events. */
	extern void start_erqd(void);
	extern void stop_erqd(void);
	extern void prepare_ipc(void);
	extern void remove_ipc(void);
	extern void start_purge_event(void);

	/* Run time events. */
	extern void process_sockets(void);
	extern void new_user(int, int);
	extern void handle_event(void);

	/* Shutdown events. */
	extern void closedown_jlms(void);

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
#ifdef UDP_SUPPORT
	extern void initialise_inetd(void);
	extern void ping_all(void);
	extern void save_hosts(struct host *, char *);
	extern struct host *hosts;
#ifdef CDUDP_SUPPORT
	extern struct host *cdudp_hosts;
	extern void ping_all_cdudp_event(struct event *);
#endif /* CDUDP_SUPPORT */
#endif /* UDP_SUPPORT */
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
		fatal("Cannot chdir !\n");

	argc--, argv++;
	while (argc > 0 && *argv[0] == '-') 
	{
		for (cp = &argv[0][1]; *cp; cp++) 
			switch (*cp) 
	      		{
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
				sysflags |= SYS_LOG;
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
				    "\tp\tBoot to a specific port.\n"
				    "\tP\tDont change proctitle.\n"
				    "\ts\tBoot without sending startup "
				         "pings.\n"
				    "\tS\tBoot to single user mode.\n"
				    "\tt\tBoot without starting the TCP "
					"service port.\n"
				    "\tu\tBoot without starting UDP "
					"services.\n"
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
		fatal("Bad lib directory [%s].\n", lib_path);
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
		xfree(tmp);
		/* Carry on.. it's not serious ;-) */
        }
#endif

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
					my_sleep(2);
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

#ifdef RUN_GROUP
	if ((grp = getgrnam(RUN_GROUP)) == (struct group *)NULL)
		fatal("Group %s does not exist.", RUN_GROUP);
	if (setgid(grp->gr_gid) == -1)
		fatal("Can not set group id to %s.", RUN_GROUP);
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
#ifdef UDP_SUPPORT
	initialise_inetd();
	if (!(sysflags & SYS_EMPTY))
	{
		log_file("syslog", "Loading inetd hosts.");
		load_hosts(0);
#ifdef CDUDP_SUPPORT
		log_file("syslog", "Loading cdudp hosts.");
		load_hosts(1);
#endif
	}
#endif

	if (!(sysflags & SYS_EMPTY))
	{
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

	log_file("syslog", "Initialising services.");
	init_services();

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

	log_file("syslog", "Starting miscellaneous events.");
	handle_idlers((struct event *)NULL);
	add_event(create_event(), autosave, AUTOSAVE_FREQUENCY, "autosave");
	add_event(create_event(), clean_up, CLEANUP_FREQUENCY, "cleanup");
	update_averages((struct event *)NULL);
#ifdef STATISTICS_INTERVAL
        add_event(create_event(), update_statistics, STATISTICS_INTERVAL,
            "stats");
#endif
	/* Better update the current time before setting these events.. */
	update_current_time();
	start_purge_event();
	setup_hour_notify();

#ifdef UDP_SUPPORT
	if (!(sysflags & (SYS_NOPING | SYS_NOUDP)))
	{
		ping_all();
#ifdef CDUDP_SUPPORT
		add_event(create_event(), ping_all_cdudp_event,
		    (RETRIES + 1) * REPLY_TIMEOUT + 5, "cdudp ping");
#endif
	}
#endif

#ifdef DEBUG_MALLOC
	signal(SIGUSR1, dump_malloc_table_sig);
#endif /* DEBUG_MALLOC */

	signal(SIGTERM, tidy_shutdown);
#ifdef SIGUSR2
	signal(SIGUSR2, SIG_IGN);
#endif

	if (sysflags & SYS_CONSOLE)
		new_user(fileno(stdin), 1);

	/* Here is the /massive/ (*ahem*) main loop... */
	while(!(sysflags & SYS_SHUTDWN))
	{
		if (sysflags & SYS_EVENT)
			handle_event();
		process_sockets();
	}

	write_all("\n"
	    "---------------------------\n"
	    "SERVER SHUTDOWN IN PROGRESS\n"
	    "---------------------------\n"
	    );

	/* Kick off all but admin with the nokick flag set... */
	notify_level(L_WARDEN, "[ SHUTDOWN: Closing down user sockets. ]\n");
	for (p = users->next; p != (struct user *)NULL; p = p_next)
	{
		p_next = p->next;

		notify_level(L_WARDEN, "[ SHUTDOWN: Saving %s. ]\n",
		    p->capname);
		save_user(p, 1, 0);

		if (p->level >= L_WARDEN && (p->saveflags & U_SHUTDOWN_NOKICK))
			continue;

		if (IN_GAME(p))
			notify_level(L_WARDEN,
			    "[ SHUTDOWN: Removed %s. ]\n", p->capname);
		close_socket(p);
	}

	notify_level(L_WARDEN, "[ SHUTDOWN: Shutting down erqd. ]\n");
	stop_erqd();
	my_sleep(1);
#ifdef UDP_SUPPORT
	if (!(sysflags & SYS_EMPTY))
	{
		/* We don't want to overwrite our hosts files with the blank
		 * tables.
		 * This comes from experience... oh yes! */
		notify_level(L_WARDEN,
		    "[ SHUTDOWN: Saving intermud hosts. ]\n");
		save_hosts(hosts, F_INETD_HOSTS);
#ifdef CDUDP_SUPPORT
		save_hosts(cdudp_hosts, F_CDUDP_HOSTS);
#endif
	}
#endif

	FUN_END;
	
	notify_level(L_WARDEN, "[ SHUTDOWN: Removing ipc. ]\n");
	log_file("syslog", "Removing ipc.");
	remove_ipc();
	notify_level(L_WARDEN, "[ SHUTDOWN: Closing down jlms. ]\n");
	log_file("syslog", "Removing jlms.");
	closedown_jlms();
	/* Give the jlms time to clean up.. 
	 * This used to be implemented using sleep() but some systems
	 * object to mixing alarm() and sleep() calls
	 */
	notify_level(L_WARDEN, "[ SHUTDOWN: Waiting for jlms to exit. ]\n");
	my_sleep(6);
	notify_level(L_WARDEN, "[ SHUTDOWN: Complete. ]\n");
	log_file("syslog", "Exiting.\n");
	return 0;
}

