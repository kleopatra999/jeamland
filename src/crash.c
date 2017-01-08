/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	crash.c
 * Function:	Function tracing support.. for machines on which core dumps
 *		don't work or where the debugger isn't giving useful info..
 *		Do I sound bitter ?.. ah well
 **********************************************************************/
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include "jeamland.h"

#ifdef CRASH_TRACE

#undef CRASHTRACE_DEBUG

extern time_t current_time;

#define FUNSTACK_SIZE	40

static struct func {
	char *file;
	char *funid;
	char *arg;
	void (*addr)();
	int line;
	} functions[FUNSTACK_SIZE];
static int funptr = 0;

void
backtrace(char *msg, int sig)
{
	extern struct utsname system_uname;
	FILE *fp;
	int i;

	fp = fopen("log/backtrace", "a");

	fprintf(fp, "\n>>> %s : %s\n", nctime(&current_time), msg);
	fprintf(fp, "VERSION: %s\n", VERSION);
	fprintf(fp, "OS: %s %s %s.\n", system_uname.sysname,
	    system_uname.release, system_uname.version);
	fprintf(fp, "MACHINE: %s.\n", system_uname.machine);
	if (sig)
		fprintf(fp, "Sig: %d\n", sig);

	i = funptr;
	while (i--)
		fprintf(fp, "%s(%s) [%s %d] <%p>\n", functions[i].funid,
		    functions[i].arg != (char *)NULL ?
		    functions[i].arg : "",
		    functions[i].file, functions[i].line,
		    (unsigned long *)functions[i].addr);

	fclose(fp);
}

/* Nothing fancy in here, we're crashing */
void
crash_handler(int sig)
{
	extern void closedown_jlms(void);

	signal(sig, SIG_DFL);

	backtrace("crash_handler", sig);

	notify_level(L_VISITOR, "--- SOMETHING TERRIBLE HAS HAPPENED ---");
	notify_level(L_WARDEN, "--- Crashing due to signal %d ---", sig);

	/* Have to at least attempt this.. */
	closedown_jlms();
	/* One way to make the angel log properly.. .*/
	kill(getpid(), sig);
}

void
check_fundepth(int dep)
{
	if (funptr != dep)
		fatal("Crashtrace function depth != %d", dep);
}

void
add_function(char *funid, char *file, int line)
{
	functions[funptr].file = file;
	functions[funptr].line = line;
	functions[funptr].arg = (char *)NULL;
	functions[funptr].addr = (void (*)())NULL;
	functions[funptr++].funid = funid;
	if (funptr >= FUNSTACK_SIZE)
		fatal("Crash function stack overflow.");
#ifdef CRASHTRACE_DEBUG
	backtrace("Crashtrace debug (add)", 0);
#endif
}

void
add_function_line(int line)
{
	functions[funptr - 1].line = line;
}

void
add_function_arg(char *txt)
{
	functions[funptr - 1].arg = txt;
}

void
add_function_addr(void (*addr)())
{
	functions[funptr - 1].addr = addr;
}

void
end_function()
{
	funptr--;
#ifdef CRASHTRACE_DEBUG
	backtrace("Crashtrace debug (end)", 0);
#endif
}

void
init_crash()
{
	signal(SIGBUS, crash_handler);
	signal(SIGSEGV, crash_handler);
	signal(SIGILL, crash_handler);
}

#endif /* CRASH_TRACE */

