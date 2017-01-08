/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	cmd.consul.c
 * Function:	Consul commands.
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#ifdef HPUX
#include <time.h>
#endif
#include <sys/time.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "jeamland.h"

extern time_t current_time;
extern struct user *users;

/*
 * CONSUL commands....
 */

void
f_as(struct user *p, int argc, char **argv)
{
	int newlev;
	char *cmd;

	if ((newlev = level_number(argv[1])) == -1 ||
	    newlev < L_VISITOR || newlev >= p->level)
	{
		write_socket(p, "Invalid level %s\n", argv[1]);
		return;
	}

	cmd = string_copy(argv[2], "f_as cmd");
	write_socket(p, "Executing as %s: %s\n", level_name(newlev, 0), cmd);
	p->level = newlev;
	parse_command(p, &cmd);
	p->level = (p->level = query_real_level(p->rlname)) == -1 ?
	    L_VISITOR : p->level;
	xfree(cmd);
}

void
f_at(struct user *p, int argc, char **argv)
{
	struct user *who;
	struct room *old, *r;
	char *cd;

	if (!strcmp(argv[0], "at"))
	{
		if ((who = find_user_msg(p, argv[1])) == (struct user *)NULL)
			return;
		r = who->super;
	}
	else	/* in */
	{
		if (!ROOM_POINTER(r, argv[1]))
		{
			write_socket(p, "No such room, %s.\n",
			    capitalise(argv[1]));
			return;
		}
	}
		
	if ((r->flags & R_LOCKED) &&
	    strcmp(r->owner, p->rlname) && (p->level < L_OVERSEER ||
	    !iaccess_check(p, query_real_level(r->owner))))
	{
		write_socket(p, "Cannot move you to that room.\n");
		return;
	}
	old = p->super;
	move_object(p->ob, r->ob);
	cd = string_copy(argv[2], "f_at tmp");
	parse_command(p, &cd);
	xfree(cd);
	move_object(p->ob, old->ob);
}

void
f_board(struct user *p, int argc, char **argv)
{
	if (!CAN_CHANGE_ROOM(p, p->super))
	{
		write_socket(p, "You do not own this room.\n");
		return;
	}
	deltrail(argv[1]);
	if (!strcmp(argv[1], "-add"))
	{
		struct mbs *m;

		if (p->super->flags & R_BOARD)
		{
			write_socket(p, "This room already has a board.\n");
			return;
		}
		p->super->flags |= R_BOARD;
		p->super->board = restore_board("board", p->super->fname);
		write_socket(p, "Ok.\n");
		store_room(p->super);

		/* Mbs stuff
		 * If this room is in mbs, set the last-note time */
		if ((m = find_mbs_by_room(p->super->fname)) !=
		    (struct mbs *)NULL)
		{
			m->last = mbs_get_last_note_time(p->super->board);
			store_mbs();
		}
		return;
	}
	else if (!(p->super->flags & R_BOARD))
	{
		write_socket(p, "No board in this room.\n");
		return;
	}
	else if (!strcmp(argv[1], "-remove"))
	{
		p->super->flags &= ~R_BOARD;
		free_board(p->super->board);
		p->super->board = (struct board *)NULL;
		store_room(p->super);
		write_socket(p, "Board removed.\n");
		return;
	}
	else if (!strcmp(argv[1], "-reload"))
	{
		free_board(p->super->board);
		p->super->board = restore_board("board", p->super->fname);
		write_socket(p, "Board reloaded.\n");
		return;
	}
	else if (!strcmp(argv[1], "-archive"))
	{
		if (argc > 2)
		{
			COPY(p->super->board->archive, argv[2],
			    "board archive");
			write_socket(p, "Archive board set to: %s\n",
			    argv[2]);
		}
		else
		{
			if (p->super->board->archive != (char *)NULL)
			{	
				FREE(p->super->board->archive);
				write_socket(p, "Archive board removed.\n");
			}
			else
			{
				write_socket(p, "Archive board is not set.\n");
				return;
			}
		}
	}
	else if (!strcmp(argv[1], "-followup"))
	{
		if (argc > 2)
		{
			COPY(p->super->board->followup, argv[2],
			    "board followup");
			write_socket(p, "Followup board set to: %s\n",
			    argv[2]);
		}
		else
		{
			if (p->super->board->followup != (char *)NULL)
			{	
				FREE(p->super->board->followup);
				write_socket(p, "Followup board removed.\n");
			}
			else
			{
				write_socket(p, "Followup board is not set.\n");
				return;
			}
		}
	}
	else if (!strcmp(argv[1], "-limit"))
	{
		int lim;

		if (argc < 3)
		{
			write_socket(p, "Syntax: board limit <new limit>\n");
			return;
		}

		lim = atoi(argv[2]);

		if ((lim != -1 && lim < 5) || lim > 500)
		{
			write_socket(p, "Silly limit.\n");
			return;
		}

		if (p->super->board->num > lim)
		{
			write_socket(p,
			    "Board contains %d messages.\n"
			    "Cannot change the limit to %d.\n",
			    p->super->board->num, lim);
			return;
		}

		write_socket(p, "Board limit set to: %d.\n", lim);
		p->super->board->limit = lim;
	}
	else if (!strcmp(argv[1], "-write"))
	{
		if (argc < 3)
		{
			if (p->super->board->write_grupe != (char *)NULL)
			{
				FREE(p->super->board->write_grupe);
				write_socket(p, "Write grupe removed.\n");
			}
			else
			{
				write_socket(p, "No write grupe set.\n");
				return;
			}
		}
		else
		{
			if (*argv[2] == '#')
				argv[2]++;
			COPY(p->super->board->write_grupe, argv[2],
			    "write grupe");
			write_socket(p,
			    "Board write grupe set to #%s.\n", argv[2]);
		}
	}
	else if (!strcmp(argv[1], "-read"))
	{
		if (argc < 3)
		{
			if (p->super->board->read_grupe != (char *)NULL)
			{
				FREE(p->super->board->read_grupe);
				write_socket(p, "Read grupe removed.\n");
			}
			else
			{
				write_socket(p, "No read grupe set.\n");
				return;
			}
		}
		else
		{
			if (*argv[2] == '#')
				argv[2]++;
			COPY(p->super->board->read_grupe, argv[2],
			    "read grupe");
			write_socket(p, "Board read grupe set to #%s.\n",
			    argv[2]);
		}
	}
	else if (!strcmp(argv[1], "-anon"))
	{
		write_socket(p, "Board is %s marked anonymous.\n",
		    (p->super->board->flags ^= B_ANON) & B_ANON ?
		    "now" : "no longer");
	}
	else
	{
		write_socket(p, "Board command not understood.\n");
		return;
	}
	store_board(p->super->board);
	return;
}

void
f_chgender(struct user *p, int argc, char **argv)
{
	struct user *who;
	int newgender;

	if (argc < 3)
	{
		write_socket(p, "Use 'chgender <user> <gender>' to alter.\n"
		    "Abbreviated genders are accepted.\n"
		    "Available genders are: %s\n"
		    "Or '-1' to force the user to choose at next login.\n",
		    gender_list());
		return;
	}
	deltrail(argv[1]);
	if ((newgender = gender_number(argv[2], 1)) == -1)
	{
		/* Allow setting gender to -1 ie. user sets it next login */
		if ((newgender = atoi(argv[2])) != -1)
		{
			write_socket(p, "Invalid gender, %s\n", argv[2]);
			return;
		}
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

	if (IN_GAME(who))
	{
		if (newgender == -1)
		{
			write_socket(p, "Cannot set gender to -1 on an "
			    "interactive user.\n");
			doa_end(who, 0);
			return;
		}
		write_socket(who, "%s has changed your gender to %s.\n",
		    p->name, query_gender(newgender, G_ABSOLUTE));
	}

	notify_level(p->level, "[ CHGENDER by %s: %s -> %s  ]",
	    p->capname, who->capname, query_gender(newgender, G_ABSOLUTE));

	who->gender = newgender;
	doa_end(who, 1);
	write_socket(p, "Ok.\n");
}

void
f_chlev(struct user *p, int argc, char **argv)
{
	struct user *who;
	int oldlev, newlev;
	char *q, *r;

	newlev = atoi(argv[2]);
	if ((newlev <= L_VISITOR || newlev > L_MAX) &&
	    (newlev = level_number(argv[2])) == -1)
	{
		write_socket(p, "Invalid level %s\n", argv[2]);
		return;
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
	if (newlev >= p->level && !ISROOT(p))
	{
		doa_end(who, 0);
		write_socket(p, "Permission denied.\n");
		return;
	}
	if (who->level == L_VISITOR)
	{
		write_socket(p, "%s is a guest.\n", who->capname);
		doa_end(who, 0);
		return;
	}
	if (who == p)
	{
		write_socket(p, "You may not change your own level!\n");
		doa_end(who, 0);
		return;
	}
	oldlev = query_real_level(who->rlname);
	if (oldlev == -1)
		oldlev = L_RESIDENT;
	if (oldlev == newlev)
	{
		write_socket(p, "%s is already at that level.\n",
		    who->capname);
		doa_end(who, 0);
		return;
	}
	change_user_level(who->rlname, newlev, p->rlname);
	q = lower_case(level_name(oldlev, 0));
	r = lower_case(level_name(newlev, 0));
	if (IN_GAME(who))
	{
		who->level = newlev;
		write_socket(who,
		    "Your level has been changed from %s to %s by %s\n",
		    q, r, p->name);
		reposition(who);
	}
	log_file("level", "%s, %s(%d) -> %s(%d) by %s", who->capname,
	    q, oldlev, r, newlev, p->capname);
	write_socket(p, "Ok.\n");
	notify_level(p->level, "[ CHLEV by %s: %s (%s(%d) -> %s(%d)) ]",
	    p->capname, who->capname, q, oldlev, r, newlev);
	doa_end(who, 0);
}

void
f_chlim(struct user *p, int argc, char **argv)
{
	struct user *who;
	int lim;

	lim = atoi(argv[3]);

	if (lim != -1 && (lim < 5 || lim > 100))
	{
		write_socket(p, "Silly limit.\n");
		return;
	}

	deltrail(argv[1]);
	deltrail(argv[2]);

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

	if (!strcmp(argv[2], "alias"))
	{
		int i;

		if ((i = count_aliases(who->alias)) > lim)
		{
			write_socket(p,
			    "This user has %d aliases.\n"
			    "Cannot change the limit to %d.\n", i, lim);
			doa_end(who, 0);
		}
		who->alias_lim = lim;
	}
	else if (!strcmp(argv[2], "mail"))
	{
		struct board *b;
		int live;

		if (who->mailbox == (struct board *)NULL)
		{
			b = restore_mailbox(who->rlname);
			live = 0;
		}
		else
		{
			b = who->mailbox;
			live = 1;
		}

		if (b->num > lim)
		{
			write_socket(p,
			    "This user's mailbox contains %d messages.\n"
			    "Cannot change the limit to %d.\n", b->num, lim);
			if (!live)
				free_board(b);
			doa_end(who, 0);
			return;
		}
		b->limit = lim;
		store_board(b);
		if (!live)
			free_board(b);
	}
	else
	{
		write_socket(p, "Unknown limit: %s\n", argv[2]);
		doa_end(who, 0);
		return;
	}

	if (IN_GAME(who))
	{
		write_socket(who, "%s has changed your %s limit to: ",
		    p->name, argv[2]);
		if (lim == -1)
			write_socket(who, "unlimited.\n");
		else
			write_socket(who, "%d\n", lim);
	}
	doa_end(who, 1);
	write_socket(p, "Ok.\n");
}

void
f_chown(struct user *p, int argc, char **argv)
{
	char *tmp;
	
	if (!CAN_CHANGE_ROOM(p, p->super))
	{
		write_socket(p, "You do not own this room.\n");
		return;
	}
	tmp = p->super->owner;
	p->super->owner = string_copy(argv[1], "owner");
	if (!CAN_CHANGE_ROOM(p, p->super))
	{
		write_socket(p,
		    "Change would remove your write access to room.\n");
		/* Undo */
		xfree(p->super->owner);
		p->super->owner = tmp;
		return;
	}
	write_socket(p, "Rooms owner changed from %s to %s\n", tmp, argv[1]);
	xfree(tmp);
	store_room(p->super);
}

void
chpasswd4(struct user *p, char *c)
{
	extern char *host_name, *host_ip;

	p->input_to = NULL_INPUT_TO;

	if (!strlen(c) || tolower(*c) != 'y')
	{
		write_socket(p, "No email sent.\n");
		pop_n_elems(&p->stack, 4);
		return;
	}

	send_email(p->rlname, p->stack.sp->u.string, (char *)NULL,
	    "Password change on "LOCAL_NAME, 1,
	    "Your password for the username %s on %s has been reset to:\n"
	    "\t%s\n"
	    "\nI look forward to seeing you there.\n\n"
	    "Enjoy the %s experience at:\n\t%s %d (%s %d)\n\n"
	    "Regards,\n\n%s.\n\n",
	    (p->stack.sp - 1)->u.string, LOCAL_NAME,
	    (p->stack.sp - 2)->u.string, LOCAL_NAME,
	    host_name, DEFAULT_PORT,
	    host_ip, DEFAULT_PORT,
	    p->capname);

	write_socket(p, "Email submitted.\n");
	pop_n_elems(&p->stack, 4);
}

void
chpasswd3(struct user *p, char *c)
{
	struct user *u;

	echo(p);
	p->input_to = NULL_INPUT_TO;
	if (strcmp(c, p->stack.sp->u.string))
	{
		write_socket(p, "\nPasswords didn't match.\n");
		pop_n_elems(&p->stack, 2);
		return;
	}
	if ((u = doa_start(p, (p->stack.sp - 1)->u.string)) ==
	    (struct user *)NULL)
	{
		write_socket(p, "User %s does not exist.\n");
		pop_n_elems(&p->stack, 2);
		return;
	}
	if (!access_check(p, u))
	{
		doa_end(u, 0);
		pop_n_elems(&p->stack, 2);
		return;
	}

	COPY(u->passwd, crypted(p->stack.sp->u.string), "passwd");
	u->saveflags |= U_FPC;
	write_socket(p, "\nOk, Password changed.\n");

	if (u->email != (char *)NULL)
	{
		push_string(&p->stack, u->capname);
		push_string(&p->stack, u->email);
		p->input_to = chpasswd4;
		write_prompt(p, "\nEmail user? (y/n): ");
	}
	else
		pop_n_elems(&p->stack, 2);

	doa_end(u, 1);
}

void
chpasswd2(struct user *p, char *c)
{
	if (!strlen(c))
	{
		c = random_password(8);
		push_string(&p->stack, c);
		write_socket(p, "\nPassword set to: %s\n", c);
		chpasswd3(p, c);
		return;
	}
	push_string(&p->stack, c);
	p->input_to = chpasswd3;
	write_prompt(p, "\nAgain: ");
}

void
f_chpasswd(struct user *p, int argc, char **argv)
{
	if (!exist_user(argv[1]))
	{
		write_socket(p, "User %s does not exist.\n",
		    capitalise(argv[1]));
		return;
	}
	CHECK_INPUT_TO(p)
	push_string(&p->stack, argv[1]);
	noecho(p);
	write_prompt(p, "Enter new password; press return for a system "
	    "generated one\n: ");
	p->input_to = chpasswd2;
}

void
f_cpnote(struct user *p, int argc, char **argv)
{
	struct room *from, *to;
	struct message *m, *n;

	/* Checks galore..... *yawn* */

	if (!ROOM_POINTER(from, argv[2]))
	{
		write_socket(p, "Room %s not found.\n", argv[2]);
		return;
	}
	if (!ROOM_POINTER(to, argv[3]))
	{
		write_socket(p, "Room %s not found.\n", argv[3]);
		return;
	}
	if (!(from->flags & R_BOARD))
	{
		write_socket(p, "Room %s does not contain a board.\n",
		    from->fname);
		return;
	}
	if (!(to->flags & R_BOARD))
	{
		write_socket(p, "Room %s does not contain a board.\n",
		    to->fname);
		return;
	}

	/* For the moment, I will allow copying from one to room to
	 * itself..  if it were disallowed, people could just copy via a
	 * temporary board. */
#if 0
	if (from == to)
	{
		write_socket(p, "Source matches destination.\n");
		return;
	}
#endif

	if (!CAN_READ_BOARD(p, from->board))
	{
		write_socket(p, "You may not read the board in %s.\n",
		    from->fname);
		return;
	}

	if (!CAN_WRITE_BOARD(p, to->board))
	{
		write_socket(p, "You may not write to the board in %s.\n",
		    to->fname);
		return;
	}

	if ((m = find_message(from->board, atoi(argv[1]))) ==
	    (struct message *)NULL)
	{
		write_socket(p, "No such note, %s on board %s.\n", argv[1],
		    argv[2]);
		return;
	}

	/* Right, got the message! Need to make a copy.. */

	n = create_message();
	n->flags = m->flags;
	n->text = string_copy(m->text, "message text");
	n->subject = string_copy(m->subject, "message subj");
	n->poster = string_copy(m->poster, "message poster");
	n->date = m->date;

	add_message(to->board, n);

	store_board(to->board);
	write_socket(p, "Message copied.\n");
}

void
f_cproom(struct user *p, int argc, char **argv)
{
	struct room *r;
	char buff[BUFFER_SIZE];

	if (!valid_name(p, argv[2], 1))
		return;
	valid_room_name(p, argv[2]);

	if (ROOM_POINTER(r, argv[2]))
	{
		write_socket(p, "Room %s already exists.\n", argv[2]);
		return;
	}

	/* There are many ways to do this.. I opted for loading the old
	 * room from disk and then changing the stored filename before
	 * saving it again.
	 * By calling restore_room here without checking, we could end up with
	 * this room in memory twice, but only briefly.
	 */
	if ((r = restore_room(argv[1])) == (struct room *)NULL)
	{
		write_socket(p, "Room %s does not exist.\n", argv[1]);
		return;
	}

	/* Change the rooms filename.. */
	COPY(r->fname, argv[2], "cproom new filename");
	/* Change the owner! */
	COPY(r->owner, p->rlname, "cproom new owner");

	/* If there is a board, change the filename */
	if (r->flags & R_BOARD)
	{
		sprintf(buff, "board/%s", argv[2]);
		COPY(r->board->fname, buff, "cproom board fname");
		store_board(r->board);
	}
	
	/* And save it. */
	store_room(r);
	write_socket(p, "Room copied.\n");

	/* Leave it in memory. */
}

void
f_destroy(struct user *p, int argc, char **argv)
{
	struct room *r;

	if ((r = find_room(argv[1])) == (struct room *)NULL)
	{
		write_socket(p, "Room %s not found.\n", capitalise(argv[1]));
		return;
	}
	destroy_room(r);
	write_socket(p, "Room destroyed.\n");
}


void
f_echoall(struct user *p, int argc, char **argv)
{
	write_levelabu(p, p->level, "[ECHOALL:%s]", p->capname);
	write_all("%s\n", argv[1]);
}

void
f_echoto(struct user *p, int argc, char **argv)
{
	struct user *who;

	if ((who = find_user_msg(p, argv[1])) == (struct user *)NULL)
		return;
	if (who->level >= p->level)
		write_socket(who, "[ECHOTO:%s]", p->capname);
	write_socket(who, "%s\n", argv[2]);
	write_socket(p, "You echo: %s\n", argv[2]);
}

void
f_events(struct user *p, int argc, char **argv)
{
	extern struct event *events, *current_event;
	struct event *ev;
	struct strbuf string;

	init_strbuf(&string, 0, "f_events string");

	add_strbuf(&string, "Nxt id  delay  remain  x-time    x-date"
	    "                   name\n");
	add_strbuf(&string, "---------------------------------------"
	    "-----------------------------\n");
	for (ev = events; ev != (struct event *)NULL; ev = ev->next)
		sadd_strbuf(&string, "%c%5d: %5d (%5ld) %10ld %s [%s]\n",
		    ev == current_event ? '*' : ' ', ev->id,
		    ev->delay, (long)(ev->time - current_time),
		    (long)ev->time, nctime(user_time(p, ev->time)), ev->fname);
	pop_strbuf(&string);
	more_start(p, string.str, NULL);
}

void
f_force(struct user *p, int argc, char **argv)
{
	struct user *u;

	if ((u = find_user_msg(p, argv[1])) == (struct user *)NULL)
		return;
	if (!access_check(p, u))
		return;
	write_socket(u, "You feel a strange presence in your mind.\n");

	if (u->snooped_by != (struct user *)NULL)
		write_socket(u->snooped_by, "%s\n", argv[2]);

	log_file("force", "%s forced %s to %s", p->capname, u->capname,
	    argv[2]);
	if (!insert_command(&u->socket, argv[2]))
		write_socket(p, "Input buffer overflow.\n");
	else
		write_socket(p, "You force %s to %s\n", u->capname,
	    	    argv[2]);
}

void
f_invis(struct user *p, int argc, char **argv)
{
	if (p->saveflags & U_INVIS)
	{
		write_socket(p, "You are already invis.\n");
		return;
	}
	notify_levelabu(p, p->level, "[ %s has gone invis. ]",
	    p->capname);
	/*COPY(p->name, "Someone", "name");*/
	p->saveflags |= U_INVIS;
	write_socket(p, "You are now invisible\n");
}

void
f_iptab(struct user *p, int argc, char **argv)
{
	int i;
	extern struct ipentry iptable[];
	struct in_addr saddr;
	struct strbuf tmp;

	init_strbuf(&tmp, 0, "f_iptab tmp");

	add_strbuf(&tmp, "Current ip table.\n-----------------\n");
	for (i = 0; i < IPTABLE_SIZE; i++)
	{
		if (iptable[i].addr && iptable[i].name != (char *)NULL)
		{
			saddr.s_addr = iptable[i].addr;
			sadd_strbuf(&tmp, "%-30s : %s\n", 
			    inet_ntoa(saddr),
			    iptable[i].name);
		}
	}
	pop_strbuf(&tmp);
	more_start(p, tmp.str, NULL);
}

void
f_jlm(struct user *p, int argc, char **argv)
{
	extern struct jlm *jlms;
	struct object *ob;
	struct jlm *j;

	if (argc < 2 || (argc == 2 && !strcmp(argv[1], "-v")))
	{
		struct strbuf b;

		if (jlms == (struct jlm *)NULL)
		{
			write_socket(p, "No modules loaded.\n");
			return;
		}
		init_strbuf(&b, 0, "f_jlm sb");
		add_strbuf(&b, "  pid    fd       Id         Environment"
		    "     Description\n");
		add_strbuf(&b, "---------------------------------"
		    "---------------------------------------------\n");
		for (j = jlms; j != (struct jlm *)NULL; j = j->next)
		{
			char buf[33], *bp;

			if (argc != 2)
			{
				bp = buf;
				if (j->ident == (char *)NULL)
					*buf = '\0';
				else
					my_strncpy(buf, j->ident, 32);
			}
			else
				bp = j->ident;

			sadd_strbuf(&b, "%5d ", j->pid);

			if (j->infd == j->outfd)
				sadd_strbuf(&b, " %4d %4s  ", j->infd, "");
			else
				sadd_strbuf(&b, "[%4d,%4d] ", j->infd,
				    j->outfd);

			sadd_strbuf(&b, "%-10s %-15s %s\n", j->id,
			    (j->ob != (struct object *)NULL &&
			    j->ob->super != users->ob) ?
			    obj_name(j->ob->super): "", bp);
		}
		pop_strbuf(&b);
		more_start(p, b.str, NULL);
		return;
	}

	if (!strcmp(argv[1], "-reload"))
	{
		extern void preload_jlms(void);
		extern void f_erqd(struct user *, int, char **);

		preload_jlms();
		/* Also attempt to restart erqd.. */
		argv[1] = "restart";
		f_erqd(p, argc, argv);
		write_socket(p, "Reload complete.\n");
		return;
	}

	if (argc < 3)
	{
#define JLM_SYNTAX write_socket(p, "Syntax: jlm <-remove | -add> <module> " \
		    "[<room> | @<user>].\n")
		JLM_SYNTAX;
		return;
	}

	if (p->level < A_JLM_OPTS)
	{
		write_socket(p, "Permission denied.\n");
		return;
	}

	deltrail(argv[1]);
	deltrail(argv[2]);

	if (argc < 4)
		ob = users->ob;
	else if (argv[3][0] == '@')
	{
		struct user *w;

		if ((w = find_user_msg(p, argv[3] + 1)) == (struct user *)NULL)
			return;

		ob = w->ob;
	}
	else
	{
		struct room *r;

		if (!ROOM_POINTER(r, argv[3]))
		{	
			write_socket(p, "No such room, %s.\n", argv[3]);
			return;
		}

		ob = r->ob;
	}

	if (!strcmp(argv[1], "-remove"))
	{
		if ((j = find_jlm_in_env(ob, argv[2])) == (struct jlm *)NULL)
		{
			write_socket(p, "No such jlm in list, %s.\n", argv[2]);
			return;
		}
		if (ob == users->ob)
		{
			notify_level((p->saveflags & U_INVIS) ? p->level :
			    L_CONSUL, "[ JLM %s removed by %s. ]",
			    j->id, p->capname);
			write_socket(p, "Removing module %s.\n", j->id);
		}
		else
		{
			notify_level((p->saveflags & U_INVIS) ? p->level :
			    L_CONSUL, "[ JLM %s removed from %s by %s. ]",
			    j->id, obj_name(ob), p->capname);
			write_socket(p, "Removing module %s from %s.\n", j->id,
			    obj_name(ob));
		}
		kill_jlm(j);
		return;
	}
	else if (!strcmp(argv[1], "-add"))
	{
                attach_jlm(ob, argv[2]);
		if (ob == users->ob)
		{
			write_socket(p, "Attaching module %s.\n", argv[2]);
			notify_level((p->saveflags & U_INVIS) ? p->level :
			    L_CONSUL, "[ JLM %s added by %s. ]",
			    argv[2], p->capname);
		}
		else
		{
			write_socket(p, "Attaching module %s to %s.\n", argv[2],
			    obj_name(ob));
			notify_level((p->saveflags & U_INVIS) ? p->level :
			    L_CONSUL, "[ JLM %s added to %s by %s. ]",
			    argv[2], obj_name(ob), p->capname);
		}
		return;
	}
	else
	{
		JLM_SYNTAX;
		return;
	}
}

void
f_mkroom(struct user *p, int argc, char **argv)
{
	extern int valid_name(struct user *, char *, int);
	struct room *r;

	if (ROOM_POINTER(r, argv[1]))
	{
		write_socket(p, "Room %s already exists\n", argv[1]);
		return;
	}
	if (!valid_name(p, argv[1], 1))
		return;
	valid_room_name(p, argv[1]);
	r = new_room(argv[1], p->rlname);
	write_socket(p, "Room %s created.\n", argv[1]);
	notify_levelabu(p, p->level, "[ New room, '%s', created by %s. ]",
	    argv[1], p->capname);
}

void
f_rmroom(struct user *p, int argc, char **argv)
{
	struct room *r;

	if (!ROOM_POINTER(r, argv[1]))
	{
		write_socket(p, "Room %s does not exist.\n", argv[1]);
		return;
	}
	if (!CAN_CHANGE_ROOM(p, r))
	{
		write_socket(p, "You do not own that room.\n");
		return;
	}
	destroy_room(r);
	delete_room(argv[1]);
	write_socket(p, "Room %s deleted.\n", argv[1]);
	notify_levelabu(p, p->level, "[ Room %s deleted by %s. ]",
	    argv[1], p->capname);
}

#ifdef REMOTE_NOTIFY
void
f_rntab(struct user *p, int argc, char **argv)
{
	if (p->level >= L_OVERSEER && argc > 1)
	{
		if (argc < 3)
		{
			write_socket(p,
			    "Syntax: rntab [<client> <message>]\n");
			return;
		}

		deltrail(argv[1]);
		if (send_a_rnotify_msg(atoi(argv[1]), argv[2]))
			write_socket(p, "Message sent.\n");
		else
			write_socket(p, "Send failed.\n");
	}
	else
		dump_rnotify_table(p);
}
#endif

void
f_showtail(struct user *p, int argc, char **argv)
{
	struct user *who;
	char *q;

	if ((who = find_user_msg(p, argv[1])) == (struct user *)NULL)
		return;

	while (argv[2][0] == '/')
		argv[2]++;

	if (!valid_path(p, argv[2], VALID_READ))
	{
		write_socket(p, "%s: Permission denied.\n", argv[2]);
		return;
	}
	if ((q = strchr(argv[2], '/')) == (char *)NULL)
	{
		write_socket(p, "Can only %s files in subdirectories.\n",
		    argv[0]);
		return;
	}
	*q = '\0';
	if (!dump_file(who, argv[2], q + 1, DUMP_TAIL))
		write_socket(p, "No such file.\n");
}

static void
siteban2(struct user *p, int i)
{
	if (i != EDX_NORMAL)
	{
		dec_n_elems(&p->stack, 3);
		return;
	}

	add_siteban(
	    (p->stack.sp - 3)->u.unumber,
	    (p->stack.sp - 2)->u.unumber,
	    (p->stack.sp - 1)->u.number,
	    p->stack.sp->u.string);

	dec_n_elems(&p->stack, 4);

	write_socket(p, "Rule added to siteban table.\n");
}

void
f_siteban(struct user *p, int argc, char **argv)
{
	unsigned long addr, netmask;
	enum siteban_level lev;
	int remove = 0;
	int i;
	int inv = 0;

	if (argc < 2)
	{
		dump_siteban_table(p, -1);
		return;
	}

	deltrail(argv[1]);

	if (!strcmp(argv[1], "-v"))
	{
		if (argc != 3)
			write_socket(p, "Syntax: siteban -v <number>\n");
		else
			dump_siteban_table(p, atoi(argv[2]));
		return;
	}

	if (!strcmp(argv[1], "-r"))
	{
		/* Remove entry.. */
		if (argc != 3)
		{
			write_socket(p, "Syntax: siteban -r <host>[/mask]\n");
			return;
		}
		remove = 1;
		argv++, argc--;
	}

	if (argc < 3)
		lev = BANNED_ALL;
	else
	{
		lev = atoi(argv[2]);

		if (lev < 1 || lev > BANNED_MAX)
		{
			write_socket(p, "Bad ban level, %d.\n", lev);
			return;
		}
	}

	if (*argv[1] == '!')
	{
		inv = 1, argv[1]++;
		lev = BANNED_INV;
	}

	if ((i = parse_banned_site(argv[1], &addr, &netmask)) == -2)
	{
		write_socket(p, "Badly specified host ip.\n");
		return;
	}
	else if (i == -1)
	{
		write_socket(p, "Badly specified netmask.\n");
		return;
	}

	addr &= netmask;

	if (remove)
	{
		if (remove_siteban(addr, netmask))
			write_socket(p, "Rule removed from siteban table.\n");
		else
			write_socket(p, "Rule not found in siteban table.\n");
		return;
	}

	if (inv)
	{
		/* Inverse entry, no reason or level required. */
		add_siteban(addr, netmask, BANNED_INV, (char *)NULL);
		write_socket(p, "Rule added to siteban table.\n");
	}
	else
	{
		push_unsigned(&p->stack, addr);
		push_unsigned(&p->stack, netmask);
		push_number(&p->stack, lev);
		write_socket(p, "Enter message to be displayed below:\n");
		ed_start(p, siteban2, 24, 0);
	}
}

void
f_slylogin(struct user *p, int argc, char **argv)
{
	write_socket(p, "Sly login mode turned %s.\n",
	    (p->saveflags ^= U_SLY_LOGIN) & U_SLY_LOGIN ? "on" : "off");
}

void
f_trans(struct user *p, int argc, char **argv)
{
	struct user *who;

	if ((who = find_user_msg(p, argv[1])) == (struct user *)NULL)
		return;
	if (!access_check(p, who))
		return;
	if (who->super == p->super)
	{
		write_socket(p, "%s is already here.\n", who->capname);
		return;
	}
	write_socket(who, "You are transferred by %s\n", p->name);
	write_socket(p, "You transfer %s.\n", who->name);
	move_user(who, p->super);
}

void
f_unbanish(struct user *p, int argc, char **argv)
{
	extern char *unbanish(char *);
	char *b = unbanish(argv[1]);
	if (b == (char *)NULL)
		write_socket(p, "%s is not banished.\n", capitalise(argv[1]));
	else
	{
		notify_levelabu(p, p->level,
		    "[ %s has been unbanished by %s. ]",
	    	    capitalise(argv[1]), p->capname);
		write_socket(p, "Unbanished %s:\n%s\n", capitalise(argv[1]),
		    b);
	}
}

void
f_valemail(struct user *p, int argc, char **argv)
{
	struct user *who;

#ifdef AUTO_VALEMAIL
	deltrail(argv[1]);
	if (!strcmp(argv[1], "-p"))
	{
		pending_valemails(p);
		return;
	}
	else if (!strcmp(argv[1], "-d"))
	{
		if (argc < 3)
			write_socket(p,
			    "Syntax: valemail -d <user>\n");
		else if (remove_any_valemail(argv[2]))
			write_socket(p, "Pending request removed.\n");
		else
			write_socket(p, "No pending request for %s.\n",
			    argv[2]);
		return;
	}
#endif

	if ((who = doa_start(p, argv[1])) == (struct user *)NULL)
	{
		write_socket(p, "User %s does not exist.\n", argv[1]);
		return;
	}

	if (who->email == (char *)NULL)
	{
		write_socket(p, "That user has no email address.\n");
		doa_end(who, 0);
		return;
	}

	write_socket(p, "You have %svalidated the email address for %s.\n"
	    "it is: %s\n", (who->saveflags ^= U_EMAIL_VALID) & U_EMAIL_VALID ?
	    "" : "in", who->capname, who->email);
	log_file("valemail", "%s (%s) %svalidated by %s", who->capname,
	    who->email, (who->saveflags & U_EMAIL_VALID) ? "" : "in",
	    p->capname);
	notify_levelabu(p, p->level,
	    "[ %s has %svalidated %s%s email address ]", p->name,
	    who->saveflags & U_EMAIL_VALID ? "" : "in", who->capname,
	    who->capname[strlen(who->capname) - 1] == 's' ? "'" : "'s");

#ifdef AUTO_VALEMAIL
	if (who->saveflags & U_EMAIL_VALID)
		remove_any_valemail(who->rlname);
#endif

	if (IN_GAME(who) && (who->saveflags & U_EMAIL_VALID))
	{
		attr_colour(who, "notify");
		write_socket(who,
		    "Your email address has been validated by %s.", p->name);
		reset(who);
		write_socket(who, "\n");
	}
	doa_end(who, 1);
}

void
f_vis(struct user *p, int argc, char **argv)
{
	if (!(p->saveflags & U_INVIS))
	{
		write_socket(p, "You are already visible.\n");
		return;
	}
	/*COPY(p->name, p->capname, "name");*/
	p->saveflags ^= U_INVIS;
	notify_levelabu(p, p->level, "[ %s has become visible. ]",
	    p->capname);
	write_socket(p, "You are now visible\n");
}

void
f_wall(struct user *p, int argc, char **argv)
{
	log_file("wall", "%s\n%s", p->capname, argv[1]);
	write_all("\n---\nBroadcast message from %s at %s\n", p->name,
	    ctime(&current_time));
	write_all("%c%c%c%s\n---\n", 7, 7, 7, argv[1]);
}

