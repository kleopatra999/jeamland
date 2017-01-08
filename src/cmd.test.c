/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
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

#if 0
void
f_test(struct user *p, int argc, char **argv)
{
	write_socket(p, "Nothing to test...\n");
}
#endif

#if 1
void
f_test(struct user *p, int argc, char **argv)
{
	/* Test mapping code/decode */

	char *str = "([\"alcides\":\"andy\",\"angellica\":1,\"andy\":({\"test\",2,5,\"wibble\",}),\"ohno\":([\"map\":\"val\",\"dup\":\"oops\",]),])";
	char *dyt;
	struct svalue *sv;

	write_socket(p, "String is: %s\n", str);

	dyt = string_copy(str, "f_test");

	if ((sv = string_to_svalue(dyt, "f_test")) == (struct svalue *)NULL)
	{
		write_socket(p, "\nDecode failed.\n");
		return;
	}

	xfree(dyt);

	write_socket(p, "\nDecoded ok.\n\n");

	write_socket(p, "mapping[\"andy\"] = ");
	print_svalue(p, map_string(sv->u.map, "andy"));
	write_socket(p, "\n");


	write_socket(p, "mapping[\"ohno\"] = ");
	print_svalue(p, map_string(sv->u.map, "ohno"));
	write_socket(p, "\n");

	dyt = svalue_to_string(sv);
	write_socket(p, "\nEncoded as: %s\n\n", dyt);

	if (!strcmp(str, dyt))
		write_socket(p, "MATCH!\n");
	else
		write_socket(p, "NO MATCH :(\n");

	free_svalue(sv);
	xfree(dyt);
}
#endif

#if 0
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
	write_socket(p, "\n");
	sv = stack_svalue(&s, 2);
	print_svalue(p, sv);
	write_socket(p, "\n");
	sv = stack_svalue(&s, 3);
	print_svalue(p, sv);
	write_socket(p, "\n");

	write_socket(p, "Popping.\n");
	pop_stack(&s);
	write_socket(p, "Popping.\n");
	pop_stack(&s);
	write_socket(p, "Popping.\n");
	pop_stack(&s);
	write_socket(p, "Stack has %d elements.\n", s.el);
	write_socket(p, "Next pop will cause a stack underflow.\n");
	/* This will cause a stack underflow */
	pop_stack(&s);
}
#endif

