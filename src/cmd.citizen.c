/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	cmd.citizen.c
 * Function:	Citizen commands
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include <netinet/in.h>
#include "jeamland.h"

/*
 * CITIZEN commands....
 */

#ifndef REQUIRE_REGISTRATION
void
validate2(struct user *p, char *c)
{
	int g;

        if ((g = gender_number(c, 1)) == -1)
        {
                write_prompt(p, "Invalid choice, please select again: ");
                return;
        }
        p->gender = g;
	p->input_to =
	    (void (*)(struct user *, char *))p->stack.sp->u.fpointer;
	dec_stack(&p->stack);
	dump_file(p, "etc", "validate", DUMP_CAT);
	return;
}

void
f_validate(struct user *p, int argc, char **argv)
{
	struct user *who;

	if ((who = find_user_msg(p, argv[1])) == (struct user *)NULL)
		return;
	if (who->level != L_VISITOR)
	{
		write_socket(p, "%s is not a guest!\n", who->name);
		return;
	}
	log_file("validate", "%s (%s@%s) validated by %s",
	    who->capname, who->uname ? who->uname : "Unknown",
	    lookup_ip_name(who), p->capname);
	who->level = L_RESIDENT;
	reposition(who);
	COPY(who->validator, p->rlname, "validator");
	write_socket(p, "Ok.\n");
	notify_levelabu(p, p->level, "[ %s has been validated by %s. ]",
	    who->capname, p->capname);

	push_fpointer(&who->stack, (void (*)())who->input_to);
	who->input_to = validate2;
	write_socket(who, "You have been granted residency by %s\n",
	   p->name);
	write_prompt(who, "Please choose a gender from the following list:\n"
	    "  (m)ale, (f)emale, (n)euter, (p)lural: ");
}
#endif /* REQUIRE_REGISTRATION */

void
f_warn(struct user *p, int argc, char **argv)
{
	struct user *who;

	if ((who = find_user_msg(p, argv[1])) == (struct user *)NULL)
		return;
	log_file("warn", "%s: %s -\n%s", p->capname, who->capname, argv[2]);
	parse_colour(who, "!r", (struct strbuf *)NULL);
	write_socket(who, "%s warns you: -=> %s <=-", p->name,
	    argv[2]);
	reset(who);
	write_socket(who, "\n");
	notify_levelabu(p, p->level, "[ %s warns %s: %s ]", p->capname,
	    who->capname, argv[2]);
	write_socket(p, "You warn %s: %s\n", who->capname, argv[2]);
}

