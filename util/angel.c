/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	angel.c
 * Function:	Guardian angel. Reboots the talker after a crash or machine
 *		reboot. Keep logs, can be run from crontab
 **********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>
#include "files.h"
#include "config.h"

#define ANGELUP		0x1
#define JLUP		0x2
#define FDEBUG		0x4

/* Configurable section... Should be ok. */
#define BINARY_NAME		"jeamland"
#define BINARY_OPTIONS		"-L"
#define DEFAULT_PS		"JeamLand"
#define ABORT_THRESHOLD		15
#define LOG_FILE		"UPRECORD"

struct {
	int sig;
	char *name;
	} signal_list[] = {
	{ SIGSEGV,	"Segmentation Violation" },
	{ SIGBUS,	"Bus Error" },
	{ SIGILL,	"Illegal Instruction" },
	{ SIGALRM,	"Alarm" },
	{ SIGQUIT,	"Quit" },
	{ SIGKILL,	"Kill" },
	{ SIGHUP,	"Hangup" },
	{ SIGTERM,	"Terminate" },
	{ SIGPIPE,	"Pipe" },
#ifdef SIGABRT
	{ SIGABRT,	"Abort (fatal error)" },
#endif
#ifdef SIGIOT
	{ SIGIOT,	"Abort (fatal error)" },
#endif
	{ 0, NULL } };

FILE *fp = (FILE *)NULL;

void
log_it(char *fmt, ...)
{
	va_list argp;
	char t_buf[30];
	time_t tm = time((time_t *)NULL);

	if (fp == (FILE *)NULL || fflush(fp) == -1)
	{
		if ((fp = fopen(LIB_PATH"/log/"LOG_FILE, "a")) == (FILE *)NULL)
		{
			perror("fopen");
			fprintf(stderr, "Could not open log file.\n");
			exit(-1);
		}
	}

	strcpy(t_buf, ctime(&tm));
	*strchr(t_buf, '\n') = '\0';
	fprintf(fp, "%s: ", t_buf);
	va_start(argp, fmt);
	vfprintf(fp, fmt, argp);
	va_end(argp);
	fprintf(fp, "\n");
	fflush(fp);
}

void
die_gracefully(int sig)
{
	log_it("### SIGHUP received - Exiting.");
	fclose(fp);
	exit(0);
}

int
main(int argc, char **argv)
{
	int restart = 0;
	int flags = 0;
	int status, i, sig;
	int angelpid, talkerpid;
	FILE *tfp;
	char pidfile[20];
	pid_t pid;
	time_t	tm, start_time;
	char *cp;

	if (chdir(TOP_DIR))
	{
		fprintf(stderr, "Cannot change to %s\n", TOP_DIR);
		exit(-1);
	}

#ifdef SETPROCTITLE
	/* Kludge, but does the job */
	if (strncmp(argv[0], "-=>", 3))
	{
		char *tmp = (char *)malloc(0x100);
		sprintf(tmp, "-=> %s Guardian Angel <=-", LOCAL_NAME);
		argv[0] = tmp;
		execv("bin/angel", argv);
	}
#endif

	if ((tfp = fopen("lib/" F_ANGEL_PID, "r")) != (FILE *)NULL)
	{
		int args, pid;

		if ((args = fscanf(tfp, "%d", &pid)) && args != EOF &&
		    kill(pid, SIGUSR2) != -1)
			flags |= ANGELUP;
		fclose(tfp);
		angelpid = pid;
	}
	else
		angelpid = -1;

	sprintf(pidfile, "lib/etc/pid.%d", DEFAULT_PORT);

	if ((tfp = fopen(pidfile, "r")) != (FILE *)NULL)
	{
                int pid, args;

		if ((args = fscanf(tfp, "%d", &pid)) && args != EOF &&
		    kill(pid, SIGUSR2) != -1)
			flags |= JLUP;
		fclose(tfp);
		talkerpid = pid;
	}
	else
		talkerpid = -1;

#ifdef NICE
	nice(NICE);
#endif

        argc--, argv++;
        while (argc > 0 && *argv[0] == '-')
        {
                for (cp = &argv[0][1]; *cp != '\0'; cp++)
                        switch (*cp)
                        {
			    case 'c':
				if (flags & (ANGELUP | JLUP))
					exit(24);
				break;
			    case 'K':
				if (kill(angelpid, SIGHUP) == -1)
					printf("Running angel not found.\n");
				else
				{
					printf("Running angel killed.\n");
					flags &= ~ANGELUP;
				}
				if (kill(talkerpid, SIGTERM) == -1)
					printf("Running talker not found.\n");
				else
				{
					printf("Running talker killed.\n");
					flags &= ~JLUP;
				}
				exit(0);
			    case 'k':
				if (kill(angelpid, SIGHUP) == -1)
					printf("Running angel not found.\n");
				else
					flags &= ~ANGELUP;
				exit(0);
			    case 'l':
				flags |= FDEBUG;
				break;
			    case 'h':
				printf("Angel boot options:\n"
				    "\tc\tExit silently if already running.\n"
				    "\tk\tKill running angel (nicely).\n"
				    "\tK\tKill running angel & talker.\n"
				    "\tl\tRun in foreground.\n");
				exit(0);
                            default:
                                fprintf(stderr,
                                    "%s: Unknown flag -%c ignored.\n", *argv,
                                    *cp);
                                break;
                        }
/*nextopt:*/
                argc--, argv++;
        }

	if (flags & ANGELUP)
	{
		fprintf(stderr,
		    "There is already a copy of the angel running.\n");
		exit(-1);
	}
	if (flags & JLUP)
	{
		fprintf(stderr,
		    "There is already a copy of the talker running.\n");
		exit(-1);
	}

	/* Background ourself.. */
	if (flags & FDEBUG)
		fp = stdout;
	else switch((int)fork())
	{
	    case 0:
		break;
	    case -1:
		perror("fork");
		fprintf(stderr, "Could not fork.\n");
		exit(-1);
	    default:
		exit(0);
	}

	if ((tfp = fopen("lib/" F_ANGEL_PID, "w")) != (FILE *)NULL)
	{
		fprintf(tfp, "%ld\n", (long)getpid());
		fclose(tfp);
	}

	signal(SIGHUP, die_gracefully);
	signal(SIGUSR2, SIG_IGN);
	log_it("######");
	log_it("### Guardian angel started (%d)", getpid());
	log_it("######");

	for (;;)
	{
		/* Start the process.. */
		switch((int)(pid = fork()))
		{
		    case 0:
			if (execl(BIN_PATH"/"BINARY_NAME, DEFAULT_PS,
			    BINARY_OPTIONS, (char *)NULL) == -1)
				perror("execl");
			_exit(-1);  /* Beware of flushing buffers. */
			/* NOTREACHED */
		    case -1:
			fprintf(stderr, "Could not fork.\n");
			fclose(fp);
			exit(-1);
		    default:
			break;
		}
		start_time = time((time_t *)NULL);
		log_it("Restart %-5d (%d).", restart++, pid);

		/* Hang around for the process to die.. */
		if (waitpid(pid, &status, 0) == -1)
		{
			log_it("### Could not start process.");
			fclose(fp);
			exit(-1);
		}
		if (WIFEXITED(status))
			log_it(
			    "\tProcess exited normally with status %d.",
			    WEXITSTATUS(status));
		else if (WIFSIGNALED(status))
		{
			log_it(
			    "\tProcess died due to untrapped signal %d.",
			    (sig = WTERMSIG(status)));
			for (i = 0; signal_list[i].sig; i++)
				if (sig == signal_list[i].sig)
				{
					log_it("\t\t'%s'",
					    signal_list[i].name);
					break;
				}
		}
		tm = time((time_t *)NULL);
		if (tm - start_time < ABORT_THRESHOLD)
		{
			log_it("### Abort threshold exceeded.");
			log_it("### Exiting.");
			fclose(fp);
			exit(-1);
		}
	}
	/* NOTREACHED */
}

