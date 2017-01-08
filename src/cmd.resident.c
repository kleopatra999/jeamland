/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	cmd.resident.c
 * Function:	Resident commands.
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/param.h>
#include <arpa/telnet.h>
#include <netinet/in.h>
#include "jeamland.h"

extern struct user *users;
extern struct grupe *sys_grupes;
extern struct alias *galiases;
extern time_t current_time;

/*
 * RESIDENT commands....
 */

void
afk2(struct user *p, char *c)
{
	if (strcmp(p->passwd, crypt(c, p->passwd)))
	{
		write_prompt(p, "\nIncorrect password.\nEnter your password: ");
		return;
	}
	echo(p);
	write_socket(p, "\nUnlocking commands.\n");
	if (p->stack.sp->type == T_STRING)
		write_socket(p, "\t[ %s ]\n", p->stack.sp->u.string);
	pop_stack(&p->stack);
	notify_levelabu(p, p->level, "[%s returns from afk.]",
	    p->capname);
	write_roomabu(p, "%s returns from afk.\n", p->name);
#ifdef REMOTE_NOTIFY
	send_rnotify(p, "-afk");
#endif
	p->flags &= ~U_AFK;
	p->flags &= ~U_NOESCAPE;
	p->input_to = NULL_INPUT_TO;
	return;
}

void
f_afk(struct user *p, int argc, char **argv)
{
	CHECK_INPUT_TO(p)

	notify_levelabu(p, p->level, "[%s goes afk.]", p->capname);
	write_socket(p, "You set yourself in Away From Keyboard mode.\n");
	if (argc > 1)
	{
		write_socket(p, "\t[ %s ]\n", argv[1]);
		push_string(&p->stack, argv[1]);
	}
	else
		push_number(&p->stack, 0);

	write_socket(p, "Enter your password to unlock commands.\n");
	noecho(p);
	write_prompt(p, "Enter your password: ");
	if (argc > 1)
		write_roomabu(p, "%s goes afk [ %s ].\n", p->name, argv[1]);
	else
		write_roomabu(p, "%s goes afk.\n", p->name);
#ifdef REMOTE_NOTIFY
	send_rnotify(p, "+afk");
#endif
	p->flags |= U_AFK;
	p->flags |= U_NOESCAPE;
	p->input_to = afk2;
	return;
}

enum alias_type { AL_USER, AL_ROOM, AL_GLOBAL };

void
edalias2(struct user *p, int i)
{
	struct alias *a, **list;
	char *c;
	int j, r;

	if (i != EDX_NORMAL)
	{
		pop_n_elems(&p->stack, 2);
		return;
	}

	if ((r = (p->stack.sp - 2)->u.number) == AL_ROOM)
		list = &p->super->alias;
	else if (r == AL_GLOBAL)
		list = &galiases;
	else
		list = &p->alias;

	if ((a = find_alias(*list, (p->stack.sp - 1)->u.string))
	    != (struct alias *)NULL)
		remove_alias(list, a);

	/* Need to sort out the string returned from the editor.. */

	/* Remove trailing newlines */
	j = strlen(p->stack.sp->u.string) - 1;
	while (p->stack.sp->u.string[j] == '\n')
		p->stack.sp->u.string[j--] = '\0';
	
	/* Replace all newlines with ; characters */
	for (c = p->stack.sp->u.string; *c != '\0'; c++)
		if (*c == '\n')
			*c = ';';

	if ((a = create_alias((p->stack.sp - 1)->u.string,
	    p->stack.sp->u.string)) == (struct alias *)NULL)
		write_socket(p, "Bad alias, aborting.\n");
	else
		add_alias(list, a);
	pop_n_elems(&p->stack, 3);
	if (r == AL_ROOM)
		store_room(p->super);
	else if (r == AL_GLOBAL)
		store_galiases();
}

static void
alias2(struct user *p, int argc, char **argv, struct alias **list, int lim)
{
	struct alias *a;
	int i;

	i = count_aliases(*list);

	if (argc < 3)
	{
		if (argc < 2)
		{
			struct strbuf ab;

			init_strbuf(&ab, 0, "f_alias ab");
			for (a = *list; a != (struct alias *)NULL; 
			    a = a->next)
				sadd_strbuf(&ab, "%-20s %s\n", a->key, a->fob);
			sadd_strbuf(&ab, "%d alias%s defined.\t", i,
			    i == 1 ? "" : "es");
			if (lim == -1)
				add_strbuf(&ab, "No alias limit set.\n");
			else
				sadd_strbuf(&ab, "Alias limit: %d.\n",
				    lim);
			pop_strbuf(&ab);
			more_start(p, ab.str, NULL);
			return;
		}
		if (!strcmp(argv[1], "-debug"))
		{
			write_socket(p, "Alias debug mode turned %s.\n",
			    (p->flags ^= U_DEBUG_ALIAS) & U_DEBUG_ALIAS ?
			    "on" : "off");
			return;
		}
		if (!strcmp(argv[1], "-wipe"))
		{
			free_aliases(list);
			write_socket(p, "All aliases removed.\n");
			return;
		}
		if ((a = find_alias(*list, argv[1])) !=
		    (struct alias *)NULL)
		{
				write_socket(p, "Unaliased: %-20s %s\n",
				    a->key, a->fob);
				remove_alias(list, a);
				return;
		}
		write_socket(p, "%s is not aliased.\n", argv[1]);
		return;
	}
	deltrail(argv[1]);

	if (*argv[0] == 'r' && !ISROOT(p) && !valid_ralias(argv[1]))
	{
		write_socket(p, "%s is an existing system command, "
		    "can not room alias.\n", argv[1]);
		return;
	}

	if ((a = find_alias(*list, argv[1])) != (struct alias *)NULL)
	{
		write_socket(p, "%s is already aliased - removed.\n", argv[1]);
		remove_alias(list, a);
		i--;
	}
	if (lim != -1 && i > lim)
	{
		write_socket(p, "Cannot have more than %d aliases\n", lim);
		return;
	}
	if ((a = create_alias(argv[1], argv[2])) == (struct alias *)NULL)
	{
		write_socket(p, "Bad alias, aborted.\n");
		return;
	}
	add_alias(list, a);
	write_socket(p, "Aliased '%s' to '%s'\n", argv[1], argv[2]);
}

void
f_alias(struct user *p, int argc, char **argv)
{
	enum alias_type al;

	if (!strcmp(argv[0], "ralias"))
		al = AL_ROOM;
	else if (!strcmp(argv[0], "galias"))
		al = AL_GLOBAL;
	else
		al = AL_USER;

	if (argc == 2 && !strncmp(argv[1], "edit:", 5))
	{
		struct alias *a;

		argv[1] += 5;

		if (al == AL_ROOM)
		{
			if (!ISROOT(p) && !valid_ralias(argv[1]))
			{
				write_socket(p,
				    "%s is an existing system command, "
				    "can not room alias.\n", argv[1]);
				return;
			}
			a = p->super->alias;
		}
		else if (al == AL_GLOBAL)
		{
			if (p->level < A_GALIASES)
			{
				write_socket(p, "Permission denied.\n");
				return;
			}
			a = galiases;
		}
		else
			a = p->alias;
			
		push_number(&p->stack, al);

		if ((a = find_alias(a, argv[1])) == (struct alias *)NULL)
		{
			/* New alias.. */
			push_string(&p->stack, argv[1]);
			ed_start(p, edalias2, 20, 0);
			return;
		}
		push_string(&p->stack, a->key);
		push_string(&p->stack, ";");	/* Ed token */
		push_string(&p->stack, a->fob);
		ed_start(p, edalias2, 20, ED_STACKED_TOK | ED_STACKED_TEXT);
		return;
	}

	if (al == AL_ROOM)
	{
		if (!CAN_CHANGE_ROOM(p, p->super))
		{
			write_socket(p, "You don't own this room.\n");
			return;
		}
		alias2(p, argc, argv, &p->super->alias, p->super->alias_lim);
		store_room(p->super);
	}
	else if (al == AL_GLOBAL)
	{
		if (argc > 1 && p->level < A_GALIASES)
		{
			write_socket(p, "Permission denied.\n");
			return;
		}
		alias2(p, argc, argv, &galiases, -1);
		store_galiases();
	}
	else if (!strcmp(argv[0], "aliases"))
	{
		struct user *u;

		if (argc < 2)
		{
			write_socket(p, "Syntax: aliases <user>\n");
			return;
		}

		deltrail(argv[1]);
		if ((u = find_user_msg(p, argv[1])) == (struct user *)NULL)
			return;
		argv++, argc--;
		alias2(p, argc, argv, &u->alias, u->alias_lim);
	}
	else
		alias2(p, argc, argv, &p->alias, p->alias_lim);
}

void
f_beep(struct user *p, int argc, char **argv)
{
	struct user *who;

	if (p->saveflags & U_BEEP_CURSE)
	{
		write_socket(p, "You may not use the beep command.\n");
		return;
	}
	if ((who = find_user_msg(p, argv[1])) == (struct user *)NULL)
		return;
	attr_colour(who, "incoming");
	write_socket(who, "%c%s beeps you.", 7, p->name);
	reset(who);
	write_socket(who, "\n");
	write_socket(p, "You beep %s.\n", who->name);
}

void
f_beeplogin(struct user *p, int argc, char **argv)
{
	write_socket(p, "Beep-at-login turned %s.\n",
	    (p->saveflags ^= U_BEEP_LOGIN) & U_BEEP_LOGIN ? "on" : "off");
}

void
f_capname(struct user *p, int argc, char **argv)
{
	if (strcasecmp(argv[1], p->rlname))
	{
		write_socket(p, "Capitalised name does not match your real"
		    " name!!!\n");
		return;
	}
	COPY(p->capname, argv[1], "capname");
	if (!strcasecmp(p->name, p->rlname))
	{
		COPY(p->name, argv[1], "capname");
	}
	write_socket(p, "Ok.\n");
}

#if defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT) || defined(IMUD3_SUPPORT)
void
f_chanblock(struct user *p, int argc, char **argv)
{
	struct strlist *s;
	int ok;

	if (argc < 2)
	{
		if (p->chan_earmuff == (struct strlist *)NULL)
		{
			write_socket(p,
			    "You are not blocking any channels.\n");
			return;
		}
		write_socket(p, "Blocking: ");
		for (s = p->chan_earmuff; s != (struct strlist *)NULL;
		    s = s->next)
			write_socket(p, "%s ", s->str);
		write_socket(p, "\n");
		return;
	}

	if ((s = member_strlist(p->chan_earmuff, argv[1])) !=
	    (struct strlist *)NULL)
	{
		remove_strlist(&p->chan_earmuff, s);
		write_socket(p, "Unblocked: %s\n", argv[1]);
		return;
	}

	ok = 0;

#ifdef IMUD3_SUPPORT
	if (i3_find_channel(argv[1]) != (struct i3_channel *)NULL)
		ok = 1;
#endif
#if defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT)
	if (inter_channel(argv[1], p->level))
		ok = 1;
#endif

	if (ok)
	{
		s = create_strlist("chanblock");
		s->str = string_copy(argv[1], "chanblock");
		add_strlist(&p->chan_earmuff, s, SL_SORT);
		write_socket(p, "Now blocking %s\n", argv[1]);
	}
	else
		write_socket(p, "Unknown channel, %s.\n", argv[1]);
}
#endif /* INETD_SUPPORT || CDUDP_SUPPORT || IMUD3_SUPPORT */

void
f_chex(struct user *p, int argc, char **argv)
{
	struct exit *e;

	if (!CAN_CHANGE_ROOM(p, p->super))
	{
		write_socket(p, "You do not own this room.\n");
		return;
	}
	for (e = p->super->exit; e != (struct exit *)NULL; e = e->next)
	{
		if (!strcmp(argv[1], e->name))
		{
			COPY(e->name, argv[2], "exit name");
			write_socket(p, "Ok.\n");
			store_room(p->super);
			return;
		}
	}
	write_socket(p, "%s is not a valid exit in this room.\n", argv[1]);
}

void
desc2(struct user *p, int i)
{
	struct room *r;

	if (i != EDX_NORMAL)
	{
		pop_stack(&p->stack);
		return;
	}

	/* The user may have changed environment! */
	if (!strcmp(p->super->fname, (p->stack.sp - 1)->u.string))
		r = p->super;
	else if (!ROOM_POINTER(r, (p->stack.sp - 1)->u.string))
	{
		write_socket(p, "The room you were in has disappeared!.\n");
		pop_n_elems(&p->stack, 2);
		return;
	}

	FREE(r->long_desc);
	r->long_desc = p->stack.sp->u.string;
	dec_stack(&p->stack);
	pop_stack(&p->stack);
	store_room(r);
}

void
f_desc(struct user *p, int argc, char **argv)
{
	if (!CAN_CHANGE_ROOM(p, p->super))
	{
		write_socket(p, "You do not own this room.\n");
		return;
	}
	push_string(&p->stack, p->super->fname);
	if (*argv[0] == 'e')
	{
		push_string(&p->stack, p->super->long_desc);
		ed_start(p, desc2, 20, ED_STACKED_TEXT);
	}
	else
		ed_start(p, desc2, 20, 0);
}

void
f_earmuffs(struct user *p, int argc, char **argv)
{
	int i;
	static struct {
		char *name;
		int flag;
		int level;
		} flags[] = {
	    { "shout",		EAR_SHOUTS,	L_RESIDENT },
	    { "notify",		EAR_NOTIFY,	L_RESIDENT },
	    { "chime",		EAR_CHIME,	L_RESIDENT },
	    { "tcp_notify",	EAR_TCP,	L_CONSUL },
#ifdef DEBUG_UDP
	    { "udp_notify",	EAR_UDP,	L_CONSUL },
#endif
#ifdef DEBUG_IMUD3
	    { "i3_notify",	EAR_I3,		L_CONSUL },
#endif
#ifdef DEBUG_RNCLIENT
	    { "rnclient",	EAR_RNCLIENT,	L_CONSUL },
#endif
	    { NULL, 0, 0 } };


	if (argc < 2)
	{
		struct strbuf blocked, canblock;
		int blockedflag = 0, blockflag = 0;
		int blockeddone = 0, blockdone = 0;

		init_strbuf(&blocked, 0, "f_earmuffs blocked");
		init_strbuf(&canblock, 0, "f_earmuffs canblock");

		for (i = 0; flags[i].name; i++)
		{
			if (flags[i].level > p->level)
				continue;
			if (p->earmuffs & flags[i].flag)
			{
				if (blockedflag)
					add_strbuf(&blocked, ", ");
				else
					blockedflag = 1;
				if (blocked.offset > 50 && !blockeddone)
				{
					add_strbuf(&blocked, "\n\t\t");
					blockeddone = 1;
				}
				add_strbuf(&blocked, flags[i].name);
			}
			else
			{
				if (blockflag)
					add_strbuf(&canblock, ", ");
				else
					blockflag = 1;
				if (canblock.offset > 50 && !blockdone)
				{
					add_strbuf(&canblock, "\n\t\t");
					blockdone = 1;
				}
				add_strbuf(&canblock, flags[i].name);
			}
		}
		if (blocked.offset)
			write_socket(p, "Blocked:\t%s\n", blocked.str);
		if (canblock.offset)
			write_socket(p, "Unblocked:\t%s\n", canblock.str);
		free_strbuf(&blocked);
		free_strbuf(&canblock);
		return;
	}
	for (i = 0; flags[i].name; i++)
	{
		if (flags[i].level > p->level)
			continue;
		if (!strcmp(argv[1], flags[i].name))
		{
			write_socket(p, "You are %s blocking %s.\n",
			    (p->earmuffs ^= flags[i].flag) & flags[i].flag ?
			    "now" : "no longer", flags[i].name);
			return;
		}
	}
	write_socket(p, "Unknown argument to earmuffs, %s\n", argv[1]);
}

void
f_eject(struct user *p, int argc, char **argv)
{
	struct user *who;

	if ((who = with_user_msg(p, argv[1])) == (struct user *)NULL)
		return;
	if (!CAN_CHANGE_ROOM(p, p->super))
	{
		write_socket(p, "You do not own this room.\n");
		return;
	}
	if (who->super == get_entrance_room())
	{
		write_socket(p, "Cannot eject someone from the entrance.\n");
		return;
	}
	if (!access_check(p, who))
		return;
	write_room(p, "%s has been ejected by %s\n", who->name, p->name);
	move_user(who, get_entrance_room());
	write_socket(p, "Succesfully ejected %s\n", who->name);
}

void
send_valinfo(struct user *p, char *addr)
{
	char *q;

	if ((q = read_file(F_VALINFO)) == (char *)NULL)
		return;

	send_email(p->rlname, addr, (char *)NULL, "Welcome to "LOCAL_NAME, 1,
	    "%s\n", q);

	xfree(q);
}

static void
email2(struct user *p, char *c)
{
#ifdef AUTO_VALEMAIL
	char buf[BUFFER_SIZE];
	char *id;
#endif

	p->input_to = NULL_INPUT_TO;

	if (tolower(*c) != 'y')
	{
		write_socket(p, "Email address not changed.\n");
		pop_stack(&p->stack);
		return;
	}

	/* Unset the validated email flag.. */
	p->saveflags &= ~U_EMAIL_VALID;
	p->saveflags &= ~U_NOEMAIL;

	notify_levelabu(p, p->level >= L_WARDEN ? p->level : L_WARDEN,
	    "[ %s has changed %s email address to %s. ]",
	    p->capname, query_gender(p->gender, G_POSSESSIVE),
	    p->stack.sp->u.string);

	if (p->email == (char *)NULL)
		send_valinfo(p, p->stack.sp->u.string);

	FREE(p->email);
	p->email = p->stack.sp->u.string;
	dec_stack(&p->stack);

	write_socket(p, "You have changed your email address to: %s\n",
	    p->email);
	write_socket(p,
	    "Email address is %s - use 'email -p' to toggle this.\n",
	    p->saveflags & U_EMAIL_PRIVATE ? "private" : "public");

#ifdef AUTO_VALEMAIL
	sprintf(buf, "%s email validation. JL-id: %s", LOCAL_NAME,
	    id = random_password(12));

	send_email(p->rlname, p->email, (char *)NULL, buf, 1,
	    "%s on the %s talker wishes to register this as %s email\n"
	    "address. Please reply to this message, preserving the subject\n"
	    "line, with the text body 'yes' if this is acceptable.\n\n"
	    "If it is unacceptable then please help us to cut down\n"
	    "on fraudulent use of our system by replying with the text body\n"
	    "'no'.\n\n"
	    "Regards,\n\t%s talker administration.\n\n\n",
	    p->capname, LOCAL_NAME,
	    query_gender(p->gender, G_POSSESSIVE), LOCAL_NAME);

	register_valemail_id(p, id);
#endif /* AUTO_VALEMAIL */
}

void
f_email(struct user *p, int argc, char **argv)
{
	if (argc < 2)
	{
		if (p->email != (char *)NULL)
			write_socket(p, "Your email address is set to: %s\n",
			    p->email);
		else
			write_socket(p, "Your email address is not set.\n");
		write_socket(p, "Your email address is %s.\n",
		    p->saveflags & U_EMAIL_PRIVATE ? "private" : "public");
		write_socket(p, "---\n");
		write_socket(p, "To set or change an email address, type:\n\t"
		    "email <address>\n"
		    "To toggle your email address between being private and "
		    "public, type:\n\temail -p\n");
		return;
	}
	if (!strcmp(argv[1], "-p"))
	{
		if ((p->saveflags ^= U_EMAIL_PRIVATE) & U_EMAIL_PRIVATE)
			write_socket(p, "Email address marked private.\n");
		else
			write_socket(p, "Email address is no longer marked"
			    " private.\n");
		return;
	}
	if (!valid_email(argv[1]))
	{
		write_socket(p, "Not a valid email address.\n");
		return;
	}

	CHECK_INPUT_TO(p)

	push_string(&p->stack, argv[1]);

	write_prompt(p, "New email address: %s\nIs this correct ? (y/n) ",
	    argv[1]);

	p->input_to = email2;
}

void
f_finger(struct user *p, int argc, char **argv)
{
	extern int fob_finger(struct user *, char *);

#if defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT) || defined(IMUD3_SUPPORT)
	if (strchr(argv[1], '@') != (char *)NULL)
	{
		imud_finger(p, argv[1]);
		return;
	}
#endif /* INETD_SUPPORT || CDUDP_SUPPORT || IMUD3_SUPPORT */

	if (*argv[1] == '#')
	{
		struct vecbuf buf;
		struct vector *v;
		char *t;

		argv[1]++;

		init_vecbuf(&buf, 0, "f_finger grupe vb");
		t = string_copy(argv[1], "f_finger t copy");
		expand_grupe(p, &buf, t);
		xfree(t);
		v = vecbuf_vector(&buf);

		if (!v->size)
			write_socket(p, "%s: No such grupe.\n", argv[1]);
		else
		{
			int i, flag = 0;

			write_socket(p, "%s: ", argv[1]);
			for (i = v->size; i--; )
			{
				if (v->items[i].type != T_STRING)
					continue;
				if (flag)
					write_socket(p, ", ");
				else
					flag = 1;
				write_socket(p, "%s", v->items[i].u.string);
			}
			write_socket(p, "\n");
		}
				
		free_vector(v);
		return;
	}

	more_start(p, finger_text(p, lower_case(argv[1]), FINGER_LOCAL),
	    NULL);
}

void post2(struct user *, int);

void
f_followup(struct user *p, int argc, char **argv)
{
	struct message *m;
	struct board *targ;
	struct room *r;
	char *subj;
	int em = 0;

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

	deltrail(argv[1]);

	if (argc > 2 && !strcmp(argv[1], "-e"))
	{
		em = 1;
		argc--, argv++;
	}

	if ((m = find_message(p->super->board, atoi(argv[1]))) ==
	    (struct message *)NULL)
	{
		write_socket(p, "No such note, %s\n", argv[1]);
		return;
	}

	if (em)
	{
		reply_message(p, m, 0);
		return;
	}

	r = p->super;
	targ = r->board;

	if (targ->followup != (char *)NULL)
	{
		if (!ROOM_POINTER(r, targ->followup))
		{
			log_file("error", "Followup board room missing: "
			    "%s (from %s).", targ->followup, p->super->fname);
			write_socket(p, "Followup board room not found.\n");
			return;
		}
		if (!(r->flags & R_BOARD))
		{
			log_file("error", "Followup board missing: "
			    "%s (from %s).", targ->followup, p->super->fname);
			write_socket(p, "Followup board not found.\n");
			return;
		}
		targ = r->board;
	}

	if (!CAN_WRITE_BOARD(p, targ))
	{
		write_socket(p, "You may not post to the followup board.\n");
		return;
	}

	if (r != p->super)
		write_socket(p, "--\nNB: Followups set to %s.\n--\n", r->name);

	subj = prepend_re(m->subject);

	write_roomabu(p, "%s begins to write a note entitled '%s'\n",
	    p->name, subj);

	push_string(&p->stack, r->fname);
	push_malloced_string(&p->stack, subj);
	push_malloced_string(&p->stack, quote_message(m->text, p->quote_char));
	ed_start(p, post2, MAX_BOARD_LINES, ED_STACKED_TEXT |
	    ((p->medopts & U_EDOPT_SIG) ? ED_APPEND_SIG : 0));
	return;
}

void
f_from(struct user *p, int argc, char **argv)
{
	extern char *mail_from(struct user *, int);
	char *tmp;
	int flag = 0;
	int o;

	if (p->mailbox == (struct board *)NULL)
	{
		flag = 1;
		p->mailbox = restore_mailbox(p->rlname);
	}
	
	o = p->medopts;
	if (argc > 1 && !strcmp(argv[1] , "-a"))
		p->medopts &= ~U_MAILOPT_NEW_ONLY;

	if ((tmp = mail_from(p, 0)) != (char *)NULL)
		more_start(p, tmp, NULL);
	p->medopts = o;

	if (flag)
	{
		free_board(p->mailbox);
		p->mailbox = (struct board *)NULL;
	}
}

void
f_grupe(struct user *p, int argc, char **argv)
{
	extern void store_mastersave(void);
	struct grupe **list;
	struct grupe *g;
	struct grupe_el *e;
	char *name;
	int sysgrp;

	/* Modify system grupes if command was 'Grupe' */
	if ((sysgrp = isupper(**argv)))
		list = &sys_grupes;
	else
		list = &p->grupes;

	if (argc < 2)
	{	/* List them... */
		struct strbuf buf;
		int flag;

		init_strbuf(&buf, 0, "f_grupe lsgrupes");

		for (g = *list; g != (struct grupe *)NULL; g = g->next)
		{
			sadd_strbuf(&buf, "%s: ", g->gid);
			for (flag = 0, e = g->el; e != (struct grupe_el *)NULL;
			    e = e->next)
			{
				if (flag)
					add_strbuf(&buf, ", ");
				else
					flag = 1;
				add_strbuf(&buf, e->name);
			}
			cadd_strbuf(&buf, '\n');
		}
		if (!buf.offset)
		{
			write_socket(p, "No grupes defined.\n");
			free_strbuf(&buf);
		}
		else
		{
			pop_strbuf(&buf);
			more_start(p, buf.str, NULL);
		}
		return;
	}
	if (argc < 3)
	{
		if (!strcmp(argv[1], "-r"))
			write_socket(p,
			    "Syntax: grupe -r <grupe id>\n");
		else
			write_socket(p,
			    "Syntax: grupe <grupe id> <user> [user]...\n");
		return;
	}
	deltrail(argv[1]);

	if (sysgrp && p->level < A_GRUPE_ADMIN)
	{
		write_socket(p, "Permission denied.\n");
		return;
	}

	if (!strcmp(argv[1], "-r"))
	{
		/* Wipe a grupe. */

		if (*argv[2] == '#')
			argv[2]++;

		if ((g = find_grupe(*list, argv[2])) == (struct grupe *)NULL)
		{
			write_socket(p, "Grupe %s does not exist.\n", argv[2]);
			return;
		}
		remove_grupe(list, g);
		write_socket(p, "Removed grupe %s.\n", argv[2]);
		if (sysgrp)
			store_mastersave();
		return;
	}

	if (*argv[1] == '#')
		argv[1]++;

	/* First things first, are we adding a new grupe.. */
	if ((g = find_grupe(*list, argv[1])) == (struct grupe *)NULL)
	{
		if (!sysgrp && count_grupes(*list) >= GRUPE_LIMIT)
		{
			write_socket(p,
			    "You already have the maximum permitted number of "
			    "grupes.\n");
			return;
		}
		/* Make a new one.. */
		add_grupe(list, argv[1]);
		g = find_grupe(*list, argv[1]);
	}

	write_socket(p, "Grupe %s:\n", g->gid);

	/* Loop through the remaining arguments.. */

	name = strtok(argv[2], " ");
	do
	{
		if ((e = find_grupe_el(g, name)) != (struct grupe_el *)NULL)
		{
			remove_grupe_el(g, e);
			write_socket(p, "\tRemoved %s.\n", name);
		}
		else if (!sysgrp && count_grupe_els(g) >= GRUPE_EL_LIMIT)
			write_socket(p, "\tGrupe full, skipping %s.\n", name);
		else
		{
			add_grupe_el(g, name, 0);
			write_socket(p, "\tAdded %s.\n", name);
		}
	} while ((name = strtok((char *)NULL, " ")) != (char *)NULL);

	/* Now check if the grupe is empty. */
	if (!count_grupe_els(g))
	{
		write_socket(p, "Grupe empty: removed.\n");
		remove_grupe(list, g);
	}
	if (sysgrp)
		store_mastersave();
}

void
f_go(struct user *p, int argc, char **argv)
{
	struct room *r;
	int i;
	int barge, sneak;
	extern void f_vis(struct user *, int, char **);

	barge = strcmp(argv[0], "barge") ? 0 : 1;
	sneak = strcmp(argv[0], "sneak") ? 0 : 1;

	if ((i = handle_exit(p, argv[1], barge)))	/* Poke -Wall */
	{
		if (i == -1)
			write_socket(p, "Could not move you.\n");
		return;
	}

	if (!ROOM_POINTER(r, argv[1]))
	{
		write_socket(p, "That room does not exist!\n");
		return;
	}
	if (p->level < L_WARDEN && strcmp(p->rlname, r->owner))
	{
		write_socket(p, "You do not own that room.\n");
		return;
	}
	if ((r->flags & R_LOCKED) && strcmp(p->rlname, r->owner))
	{
		if (barge)
		{
			if (!CAN_CHANGE_ROOM(p, r))
				return;
		}
		else
		{
			write_socket(p, "That room is locked.\n");
			return;
		}
	}
	if (r->lock_grupe != (char *)NULL && !ISROOT(p) &&
	    !member_sysgrupe(r->lock_grupe, p->rlname))
	{
		if (barge)
		{
			if (!CAN_CHANGE_ROOM(p, r))
				return;
		}
		else
		{
			write_socket(p, "That room is locked.\n");
			return;
		}
	}
	else if (sneak)
	{
		if (!(p->saveflags & U_INVIS))
		{
			p->saveflags |= U_INVIS;
			sneak = 2;
		}
	}
	move_user(p, r);
	if (sneak)
		write_room_levelabu(p, p->level, "%s sneaks in.\n",
		    p->name);
	if (sneak == 2)
		p->saveflags &= ~U_INVIS;
}

void
f_home(struct user *p, int argc, char **argv)
{
	struct room *r;
	char *tmp = lower_case(p->rlname);

	if (!ROOM_POINTER(r, tmp))
	{
	    	if ((r = new_room(p->rlname, tmp)) == (struct room *)NULL)
			fatal("Error creating room.");
		notify_levelabu(p, p->level >= L_WARDEN ? p->level : L_WARDEN,
		    "[ %s has created a home. ]", p->capname);
	}
	move_user(p, r);
}

#if defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT)
void
f_hosts(struct user *p, int argc, char **argv)
{
	extern void load_hosts(int), save_hosts(struct host *, char *);
#ifdef INETD_SUPPORT
	extern void ping_all(void);
	extern struct host *hosts;
#endif
#ifdef CDUDP_SUPPORT
	extern void ping_all_cdudp(void);
	extern struct host *cdudp_hosts;
#endif
	struct host *h;
	int lon, column, total, up;
	int ghosts = !strcmp(argv[0], "chosts");
	int jlhosts = !strcmp(argv[0], "jlhosts");
	time_t tm;
	struct strbuf str;

	lon = column = total = up = 0;

	if (!jlhosts && p->level > L_CONSUL && argc > 1)
	{
		if (!strcmp(argv[1], "-startup"))
		{
			write_socket(p, "Pinging all hosts.\n");
#ifdef CDUDP_SUPPORT
			if (ghosts)
				ping_all_cdudp();
#endif
#ifdef INETD_SUPPORT
			if (!ghosts)
				ping_all();
#endif
			return;
		}
		else if (!strcmp(argv[1], "-reload"))
		{
			load_hosts(ghosts);
			write_socket(p, "Rescanned hosts file.\n");
			return;
		}
		else if (!strcmp(argv[1], "-dump"))
		{
#ifdef CDUDP_SUPPORT
			if (ghosts)
				save_hosts(cdudp_hosts, F_CDUDP_HOSTS);
#endif
#ifdef INETD_SUPPORT
			if (!ghosts)
				save_hosts(hosts, F_INETD_HOSTS);
#endif
			write_socket(p, "Dumped.\n");
			return;
		}
	}
	if (argc > 1)
	{
		if (!strcmp(argv[1], "-l"))
			lon++;
		else
		{
#if defined(INETD_SUPPORT)
			if (!ghosts)
				h = lookup_inetd_host(argv[1]);
#endif
#if defined(CDUDP_SUPPORT)
			if (ghosts)
				h = lookup_cdudp_host(argv[1]);
#endif
			if (h == (struct host *)NULL)
			{
				write_socket(p,
				    "Unknown or ambiguous host name:"
				    " %s\n", argv[1]);
				return;
			}
			write_socket(p, "%s: %s (%d)\n", h->name,
			    h->host, h->port);
#ifdef INETD_SUPPORT
			if (!ghosts)
				write_socket(p,
				    "Use iquery <host> mud_port to get "
				    "the login port\n");
#endif
			return;
		}
	}

	init_strbuf(&str, 0, "f_hosts str");
#ifdef CDUDP_SUPPORT
	if (ghosts)
		h = cdudp_hosts;
#endif
#ifdef INETD_SUPPORT
	if (!ghosts)
		h = hosts;
#endif
	for (; h != (struct host *)NULL; h = h->next)
	{
		if (jlhosts && !h->is_jeamland)
			continue;

		total++;
		if (h->status > 0)
		{
			tm = (time_t)h->status;
			up++;
		}
		else
			tm = -(time_t)h->status;

		if (lon)
			if (h->status)
				sadd_strbuf(&str, "%-20s %-5s %s", h->name,
			    	    h->status > 0 ? "UP" : "DOWN",
				    ctime(user_time(p, tm)));
			else
				sadd_strbuf(&str,
				    "%-20s UNKNOWN Never accessed.\n",
				    h->name);
		else
		{
#define TRUNC_LEN 16
			char trunc_name[TRUNC_LEN + 1];

			my_strncpy(trunc_name, h->name, TRUNC_LEN);
#undef TRUNC_LEN
				
			sadd_strbuf(&str, "%c%-17s", h->status > 0 ? 
			    (h->is_jeamland ? '!' : '*') : 
		    	    (!h->status ? '-' : ' '), trunc_name);
			if (column++ > 2)
			{
				column = 0;
				cadd_strbuf(&str, '\n');
			}
		}
	}
	write_socket(p, "Currently listed hosts [total:%d up:%d down:%d]\n",
	    total, up, total - up);
	cadd_strbuf(&str, '\n');
	pop_strbuf(&str);
	more_start(p, str.str, NULL);
}
#endif

#ifdef INETD_SUPPORT
void
f_ichannel(struct user *p, int argc, char **argv)
{
	extern void inetd_channel(struct user *, char *, char *, int);

	inetd_channel(p, argv[0], argv[1], 0);
}

void
f_ichannel_jl(struct user *p, int argc, char **argv)
{
	extern void inetd_channel(struct user *, char *, char *, int);

	inetd_channel(p, argv[0], argv[1], 1);
}
#endif

#if defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT) || defined(IMUD3_SUPPORT)
void
f_ilocate(struct user *p, int argc, char **argv)
{
	int all;

	deltrail(argv[1]);

	all = argc > 2 && !strcmp(argv[2], "all");

#ifdef IMUD3_SUPPORT
	i3_cmd_locate(p, argv[1]);
#endif
#if defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT)
	inetd_locate(p, argv[1], all);
#endif
}
#endif /* INETD_SUPPORT || CDUDP_SUPPORT || IMUD3_SUPPORT */

#if defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT)
void
f_iquery(struct user *p, int argc, char **argv)
{
	extern void inetd_query(struct user *, char *, char *);
	inetd_query(p, argv[1], argv[2]);
}
#endif

#ifdef INETD_SUPPORT
void
f_import(struct user *p, int argc, char **argv)
{
	char *q;

	if ((q = strchr(argv[1], '@')) == (char *)NULL)
	{
		write_socket(p, "Syntax: import <user>@<JeamLand Host>\n");
		return;
	}
	*q++ = '\0';
	inetd_import(p, argv[1], q);
}
#endif

void
f_jobs(struct user *p, int argc, char **argv)
{
	int i;
	struct event *ev;

	for (i = 0; i < NUM_JOBS; i++)
		if (p->jobs[i].flags & JOB_USED)
		{
			if ((ev = find_event(p->jobs[i].event)) ==
			    (struct event *)NULL)
			{
				write_socket(p, "[%d]  Dead.\n", i + 1);
				p->jobs[i].flags = 0;
			}
			else
				write_socket(p, "[%d] %cRunning (%d)  %s\n",
				    i + 1,
				    p->jobs[i].flags & JOB_QUIET ? '*' : ' ',
				    ev->time - current_time,
				    (ev->stack.sp - 1)->u.string);
		}
}

void
f_join(struct user *p, int argc, char **argv)
{
	struct user *u;

	if ((u = find_user_msg(p, argv[1])) == (struct user *)NULL)
		return;
	if ((u->super->flags & R_LOCKED) && 
	    strcmp(u->super->owner, p->rlname))
	{
		write_socket(p, "That room is locked.\n");
		return;
	}
	if (u->super->lock_grupe != (char *)NULL && !ISROOT(p) &&
	    !member_sysgrupe(u->super->lock_grupe, p->rlname))
	{
		write_socket(p, "That room is locked.\n");
		return;
	}
	move_user(p, u->super);
}

void
f_kill(struct user *p, int argc, char **argv)
{
	int i;

	if (argv[1][0] == '#')
	{
		if (!ISROOT(p))
		{
			write_socket(p, "Only root may kill events.\n");
			return;
		}
		if (find_event(i = atoi(++argv[1])) == (struct event *)NULL)
		{
			write_socket(p, "No such event.\n");
			return;
		}
		remove_event(i);
		write_socket(p, "Removed event %d.\n", i);
		return;
	}

	if (argv[1][0] == '%')
		argv[1]++;

	i = atoi(argv[1]);

	if (i < 1 || i > NUM_JOBS || !(p->jobs[--i].flags & JOB_USED))
	{
		write_socket(p, "No such job, %s\n", argv[1]);
		return;
	}
	p->jobs[i].flags = 0;
	remove_event(p->jobs[i].event);
	write_socket(p, "[%d] Killed.\n", i + 1);
}

void
f_last(struct user *p, int argc, char **argv)
{
	struct strbuf str;
	struct vecbuf vb;
	struct vector *v;
	int i;

	init_vecbuf(&vb, 0, "f_last vecbuf");
	expand_user_list(p, argv[1], &vb, 0, 0, 0);

	v = vecbuf_vector(&vb);

	init_strbuf(&str, 0, "f_last grupe sb");

	/* This will only happen if a grupe name was supplied. */
	if (!v->size)
		sadd_strbuf(&str, "%s: No members.\n", argv[1]);
	else for (i = v->size; i--; )
	{
		if (v->items[i].type != T_STRING)
			continue;

		sadd_strbuf(&str, "%15s : ", capfirst(v->items[i].u.string));
		if (find_user_absolute(p, v->items[i].u.string) !=
		    (struct user *)NULL)
			add_strbuf(&str, "[ Currently logged on ].\n");
		else
		{
			char buff[MAXPATHLEN + 1];
			time_t tmptim;

			sprintf(buff, "users/%c/%s.o",
			    v->items[i].u.string[0],
			    v->items[i].u.string);

			if ((tmptim = file_time(buff)) == -1)
				add_strbuf(&str, "<Unknown>\n");
			else
			{
				sadd_strbuf(&str, "%s.\n",
				    nctime(user_time(p, tmptim)));
				sadd_strbuf(&str, "%15s   (%s)\n", " ",
				    conv_time(current_time - tmptim));
			}
		}
	}
	free_vector(v);
	pop_strbuf(&str);
	more_start(p, str.str, NULL);
}

void
f_link(struct user *p, int argc, char **argv)
{
	struct exit *e;
	struct room *r;

	if (!CAN_CHANGE_ROOM(p, p->super))
	{
		write_socket(p, "You do not own this room.\n");
		return;
	}
	if (!ROOM_POINTER(r, argv[2]))
	{
		write_socket(p, "Room %s does not exist.\n", argv[2]);
		return;
	}
	if (r == p->super)
	{
		write_socket(p, "Exit can not lead back here!.\n");
		return;
	}
	if (!CAN_CHANGE_ROOM(p, r))
	{
		write_socket(p, "You do not own room %s.\n", argv[2]);
		return;
	}
	if (p->level < L_CONSUL && SYSROOM(p->super))
	{
		write_socket(p, "You may not add exits to system rooms.\n");
		return;
	}
		
	e = create_exit();
	e->name = string_copy(argv[1], "exit name");
	e->file = string_copy(argv[2], "exit file");
	add_exit(p->super, e);
	write_socket(p, "Linked.\n");
	store_room(p->super);
}

void
f_lock(struct user *p, int argc, char **argv)
{
	if (p->super == (struct room *)NULL)
	{
		write_socket(p, "You have no environment.\n");
		return;
	}
	if (!CAN_CHANGE_ROOM(p, p->super))
	{
		write_socket(p, "You don't own this room.\n");
		return;
	}
	if (p->level > L_WARDEN && argc > 1)
	{
		/* # Not required, but allowed */
		if (*argv[1] == '#')
			argv[1]++;
		COPY(p->super->lock_grupe, argv[1], "lock_grupe");
		store_room(p->super);
		write_socket(p, "Locked environment to grupe %s\n",
		    argv[1]);
		return;
	}
	if (p->super->flags & R_LOCKED)
	{
		write_socket(p, "This room was already locked.\n");
		return;
	}
	p->super->flags |= R_LOCKED;
	store_room(p->super);
	write_socket(p, "This room is now locked.\n");
}

#ifdef INETD_SUPPORT
void
f_lookup(struct user *p, int argc, char **argv)
{
	extern void inetd_dict(struct user *, char *);

	inetd_dict(p, argv[1]);
}
#endif

void
mail3(struct user *p, char *c)
{
	extern void mail_more_ret(struct user *);
	struct vecbuf v;
	struct vector *vec;
	struct strbuf cc;
	int i, flag;
	int snt = 0;

	init_vecbuf(&v, 0, "mail3 vecbuf");
	init_strbuf(&cc, 0, "mail3 cc");
	expand_user_list(p, (p->stack.sp - 2)->u.string, &v, 0, 0, 1);
	expand_user_list(p, c, &v, 0, 0, 1);
	vec = vecbuf_vector(&v);

	for (i = vec->size; i--; )
	{
		char *r;

		if (vec->items[i].type != T_STRING)
			continue;

		r = vec->items[i].u.string;
		
#ifdef INETD_SUPPORT
		if (strchr(r, '@') != (char *)NULL)
		{
			inetd_mail(p->rlname, r, (p->stack.sp - 1)->u.string,
			    p->stack.sp->u.string, 0);
			snt++;
			*r = '\0';
		}
		else
#endif
		{
			if (!exist_user(r))
			{
				write_socket(p, "No such user, %s\n", r);
				*r = '\0';
			}
			else
			{
				int k, flag = 0;

				/* Construct cc field.. */
				reinit_strbuf(&cc);

				for (k = vec->size; k--; )
				{
					char *s;

					if (k == i)
						continue;
					s = vec->items[k].u.string;

					if (*s == '\0')
						continue;
					/* Latter half of the array has not yet
					 * been checked */
					if (strchr(s, '@') == (char *)NULL &&
					    !exist_user(s))
						continue;

					if (flag)
						cadd_strbuf(&cc, ',');
					else
						flag = 1;
					add_strbuf(&cc, s);
				}
				if (!deliver_mail(r, p->rlname,
				    (p->stack.sp - 1)->u.string,
				    p->stack.sp->u.string, cc.str, 0, 0))
				{
					write_socket(p, "Mail to %s failed -"
					    " mailbox probably full.\n",
					    capitalise(r));
					*r = '\0';
				}
				else
					snt++;
			}
		}
	}

	if (!snt)
		write_socket(p,
		    "No recipients found. Message can be retrieved with ~R\n");
	else
	{
		reinit_strbuf(&cc);
		for (flag = 0, i = vec->size; i--; )
		{
			char *r;

			if (vec->items[i].type != T_STRING)
				continue;

			r = vec->items[i].u.string;

			if (*r == '\0')
				continue;
			if (flag)
				add_strbuf(&cc, ", ");
			else
				flag = 1;
			add_strbuf(&cc, r);
		}
		write_socket(p, "Mail sent: %s\n", cc.str);
	}
	free_vector(vec);
	free_strbuf(&cc);
	pop_n_elems(&p->stack, 3);
	p->input_to = NULL_INPUT_TO;
	if (p->flags & U_IN_MAILER)
                mail_more_ret(p);
}

void
mail2(struct user *p, int i)
{
	extern void mail_more_ret(struct user *);

	if (i != EDX_NORMAL)
	{
		pop_n_elems(&p->stack, 2);
 		if (p->flags & U_IN_MAILER)
                	mail_more_ret(p);
		return;
	}
	write_prompt(p, "Cc: ");
	p->input_to = mail3;
}

void
mail_subject(struct user *p, char *c)
{
	if (!strlen(c))
		push_string(&p->stack, "(no subject)");
	else
		push_string(&p->stack, c);
	p->input_to = NULL_INPUT_TO;
	ed_start(p, mail2, MAX_MAIL_LINES,
	    (p->medopts & U_MAILOPT_SIG) ? ED_APPEND_SIG : 0);
}

void
f_mail(struct user *p, int argc, char **argv)
{
	extern void mail_start(struct user *);
	extern void kill_mailspool(struct user *, char *);

	CHECK_INPUT_TO(p)

	if (argc > 1)
	{
#ifdef INETD_SUPPORT
		if (!strncmp(argv[1], "-s", 2))
		{
			kill_mailspool(p, argv[1] + 2);
			return;
		}
		else if (!strncmp(argv[1], "-k", 2))
		{
			extern void handle_mailspool(struct event *);

			if (p->level < L_CONSUL)
			{
				write_socket(p, "Permission denied.\n");
				return;
			}

			handle_mailspool((struct event *)NULL);
			write_socket(p, "Kicked mailspool.\n");
			return;
		}
#endif
		push_string(&p->stack, argv[1]);
		p->input_to = mail_subject;
		write_prompt(p, "Subject: ");
		return;
	}
	mail_start(p);
}

void
f_mailnote(struct user *p, int argc, char **argv)
{
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
	if ((m = find_message(p->super->board, atoi(argv[1]))) ==
	    (struct message *)NULL)
	{
		write_socket(p, "No such note, %s\n", argv[1]);
		return;
	}
	if (!deliver_mail(p->rlname, "<Board system>", m->subject, m->text,
	    (char *)NULL, 0, 0))
	{
		write_socket(p, "Could not deliver mail to you, "
		    "mailbox full ?\n");
		return;
	}
	write_socket(p, "Mail sent.\n");
}

void
f_messages(struct user *p, int argc, char **argv)
{
	short int leave = **argv == 'l';
	char *c;

	if (argc < 2)
	{
		if ((leave && p->leavemsg == (char *)NULL) ||
		    (!leave && p->entermsg == (char *)NULL))
			write_socket(p, "Your %s message is set to: %s\n",
			    leave ? "leave" : "enter",
			    leave ? DEFAULT_LEAVEMSG : DEFAULT_ENTERMSG);
		else
			write_socket(p, "Your %s message is set to: %s\n",
			    leave ? "leave" : "enter",
			    leave ? p->leavemsg : p->entermsg);
		return;
	}

	if (!exist_cookie(argv[1], 'N'))
	{
		/* Prepend.. */
		c = (char *)xalloc(strlen(argv[1]) + 4, "le messages");
		strcpy(c, "%N ");
		strcat(c, argv[1]);
		write_socket(p, "%s",
		    "No %N cookie found in string, prepending.\n");
	}
	else
		c = string_copy(argv[1], "le messages_2");

	if (leave)
	{
		FREE(p->leavemsg);
		p->leavemsg = c;
	}
	else
	{
		FREE(p->entermsg);
		p->entermsg = c;
	}
	write_socket(p, "%s message set to: %s\n", leave ? "Leave" : "Enter",
	    c);
}

void
f_myrooms(struct user *p, int argc, char **argv)
{
	struct vector *vec;
	struct room *r;
	char buf[MAXPATHLEN + 1], *user;
	int found = 0, s;

	if (argc > 1 && p->level > L_WARDEN)
		user = argv[1];
	else
		user = p->rlname;

	s = strlen(user);

	sprintf(buf, "room/%c", user[0]);

	if ((vec = get_dir(buf, 0)) != (struct vector *)NULL)
	{
		int i;

		for (i = vec->size; i--; )
		{
			char *t = strrchr(vec->items[i].u.string, '.');
			if (t != (char *)NULL && !strcmp(t, ".o"))
				*t = '\0';
			else
				continue;

			if (!strncmp(vec->items[i].u.string, user, s) &&
			    ROOM_POINTER(r, vec->items[i].u.string) &&
			    !strcmp(r->owner, user))
			{
				struct exit *e;

				write_socket(p, "%-15s  :  %s\n",
				    r->fname, r->name);
				for (e = r->exit; e != (struct exit *)NULL;
				    e = e->next)
					write_socket(p, "\t%-15s : %s\n",
					    e->name, e->file);
				found++;
			}
		}
		free_vector(vec);
	}
	if ((vec = get_dir("room/_", 0)) != (struct vector *)NULL)
	{
		int i;
		int destroy_after;

		/* Don't allow these to clutter up the memory hence the
		 * sneaky destroy_after assignment in the if statement.
		 * If the room was not already in memory, it is destroyed
		 * after it is finished with */
		for (i = vec->size; i--; )
		{
			char *t = strrchr(vec->items[i].u.string, '.');
			if (t != (char *)NULL && !strcmp(t, ".o"))
				*t = '\0';
			else
				continue;

			destroy_after = 0;

			if (((r = find_room(vec->items[i].u.string)) !=
			    (struct room *)NULL || ((destroy_after = 1) &&
			    (r = restore_room(vec->items[i].u.string)) !=
			    (struct room *)NULL)))
			{
				if (!strcmp(r->owner, user))
				{
					write_socket(p, "%-15s  :  %s\n",
				    	    r->fname, r->name);
					found++;
				}
				/* *wave sadly* */
				if (destroy_after)
					destroy_room(r);
			}
		}
		free_vector(vec);
	}

	if (!found)
		write_socket(p, "No rooms found.\n");
	else
		write_socket(p, "%d room%s found.\n", found, found == 1 ?
		    "" : "s");
}

void
f_name(struct user *p, int argc, char **argv)
{
	if (!CAN_CHANGE_ROOM(p, p->super))
	{
		write_socket(p, "You do not own this room.\n");
		return;
	}
	COPY(p->super->name, argv[1], "room name");
	write_socket(p, "Ok.\n");
	store_room(p->super);
}

void
f_nosnoop(struct user *p, int argc, char **argv)
{
	write_socket(p, "You have %s your nosnoop flag.\n",
	    (p->saveflags ^= U_NOSNOOP) & U_NOSNOOP ? "set" : "unset");
}

void new_passwd(struct user *, char *);

void
new_passwd2(struct user *p, char *c)
{
	if (strcmp(p->stack.sp->u.string, c))
	{
		write_socket(p, "\nPasswords didn't match.\n");
		pop_stack(&p->stack);
		if (p->flags & U_LOOP_PASSWD)
		{
			p->input_to = new_passwd;
			write_prompt(p, "\nEnter new password: ");
			return;
		}
		echo(p);
		p->input_to = NULL_INPUT_TO;
		write_socket(p, "\n");
		return;
	}
	echo(p);
	p->input_to = NULL_INPUT_TO;
	pop_stack(&p->stack);
	COPY(p->passwd, crypted(c), "passwd");
	p->saveflags &= ~U_FPC;
	save_user(p, 1, 0);
	p->flags &= ~U_LOOP_PASSWD;
	write_socket(p, "\nOk.\n");
}

void
new_passwd(struct user *p, char *c)
{
	extern int secure_password(struct user *, char *, char *);
	int sp;

	if ((sp = secure_password(p, c, p->passwd)))
	{
		extern void insecure_passwd_text(struct user *, int);
		insecure_passwd_text(p, sp);

		if (p->flags & U_LOOP_PASSWD)
		{
			write_prompt(p, "\nEnter new password: ");
			return;
		}
		echo(p);
		write_socket(p, "\n");
		p->input_to = NULL_INPUT_TO;
		return;
	}

	push_string(&p->stack, c);
	p->input_to = new_passwd2;
	write_prompt(p, "\nReenter new password: ");
}

void
old_passwd(struct user *p, char *c)
{
	if (!strlen(c))
	{
		write_socket(p, "\n");
		echo(p);
		p->input_to = NULL_INPUT_TO;
		return;
	}
	if (strcmp(p->passwd, crypt(c, p->passwd)))
	{
		write_socket(p, "\nIncorrect password.\n");
		echo(p);
		p->input_to = NULL_INPUT_TO;
		return;
	}
	p->input_to = new_passwd;
	write_prompt(p, "\nEnter new password: ");
}

void
f_passwd(struct user *p, int argc, char **argv)
{
	CHECK_INPUT_TO(p)

	noecho(p);
	if (p->passwd != (char *)NULL && strlen(p->passwd))
	{
		p->input_to = old_passwd;
		write_prompt(p, "Enter old password: ");
		return;
	}
	p->flags &= ~U_LOOP_PASSWD;
	p->input_to = new_passwd;
	write_prompt(p, "Enter new password: ");
}

void
paste2(struct user *p, int i)
{
	if (i != EDX_NORMAL)
		return;
	write_roomabu(p, "%s pastes a file.\n", p->name);
	write_roomabu(p, "-- PASTE START --\n");
	write_roomabu(p, "%s", p->stack.sp->u.string);
	write_roomabu(p, "-- PASTE END --\n");
	write_socket(p, "Pasted..\n");
	pop_stack(&p->stack);
}

void
f_paste(struct user *p, int argc, char **argv)
{
	ed_start(p, paste2, 20, 0);
}

void
plan2(struct user *p, int i)
{
	if (i != EDX_NORMAL)
		return;
	FREE(p->plan);
	p->plan = p->stack.sp->u.string;
	dec_stack(&p->stack);
}

void
f_plan(struct user *p, int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "-r"))
	{
		FREE(p->plan);
		write_socket(p, "Plan removed.\n");
		return;
	}
	if (p->plan != (char *)NULL)
	{
		push_string(&p->stack, p->plan);
		ed_start(p, plan2, 10, ED_STACKED_TEXT);
	}
	else
		ed_start(p, plan2, 10, 0);
}

void
post2(struct user *p, int i)
{
	struct message *m;
	struct room *r;

	if (i != EDX_NORMAL)
	{
		write_roomabu(p, "%s aborts writing a note.\n", p->name);
		pop_n_elems(&p->stack, 2);
		return;
	}

	if (!strcmp(p->super->fname, (p->stack.sp - 2)->u.string))
		r = p->super;
	else if (!ROOM_POINTER(r, (p->stack.sp - 2)->u.string))
	{
		write_socket(p, "The room you were in has disappeared!.\n");
		pop_n_elems(&p->stack, 3);
		return;
	}
		
	if (!(r->flags & R_BOARD))
	{
		write_socket(p, "Board has dissapeared from this room!\n");
		return;
	}
	m = create_message();
	m->text = p->stack.sp->u.string;
	dec_stack(&p->stack);
	m->subject = p->stack.sp->u.string;
	dec_stack(&p->stack);
	pop_stack(&p->stack);
	COPY(m->poster, p->rlname, "message poster");
	m->date = current_time;
	add_message(r->board, m);
	store_board(r->board);
	notify_mbs(r->fname);
	p->postings++;
	write_roomabu(p, "%s finishes writing a note.\n", p->name);
}

void
epost_cleanup(struct user *p)
{
	struct room *r;

	if (ROOM_POINTER(r, p->atexit.sp->u.string))
		r->inhibit_cleanup--;
	pop_stack(&p->atexit);
}

void
epost3(struct user *p, int i)
{
	struct message *m;
	struct room *r;
	int roomc;

	roomc = i == EDX_NORMAL ? 4 : 3;

	/* We made it to here, so remove atexit function */
	pop_n_elems(&p->atexit, 2);

	if (!strcmp(p->super->fname, (p->stack.sp - roomc)->u.string))
		r = p->super;
	else if (!ROOM_POINTER(r, (p->stack.sp - roomc)->u.string))
	{
		write_socket(p, "The room you were in has disappeared!.\n");
		pop_n_elems(&p->stack, roomc + 1);
		return;
	}

	/* Remove memory-resident flag of room. */
	r->inhibit_cleanup--;

	if (i != EDX_NORMAL)
	{
		pop_n_elems(&p->stack, 4);
		return;
	}

	m = (struct message *)(p->stack.sp - 2)->u.pointer;
	/* Simple checks first.. */
	if (!(r->flags & R_BOARD))
	{
		write_socket(p, "Board has disappeared from this room!\n");
		pop_n_elems(&p->stack, 5);
		return;
	}
	/* The most common occurence will be that the note has not moved
	 * at all - try that first. */
	if (m != find_message(r->board, (p->stack.sp - 3)->u.number))
	{
		/* Ok.. let's scan all messages for a matching pointer. */
		struct message *n;

		for (n = r->board->messages; n != (struct message *)NULL;
		    n = n->next)
			if (n == m)
				break;
		write_socket(p, "Note has been removed.\n");
		pop_n_elems(&p->stack, 5);
		return;
	}
	/* Now m points to a valid message struct.. change it */
	FREE(m->text);
	m->text = p->stack.sp->u.string;
	m->subject = (p->stack.sp - 1)->u.string;
	/* Don't free the strings we've just copied. */
	dec_n_elems(&p->stack, 4);
	pop_stack(&p->stack);
	store_board(r->board);
}

void
epost2(struct user *p, char *c)
{
	if (strlen(c))
	{
		/* Replace the stacked subject... it is one down the stack */
		xfree((p->stack.sp - 1)->u.string);
		(p->stack.sp - 1)->u.string = string_copy(c, "epost2 subj");
	}
	p->input_to = NULL_INPUT_TO;
	ed_start(p, epost3, MAX_BOARD_LINES, ED_STACKED_TEXT);
}

void
f_post(struct user *p, int argc, char **argv)
{
	struct message *m;
	int num;

	if (!(p->super->flags & R_BOARD))
	{
		write_socket(p, "No board in this room.\n");
		return;
	}
	if (!CAN_WRITE_BOARD(p, p->super->board))
	{
		write_socket(p, "You may not post to this board.\n");
		return;
	}
	if (argv[0][0] != 'e')
	{
		write_roomabu(p,
		    "%s begins to write a note entitled '%s'\n",
		    p->name, argv[1]);
		push_string(&p->stack, p->super->fname);
		push_string(&p->stack, argv[1]);
		ed_start(p, post2, MAX_BOARD_LINES,
		    (p->medopts & U_EDOPT_SIG) ? ED_APPEND_SIG : 0);
		return;
	}
	if ((m = find_message(p->super->board, (num = atoi(argv[1])))) ==
	    (struct message *)NULL)
	{
		write_socket(p, "No such note, %s\n", argv[1]);
		return;
	}
	if (strcmp(m->poster, p->rlname) && (p->level < L_WARDEN ||
	    !iaccess_check(p, query_real_level(m->poster))))
	{
		write_socket(p, "You didn't post that note.\n");
		return;
	}
	write_roomabu(p, "%s begins to edit a note entitled '%s'\n",
	    p->name, m->subject);
	push_string(&p->stack, p->super->fname);
	push_number(&p->stack, num);
	push_pointer(&p->stack, (void *)m);
	push_string(&p->stack, m->subject);
	push_string(&p->stack, m->text);

	/* Make the room memory resident for the moment.. */
	p->super->inhibit_cleanup++;
	/* Set an exit function to reset the above flag if the user linkdies */
	push_string(&p->atexit, p->super->fname);
	push_fpointer(&p->atexit, epost_cleanup);

	write_prompt(p, "(Press return to leave as: %s)\nSubject: ",
	    m->subject);
	p->input_to = epost2;
}

void
f_prompt(struct user *p, int argc, char **argv)
{
	if (argc < 2)
	{
		if (p->prompt == (char *)NULL)
			write_socket(p, "Your prompt is not set.\n");
		else
			write_socket(p, "Your prompt is currently: %s\n",
			    p->prompt);
		if (p->saveflags & U_IACEOR)
			write_socket(p,
			    "IAC EOR prompt compatiblility turned on.\n");
		if (p->saveflags & U_IACGA)
			write_socket(p,
			    "IAC GA prompt compatilibility turned on.\n");
		return;
	}
	if (!strcmp(argv[1], "-default"))
	{
		COPY(p->prompt, PROMPT, "prompt");
		write_socket(p, "Prompt reset to default.\n");
		return;
	}
	if (!strcmp(argv[1], "-iaceor"))
	{
		write_socket(p, "IAC EOR prompt compatibility turned %s.\n",
		    (p->saveflags ^= U_IACEOR) & U_IACEOR ? "on" : "off");
		if (p->saveflags & U_IACEOR)
		{
			write_socket(p, "%c%c%c", IAC, WILL, TELOPT_EOR);
			p->socket.telnet.expect |= TN_EXPECT_EOR;
			if (p->saveflags & U_IACGA)
			{
				p->saveflags &= ~U_IACGA;
				write_socket(p, "%c%c%c", IAC, WILL,
				    TELOPT_SGA);
				p->socket.telnet.expect |= TN_EXPECT_SGA;
			}
		}
		return;
	}
	if (!strcmp(argv[1], "-iacga"))
	{
		write_socket(p, "IAC GA prompt compatibility turned %s.\n",
		    (p->saveflags ^= U_IACGA) & U_IACGA ? "on" : "off");
		if (p->saveflags & U_IACGA)
		{
			write_socket(p, "%c%c%c", IAC, WONT, TELOPT_SGA);
			p->socket.telnet.expect |= TN_EXPECT_SGA;
			/* Assume we may.. */
			p->socket.flags |= TCPSOCK_GA_OK;
			if (p->saveflags & U_IACEOR)
			{
				p->saveflags &= ~U_IACEOR;
				write_socket(p, "%c%c%c", IAC, WONT,
				    TELOPT_EOR);
				p->socket.telnet.expect |= TN_EXPECT_EOR;
			}
		}
		return;
	}
	if (strlen(argv[1]) > PROMPT_LEN)
	{
		write_socket(p, "Sorry, that prompt is too long!\n");
		return;
	}
	COPY(p->prompt, argv[1], "prompt");
	write_socket(p, "Ok.\n");
}

void
f_quotechar(struct user *p, int argc, char **argv)
{
	char q;

	if (argc < 2)
	{
		write_socket(p, "Quote character currently set to '%c'.\n",
		    p->quote_char);
		return;
	}

	if (strlen(argv[1]) > 1)
		write_socket(p,
		    "Warning: only first character is significant.\n");
	if (!isprint(q = *argv[1]))
	{
		write_socket(p,
		    "Character must be printable!\n");
		return;
	}
	p->quote_char = q;
	write_socket(p, "Quote character set to '%c'.\n", q);
}

void
f_remove(struct user *p, int argc, char **argv)
{
	int num;
	struct message *m;
	struct mbs *mb;

	if (!(num = atoi(argv[1])))
	{
		write_socket(p, "You must supply a note number.\n");
		return;
	}
	if (!CAN_WRITE_BOARD(p, p->super->board))
	{
		write_socket(p, "You may not write to this board.\n");
		return;
	}
	if ((m = find_message(p->super->board, num)) == (struct message *)NULL)
	{
		write_socket(p, "No such note, %s\n", argv[1]);
		return;
	}
	if (strcmp(m->poster, p->rlname) && (p->level < L_WARDEN ||
	    !iaccess_check(p, query_real_level(m->poster))))
	{
		write_socket(p, "You didn't post that note.\n");
		return;
	}
	if (!strcmp(p->rlname, m->poster))
		p->postings--;
	write_roomabu(p, "%s removes a note entitled '%s'\n",
	    p->name, m->subject);
	remove_message(p->super->board, m);
	store_board(p->super->board);
	/* Mbs stuff */
	if ((mb = find_mbs_by_room(p->super->fname)) != (struct mbs *)NULL)
	{
		mb->last = mbs_get_last_note_time(p->super->board);
		store_mbs();
	}
	write_socket(p, "Note removed.\n");
}

void
f_save(struct user *p, int argc, char **argv)
{
	if (save_user(p, 1, 0))
		write_socket(p, "Ok.\n");
	else
		write_socket(p, "Could not save you!\n"
		    "This is probably due to either your password or email\n"
		    "address being unset. If you have any queries, please\n"
		    "contact an administrator or use the 'assist' command.\n");
}

void
sched2(struct event *ev)
{
	struct user *p;
	int unbuf;
	char *cmd;
	int quiet, qi;
	int i;

	if ((p = find_user_absolute((struct user *)NULL,
	    ev->stack.sp->u.string)) == (struct user *)NULL)
	{
		/* User has logged off */
		pop_n_elems(&ev->stack, 2);
		return;
	}
	for (i = 0; i < NUM_JOBS; i++)
		if ((p->jobs[i].flags & JOB_USED) && p->jobs[i].event == ev->id)
			break;
	if (i >= NUM_JOBS)
	{
		/* The job has gone.. */
		pop_n_elems(&ev->stack, 2);
		return;
	}
	quiet = p->jobs[i].flags & JOB_QUIET;
	qi = (p->jobs[i].flags & JOB_INHIBIT_QUIT) &&
	    !(p->flags & U_INHIBIT_QUIT);
	p->jobs[i].flags = 0;
	pop_stack(&ev->stack);
	cmd = ev->stack.sp->u.string;
	dec_stack(&ev->stack);

	if (qi)
		p->flags |= U_INHIBIT_QUIT;

	unbuf = !!(p->flags & U_UNBUFFERED_TEXT);
	p->flags |= U_UNBUFFERED_TEXT;
	reset_eval_depth();
	parse_command(p, &cmd);

	if (!quiet)
		write_socket(p, "[%d] Done        %s\n", i + 1, cmd);
	xfree(cmd);

	if (qi)
		p->flags &= ~U_INHIBIT_QUIT;

	if (!unbuf)
		p->flags &= ~U_UNBUFFERED_TEXT;
}

void
f_sched(struct user *p, int argc, char **argv)
{
	int delay;
	int i;
	int quiet;
	struct event *ev;

	deltrail(argv[1]);

	if (!strcmp(argv[1], "-q"))
	{
		if (argc < 4)
		{
			write_socket(p, "Syntax: sched -q <time> <command>\n");
			return;
		}
		quiet = 1;
		argv++, argc--;
		deltrail(argv[1]);
	}
	else
		quiet = 0;

	if (*argv[1] == '@')
	{	/* Absolute time.. this is a pain. */
		struct tm then;
		int hour, min;
		time_t tim;

		if (sscanf(argv[1] + 1, "%d:%d", &hour, &min) != 2)
		{
			write_socket(p, "Invalid absolute time, %s; "
			    "use @hour:minute\n", argv[1]);
			return;
		}

		/* Get a broken down copy of the current time */

		memcpy((void *)&then, localtime(&current_time),
		    sizeof(struct tm));

		/* Set the hour and minute fields.. */
		then.tm_hour = hour;
		then.tm_min = min;
		then.tm_sec = 0;

		if ((tim = mktime(&then)) == (time_t)-1)
		{
			write_socket(p, "Bad absolute time: %s\n", argv[1]);
			return;
		}

		/* Have we gone back in time ? */

		if (tim < current_time)
			tim += 3600 * 24;

		delay = tim - current_time;
	}
	else if ((delay = atoi(argv[1])) < 1)
	{
		write_socket(p, "Invalid delay: %s\n", argv[1]);
		return;
	}
	for (i = 0; i < NUM_JOBS; i++)
		if (!(p->jobs[i].flags & JOB_USED))
			break;
	if (i >= NUM_JOBS)
	{
		write_socket(p, "All job slots in use.\n");
		return;
	}

	ev = create_event();
	push_string(&ev->stack, argv[2]);
	push_string(&ev->stack, p->rlname);
	add_event(ev, sched2, delay, "sched");
	p->jobs[i].event = ev->id;
	p->jobs[i].flags = JOB_USED | (quiet ? JOB_QUIET : 0);
	if (p->flags & U_INHIBIT_QUIT)
		p->jobs[i].flags |= JOB_INHIBIT_QUIT;
	if (!quiet)
		write_socket(p, "[%d] %d\n", i + 1, ev->id);
}

void
f_sendme(struct user *p, int argc, char **argv)
{
	if (argc < 2)
	{
		dump_file(p, F_SENDME, F_INDEX, DUMP_MORE);
		return;
	}

	if (!(p->saveflags & U_EMAIL_VALID))
	{
		write_socket(p, "You must have your email address validated "
		    "before you can use the 'sendme' command.\n"
		    "Contact an administrator or send an email request to: "
		    OVERSEER_EMAIL);
		return;
	}

	if (strchr(argv[1], '/') != (char *)NULL)
	{
		/* Naughty, naughty.. */
		notify_level(L_CONSUL,
		    "[ %s tried to read outside the lib (%s) ]",
		    p->capname, argv[1]);
		log_file("illegal", "%s: sendme: %s", p->capname, argv[1]);
		write_socket(p, "Email request failed. No such file ?\n");
		return;
	}

	if (send_email_file(p, argv[1]) != -1)
	{
		write_socket(p, "Email request submitted.\n");
		log_file("sendme", "%s (%s)", p->rlname, argv[1]);
	}
	else
		write_socket(p, "Email request failed. No such file ?\n");
}

void
f_shout(struct user *p, int argc, char **argv)
{
	struct user *u;

	if (p->saveflags & U_SHOUT_CURSE)
	{
		write_socket(p,
		    "Your throat dries up as you attempt to shout.\n");
		return;
	}
	if (p->earmuffs & EAR_SHOUTS)
	{
		write_socket(p, "You are blocking shouts.\n");
		return;
	}
	for (u = users->next; u != (struct user *)NULL; u = u->next)
		if (IN_GAME(u) && !(u->earmuffs & EAR_SHOUTS) &&
		    p != u)
		{
			attr_colour(u, "shout");
			write_socket(u, "%s shouts: %s", p->name,
	    		    argv[1]);
			reset(u);
			write_socket(u, "\n");
		}
	write_socket(p, "You shout: %s\n", argv[1]);
}

void
sig2(struct user *p, int i)
{
	if (i != EDX_NORMAL)
		return;
	FREE(p->sig);
	p->sig = p->stack.sp->u.string;
	dec_stack(&p->stack);
}

void
f_sig(struct user *p, int argc, char **argv)
{
	if (argc > 1)
	{
		if (!strcmp(argv[1], "-edit"))
		{
			if (p->sig != (char *)NULL)
			{
				push_string(&p->stack, p->sig);
				/* The FORCE_WRAP means that users can't
				 * have lines longer than 75 characters. */
				ed_start(p, sig2, 4,
				    ED_STACKED_TEXT | ED_FORCE_WRAP);
			}
			else
			{
				/* Defaults */
				p->medopts |= (U_MAILOPT_SIG | U_EDOPT_SIG);
				ed_start(p, sig2, 4, ED_FORCE_WRAP);
			}
			return;
		}

		if (!strcmp(argv[1], "-remove"))
		{
			FREE(p->sig);
			write_socket(p, "Sig removed.\n");
			p->medopts &= ~(U_EDOPT_SIG | U_MAILOPT_SIG);
			return;
		}

		if (!strcmp(argv[1], "-mail"))
		{
			if ((p->medopts ^= U_MAILOPT_SIG) & U_MAILOPT_SIG)
				write_socket(p,
				    "Your signature will be appended"
				    " to mail.\n");
			else
				write_socket(p,
				    "Your signature will no longer be appended"
				    " to mail.\n");
			return;
		}

		if (!strcmp(argv[1], "-board"))
		{
			if ((p->medopts ^= U_EDOPT_SIG) & U_EDOPT_SIG)
				write_socket(p,
				    "Your signature will be appended"
				    " to board messages.\n");
			else
				write_socket(p,
				    "Your signature will no longer be appended"
				    " to board messages.\n");
			return;
		}
		write_socket(p, "Sig: unknown argument.\n");
		return;
	}

	if (p->sig == (char *)NULL)
		write_socket(p, "Your signature is not currently set.\n");
	else
		write_socket(p, "%s\n", p->sig);
	if (p->medopts & U_EDOPT_SIG)
		write_socket(p, "Signature is appended to board messages.\n");
	if (p->medopts & U_MAILOPT_SIG)
		write_socket(p, "Signature is appended to mail messages.\n");
}

void
f_start(struct user *p, int argc, char **argv)
{
	struct room *r;

	if (argc > 1 && p->level > L_CITIZEN)
	{
		if (!ROOM_POINTER(r, argv[1]))
		{
			write_socket(p, "Room %s does not exist.\n", argv[1]);
			return;
		}
	}
	else
		r = p->super;

	COPY(p->startloc, r->fname, "startloc");
	write_socket(p, "Start location set to: %s.\n", r->name);
}

void
f_sticky(struct user *p, int argc, char **argv)
{
	struct user *who;
	char fname[MAXPATHLEN + 1];
	char *buf;

	if (argc < 2)
	{
		sprintf(fname, "%s#sticky", p->rlname);
		if (!dump_file(p, "mail", fname, DUMP_MORE))
			write_socket(p, "You have no sticky notes.\n");
		return;
	}

	if (argc < 3)
	{
		write_socket(p, "Syntax: sticky <user> <message>\n");
		return;
	}

	deltrail(argv[1]);

	argv[1] = lower_case(argv[1]);

	if (!exist_user(argv[1]))
	{
		write_socket(p, "User not found, %s.\n", capitalise(argv[1]));
		return;
	}

	who = find_user_absolute((struct user *)NULL, argv[1]);

	sprintf(fname, "mail/%s#sticky", argv[1]);

	/* The 30 is for '[ <time> ]' - Added 5 for newlines & end of string */
	buf = (char *)xalloc(35 + strlen(p->name) + strlen(argv[2]),
	    "sticky buf");
	sprintf(buf, "[ %s : %s ]\n%s\n", nctime(&current_time), p->name,
	    argv[2]);

	if (!append_file(fname, buf))
	{
		write_socket(p, "Could not write to sticky note file.\n");
		xfree(buf);
		return;
	}

	xfree(buf);

	if (who != (struct user *)NULL)
	{
		attr_colour(who, "notify");
		write_socket(who, "[ New sticky note from %s. ]", p->name);
		reset(who);
		write_socket(who, "\n");
	}

	write_socket(p, "Posted a sticky note to %s.\n", capitalise(argv[1]));
}

void
f_sudo(struct user *p, int argc, char **argv)
{
	char *cmd;
	int olevel;

	if (argc < 2)
	{
		list_sudos(p, p->rlname);
		return;
	}

	cmd = string_copy(argv[1], "sudo tmp");
	deltrail(cmd);
	if (!check_sudo(p->rlname, cmd, argc > 2 ? argv[2] : (char *)NULL))
	{
		write_socket(p, "Permission denied.\n");
		log_file("secure/sudo", "%s - (%s)", p->capname,
		    argv[1]);
		notify_levelabu(p, p->level >= L_CONSUL ? p->level : L_CONSUL,
		    "[ SUDO: - %s: %s ]", p->capname, argv[1]);
		xfree(cmd);
		return;
	}

	log_file("secure/sudo", "%s + (%s)", p->capname, argv[1]);
	notify_levelabu(p, p->level >= L_CONSUL ? p->level : L_CONSUL,
	    "[ SUDO: + %s: %s ]", p->capname, argv[1]);

	strcpy(cmd, argv[1]);
	p->flags |= U_SUDO;
	olevel = p->level;
	p->level = L_OVERSEER;

	p->flags |= U_INHIBIT_ALIASES;
	parse_command(p, &cmd);
	p->flags &= ~U_INHIBIT_ALIASES;

	xfree(cmd);

	p->flags &= ~U_SUDO;
	if ((p->level = query_real_level(p->rlname)) == -1)
		p->level = olevel;
}

void
suicide3(struct user *p, char *c)
{
	p->input_to = NULL_INPUT_TO;
	if (strcmp(c, "yes"))
	{
		write_socket(p, "You had a lucky escape.\n");
		log_file("suicide", "* %s (%s)", p->capname,
		    lookup_ip_name(p));
		return;
	}
	write_socket(p, "Goodbye.............\n"
	                "         .................for ever!\n");
	log_file("suicide", "+ %s (%s)", p->capname, lookup_ip_name(p));
	disconnect_user(p, 0);
	delete_user(p->rlname);
	notify_levelabu(p, p->level, "[ %s has committed suicide. ]",
	    p->capname);
	write_roomabu(p, "%s has committed suicide.\n", p->name);
}

void
suicide2(struct user *p, char *c)
{
	echo(p);
	if (p->passwd && strlen(p->passwd) &&
	    !strcmp(p->passwd, crypt(c, p->passwd)))
	{
		write_prompt(p, "\n\n!!! THIS IS YOUR LAST WARNING !!!\n"
		    "TYPE 'yes' AT THE FOLLOWING PROMPT TO REMOVE THIS\n"
		    "CHARACTER PERMANENTLY\n\n: ");
		p->input_to = suicide3;
	}
	else
	{
		write_socket(p, "\nIncorrect password.\n");
		log_file("suicide", "- %s (%s)", p->capname,
		    lookup_ip_name(p));
		p->input_to = NULL_INPUT_TO;
	}
}

void
f_suicide(struct user *p, int argc, char **argv)
{
	if (p->level >= A_NO_SUICIDE)
	{
		write_socket(p, "Permission denied.\n");
		return;
	}
	CHECK_INPUT_TO(p)
	noecho(p);
	write_prompt(p, "You have chosen to delete this user permanently.\n"
	    "This can NOT be undone!\n"
	    "Enter your password for confirmation: ");
	p->input_to = suicide2;
}

void
f_swho(struct user *p, int argc, char **argv)
{
	struct strbuf str;
	struct vecbuf vb;
	struct vector *v;
	int i;

	init_vecbuf(&vb, 0, "f_swho vecbuf");
	expand_user_list(p, argv[1], &vb, 1, 0, 0);

	v = vecbuf_vector(&vb);

	/* This will only happen if a grupe name was supplied. */
	if (!v->size)
		write_socket(p, "%s: No members logged on.\n", argv[1]);
	else
	{
		init_strbuf(&str, 0, "f_swho grupe sb");
		init_composite_words(&str);

		for (i = v->size; i--; )
			if (v->items[i].type == T_POINTER)
				add_composite_word(&str,
				    ((struct user *)(v->items[i].u.pointer))
				    ->name);
		end_composite_words(&str);
		write_socket(p, "%s\n", str.str);
		free_strbuf(&str);
	}
	free_vector(v);
}

void
f_title(struct user *p, int argc, char **argv)
{
	if (strlen(argv[1]) > TITLE_LEN)
	{
		write_socket(p, "That title is too long.\n");
		return;
	}
	if (strchr(argv[1], '\n') != (char *)NULL)
	{
		write_socket(p, "No embedded newlines allowed in a title.\n");
		return;
	}
	COPY(p->title, argv[1], "title");
	write_socket(p, "Ok.\n");
}

void
f_unalias(struct user *p, int argc, char **argv)
{
	struct alias *a;
	struct alias **list;
	int r = 0;

	if (*argv[0] == 'r')
	{
		r = 1;
		list = &p->super->alias;
	}
	else if (*argv[0] == 'g')
	{
		list = &galiases;
		r = 2;
	}
	else
		list = &p->alias;

	if ((a = find_alias(*list, argv[1])) == (struct alias *)NULL)
	{
		if (strcmp(argv[1], "-a"))
		{
			write_socket(p, "%s is not aliased.\n", argv[1]);
			return;
		}
		else
		{
			free_aliases(list);
			write_socket(p, "Removed all aliases.\n");
			return;
		}
	}
	remove_alias(list, a);
	write_socket(p, "Unaliased '%s'\n", argv[1]);
	if (r == 1)
		store_room(p->super);
	else if (r == 1)
		store_galiases();
}

void
f_unlink(struct user *p, int argc, char **argv)
{
	struct exit *e;

	if (!CAN_CHANGE_ROOM(p, p->super))
        {
                write_socket(p, "You do not own this room.\n");
                return;
        }

	if (p->level < L_CONSUL && p->super->fname[0] == '_')
	{
		write_socket(p,
		    "You may not remove exits from system rooms.\n");
		return;
	}

	for (e = p->super->exit; e != (struct exit *)NULL; e = e->next)
	{
		if (!strcmp(argv[1], e->name))
		{
			remove_exit(p->super, e);
			write_socket(p, "Unlinked.\n");
			store_room(p->super);
			return;
		}
	}
	write_socket(p, "No such exit.\n");
}

void
f_unlock(struct user *p, int argc, char **argv)
{
	if (!CAN_CHANGE_ROOM(p, p->super))
	{
		write_socket(p, "You don't own this room.\n");
		return;
	}
	if (p->level > L_WARDEN && argc > 1)
	{
		if (!strcmp(argv[1], "grupe"))
		{
			if (p->super->lock_grupe != (char *)NULL)
			{
				FREE(p->super->lock_grupe);
				store_room(p->super);
				write_socket(p, "Environment is no longer "
				    "grupe locked.\n");
			}
			else
				write_socket(p,
				    "Environment is not grupe locked.\n");
		}
		else
			write_socket(p, "Unknown argument to %s.\n", argv[0]);
		return;
	}
	if (!(p->super->flags & R_LOCKED))
	{
		write_socket(p, "This room was already unlocked.\n");
		return;
	}
	p->super->flags ^= R_LOCKED;
	store_room(p->super);
	write_socket(p, "This room is now unlocked.\n");
}

void
f_url(struct user *p, int argc, char **argv)
{
	if (argc < 2)
	{
		if (p->url != (char *)NULL)
			write_socket(p, "Your homepage url is set to: %s\n",
			    p->url);
		else
			write_socket(p, "Your homepage url is not set.\n");
		write_socket(p, "To set or change your url address, type:\n\t"
		    "url <address>\n"
		    "To remove your address use 'url -r'\n");
		return;
	}
	if (!strcmp(argv[1], "-r"))
	{
		FREE(p->url);
		notify_levelabu(p, p->level >= L_WARDEN ? p->level : L_WARDEN,
		    "[ %s has removed %s homepage url. ]",
		    p->capname, query_gender(p->gender, G_POSSESSIVE));
		write_socket(p, "Your homepage url has been removed.\n");
		return;
	}

	/* _very_ simple check - perhaps add more later. */
	if (strncmp(argv[1], "http://", 7) ||
	    strchr(argv[1] + 8, '.') == (char *)NULL ||
	    strchr(argv[1] + 8, '/') == (char *)NULL)
	{
		write_socket(p,
		    "That does not appear to be a valid url string.\n");
		return;
	}
	notify_levelabu(p, p->level >= L_WARDEN ? p->level : L_WARDEN,
	    "[ %s has changed %s homepage url to %s. ]",
	    p->capname, query_gender(p->gender, G_POSSESSIVE), argv[1]);
	COPY(p->url, argv[1], "url");
	write_socket(p, "You have changed your homepage url to: %s\n",
	    argv[1]);
}

void
f_which(struct user *p, int argc, char **argv)
{
	struct command *cmd;
	struct alias *a;
	struct sent *s;

	/* It could be a global alias.. */
	if ((a = find_alias(galiases, argv[1])) != (struct alias *)NULL)
	{
		write_socket(p, "%s: global aliased to %s\n", argv[1], a->fob);
		return;
	}

	/* Perhaps a normal alias.. */
	if ((a = find_alias(p->alias, argv[1])) != (struct alias *)NULL)
	{
		write_socket(p, "%s: aliased to %s\n", argv[1], a->fob);
		return;
	}

	/* Perhaps a room alias.. */
	if ((a = find_alias(p->super->alias, argv[1])) != (struct alias *)NULL)
	{
		write_socket(p, "%s: room aliased to %s\n", argv[1], a->fob);
		return;
	}

	/* Perhaps a sentence.. */
	if ((s = find_sent_cmd(p, argv[1])) != (struct sent *)NULL)
	{
		char buf[0x100];

		strcpy(buf, obj_name(s->ob));

		write_socket(p, "%s: Sentence in %s [%s]\n", argv[1],
		    s->ob->type == OT_JLM ? obj_name(s->ob->super) : "<..>",
		    buf);
		return;
	}

	/* A normal command ?? */
	if ((cmd = find_command(p, argv[1])) != (struct command *)NULL)
	{
		write_socket(p, "%s: %s command.\n", argv[1],
		    capfirst(level_name(cmd->level, 0)));
		if (cmd->syntax)
			write_socket(p, "\tSyntax: %s\n", cmd->syntax);
		write_socket(p, "\tMinimum arguments: %d\n", cmd->args);
		if (cmd->flags & CMD_PARTIAL)
			write_socket(p, "\tPartial command.\n");
		if (cmd->flags & CMD_AUDIT)
			write_socket(p, "\tAudited command.\n");
#ifdef PROFILING
		write_socket(p, "\tTimes used: %d.\n", cmd->calls);
#endif
		return;
	}

	/* A feeling ? */
	if (exist_feeling(argv[1]) != (struct feeling *)NULL)
	{
		extern void f_feelings(struct user *, int, char **);

		write_socket(p, "%s: feeling command.\n", argv[1]);
		f_feelings(p, argc, argv);
		return;
	}

	/* I give up! */
	write_socket(p, "%s: not found.\n", argv[1]);
}

void
f_whisper(struct user *p, int argc, char **argv)
{
	struct user *who;

	if ((who = with_user_msg(p, argv[1])) == (struct user *)NULL)
		return;
	if (who == p)
	{
		write_socket(p, "You want to talk to yourself ??\n");
		return;
	}
	write_socket(p, "You whisper to %s: %s.\n", who->name, argv[2]);
	attr_colour(who, "incoming");
	write_socket(who, "%s whispers to you: %s.", p->name, argv[2]);
	reset(who);
	write_socket(who, "\n");
	write_roomabu2(p, who, "%s whispers something to %s.\n",
	    p->name, who->name);
}

#ifdef IMUD3_SUPPORT
void
f_3channel(struct user *p, int argc, char **argv)
{
	i3_cmd_channel(p, argv[1], argv[2]);
}

void
f_3hosts(struct user *p, int argc, char **argv)
{
	extern struct i3_host *i3hosts;
	struct i3_host *i;
	struct strbuf h;
	int column;
	int tot, up;

	init_strbuf(&h, 0, "3hosts buf");

	if (argc > 1)
	{
		int len = strlen(argv[1]);

		for (i = i3hosts; i != (struct i3_host *)NULL; i = i->next)
			if (!strncasecmp(argv[1], i->name, len))
			{
				struct svalue *sv;
				int flag, j;

				sadd_strbuf(&h,
				    "Name    : %-25s   Type    : %s\n",
				    i->name, i->mud_type);
				sadd_strbuf(&h,
				    "Driver  : %-25s   Ports   : "
				    "UDP %d, TCP %d\n", i->driver,
				    i->imud_tcp_port, i->imud_udp_port);
				sadd_strbuf(&h,
				    "Mudlib  : %-25s   Base    : %s\n",
				    i->mudlib, i->base_mudlib);
				sadd_strbuf(&h,
				    "          %s\n", i->open_status);
				add_strbuf(&h,
				    "Status  : ");
				if (i->state == -1)
					add_strbuf(&h, "UP\n");
				else if (i->state == 0)
					add_strbuf(&h, "DOWN\n");
				else
					sadd_strbuf(&h,
					    "DOWN but expected back in %s\n",
					    conv_time(i->state));
				sadd_strbuf(&h,
				    "Email   : %s\n", i->admin_email);
				sadd_strbuf(&h,
				    "Address : %-17s %-5d\n",
				    i->ip_addr, i->player_port);

				if ((sv = map_string(i->services, "ftp")) !=
				    (struct svalue *)NULL &&
				    sv->type == T_NUMBER)
					sadd_strbuf(&h, "FTP Server port: %d\n",
					    sv->u.number);

				if ((sv = map_string(i->services, "http")) !=
				    (struct svalue *)NULL &&
				    sv->type == T_NUMBER)
					sadd_strbuf(&h, "WWW Server port: %d\n",
					    sv->u.number);

				add_strbuf(&h, "Services: ");

				for (flag = j = 0; j < i->services->size; j++)
				{
					if (i->services->indices->items[j].type
					    == T_STRING &&
					    strcmp(i->services->indices->
					    items[j].  u.string, "ftp") &&
					    strcmp(i->services->indices->
					    items[j].u.string, "http"))
					{
						if (flag)
							add_strbuf(&h, ", ");
						else
							flag = 1;
						add_strbuf(&h,
						    i->services->indices->
						    items[j].u.string);
					}
				}
				add_strbuf(&h, "\n\n");
			}

		if (!h.offset)
		{
			write_socket(p, "No hosts found matching: %s\n",
			    argv[1]);
			free_strbuf(&h);
		}
		else
		{
			pop_strbuf(&h);
			more_start(p, h.str, NULL);
		}
		return;
	}

	tot = up = column = 0;

	for (i = i3hosts; i != (struct i3_host *)NULL; i = i->next)
	{
#define TRUNC_LEN 16
		char trunc_name[TRUNC_LEN + 1];

		my_strncpy(trunc_name, i->name, TRUNC_LEN);
#undef TRUNC_LEN

		sadd_strbuf(&h, "%c%-17s", i->state == -1 ? 
		    (i->is_jl ? '!' : '*') : 
		    (i->state > 0 ? '-' : ' '), trunc_name);
		if (column++ > 2)
		{
			column = 0;
			cadd_strbuf(&h, '\n');
		}
		tot++;
		if (i->state == -1)
			up++;
	}
	write_socket(p, "Currently listed hosts [total:%d up:%d down:%d]\n",
	    tot, up, tot - up);
	pop_strbuf(&h);
	more_start(p, h.str, NULL);
}
#endif /* IMUD3_SUPPORT */

