/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	user.c
 * Function:	The user object type
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef NeXT
#include <unistd.h>
#endif
#include <time.h>
#include <sys/types.h>
#include <arpa/telnet.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include "jeamland.h"

extern struct termcap_entry termcap[];
struct user *users = (struct user *)NULL;
extern struct room *rooms;
extern int logons;
extern int peak_users;
extern time_t current_time;
extern char *host_name, *host_ip;
extern int port;
extern unsigned long version_code;
extern int num_users;

int
count_users(struct user *p)
{
	struct user *u;
	int num = 0;

	if (p == (struct user *)NULL)
		return num_users;

	for (u = users->next; u != (struct user *)NULL; u = u->next)
		if (CAN_SEE(p, u))
			num++;
	return num;
}

char *
who_text(int source)
{
	struct user *p;
	int cnt = 0;
	struct strbuf mm;

	init_strbuf(&mm, 0, "who_text");

	if (source == FINGER_LOCAL)
		sadd_strbuf(&mm, "%s\n--------------------------\n",
		    LOCAL_NAME);
	else
		sadd_strbuf(&mm, "%s (%s %d)\n--------------------------\n",
		    LOCAL_NAME, host_name, port);

	for (p = users->next; p != (struct user *)NULL; p = p->next)
	{
		if (IN_GAME(p) && !(p->saveflags & U_INVIS))
		{
			if (source == FINGER_SERVICE)
				sadd_strbuf(&mm, "%-10s "
				    "<a href=\"finger.cgi?%s\">%s</a> %s\n",
				    level_name(p->level, 1),
				/* Deliberately didn't use p->rlname here */
				    lower_case(p->name), p->name,
				    p->title != (char *)NULL ? p->title : "");
			else
				sadd_strbuf(&mm, "%-10s %s %s\n",
				    level_name(p->level, 1),
				    p->name, p->title !=
				    (char *)NULL ? p->title : "");
			cnt++;
		}
	}
	if (!cnt)
	{
		add_strbuf(&mm, LOCAL_NAME);
		add_strbuf(&mm, " is deserted at the moment.\n");
	}
	pop_strbuf(&mm);
	return mm.str;
}

void
init_user_list()
{
	struct object *ob;

	/* Create a dummy user for the head of the users list */
	init_user(users = create_user());

	/* Used by the soul in some cases..
	 * (when there is an error in the FEELINGS file) */
	users->gender = G_MALE;

	/* Dummy user should not allocate a 2K buffer!
	 * NB: This will cause problems if the dummy user is ever free'd.
	 *     (This never happens at present)
	 */
	free_tcpsock(&users->socket);

	/* Or a history array *;0) */
	free_vector(users->history);
	users->history = (struct vector *)NULL;

	/* But should have a name set, just in case */
	COPY(users->name, "<DUMMY>", "*dummy user name");
	COPY(users->rlname, "<DUMMY>", "*dummy user name");
	COPY(users->capname, "<DUMMY>", "*dummy user name");

	ob = create_object();
	ob->type = OT_USER;
	ob->m.user = users;
	users->ob = ob;
}

struct user *
create_user()
{
	struct user *p = (struct user *)xalloc(sizeof(struct user),
	    "user struct");

	p->next = (struct user *)NULL;	/* For safety */
	p->history = allocate_vector(HISTORY_SIZE, T_STRING,
	    "user history vector");
	init_tcpsock(&p->socket, USER_BUFSIZE);
	/* For a user, parse special characters */
	p->socket.flags |= TCPSOCK_SPECIAL;
	p->socket.flags |= TCPSOCK_TELNET;
	return p;
}

int
restore_user(struct user *p, char *name)
{
	FILE *fp;
	int got;
	struct stat st;
	char fname[MAXPATHLEN + 1];
	char *buf, *ptr;

	COPY(p->rlname, name, "rlname");
	COPY(p->name, capitalise(p->rlname), "name");
	COPY(p->capname, p->name, "capname");

	sprintf(fname, "users/%c/%s.o", name[0], name);
	if ((fp = fopen(fname, "r")) == (FILE *)NULL)
		return 0;

	if (fstat(fileno(fp), &st) == -1)
	{
		log_perror("fstat");
		fatal("Couldn't stat open file.");
	}

	if (!st.st_size)
	{
		unlink(fname);
		return 0;
	}

	buf = (char *)xalloc((size_t)st.st_size, "restore user");

	got = 0;
	while (fgets(buf, (int)st.st_size, fp) != (char *)NULL)
	{
		/* Talker version code */
		if (sscanf(buf, "version %lx", &p->version))
			continue;
		/* Strings.. (need to be decoded.. ) */
		if (!strncmp(buf, "password \"", 10))
		{
			DECODE(p->passwd, "passwd");
			got++;
			continue;
		}
		if (!strncmp(buf, "capname \"", 9))
		{
			DECODE(p->capname, "capname");
			continue;
		}
		if (!strncmp(buf, "last_ip \"", 9))
		{
			DECODE(p->last_ip, "last ip");
			continue;
		}
		if (!strncmp(buf, "email \"", 7))
		{
			DECODE(p->email, "email");
			continue;
		}
		if (!strncmp(buf, "url \"", 5))
		{
			DECODE(p->url, "url");
			continue;
		}
		if (!strncmp(buf, "title \"", 7))
		{
			DECODE(p->title, "title");
			continue;
		}
		if (!strncmp(buf, "prompt \"", 8))
		{
			DECODE(p->prompt, "prompt");
			continue;
		}
		if (!strncmp(buf, "plan \"", 6))
		{
			DECODE(p->plan, "plan");
			continue;
		}
		if (!strncmp(buf, "sig \"", 5))
		{
			DECODE(p->sig, ".sig");
			continue;
		}
		if (!strncmp(buf, "startloc \"", 10))
		{
			DECODE(p->startloc, "startloc");
			continue;
		}
		if (!strncmp(buf, "validator \"", 11))
		{
			DECODE(p->validator, "validator");
			continue;
		}
		if (!strncmp(buf, "comment \"", 9))
		{
			DECODE(p->comment, "comment");
			continue;
		}
		if (!strncmp(buf, "leavemsg \"", 10))
		{
			DECODE(p->leavemsg, "leavemsg");
			continue;
		}
		if (!strncmp(buf, "entermsg \"", 10))
		{
			DECODE(p->entermsg, "entermsg");
			continue;
		}
		if (!strncmp(buf, "alias ", 6))
		{
			restore_alias(&p->alias, buf, "user", name);
			continue;
		}
		if (!strncmp(buf, "mbs ", 4))
		{
			struct svalue *sv;

			if ((sv = decode_one(buf, "decode_one umbs")) ==
			    (struct svalue *)NULL)
				continue;
			if (sv->type == T_VECTOR && sv->u.vec->size == 2 &&
			    sv->u.vec->items[0].type == T_STRING &&
			    sv->u.vec->items[1].type == T_NUMBER)
			{
				struct umbs *m = create_umbs();

				m->id = string_copy(
				    sv->u.vec->items[0].u.string,
				    "umbs id");
				m->last = (time_t)sv->u.vec->items[1].u.number;
				add_umbs(p, m);
			}
			else
				log_file("error", "Error in mbs line: ",
				    "user: %s, line: %s", name, buf);
			free_svalue(sv);
			continue;
		}
		if (!strncmp(buf, "grupe ", 6))
		{
			restore_grupes(&p->grupes, buf);
			continue;
		}
		if (!strncmp(buf, "watch ", 6))
		{
			free_strlists(&p->watch);
			p->watch = decode_strlist(buf, SL_SORT);
			continue;
		}
		if (!strncmp(buf, "chan_earmuff ", 13))
		{
			free_strlists(&p->chan_earmuff);
			p->chan_earmuff = decode_strlist(buf, SL_SORT);
			continue;
		}
		if (!strncmp(buf, "col_attr ", 9))
		{
			struct svalue *sv;

			if ((sv = decode_one(buf, "decode_one col_attr")) ==
			    (struct svalue *)NULL)
				continue;
			if (sv->type == T_MAPPING)
			{
				if (p->attr != (struct mapping *)NULL)
					free_mapping(p->attr);
				p->attr = sv->u.map;
				/* Don't want the mapping freed */
				sv->type = T_EMPTY;
			}
			else
				log_file("error", "Error in attr line: ",
				    "user: %s, line: %s", name, buf);
			free_svalue(sv);
			continue;
		}
		if (!strncmp(buf, "user_attr ", 10))
		{
			struct svalue *sv;

			if ((sv = decode_one(buf, "decode_one user_attr")) ==
			    (struct svalue *)NULL)
				continue;
			if (sv->type == T_MAPPING)
			{
				if (p->uattr != (struct mapping *)NULL)
					free_mapping(p->uattr);
				p->uattr = sv->u.map;
				/* Don't want the mapping freed */
				sv->type = T_EMPTY;
			}
			else
				log_file("error", "Error in uattr line: ",
				    "user: %s, line: %s", name, buf);
			free_svalue(sv);
			continue;
		}
		if (sscanf(buf, "level %d", &p->level))
		{
			got++;
			continue;
		}
		if (sscanf(buf, "saveflags %ld", &p->saveflags))
			continue;
		if (sscanf(buf, "medopts %u", &p->medopts))
			continue;
		if (sscanf(buf, "terminal %u", &p->terminal))
			continue;
		if (sscanf(buf, "morelen %u", &p->morelen))
			continue;
		if (sscanf(buf, "screenwidth %u", &p->screenwidth))
			continue;
		if (sscanf(buf, "last_login %ld", (long *)&p->last_login))
			continue;
		if (sscanf(buf, "time_on %ld", (long *)&p->time_on))
			continue;
		if (sscanf(buf, "hibernate %ld", (long *)&p->hibernate))
			continue;
		if (sscanf(buf, "gender %d", &p->gender))
			continue;
		if (sscanf(buf, "earmuffs %u", &p->earmuffs))
			continue;
		if (sscanf(buf, "bad_logins %u", &p->bad_logins))
			continue;
		if (sscanf(buf, "alias_lim %d", &p->alias_lim))
			continue;
		if (sscanf(buf, "quote_char %c", &p->quote_char))
			continue;
		if (sscanf(buf, "postings %u", &p->postings))
			continue;
		if (sscanf(buf, "num_logins %u", &p->num_logins))
			continue;
		if (sscanf(buf, "tz_adjust %d", &p->tz_adjust))
			continue;
		if (sscanf(buf, "last_watch %ld", (long *)&p->last_watch))
			continue;
	}
	fclose(fp);
	xfree(buf);
	if (p->level > L_MAX)
		p->level = L_MAX;
	if (p->level < L_VISITOR)
		p->level = L_VISITOR;
	if (!got)
		return 0;
	return 1;
}

int
save_user(struct user *p, int live, int force)
{
	FILE *fp;
	char buf[MAXPATHLEN + 1];
	char *ptr = (char *)NULL;
	struct alias *a;
	struct umbs *m;

	if  (p->level < L_RESIDENT || p->passwd == (char *)NULL ||
	    (p->email == (char *)NULL && !(p->saveflags & U_NOEMAIL)) ||
	    (!force && !IN_GAME(p)))
		return 0;

	sprintf(buf, "users/%c/%s.o", p->rlname[0], p->rlname);
	if ((fp = fopen(buf, "w")) == (FILE *)NULL)
		return 0;
	fprintf(fp, "version %lx\n", version_code);
	ENCODE(p->passwd);
	fprintf(fp, "password \"%s\"\n", ptr);
	if (p->email != (char *)NULL)
	{
		ENCODE(p->email);
		fprintf(fp, "email \"%s\"\n", ptr);
		p->saveflags &= ~U_NOEMAIL;
	}
	ENCODE(p->capname);
	fprintf(fp, "capname \"%s\"\n", ptr);
	if (p->url != (char *)NULL)
	{
		ENCODE(p->url);
		fprintf(fp, "url \"%s\"\n", ptr);
	}
	if (p->title != (char *)NULL)
	{
		ENCODE(p->title);
		fprintf(fp, "title \"%s\"\n", ptr);
	}
	if (p->prompt != (char *)NULL)
	{
		ENCODE(p->prompt);
		fprintf(fp, "prompt \"%s\"\n", ptr);
	}
	if (p->plan != (char *)NULL)
	{
		ENCODE(p->plan);
		fprintf(fp, "plan \"%s\"\n", ptr);
	}
	if (p->sig != (char *)NULL)
	{
		ENCODE(p->sig);
		fprintf(fp, "sig \"%s\"\n", ptr);
	}
	if (p->startloc != (char *)NULL)
	{
		ENCODE(p->startloc);
		fprintf(fp, "startloc \"%s\"\n", ptr);
	}
	if (p->validator != (char *)NULL)
	{
		ENCODE(p->validator);
		fprintf(fp, "validator \"%s\"\n", ptr);
	}
	if (p->comment != (char *)NULL)
	{
		ENCODE(p->comment);
		fprintf(fp, "comment \"%s\"\n", ptr);
	}
	if (p->entermsg != (char *)NULL)
	{
		ENCODE(p->entermsg);
		fprintf(fp, "entermsg \"%s\"\n", ptr);
	}
	if (p->leavemsg != (char *)NULL)
	{
		ENCODE(p->leavemsg);
		fprintf(fp, "leavemsg \"%s\"\n", ptr);
	}
	if (live)
	{
		fprintf(fp, "last_login %ld\n", (long)p->login_time);
		fprintf(fp, "last_ip \"%s\"\n", lookup_ip_name(p));
		fprintf(fp, "time_on %ld\n", (long)(p->time_on + current_time -
		    p->login_time));
	}
	else
	{
		fprintf(fp, "last_login %ld\n", (long)p->last_login);
		if (p->last_ip != (char *)NULL)
			fprintf(fp, "last_ip \"%s\"\n", p->last_ip);
		fprintf(fp, "time_on %ld\n", (long)p->time_on);
	}
	fprintf(fp, "level %d\n", p->level);
	fprintf(fp, "saveflags %ld\n", p->saveflags);
	fprintf(fp, "medopts %u\n", p->medopts);
	fprintf(fp, "terminal %u\n", p->terminal);
	fprintf(fp, "screenwidth %u\n", p->screenwidth);
	fprintf(fp, "morelen %u\n", p->morelen);
	fprintf(fp, "gender %d\n", p->gender);
	fprintf(fp, "earmuffs %u\n", p->earmuffs);
	fprintf(fp, "hibernate %ld\n", (long)p->hibernate);
	fprintf(fp, "bad_logins %u\n", p->bad_logins);
	fprintf(fp, "alias_lim %d\n", p->alias_lim);
	fprintf(fp, "quote_char %c\n", p->quote_char);
	fprintf(fp, "postings %u\n", p->postings);
	fprintf(fp, "num_logins %u\n", p->num_logins);
	fprintf(fp, "tz_adjust %d\n", p->tz_adjust);
	for (a = p->alias; a != (struct alias *)NULL; a = a->next)
	{
		char *key, *fob;

		key = code_string(a->key);
		fob = code_string(a->fob);
		fprintf(fp, "alias ({\"%s\",\"%s\",})\n", key, fob);
		xfree(key);
		xfree(fob);
	}
	for (m = p->umbs; m != (struct umbs *)NULL; m = m->next)
	{
		char *id = code_string(m->id);

		fprintf(fp, "mbs ({\"%s\",%ld,})\n", id, (long)m->last);

		xfree(id);
	}
	store_grupes(fp, p->grupes);
	write_strlist(fp, "watch", p->watch);
	fprintf(fp, "last_watch %ld\n", (long)p->last_watch);
	write_strlist(fp, "chan_earmuff", p->chan_earmuff);
	if (p->attr != (struct mapping *)NULL)
	{
		struct svalue sv;
		char *str;

		sv.type = T_MAPPING;
		sv.u.map = p->attr;
		str = svalue_to_string(&sv);
		fprintf(fp, "col_attr %s\n", str);
		xfree(str);
	}
	if (p->uattr != (struct mapping *)NULL)
	{
		struct svalue sv;
		char *str;

		sv.type = T_MAPPING;
		sv.u.map = p->uattr;
		str = svalue_to_string(&sv);
		fprintf(fp, "user_attr %s\n", str);
		xfree(str);
	}

	fclose(fp);
	/* This extra free is important! */
	FREE(ptr);
	return 1;
}

struct user *
dead_copy(char *name)
{
	struct user *p;

	if (!exist_user(lower_case(name)))
		return (struct user *)NULL;

	init_user(p = create_user());
	if (!restore_user(p, lower_case(name)))
	{
		tfree_user(p);
		return (struct user *)NULL;
	}
	return p;
}

struct user *
with_user_common(struct user *u, char *who, struct strbuf *b)
{
	struct user *p, *last;
	struct object *ob;
	int flag = 0;
	char *tmp;

	if (who == (char *)NULL || !strlen(who))
		return (struct user *)NULL;

	tmp = lower_case(who);

	for (ob = u->super->ob->contains; ob != (struct object *)NULL;
	    ob = ob->next_contains)
	{
		if (ob->type != OT_USER || !IN_GAME(ob->m.user))
			continue;
		p = ob->m.user;
		if (!strcmp(p->rlname, tmp))
		{
			if (b != (struct strbuf *)NULL)
				reinit_strbuf(b);
			return p;
		}
		else if (!strncasecmp(p->rlname, tmp, strlen(tmp)))
		{
			if (b != (struct strbuf *)NULL)
			{
				if (b->offset)
					add_strbuf(b, ", ");
				add_strbuf(b, p->name);
			}
			/* Handle preferred grupe members */
			if (u != (struct user *)NULL &&
			    member_grupe(u->grupes, "_preferred",
			    lower_case(p->name), 0, 0))
			{
				if (b != (struct strbuf *)NULL)
					reinit_strbuf(b);
				return p;
			}
			last = p;
			flag++;
		}
	}
	if (flag != 1)
	{
		if (!flag)
		{
			if (!strcmp(tmp, "me"))
				return u;
			return (struct user *)NULL;
		}
		return (struct user *)1;
	}
	return last;
}

struct user *
with_user(struct user *u, char *who)
{
	u = with_user_common(u, who, (struct strbuf *)NULL);
	if (u == (struct user *)1)
		return (struct user *)NULL;
	return u;
}

struct user *
with_user_msg(struct user *p, char *who)
{
	struct user *u;
	struct strbuf b;

#ifdef DEBUG
	if (p == (struct user *)NULL)
		fatal("with_user_msg with null user");
#endif

	init_strbuf(&b, 0, "with_user_msg");

	u = with_user_common(p, who, &b);

	if (u == (struct user *)1)
	{
		write_socket(p, "Ambiguous username, %s [%s]\n",
		    capitalise(who), b.str);
		u = (struct user *)NULL;
	}
	else if (u == (struct user *)NULL)
		write_socket(p, "User not found, %s.\n", capitalise(who));

	free_strbuf(&b);
	return u;
}

struct user *
find_user_common(struct user *u, char *who, struct strbuf *b)
{
	struct user *p, *last;
	int flag = 0;
	int num, n = 0;
	char *tmp;

	if (who == (char *)NULL || !strlen(who))
		return (struct user *)NULL;

	if (who[0] == '#')
		num = atoi(who + 1);
	else
		num = -1;

	tmp = lower_case(who);

	for (p = users->next; p != (struct user *)NULL; p = p->next)
	{
		if (num == ++n && u->level >= L_OVERSEER)
			return p;
		if (!IN_GAME(p))
			continue;
		if (!strcmp(p->rlname, tmp))
		{
			if (u != (struct user *)NULL && !CAN_SEE(u, p))
				continue;
			else
			{
				if (b != (struct strbuf *)NULL)
					reinit_strbuf(b);
				return p;
			}
		}
		else if (!strncasecmp(p->rlname, tmp, strlen(tmp)) &&
		    (u == (struct user *)NULL || CAN_SEE(u, p)))
		{
			if (b != (struct strbuf *)NULL)
			{
				if (b->offset)
					add_strbuf(b, ", ");
				add_strbuf(b, p->name);
			}
			/* Handle preferred grupe members */
			if (u != (struct user *)NULL &&
			    member_grupe(u->grupes, "_preferred",
			    lower_case(p->name), 0, 0))
			{
				if (b != (struct strbuf *)NULL)
					reinit_strbuf(b);
				return p;
			}
			last = p;
			flag++;
		}
	}
	if (flag != 1)
	{
		if (!flag)
		{
			if (!strcmp(tmp, "me"))
				return u;
			return (struct user *)NULL;
		}
		return (struct user *)1;
	}
	return last;
}

struct user *
find_user_msg(struct user *p, char *who)
{
	struct user *u;
	struct strbuf b;

#ifdef DEBUG
	if (p == (struct user *)NULL)
		fatal("find_user_msg with null user");
#endif

	init_strbuf(&b, 0, "find_user_msg");

	u = find_user_common(p, who, &b);

	if (u == (struct user *)1)
	{
		write_socket(p, "Ambiguous username, %s [%s]\n",
		    capitalise(who), b.str);
		u = (struct user *)NULL;
	}
	else if (u == (struct user *)NULL)
		write_socket(p, "User not found, %s.\n", capitalise(who));

	free_strbuf(&b);
	return u;
}

struct user *
find_user(struct user *u, char *who)
{
	u = find_user_common(u, who, (struct strbuf *)NULL);
	if (u == (struct user *)1)
		return (struct user *)NULL;
	return u;
}

struct user *
find_user_absolute(struct user *u, char *who)
{
	struct user *p;
	char *tmp;

	if (who == (char *)NULL || !strlen(who))
		return (struct user *)NULL;

	tmp = lower_case(who);

	for (p = users->next; p != (struct user *)NULL; p = p->next)
	{
		if (!IN_GAME(p))
			continue;
		if (!strcmp(p->rlname, tmp))
		{
			if (u != (struct user *)NULL && !CAN_SEE(u, p))
				continue;
			else
				return p;
		}
	}
	return (struct user *)NULL;
}

int
exist_user(char *name)
{
	char fname[MAXPATHLEN + 1];
	
	sprintf(fname, "users/%c/%s.o", name[0], name);
	if (file_size(fname) < 1)
		return 0;
	return 1;
}

void
move_user(struct user *p, struct room *r)
{
	extern void f_look(struct user *, int, char **);
	struct room *old = p->super;
	char *cookie;

	if (p == (struct user *)NULL || r == (struct room *)NULL)
		fatal("move_user with null arguments.");

	if (old == r)
	{
		write_socket(p, "You are already there!\n");
		return;
	}	

	/* Hmm.. is this the right place for this ? ...
	 * Possible to have another field in the user for
	 * 'room in which last note was read' and update it in the
	 * 'next' command code - maybe cleaner but more memory use.
	 */
	p->last_note = 0;

	if (p->flags & U_SOCK_CLOSING)
	{
		/* Shhhhhh.. */
		move_object(p->ob, r->ob);
		return;
	}
	else if (p->saveflags & U_INVIS)
	{
		move_object(p->ob, r->ob);
		f_look(p, 0, (char **)NULL);
		return;
	}

#ifdef DEPART_ALIAS
	if (find_alias(old->alias, DEPART_ALIAS) != (struct alias *)NULL)
	{
		char *cmd = string_copy(DEPART_ALIAS, "move_user");
		/*reset_eval_depth();*/
		p->flags |= U_INHIBIT_QUIT;
		parse_command(p, &cmd);
		p->flags &= ~U_INHIBIT_QUIT;
		xfree(cmd);
	}
#endif

	if (p->entermsg != (char *)NULL)
		cookie = parse_cookie(p, p->entermsg);
	else
		cookie = parse_cookie(p, DEFAULT_ENTERMSG);
	tell_room(r, "%s\n", cookie);
	xfree(cookie);

	move_object(p->ob, r->ob);

	if (old != (struct room *)NULL)
	{
		if (p->leavemsg != (char *)NULL)
			cookie = parse_cookie(p, p->leavemsg);
		else
			cookie = parse_cookie(p, DEFAULT_LEAVEMSG);
		tell_room(old, "%s\n", cookie);
		xfree(cookie);
	}
	f_look(p, 0, (char **)NULL);

#ifdef ARRIVE_ALIAS
	if (find_alias(r->alias, ARRIVE_ALIAS) != (struct alias *)NULL)
	{
		char *cmd = string_copy(ARRIVE_ALIAS, "move_user");
		/*reset_eval_depth();*/
		p->flags |= U_INHIBIT_QUIT;
		parse_command(p, &cmd);
		p->flags &= ~U_INHIBIT_QUIT;
		xfree(cmd);
	}
#endif
}

void
newmail_check(struct user *p)
{
	struct board *b;

	b = restore_mailbox(p->rlname);
	if (b->flags & B_NEWM)
		write_socket(p, "\n%c%c%c\t*** YOU HAVE NEW MAIL ***\n",
		    7, 7, 7);
	else if (b->messages != (struct message *)NULL)
		write_socket(p, "\n\t*** You have mail ***\n");
	if (b->limit != -1)
	{
		if (b->limit != -1 && b->num >= b->limit)
			write_socket(p,
			    "\n\t*** WARNING: Your mailbox is full. ***\n"
			    "\t*** You are currently unable to receive new "
			    "mail. ***\n");
		else if (b->limit != -1 && b->num >= b->limit * 3 / 4)
			write_socket(p, "%s",
			    "\n\t*** WARNING: Your mailbox is over 75% full"
			    " ***\n");
	}
	free_board(b);
}

static void
dead_ed_check(struct user *p)
{
	char buff[MAXPATHLEN + 1];

	sprintf(buff, "dead_ed/%s", p->rlname);

	if (file_time(buff) != -1)
		write_socket(p, "\n\t*** A copy of your editor buffer "
		    "was saved when\n"
		    "\t*** you lost link. Use the ~r command in the editor\n"
		    "\t*** to recover it.\n");
}

static void
login_user2(struct user *p, int partial)
{
	FUN_START("login_user2");
#ifdef STARTUP_ALIAS
	if (find_alias(p->alias, STARTUP_ALIAS) != (struct alias *)NULL)
	{
		char *cmd = string_copy(STARTUP_ALIAS, "login_user2 cmd");
		p->flags |= U_INHIBIT_QUIT;
		reset_eval_depth();
		parse_command(p, &cmd);
		p->flags &= ~U_INHIBIT_QUIT;
		xfree(cmd);
	}
#endif
	FUN_LINE;

	/* Are they required to change their password ?
	 * This comes after everything else as it just jumps into the
	 * normal password setting routine.
	 */
	if (!partial && (p->saveflags & U_FPC))
	{
		extern void new_passwd(struct user *, char *);

		write_socket(p,
		    "You are required to choose a new password.\n");
		noecho(p);
		/* First one doesn't always seem to work.. why ? */
		noecho(p);
		write_socket(p, "New password: ");
		p->input_to = new_passwd;
		p->flags |= U_LOOP_PASSWD;
	}
	FUN_END;
}

static void
sticky_check_ret(struct user *p)
{
	int g = p->stack.sp->u.number;
	reset(p);
	write_socket(p, "\n");
	dec_stack(&p->stack);
	login_user2(p, g);
}

static void
sticky_check(struct user *p)
{
	char *f;
	char fname[MAXPATHLEN + 1];

	sprintf(fname, "mail/%s#sticky", p->rlname);
	if ((f = read_file(fname)) == (char *)NULL)
		sticky_check_ret(p);
	else
	{
		write_socket(p, "\n");
		attr_colour(p, "notify");
		more_start(p, f, sticky_check_ret);
	}
}

void
login_user(struct user *p, int partial)
{
	struct user *ptr;
	struct room *r;
	int us;
	extern time_t mail_time(char *);
	extern int check_supersnooped(char *);
	extern void newbie_tuneout(struct user *);

	FUN_START("login_user");

	reset_eval_depth();

#if defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT) || defined(IMUD3_SUPPORT)
#ifdef NEWBIE_TUNEOUT
	if (!p->num_logins && p->chan_earmuff == (struct strlist *)NULL)
		newbie_tuneout(p);
#endif
#endif

	if (!partial)
	{
		logons++;

		FUN_LINE;

		/* Disregard this login if it is less than 30 minutes since
		 * this user last logged in. */
		if (current_time - p->last_login > 1800)
			p->num_logins++;

		COPY(p->name, p->capname, "name");

		write_socket(p, "\n");
		dump_file(p, "etc", F_MOTD, DUMP_HEAD);
		log_file("enter", "%s (%d) from %s.", p->capname, p->level,
		    lookup_ip_name(p));
		log_file("secure/enter", "%s (%d) from %s@%s.", p->capname,
	    	    p->level, p->uname == (char *)NULL ? "unknown" : p->uname,
	    	    lookup_ip_name(p));
		for (ptr = users->next; ptr != (struct user *)NULL;
		    ptr = ptr->next)
			if (!(ptr->earmuffs & EAR_NOTIFY) && IN_GAME(ptr) &&
		    	    (!(p->saveflags & U_SLY_LOGIN) ||
			    ptr->level >= p->level))
			{
				attr_colour(ptr, "notify");
				if (ptr->level >= A_SEE_UNAME)
					write_socket(ptr,
					    "[ %s [%s] (%s@%s) has "
					    "connected. ]",
					    p->capname,
					    capfirst(level_name(p->level, 0)),
					    p->uname == (char *)NULL ?
					    "unknown" : p->uname,
					    lookup_ip_name(p));
				else
					write_socket(ptr,
					    "[ %s [%s] has connected. ]",
					    p->capname,
					    capfirst(level_name(p->level, 0)));
				reset(ptr);
				write_socket(ptr, "\n");
				if (ptr->saveflags & U_BEEP_LOGIN)
					write_socket(ptr, "%c%c", 7, 7);
			}

#ifdef REMOTE_NOTIFY
		send_rnotify(p, "+login");
#endif

		if (!(p->flags & U_LOGGING_IN))
			fatal("Login_user on logged in user!");
		p->flags ^= U_LOGGING_IN;

		FUN_LINE;
				    
		if (p->saveflags & U_SLY_LOGIN)
			/* To the void */
			move_object(p->ob, rooms->ob);
		else if (p->startloc == (char *)NULL)
			move_user(p, get_entrance_room());
		else
		{
			if (!ROOM_POINTER(r, p->startloc))
			{
				write_socket(p,
				    "Your startlocation is corrupt!\n");
				/* To the void */
				move_object(p->ob, rooms->ob);
			}
			else if ((r->flags & R_LOCKED) &&
		    	    strcmp(p->rlname, r->owner))
			{
				write_socket(p,
				    "Your startlocation is locked!\n");
				/* To the void */
				move_object(p->ob, rooms->ob);
			}
			else
				move_user(p, r);
		}
	}
	FUN_LINE;
	if (p->saveflags & U_INVIS)
	{
		/*COPY(p->name, "Someone", "name");*/
		write_socket(p, "\n\t*** YOU ARE INVISIBLE ***\n");
	}
	if (p->saveflags & U_SLY_LOGIN)
		write_socket(p,
		    "\n\t*** YOU HAVE SLY LOGIN MODE TURNED ON ***\n");
	if (p->last_login != (time_t)0 && p->last_ip != (char *)NULL)
		write_socket(p, "\nLast login: %s from %s\n", 
	    	    nctime(user_time(p, p->last_login)), p->last_ip);
	if (p->bad_logins)
	{
		attr_colour(p, "notify");
		write_socket(p,
		    "\n\t*** %d unsuccessful login attempt%s since last "
		    "login. ***",
		    p->bad_logins, p->bad_logins == 1 ? "" : "s");
		p->bad_logins = 0;
		reset(p);
		write_socket(p, "\n");
	}

	FUN_LINE;

	if (p->version && (r = get_entrance_room()) != rooms)
	{
		struct mbs *m;

		if ((m = find_mbs_by_room(r->fname)) != (struct mbs *)NULL &&
		    m->last > p->last_login)
			write_socket(p, "\n\t*** New note(s) on the "
			    "entrance room board. ***\n");
	}
	if (p->version && version_code > p->version)
	{
		write_socket(p, "\n"
		    "\t*** A NEW VERSION OF JEAMLAND HAS BEEN INSTALLED ***\n"
		    "\t***           PLEASE READ 'help CHANGES'         ***\n");

	}

	write_socket(p, "\n");

	/* If we use a client, handle telnet codes. */
	if (p->saveflags & U_IACEOR)
	{
		write_socket(p, "%c%c%c", IAC, WILL, TELOPT_EOR);
		p->socket.telnet.expect |= TN_EXPECT_EOR;
	}
	if (p->saveflags & U_IACGA)
	{
		write_socket(p, "%c%c%c", IAC, WONT, TELOPT_SGA);
		p->socket.telnet.expect |= TN_EXPECT_SGA;
	}
		
	newmail_check(p);
	dead_ed_check(p);

	/* Compare stored level with mastersave level.. and resolve
	 * differences */
	if ((us = query_real_level(p->rlname)) != p->level && (us != -1 ||
	    p->level > L_RESIDENT))
	{
		write_socket(p,
		    "*** YOUR LEVEL HAS CHANGED IN YOUR ABSENCE. ***\n");
		p->level = (us != -1) ? us : L_RESIDENT;
	}

	if ((us = count_users((struct user *)NULL)) > peak_users)
		peak_users = us;

#ifdef SUPER_SNOOP
	if (p->snoop_fd == -1)
		p->snoop_fd = check_supersnooped(p->rlname);
#endif

	FUN_LINE;

	reposition(p);

	push_number(&p->stack, partial);
	sticky_check(p);

	FUN_END;
}
	
void
reposition(struct user *p)
{
	struct user *ptr;

	/* Move the user to the correct place in the list.
	 * First, let's take him out of the current list. */
	for (ptr = users; ptr != (struct user *)NULL; ptr = ptr->next)
		if (ptr->next == p)
		{
			ptr->next = ptr->next->next;
			break;
		}

	/* Now, let's put him in the position he belongs. */
	for (ptr = users; ptr != (struct user *)NULL; ptr = ptr->next)
	{
		if (ptr->next == (struct user *)NULL ||
		    !IN_GAME(ptr->next) ||
		    ptr->next->level < p->level ||
		    (ptr->next->level == p->level &&
		    strcmp(p->rlname, ptr->next->rlname) < 0))
		{
			p->next = ptr->next;
			ptr->next = p;
			break;
		}
	}
	/* et. voila! */
}

void
save_all_users()
{
	struct user *p;
	
	for (p = users->next; p != (struct user *)NULL; p = p->next)
		save_user(p, 1, 0);
}

void
init_user(struct user *p)
{
	int i;

	/* This initialisation is not pretty... */
	p->version = 0;
	/* Free.. */
	p->name = (char *)NULL;
	p->capname = (char *)NULL;
	p->rlname = (char *)NULL;
	p->passwd = (char *)NULL;
	p->last_ip = (char *)NULL;
	p->email = (char *)NULL;
	p->url = (char *)NULL;
	p->title = (char *)NULL;
	p->prompt = (char *)NULL;
	p->uname = (char *)NULL;
	p->plan = (char *)NULL;
	p->sig = (char *)NULL;
	p->startloc = (char *)NULL;
	p->validator = (char *)NULL;
	p->reply_to = (char *)NULL;
	p->mbs_current = (char *)NULL;
	p->comment = (char *)NULL;
	p->entermsg = (char *)NULL;
	p->leavemsg = (char *)NULL;

	/* Temp variables */
	init_stack(&p->alias_stack);
	init_stack(&p->stack);
	init_stack(&p->atexit);

	/* Nofree.. */
	p->quote_char = DEFAULT_QUOTE_CHAR;
	p->history_ptr = 1;
	p->col = 0;
	p->last_command = current_time;
	p->snoop_fd = -1;
	p->flags = 0;
	p->saveflags = 0;
	p->medopts = U_EDOPT_AUTOLIST | U_EDOPT_RULER | U_EDOPT_WRAP |
	    U_EDOPT_PROMPT;
	p->terminal = 0;
	p->gender = G_NEUTER;
	p->earmuffs = 0;
	p->screenwidth = DEFAULT_SCREENWIDTH;
	p->morelen = DEFAULT_MORELEN;
	p->input_to = NULL;
	p->time_on = 0;
	p->last_login = 0;
	p->hibernate = 0;
	p->bad_logins = 0;
	p->postings = 0;
	p->num_logins = 0;
	p->tz_adjust = 0;
	p->last_command = 0;
	p->alias_lim = ALIAS_LIMIT;
	p->alias_indent = 0;
	p->last_watch = 0;
	p->last_note = 0;

	p->snooping = p->snooped_by = (struct user *)NULL;
	p->super = (struct room *)NULL;

	/* List.. */
	p->alias = (struct alias *)NULL;
	p->umbs = (struct umbs *)NULL;
	p->grupes = (struct grupe *)NULL;
	p->watch = (struct strlist *)NULL;
	p->ed = (struct ed_buffer *)NULL;
	p->more = (struct more_buf *)NULL;
	p->mailbox = (struct board *)NULL;
	p->chan_earmuff = (struct strlist *)NULL;
	p->attr = (struct mapping *)NULL;
	p->uattr = (struct mapping *)NULL;

	/* Other.. */
	for (i = NUM_JOBS; i--; )
		p->jobs[i].flags = 0;
}

void
free_user(struct user *p)
{
	extern void tfree_ed(struct user *, int), tfree_more(struct user *);

	if (p->ed != (struct ed_buffer *)NULL)
		tfree_ed(p, 1);

	/* Free.. */
	FREE(p->name);
	FREE(p->capname);
	FREE(p->rlname);
	FREE(p->passwd);
	FREE(p->last_ip);
	FREE(p->email);
	FREE(p->url);
	FREE(p->title);
	FREE(p->prompt);
	FREE(p->uname);
	FREE(p->plan);
	FREE(p->sig);
	FREE(p->startloc);
	FREE(p->validator);
	FREE(p->reply_to);
	FREE(p->mbs_current);
	FREE(p->comment);
	FREE(p->leavemsg);
	FREE(p->entermsg);

	free_aliases(&p->alias);
	free_umbses(p);
	free_grupes(&p->grupes);
	free_strlists(&p->watch);
	free_strlists(&p->chan_earmuff);
	if (p->attr != (struct mapping *)NULL)
		free_mapping(p->attr);
	p->attr = (struct mapping *)NULL;
	if (p->uattr != (struct mapping *)NULL)
		free_mapping(p->uattr);
	p->uattr = (struct mapping *)NULL;
	if (p->more != (struct more_buf *)NULL)
		tfree_more(p);
	if (p->mailbox != (struct board *)NULL)
		free_board(p->mailbox);
	p->mailbox = (struct board *)NULL;
}

void
tfree_user(struct user *p)
{
	free_user(p);
	free_tcpsock(&p->socket);
	free_vector(p->history);
	clean_stack(&p->stack);
	clean_stack(&p->alias_stack);
#ifdef DEBUG
	if (!STACK_EMPTY(&p->atexit))
		fatal("Atexit stack is not clean in tfree_user().");
#endif
	xfree(p);
}

static void
forward_mail_message(char *addr, char *to, char *from, char *subj, char *msg,
    char *cc, int rec)
{
	char *c;

	c = (char *)xalloc(strlen(msg) + BUFFER_SIZE,
	    "forward_mail_message tmp");

	sprintf(c, "Automatically forwarded mail, original to: %s@%s\n", to,
	    LOCAL_NAME);
	if (cc != (char *)NULL && strlen(cc))
	{
		strcat(c, "Copies to: ");
		strcat(c, cc);
		strcat(c, "\n");
	}
	strcat(c, "\n\n");
	strcat(c, msg);

	/* Need to identify what type of forward this is.. */
	switch (*addr)
	{
	    case '!':	/* Real email */
		send_email(from, addr + 1, (char *)NULL, subj, 1, "%s", c);
		break;

#ifdef INETD_SUPPORT
	    case '@':	/* Intermud email */
		inetd_mail(from, addr + 1, subj, c, ++rec);
		break;
#endif

	    default:	/* Internal mail */
		deliver_mail(addr, from, subj, c, cc, ++rec, 0);
		break;
	}
	xfree(c);
}

#define DELIVER_MAIL_RECLIM 4
int
deliver_mail(char *to, char *from, char *subject, char *data, char *cc,
    int rec, int force)
{
	struct board *b;
	struct message *m;
	struct user *who;
	int live = 0;

	if (!exist_user(to))
		return 0;

	if (++rec >= DELIVER_MAIL_RECLIM)
	{
		log_file("error", "Deliver_mail: %s (from %s): "
		    "Recursion too deep!\n", to, from);
		return 0;
	}

	/* If they are on, be careful to update their LOADED copy
	 * of the mailbox, if loaded; and notify of new mail. */
	who = find_user_absolute((struct user *)NULL, to);
	if (who != (struct user *)NULL && who->mailbox !=
	    (struct board *)NULL)
	{
		b = who->mailbox;
		live++;
	}
	else
		b = restore_mailbox(to);

	if (!force && b->archive != (char *)NULL)
		forward_mail_message(b->archive, to, from, subject, data,
		    cc, rec);

	if (b->limit != -1 && b->num >= b->limit)
	{
		/* Cannot deliver to a full mailbox. */
		char fname[MAXPATHLEN + 1];
		char s[BUFFER_SIZE];

		sprintf(fname, "mail/%s#sticky", to);
		sprintf(s, "[ %s : <Mail system> ]\n"
		    "Mail to you from %s failed (your mailbox is full).\n",
		    nctime(&current_time), capitalise(from));
		append_file(fname, s);

		if (who)
		{
			attr_colour(who, "notify");
			write_socket(who,
			    "[ Mail to you from %s failed (mailbox full) ]",
			    capitalise(from));
			reset(who);
			write_socket(who, "\n");
		}
		if (!live)
			free_board(b);
		return 0;
	}

	m = create_message();
	COPY(m->poster, from, "mail sender");
	COPY(m->subject, subject, "mail subject");
	COPY(m->text, data, "mail text");
	m->date = current_time;
	if (cc != (char *)NULL && strlen(cc))
		m->cc = string_copy(cc, "mail cc");
	add_message(b, m);
	b->flags |= B_NEWM;
	store_board(b);
	if (who != (struct user *)NULL)
	{
		attr_colour(who, "notify");
		write_socket(who, "[ You have new mail from %s ]",
		    capitalise(from));
		reset(who);
		write_socket(who, "\n");
	}

	if (!live)
		free_board(b);
	return 1;
}

static char *gender_tab[] = GENDER_TABLE;

char *
gender_list()
{
	static char buf[0x100];
	static int called = 0;

	if (!called)
	{
		int i, flag;

		called = 1;

		buf[0] = '\0';

		for (flag = i = 0; i < G_MAX; i++)
		{
			if (flag)
				strcat(buf, ", ");
			else
				flag = 1;
			strcat(buf, query_gender(i, G_ABSOLUTE));
		}
	}
	return buf;
}

int
gender_number(char *name, int partial)
{
	int i, j, nlen;

	nlen = strlen(name);

	for (i = 0; i < G_MAX; i++)
	{
		j = i * G_INDICES + G_ABSOLUTE;

		if (!strcasecmp(gender_tab[j], name) ||
		    (partial && !strncasecmp(gender_tab[j], name, nlen)))
			return i;
	}
	return -1;
}

char *
query_gender(int gender, int type)
{
	int index;

	if (gender == G_UNSET)
		return "unset";

	index = gender * G_INDICES + type;

	if (index < 0 || index >= G_MAX * G_INDICES)
		return "unknown";
	return gender_tab[index];
}

int
level_number(char *level)
{
	static char *level_names[] = LEVEL_NAMES;
	int i;

	for (i = 0; i <= L_MAX; i++)
		if (!strcasecmp(level, level_names[i]))
			return i;
	return -1;
}

char *
level_name(int lev, int formatted)
{
	static char *level_names[] = LEVEL_NAMES;
	static char **flevel_names;
	static int called = 0;

	if (!called)
	{	/* Only executed on the first call.. */
		int i, j, max;
		char format[12];

		for (max = i = 0; i <= L_MAX; i++)
			if ((j = strlen(level_names[i])) > max)
				max = j;
		flevel_names = (char **)xalloc(sizeof(char *) * (L_MAX + 1),
		    "*flevel_names array");
		sprintf(format, "[ %%-%ds ]", max);
		for (i = 0; i <= L_MAX; i++)
		{
			flevel_names[i] = (char *)xalloc(max + 5,
			    "*flevel_names");
			sprintf(flevel_names[i], format, level_names[i]);
		}
		called = 1;
	}

	if (lev < 0 || lev > L_MAX)
		return "?????";
	return formatted ? flevel_names[lev] : level_names[lev];
}

#define FINGER_BANNER add_strbuf(&text, \
	    "------------------------------------------------" \
	    "----------------------------\n")
char *
finger_text(struct user *p, char *user, int source)
{
	struct user *u;
	int tmp;
	time_t tmptim, on_time;
	struct strbuf text;
	char buf[MAXPATHLEN + 1];
	extern time_t mail_time(char *);

	/* This is to support queries coming from the inetd */
	if (p == (struct user *)NULL)
	{
		users->level = L_VISITOR;
		users->tz_adjust = 0;
		p = users;
	}

	if ((u = doa_start(p, user)) == (struct user *)NULL)
	{
		sprintf(buf, "No such user, %s.\n", capitalise(user));
		return string_copy(buf, "finger_text nouser");
	}

	init_strbuf(&text, 0, "finger_text text");

	FINGER_BANNER;
	sadd_strbuf(&text, "%s %s\n", u->capname, u->title != (char *)NULL ?
	    u->title : "");
	if ((u->flags & U_AFK) && u->stack.sp->type == T_STRING)
		sadd_strbuf(&text, "\t[ %s ]\n", u->stack.sp->u.string);
	FINGER_BANNER;

	tmp = query_real_level(u->rlname);
	if (tmp == -1)
		tmp = u->level <= L_RESIDENT ? u->level : L_VISITOR;

	/* Line 1 */
	sadd_strbuf(&text, "Level:   %-10s ", capfirst(level_name(tmp, 0)));
	if (p->level >= A_SEE_EXINFO)
	{
		char *t;

		sadd_strbuf(&text, "Validated by: %-15s ",
		    u->validator == (char *)NULL ? "" : capfirst(u->validator));
		if (u->level > L_RESIDENT && (t = query_level_setter(u->rlname))
		    != (char *)NULL)
			sadd_strbuf(&text, "Set by:          %s",
			    capfirst(t));
	}
	cadd_strbuf(&text, '\n');

	/* Line 2 */
	sadd_strbuf(&text, "Gender:  %-10s ",
	    query_gender(u->gender, G_ABSOLUTE));
	if (source != FINGER_INETD)
	{
		sadd_strbuf(&text, "Total logins: %-15d ", u->num_logins);
		sadd_strbuf(&text, "Articles posted: %d\n", u->postings);
	}
	else
		cadd_strbuf(&text, '\n');

	FINGER_BANNER;

	if (IN_GAME(u))
	{
		sadd_strbuf(&text, "Total time on:      %s.\n",
		    conv_time((on_time = u->time_on + current_time -
		    u->login_time)));

		add_strbuf(&text, "Average login time: ");
		if (!u->num_logins)
			add_strbuf(&text, "Um... infinite.");
		else
			add_strbuf(&text, conv_time(on_time / u->num_logins));
		cadd_strbuf(&text, '\n');

		sadd_strbuf(&text, "\tOn since:   %s",
		    nctime(user_time(p, u->login_time)));
		if (p->level >= A_SEE_IP)
		{
			add_strbuf(&text, "  From: ");
			if (p->level >= A_SEE_UNAME && u->uname != (char *)NULL)
			{
				add_strbuf(&text, u->uname);
				add_strbuf(&text, "@");
			}
			add_strbuf(&text, lookup_ip_name(u));
		}
		cadd_strbuf(&text, '\n');
		sadd_strbuf(&text, "\tOn for:     %s\n",
		    conv_time(current_time - u->login_time));

		sadd_strbuf(&text, "\tIdle:       %s.\n",
		    conv_time(current_time - u->last_command));
	}
	else
	{
		sadd_strbuf(&text, "Total time on:      %s.\n",
		    conv_time((on_time = u->time_on)));

		add_strbuf(&text, "Average login time: ");
		if (!u->num_logins)
			add_strbuf(&text, "Um... infinite.");
		else
			add_strbuf(&text, conv_time(on_time / u->num_logins));
		cadd_strbuf(&text, '\n');

		if (u->last_login)
		{
			sadd_strbuf(&text, "Last login:         %s",
			    nctime(user_time(p, u->last_login)));
			if (p->level >= A_SEE_IP && u->last_ip != (char *)NULL)
				sadd_strbuf(&text, "  From: %s\n",
				    u->last_ip);
			else
				cadd_strbuf(&text, '\n');
			sadd_strbuf(&text, "        Elapsed:    %s.\n",
			    conv_time(current_time - u->last_login));
		}
		else
			add_strbuf(&text, "Never logged in.\n");

		sprintf(buf, "users/%c/%s.o", u->rlname[0], u->rlname);
		if (p->level >= A_SEE_LASTMOD &&
		    (tmptim = file_time(buf)) != -1)
		{
			sadd_strbuf(&text, "Last mod:           %s",
			    ctime(user_time(p, tmptim)));
			sadd_strbuf(&text, "        Elapsed:    %s.\n",
			    conv_time(current_time - tmptim));
		}
	}

	FINGER_BANNER;

	if (p->level >= A_SEE_EXINFO)
	{
		sadd_strbuf(&text, "Startloc: %-15s ", u->startloc !=
		    (char *)NULL ? u->startloc : ENTRANCE_PREFIX);
		sadd_strbuf(&text, "Termtype: %d       ", u->terminal);
		sadd_strbuf(&text, "Flags: %s\n", flags(u));
		if (u->comment != (char *)NULL)
			sadd_strbuf(&text, "Comment:  %s\n", u->comment);
		FINGER_BANNER;
	}

	if (u->email != (char *)NULL &&
	    (!(u->saveflags & U_EMAIL_PRIVATE) || p->level >= A_SEE_HEMAIL ||
	    p == u))
	{
		if (source == FINGER_SERVICE)
			sadd_strbuf(&text,
			    "Email: <a href=\"mailto:%s\">%s</a>\n", u->email,
			    u->email);
		else
			sadd_strbuf(&text, "Email: %s %s\n", u->email,
			    u->saveflags & U_EMAIL_PRIVATE ?  "(hidden)" : "");
	}

	if (u->url != (char *)NULL)
	{
		/* Different responses depending on where the finger request
		 * originated. */
		if (source == FINGER_SERVICE)
			sadd_strbuf(&text, "Url: <a href=\"%s\">%s</a>\n",
			    u->url, u->url);
		else
			sadd_strbuf(&text, "Url:   %s\n", u->url);
	}

	FINGER_BANNER;

	if (u->mailbox != (struct board *)NULL)
		add_strbuf(&text, "Mail currently being read.\n");
	else if ((tmptim = mail_time(u->rlname)) != -1)
	{
		u->mailbox = restore_mailbox(u->rlname);

		if (u->mailbox->archive != (char *)NULL)
		{
			char *t = u->mailbox->archive;

			switch (*t)
			{
			    case '!':
				if ((u->saveflags & U_EMAIL_PRIVATE) &&
				    p->level < A_SEE_HEMAIL && p != u)
				{
					add_strbuf(&text,
					    "Mail forwarded via email.\n");
					break;
				}
				/* FALLTHROUGH */
			    case '@':
				t++;
				/* FALLTHROUGH */
			    default:
				sadd_strbuf(&text, "Mail forwarded to: %s\n",
				    t);
			}
		}

		if (u->mailbox->messages != (struct message *)NULL)
		{
			if (u->mailbox->flags & B_NEWM)
				add_strbuf(&text, "New mail received: ");
			else
				add_strbuf(&text, "Mail last read: ");
			add_strbuf(&text, ctime(user_time(p, tmptim)));
		}
		free_board(u->mailbox);
		u->mailbox = (struct board *)NULL;
	}

	if (u->plan != (char *)NULL)
	{
		/* BEWARE - can't use sadd_strbuf() as the plan may be more
		 * than BUFFER_SIZE characters!!
		 */
		add_strbuf(&text, "Plan:\n");
		add_strbuf(&text, u->plan);
	}

	doa_end(u, 0);
	pop_strbuf(&text);
	return text.str;
}

int
iaccess_check(struct user *p, int lev)
{
	if (lev < p->level || ISROOT(p))
		return 1;
	write_socket(p, "Permission denied.\n");
	return 0;
}

int
access_check(struct user *p, struct user *u)
{
	return p == u || iaccess_check(p, u->level);
}

char *
query_password(char *name)
{
	struct user *p;
	static char pass[20];

	if ((p = doa_start((struct user *)NULL, lower_case(name))) ==
	    (struct user *)NULL)
		return (char *)NULL;

	if (p->passwd == (char *)NULL)
	{
		doa_end(p, 0);
		return (char *)NULL;
	}

	strcpy(pass, p->passwd);
	doa_end(p, 0);
	return pass;
}

void
lastlog(struct user *p)
{
	time_t time_on;
	char buf[7];

	strcpy(buf, vshnctime(&p->login_time));
	time_on = (current_time - p->login_time) / 60;

	log_file("secure/last", "%s-%s (%02d:%02d) %s@%s",
	    buf, vshnctime(&current_time),
	    time_on / 60, time_on % 60, p->capname, lookup_ip_name(p));
}

char *
flags(struct user *p)
{
	int i;
	static char ret[10];

	struct {
	    int flag;
	    char ch;
		} flagtab[] = {
	{ U_EMAIL_PRIVATE,	'P' },
	{ U_EMAIL_VALID,	'V' },
	{ U_SHOUT_CURSE,	'C' },
	{ U_BEEP_CURSE,		'B' },
	{ U_FPC,		'F' },
	{ U_NO_PURGE,		'N' },
	{ U_NOSNOOP,		'S' },
	{ U_NO_IDLEOUT,		'I' },
	{ U_SHUTDOWN_NOKICK,	'K' },
	{ U_NOEMAIL,		'E' },
	{ 0,			'\0' } };

	for (i = 0; flagtab[i].flag; i++)
		if (p->saveflags & flagtab[i].flag)
			ret[i] = flagtab[i].ch;
		else
			ret[i] = ' ';
	ret[i] = '\0';
	return ret;
}

#define DOA_STACK_SIZE 5
static struct doa {
	struct user *user;
	int live;
	} doa_stack[DOA_STACK_SIZE];

void
init_doa_stack()
{
	int i;

	for (i = DOA_STACK_SIZE; i--; )
		doa_stack[i].user = (struct user *)NULL;
}

struct user *
doa_start(struct user *p, char *name)
{
	int i;
	struct user *u;

	name = lower_case(name);

	for (i = DOA_STACK_SIZE; i--; )
		if (doa_stack[i].user == (struct user *)NULL)
			break;
	if (i < 0)
		fatal("doa_start: doa stack overflow.");

	if ((u = find_user_absolute(p, name)) != (struct user *)NULL)
	{
		doa_stack[i].user = u;
		doa_stack[i].live = 1;
		return u;
	}
	if ((u = dead_copy(name)) != (struct user *)NULL)
	{
		doa_stack[i].user = u;
		doa_stack[i].live = 0;
		return u;
	}
	return (struct user *)NULL;
}

void
doa_end(struct user *u, int save)
{
	int i;

	for (i = DOA_STACK_SIZE; i--; )
		if (doa_stack[i].user == u)
			break;
	if (i < 0)
		fatal("doa_end: user %s not on stack.", u->capname);

	if (save)
		save_user(u, doa_stack[i].live, 1);

	if (!doa_stack[i].live)
		tfree_user(u);

	doa_stack[i].user = (struct user *)NULL;
}

time_t *
user_time(struct user *p, time_t t)
{
	static time_t tm;

	/* tz_adjust is already multiplied by 10. */
	tm = t + p->tz_adjust * 360;
	return &tm;
}

