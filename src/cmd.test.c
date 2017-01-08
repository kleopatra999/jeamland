/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	cmd.tmp.c
 * Function: 	A dummy file I use when writing new, complicated commands.
 *		Putting them temporarily in here saves having to continually
 *		recompile one of the large cmd.* files
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef HPUX
#include <time.h>
#endif
#include <sys/time.h>
#include <errno.h>
#include <sys/param.h>
#include <netinet/in.h>
#include "jeamland.h"

void
f_test(struct user *p, int argc, char **argv)
{
	struct stack s;
	struct svalue *sv;

	init_stack(&s);

	push_string(&s, "Test string");
	write_socket(p, "Cleaning.\n");
	clean_stack(&s);

	write_socket(p, "Stack has %d elements.\n", s.el);

	write_socket(p, "Push string.\n");
	push_string(&s, "Test string");
	write_socket(p, "Push number.\n");
	push_number(&s, 1);
	write_socket(p, "Push fpointer.\n");
	push_fpointer(&s, f_test);

	write_socket(p, "Stack has %d elements.\n", s.el);

	sv = stack_svalue(&s, 1);
	print_svalue(p, sv);
	sv = stack_svalue(&s, 2);
	print_svalue(p, sv);
	sv = stack_svalue(&s, 3);
	print_svalue(p, sv);

	write_socket(p, "Popping.\n");
	pop_stack(&s);
	write_socket(p, "Popping.\n");
	pop_stack(&s);
	write_socket(p, "Popping.\n");
	pop_stack(&s);
	write_socket(p, "Stack has %d elements.\n", s.el);
}

