/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	cmd.warden.c
 * Function:	Warden commands.
 **********************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/param.h>
#include <netinet/in.h>
#include "jeamland.h"

extern struct user *users;
extern struct room *rooms;
extern int num_rooms, peak_rooms, peak_users;
extern time_t current_time;
extern float event_av, command_av;

extern int shutdown_id;
extern int sysflags;

/*
 * WARDEN commands....
 */

void
f_banish(struct user *p, int argc, char **argv)
{
	struct user *u;
	char *b;
	extern char *banished(char *);
	extern void banish(struct user *, char *);

	strcpy(argv[1], lower_case(argv[1]));

	if ((u = doa_start(p, argv[1])) != (struct user *)NULL)
	{
		if (!access_check(p, u))
		{
			log_file("secure/illegal", "Banish attempt, %s on %s.",
			    capitalise(p->capname), capitalise(u->capname));
			doa_end(u, 0);
			return;
		}
		if (IN_GAME(u))
		{
			write_socket(p, "%s is logged on, disconnecting.\n",
		    	    u->name);
			save_user(u, 1, 0);
			disconnect_user(u, 0);
		}
		doa_end(u, 0);
	}
	if ((b = banished(argv[1])) != (char *)NULL)
	{
		write_socket(p, "%s is already banished:\n%s\n",
		    capitalise(argv[1]), b);
		return;
	}
	banish(p, argv[1]);
	notify_levelabu(p, p->level, "[ %s has been banished by %s. ]",
	    capitalise(argv[1]), p->capname);
	write_socket(p, "You have banished %s.\n", capitalise(argv[1]));
}

void
f_boot(struct user *p, int argc, char **argv)
{
	struct user *who;
	int tm = 5;

	if (!strcmp(argv[1], "-t"))
	{
		if ((tm = atoi(argv[2])) < 5)
		{
			write_socket(p, "Time delay too short ( < 5 mins ).\n");
			return;
		}

		if (argc < 5)
		{
			write_socket(p,
			    "Syntax: boot -t <time> <user> <msg>.\n");
			return;
		}
		argv[1] = argv[3];
		argv[2] = argv[4];
	}

	tm *= 60;

	if ((who = find_user_msg(p, argv[1])) == (struct user *)NULL)
		return;
	if (!access_check(p, who))
		return;
	log_file("boot", "%s booted by %s (%s).", who->capname,
	    p->capname, argv[2]);
	write_socket(p, "You boot %s.\n", who->name);
	who->hibernate = current_time + tm;
	save_user(who, 1, 0);
	write_socket(who, "You have been kicked off by %s\n", 
	    p->name);
	disconnect_user(who, 0);
	write_allabu(p, "[ %s has been booted off. ]\n",
	    who->name);
	notify_levelabu(p, p->level, "[ %s has been booted off by %s. (%s) ]",
	    who->capname, p->capname, argv[2]);
}

void
f_cat(struct user *p, int argc, char **argv)
{
	char *q;

	while (argv[1][0] == '/')
		argv[1]++;

	if (!valid_path(p, argv[1], VALID_READ))
	{
		write_socket(p, "%s: Permission denied.\n", argv[1]);
		return;
	}
	if ((q = strchr(argv[1], '/')) == (char *)NULL)
	{
		write_socket(p, "Can only %s files in subdirectories.\n",
		    argv[0]);
		return;
	}
	*q = '\0';
	if (!dump_file(p, argv[1], q + 1, !strcmp(argv[0], "more") ?
	    DUMP_MORE : (!strcmp(argv[0], "cat") ? DUMP_CAT : DUMP_TAIL)))
		write_socket(p, "No such file.\n");
}

#ifdef CDUDP_SUPPORT
void
f_cdudpchan(struct user *p, int argc, char **argv)
{
	extern void cdudp_channel(struct user *, char *, char *);

	cdudp_channel(p, argv[0], argv[1]);
}
#endif

void
f_channel(struct user *p, int argc, char **argv)
{
	int lev;
#ifdef LOCAL_CHANHIST
	char fname[MAXPATHLEN + 1];
#endif
	char attr[15];
	char sbuf[5];

	if (!strcmp(argv[0], "warden"))
		lev = L_WARDEN;
	else if (!strcmp(argv[0], "consul"))
		lev = L_CONSUL;
	else if (!strcmp(argv[0], "overseer"))
		lev = L_OVERSEER;
	else lev = p->level;

	sprintf(attr, "lc-%s", argv[0]);

#ifdef LOCAL_CHANHIST
	/* History */
	if (!strcmp(argv[1], "!"))
	{
		dump_file(p, "log/channel", argv[0], DUMP_TAIL);
		return;
	}
#endif

	switch (*argv[1])
	{
	    case ':':	/* Emote */
		strcpy(sbuf, "");
		argv[1]++;
		break;
	    case ';':	/* Possessive emote */
		sprintf(sbuf, "'%s", p->rlname[strlen(p->rlname) - 1] == 's' ?
		    "" : "s");
		argv[1]++;
		break;
	    case '.':	/* Feeling (*grin Dreamer*) */
	    {
		if (argv[1][1] != '.')	/* Fallthrough if .. (*grin Ernakk*) */
		{
			/* Ugh, code has to be somewhat duplicated here. */
			char *q;

			/* Rationalise argv.. */
			argc--, argv++;
			deltrail(++argv[0]);

			if ((q = expand_feeling(p, argc, argv, 0)) ==
			    (char *)NULL || q == (char *)1)
			{
				write_socket(p,
				    "Error in channel feeling, %s.\n",
				    argv[0]);
				return;
			}
			write_level_wrt_attr(lev, attr,
			    "[%s] %s", capitalise(argv[-1]), q);
#ifdef LOCAL_CHANHIST
			sprintf(fname, "channel/%s", argv[-1]);
			log_file(fname, "%s", q);
#endif
			xfree(q);
			return;
		}
	    }
	    /* FALLTHROUGH */
		
	    default:
		strcpy(sbuf, ":");
		break;
	}

	write_level_wrt_attr(lev, attr,
	    "[%s] %s%s %s", capitalise(argv[0]), p->capname,
	    sbuf, argv[1]);
#ifdef LOCAL_CHANHIST
	sprintf(fname, "channel/%s", argv[0]);
	log_file(fname, "%s%s %s", p->capname, sbuf, argv[1]);
#endif
}

void
f_curse(struct user *p, int argc, char **argv)
{
	struct user *u;

	if ((u = find_user_msg(p, argv[1])) == (struct user *)NULL)
		return;
	if (!access_check(p, u))
		return;
	if (!strcmp(argv[0], "curse"))
	{
		if (u->saveflags & U_SHOUT_CURSE)
		{
			write_socket(p, "%s is already cursed.\n",
			    u->capname);
			return;
		}
		u->saveflags |= U_SHOUT_CURSE;
		write_socket(p, "Cursed %s\n", u->name);
		log_file("curse", "%s shoutcursed by %s\n(%s)",
		    u->capname, p->capname, argv[2]);
		notify_level(p->level, "[ %s shoutcursed by %s. ]",
		    u->capname, p->capname);
		return;
	}
	if (!(u->saveflags & U_SHOUT_CURSE))
	{
		write_socket(p, "%s is not cursed.\n", u->capname);
		return;
	}
	u->saveflags ^= U_SHOUT_CURSE;
	write_socket(p, "Uncursed %s\n", u->capname);
	log_file("curse", "Shoutcurse on %s removed by %s.",
	    u->capname, p->capname);
	notify_level(p->level, "[ %s un-shoutcursed by %s. ]",
	    u->capname, p->capname);
}

void
f_dump(struct user *p, int argc, char **argv)
{
	struct exit *e;
	struct sent *s;

	write_socket(p, "Flags:\t%d\n", p->super->flags);
	write_socket(p, "Fname:\t%s\n", p->super->fname);
	write_socket(p, "Name:\t%s\n", p->super->name);
	write_socket(p, "Owner:\t%s\n", p->super->owner);
	if (p->super->flags & R_BOARD)
	{
		struct board *b = p->super->board;

		write_socket(p, "Board.\n");
		if (b->write_grupe != (char *)NULL)
			write_socket(p, "\tWrite grupe: %s.\n",
			    b->write_grupe);
		if (b->read_grupe != (char *)NULL)
			write_socket(p, "\tRead grupe: %s.\n",
			    b->read_grupe);
		write_socket(p, "\tMessage limit: %d.\n", b->limit);
		if (b->archive != (char *)NULL)
			write_socket(p, "\tArchived: %s\n", b->archive);
		if (b->followup != (char *)NULL)
			write_socket(p, "\tFollowup: %s\n", b->followup);
	}
	if (p->super->lock_grupe != (char *)NULL)
		write_socket(p, "Grupe locked: %s\n", p->super->lock_grupe);
	if (p->super->flags & R_LOCKED)
		write_socket(p, "Room is locked.\n");
	for (e = p->super->exit; e != (struct exit *)NULL; e = e->next)
		write_socket(p, "Exit:\t%-15s : %s\n", e->name, e->file);
	for (s = p->super->ob->inv; s != (struct sent *)NULL; s = s->next_inv)
		write_socket(p, "Sent:\t%-15s : %s\n", s->cmd,
		    obj_name(s->ob));
	if (p->super->inhibit_cleanup)
		write_socket(p, "Cleanup inhibit: %d\n",
		    p->super->inhibit_cleanup);
}
	
void
f_echo(struct user *p, int argc, char **argv)
{
	write_room_levelabu(p, p->level, "[ECHO:%s] ", p->capname);
	write_room(p, "%s\n", argv[1]);
}

void
edret(struct user *p, int i)
{
	if (i != EDX_NORMAL)
	{
		pop_stack(&p->stack);
		return;
	}

	if (!write_file((p->stack.sp - 1)->u.string, p->stack.sp->u.string))
		write_socket(p, "Error saving file.\n");
	pop_n_elems(&p->stack, 2);
}

void
f_ed(struct user *p, int argc, char **argv)
{
	char *contents;

	if (!valid_path(p, argv[1], VALID_WRITE))
	{
		write_socket(p, "Permission denied.\n");
		return;
	}
	push_string(&p->stack, argv[1]);
	if ((contents = read_file(argv[1])) == (char *)NULL)
	{
		write_socket(p, "[ New file. ]\n");
		ed_start(p, edret, -1, ED_NO_AUTOLIST | ED_INFO);
	}
	else
	{
		push_malloced_string(&p->stack, contents);
		ed_start(p, edret, -1, ED_STACKED_TEXT | ED_NO_AUTOLIST |
		    ED_INFO);
	}
}

void
f_grep(struct user *p, int argc, char **argv)
{
	struct strbuf buf;
	int i, casex;

	casex = 0;
	deltrail(argv[1]);
	if (!strcmp(argv[1], "-i"))
	{
		casex = 1;
		argv++, argc--;
	}

	if (argc != 3)
	{
		write_socket(p, "Syntax: grep [-i] <substring> <file>\n");
		return;
	}

	deltrail(argv[1]);

	while (argv[2][0] == '/')
		argv[2]++;

	if (!valid_path(p, argv[2], VALID_READ))
	{
		write_socket(p, "%s: Permission denied.\n", argv[2]);
		return;
	}
	if (strchr(argv[2], '/') == (char *)NULL)
	{
		write_socket(p, "Can only grep files in subdirectories.\n");
		return;
	}

	init_strbuf(&buf, 0, "f_grep");

	switch ((i = grep_file(argv[2], argv[1], &buf, casex)))
	{
	    case -1:	/* No such file */
		write_socket(p, "File %s not found.\n", argv[2]);
		break;

	    case 0:	/* No matches */
		write_socket(p, "No matching lines found.\n");
		break;

	    default:
		sadd_strbuf(&buf, "---\n%d match%s found.\n",
		    i, i == 1 ? "" : "es");
		pop_strbuf(&buf);
		more_start(p, buf.str, 0);
		return;
	}
	free_strbuf(&buf);
}

void
f_hash(struct user *p, int argc, char **argv)
{
	int verbose = 0;
	int done = 0;
	char *opt = (char *)NULL;

	if (argc > 1)
	{
		deltrail(argv[1]);
		if (!strcmp(argv[1], "-t"))
		{
			verbose = 1;
			argv++, argc--;
		}
	}

	if (argc > 1)
		opt = argv[1];

#ifdef HASH_COMMANDS
	if (opt == (char *)NULL || !strncmp(opt, "comm", 4))
	{
		command_hash_stats(p, verbose);
		done = 1;
	}
#endif
#ifdef HASH_FEELINGS
	if (opt == (char *)NULL || !strncmp(opt, "feel", 4))
	{
		feeling_hash_stats(p, verbose);
		done = 1;
	}
#endif
#ifdef HASH_ALIAS_FUNCS
	if (opt == (char *)NULL || !strncmp(opt, "alia", 4))
	{
		alias_hash_stats(p, verbose);
		done = 1;
	}
#endif
#ifdef HASH_JLM_FUNCS
	if (opt == (char *)NULL || !strncmp(opt, "jlm", 3))
	{
		jlm_hash_stats(p, verbose);
		done = 1;
	}
#endif
#if defined(HASH_I3_HOSTS) && defined(IMUD3_SUPPORT)
	if (opt == (char *)NULL || !strncmp(opt, "i3", 2))
	{
		i3_hash_stats(p, verbose);
		done = 1;
	}
#endif
	if (!done)
		write_socket(p, "No information listed.\n"
		    "Syntax: hash [-t] [table]\n");
}

#if defined(CDUDP_SUPPORT) || defined(INETD_SUPPORT)

void
f_iping(struct user *p, int argc, char **argv)
{
	extern void inetd_ping(struct user *, char *);
	extern void inetd_ping_host(struct user *, char *, int, int);

	if (p->level > L_CONSUL && argc > 2)
	{
		deltrail(argv[1]);
		if (argv[1][0] == '%')
			inetd_ping_host(p, argv[1] + 1, atoi(argv[2]), 1);
		else
			inetd_ping_host(p, argv[1], atoi(argv[2]), 0);
		write_socket(p, "Host ping sent.\n");
		return;
	}
	inetd_ping(p, argv[1]);
}
#endif

void f_ls(struct user *, int, char **);

void
f_log(struct user *p, int argc, char **argv)
{
	if (argc == 1)
	{
		if (p->level > L_WARDEN)
		{
			write_socket(p, "[  SECURE LOGS  ]\n");
			argv[1] = "log/secure";
			f_ls(p, 2, argv);
			write_socket(p, "[  NORMAL LOGS  ]\n");
		}
		argv[1] = "log";
		f_ls(p, 2, argv);
		return;
	}
	if (valid_path(p, "log/secure", VALID_READ) &&
	    dump_file(p, "log/secure", argv[1], DUMP_TAIL))
		return;
	
	if (!dump_file(p, "log", argv[1], DUMP_TAIL))
		write_socket(p, "No such log.\n");
}

void
f_ls(struct user *p, int argc, char **argv)
{
        struct vector *v;
        int i, flag, lng = 0;
	struct strbuf d;

	if (argc > 1)
	{
		if (!strncmp(argv[1], "-l", 2))
		{
			lng = 1;
			argv[1] += 2;
			while (argv[1][0] == ' ')
				argv[1]++;
		}
		while (argv[1][0] == '/')
			argv[1]++;
	}
        if (argc == 1 || !strlen(argv[1]))
                argv[1] = ".";

        if ((v = get_dir(argv[1], lng)) == (struct vector *)NULL)
        {
                write_socket(p, "%s: No files found.\n", argv[1]);
                return;
        }
        if (!valid_path(p, argv[1], VALID_READ))
        {
		free_vector(v);
                write_socket(p, "%s: Permission denied.\n", argv[1]);
                return;
        }

	init_strbuf(&d, 0, "f_ls d");

        for (flag = 0, i = 0; i < v->size; i++)
        {
		if (lng)
		{
			sadd_strbuf(&d, "%10d   %s   %s\n",
			    v->items[i + 2].u.number,
			    nctime(user_time(p,
			    (time_t)v->items[i + 1].u.number)),
			    v->items[i].u.string);
			i += 2;
		}
		else
		{
                	if (flag)
				add_strbuf(&d, ", ");
                	else
                        	flag = 1;
			add_strbuf(&d, v->items[i].u.string);
		}
        }
        write_socket(p, "%s\n", d.str);
	free_strbuf(&d);
	flag = lng ? v->size / 3 : v->size;
		
	write_socket(p, "%d file%s.\n", flag, flag == 1 ? "" : "s");
        free_vector(v);
}

void
f_mbs(struct user *p, int argc, char **argv)
{
	extern struct mbs *mbs_master;
	struct mbs *m;
	struct umbs *n;

#define MBS_ADD_BOARD(mmbs, lmbs) \
			   sadd_strbuf(&buf, "%c%c%-15s %s\n", \
			   p->mbs_current != (char *)NULL && \
			   !strcmp(p->mbs_current, mmbs->id) ? '>' : ' ', \
			   mmbs->last > lmbs->last ? '*' : ' ', \
			   mmbs->id, mmbs->desc);

	/* Might as well.. if not here, where ? */
	remove_bad_umbs(p);

	if (argc < 2)
	{
		/* Default to 'mbs n' */
		argc++;
		argv[1] = "n";
		/* And do not deltrail this static string! */
	}
	else
	{
		deltrail(argv[1]);
		if (*argv[1] == '-')
			argv[1]++;
	}
	if (strlen(argv[1]) == 1)
	{
		struct strbuf buf;
		int only_new = 0;

		switch (*argv[1])
		{
		    case 'A':   /* Add a new board to mbs.. */
		    {
			struct room *r;

			if (p->level < A_MBS_ADMIN)
			{
				write_socket(p, "Permission denied.\n");
				return;
			}
			if (argc < 5)
			{
				write_socket(p,
				    "Syntax: mbs -A <board id> <room id> "
				    "<board desc>\n");
				return;
			}
			deltrail(argv[2]);
			deltrail(argv[3]);
			if (find_mbs(argv[2], 0) != (struct mbs *)NULL)
			{
				write_socket(p,
				    "Board %s already exists.\n", argv[2]);
				return;
			}
			m = create_mbs();
			if (!ROOM_POINTER(r, argv[3]) ||
			    !(r->flags & R_BOARD))
				m->last = 0;
			else
				m->last = mbs_get_last_note_time(r->board);
			m->id = string_copy(argv[2], "*mbs id");
			m->room = string_copy(argv[3], "*mbs room");
			m->desc = string_copy(argv[4], "*mbs desc");
			add_mbs(m);
			store_mbs();
			write_socket(p, "Added board: %s\n", argv[2]);
			return;
		    }

		    case 'c':   /* Catchup current board */
			if (p->mbs_current == (char *)NULL ||
			    (n = find_umbs(p, p->mbs_current, 0)) ==
			    (struct umbs *)NULL)
			{
				write_socket(p, "No current board.\n");
				return;
			}
			n->last = current_time;
			write_socket(p, "Caught up: %s\n", n->id);
			return;

		    case 'C':   /* Catchup all boards */
			for (n = p->umbs; n != (struct umbs *)NULL;
			    n = n->next)
				n->last = current_time;
			write_socket(p, "Caught up all boards.\n");
			return;

		    case 'd':	/* Delete a note */
		    {
			struct room *r;
			struct message *msg;

                        if (argc < 3)
                        {
                                write_socket(p, "Syntax: mbs -d <number>\n");
                                return;
                        }

                        if ((r = get_current_mbs(p, &n, &m)) ==
                            (struct room *)NULL)
                                return;

			if (!CAN_WRITE_BOARD(p, r->board))
			{
				write_socket(p,
				    "No write access to board %s.\n", n->id);
				return;
			}

        		if ((msg = find_message(r->board, atoi(argv[2]))) ==
			    (struct message *)NULL)
        		{
                		write_socket(p, "No such note, %s\n",
				    argv[2]);
                		return;
        		}
        		if (strcmp(msg->poster, p->rlname) &&
			    (p->level < L_WARDEN ||
            		    !iaccess_check(p, query_real_level(msg->poster))))
        		{
                		write_socket(p, "You didn't post that note.\n");
                		return;
        		}
			if (!strcmp(msg->poster, p->rlname))
				p->postings--;
        		remove_message(r->board, msg);
        		store_board(r->board);
			m->last = mbs_get_last_note_time(r->board);
			store_mbs();
        		write_socket(p, "Note removed.\n");
			return;
		    }

		    case 'D':   /* Delete a board from mbs */
			if (p->level < A_MBS_ADMIN)
			{
				write_socket(p, "Permission denied.\n");
				return;
			}
			if (argc < 3)
			{
				write_socket(p, "Syntax: mbs -D <board>\n");
				return;
			}
			if ((m = find_mbs(argv[2], 1)) == (struct mbs *)NULL)
			{
				write_socket(p, "No such board, %s\n",
				    argv[2]);
				return;
			}
			write_socket(p, "Removed board: %s\n", m->id);
			remove_mbs(m);
			store_mbs();
			return;

		    case 'e':	/* Edit a note */
		    {
			struct room *r;
			struct message *msg;
			int num;
			extern void epost2(struct user *, char *);
			extern void epost_cleanup(struct user *);

			if (argc < 3)
			{
				write_socket(p, "Syntax: mbs -e <number>\n");
				return;
			}

			if ((r = get_current_mbs(p, &n, &m)) ==
			    (struct room *)NULL)
				return;

			if (!CAN_WRITE_BOARD(p, r->board))
			{
				write_socket(p,
				    "No write access to board %s.\n", n->id);
				return;
			}

        		if ((msg = find_message(r->board,
			    (num = atoi(argv[2])))) == (struct message *)NULL)
        		{
                		write_socket(p, "No such note, %s\n", argv[2]);
                		return;
        		}
	
        		if (strcmp(msg->poster, p->rlname) &&
			    (p->level < L_WARDEN ||
            		    !iaccess_check(p, query_real_level(msg->poster))))
        		{
                		write_socket(p, "You didn't post that note.\n");
                		return;
        		}

        		push_string(&p->stack, r->fname);
        		push_number(&p->stack, num);
        		push_pointer(&p->stack, (void *)msg);
			push_string(&p->stack, msg->subject);
        		push_string(&p->stack, msg->text);
			write_prompt(p, "(Press return to leave as: %s)\n"
			    "Subject: ", msg->subject);          
			p->input_to = epost2;

			/* Make the room memory resident for the moment.. */
			r->inhibit_cleanup++;
			/* Set an exit function to reset the above flag if
			 * the user linkdies */
			push_string(&p->atexit, r->fname);
			push_fpointer(&p->atexit, epost_cleanup);

			return;
		    }

		    case 'f':	/* Followup a note */
		    {
			extern void post2(struct user *, int);
			struct room *r, *s;
			struct board *targ;
			struct message *msg;
			char *subj;

			if (argc < 3)
			{
				write_socket(p, "Syntax: mbs -f <number>\n");
				return;
			}

			if ((r = get_current_mbs(p, &n, &m)) ==
			    (struct room *)NULL)
				return;

			if (!CAN_READ_BOARD(p, r->board))
			{
				write_socket(p,
				    "No read access to board %s.\n", n->id);
				return;
			}

			targ = r->board;
			s = r;
			if (targ->followup != (char *)NULL)
			{
				if (!ROOM_POINTER(s, targ->followup))
				{
					log_file("error",
					    "Followup board room missing: "
					    "%s (from %s).", targ->followup,
					    r->fname);
					write_socket(p,
					    "Followup board room not found.\n");
					return;
				}
				if (!(s->flags & R_BOARD))
				{
					log_file("error",
					    "Followup board missing: "
					    "%s (from %s).", targ->followup,
					    r->fname);
					write_socket(p,
					    "Followup board not found.\n");
					return;
				}
				targ = s->board;
			}

			if (!CAN_WRITE_BOARD(p, targ))
			{
				write_socket(p,
				    "You may not post to the followup "
				    "board (%s).\n", s->fname);
				return;
			}

        		if ((msg = find_message(r->board,
			    atoi(argv[2]))) == (struct message *)NULL)
        		{
                		write_socket(p, "No such note, %s\n", argv[2]);
                		return;
        		}

			if (r != s)
				write_socket(p,
				    "--\nNB: Followups set to %s.\n--\n",
				    s->name);

			subj = prepend_re(msg->subject);

			push_string(&p->stack, s->fname);
			push_malloced_string(&p->stack, subj);
			push_malloced_string(&p->stack,
			    quote_message(msg->text, p->quote_char));
                	ed_start(p, post2, MAX_BOARD_LINES, ED_STACKED_TEXT |
			    ((p->medopts & U_EDOPT_SIG) ? ED_APPEND_SIG : 0));
			return;
		    }

		    case 'h':   /* Show a board's unread messages */
			only_new = 1;
			/* Fallthrough */
		    case 'H':   /* Show all a board's messages */
		    {
			struct room *r;
			struct board *b;
			struct message *msg;
			extern void add_header(struct user *, struct strbuf *,
			    struct message *, int);
			int i;

			if ((r = get_current_mbs(p, &n, &m)) ==
			    (struct room *)NULL)
				return;
			b = r->board;
			if (!CAN_READ_BOARD(p, b))
			{
				write_socket(p,
				    "No read access to board %s.\n"
				    "Automatically unsubscribed.\n", n->id);
				remove_umbs(p, n);
				return;
			}
			if (!only_new)
			{
				char *h = get_headers(p, b, 0, 0);
				if (h == (char *)NULL)
				{
					write_socket(p,
					    "No notes on board %s.\n", n->id);
					return;
				}
				more_start(p, h, NULL);
				return;
			}

			/* Else - got to do it the hard way */

			init_strbuf(&buf, 0, "f_mbs h buf");

			for (i = 1, msg = b->messages;
			    msg != (struct message *)NULL;
			    msg = msg->next, i++)
				if (msg->date > n->last)
					add_header(p, &buf, msg, i);
			if (!buf.offset)
			{
				write_socket(p,
				    "No unread notes on board %s.\n",
				    n->id);
				free_strbuf(&buf);
			}
			else
			{
				pop_strbuf(&buf);
				more_start(p, buf.str, NULL);
			}
			return;
		    }

		    case 'K':	/* Trawl board list and check for problems */
			if (p->level < A_MBS_ADMIN)
			{
				write_socket(p, "Permission denied.\n");
				return;
			}
			/* Currently, only check to see if the room exists! */
			for (m = mbs_master; m != (struct mbs *)NULL;
			    m = m->next)
				(void)get_mbs_room(p, m);
			return;

		    case 'n':   /* List boards with new messages.. */
			only_new = 1;
			/* Fallthrough */
		    case 'l':   /* List subscribed boards.. */
			init_strbuf(&buf, 0, "f_mbs l buf");

			for (n = p->umbs; n != (struct umbs *)NULL;
			    n = n->next)
				if ((m = find_mbs(n->id, 0))
				    != (struct mbs *)NULL &&
				    (!only_new || m->last > n->last))
					MBS_ADD_BOARD(m, n);
			if (!buf.offset)
			{
				if (only_new)
					write_socket(p,
					    "No unread notes.\n");
				else
					write_socket(p,
					    "You are not subscribed to any "
					    "boards.\n");
				free_strbuf(&buf);
			}
			else
			{
				pop_strbuf(&buf);
				more_start(p, buf.str, NULL);
			}
			return;

		    case 'L':   /* List all unsubscribed boards */
			init_strbuf(&buf, 0, "f_mbs L buf");

			for (m = mbs_master; m != (struct mbs *)NULL;
			    m = m->next)
				if (find_umbs(p, m->id, 0) ==
				    (struct umbs *)NULL)
					MBS_ADD_BOARD(m, m);
			if (!buf.offset)
			{
				write_socket(p, "No unsubscribed boards.\n");
				free_strbuf(&buf);
			}
			else
			{
				pop_strbuf(&buf);
				more_start(p, buf.str, NULL);
			}
			return;

		    case 'm':	/* Mail a note.. */
		    {
			struct message *msg;
			struct room *r;
			struct board *b;

                        if (argc < 3)
                        {
                                write_socket(p, "Syntax: mbs -m <number>\n");
                                return;
                        }

			if ((r = get_current_mbs(p, &n, &m)) ==
			    (struct room *)NULL)
				return;
			b = r->board;

			if (!CAN_READ_BOARD(p, b))
			{
				write_socket(p,
				    "No read access to board %s.\n"
				    "Automatically unsubscribed.\n", n->id);
				remove_umbs(p, n);
				return;
			}
			if ((msg = find_message(b, atoi(argv[2]))) ==
			    (struct message *)NULL)
			{
				write_socket(p, "No such note, %s\n",
				    argv[2]);
				return;
			}
			if (!deliver_mail(p->rlname, "<Board system>",
			    msg->subject, msg->text, (char *)NULL, 0, 0))
			{
				write_socket(p, "Could not deliver mail to "
				    "you, mailbox full ?\n");
				return;
			}
			write_socket(p, "Mail sent.\n");
			return;
		    }

		    case 'p':	/* Post a note.. */
		    {
			struct room *r;
			extern void post2(struct user *, int);

			if (argc < 3)
			{
				write_socket(p, "Syntax: mbs -p <subject>\n");
				return;
			}

			if ((r = get_current_mbs(p, &n, &m)) ==
			    (struct room *)NULL)
				return;

			if (!CAN_WRITE_BOARD(p, r->board))
			{
				write_socket(p,
				    "No write access to board %s.\n", n->id);
				return;
			}

			push_string(&p->stack, r->fname);
			push_string(&p->stack, argv[2]);
                	ed_start(p, post2, MAX_BOARD_LINES,
			    (p->medopts & U_EDOPT_SIG) ? ED_APPEND_SIG : 0);
                	return;
		    }

		    case 'P':   /* Show all boards.. */
			if (p->level < A_MBS_ADMIN)
			{
				write_socket(p, "Permission denied.\n");
				return;
			}
			init_strbuf(&buf, 0, "f_mbs P buf");

			for (m = mbs_master; m != (struct mbs *)NULL;
			    m = m->next)
				sadd_strbuf(&buf, "%-15s %-15s %s\n",
				    m->id, m->room, m->desc);
			if (!buf.offset)
			{
				write_socket(p, "Mbs contains no boards!\n");
				free_strbuf(&buf);
			}
			else
			{
				pop_strbuf(&buf);
				more_start(p, buf.str, NULL);
			}
			return;

		    case 'r':   /* Read a message */
		    {
			struct room *r;
			struct board *b;
			struct message *msg;
			int num;

			/* If we just typed 'mbs -r' scan for unread */
			if (argc < 3)
			{
				/* Is our current board all read.. */
			    	if (p->mbs_current == (char *)NULL ||
			    	    (m = find_mbs(p->mbs_current, 0)) ==
			    	    (struct mbs *)NULL ||
				    (n = find_umbs(p, p->mbs_current, 0)) ==
				    (struct umbs *)NULL ||
				    n->last >= m->last)
				{
					/* Store current board in case we
					 * don't find any messages */
					char *ombs_current = p->mbs_current;

					p->mbs_current = (char *)NULL;

					/* Let's find a board with some
					 * unread notes */
                 			for (n = p->umbs;
					    n != (struct umbs *)NULL;
                            	    	    n = n->next)
					{
						if ((m = find_mbs(n->id, 0))
						    != (struct mbs *)NULL &&
						    m->last > n->last)
						{
							write_socket(p,
							    "Selected '%s'.\n",
							    m->id);
							p->mbs_current =
							    string_copy(m->id,
							    "umbs current");
							break;
						}
					}
					if (p->mbs_current == (char *)NULL)
						p->mbs_current = ombs_current;
					else
					{
						FREE(ombs_current);
					}
				}
			}

			if ((r = get_current_mbs(p, &n, &m)) ==
			    (struct room *)NULL)
				return;
			b = r->board;
			if (!CAN_READ_BOARD(p, b))
			{
				write_socket(p,
				    "No read access to board %s.\n"
				    "Automatically unsubscribed.\n", n->id);
				remove_umbs(p, n);
				return;
			}
			if (argc < 3)
			{
				if ((msg = mbs_get_first_unread(p, b, n, &num))
				    == (struct message *)NULL)
				{
					write_socket(p, "No unread notes.\n");
					return;
				}
			}
			else
			{
				if ((msg = find_message(b,
				    (num = atoi(argv[2])))) ==
				    (struct message *)NULL)
				{
					write_socket(p,
					    "No such note, %s.\n", argv[2]);
					return;
				}
			}
			if (msg->date > n->last)
				n->last = msg->date;
			write_socket(p, "You read note %d on board %s.\n",
			    num, n->id);
			show_message(p, b, msg, num,
			    (void (*)(struct user *))NULL);
			return;
		    }

		    case 'R':	/* Reply to sender.. */
		    {
			struct room *r;
			struct message *msg;

			if (argc < 3)
			{
				write_socket(p, "Syntax: mbs -R <number>\n");
				return;
			}

			if ((r = get_current_mbs(p, &n, &m)) ==
			    (struct room *)NULL)
				return;

			if (!CAN_READ_BOARD(p, r->board))
			{
				write_socket(p,
				    "No read access to board %s.\n"
				    "Automatically unsubscribed.\n", n->id);
				remove_umbs(p, n);
				return;
			}

        		if ((msg = find_message(r->board,
			    atoi(argv[2]))) == (struct message *)NULL)
        		{
                		write_socket(p, "No such note, %s\n", argv[2]);
                		return;
        		}
			reply_message(p, msg, 0);
			return;
		    }

		    case 's':   /* Subscribe to a new board.. */
		    {
			struct room *r;

			if (argc < 3)
			{
				write_socket(p, "Syntax: mbs -s <board>\n");
				return;
			}
			if ((m = find_mbs(argv[2], 1)) == (struct mbs *)NULL)
			{
				write_socket(p, "No such board: %s\n",
				    argv[2]);
				return;
			}
			if (find_umbs(p, m->id, 0) != (struct umbs *)NULL)
			{
				write_socket(p, "Already subscribed to: %s\n",
				    argv[2]);
				return;
			}
			if ((r = get_mbs_room(p, m)) != (struct room *)NULL &&
			    !CAN_READ_BOARD(p, r->board))
			{
				write_socket(p, "No read access to board.\n");
				return;
			}

			n = create_umbs();
			n->id = string_copy(m->id, "user mbs id");
			add_umbs(p, n);
			write_socket(p, "Subscribed to: %s (%s)\n", n->id,
			    m->desc);
			return;
		    }

		    case 'S':   /* Subscribe to all boards.. */
		    {
			struct room *r;

			if (mbs_master == (struct mbs *)NULL)
			{
				write_socket(p, "Mbs contains no boards!\n");
				return;
			}
			for (m = mbs_master; m != (struct mbs *)NULL;
			    m = m->next)
				if (find_umbs(p, m->id, 0) !=
				    (struct umbs *)NULL)
					write_socket(p,
					    "Already subscribed: %s.\n",
					    m->id);
				else if ((r = get_mbs_room(p, m)) != 
				    (struct room *)NULL &&
				    !CAN_READ_BOARD(p, r->board))
					write_socket(p,
					    "No access:          %s.\n",
					    m->id);
				else
				{
					n = create_umbs();
					n->id = string_copy(m->id,
					    "user mbs id");
					add_umbs(p, n);
					write_socket(p,
					    "Subscribed:         %s (%s).\n",
					    m->id, m->desc);
				}
			return;
		    }

		    case 'u':   /* Unsubscribe to a board.. */
			if (argc < 3)
			{
				write_socket(p, "Syntax: mbs -u <board>\n");
				return;
			}
			if ((n = find_umbs(p, argv[2], 1)) ==
			    (struct umbs *)NULL)
			{
				write_socket(p,
				    "No such board subscribed to.\n");
				return;
			}
			write_socket(p, "Unsubscribed from: %s\n", n->id);
			remove_umbs(p, n);
			return;

		    case 'U':   /* Unsubscribe from all boards.. */
			while (p->umbs != (struct umbs *)NULL)
				remove_umbs(p, p->umbs);
			write_socket(p, "Unsubscribed from all boards.\n");
			return;

		    default:
			write_socket(p, "Unknown option: -%c\n", *argv[1]);
			return;
		}
		/* Not reached.. I hope */
		fatal("Mbs parse dropped through.");
		return;
	}
	if (argc > 2)
	{
		write_socket(p, "Syntax: mbs <board>\n");
		return;
	}
	/* Select new board.. Allow fuzzy match */
	if ((n = find_umbs(p, argv[1], 1)) != (struct umbs *)NULL)
	{
		struct room *r;

		COPY(p->mbs_current, n->id, "umbs current id");
		write_socket(p, "Selected '%s'.\n", n->id);
		/* Access check.. */

		if ((m = find_mbs(n->id, 0)) != (struct mbs *)NULL &&
		    (r = get_mbs_room(p, m)) != (struct room *)NULL &&
		    !CAN_READ_BOARD(p, r->board))
		{
			write_socket(p,
			    "No read access to board %s.\n"
			    "Automatically unsubscribed.\n", n->id);
			remove_umbs(p, n);
		}
		else
		{
			char tmp[] = "h";

			argv[1] = tmp;
			/* Force a list of "headers" */
			f_mbs(p, 2, argv);
		}
	}
	else
		write_socket(p, "No such board.\n");
	return;
}
#undef MBS_ADD_BOARD

void
f_meminfo(struct user *p, int argc, char **argv)
{
	extern void dump_meminfo(struct user *);
	dump_meminfo(p);
}

void
f_nbeep(struct user *p, int argc, char **argv)
{
        struct user *u;

	deltrail(argv[1]);
        if ((u = find_user_msg(p, argv[1])) == (struct user *)NULL)
                return;
	if (!access_check(p, u))
		return;
	if (u->saveflags & U_BEEP_CURSE)
	{
		u->saveflags &= ~U_BEEP_CURSE;
		write_socket(p, "%s is no longer beep cursed.\n", u->capname);
		log_file("curse", "Beepcurse on %s removed by %s.",
	    	    u->capname, p->capname);
		notify_level(p->level, "[ %s beep uncursed by %s. ]",
		    u->capname, p->capname);
	}
	else
	{
		if (argc < 3)
		{
			write_socket(p, "Must supply a reason.\n");
			return;
		}
		u->saveflags |= U_BEEP_CURSE;
		log_file("curse", "%s beepcursed by %s\n(%s)",
		    u->capname, p->capname, argv[2]);
		write_socket(p, "%s is now beep cursed.\n",
		    u->capname);
		notify_level(p->level, "[ %s beep cursed by %s (%s). ]",
		    u->capname, p->capname, argv[2]);
	}
}

void
f_netstat(struct user *p, int argc, char **argv)
{
	extern void dump_netstat(struct user *);
	dump_netstat(p);
}

void
f_objects(struct user *p, int argc, char **argv)
{
	extern void object_dump(struct user *);
	object_dump(p);
}

#define P_RLNAME	0
#define P_ROOM		1
#define P_SNOOP		2
#define P_LEVEL		3
#define P_MAX		4

void
f_people(struct user *p, int argc, char **argv)
{
	static int lengths[P_MAX];
	int num_users, i;

	struct user *ptr;
	struct strbuf buf;

        num_users = count_users(p);
        write_socket(p, "-- %s: %d user%s, %d room%s. (Max: %d, %d)",
            LOCAL_NAME, num_users, num_users == 1 ? "" : "s",
            num_rooms, num_rooms == 1 ? "" : "s", peak_users, peak_rooms);
	write_socket(p, ". %.2f cmds/s, %.2f events/s\n", command_av,
	    event_av);

	for (i = P_MAX; i--; )
		lengths[i] = 0;

	for (ptr = users->next; ptr != (struct user *)NULL; ptr = ptr->next)
	{
		int tmp;
                if ((tmp = strlen(ptr->rlname)) > lengths[P_RLNAME])
                        lengths[P_RLNAME] = tmp;
		if (ptr->super != (struct room *)NULL
		    && ptr->super->fname != (char *)NULL)
		{
			if ((tmp = strlen(ptr->super->fname)) >
			    lengths[P_ROOM])
				lengths[P_ROOM] = tmp;
		}
		else
			lengths[P_ROOM] = lengths[P_ROOM] > 4 ?
			    lengths[P_ROOM] : 4;
		if (ptr->snooped_by != (struct user *)NULL)
		{
			if ((tmp = strlen(ptr->snooped_by->rlname)) >
			    lengths[P_SNOOP])
				lengths[P_SNOOP] = tmp;
		}
		else
			lengths[P_SNOOP] = lengths[P_SNOOP] > 3 ?
			    lengths[P_SNOOP] : 3;
		if ((tmp = strlen(level_name(ptr->level, 0))) >
		    lengths[P_LEVEL])
			lengths[P_LEVEL] = tmp;
	}

	init_strbuf(&buf, 0, "f_people buf");

	for (ptr = users->next; ptr != (struct user *)NULL; ptr = ptr->next)
	{
		if (!CAN_SEE(p, ptr))
			continue;

		/* I believe the word is..... UGH */
		/*   *@ (  NAME  )   LEVEL  G  I   ROOM   SNOOP */
		sadd_strbuf(&buf,
		    "%c%c%-*s%c %-*s %s%c %-*s %-*s %s ",
		    ptr->flags & U_INED ? '*' :
		    (ptr->input_to != NULL_INPUT_TO ? '@' : ' '),
		    ptr->saveflags & U_INVIS ? '(' : ' ',
		    lengths[P_RLNAME],
		    ptr->capname,
		    ptr->saveflags & U_INVIS ? ')' : ' ',
		    lengths[P_LEVEL],
		    lower_case(level_name(ptr->level, 0)),
		    query_gender(ptr->gender, G_LETTER),
		    current_time - ptr->last_command > 900 ? 'I' :
                    current_time - ptr->last_command > 300 ? 'i' : ' ',
		    lengths[P_ROOM],
		    ptr->super == (struct room *)NULL ||
		    ptr->super->fname == (char *)NULL ? "void" :
		    ptr->super->fname,
		    lengths[P_SNOOP],
		    ptr->snooped_by == (struct user *)NULL
		    || ptr->snooped_by->level > p->level ?  "---" :
		    ptr->snooped_by->capname, flags(ptr));

		if (p->level >= A_SEE_UNAME && ptr->uname != (char *)NULL)
			sadd_strbuf(&buf, "%s@", ptr->uname);
		add_strbuf(&buf, lookup_ip_name(ptr));
		cadd_strbuf(&buf, '\n');
	}
	pop_strbuf(&buf);
	more_start(p, buf.str, NULL);
}

void
f_rm(struct user *p, int argc, char **argv)
{
	int err;

	if (!valid_path(p, argv[1], VALID_WRITE))
	{
		write_socket(p, "%s: Permission denied.\n", argv[1]);
		return;
	}

	if (!(err = unlink(argv[1])))
		write_socket(p, "Deleted %s.\n", argv[1]);
	else if (err == -1)
		write_socket(p, "Error: %s\n", perror_text());
	else
		write_socket(p, "Unknown error.\n");
}

void
f_rooms(struct user *p, int argc, char **argv)
{
	struct room *r;
	int col = 0;
	int n = 0;
	char format[10];

	if (argc > 1 && p->level > L_WARDEN)
	{
		if (!strcmp(argv[1], "-reload"))
		{
			extern void init_start_rooms(void);

			init_start_rooms();
			write_socket(p, "Entrance rooms rescanned.\n");
		}
		else
			write_socket(p, "Unknown argument, %s.\n", argv[1]);
		return;
	}

	sprintf(format, "%%-2d: %%-%ds", NAME_LEN);

	write_socket(p, "Currently loaded rooms are:\n");
	for (r = rooms->next; r != (struct room *)NULL; r = r->next)
	{
		write_socket(p, format, ++n, r->fname);
		col++;
		if (col >= 3)
		{
			col = 0;
			write_socket(p, "\n");
		}
	}
	write_socket(p, "\n");
}

#ifdef RUSAGE
void
f_rusage(struct user *p, int argc, char **argv)
{
	extern void print_rusage(struct user *);
	print_rusage(p);
}
#endif

void
f_shutdown(struct user *p, int argc, char **argv)
{
	int seconds, shutdown_warn;
	struct event *ev;
	extern void sig_shutdown(struct event *);

	deltrail(argv[1]);

	if (!strcmp(argv[1], "nokick"))
	{
		write_socket(p, "Your shutdown nokick flag has been %s.\n",
		    (p->saveflags ^= U_SHUTDOWN_NOKICK) & U_SHUTDOWN_NOKICK ?
		    "set" : "unset");
		return;
	}
	
	if (!strcmp(argv[1], "abort"))
	{
		if (shutdown_id == -1)
		{
			write_socket(p,
			    "No shutdown currently in progress.\n");
			return;
		}
		log_file("shutdown", "Shutdown aborted by %s.", p->capname);
		write_socket(p, "Shutdown aborted.\n");
		write_all("Server shutdown aborted.\n");
		remove_event(shutdown_id);
		shutdown_id = -1;
		return;
	}
	if (shutdown_id != -1)
	{
		write_socket(p, "Server is already shutting down.\n");
		return;
	}
	if (argc < 3)
		if (p->level > L_CONSUL)
			argv[2] = "Presumably for a good reason ;-)";
		else
		{
			write_socket(p, "A reason must be supplied.\n");
			return;
		}
	if (!strcmp(argv[1], "now"))
	{
		log_file("shutdown", "Immediate shutdown by %s.\n\t%s",
		    p->capname, argv[2]);
		write_all("%c%cImmediate Server shutdown by %s\n%s\n", 7, 7,
		    p->name, argv[2]);
		write_socket(p, "Shutting down immediately.\n");
		sysflags |= SYS_SHUTDWN;
		return;
	}
	if (!(seconds = atoi(argv[1])))
	{
		write_socket(p, "Invalid time %s\n", argv[1]);
		return;
	}
	log_file("shutdown", "Server shutdown started by %s [%d]\n\t%s.",
	    p->capname, seconds, argv[2]);
	notify_level(p->level, "[ Server shutdown started by %s ]",
	    p->capname);
	write_all("%c%cServer shutdown has been initiated by %s\n", 7, 7,
	    p->name);
	write_all("Server will shut down in %s.\n", conv_time(seconds));
	write_all("%s\n", argv[2]);

	shutdown_warn = seconds / 10;
	if (shutdown_warn < 1)
		shutdown_warn = 1;
	ev = create_event();
	push_number(&ev->stack, seconds);
	shutdown_id = add_event(ev, sig_shutdown, shutdown_warn, "shutdown");
}

void
f_snoop(struct user *p, int argc, char **argv)
{
	struct user *who, *tmp;

	if (argc < 2)
	{
		if (p->snooping == (struct user *)NULL)
		{
			write_socket(p, "You are not snooping anyone.\n");
			return;
		}
		notify_levelabu(p, p->level,
		    "[ Snoop on %s by %s terminated. ]",
		    p->snooping->capname, p->capname);
		write_socket(p, "Snoop on %s terminated.\n",
		    p->snooping->capname);
		if (p->level < L_OVERSEER && p->snooping->level > L_VISITOR)
			write_socket(p->snooping,
			    "You are no longer being snooped by %s.\n",
			    p->name);
		p->snooping->snooped_by = (struct user *)NULL;
		p->snooping = (struct user *)NULL;
		return;
	}
	if (p->level > L_WARDEN && !strncmp(argv[1], "break ", 6))
	{
		argv[1] += 6;
		if (!strlen(argv[1]))
		{
			write_socket(p, "User not supplied.\n");
			return;
		}
	 	if ((who = find_user_msg(p, argv[1])) == (struct user *)NULL)
			return;
		if (!access_check(p, who))
			return;
		if (who->snooping == (struct user *)NULL)
		{
			write_socket(p, "%s is not snooping.\n",
			    who->capname);
			return;
		}
		who->snooping->snooped_by = (struct user *)NULL;
		who->snooping = (struct user *)NULL;
		write_socket(who, "%s broke your snoop.\n",
		    p->name);
		write_socket(p, "Broken.\n");
		return;
	}
	if (p->level > L_WARDEN && !strncmp(argv[1], "force ", 6))
	{
		if (argc != 4)
		{
			write_socket(p, "Syntax: snoop force <by> <on>\n");
			return;
		}
		deltrail(argv[2]);
		if ((who = find_user_msg(p, argv[2])) == (struct user *)NULL)
			return;
		if ((tmp = find_user_msg(p, argv[3])) == (struct user *)NULL)
			return;
		if (!access_check(p, who) || !access_check(p, tmp))
			return;
		argv[1] = argv[3];
		who->flags |= U_SUDO;
		f_snoop(who, 2, argv);
		who->flags &= ~U_SUDO;
		return;
	}
	if ((who = find_user_msg(p, argv[1])) == (struct user *)NULL)
		return;
	if (who == p)
	{
		write_socket(p, "You can't snoop yourself !\n");
		return;
	}
	if (!access_check(p, who))
		return;
	if (p->level < L_OVERSEER && (who->saveflags & U_NOSNOOP))
	{
		write_socket(p, "%s may not be snooped.\n", who->name);
		return;
	}
	if (who->snooped_by)
	{
		write_socket(p, "%s is already snooped by %s.\n",
		    capitalise(who->name), capitalise(who->snooped_by->name));
		return;
	}
	tmp = who;
	while (tmp->snooping != (struct user *)NULL)
		tmp = tmp->snooping;
	if (tmp == p)
	{
		write_socket(p, "Cannot snoop in loops!\n");
		return;
	}
	if (p->snooping != (struct user *)NULL)
	{
		write_socket(p, "Already snooping %s\n", p->snooping->name);
		f_snoop(p, 1, argv);
	}
	log_file("secure/snoop", "Snoop started on %s by %s", who->capname,
	    p->capname);
	if (p->level < L_OVERSEER && who->level > L_VISITOR)
	{
		write_socket(who,
		    "You are now being snooped by %s.\n"
		    "If you are unsure as to what this means, "
		    "type 'help snooped'\n", p->name);
		notify_levelabu(p, p->level, "[ Snoop started on %s by %s. ]",
		    who->capname, p->capname); 
	}
	p->snooping = who;
	who->snooped_by = p;
	write_socket(p, "Snooping %s\n", who->capname);
}

#ifdef STRBUF_STATS
void
f_strbufs(struct user *p, int argc, char **argv)
{
	extern void strbuf_stats(struct user *);
	strbuf_stats(p);
}
#endif

void
f_usercount(struct user *p, int argc, char **argv)
{
	int tot_users;
	int level_count[L_MAX + 1];
	int tot, i, j;

	/* This is the total number of talker users.. */
	tot_users = count_all_users();

	/* Get the number of each level.. excluding resident! */
	for (tot = j = i = 0; i <= L_MAX; i++)
	{
		j = count_level_users(i);
		tot += j;
		level_count[i] = j;
	}
	level_count[L_RESIDENT] = tot_users - tot;

	write_socket(p, "Current talker users:\n"
	    "---------------------\n");
	for (i = L_MAX + 1; --i; )
		write_socket(p, "%-15s: %d\n", capfirst(level_name(i, 0)),
		    level_count[i]);
	write_socket(p, "\n%-15s: %d\n", "Total", tot_users);
}

void
f_watch(struct user *p, int argc, char **argv)
{
	struct strlist *s;

	if (argc < 2)
	{
		/* Default to 'watch n' */
		argc++;
		argv[1] = "n";
		/* And do not deltrail this static string! */
	}
	else
	{
		deltrail(argv[1]);
		if (*argv[1] == '-')
			argv[1]++;
	}
	if (strlen(argv[1]) != 1)
	{
		write_socket(p, "Syntax: watch [-opt [args]]\n");
		return;
	}

	switch (*argv[1])
	{
	    case 'c':	/* Catch-up all files. */
		p->last_watch = current_time;
		write_socket(p, "Caught up on watched files.\n");
		break;

	    case 'l':	/* List subscribed files. */
	    {
		struct strbuf str;

		if (p->watch == (struct strlist *)NULL)
		{
			write_socket(p, "Not watching any files.\n");
			break;
		}
		init_strbuf(&str, 0, "f_watch strbuf");
		add_strbuf(&str, "Watching:\n");

		for (s = p->watch; s != (struct strlist *)NULL; s = s->next)
		{
			cadd_strbuf(&str, '\t');
			add_strbuf(&str, s->str);
			cadd_strbuf(&str, '\n');
		}

		pop_strbuf(&str);
		more_start(p, str.str, NULL);
		break;
	    }

	    case 'n':	/* Check files. */
	    {
		for (s = p->watch; s != (struct strlist *)NULL; s = s->next)
			if (file_time(s->str) > p->last_watch)
				write_socket(p, "*** Watch: %s.\n", s->str);
		break;
	    }

	    case 's':	/* Subscribe to a file.. */
		if (argc < 3)
		{
			write_socket(p,
			    "Syntax: watch -s <file|directory>\n");
			break;
		}
		if (member_strlist(p->watch, argv[2]) != (struct strlist *)NULL)
		{
			write_socket(p, "You are already watching %s\n",
			    argv[2]);
			break;
		}
		if (!valid_path(p, argv[2], VALID_READ))
		{
			write_socket(p, "%s: Permission denied.\n", argv[2]);
			break;
		}

		if (file_size(argv[2]) == -1)
			write_socket(p, "WARNING: %s: File not found.\n",
			    argv[2]);

		s = create_strlist("watch strlist");
		s->str = string_copy(argv[2], "watch");

		add_strlist(&p->watch, s, SL_SORT);
		write_socket(p, "Added %s.\n", argv[2]);
		break;

	    case 'S':	/* Subscribe to all files in a dir. */
	    {		/* Complicated.... *sigh* */
		struct vector *v;
		char buf[MAXPATHLEN + 1];
		char *bufp;
		int i;

		if (argc < 3)
		{
			write_socket(p, "Syntax: watch -S <directory>.\n");
			break;
		}

		/* Strip trailing / characters. */
		i = strlen(argv[2]);
		while (argv[2][i - 1] == '/' && i > 0)
			argv[2][--i] = '\0';

		if (file_size(argv[2]) != -2)
		{
			write_socket(p, "%s: Not a directory.\n", argv[2]);
			break;
		}

		if (!valid_path(p, argv[2], VALID_READ))
		{
			write_socket(p, "%s: Permission denied.\n", argv[2]);
			break;
		}

		if ((v = get_dir(argv[2], 0)) == (struct vector *)NULL)
		{
			write_socket(p, "%s: No files found.\n", argv[2]);
			break;
		}

		strcpy(buf, argv[2]);
		strcat(buf, "/");
		bufp = buf + strlen(argv[2]) + 1;

		for (i = 0; i < v->size; i++)
		{
			strcpy(bufp, v->items[i].u.string);

			if (!valid_path(p, buf, VALID_READ))
				write_socket(p, "%s: Permission denied,"
				    " skipping.\n", buf);
			else if (strchr(v->items[i].u.string, '/') !=
			    (char *)NULL)
				write_socket(p, "%s: Directory, skipping.\n",
				    buf);
			else
			{
				s = create_strlist("watch strlist");
				s->str = string_copy(buf, "watch str");

				add_strlist(&p->watch, s, SL_SORT);
				write_socket(p, "%s: Added.\n", s->str);
			}
		}

		free_vector(v);
		break;
	    }

	    case 'u':	/* Unsubscribe from a file.. */
		if (argc < 3)
		{
			write_socket(p,
			    "Syntax: watch -u <file|directory>.\n");
			break;
		}
		if ((s = member_strlist(p->watch, argv[2])) ==
		    (struct strlist *)NULL)
		{
			write_socket(p, "You are not watching %s\n", argv[2]);
			break;
		}
		remove_strlist(&p->watch, s);
		write_socket(p, "Removed %s.\n", argv[2]);
		break;

	    case 'U':	/* Unsub from all files with prefix.. */
	    {
		struct strlist *s_next;
		int al, flag;

		if (argc < 3)
		{
			write_socket(p, "Syntax: watch -U <prefix>\n");
			break;
		}

		al = strlen(argv[2]);

		for (flag = 0, s = p->watch; s != (struct strlist *)NULL;
		    s = s_next)
		{
			s_next = s->next;

			if (!strcmp(argv[2], "*") ||
			    !strncmp(s->str, argv[2], al))
			{
				write_socket(p, "Removed: %s\n", s->str);
				remove_strlist(&p->watch, s);
				flag = 1;
			}
		}
		if (!flag)
			write_socket(p, "No files found to remove.\n");
		break;
	    }

	    default:
		write_socket(p, "Unknown option, -%c\n", *argv[1]);
	}
}

