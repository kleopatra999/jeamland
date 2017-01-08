/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	board.c
 * Function:	Bulletin board & mail systems
 **********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#ifndef NeXT
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include "jeamland.h"

extern time_t current_time;

struct board *
create_board()
{
	struct board *b = (struct board *)xalloc(sizeof(struct board),
	    "board struct");
	b->fname = b->archive = b->followup = (char *)NULL;
	b->flags = b->lastread = 0;
	b->read_grupe = b->write_grupe = (char *)NULL;
	b->limit = BOARD_LIMIT;
	b->messages = b->last_msg = (struct message *)NULL;
	b->num = 0;
	return b;
}

struct message *
create_message()
{
	struct message *m = (struct message *)xalloc(sizeof(struct message),
	    "message struct");
	m->text = m->subject = m->poster = m->cc = (char *)NULL;
	m->date = (time_t)0;
	m->flags = 0;
	m->next = (struct message *)NULL;
	return m;
}

void
free_message(struct message *m)
{
	FREE(m->poster);
	FREE(m->subject);
	FREE(m->text);
	FREE(m->cc);
	xfree(m);
}

void
free_board(struct board *b)
{
	struct message *m, *n;

	if (b == (struct board *)NULL)
		return;
	for (m = b->messages; m != (struct message *)NULL; m = n)
	{
		n = m->next;
		free_message(m);
	}
	FREE(b->fname);
	FREE(b->archive);
	FREE(b->followup);
	FREE(b->read_grupe);
	FREE(b->write_grupe);
	xfree(b);
}

struct message *
find_message(struct board *b, int num)
{
	struct message *m;

	for (num--, m = b->messages;
	    m != (struct message *)NULL && num;
	    m = m->next)
		num--;
	if (num)
		return (struct message *)NULL;
	return m;
}

/* *sigh*, you've got to check for everything these days... */
static int
archive_recurse_check(char *next, char *start)
{
	struct room *r;

	if (next == (char *)NULL || !ROOM_POINTER(r, next) ||
	    !(r->flags & R_BOARD))
		return 0;

	/* This can happen while a room is being loaded */
	if (r->board == (struct board *)NULL)
		return 1;

	if (!strcmp(next, start))
		return 1;

	return archive_recurse_check(r->board->archive, start);
}

void
add_message(struct board *b, struct message *m)
{
	/* The last message is now cached. */
	if (b->last_msg == (struct message *)NULL)
		b->messages = m;
	else
		b->last_msg->next = m;
	b->last_msg = m;

	b->num++;

	if (b->flags & B_ANON)
	{
		/* Simple, but effective! */
		COPY(m->poster, "<Anonymous>", "message poster");
	}

	if (((b->flags & B_CRYPTED) && !(m->flags & M_CRYPTED)) ||
	    (!(b->flags & B_CRYPTED) && (m->flags & M_CRYPTED)))
	{
		xcrypt(m->text);
		m->flags ^= M_CRYPTED;
	}

	while (b->limit != -1 && b->num > b->limit)
	{
		/* Archive them if required. */
		if (b->archive != (char *)NULL)
		{
			struct room *r;

			if (!ROOM_POINTER(r, b->archive) ||
			    !(r->flags & R_BOARD))
			{
				log_file("error",
				    "Missing archive board %s (for %s)",
				    b->archive, b->fname);
			}
			else if (archive_recurse_check(r->board->archive,
			    r->fname))
			{
				log_file("error",
				    "Recursive archive board %s for (%s)",
				    b->archive, b->fname);
				break;
			}
			else
			{
				struct message *mt;

				mt = b->messages;

				if (b->last_msg == mt)
				{
#ifdef DEBUG
					if (mt->next != (struct message *)NULL)
						fatal("mt->next not null in "
						    "add_message");
#endif
					b->last_msg = mt->next;
				}


				b->messages = b->messages->next;
				mt->next = (struct message *)NULL;
				add_message(r->board, mt);
				store_board(r->board);
				b->num--;
				/* b is stored by the caller... */
			}
		}
		else
			remove_message(b, b->messages);
	}
}

void
remove_message(struct board *b, struct message *m)
{
	struct message **n;

	for (n = &b->messages; *n != (struct message *)NULL; n = &((*n)->next))
	{
		if (*n == m)
		{
			*n = m->next;
			b->num--;

			if (m == b->last_msg)
			{	/* Need to recalculate.. */
				struct message *t;

				if (b->num < 2)
					b->last_msg = b->messages;
				else
				{
					for (t = b->messages;
					    t->next != (struct message *)NULL;
					    t = t->next)
						;
					b->last_msg = t;
				}
			}
			free_message(m);
			return;
		}
	}
}

static void
set_subj_tim(struct user *p, struct message *m, char **subjp, char **timp)
{
	static char subj[35];
	static char tim[35];

	if (timp != (char **)NULL)
	{
		/* If over a year old, show the year instead of the time
		 * Ok, so not _exactly_ a year..
		 * Fri Jan 26 02:37:18 1996 */
		strcpy(tim, nctime(user_time(p, m->date)));
		if (current_time - m->date < 365 * 24 * 3600)
			tim[16] = '\0';
		else
		{
			strcpy(tim + 11, tim + 20);
			strcat(tim, " ");
		}
		*timp = tim;
	}
	if (subjp != (char **)NULL)
	{
		if (strlen(m->subject) < 35)
			strcpy(subj, m->subject);
		else
			my_strncpy(subj, m->subject, 34);
		*subjp = subj;
	}
}

/* Used by mbs.. */
void
add_header(struct user *p, struct strbuf *buf, struct message *m, int i)
{
	char *subj, *tim;

	set_subj_tim(p, m, &subj, &tim);

	sadd_strbuf(buf, "%3d: %-35s (%-11s %s)\n", i, subj,
	    capitalise(m->poster), tim);
}

char *
get_headers(struct user *p, struct board *b, int mailer, int new_only)
{
	int i;
	struct message *m;
	char *subj, *tim;
	struct strbuf msg;

	if (b->messages == (struct message *)NULL)
		return (char *)NULL;

	init_strbuf(&msg, 0, "get_headers msg");

	if (b->flags & B_ANON)
		add_strbuf(&msg, "-- This is an anonymous board. --\n");
	if (b->followup != (char *)NULL)
		sadd_strbuf(&msg, "-- Followups set to: %s --\n", b->followup);
	for (i = 0, m = b->messages; m != (struct message *)NULL; m = m->next)
	{
		set_subj_tim(p, m, &subj, &tim);

		if (mailer)
		{
			i++;

			if (new_only && (m->flags & M_MSG_READ))
				continue;

			sadd_strbuf(&msg, "%c%3d: %-35s (%-11s %s)\n",
			    (m->flags & M_DELETED) ? 'D' :
			    (b->lastread == i ? '>' :
			    ((m->flags &
			    M_MSG_READ) ? ' ' : '*')), i, subj,
			    capitalise(m->poster), tim);
		}
		else
			sadd_strbuf(&msg, "%3d: %-35s (%-11s %s)\n", ++i, subj,
			    capitalise(m->poster), tim);
	}
	pop_strbuf(&msg);
	return msg.str;
}

void
show_message(struct user *p, struct board *b, struct message *m, int num,
    void (*exit_func)(struct user *))
{
	char *tim;
	struct strbuf msg;

	init_strbuf(&msg, 0, "show_message msg");

	set_subj_tim(p, m, (char **)NULL, &tim);

	if (num)
		sadd_strbuf(&msg, "Note %d is entitled:\n", num);
	else
		add_strbuf(&msg, "Note is entitled:\n");
	sadd_strbuf(&msg, "'%s (%s, %s)'\n", m->subject,
	    capitalise(m->poster), tim);
	if (m->cc != (char *)NULL)
		sadd_strbuf(&msg, "Cc: %s\n", m->cc);
	cadd_strbuf(&msg, '\n');
		
	if (m->flags & M_CRYPTED)
	{
		add_strbuf(&msg, (tim = xcrypted(m->text)));
		xfree(tim);
	}
	else
		add_strbuf(&msg, m->text);
	pop_strbuf(&msg);
	more_start(p, msg.str, exit_func);
}

struct board *
restore_board(char *dir, char *name)
{
	FILE *fp;
	char fname[MAXPATHLEN + 1];
	struct stat st;
	char *buf, *ptr = (char *)NULL;
	struct board *b;

	sprintf(fname, "%s/%s", dir, name);
	b = create_board();
	COPY(b->fname, fname, "board fname");
	if ((fp = fopen(fname, "r")) == (FILE *)NULL)
		return b;
	if (fstat(fileno(fp), &st) == -1)
	{
		log_perror("fstat");
		fatal("Can't stat open file.");
	}
	if (!st.st_size)
	{
		fclose(fp);
		unlink(fname);
		return b;
	}

	/* Definitely big enough ;-) */
	buf = (char *)xalloc((size_t)st.st_size, "restore board");
	while (fgets(buf, (int)st.st_size - 1, fp) != (char *)NULL)
	{
		if (ISCOMMENT(buf))
			continue;
		if (!strncmp(buf, "message ", 8))
		{
			struct svalue *sv;

			if ((sv = decode_one(buf, "decode_one message")) ==
			    (struct svalue *)NULL)
				continue;
			if (sv->type == T_VECTOR && sv->u.vec->size >= 5 &&
			    sv->u.vec->items[0].type == T_STRING &&
			    sv->u.vec->items[1].type == T_STRING &&
			    sv->u.vec->items[2].type == T_NUMBER &&
			    sv->u.vec->items[3].type == T_NUMBER &&
			    sv->u.vec->items[4].type == T_STRING)
			{
				/* poster, subject, time, flags, text */
				struct message *m = create_message();
				m->poster = string_copy(
				    sv->u.vec->items[0].u.string,
				    "message poster");
				m->subject = string_copy(
				    sv->u.vec->items[1].u.string,
				    "message subject");
				m->date = (time_t)sv->u.vec->items[2].u.number;
				m->flags = sv->u.vec->items[3].u.number;
				m->text = string_copy(
				    sv->u.vec->items[4].u.string,
				    "message text");
				/* Optional cc field.. */
				if (sv->u.vec->size == 6 &&
				    sv->u.vec->items[5].type == T_STRING)
					m->cc = string_copy(
					    sv->u.vec->items[5].u.string,
					    "message cc");
			
				add_message(b, m);
			}
			else
				log_file("error", "Error in message line: "
				    "file %s, line '%s'", name, buf);
			free_svalue(sv);
			continue;
		}
		if (!strncmp(buf, "archive \"", 9))
		{
			DECODE(b->archive, "board archive");
			continue;
		}
		if (!strncmp(buf, "followup \"", 10))
		{
			DECODE(b->followup, "board followup");
			continue;
		}
		if (!strncmp(buf, "write_grupe \"", 13))
		{
			DECODE(b->write_grupe, "write grupe");
			continue;
		}
		if (!strncmp(buf, "read_grupe \"", 12))
		{
			DECODE(b->read_grupe, "read grupe");
			continue;
		}
		if (sscanf(buf, "flags %d", &b->flags))
			continue;
		if (sscanf(buf, "limit %d", &b->limit))
			continue;
	}
	fclose(fp);
	xfree(buf);
	FREE(ptr);
	return b;
}

int
store_board(struct board *b)
{
	FILE *fp;
	struct message *m;
	char *ptr = (char *)NULL;

	if (b == (struct board *)NULL)
		return 0;

	if (b->fname == (char *)NULL ||
	    (fp = fopen(b->fname, "w")) == (FILE *)NULL)
		return 0;

	fprintf(fp, "flags %d\n", b->flags);
	if (b->read_grupe != (char *)NULL)
	{
		ENCODE(b->read_grupe);
		fprintf(fp, "read_grupe \"%s\"\n", ptr);
	}
	if (b->write_grupe != (char *)NULL)
	{
		ENCODE(b->write_grupe);
		fprintf(fp, "write_grupe \"%s\"\n", ptr);
	}
	if (b->archive != (char *)NULL)
	{
		ENCODE(b->archive);
		fprintf(fp, "archive \"%s\"\n", ptr);
	}
	if (b->followup != (char *)NULL)
	{
		ENCODE(b->followup);
		fprintf(fp, "followup \"%s\"\n", ptr);
	}
	fprintf(fp, "limit %d\n", b->limit);
	fprintf(fp, "# poster, subject, time, flags, text, [cc]\n");
	for (m = b->messages; m != (struct message *)NULL; m = m->next)
	{
		char *subject   = code_string(m->subject);
		char *poster    = code_string(m->poster);
		char *text      = code_string(m->text);
		fprintf(fp, "message ({\"%s\",\"%s\",%ld,%d,\"%s\",",
		    poster, subject, (long)m->date, m->flags, text);
		xfree(subject);
		xfree(poster);
		xfree(text);
		if (m->cc != (char *)NULL)
		{
			char *cc = code_string(m->cc);
			fprintf(fp, "\"%s\",", cc);
			xfree(cc);
		}
		fprintf(fp, "})\n");
	}
	fclose(fp);
	FREE(ptr);
	return 1;
}

static void mail_next(struct user *, char *);

void
mail_more_ret(struct user *p)
{
	write_socket(p, MAIL_PROMPT);
	p->input_to = mail_next;
}

char *
quote_message(char *msg, char q)
{
	char *ptr;
	struct strbuf buf;

	init_strbuf(&buf, 0, "reply_message buf");
	cadd_strbuf(&buf, q);
	cadd_strbuf(&buf, ' ');
	for (ptr = msg; *ptr != '\0'; ptr++)
	{
		/* Strip off any .sig */
		if (!strncmp(ptr, "\n-- \n", 5))
		{
			cadd_strbuf(&buf, '\n');
			break;
		}

		cadd_strbuf(&buf, *ptr);
		if (*ptr == '\n')
		{
			cadd_strbuf(&buf, q);
			cadd_strbuf(&buf, ' ');
		}
	}
	pop_strbuf(&buf);
	return buf.str;
}

/* Prepend a 'Re: ' if there isn't already one there
 * If there is one, change it to a 'Re[num]: '
 */
char *
prepend_re(char *s)
{
	char *ptr;
	int num = 0;

	ptr = (char *)xalloc(strlen(s) + 15, "prepend_re");

	if (!strncmp(s, "Re: ", 4))
		sprintf(ptr, "Re[2]: %s", s +  4);
	else if (sscanf(s, "Re[%d]: *[^\n]", &num) == 1)
	{
		char *q;

		if (!num || (q = strchr(s, ':')) == (char *)NULL || num >= 999)
			/* Something funny here. */
			sprintf(ptr, "Re: %s", s);
		else
			sprintf(ptr, "Re[%d]: %s", num + 1, q + 2);
	}
	else
		sprintf(ptr, "Re: %s", s);

	return ptr;
}

void
reply_message(struct user *p, struct message *m, int all)
{
	extern void mail2(struct user *, int);
	char *ptr;

	ptr = prepend_re(m->subject);

	if (all && m->cc != (char *)NULL)
	{
		char *cc = (char *)xalloc(strlen(m->cc) + strlen(m->poster) +
		    5, "reply_mail cc buff");
		sprintf(cc, "%s,%s", m->cc, m->poster);
		push_malloced_string(&p->stack, cc);
	}
	else
		push_string(&p->stack, m->poster);
	push_malloced_string(&p->stack, ptr);
	if (m->flags & M_CRYPTED)
	{
		ptr = xcrypted(m->text);
		push_malloced_string(&p->stack,
		    quote_message(ptr, p->quote_char));
		xfree(ptr);
	}
	else
		push_malloced_string(&p->stack,
		    quote_message(m->text, p->quote_char));

	p->input_to = NULL_INPUT_TO;
	ed_start(p, mail2, MAX_MAIL_LINES, ED_STACKED_TEXT |
	    ((p->medopts & U_MAILOPT_SIG) ? ED_APPEND_SIG : 0));
}

static void
forward_message(struct user *p, struct message *m, char *to)
{
	extern void mail2(struct user *, int);
	char *ptr;

	while (isspace(*to))
		to++;
	push_string(&p->stack, to);

	ptr = (char *)xalloc(strlen(m->subject) + 14, "pushed forward subj");
	sprintf(ptr, "%s (Forwarded)", m->subject);
	push_malloced_string(&p->stack, ptr);

	/* Hopefully big enough! */
	ptr = (char *)xalloc(strlen(m->text) + 0x100, "pushed forward text");
	if (m->flags & M_CRYPTED)
	{
		char *c;
		sprintf(ptr, "\n\n(Original from %s.)\n\n%s\n", m->poster,
		    (c = xcrypted(m->text)));
		xfree(c);
	}
	else
		sprintf(ptr, "\n\n(Original from %s.)\n\n%s\n", m->poster,
		    m->text);
	push_malloced_string(&p->stack, ptr);


	p->input_to = NULL_INPUT_TO;
	write_socket(p, "Forwarding mail: Edit if desired.\n");
	ed_start(p, mail2, MAX_MAIL_LINES, ED_STACKED_TEXT);
}

static void
email_mbox(struct user *p, char *c)
{
	struct message *m;
	struct strbuf buf;
	struct vector *v;
	int i, flag;

	v = parse_range(p, c, p->mailbox->num);
	if (!v->size)
		flag = -1;
	else
		for (flag = 0, i = v->size; i--; )
			if ((m = find_message(p->mailbox,
			    v->items[i].u.number)) == (struct message *)NULL)
				write_socket(p, "No such message, %d\n",
				    v->items[i].u.number);
			else
			{
				flag++;
				m->flags |= M_TOMAIL;
			}
	free_vector(v);
	if (!flag)
	{
		write_socket(p, "No messages selected.\n");
		return;
	}

	init_strbuf(&buf, 0, "email mbox buf");

	for (i = 0, m = p->mailbox->messages;
	    m != (struct message *)NULL; m = m->next)
	{
		if (flag != -1 && !(m->flags & M_TOMAIL))
			continue;
		sadd_strbuf(&buf, "Message: %d\n", ++i);
		sadd_strbuf(&buf, "From:    %s\n", m->poster);
		sadd_strbuf(&buf, "Date:    %s", ctime(user_time(p, m->date)));
		sadd_strbuf(&buf, "Subject: %s\n\n", m->subject);
		if (m->flags & M_CRYPTED)
		{
			add_strbuf(&buf, (c = xcrypted(m->text)));
			xfree(c);
		}
		else
			add_strbuf(&buf, m->text);
		add_strbuf(&buf, "\n\n");
		m->flags &= ~M_TOMAIL;
	}

	if (send_email(p->rlname, p->email, (char *)NULL,
	    LOCAL_NAME " mailbox..", 1, "%s", buf.str) == -1)
		write_socket(p, "Email request failed, try again later.\n");
	free_strbuf(&buf);
}

static void
resync_mailbox(struct user *p)
{
	int num;
	struct message *m, *n;

	for (num = 0, m = p->mailbox->messages;
	    m != (struct message *)NULL; m = n)
	{
		n = m->next;
		if (m->flags & M_DELETED)
		{
			remove_message(p->mailbox, m);
			num++;
		}
	}
	if (num)
		write_socket(p, "Deleted %d message%s.\n", num,
		    num == 1 ? "" : "s");
}

static void
mail_next(struct user *p, char *c)
{
	struct message *m;
	char *tmp;
	int num, new_only = 0;

	while (*c == ' ')
		c++;

	if (!strlen(c))
		num = p->mailbox->lastread + 1;
	else
		num = atoi(c);
	if (num)
	{
		if ((m = find_message(p->mailbox, num)) ==
		    (struct message *)NULL)
			write_socket(p, "No such message, %s\n", c);
		else
		{
			p->mailbox->lastread = num;
			p->input_to = NULL;
			show_message(p, p->mailbox, m, num, mail_more_ret);
			m->flags |= M_MSG_READ;
			return;
		}
	}
	else switch (*c)
	{
	    case 'd':	/* Delete */
	    {
		struct vector *v = parse_range(p, c + 1, p->mailbox->num);
		int flagged = 0;

		if (!v->size)
		{
			/* Ok to do this as parse_range uses a vecbuf */
			v->size++;
			v->items[0].type = T_NUMBER;
			v->items[0].u.number = p->mailbox->lastread;
		}
		for (num = v->size; num--; )
		{
			if ((m = find_message(p->mailbox,
			    v->items[num].u.number)) ==
			    (struct message *)NULL)
				write_socket(p, "No such message, %d\n",
				    v->items[num].u.number);
			else if (!(m->flags & M_DELETED))
			{
				flagged++;
				m->flags |= M_DELETED;
			}
		}
		write_socket(p, "%d message%s marked for deletion.\n",
		    flagged, flagged == 1 ? "" : "s");
		free_vector(v);
		break;
	    }

	    case 'f': 	/* Forward */
	    {
		char to[0x100];

		if (sscanf(c + 1, "%d %s", &num, to) != 2)
		{
			num = p->mailbox->lastread;
			if (!strlen(c + 1))
			{
				write_socket(p,
				    "Syntax: f [message] <user>\n");
				break;
			}
			strcpy(to, c + 1);
		}
		if (!num || (m = find_message(p->mailbox, num)) ==
		    (struct message *)NULL)
			write_socket(p, "No such message, %d\n", num);
		else
		{
			forward_message(p, m, to);
			p->mailbox->lastread = 0;
			return;
		}
		break;
	    }

	    case 'F':	/* Set mail forwarding address. */
		while (*++c == ' ')
			;
		if (*c == '\0')
		{
			if (p->mailbox->archive == (char *)NULL)
				write_socket(p,
				    "Your mail forwarding address is not "
				    "set.\n"
				    "Type '?F' for information on this "
				    "option.\n");
			else
				write_socket(p,
				    "Mail currently forwarded to: %s\n",
				    p->mailbox->archive);
			break;
		}

		FREE(p->mailbox->archive);

		if (*c == '-')
		{
			write_socket(p, "Mail forwarding turned off.\n");
			break;
		}

		/* Special case, own email address */
		if (*c == '!')
		{
			if (*(c + 1) == '\0')
			{
				if (!(p->saveflags & U_EMAIL_VALID))
				{
					write_socket(p, "Your email address "
					    "is not validated, "
					    "cannot action.\n");
					break;
				}
				if (p->email == (char *)NULL)
				{
					write_socket(p,
					    "Your email address is not "
					    "set!\n");
					break;
				}
				p->mailbox->archive =
				    (char *)xalloc(strlen(p->email) +
				    2, "mailbox archive");
				sprintf(p->mailbox->archive, "!%s", p->email);
			}
			else if (p->email == (char *)NULL ||
			    strcmp(p->email, c + 1))
			{
				write_socket(p, "You may not currently set the"
				    " forwarding address to other than your "
				    "own,\nvalidated address.\n");
				break;
			}
		}
		else
			p->mailbox->archive = string_copy(c,
			    "mailbox archive");
		write_socket(p, "Mail forwarding set to: %s\n",
		    p->mailbox->archive);
		break;

	    case 'h':	/* New headers */
		new_only++;
		/* Fall through */
	    case 'H':	/* All headers */
		if ((tmp = get_headers(p, p->mailbox, 1, new_only)) ==
		    (char *)NULL)
			write_socket(p, new_only ? "No unread messages.\n" :
			    "No messages.\n");
		else if (!strlen(tmp))
		{
			xfree(tmp);
			write_socket(p, new_only ? "No unread messages.\n" :
			    "No messages.\n");
		}
		else
		{
			p->input_to = NULL_INPUT_TO;
			more_start(p, tmp, mail_more_ret);
			return;
		}
		break;

	    case 'M':	/* Email mailbox.. */
		if (!(p->saveflags & U_EMAIL_VALID))
			write_socket(p,
			    "Your email address is not validated.\n"
			    "To validate this address, send email to %s\n",
			    OVERSEER_EMAIL);
		else
		{
			email_mbox(p, c + 1);
			write_socket(p, "Email being processed.\n");
		}
		break;

	    case 'n':	/* Mark as new */
	    {
		struct vector *v = parse_range(p, c + 1, p->mailbox->num);
		int flagged = 0;

		if (!v->size)
		{
			/* Ok to do this as parse_range uses a vecbuf */
			v->size++;
			v->items[0].type = T_NUMBER;
			v->items[0].u.number = p->mailbox->lastread;
		}
		for (num = v->size; num--; )
		{
			if ((m = find_message(p->mailbox,
			    v->items[num].u.number)) ==
			    (struct message *)NULL)
				write_socket(p, "No such message, %d\n",
				    v->items[num].u.number);
			else if (m->flags & M_MSG_READ)
			{
				flagged++;
				m->flags &= ~M_MSG_READ;
			}
		}
		write_socket(p, "%d message%s marked as new.\n",
		    flagged, flagged == 1 ? "" : "s");
		free_vector(v);
		break;
	    }

	    case 'o':	/* Toggle new-only */
		if ((p->medopts ^= U_MAILOPT_NEW_ONLY) & U_MAILOPT_NEW_ONLY)
			write_socket(p,
			    "Only new messages will be shown at mailer "
			    "startup.\n");
		else
			write_socket(p,
			    "All messages will be shown at mailer "
			    "startup.\n");
		break;

	    case 'r':	/* Reply */
		/* Fall through */
	    case 'R':	/* Reply only to originator (not cc list) */
		if (!(num = atoi(c + 1)))
			num = p->mailbox->lastread;
		if (!num || (m = find_message(p->mailbox, num)) ==
		    (struct message *)NULL)
			write_socket(p, "No such message, %s\n", c + 1);
		else
		{
			reply_message(p, m, *c == 'r');
			return;
		}
		break;

	    case 'u':	/* Undelete */
	    {
		struct vector *v = parse_range(p, c + 1, p->mailbox->num);
		int flagged = 0;

		if (!v->size)
		{
			/* Ok to do this as parse_range uses a vecbuf */
			v->size++;
			v->items[0].type = T_NUMBER;
			v->items[0].u.number = p->mailbox->lastread;
		}
		for (num = v->size; num--; )
		{
			if ((m = find_message(p->mailbox,
			    v->items[num].u.number)) ==
			    (struct message *)NULL)
				write_socket(p, "No such message, %d\n",
				    v->items[num].u.number);
			else if (m->flags & M_DELETED)
			{
				flagged++;
				m->flags &= ~M_DELETED;
			}
		}
		write_socket(p, "%d message%s unmarked.\n",
		    flagged, flagged == 1 ? "" : "s");
		free_vector(v);
		break;
	    }

	    case 'q':	/* Quit */
	    case 'x':	/* or Exit ;) */
	    {
		resync_mailbox(p);

		p->mailbox->flags &= ~B_NEWM;
		store_board(p->mailbox);
		free_board(p->mailbox);
		p->mailbox = (struct board *)NULL;
		p->input_to = NULL_INPUT_TO;
		p->flags &= ~U_IN_MAILER;
		return;
	    }

	    case '?':	/* Help */
		switch (*++c)
		{
		    case 'F':
			dump_file(p, "help", "_mail_forward", DUMP_CAT);
			break;
		    default:
			dump_file(p, "help", "_mail", DUMP_CAT);
			break;
		}
		break;

	    case '$':	/* Resync mailbox */
		write_socket(p, "Resynchronising mailbox.\n");
		resync_mailbox(p);
		break;

	    default:
		write_socket(p, "Unknown command.\n");
	}
	write_socket(p, MAIL_PROMPT);
}

time_t
mail_time(char *user)
{
	char buf[MAXPATHLEN + 1];

	sprintf(buf, "mail/%s", user);
	return file_time(buf);
}

static void
convert_mailbox(struct user *p)
{
	struct message *m;

	/* Temporary code.. convert plaintext mailboxes */
	if (!(p->mailbox->flags & B_CRYPTED))
	{
		write_socket(p, "<< Converting old style mailbox.");
		log_file("syslog", "Converting mailbox for %s.", p->name);
		for (m = p->mailbox->messages; m != (struct message *)NULL;
		    m = m->next)
		{
			if (!(m->flags & M_CRYPTED))
			{
				xcrypt(m->text);
				write_socket(p, ".");
				m->flags |= M_CRYPTED;
			}
		}
		write_socket(p, " >>\n");
		
		p->mailbox->flags |= B_CRYPTED;
		store_board(p->mailbox);
	}
}

char *
mail_from(struct user *p, int header)
{
	char *tmp;

#ifdef DEBUG
	if (p->mailbox == (struct board *)NULL)
		fatal("mail_from on null mailbox.");
#endif

	convert_mailbox(p);

	if ((tmp = get_headers(p, p->mailbox, 1,
	    (p->medopts & U_MAILOPT_NEW_ONLY) ? 1 : 0)) == (char *)NULL)
	{
		write_socket(p, "No mail for %s.\n", p->name);
		return (char *)NULL;
	}

	if (header)
	{
		int unread;
		struct message *m;

		write_socket(p, "Mail [%s JeamLand] Type ? for help.\n",
		    MAIL_VERSION);
		write_socket(p, "Maximum mailbox size: ");
		if (p->mailbox->limit == -1)
			write_socket(p, "unlimited.\t");
		else
			write_socket(p, "%d.\t", p->mailbox->limit);
		for (unread = 0, m = p->mailbox->messages;
		    m != (struct message *)NULL; m = m->next)
			if (!(m->flags & M_MSG_READ))
				unread++;
		write_socket(p, "(%d message%s, %d unread)\n", p->mailbox->num,
		    p->mailbox->num == 1 ? "" : "s", unread);
		if (p->mailbox->num == p->mailbox->limit)
			write_socket(p, "WARNING: Your mailbox is full.\n");
	}

	if (!strlen(tmp))
	{
		xfree(tmp);
		write_socket(p, (p->medopts & U_MAILOPT_NEW_ONLY) ?
		    "No unread messages.\n" : "No messages.\n");
		return (char *)NULL;
	}
	else
		return tmp;
}

struct board *
restore_mailbox(char *user)
{
	struct board *b = restore_board("mail", user);
	b->flags |= B_CRYPTED;
	return b;
}

void
mail_start(struct user *p)
{
	char *tmp;

	if (p->mailbox != (struct board *)NULL)
		fatal("Double mail start.");
	if (p->input_to != NULL_INPUT_TO)
		fatal("Input_to set in mail_start.");

	p->mailbox = restore_mailbox(p->rlname);

	p->flags |= U_IN_MAILER;
	if ((tmp = mail_from(p, 1)) != (char *)NULL)
		more_start(p, tmp, mail_more_ret);
	else
		mail_more_ret(p);
}

