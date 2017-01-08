/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	login.c
 * Function:	Functions used during login.
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>
#ifdef HPUX
#include <sys/time.h>
#include <unistd.h>
#endif
#include <time.h>
#include <sys/param.h>
#include <netinet/in.h>
#include "jeamland.h"

#define LOGIN "Enter your name: "
#define PASS  "Password: "

extern time_t current_time;
extern struct termcap_entry termcap[];

void get_name(struct user *, char *);
extern void login_user(struct user *, int);
extern char *banished(char *);

int
login_allowed(struct user *p, struct user *u)
{
	if (banished(u->rlname) != (char *)NULL)
	{
		dump_file(p, "etc", F_BANISH, DUMP_CAT);
		log_file("refused", "Banished user: %s", u->capname);
		notify_level(L_OVERSEER,
		    "[ Banished user login: %s (%s@%s) ]",
		    u->capname, u->uname == (char *)NULL ? "unknown" :
		    u->uname, lookup_ip_name(u));
		return 0;
	}
	/* Check hibernate *evil grin* */
	if (u->hibernate > current_time)
	{
		write_socket(p, "\n\nYou may not log in until %s"
		    "That's in %s.\n\n",
		    ctime(user_time(p, u->hibernate)),
		    conv_time(u->hibernate - current_time));
		/* Hmm. */
		if (p == u)
			notify_level(L_OVERSEER,
			    "[ Hibernation login: %s (%s@%s) ]",
			    p->capname, p->uname == (char *)NULL ? "unknown" :
			    p->uname, lookup_ip_name(p));
		log_file("refused", "Hibernate user: %s (%ld)", u->capname,
		    (long)u->hibernate);
		return 0;
	}
	return 1;
}

void
logon_name(struct user *p)
{
	COPY(p->name, "<logon>", "name");
	COPY(p->rlname, "<logon>", "name");
	COPY(p->capname, "<logon>", "name");
}

static void login_name_ok(struct user *, char *);

static void
login_set_gender(struct user *p, char *c)
{
	int g;

	if (c != (char *)NULL)
	{
		if ((g = gender_number(c, 1)) == -1)
		{
			write_socket(p,
			    "Invalid choice, please select again: ");
			return;
		}
		p->gender = g;
	}

	/* Make editor autowrap a default. */
	if (p->version < JL_VERSION_CODE(1, 1, 36))
		p->medopts |= U_EDOPT_WRAP;

	/* Make editor prompt a default. */
	if (p->version < JL_VERSION_CODE(1, 2, 0))
		p->medopts |= U_EDOPT_PROMPT;

	/* If user has never logged in before, jump into the new user
	 * code to show conditions of use etc. */
	if (!p->num_logins)
		login_name_ok(p, "y");
	else
		login_user(p, 0);
}

static void
perform_login(struct user *p)
{
	/* Does this user need to select a new gender ? */
	if (p->gender == G_UNSET)
	{
		write_socket(p, "\nAvailable genders are: %s\n"
		    "Please select a gender: ", gender_list());
		p->input_to = login_set_gender;
	}
	else
		login_set_gender(p, (char *)NULL);
}

static void
disconnect_query(struct user *p, char *buf)
{
	struct user *other_copy;

	if (!strlen(buf) || tolower(buf[0]) != 'y')
	{
		write_socket(p, "Goodbye.\n");
		disconnect_user(p, 0);
		return;
	}
	if ((other_copy = find_user_absolute((struct user *)NULL, p->rlname)) ==
	    (struct user *)NULL)
	{
		write_socket(p, "Lost other copy while asking.\n"
		    "Continuing login.\n");
		p->input_to = NULL;
		perform_login(p);
		return;
	}
	write_socket(p, "[ Connecting ]\n");
	write_socket(other_copy,
	    "[ Login from another session, disconnecting. ]\n");

	replace_interactive(other_copy, p);

	/* Now, other_copy is our active user - p is no more */

	notify_levelabu_wrt_flags(other_copy, other_copy->level, EAR_NOTIFY,
	    "[ %s has changed connection. ]", other_copy->name);

	print_prompt(other_copy);
}

void
get_password(struct user *p, char *buf)
{
	struct user *other_copy;

	FUN_START("get_password");

	if (strcmp(p->passwd, crypt(buf, p->passwd)))
	{
		write_socket(p, "\nIncorrect password.\n");
		notify_level(L_OVERSEER,
		    "[ %s (%s) has tried an incorrect password. ]",
		    p->capname, lookup_ip_name(p));
		log_file("wrong_password", "%s (%s)", p->capname,
		    lookup_ip_name(p));

		/* Store badlogins variable */
		p->bad_logins++;
		save_user(p, 0, 1);

		FUN_LINE;

		if (++p->stack.sp->u.number > PASSWORD_RETRIES)
		{
			FUN_LINE;
			if ((other_copy =
			    find_user_absolute((struct user *)NULL,
			    p->rlname)) != (struct user *)NULL)
			{
				attr_colour(other_copy, "notify");
				write_socket(other_copy,
				    "[ An invalid login to your username has "
				    "occurred ]\n"
				    "[ Contact an administrator if this "
				    "concerns you. ]");
				reset(other_copy);
				write_socket(other_copy, "\n");
			}
			write_socket(p, "Too many failed attempts.\n");
			write_socket(p,
			    "Please send email to:\n\t%s\n"
			    "with any queries or if "
			    "you need your password resetting.\n",
			    OVERSEER_EMAIL);
			disconnect_user(p, 0);
		}
		else
			write_prompt(p, "%s", PASS);
		FUN_END;
		return;
	}
	dec_stack(&p->stack);
	echo(p);
	if (!login_allowed(p, p))
	{
		disconnect_user(p, 0);
		FUN_END;
		return;
	}
	FUN_LINE;
	if ((other_copy = find_user_absolute((struct user *)NULL, p->rlname)) 
	    != (struct user *)NULL)
	{
		write_socket(p, "You are already logged on.\n");
		write_prompt(p, "Disconnect other copy ? [y / n] ");
		p->input_to = disconnect_query;
		FUN_END;
		return;
	}
	p->input_to = NULL;
	perform_login(p);
	FUN_END;
}

static void
termtype_select(struct user *p, char *c)
{
	if (c != (char *)NULL)
	{
		int i, flag;

		for (i = flag = 0; termcap[i].name; i++)
		{
			if (!strcmp(termcap[i].name, c))
			{
				p->terminal = i;
				p->saveflags |= U_ANSI;
				write_socket(p,
				    "Terminal type set to: %s\n", c);
				flag = 1;
				break;
			}
		}
		if (!flag)
		{
			write_socket(p, "Unknown terminal type, select: ");
			return;
		}
	}
	p->input_to = NULL_INPUT_TO;
	
	/* Hook for further questions */

	login_user(p, 0);
}

static void
col_select(struct user *p, char *c)
{
	extern void f_attr(struct user *, int, char **);
	int i;

	if ((i = atoi(c)) < 1 || i > 3)
	{
		write_socket(p, "Invalid choice, select: ");
		return;
	}

	switch (i)
	{
	    case 1:
		termtype_select(p, (char *)NULL);
		return;

	    case 2:
	    {
		char *argv[] = { "attr", "incoming", "d" };

		f_attr(p, 3, argv);
		argv[1] = "notify";
		argv[2] = "!y";
		f_attr(p, 3, argv);
		break;
	    }

	    case 3:
	    {
		char *argv[] = { "attr", "-default" };
		int i = p->level;

		p->level = L_RESIDENT;
		f_attr(p, 2, argv);
		p->level = i;

		COPY(p->prompt, COLOUR_PROMPT, "prompt");
		break;
	    }
	}

	write_socket(p, "\n\n");
	show_terminals(p);
	write_socket(p, "If you are unsure, choose colvt100 or vt100.\n");
	write_socket(p, "Select: ");

	p->input_to = termtype_select;
}

static void
login_newuser_note(struct user *p)
{
	write_socket(p, "\nWelcome to %s! Enjoy.\n\n", LOCAL_NAME);
	log_file("new_user", "%s (from %s)", p->name, lookup_ip_name(p));

	p->input_to = NULL_INPUT_TO;
#ifdef AUTO_VALIDATE
	if (p->level == L_VISITOR)
		p->level = L_RESIDENT;
#endif
	COPY(p->title, NEWBIE_TITLE, "title");
	COPY(p->prompt, PROMPT, "prompt");

	write_socket(p, "Please choose the initial level of colour you would"
	    " like:\n"
	    "    1.  No colour.\n"
	    "    2.  Basic colour (incoming messages are highlighted).\n"
	    "    3.  Full colour.\n"
	    "Select: ");
	p->input_to = col_select;
}

static void
login_newuser_note_ret(struct user *p, char *c)
{
	login_newuser_note(p);
}

static void
accept_conditions(struct user *p, char *c)
{
	char *t;

	if (strcasecmp(c, "yes"))
	{
		write_socket(p, "\n\nGoodbye.\n");
		disconnect_user(p, 0);
		return;
	}

	if (file_size(F_NEWUSER) == -1 ||
	    (t = read_file(F_NEWUSER)) == (char *)NULL)
		login_newuser_note(p);
	else
	{
		write_socket(p, "%s\n", t);
		write_prompt(p, "\nPress return to continue:");
		xfree(t);
		p->input_to = login_newuser_note_ret;
	}
}

static void
conditions_ret(struct user *p)
{
	write_socket(p, "\nDo you accept these conditions ?\n"
	    "You must type the entire word 'yes' to accept: ");
	p->input_to = accept_conditions;
}

static void
login_name_ok(struct user *p, char *c)
{
#ifdef REQUIRE_CONDITIONS
	char *str;
#endif

	if (!strlen(c) || tolower(*c) != 'y')
	{
		write_prompt(p, "\n%s", LOGIN);
		logon_name(p);
		p->input_to = get_name;
		return;
	}

#ifdef REQUIRE_CONDITIONS
	if ((str = read_file(F_CONDITIONS)) == (char *)NULL)
	{
		write_socket(p,
		    "\n!!!!ERROR!!!! - Conditions file is missing.\n");
		accept_conditions(p, "yes");
	}
	p->input_to = NULL_INPUT_TO;
	more_start(p, str, conditions_ret);
#else
	accept_conditions(p, "yes");
#endif
}

static void
login_who_ret(struct user *p)
{
	p->input_to = get_name;
	write_socket(p, "\n");
	get_name(p, "");
}

void
get_name(struct user *p, char *buf)
{
	struct banned_site *h;
#ifndef REQUIRE_REGISTRATION
	struct user *other_copy;
#endif
	int banned = 0;
	int blen;
	int sly = 0;
	int us;

	buf = lower_case(buf);

	if (!strcmp(buf, "quit"))
	{
		if (p->flags & U_CONSOLE)
		{
			extern void detach_console(void);

			detach_console();
			p->flags ^= U_CONSOLE;
			write_socket(p, "***\nConsole detached.\n***\n");
		}
		else
			write_socket(p, "Goodbye!\n");
		disconnect_user(p, 0);
		return;
	}
	else if (!strcmp(buf, "who"))
	{
		char *t = who_text(FINGER_LOCAL);

		p->input_to = NULL_INPUT_TO;

		write_socket(p, "\n");
		more_start(p, t, login_who_ret);
		return;
	}

	if (!(blen = strlen(buf)))
	{
		write_prompt(p, LOGIN);
		return;
	}
	if (blen > NAME_LEN)
	{
		write_prompt(p, "Name too long.\n%s", LOGIN);
		return;
	}
	if (*buf == '-')
	{
		sly = 1;
		buf++;
	}
	if (!valid_name(p, buf, 0))
	{
		write_prompt(p, LOGIN);
		return;
	}
	if (offensive(buf))
	{
		write_socket(p, "Name caught in offensiveness filter.\n");
		write_socket(p,
		    "Come up with something better or go elsewhere!\n");
		log_file("refused", "Offensive name: %s", buf);
		notify_level(L_OVERSEER,
		    "[ Offensive name login: %s (%s@%s) ]",
		    buf, p->uname == (char *)NULL ? "unknown" : p->uname,
		    lookup_ip_name(p));
		disconnect_user(p, 0);
		return;
	}

	if (banished(buf) != (char *)NULL)
	{
		dump_file(p, "etc", F_BANISH, DUMP_CAT);
		log_file("refused", "Banished user: %s", buf);
		notify_level(L_OVERSEER,
		    "[ Banished user login: %s (%s@%s) ]",
		    buf, p->uname == (char *)NULL ? "unknown" : p->uname,
		    lookup_ip_name(p));
		disconnect_user(p, 0);
		return;
	}

	if ((h = sitebanned(p->socket.addr.sin_addr.s_addr)) !=
	    (struct banned_site *)NULL)
	{
		int g = exist_user(buf);
		switch (h->level)
		{
		    case BANNED_NEW:
			if (g)
				break;
		    case BANNED_ALL:
		    case BANNED_ABA:
			write_socket(p, "Your site is banned:\n%s", h->reason);
			log_file("refused", "Banned site: %s (%s)", buf,
			    lookup_ip_number(p));
			banned++;
			break;
		    default:
			write_socket(p, "Unknown siteban code\n");
			log_file("refused", "WARNING: Unknown ban code %d",
			    h->level);
			log_file("refused", "Banned site: %s (%s)", buf,
			    lookup_ip_number(p));
			banned++;
			break;
		}
	}

	if (restore_user(p, buf))
	{
		if (banned)
			if (h->level == BANNED_ALL || p->level < L_CONSUL)
			{
				notify_level(L_OVERSEER,
				    "[ Sitebanned login: %s (%s@%s) ]",
		    		    buf, p->uname == (char *)NULL ?
				    "unknown" : p->uname, lookup_ip_name(p));
				disconnect_user(p, 0);
				return;
			}
			else
				write_socket(p,
				    "\nAs you are Admin, you are allowed"
				    " to proceed.\n");
		if (sly && p->level > L_WARDEN)
			p->saveflags |= U_SLY_LOGIN;
		if (p->passwd == (char *)NULL || !strlen(p->passwd))
		{
			p->saveflags |= U_FPC;
			write_socket(p, "You have no password!\n");
			p->input_to = NULL;
			login_user(p, 0);
			return;
		}
		p->input_to = get_password;
		push_number(&p->stack, 0);
		noecho(p);
		write_prompt(p, PASS);
		return;
	}
	if (banned)
	{
		disconnect_user(p, 0);
		return;
	}

#ifndef REQUIRE_REGISTRATION
	other_copy = find_user_absolute(p, buf);

	if (other_copy && IN_GAME(other_copy) &&
	    !strcmp(other_copy->rlname, p->rlname))
	{
		write_socket(p, "There is already a guest using that name.\n");
		write_prompt(p, LOGIN);
		logon_name(p);
		return;
	}
	if ((us = query_real_level(p->rlname)) != -1)
	{
		/* Present in mastersave, but user file missing! */
		log_file("error", "Badly deleted: %s (%d)",
		    capitalise(p->rlname), us);
		 
		write_socket(p,
		    "This name belonged to a user who was deleted in a bad\n"
		    "way. Please contact an admin about this so that it may\n"
		    "be fixed.\n");
		write_prompt(p, LOGIN);
		logon_name(p);
		return;
	}

	p->input_to = login_name_ok;
	write_socket(p, "\n[ New user ]\n\n");
	write_prompt(p, "Is %s definitely the name you want ? ", p->name);

#else /* REQUIRE_REGISTRATION */

	if (strcmp(buf, "guest"))
	{
		dump_file(p, "etc", F_UNREG, DUMP_CAT);
		write_prompt(p, LOGIN);
		logon_name(p);
		return;
	}

	for (us = 1; us < 100; us++)
	{
		char name[10];

		sprintf(name, "guest%d", us);
		if (find_user_absolute((struct user *)NULL, name) !=
		    (struct user *)NULL)
			continue;
		COPY(p->rlname, name, "rlname");
		*name = 'G';
		COPY(p->name, name, "name");
		COPY(p->capname, name, "capname");
#ifdef SUPER_SNOOP
		p->snoop_fd = start_supersnoop(p->rlname);
#endif
		login_name_ok(p, "y");
		return;
	}
	write_socket(p, "Sorry, there are too many guests currently using"
	    " the talker.\n");
	disconnect_user(p, 0);

#endif /* REQUIRE_REGISTRATION */
}

