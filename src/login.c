/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	login.c
 * Function:	Functions used during login.
 **********************************************************************/
#include <stdio.h>
#include <string.h>
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

void get_name(struct user *, char *);
extern void login_user(struct user *, int);
extern char *banished(char *);

int
login_allowed(struct user *p, struct user *u)
{
	if (banished(u->rlname) != (char *)NULL)
	{
		write_socket(p, "That user has been banished.\n");
		log_file("refused", "Banished user: %s", u->capname);
		return 0;
	}
	/* Check hibernate *evil grin* */
	if (p->hibernate > current_time)
	{
		write_socket(p, "\n\nYou may not log in until %s"
		    "That's in %s.\n\n",
		    ctime(&u->hibernate),
		    conv_time(u->hibernate - current_time));
		/* Hmm. */
		if (p == u)
			notify_level(L_OVERSEER,
			    "[ Hibernation login: %s (%s@%s) ]\n",
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

static void
disconnect_query(struct user *p, char *buf)
{
	struct user *other_copy;

	if (!strlen(buf) || tolower(buf[0]) != 'y')
	{
		write_socket(p, "Goodbye.\n");
		p->flags |= U_SOCK_QUITTING;
		return;
	}
	if ((other_copy = find_user((struct user *)NULL, p->rlname)) ==
	    (struct user *)NULL)
	{
		write_socket(p, "Lost other copy while asking.\n");
		write_socket(p, "Continuing login.\n");
		p->input_to = NULL;
		login_user(p, 0);
		return;
	}
	write_socket(p, "[ Connecting ]\n");
	write_socket(other_copy,
	    "[ Login from another session, disconnecting. ]\n");
	replace_interactive(other_copy, p);

	/* Now, other_copy is our active user - p is no more */

	notify_levelabu_wrt_flags(other_copy, other_copy->level, EAR_NOTIFY,
	    "[ %s has changed connection. ]\n", other_copy->name);

	print_prompt(other_copy);
}

static void
get_password(struct user *p, char *buf)
{
	struct user *other_copy;

	if (strcmp(p->passwd, crypt(buf, p->passwd)))
	{
		write_socket(p, "\nIncorrect password.\n");
		notify_level(L_OVERSEER,
		    "[ %s (%s) has tried an incorrect password. ]\n",
		    p->capname, lookup_ip_name(p));
		log_file("wrong_password", "%s (%s)", p->capname,
		    lookup_ip_name(p));

		/* Store badlogins variable */
		p->bad_logins++;
		save_user(p, 0, 1);

		if (++p->stack.sp->u.number > PASSWORD_RETRIES)
		{
			if ((other_copy = find_user((struct user *)NULL,
			    p->rlname)) != (struct user *)NULL &&
			    !strcmp(p->rlname, other_copy->rlname))
			{
				yellow(other_copy);
				write_socket(other_copy,
				    "[ An invalid login to your username has "
				    "occurred ]\n"
				    "[ Contact an administrator if this "
				    "concerns you. ]\n");
				reset(other_copy);
			}
			write_socket(p, "Too many failed attempts.\n");
			p->flags |= U_SOCK_QUITTING;
		}
		else
			write_prompt(p, "%s", PASS);
		return;
	}
	dec_stack(&p->stack);
	echo(p);
	if (!login_allowed(p, p))
	{
		p->flags |= U_SOCK_QUITTING;
		return;
	}
	if ((other_copy = find_user((struct user *)NULL, p->rlname)) 
	    != (struct user *)NULL && !strcmp(p->rlname, other_copy->rlname))
	{
		write_socket(p, "You are already logged on.\n");
		write_prompt(p, "Disconnect other copy ? [y / n] ");
		p->input_to = disconnect_query;
		return;
	}
	p->input_to = NULL;
	login_user(p, 0);
}

static void
accept_conditions(struct user *p, char *c)
{
	if (strcasecmp(c, "yes"))
	{
		write_socket(p, "\n\nGoodbye.\n");
		p->flags |= U_SOCK_QUITTING;
		return;
	}

	write_socket(p, "\nWelcome to %s! Enjoy.\n", LOCAL_NAME);
	log_file("new_user", "%s (from %s)", p->name, lookup_ip_name(p));

	p->input_to = NULL;
#ifdef AUTO_VALIDATE
	p->level = L_RESIDENT;
#else
	p->level = L_VISITOR;
#endif
	COPY(p->title, NEWBIE_TITLE, "title");
	COPY(p->prompt, PROMPT, "prompt");
	login_user(p, 0);
}

static void
login_name_ok(struct user *p, char *c)
{
	if (!strlen(c) || tolower(*c) != 'y')
	{
		write_prompt(p, "\n%s", LOGIN);
		logon_name(p);
		p->input_to = get_name;
		return;
	}

#ifdef REQUIRE_CONDITIONS
	if (!dump_file(p, "etc", F_CONDITIONS, DUMP_CAT))
		write_socket(p,
		    "\n!!!!ERROR!!!! - Conditions file is missing.\n");
	write_socket(p, "\nDo you accept these conditions ?\n"
	    "You must type the entire word 'yes' to accept: ");
	p->input_to = accept_conditions;
#else
	accept_conditions(p, "yes");
#endif
}

void
get_name(struct user *p, char *buf)
{
	struct banned_site *h;
	struct user *other_copy;
	int banned = 0;
	int blen;
	int us;
	int sly = 0;

	extern struct banned_site *sitebanned(char *);

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
		p->flags |= U_SOCK_QUITTING;
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
	if (offensive(lower_case(buf)))
	{
		write_socket(p, "Name caught in offensiveness filter.\n");
		write_socket(p,
		    "Come up with something better or go elsewhere!\n");
		log_file("refused", "Offensive name: %s", buf);
		notify_level(L_OVERSEER,
		    "[ Offensive name login: %s (%s@%s) ]\n",
		    buf, p->uname == (char *)NULL ? "unknown" : p->uname,
		    lookup_ip_name(p));
		p->flags |= U_SOCK_QUITTING;
		return;
	}
	if (banished(lower_case(buf)) != (char *)NULL)
	{
		write_socket(p, "That user has been banished.\n");
		log_file("refused", "Banished user: %s", buf);
		notify_level(L_OVERSEER,
		    "[ Banished user login: %s (%s@%s) ]\n",
		    buf, p->uname == (char *)NULL ? "unknown" : p->uname,
		    lookup_ip_name(p));
		p->flags |= U_SOCK_QUITTING;
		return;
	}
	if ((h = sitebanned(lookup_ip_number(p))) != (struct banned_site *)NULL
	    || (h = sitebanned(lookup_ip_name(p))) !=
	    (struct banned_site *)NULL)
	{
		int g = exist_user(lower_case(buf));
		switch(h->level)
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
			write_socket(p, "Unknown siteban code \n");
			log_file("refused", "WARNING: Unknown ban code %d",
			    h->level);
			log_file("refused", "Banned site: %s (%s)", buf,
			    lookup_ip_number(p));
			banned++;
			break;
		}
	}

	other_copy = find_user(p, buf);
	if (restore_user(p, lower_case(buf)))
	{
		if (banned)
			if (h->level == BANNED_ALL || p->level < L_OVERSEER)
			{
				notify_level(L_OVERSEER,
				    "[ Sitebanned login: %s (%s@%s) ]\n",
		    		    buf, p->uname == (char *)NULL ?
				    "unknown" : p->uname, lookup_ip_name(p));
				p->flags |= U_SOCK_QUITTING;
				return;
			}
			else
				write_socket(p,
				    "\nAs you are Admin, you are allowed"
				    " to proceed.\n");
		if (sly && p->level > L_WARDEN)
			p->saveflags |= U_SLY_LOGIN;
		p->flags |= U_LOGGING_IN;
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
		p->flags |= U_SOCK_QUITTING;
		return;
	}
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
}

