/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
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
#include <sys/types.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include "jeamland.h"

#ifdef CRASH_TRACE

extern char *currently_executing;

#define FUNSTACK_SIZE	20

static struct func {
	char *file;
	char *funid;
	char *arg;
	void (*addr)();
	int line;
	} functions[FUNSTACK_SIZE];
static int funptr = 0;

void
backtrace(int sig)
{
	extern struct utsname system_uname;
	FILE *fp;

	fp = fopen("log/backtrace", "a");

	fprintf(fp, "\nVERSION: %s\n", VERSION);
	fprintf(fp, "OS: %s %s %s.\n", system_uname.sysname,
	    system_uname.release, system_uname.version);
	fprintf(fp, "MACHINE: %s.\n", system_uname.machine);
	if (sig)
		fprintf(fp, "Sig: %d\n", sig);

	while(funptr--)
		fprintf(fp, "%s(%s) %s [%d] <%p>\n", functions[funptr].funid,
		    functions[funptr].arg != (char *)NULL ?
		    functions[funptr].arg : "",
		    functions[funptr].file, functions[funptr].line,
		    (unsigned long *)functions[funptr].addr);
	if (currently_executing != (char *)NULL)
		fprintf(fp, "EXECUTION: %s\n", currently_executing);

	fclose(fp);
}

/* Nothing fancy in here, we're crashing */
void
crash_handler(int sig)
{
	extern void closedown_jlms(void);

	signal(sig, SIG_DFL);

	backtrace(sig);

	notify_level(L_VISITOR, "--- SOMETHING TERRIBLE HAS HAPPENED ---\n");
	notify_level(L_WARDEN, "--- Crashing due to signal %d ---\n", sig);

	/* Have to at least attempt this.. */
	closedown_jlms();
	/* One way to make the angel log properly.. .*/
	kill(getpid(), sig);
}

void
add_function(char *funid, char *file, int line)
{
	functions[funptr].file = file;
	functions[funptr].line = line;
	functions[funptr].arg = (char *)NULL;
	functions[funptr].addr = (void (*)())NULL;
	functions[funptr++].funid = funid;
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
}

void
init_crash()
{
	signal(SIGBUS, crash_handler);
	signal(SIGSEGV, crash_handler);
	signal(SIGILL, crash_handler);
}

#endif /* CRASH_TRACE */

