/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	cmd.overseer.c
 * Function:	Overseer commands.
 **********************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include "jeamland.h"

extern struct user *users;
extern struct room *rooms;
extern int sysflags;
extern struct command commands[], partial_commands[];

/* L_OVERSEER commands.... */

void
f_chemail(struct user *p, int argc, char **argv)
{
	struct user *who;
	int remove = 0;

	if (!strcmp(argv[1], "-r"))
	{
		remove = 1;
		argv[1] = argv[2];
	}

	if ((who = doa_start(p, argv[1])) == (struct user *)NULL)
	{
		write_socket(p, "User %s does not exist.\n", argv[1]);
		return;
	}

	if (!access_check(p, who))
	{
		doa_end(who, 0);
		return;
	}

	who->saveflags &= ~U_EMAIL_VALID;

	if (remove)
	{
		who->email = (char *)NULL;
		who->saveflags |= U_NOEMAIL;
	}
	else
	{
		who->saveflags &= ~U_NOEMAIL;
		COPY(who->email, argv[2], "email");
	}
	doa_end(who, 1);
	write_socket(p, "Ok.\n");
}

void
f_chtitle(struct user *p, int argc, char **argv)
{
        struct user *who;

        if ((who = doa_start(p, argv[1])) == (struct user *)NULL)
        {
                write_socket(p, "User %s does not exist.\n", argv[1]);
                return;
        }

        if (!access_check(p, who))
        {
                doa_end(who, 0);
                return;
        }

        COPY(who->title, argv[2], "title");
        doa_end(who, 1);
}

void
f_comment(struct user *p, int argc, char **argv)
{
	struct user *u;

	deltrail(argv[1]);
	if ((u = doa_start(p, argv[1])) == (struct user *)NULL)
	{
		write_socket(p, "User %s not found.\n", argv[1]);
		return;
	}
	if (!access_check(p, u))
	{
		doa_end(u, 0);
		return;
	}
	if (argc < 3)
	{
		FREE(u->comment);
	}
	else
	{
		COPY(u->comment, argv[2], "comment");
	}
	write_socket(p, "Changed comment for %s.\n", u->capname);
	notify_levelabu(p, p->level, "[ %s has changed the comment on %s. ]",
	    p->capname, u->capname);
	doa_end(u, 1);
}

void
f_cost(struct user *p, int argc, char **argv)
{
	long outime, utime,
	     ostime, stime,
	     orss, rss;
	char *cmd;

	cmd = string_copy(argv[1], "f_cost cmd copy");
	set_times(&outime, &ostime, &orss);
	parse_command(p, &cmd);
	set_times(&utime, &stime, &rss);
	xfree(cmd);

	write_socket(p, "User time: %ld, System time: %ld, Rss: %ld\n",
	    utime - outime, stime - ostime, rss - orss);
	if (sysflags & SYS_EVENT)
		write_socket(p,
		    "WARNING: An event occured, result may be invalid.\n");
}

void
f_cpuser(struct user *p, int argc, char **argv)
{
	struct user *who;
	char fname[MAXPATHLEN + 1];

	argv[2] = lower_case(argv[2]);

	if (!valid_name(p, argv[2], 0))
		return;

	if (offensive(argv[2]))
	{
                write_socket(p, "Name caught in offensiveness filter.\n");
		return;
	}

	if (exist_user(argv[2]))
	{
		write_socket(p, "User %s already exists!.\n", argv[2]);
		return;
	}

	if ((who = dead_copy(argv[1])) == (struct user *)NULL)
	{
		write_socket(p, "No such user, %s.\n", argv[1]);
		return;
	}

	if (!access_check(p, who))
	{
		tfree_user(who);
		return;
	}

	/* Copy the mailbox over.. */
	who->mailbox = restore_mailbox(who->rlname);
	sprintf(fname, "mail/%s", argv[2]);
	COPY(who->mailbox->fname, fname, "board fname");
	store_board(who->mailbox);
	free_board(who->mailbox);
	who->mailbox = (struct board *)NULL;

	/* Now the user... */
	COPY(who->rlname, argv[2], "rlname");
	COPY(who->name, who->rlname, "name");
	*who->name = toupper(*who->name);
	COPY(who->capname, who->name, "capname");

	save_user(who, 0, 1);
	tfree_user(who);

	write_socket(p, "Copied user %s to %s.\n", argv[1], argv[2]);
}

/* A function for debugging the angel! */
void
crash2(struct user *p, char *c)
{
	/* Well, this is one way to do it ;) */
	strcpy(NULL, NULL);
}

void
f_crash(struct user *p, int argc, char **argv)
{
	if (!ISROOT(p))
	{
		write_socket(p, "Only "ROOT_USER" can do this.\n");
		return;
	}
	if (argc <= 1)
		crash2(p, "");
	else
	{
		CHECK_INPUT_TO(p)
		p->input_to = crash2;
		write_prompt(p, "Press any key to crash the talker: ");
	}
}

#ifdef JL_CRON
void
f_cron(struct user *p, int argc, char **argv)
{
	extern void init_cron(void);

	if (argc < 2)
	{
		if (!dump_file(p, "etc", "crontab", DUMP_MORE))
			write_socket(p, "No crontab.\n");
		return;
	}

	if (!strcmp(argv[1], "-reload"))
	{
		/* Kill off all current "cron" events and reinitialise */
		remove_events_by_name("cron");
		init_cron();

		write_socket(p, "Reinitialised cron.\n");
	}
	else if (!strcmp(argv[1], "-kill"))
	{
		remove_events_by_name("cron");
		write_socket(p, "Removed all cron events.\n");
	}
	else
		write_socket(p, "cron: unknown argument.\n");
}
#endif

#ifdef DEBUG_MALLOC
void
f_dm(struct user *p, int argc, char **argv)
{
	extern void dump_malloc_table(int);

	dump_malloc_table(argc > 1 && !strcmp(argv[1], "all"));
	write_socket(p, "Dumped malloc table.\n");
}
#endif

void
f_erqd(struct user *p, int argc, char **argv)
{
	extern int erqd_fd;
	int req;
	char buf[BUFFER_SIZE];
	extern void start_erqd(void), stop_erqd(void);

	if (argc < 3)
	{
		if (!strcmp(argv[1], "restart"))
		{
			if (erqd_fd != -1)
			{
				write_socket(p, "Erqd already running.\n");
				return;
			}
			start_erqd();
			write_socket(p, "Ok.\n");
			return;
		}
		else if (!strcmp(argv[1], "kill"))
		{
			if (erqd_fd == -1)
			{
				write_socket(p, "No erqd running.\n");
				return;
			}
			stop_erqd();
			write_socket(p, "Ok.\n");
			return;
		}
		write_socket(p, "Unknown request.\n");
		return;
	}
	deltrail(argv[1]);
	if (argc < 3)
		return;
	if (!strcmp(argv[1], "send"))
	{
		if (sscanf(argv[2], "%d:%s", &req, buf) != 2)
		{
			write_socket(p,
			    "Incorrect arguments to 'erqd send'.\n");
			return;
		}
		strcat(buf, "\n");
		send_user_erq(p->rlname, req, buf);
		write_socket(p, "Sent.\n");
	}
}

void
f_forceall(struct user *p, int argc, char **argv)
{
	struct user *u;

	log_file("force", "%s forced ALL to %s", p->capname, argv[1]);
	for (u = users->next; u != (struct user *)NULL; u = u->next)
	{
		if (u == p || (u->level >= p->level && !ISROOT(p)))
			continue;
		fwrite_socket(u, "You feel a strange presence in your mind.\n");
		fwrite_socket(u, "%s\n", argv[1]);
		insert_command(&u->socket, argv[1]);
	}
	write_socket(p, "Forced all to %s\n", argv[1]);
}

void
f_fpc(struct user *p, int argc, char **argv)
{
	struct user *u;

	if ((u = doa_start(p, argv[1])) == (struct user *)NULL)
	{
		write_socket(p, "No such user, %s.\n", argv[1]);
		return;
	}

	if (!access_check(p, u))
	{
		write_socket(p, "Permission denied.\n");
		doa_end(u, 0);
		return;
	}

	write_socket(p, "FPC flag in %s turned %s.\n", u->capname,
	    (u->saveflags ^= U_FPC) & U_FPC ? "on" : "off");
	doa_end(u, 1);
}

#ifdef IMUD3_SUPPORT
void
f_i3ctl(struct user *p, int argc, char **argv)
{
	deltrail(argv[1]);

	if (!strcmp(argv[1], "close"))
	{
		if (i3_shutdown() == -1)
			write_socket(p, "Not connected.\n");
		else
			write_socket(p,
			    "Successfully disconnected from router.\n");
		return;
	}

	if (!strcmp(argv[1], "open"))
	{
		int i;

		if ((i = i3_startup()) == -1)
			write_socket(p, "Already connected to router.\n");
		else if (i == 0)
			write_socket(p, "Connect failed.\n");
		else
			write_socket(p, "Successfully connected to router.\n");
		return;
	}

	if (!strcmp(argv[1], "send"))
	{
		if (argc < 3)
		{
			write_socket(p, "Syntax: i3ctl send <string>\n");
			return;
		}
		i3_send_string(argv[2]);
		write_socket(p, "Sent.\n");
		return;
	}

	if (!strcmp(argv[1], "hosts"))
	{
		if (argc < 3)
		{
			write_socket(p, "Use 'i3hosts' to view hosts.\n");
			return;
		}

		deltrail(argv[2]);

		if (!strcmp(argv[2], "dump"))
		{
			i3_save();
			write_socket(p, "Done.\n");
			return;
		}

		write_socket(p, "Unknown i3 hosts command, %s.\n", argv[2]);
		return;
	}

	if (!strcmp(argv[1], "chan"))
	{
		/* Handle channels... */
		struct i3_channel *i;

		if (argc < 3)
		{
			/* List... */
			struct strbuf s;
			extern struct i3_channel *i3chans;

			init_strbuf(&s, 0, "i3ctl chanlist");

			add_strbuf(&s, "Channel            Owner"
			    "       Status  On/Off\n");

			for (i = i3chans; i != (struct i3_channel *)NULL;
			    i = i->next)
			{
				char buf[21];

				my_strncpy(buf, i->owner, 20);

				sadd_strbuf(&s, "%-20s %-20s %s %d\n",
				    i->name, buf, i3chan_stat(i->status),
				    i->listening);
			}

			pop_strbuf(&s);
			more_start(p, s.str, NULL);

			return;
		}

		deltrail(argv[2]);

		if (!strcmp(argv[2], "delete"))
		{
			if (i3_del_channels() == -1)
				write_socket(p,
				    "Disconnect from router first.\n");
			else
				write_socket(p, "Ok.\n");
			return;
		}

		if (argc < 4)
		{
			write_socket(p,
			    "Syntax: i3ctl chan <channel> <command>\n");
			return;
		}

		/* Next argument is channel name.. */

		if ((i = i3_find_channel(argv[2])) == (struct i3_channel *)NULL)
		{
			write_socket(p, "Unknown channel: %s\n", argv[2]);
			return;
		}

		/* Command */
		deltrail(argv[3]);

		if (!strcmp(argv[3], "tunein"))
		{
			if (i->listening == 1)
				write_socket(p,
				    "Already listening to channel %s.\n",
				    i->name);
			else
			{
				i3_tune_channel(i, 1);
				write_socket(p, "Channel %s tuned in.\n",
				    i->name);
			}
			return;
		}

		if (!strcmp(argv[3], "tuneout"))
		{
			if (i->listening != 1)
				write_socket(p,
				    "Channel %s is already tuned out.\n",
				    i->name);
			else
			{
				i3_tune_channel(i, 0);
				write_socket(p, "Channel %s tuned out.\n",
				    i->name);
			}
			return;
		}

		write_socket(p, "Unknown i3 channel command, %s.\n", argv[3]);
		return;
	}

	write_socket(p, "Syntax: Unknown argument to i3ctl.\n");
}
#endif /* IMUD3_SUPPORT */

void
f_killpp(struct user *p, int argc, char **argv)
{
	pid_t pp;

	if (!ISROOT(p))
	{
		write_socket(p, "Only "ROOT_USER" can do this.\n");
		return;
	}
	if ((pp = getppid()) == -1)
	{
		log_perror("getppid");
		write_socket(p, "Error retrieving ppid.\n");
		return;
	}
	if (pp == 1)
	{
		write_socket(p, "No parent found.\n");
			return;
	}
	if (kill(pp, SIGHUP) == -1)
	{
		log_perror("kill");
		write_socket(p, "Kill error.\n");
	}
	write_socket(p, "Ok.\n");
}

void
mkuser5(struct user *p, char *c)
{
	extern void send_valinfo(struct user *, char *);
	extern char *host_name, *host_ip;

	p->input_to = NULL_INPUT_TO;

	if (!strlen(c) || tolower(*c) != 'y')
	{
		write_socket(p, "No email sent.\n");
		pop_n_elems(&p->stack, 3);
		return;
	}

	send_email(p->rlname, (p->stack.sp - 1)->u.string, (char *)NULL,
	    "New account on " LOCAL_NAME, 1,
	    "A new account has been created for you on %s in the name of\n"
	    "\t%s\nThe password for this user has been set to: %s\n"
	    "Enjoy the %s experience at:\n\t%s %d (%s %d)\n\n"
	    "Regards,\n\n%s.\n\n", LOCAL_NAME, (p->stack.sp - 2)->u.string,
	    p->stack.sp->u.string, LOCAL_NAME, host_name, DEFAULT_PORT,
	    host_ip, DEFAULT_PORT, p->capname);

	/* Send the contents of F_VALINFO if it exists */
	send_valinfo(p, (p->stack.sp - 1)->u.string);

	write_socket(p, "Email submitted.\n");
	pop_n_elems(&p->stack, 3);
}

void
mkuser4(struct user *p, char *c)
{
	struct user *who;

	echo(p);

	if (strcmp(c, p->stack.sp->u.string))
	{
		write_socket(p, "\nPasswords didn't match.\n");
		pop_n_elems(&p->stack, 3);
		p->input_to = NULL_INPUT_TO;
		return;
	}

	init_user(who = create_user());
	/* Vital fields */
	COPY(who->rlname, (p->stack.sp - 2)->u.string, "mkuser rlname");
	COPY(who->capname, capitalise(who->rlname), "mkuser rlname");
	/* Use the password from the stack as crypted() calls
	 * random_password and will corrupt the copy in c */
	COPY(who->passwd, crypted(p->stack.sp->u.string), "mkuser passwd");
	COPY(who->email, (p->stack.sp - 1)->u.string, "mkuser email");

	/* Defaults.. */
	COPY(who->title, NEWBIE_TITLE, "title");
	COPY(who->prompt, PROMPT, "prompt");

	/* Validate 'em */
	COPY(who->validator, p->rlname, "mkuser validator");
	who->level = L_RESIDENT;

	/* Make them change the password and gender at login */
	who->saveflags |= U_FPC;
	who->gender = G_UNSET;

	save_user(who, 0, 1);

	log_file("validate", "%s (%s) created by %s", who->capname, who->email,
	    p->capname);
	write_socket(p, "\nUser created.\n");
	tfree_user(who);

	p->input_to = mkuser5;
	write_prompt(p, "Email user? (y/n) ");
}

void
mkuser3(struct user *p, char *c)
{
	int i;

	if (!strlen(c))
	{
		c = random_password(8);
		push_string(&p->stack, c);
		write_socket(p, "Password set to: %s\n", c);
		mkuser4(p, c);
		return;
	}
	else if ((i = secure_password(p, c, (char *)NULL)))
	{
		extern void insecure_passwd_text(struct user *, int);
		echo(p);
		p->input_to = NULL_INPUT_TO;
		write_socket(p, "\n");
		insecure_passwd_text(p, i);
		pop_n_elems(&p->stack, 2);
		return;
	}
	push_string(&p->stack, c);
	p->input_to = mkuser4;
	write_prompt(p, "\nRe-enter password: ");
}

void
mkuser2(struct user *p, char *c)
{
	if (!strlen(c) || !valid_email(c))
	{
		write_socket(p, "Invalid email address.\n");
		pop_stack(&p->stack);
		p->input_to = NULL_INPUT_TO;
		return;
	}
	noecho(p);
	write_prompt(p, "Enter a password for %s; press return for a "
	    "system generated password.\n: ", p->stack.sp->u.string);
	push_string(&p->stack, c);
	p->input_to = mkuser3;
}

void
f_mkuser(struct user *p, int argc, char **argv)
{
	char *name = lower_case(argv[1]);

	if (exist_user(name))
	{
		write_socket(p, "User %s already exists.\n", argv[1]);
		return;
	}
	if (strlen(name) > NAME_LEN)
	{
		write_socket(p, "The name '%s' is too long.\n", name);
		return;
	}
	if (offensive(name))
	{
		write_socket(p, "That name failed the offensivness filter "
		    "check.\n");
		return;
	}
	if (!valid_name(p, name, 0))
		return;

	CHECK_INPUT_TO(p)

	push_string(&p->stack, name);

	p->input_to = mkuser2;
	write_prompt(p, "Enter an email address for %s: ", name);
}

void
f_noidle(struct user *p, int argc, char **argv)
{
	struct user *who;

	if ((who = doa_start(p, argv[1])) == (struct user *)NULL)
	{
		write_socket(p, "No such user, %s.\n", argv[1]);
		return;
	}

	if (!access_check(p, who))
	{
		doa_end(who, 0);
		return;
	}

	if (who->level >= L_OVERSEER)
	{
		write_socket(p, "Overseers do not get idled out. "
		    "Flag will not be set.\n");
		if (who->saveflags & U_NO_IDLEOUT)
		{
			who->saveflags &= ~U_NO_IDLEOUT;
			doa_end(who, 1);
		}
		else
			doa_end(who, 0);
		return;
	}

	write_socket(p, "The no idle-out flag has been %s in %s.\n",
	    (who->saveflags ^= U_NO_IDLEOUT) & U_NO_IDLEOUT ? "set" : "unset",
	    who->name);

	if (IN_GAME(who))
		write_socket(who,
		    "Your no idle-out flag has been %s by %s.\n",
		    (who->saveflags & U_NO_IDLEOUT) ? "set" : "unset",
		    p->name);

	doa_end(who, 1);
}

void
f_nopurge(struct user *p, int argc, char **argv)
{
	struct user *who;

	if ((who = doa_start(p, argv[1])) == (struct user *)NULL)
	{
		write_socket(p, "No such user, %s.\n", argv[1]);
		return;
	}

	if (!access_check(p, who))
	{
		doa_end(who, 0);
		return;
	}

	write_socket(p, "The nopurge flag has been %s in %s.\n",
	    (who->saveflags ^= U_NO_PURGE) & U_NO_PURGE ? "set" : "unset",
	    who->name);

	if (IN_GAME(who))
		write_socket(who,
		    "Your no purge flag has been %s by %s.\n",
		    (who->saveflags & U_NO_PURGE) ? "set" : "unset",
		    p->name);

	doa_end(who, 1);
}

#ifdef PROFILING
void
f_profile(struct user *p, int argc, char **argv)
{
	int i;
	struct strbuf buf;
	extern struct feeling *feelings;
	struct feeling *f;

	init_strbuf(&buf, 0, "f_profile");

	for (i = 0; partial_commands[i].command != NULL; i++)
		if (partial_commands[i].calls)
			sadd_strbuf(&buf, "%-20s : %d\n",
			    partial_commands[i].command,
			    partial_commands[i].calls);
	for (i = 0; commands[i].command != NULL; i++)
		if (commands[i].calls)
			sadd_strbuf(&buf, "%-20s : %d\n", commands[i].command,
			    commands[i].calls);
	for (f = feelings; f != (struct feeling *)NULL; f = f->next)
		if (f->calls)
			sadd_strbuf(&buf, "%-20s : %d\n", f->name, f->calls);
	pop_strbuf(&buf);
	more_start(p, buf.str, NULL);
}
#endif /* PROFILING */

void
f_purge(struct user *p, int argc, char **argv)
{
	extern void purge_users(struct event *);

	if (!ISROOT(p))
	{
		write_socket(p, "Only "ROOT_USER" can do this.\n");
		return;
	}
	purge_users((struct event *)NULL);
	write_socket(p, "Done.\n");
}

void
f_reident(struct user *p, int argc, char **argv)
{
	struct user *who;

	if ((who = find_user_msg(p, argv[1])) == (struct user *)NULL)
		return;
	send_erq(ERQ_RESOLVE_NUMBER, "%s;\n", lookup_ip_number(who));
	send_erq(ERQ_IDENT, "%s;%d,%d\n", lookup_ip_number(who),
	    who->socket.lport, who->socket.rport);
	write_socket(p, "Identification request sent.\n");
}

void
f_reload(struct user *p, int argc, char **argv)
{
	struct room *r;

	if ((r = find_room(argv[1])) == (struct room *)NULL)
	{
		write_socket(p, "Room %s not found.\n", argv[1]);
		return;
	}
	reload_room(r);
	write_socket(p, "Room reloaded.\n");
}

#if defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT)
void
f_rmhost(struct user *p, int argc, char **argv)
{
	struct host **list;
#ifdef INETD_SUPPORT
	extern struct host *hosts;
#endif
#ifdef CDUDP_SUPPORT
	extern struct host *cdudp_hosts;
#endif
	extern int remove_host_by_name(struct host **, char *);

	if (argv[1][0] == '%')
	{
#ifdef CDUDP_SUPPORT
		list = &cdudp_hosts;
#else
		list = &hosts;
#endif
		argv[1]++;
	}
	else
#ifdef INETD_SUPPORT
		list = &hosts;
#else
		list = &cdudp_hosts;
#endif
	if (remove_host_by_name(list, argv[1]))
		write_socket(p, "Removed.\n");
	else
		write_socket(p, "Host not found.\n");
}
#endif /* INETD_SUPPORT || CDUDP_SUPPORT */

void
f_rmuser(struct user *p, int argc, char **argv)
{
	struct user *u;

	if (!iaccess_check(p, query_real_level(argv[1])))
		return;
	if ((u = find_user_absolute(p, argv[1])) != (struct user *)NULL)
	{
		write_socket(p, "User is logged on... disconnecting.\n");
		write_socket(u, "You have been disonnected by %s.\n",
		    p->name);
		notify_levelabu(p, p->level,
		    "[ %s has been disconnected by %s. ]", u->capname,
		    p->capname);
		save_user(u, 1, 0);
		disconnect_user(u, 0);
	}
	if (!exist_user(argv[1]))
	{
		write_socket(p, "User %s does not exist.\n",
		    capitalise(argv[1]));
		if (query_real_level(argv[1]) != -1)
		{
			write_socket(p, "User is in mastersave database!\n"
			    "Deleting...\n");
			change_user_level(argv[1], L_RESIDENT, p->rlname);
		}
		return;
	}
	delete_user(argv[1]);
	write_socket(p, "User %s deleted\n", capitalise(argv[1]));
	notify_levelabu(p, p->level, "[ User %s has been deleted by %s. ]",
	    capitalise(argv[1]), p->capname);
}

void
f_sreboot(struct user *p, int argc, char **argv)
{
	struct room *r, *s;
	struct user *q;

	if (argc > 1 && strcmp(argv[1], "all"))
	{
		write_socket(p, "Syntax: %s [all]\n", argv[0]);
		return;
	}
	log_file("syslog", "Soft reboot by %s", capitalise(p->rlname));
	write_all("\n\nSOFT REBOOT INITIATED .. (by %s)\n",
	    p->name);
	for (r = rooms->next; r != (struct room *)NULL; r = s)
	{
		s = r->next;
		if (*r->fname == '_' || argc > 1)
			destroy_room(r);
	}
	r = get_entrance_room();
	for (q = users->next; q != (struct user *)NULL; q = q->next)
		if (IN_GAME(q) && q->super == rooms)
			move_user(q, r);

	write_all("\nDone\n");
}

void
f_supersnoop(struct user *p, int argc, char **argv)
{
	struct user *u;
	char *user;
	int remove;
	extern int check_supersnooped(char *);

	if (!ISROOT(p))
	{
		write_socket(p, "Only "ROOT_USER" may use this command.\n");
		return;
	}

	deltrail(argv[1]);

	if ((remove = !strcmp(argv[1], "-r")))
	{
		if (argc < 3)
		{
			write_socket(p, "Syntax: supersnoop -r <user>.\n");
			return;
		}
		user = argv[2];
	}
	else
		user = argv[1];

	u = find_user_absolute(p, user = lower_case(user));

	if (!remove)
	{
		if (!exist_line(F_SNOOPED, user))
		{
			add_line(F_SNOOPED, "%s\n", user);
			write_socket(p, "Added user to snooped file.\n");
		}
		if (u != (struct user *)NULL)
		{
			if (u->snoop_fd != -1)
			{
				write_socket(p, "User %s is already "
				    "super snooped.\n", u->name);
				return;
			}
			u->snoop_fd = check_supersnooped(u->rlname);
			write_socket(p, "Supersnoop started on %s.\n",
			   capitalise(user));
		}
		return;
	}
	if (exist_line(F_SNOOPED, user))
	{
		remove_line(F_SNOOPED, user);
		write_socket(p, "Removed user from snooped file.\n");
	}
	if (u != (struct user *)NULL)
	{
		if (u->snoop_fd == -1)
		{
			write_socket(p, "User is not snooped.\n");
			return;
		}
		while (close(u->snoop_fd) == -1 && errno == EINTR)
			;
		u->snoop_fd = -1;
		write_socket(p, "Supersnoop stopped on %s.\n",
		    capitalise(user));
	}
}

