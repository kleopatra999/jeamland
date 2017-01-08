/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	cmd.visitor.c
 * Function:	Visitor commands
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
#include <sys/utsname.h>
#include <sys/time.h>
#include <arpa/telnet.h>
#include <errno.h>
#include <sys/param.h>
#include <netinet/in.h>
#include "jeamland.h"

extern struct user *users;
extern struct room *rooms;
extern int num_rooms, peak_rooms, peak_users;
extern int loads, logons, rooms_created;
extern time_t start_time, current_time;
extern float event_av, command_av;
extern struct termcap_entry termcap[];
extern struct command commands[];
extern int errno;

/*
 * VISITOR commands....
 */

void
f_ansi(struct user *p, int argc, char **argv)
{
	int i;

	if (argc < 2)
	{
		write_socket(p, "No terminal type specified.\n");
		write_socket(p, "Known terminals are: \n");
		for (i = 0; termcap[i].name; i++)
			write_socket(p, "	%s\n", termcap[i].name);
		write_socket(p, "If you are unsure, choose vt100.\n");
		if (p->saveflags & U_ANSI)
			write_socket(p, "You have currently got your terminal"
			    " type set to '%s'\n", termcap[p->terminal].name);
		return;	
	}
	if (!strcmp(argv[1], "off"))
	{
		if (p->saveflags & U_ANSI)
		{
			p->saveflags ^= U_ANSI;
			write_socket(p, "Ansi turned off.\n");
		}
		else
			write_socket(p, "Ansi is not turned on.\n");
		return;
	}
	for (i = 0; termcap[i].name; i++)
	{
		if (!strcmp(termcap[i].name, argv[1]))
		{
			p->terminal = i;
			p->saveflags |= U_ANSI;
			write_socket(p, "Ansi turned on, terminal: %s\n",
			    argv[1]);
			return;
		}
	}
	write_socket(p, "No such terminal type.\n");
}

void
f_assist(struct user *p, int argc, char **argv)
{
	if (p->level > L_RESIDENT)
	{
		struct user *who;

		if (argc == 1)
		{
			write_socket(p, "Assist who ?\n");
			return;
		}
		if ((who = find_user(p, argv[1])) == (struct user *)NULL)
		{
			write_socket(p, "User %s not found.\n", argv[1]);
			return;
		}
		if (who->level > L_RESIDENT)
		{
			write_socket(p, "%s is able to take care of %s.\n",
			    who->capname, query_gender(who->gender, G_SELF));
			return;
		}
		bold(who);
		write_socket(who, "%s is a frequent user.\n"
		    "%s is able and willing to help you and can also give "
		    "you a residency.\n"
		    "If you require help, feel free to send %s a message\n"
		    "using the 'tell' command; for example.\n"
		    "\ttell %s hello\n"
		    "There is also a short form of this:\n"
		    "\t>%s hello\n"
		    "Also, the name can usually be abbreviated to just the "
		    "first few letters.\n", p->capname,
		    lower_case(level_name(p->level, 0)),
		    capitalise(query_gender(p->gender, G_PRONOUN)),
		    query_gender(p->gender, G_OBJECTIVE),
		    p->capname, p->capname);
		reset(who);
		notify_levelabu(p, p->level, "[ %s is assisting %s. ]\n",
		    p->capname, who->capname);
		write_socket(p, "Ok.\n");
		return;
	}
	write_socket(p, "Message sent to any admin logged on.\n");
	notify_level(L_CITIZEN, "[ %c%cNewbie needs help! (%s) ]\n", 7, 7,
	    p->capname);
}

void
f_brief(struct user *p, int argc, char **argv)
{
	write_socket(p, "Brief mode turned %s.\n",
	    (p->saveflags ^= U_BRIEF_MODE) & U_BRIEF_MODE ? "on" : "off");
}

void
f_clear(struct user *p, int argc, char **argv)
{
	if (!(p->saveflags & U_ANSI))
	{
		write_socket(p,
		    "You must have ansi turned on for this command to work.\n");
		return;
	}
	write_socket(p, "%s", termcap[p->terminal].cls);
}

void
f_commands(struct user *p, int argc, char **argv)
{
	int i, num = 0, last;
	struct strbuf text;
	int lev = -1;

	init_strbuf(&text, 0, "f_commands text");
	add_strbuf(&text, "Available commands:");

	if (argc > 1)
		if ((lev = level_number(argv[1])) == -1)
		{
			write_socket(p, "No such level, %s.\n", argv[1]);
			return;
		}

	last = -1;
	for (i = 0; commands[i].command != NULL; i++)
	{
		if (p->level < commands[i].level)
			break;
		if (lev != -1 && commands[i].level != lev)
			continue;
		if (commands[i].level > last)
		{
			num = 0;
			last = commands[i].level;
			cadd_strbuf(&text, '\n');
			add_strbuf(&text, level_name(last, 1));
			cadd_strbuf(&text, ' ');
		}
		if (num)
			add_strbuf(&text, ", ");
		add_strbuf(&text, commands[i].command);
		num++;
	}
	if (!text.offset)
		write_socket(p, "No commands found.\n");
	else
		write_socket(p, "%s\n", text.str);
	free_strbuf(&text);
	/*pop_strbuf(&text);*/
	/*more_start(p, text.str, NULL);*/
}

void
f_converse(struct user *p, int argc, char **argv)
{
	write_socket(p, "Converse mode turned %s.\n",
	    (p->saveflags ^= U_CONVERSE_MODE) & U_CONVERSE_MODE ?
	    "on.\nTo execute commands, prefix them with a '.' character" :
	    "off");
}

void
f_date(struct user *p, int argc, char **argv)
{
	write_socket(p, "Local time:\t%s", ctime(&current_time));
	write_socket(p, "Start time:\t%s", ctime(&start_time));
	write_socket(p, "Up time:\t%s.\n", conv_time(current_time -
	    start_time));
	write_socket(p, "Logons:\t\t%d\n", logons);
	write_socket(p, "Rooms created:\t%d\n", rooms_created);
	write_socket(p, "Loads:\t\t%d\n", loads);
}

void
dolog2(struct user *p, int i)
{
	if (i != EDX_NORMAL)
	{
		pop_stack(&p->stack);
		return;
	}
	log_file((p->stack.sp - 1)->u.string, "(%s)\n%s", p->capname,
	    p->stack.sp->u.string);
	notify_levelabu(p, p->level >= L_WARDEN ? p->level : L_WARDEN,
	    "[ %s has made an entry in the %s log. ]\n",
	    p->capname, (p->stack.sp - 1)->u.string);
	pop_n_elems(&p->stack, 2);
	write_socket(p, "Thankyou, your comment has been noted.\n");
}

void
f_dolog(struct user *p, int argc, char **argv)
{
	push_string(&p->stack, argv[0]);
	if (argc == 1)
	{
		write_socket(p, "Please enter your comment below.\n");
		ed_start(p, dolog2, 10, 0);
		return;
	}
	push_string(&p->stack, argv[1]);
	dolog2(p, EDX_NORMAL);
}

void
f_echoback(struct user *p, int argc, char **argv)
{
	write_socket(p, "Echoback mode turned %s.\n",
	    (p->saveflags ^= U_NO_ECHOBACK) & U_NO_ECHOBACK ? "off" : "on");
}

void
f_emote(struct user *p, int argc, char **argv)
{
	if (*argv[0] == 'm' || *argv[0] == ';')
	{
		int i;

		write_roomabu(p, "%s%s %s\n", p->name,
		    (i = (tolower(p->name[strlen(p->name) - 1]) == 's')) ? "'" :
		    "'s", argv[1]);
		if (!(p->saveflags & U_NO_ECHOBACK))
			write_socket(p, "%s%s %s\n", p->name,
			    i ? "'" : "'s", argv[1]);
		else
			write_socket(p, "Ok.\n");
	}
	else
	{
		write_roomabu(p, "%s %s\n", p->name, argv[1]);
		if (!(p->saveflags & U_NO_ECHOBACK))
			write_socket(p, "%s %s\n", p->name,
			    argv[1]);
		else
			write_socket(p, "Ok.\n");
	}
}

void f_headers(struct user *, int, char **);
extern void f_finger(struct user *, int, char **);

void
f_examine(struct user *p, int argc, char **argv)
{
	struct user *who;

	if (argc < 2)
	{
		write_socket(p, "%s [at] what ?\n", capitalise(argv[0]));
		return;
	}

	if (!strcmp(argv[1], "board"))
	{
		f_headers(p, argc, argv);
		return;
	}
	if ((who = with_user(p, argv[1])) == (struct user *)NULL)
	{
		write_socket(p, "User %s is not here.\n",
		    capitalise(argv[1]));
		return;
	}
	argv[1] = who->rlname;
	f_finger(p, argc, argv);
}

void
f_feelings(struct user *p, int argc, char **argv)
{
	extern void load_feelings(void);
	struct feeling *f;
	int flag = 0, last = -1;
	struct strbuf text;
	extern struct feeling *feelings;

	if (argc > 1)
	{
		if (p->level > L_CONSUL && !strcmp(argv[1], "reload"))
		{
			write_socket(p, "Reloading feeling database.\n");
			load_feelings();
			write_socket(p, "Done.\n");
			return;
		}
		else if ((f = exist_feeling(argv[1])) !=
		    (struct feeling *)NULL)
		{
			write_socket(p, "Name:   %s\n", f->name);
			write_socket(p, "Adverb: %s\n",
			    f->adverb != (char *)NULL ? f->adverb : "<None>");
			write_socket(p, "Prep:   %s\n",
			    f->prep != (char *)NULL ? f->prep : "<None>");
			write_socket(p, "Type:   %d\n", f->type);
			write_socket(p, "Noverb: %d\n", f->no_verb);
#ifdef PROFILING
			write_socket(p, "Times used: %d\n", f->calls);
#endif
			return;
		}
		write_socket(p, "No such feeling.\n");
		return;
	}
	init_strbuf(&text, 0, "f_feelings");
	for (f = feelings; f != (struct feeling *)NULL; f = f->next)
	{
		if (f->type != last)
		{
			if (last != -1)
				add_strbuf(&text, "\n\n");
			switch(last = f->type)
			{
			    case S_STD:
				add_strbuf(&text,
				    "<feeling> [adverb] [user]\n"
				    "-------------------------\n");
				break;
			    case S_STD2:
				add_strbuf(&text,
				    "<feeling> <user> [adverb]\n"
				    "-------------------------\n");
				break;
			    case S_NOARG:
				add_strbuf(&text,
				    "<feeling>\n"
				    "---------\n");
				break;
			    case S_NOTARG:
				add_strbuf(&text,
				    "<feeling> [adverb]\n"
				    "------------------\n");
				break;
			    case S_TARG:
				add_strbuf(&text,
				    "<feeling> <user>\n"
				    "----------------\n");
				break;
			    case S_OPT_TARG:
				add_strbuf(&text,
				    "<feeling> [user]\n"
				    "---------\n");
				break;
			    default:
				add_strbuf(&text,
				    "Not yet implemented\n"
				    "-------------------\n");
				break;
			}
			flag = 0;
		}
		if (flag)
			add_strbuf(&text, ", ");
		add_strbuf(&text, f->name);
		flag++;
	}
	cadd_strbuf(&text, '\n');
	pop_strbuf(&text);
	more_start(p, text.str, NULL);
}

void
f_fixscreen(struct user *p, int argc, char **argv)
{
	/* This should fix most screens... termcap it later ? */
	write_socket(p, "\033c");
}

#ifdef SUPER_SNOOP

void
harassment3(struct event *ev)
{
	struct user *who;
	extern int check_supersnooped(char *);

	if ((who = find_user((struct user *)NULL, ev->stack.sp->u.string)) ==
	    (struct user *)NULL)
		return;
	notify_level(L_OVERSEER, "[ Harass log on %s closed. ]\n",
	    who->capname);
	if (who->snoop_fd != -1)
		while (close(who->snoop_fd) == -1 && errno == EINTR)
			;
	who->snoop_fd = check_supersnooped(who->rlname);
}

void
harrasment2(struct user *p, char *c)
{
	struct user *who;
	struct event *ev;
	char fname[MAXPATHLEN + 1];

	p->input_to = NULL_INPUT_TO;

	if (tolower(*c) != 'y')
	{
		write_socket(p, "Harass log cancelled.\n");
		pop_stack(&p->stack);
		return;
	}

        if ((who = find_user(p, p->stack.sp->u.string)) == (struct user *)NULL)
        {
                write_socket(p, "User %s has vanished.\n",
		    p->stack.sp->u.string);
		pop_stack(&p->stack);
                return;
        }
	pop_stack(&p->stack);
        if (who == p)
        {
                write_socket(p, "Harassed by yourself ?!?\n");
                return;
        }
	write_socket(p, "IMPORTANT: You have started a harass log on %s\n",
	    who->name);
	log_file("secure/harass", "%s (%s@%s) on %s (%s@%s)",
	    p->capname, p->uname != (char *)NULL ? p->uname :
	    "unknown", lookup_ip_name(p), who->capname,
	    who->uname != (char *)NULL ? who->uname : "unknown",
	    lookup_ip_name(who));
	notify_level(L_OVERSEER, "[ %s has started a harass log on %s. ]\n",
	    p->capname, who->capname);

	if (who->snoop_fd == -1)
	{
		sprintf(fname, F_SNOOPS "%s.harass", who->rlname);
		if ((who->snoop_fd = open(fname, O_CREAT | O_WRONLY |
		    O_APPEND, 0600)) == -1)
		{
			log_perror("harass fileopen");
			return;
		}
	}
	sprintf(fname, "#### Harass log started by %s at %s",
	    p->capname, ctime(&current_time));
	write(who->snoop_fd, fname, strlen(fname));
	ev = create_event();
	push_string(&ev->stack, who->rlname);
	add_event(ev, harassment3, HARASS_LOG_TIME, "harass");
}

void
f_harassment(struct user *p, int argc, char **argv)
{
	struct user *who;

	if ((who = find_user(p, argv[1])) == (struct user *)NULL)
	{
		write_socket(p, "User %s not found.\n", argv[1]);
		return;
	}
	if (who == p)
	{
		write_socket(p, "Harassed by yourself ?!?\n");
		return;
	}
	CHECK_INPUT_TO(p);
	write_prompt(p, "IMPORTANT! MISUSE OF THE 'HARRASMENT' COMMAND IS A "
	    "BANISHMENT OFFENCE!\nAre you sure you want to do this? ");
	push_string(&p->stack, argv[1]);
	p->input_to = harrasment2;
}
#endif /* SUPER_SNOOP */

void
f_headers(struct user *p, int argc, char **argv)
{
	char *msg;

	if (!(p->super->flags & R_BOARD))
	{
		write_socket(p, "No board in this room.\n");
		return;
	}
	if (!CAN_READ_BOARD(p, p->super->board))
	{
		write_socket(p, "You may not read this board.\n");
		return;
	}
	if ((msg = get_headers(p->super->board, 0, 0)) == (char *)NULL)
		write_socket(p, "No notes.\n");
	else
		more_start(p, msg, NULL);
}

void
f_help(struct user *p, int argc, char **argv)
{
	char buf[MAXPATHLEN + 1];
	int i;

	if (argc == 1)
		argv[1] = F_INDEX;
	if (strchr(argv[1], '/') != (char *)NULL)
	{
		/* Naughty, naughty.. */
		notify_level(L_CONSUL,
		    "[ %s tried to read outside the lib (%s) ]\n",
		    p->capname, argv[1]);
		log_file("illegal", "%s: help: %s", p->capname, argv[1]);
		write_socket(p, "No such topic.\n");
		return;
	}
	for (i = p->level + 1; i--; )
	{
		sprintf(buf, "%s/%s", lower_case(level_name(i, 0)), argv[1]);
		if (dump_file(p, "help", buf, DUMP_MORE))
			return;
	}
	if (!dump_file(p, "help", argv[1], DUMP_MORE))
		write_socket(p, "No such topic.\n");
}

void
f_history(struct user *p, int argc, char **argv)
{
	int i, j;
	struct user *ptr = p;

	if (p->level > L_CONSUL && argc > 1)
	{
		if ((ptr = find_user(p, argv[1])) == (struct user *)NULL)
		{
			write_socket(p, "User %s not found.\n",
			    capitalise(argv[1]));
			return;
		}
		if (!access_check(p, ptr))
			return;
	}

	if (ptr->history_ptr == 1)
	{
		write_socket(p, "No history.\n");
		return;
	}

	if ((i = ptr->history_ptr - HISTORY_SIZE) < 1)
		i = 1;
	for (j = i; j < ptr->history_ptr; j++)
		write_socket(p, "[%4d] %s\n", j, ptr->history->items[j %
		    HISTORY_SIZE].u.string);
}

void
f_look(struct user *p, int argc, char **argv)
{
	int num = 0;
	struct user *u;
	struct object *ob;
	struct exit *e;
	struct strbuf tmp;

	if (p->super == (struct room *)NULL)
	{
		write_socket(p, "No environment!\n");
		return;
	}
	if (argc > 1)
	{
		if (!strncmp(argv[1], "at ", 3))
		{
			if (argc < 3)
			{
				write_socket(p, "Look at what ?\n");
				return;
			}
			argv[1] += 3;
		}
		f_examine(p, argc, argv);
		return;
	}

	if (p->level >= A_SEE_FNAME)
		write_socket(p, "[%s#%p - %s]\n", p->super->fname,
		    (void *)p->super, p->super->name);
	else if (!strcmp(p->super->owner, p->rlname))
		write_socket(p, "[%s - %s]\n", p->super->fname,
		    p->super->name);
	if ((p->saveflags & U_BRIEF_MODE) && !argc)
		write_socket(p, "%s\n", p->super->name);
	else
		write_socket(p, "%s", p->super->long_desc);
	if (p->super->flags & R_BOARD)
	{
		num = count_messages(p->super->board);
		write_socket(p,
		    "\nThere is a board here containing %d note%s.\n",
		    num, num == 1 ? "" : "s");
	}
	write_socket(p, "Other users in room: ");
	init_strbuf(&tmp, 0, "f_look users");
	for (num = 0, ob = p->super->ob->contains;
	    ob != (struct object *)NULL; ob = ob->next_contains)
	{
		if (ob->type != T_USER || ob->m.user == p)
			continue;
		u = ob->m.user;

/*
		if (!CAN_SEE(p, u))
			continue;
*/

		if (num)
			add_strbuf(&tmp, ", ");
		if (u->saveflags & U_INVIS)
		{
			cadd_strbuf(&tmp, '(');
			add_strbuf(&tmp, u->capname);
			cadd_strbuf(&tmp, ')');
		}
		else
			add_strbuf(&tmp, u->name);
		if (u->flags & U_AFK)
			if (u->stack.sp->type == T_STRING)
				sadd_strbuf(&tmp, " [AFK: %s ]",
				    u->stack.sp->u.string);
			else
				add_strbuf(&tmp, " [AFK] ");
		if (u->flags & U_INED)
			add_strbuf(&tmp, " [EDITING]");
		num++;
	}
	if (!num)
		write_socket(p, "None.\n");
	else
		write_socket(p, "%s\n", tmp.str);
	reinit_strbuf(&tmp);
	for (num = 0, e = p->super->exit; e != (struct exit *)NULL; e = e->next)
	{
		if (!num)
		{
			num++;
			add_strbuf(&tmp, "Exits: ");
		}
		else
			add_strbuf(&tmp, ", ");
		add_strbuf(&tmp, e->name);
	}
	if (num)
		write_socket(p, "%s\n", tmp.str);
	else
		write_socket(p, "\n");
	free_strbuf(&tmp);
}

void
f_man(struct user *p, int argc, char **argv)
{
	if (argc == 1)
		argv[1] = F_INDEX;
	if (strchr(argv[1], '/') != (char *)NULL)
	{
		/* Naughty, naughty.. */
		notify_level(L_CONSUL,
		    "[ %s tried to read outside the lib (%s) ]\n",
		    p->capname, argv[1]);
		write_socket(p, "No such topic.\n");
		return;
	}
	if (!dump_file(p, "help/man", argv[1], DUMP_MORE))
		write_socket(p, "No such topic.\n");
}

void
f_morelen(struct user *p, int argc, char **argv)
{
	int c;

	if (argc < 2)
	{
		write_socket(p, "Your current morelen is %d.\n", p->morelen);
		return;
	}

	if ((c = atoi(argv[1])) < 10)
	{
		write_socket(p, "Silly more length!\n");
		return;
	}
	p->morelen = c;
	write_socket(p, "Morelen changed to %d.\n", c);
}

void
f_motd(struct user *p, int argc, char **argv)
{
	dump_file(p, "etc", F_MOTD, DUMP_HEAD);
}

void
f_nl(struct user *p, int argc, char **argv)
{
	write_socket(p, "Extra newlines turned %s.\n",
	    (p->saveflags ^= U_EXTRA_NL) & U_EXTRA_NL ? "on" : "off");
}

void
f_quiet(struct user *p, int argc, char **argv)
{
	char *cmd;

	cmd = string_copy(argv[1], "quiet command");
	p->flags |= U_QUIET;
	parse_command(p, &cmd);
	p->flags &= ~U_QUIET;
	xfree(cmd);
}

void
f_quit(struct user *p, int argc, char **argv)
{
	if (p->flags & U_INHIBIT_QUIT)
	{
		write_socket(p, "The quit command can not be used here.\n");
		return;
	}

	write_socket(p, "Goodbye.\n");
	if (p->level > L_VISITOR && *argv[0] != 'Q')
	{
		write_socket(p, "Saving user %s.\n", p->capname);
		if (!save_user(p, 1, 0))
		{
			write_socket(p, "Could not save you!\n");
			write_socket(p, 
			    "This probably means that your email address\n");
			write_socket(p, "or your password is not set.\n");
			write_socket(p, 
			    "Use 'Quit' if you want to quit anyway.\n");
			return;
		}
	}
	save_user(p, 1, 0);
#ifdef SHUTDOWN_ALIAS
	if (find_alias(p->alias, SHUTDOWN_ALIAS) != (struct alias *)NULL)
	{
		char *cmd = string_copy(SHUTDOWN_ALIAS, "quit logout alias");
		p->flags |= U_INHIBIT_QUIT;
		reset_eval_depth();
		parse_command(p, &cmd);
		p->flags &= ~U_INHIBIT_QUIT;
		xfree(cmd);
	}
#endif
	notify_levelabu(p, SEE_LOGIN(p) ? L_VISITOR : p->level,
	    "[ %s [%s] has disconnected. ]\n",
	    p->capname, capfirst(level_name(p->level, 0)));
	write_roomabu(p, "%s has disconnected.\n", p->name);
	dump_file(p, "etc", F_LOGOUT, DUMP_CAT);
	p->flags |= U_SOCK_QUITTING;
}

void
f_read(struct user *p, int argc, char **argv)
{
	extern void show_message(struct user *, struct message *m,
	    void (*)(struct user *));
	int num;
	struct message *m;

	if (!(p->super->flags & R_BOARD))
	{
		write_socket(p, "There is no board in this room.\n");
		return;
	}
	if (!CAN_READ_BOARD(p, p->super->board))
	{
		write_socket(p, "You may not read this board.\n");
		return;
	}
	if (!(num = atoi(argv[1])))
	{
		write_socket(p, "You must supply the note number.\n");
		return;
	}
	if ((m = find_message(p->super->board, num)) == (struct message *)NULL)
	{
		write_socket(p, "No such note, %s\n", argv[1]);
		return;
	}
	show_message(p, m, NULL);
}

void f_tell(struct user *, int, char **);

void
reply2(struct user *p, char *c)
{
	char *argv[3];

	argv[1] = p->stack.sp->u.string;
	argv[2] = c;
	f_tell(p, 3, argv);
	pop_stack(&p->stack);
	p->input_to = NULL_INPUT_TO;
}

void
f_reply(struct user *p, int argc, char **argv)
{
	if (p->reply_to == (char *)NULL)
	{
		write_socket(p,
		    "You can only reply if someone told you something.\n");
		return;
	}
	if (argc == 1)
	{
		CHECK_INPUT_TO(p);
		push_string(&p->stack, p->reply_to);
		p->input_to = reply2;
		write_prompt(p, ": ");
		return;
	}
	argv[2] = argv[1];
	argv[1] = p->reply_to;
	f_tell(p, argc + 1, argv);
}

void
f_say(struct user *p, int argc, char **argv)
{
	write_roomabu(p, SAY_FORMAT, p->name, argv[1]);
	if (!(p->saveflags & U_NO_ECHOBACK))
		write_socket(p, YSAY_FORMAT, argv[1]);
	else
		write_socket(p, "Ok.\n");
}

void
f_screenwidth(struct user *p, int argc, char **argv)
{
	int c;

	if (argc < 2)
	{
		write_socket(p, "Your current screenwidth is %d.\n",
		    p->screenwidth);
		return;
	}

	if ((c = atoi(argv[1])) < 10)
	{
		write_socket(p, "Silly screenwidth!\n");
		return;
	}
	p->screenwidth = c;
	write_socket(p, "Screenwidth changed to %d.\n", c);
}

void
su2(struct user *p, char *c)
{
	char *pass, *name, *uname;
	struct user *u;
	extern void login_user(struct user *, int);

	echo(p);
	p->input_to = NULL_INPUT_TO;
	if (ISROOT(p))	/* No password required... */
		;
	else if (!strlen(c))
	{
		write_socket(p, "\n");
		pop_stack(&p->stack);
		return;
	}
	else if ((pass = query_password(p->stack.sp->u.string)) ==
	    (char *)NULL)
	{
		write_socket(p, "\nUser has disappeared.\n");
		pop_stack(&p->stack);
		return;
	}
	else if ((u = find_user(p, p->stack.sp->u.string)) !=
	    (struct user *)NULL && !strcmp(u->rlname, p->stack.sp->u.string)
	    && !ISROOT(p))
	{
		write_socket(p, "%s is already logged on.\n", u->capname);
		log_file("secure/su", "# %s -> %s", p->capname, u->capname);
		notify_levelabu(p, L_OVERSEER, "[ SU: # %s -> %s ]\n",
		    p->capname, u->capname);
		pop_stack(&p->stack);
		return;
	}
	else if (strcmp(pass, crypt(c, pass)))
	{
		write_socket(p, "\nPassword incorrect.\n");
		log_file("secure/su", "- %s -> %s", p->capname,
		    capitalise(p->stack.sp->u.string));
		notify_levelabu(p, L_OVERSEER, "[ SU: - %s -> %s ]\n",
		    p->capname, capitalise(p->stack.sp->u.string));
		pop_stack(&p->stack);
		return;
	}
	/* Sigh.. pull up a dead copy. */
	if ((u = dead_copy(p->stack.sp->u.string)) != (struct user *)NULL)
	{
		if (!login_allowed(p, u))
		{
			pop_stack(&p->stack);
			log_file("secure/su", "~ %s -> %s", p->capname,
			    u->capname);
			notify_levelabu(p, L_OVERSEER, "[ SU: ~ %s -> %s ]\n",
			    p->capname, u->capname);
			tfree_user(u);
			return;
		}
		tfree_user(u);
	}
	log_file("secure/su", "+ %s -> %s", p->capname,
	    capitalise(p->stack.sp->u.string));
	notify_levelabu(p, L_OVERSEER, "[ SU: + %s -> %s ]\n",
	    p->capname, capitalise(p->stack.sp->u.string));
	name = lower_case(p->stack.sp->u.string);
	pop_stack(&p->stack);
	save_user(p, 1, 0);

	/* Store the 'uname' manually..... *ugh* */
	if ((uname = p->uname) != (char *)NULL)
		p->uname = (char *)NULL;
	free_user(p);
	if (!restore_user(p, name))
	{
		write_socket(p, "\nSomething went wrong!.\n");
		log_file("error", "Something went wrong in su2.");
		p->flags |= U_SOCK_CLOSING;
		FREE(uname);
		return;
	}
	FREE(p->uname);
	p->uname = uname;

	login_user(p, 1);
	write_socket(p, "\n");
}

void
f_su(struct user *p, int argc, char **argv)
{
	struct user *u;

	if (p->sudo)
	{
		write_socket(p, "You may not sudo su!\n");
		return ;
	}

	CHECK_INPUT_TO(p);

	if (!exist_user(argv[1]))
	{
		write_socket(p, "User %s does not exist.\n",
		    capitalise(argv[1]));
		log_file("secure/su", "$ %s -> %s", p->capname,
		    capitalise(argv[1]));
		notify_levelabu(p, L_OVERSEER, "[ SU: $ %s -> %s ]\n",
		    p->capname, capitalise(argv[1]));
		return;
	}
	if ((u = find_user(p, argv[1])) != (struct user *)NULL &&
	    !strcmp(u->rlname, argv[1]) && !ISROOT(p))
	{
		write_socket(p, "%s is already logged on.\n", u->capname);
		log_file("secure/su", "# %s -> %s", p->capname, u->capname);
		notify_levelabu(p, L_OVERSEER, "[ SU: # %s -> %s ]\n",
		    p->capname, u->capname);
		return;
	}
	push_string(&p->stack, argv[1]);
	/* Root doesn't need to give a password */
	if (ISROOT(p))
	{
		su2(p, (char *)NULL);
		return;
	}
	p->input_to = su2;
	noecho(p);
	write_prompt(p, "Enter password: ");
}

void
f_tell(struct user *p, int argc, char **argv)
{
	struct user *who;
	int i, flag, inetd_told;
	struct strbuf told, reply;
	struct vecbuf to_tell;
	struct vector *vec;
#ifdef UDP_SUPPORT
	extern void inetd_tell(struct user *, char *, char *);
#endif

	init_vecbuf(&to_tell, 0, "f_tell to_tell");
	expand_user_list(p, argv[1], &to_tell, 1);
	vec = vecbuf_vector(&to_tell);

	init_strbuf(&told, 0, "f_tell told");
	init_strbuf(&reply, 0, "f_tell reply");

	for (i = vec->size; i--; )
	{
		int j;
		reinit_strbuf(&told);
		reinit_strbuf(&reply);

		if (vec->items[i].type == T_STRING)
		{
#ifdef UDP_SUPPORT
			inetd_tell(p, vec->items[i].u.string, argv[2]);
#endif
			continue;
		}

		if (vec->items[i].type != T_POINTER)
			continue;

		for (flag = 0, j = vec->size; j--; )
		{
			if (j == i)
				continue;

			if (vec->items[j].type != T_STRING &&
			    vec->items[j].type != T_POINTER)
				continue;

			if (flag)
			{
				add_strbuf(&told, ", ");
				cadd_strbuf(&reply, ',');
			}
			else
				flag = 1;
			if (vec->items[j].type == T_STRING)
			{
				add_strbuf(&told, vec->items[j].u.string);
				add_strbuf(&reply, vec->items[j].u.string);
			}
			else
			{
				add_strbuf(&told,
				    ((struct user *)vec->items[j].u.pointer)->
				    name);
				add_strbuf(&reply,
				    ((struct user *)vec->items[j].u.pointer)->
				    rlname);
			}
		}
		if (flag)
		{
			add_strbuf(&told, " and you");
			sadd_strbuf(&reply, ",%s", p->rlname);
		}
		else
		{
			add_strbuf(&told, "you");
			add_strbuf(&reply, p->rlname);
		}

		who = (struct user *)vec->items[i].u.pointer;

		bold(who);
		write_socket(who, "%s tells %s: %s\n", p->name,
		    told.str, argv[2]);
		reset(who);
		COPY(who->reply_to, reply.str, "reply");
	}

	reinit_strbuf(&told);
	inetd_told = 0;
	for (flag = 0, i = vec->size; i--; )
	{
		if (vec->items[i].type == T_STRING)
		{
			inetd_told++;
			continue;
		}
		if (vec->items[i].type != T_POINTER)
			continue;
		if (flag)
			if (!i)
				add_strbuf(&told, " and ");
			else
				add_strbuf(&told, ", ");
		else
			flag = 1;
		add_strbuf(&told,
		    ((struct user *)vec->items[i].u.pointer)->name);
	}

	if (told.offset)
	{
		if (!(p->saveflags & U_NO_ECHOBACK))
			write_socket(p, "You tell %s: %s\n", told.str, argv[2]);
		else
			write_socket(p, "Ok.\n");
	}
	else if (!inetd_told)
		write_socket(p, "Nobody found to tell.\n");
	
	free_strbuf(&told);
	free_strbuf(&reply);
	free_vector(vec);
}

void
f_to(struct user *p, int argc, char **argv)
{
	struct user *who;

	if ((who = with_user(p, argv[1])) == (struct user *)NULL)
	{
		write_socket(p, "%s is not here.\n", capitalise(argv[1]));
		return;
	}
	write_roomabu(who, "%s says to %s, '%s'\n", p->name, 
	    who == p ? query_gender(p->gender, G_SELF) : who->name, argv[2]);
	bold(who);
	write_socket(who, "%s says to you, '%s'\n", p->name, argv[2]);
	reset(who);
}

void
f_tush(struct user *p, int argc, char **argv)
{
	write_socket(p, "TUsh prompt compatibility turned %s.\n",
            (p->saveflags ^= U_TUSH) & U_TUSH ? "on" : "off");
	if (p->saveflags & U_TUSH)
	{
		write_socket(p, "%c%c%c", IAC, WILL, TELOPT_EOR);
		p->telnet.expect |= TN_EXPECT_EOR;
	}
}

void
f_version(struct user *p, int argc, char **argv)
{
	extern struct utsname system_uname;

	write_socket(p, "Running Jeamland Talker v.%s\n", VERSION);
	write_socket(p, "(c) Andy Fiddaman 1994-96.\n");
	if (argc)
		write_socket(p, "System: %s %s %s (%s)\n",
		    system_uname.sysname, system_uname.release,
		    system_uname.version, system_uname.machine);
}


#define UNKNOWN_WHO_OPT do \
			{ \
				free_strbuf(&mt); \
				write_socket(p, "Unknown option: '%c'\n", \
				    argv[1][1]); \
				return; \
			} while (0)
void
f_who(struct user *p, int argc, char **argv)
{
	extern void inetd_who(struct user *, char *);
	struct user *ptr;
	char *hptr;
	char tmp[NAME_LEN + 2];
	struct strbuf mt;
	unsigned int maxlen = 0;
	int num_users;

	if (argc < 2)
		argv[1] = "--";
#ifdef UDP_SUPPORT
	else if (argv[1][0] == '@')
	{
		inetd_who(p, argv[1]);
		return;
	}
#endif
	else if (argv[1][0] != '-' || argv[1][1] == '\0')
	{
		write_socket(p, "Syntax: who [-<option> | -help]\n");
		return;
	}

	num_users = count_users(p);
	write_socket(p, "-- %s: %d user%s, %d room%s. (Max: %d, %d)",
	    LOCAL_NAME, num_users, num_users == 1 ? "" : "s",
	    num_rooms, num_rooms == 1 ? "" : "s", peak_users, peak_rooms);
	if (p->level > L_CITIZEN)
		write_socket(p, ". %.2f cmds/s, %.2f events/s\n", command_av,
		    event_av);
	else
		write_socket(p, "\n");
	for (ptr = users->next; ptr != (struct user *)NULL; ptr = ptr->next)
	{
		if (!IN_GAME(ptr))
			continue;
		if (strlen(ptr->rlname) > maxlen)
			maxlen = strlen(ptr->rlname) +
			    ((ptr->saveflags & U_INVIS) &&
			    ptr->level <= p->level ?  2 : 0);
	}
	init_strbuf(&mt, 0, "f_who");
	for (ptr = users->next; ptr != (struct user *)NULL; ptr = ptr->next)
	{
		if (!IN_GAME(ptr))
		{
			add_strbuf(&mt, "[ Connecting ]\n");
			continue;
		}
		if (!CAN_SEE(p, ptr))
			continue;
		if (ptr->saveflags & U_INVIS)
			sprintf(tmp, "(%s)", ptr->capname);
		else
			strcpy(tmp, ptr->capname);
		if (argv[1][1] == '-')
			sadd_strbuf(&mt, "%-10s %s%c %s ",
			    level_name(ptr->level, 1),
			    query_gender(ptr->gender, G_LETTER),
			    current_time - ptr->last_command > 900 ? 'I' : 
			    current_time - ptr->last_command > 300 ? 'i' : ' ',
			    tmp);
		else
			sadd_strbuf(&mt, "%-10s %s%c %-*s ",
			    level_name(ptr->level, 1),
			    query_gender(ptr->gender, G_LETTER),
			    current_time - ptr->last_command > 900 ? 'I' : 
			    current_time - ptr->last_command > 300 ? 'i' : ' ',
			    maxlen, tmp);
		switch(argv[1][1])
		{
		    case 'h':
			free_strbuf(&mt);
			write_socket(p, "Who options.\n");
			write_socket(p, " -a	Show afk reasons.\n");
			write_socket(p, " -g	Show genders.\n");
			write_socket(p, " -i	Show idle times.\n");
			write_socket(p, " -w	Show locations.\n");
			if (p->level < A_SEE_HEMAIL)
				return;
			write_socket(p, " -e	Show emails.\n");
			if (p->level < A_SEE_IP)
				return;
			write_socket(p, " -p	Show ip's.\n");
			if (p->level < L_OVERSEER)
				return;
			write_socket(p, " -s	Show snoopers.\n");
			write_socket(p, " -c	Show last commands.\n");
			return;

		    case '-':
			if (ptr->title != (char *)NULL)
				add_strbuf(&mt, ptr->title);
			if (ptr->flags & U_AFK)
				add_strbuf(&mt, " [AFK]");
			if (ptr->flags & U_INED)
				add_strbuf(&mt, " [EDITING]");
			cadd_strbuf(&mt, '\n');
			break;

		    case 'a':
			if ((ptr->flags & U_AFK) &&
			    ptr->stack.sp->type == T_STRING)
				sadd_strbuf(&mt, "[ %s ]\n",
				    ptr->stack.sp->u.string);
			else
				cadd_strbuf(&mt, '\n');
			break;

		    case 'g':
			add_strbuf(&mt, "[ ");
			add_strbuf(&mt, capitalise(query_gender(ptr->gender,
			    G_ABSOLUTE)));
			add_strbuf(&mt, " ]\n");
			break;

		    case 'i':
			add_strbuf(&mt, "[ ");
			add_strbuf(&mt, conv_time(current_time -
			    ptr->last_command));
			add_strbuf(&mt, " ]\n");
		    	break;

		    case 'w':
			if (ptr->super == (struct room *)NULL)
				add_strbuf(&mt, "[ the void.");
			else if (ptr->super->name != (char *)NULL)
			{
				add_strbuf(&mt, "[ ");
				add_strbuf(&mt, capitalise(ptr->super->name));
			}
			else
				add_strbuf(&mt, "[ <No name set>");
			if (p->level >= A_SEE_FNAME)
				if (ptr->super != (struct room *)NULL)	
				{
					add_strbuf(&mt, "  (");
					add_strbuf(&mt, ptr->super->fname
					    == (char *)NULL ? "(void)" :
					    ptr->super->fname);
					add_strbuf(&mt, ") ]\n");
				}
				else
					add_strbuf(&mt, "  (void) ]\n");
			else
				add_strbuf(&mt, " ]\n");
			break;

		    case 'e':
			if (p->level < A_SEE_HEMAIL)
				UNKNOWN_WHO_OPT;
			add_strbuf(&mt, "[ ");
			add_strbuf(&mt, ptr->email == (char *)NULL ?
			    "<None set>" : ptr->email);
			add_strbuf(&mt, " ]\n");
			break;

		    case 'p':
			if (p->level < A_SEE_IP)
				UNKNOWN_WHO_OPT;
			sadd_strbuf(&mt, "[ %-17s ", lookup_ip_number(ptr));
			if (p->level >= A_SEE_UNAME &&
			    ptr->uname != (char *)NULL)
			{
				add_strbuf(&mt, ptr->uname);
				cadd_strbuf(&mt, '@');
			}
			add_strbuf(&mt, lookup_ip_name(ptr));
			add_strbuf(&mt, " ]\n");
			break;

		    case 's':
			if (p->level < L_OVERSEER)
				UNKNOWN_WHO_OPT;
			if (ptr->snooped_by == (struct user *)NULL)
				add_strbuf(&mt, "[ --- ]\n");
			else
			{
				add_strbuf(&mt, "[ ");
				add_strbuf(&mt,
				    ptr->snooped_by->capname);
				add_strbuf(&mt, " ]\n");
			}
			break;

		    case 'c':
			if (p->level < L_OVERSEER)
				UNKNOWN_WHO_OPT;
			hptr = ptr->history->items[(ptr->history_ptr - 1) %
			    HISTORY_SIZE].u.string;
			add_strbuf(&mt, "[ ");
			add_strbuf(&mt, hptr == (char *)NULL ? "---" : hptr);
			add_strbuf(&mt, " ]\n");
		 	break;

		    default:
			UNKNOWN_WHO_OPT;
		}
	}
	pop_strbuf(&mt);
	more_start(p, mt.str, NULL);
}
#undef UNKNOWN_WHO_OPT

static void
with2(struct user *p, struct room *r, struct strbuf *buf)
{
	struct object *ob;
	int flag = 0;

	for (ob = r->ob->contains; ob != (struct object *)NULL;
	    ob = ob->next_contains)
	{
		if (ob->type != T_USER)
			continue;

		if (!CAN_SEE(p, ob->m.user))
			continue;

		if (!flag)
		{
			if (p->level >= A_SEE_FNAME)
				sadd_strbuf(buf, "%s (%s) :\n\t", r->name,
				    r->fname);
			else
				sadd_strbuf(buf, "%s :\n\t", r->name);
			flag = 1;
		}

		sadd_strbuf(buf, "%s ", ob->m.user->name);
	}
	if (flag)
		cadd_strbuf(buf, '\n');
}

void
f_with(struct user *p, int argc, char **argv)
{
	struct strbuf buf;

	if (argc < 2)
	{
		struct room *r;

		init_strbuf(&buf, 0, "f_with sb");

		for (r = rooms->next; r != (struct room *)NULL; r = r->next)
			with2(p, r, &buf);
	}
	else
	{
		struct user *u;

		if ((u = find_user(p, argv[1])) == (struct user *)NULL)
		{
			write_socket(p, "User %s not found.\n", argv[1]);
			return;
		}
		init_strbuf(&buf, 0, "f_with strbuf");
		with2(p, u->super, &buf);
	}
	more_start(p, buf.str, NULL);
	pop_strbuf(&buf);
}

void
f_write(struct user *p, int argc, char **argv)
{
	if (argc == 1)
		write_socket(p, "\n");
	else
	{
		char *str = parse_chevron_cookie(p, argv[1]);
		write_socket(p, "%s\n", str);
		xfree(str);
	}
}

