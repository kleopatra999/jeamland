/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	comm.c
 * Function:	Outgoing socket communication functions
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <arpa/telnet.h>
#ifndef NeXT
#include <unistd.h>
#endif
#include <netinet/in.h>
#include "jeamland.h"

#ifndef SEEK_SET
#define SEEK_SET	0
#endif

extern struct user *users;
extern int bytes_outt;
extern int errno;

struct termcap_entry termcap[] = TERMCAP;

void
show_terminals(struct user *p)
{
	int i;

	write_socket(p, "Known terminals are: \n");
	for (i = 0; termcap[i].name; i++)
		write_socket(p, "	%-20s %s\n", termcap[i].name,
		    termcap[i].desc);
}

/*
 Attribute codes: 
 00=none 01=bold 04=underscore 05=blink 07=reverse 08=concealed
 Text color codes:
 30=black 31=red 32=green 33=yellow 34=blue 35=magenta 36=cyan 37=white
 Background color codes:
 40=black 41=red 42=green 43=yellow 44=blue 45=magenta 46=cyan 47=white
*/

#define COLSTACK_SIZE	10
static int colstack[COLSTACK_SIZE];
static int colind;

static void
push_col(int d)
{
	if (colind >= COLSTACK_SIZE)
		return;
	colstack[colind++] = d;
}

int
parse_colour(struct user *p, char *s, struct strbuf *sb)
{
	int ret = 0;
	int flag = 0;
	int c;
	char d;

	colind = 0;

	/* First see if we have modifiers.. */
	while (!flag)
	{
		switch (*s)
		{
		    case '_':	/* Underline */
			push_col(4);
			break;

		    case '!':	/* Bold */
			push_col(1);
			break;

		    case '~':	/* Blink */
			push_col(5);
			break;

		    case '#':	/* Reverse */
			push_col(7);
			break;

		    case '@':	/* Normal */
			push_col(22);
			break;

		    default:
			flag = 1;
			break;
		}
		if (!flag)
			s++, ret++;
	}

	flag = 1;
	do
	{
		if (*s == '\0')
			break;

		if (isupper(*s))
		{
			d = tolower(*s);
			c = 40;
		}
		else
		{
			d = *s;
			c = 30;
		}

		ret++, s++;

		/* Now get the main colour code. */
		switch (d)
		{
		    case 'b':
			push_col(c + 4);
			break;

		    case 'c':
			push_col(c + 6);
			break;

		    case 'g':
			push_col(c + 2);
			break;

		    case 'l':
			push_col(c);
			break;

		    case 'm':
			push_col(c + 5);
			break;

		    case 'r':
			push_col(c + 1);
			break;

		    case 'w':
			push_col(c + 7);
			break;

		    case 'y':
			push_col(c + 3);
			break;

		    case 'd':
			if (*s == '.')
				ret++;
			if (!(p->saveflags & U_ANSI))
				return ret;
			if (sb != (struct strbuf *)NULL)
				add_strbuf(sb, termcap[p->terminal].bold);
			else
			{
				c = p->col;
				write_socket(p, "%s",
				    termcap[p->terminal].bold);
				p->col = c;
			}
			p->flags |= U_COLOURED;
			return ret;

		    case 'z':
			if (*s == '.')
				ret++;
			if (!(p->saveflags & U_ANSI))
				return ret;
			if (sb != (struct strbuf *)NULL)
				add_strbuf(sb, termcap[p->terminal].reset);
			else
			{
				c = p->col;
				write_socket(p, "%s",
				    termcap[p->terminal].reset);
				p->col = c;
			}
			p->flags &= ~U_COLOURED;
			return ret;

		    case '.':
			ret++;
			/* FALLTHROUGH */
		    default:
			flag = 0;
			ret--;
			break;
		}
	} while (flag);

	if (!(p->saveflags & U_ANSI))
		return ret;

	if (termcap[p->terminal].colstr_s == (char *)NULL)
	{
		/* Terminal is not capable of colour
		 * and this is not a reset (trapped for above)
		 */
		if (sb != (struct strbuf *)NULL)
			add_strbuf(sb, termcap[p->terminal].bold);
		else
		{
			c = p->col;
			write_socket(p, "%s",
			    termcap[p->terminal].bold);
			p->col = c;
		}
	}
	else if (colind)
	{
		char t[100];
		char *s;
		int flag = 0;

		strcpy(t, termcap[p->terminal].colstr_s);

		for (s = t; *s != '\0'; s++)
			;

		for (c = 0; c < colind; c++)
		{
			if (flag)
				*s++ = ';';
			sprintf(s, "%d", colstack[c]);
			while (*s != '\0')
				s++;
			flag = 1;
		}
		strcpy(s, termcap[p->terminal].colstr_e);

		if (sb != (struct strbuf *)NULL)
			add_strbuf(sb, t);
		else
		{
			c = p->col;
			write_socket(p, "%s", t);
			p->col = c;
		}
	}
	p->flags |= U_COLOURED;
	return ret;
}

void
attr_colour(struct user *p, char *str)
{
	struct svalue *sv;

	if (!(p->saveflags & U_ANSI))
		return;

	if ((sv = map_string(p->attr, str)) == (struct svalue *)NULL)
		return;

	/* Not really necessary */
	if (sv->type != T_STRING)
		return;

	parse_colour(p, sv->u.string, (struct strbuf *)NULL);
}

void
uattr_colour(struct user *p, char *str)
{
	struct svalue *sv, *def;
	int i;

	sv = def = (struct svalue *)NULL;

	if (!(p->saveflags & U_ANSI))
		return;

	if (p->uattr == (struct mapping *)NULL)
		return;

	for (i = p->uattr->size; i--; )
	{
		if (p->uattr->indices->items[i].type != T_STRING)
			continue;

		/* grupe.. */
		if (*p->uattr->indices->items[i].u.string == '#' &&
		    member_grupe(p->grupes,
		    p->uattr->indices->items[i].u.string + 1, str, 1, 0))
		{
			sv = &p->uattr->values->items[i];
			break;
		}
		else if (!strcmp(p->uattr->indices->items[i].u.string, str))
		{
			sv = &p->uattr->values->items[i];
			break;
		}
		else if (!strcmp(p->uattr->indices->items[i].u.string,
		    "-default"))
			def = &p->uattr->values->items[i];
	}

	if (sv != (struct svalue *)NULL && sv->type == T_STRING)
		parse_colour(p, sv->u.string, (struct strbuf *)NULL);
	else if (def != (struct svalue *)NULL && def->type == T_STRING)
		parse_colour(p, def->u.string, (struct strbuf *)NULL);
}

void
reset(struct user *p)
{
	if (!(p->flags & U_COLOURED))
		return;
	if (p->saveflags & U_ANSI)
	{
		int i = p->col;
		write_socket(p, "%s", termcap[p->terminal].reset);
		p->col = i;
		p->flags &= ~U_COLOURED;
	}
}

/* socket functions.. */
static char buf[BUFFER_SIZE * 50];

#define TAB_LENGTH	8

static void
write_to_socket(struct user *u, char *fmt, va_list argp)
{
	static char buf2[BUFFER_SIZE * 10];
	int length, wlen;
	char *p, *q, *r;

	vsprintf(buf, fmt, argp);

	if (u == (struct user *)NULL)
	{
		printf(buf);
		return;
	}

	if (u->flags & U_SOCK_CLOSING)
	{
		log_file("tcp", "Write to closing.");
		return;
	}

	FUN_START("write_to_socket");
	FUN_ARG(u->capname);

	if ((u->flags & (U_INED | U_QUIET)) && !(u->flags & U_UNBUFFERED_TEXT))
	{
		FUN_END;

		if (u->flags & U_INED)
		{
			if (u->ed->rtext.offset == -1)
				return;
			if (u->ed->rtext.offset > MAX_RTEXT)
			{
				add_strbuf(&u->ed->rtext,
				    "\n-- !TRUNCATED! --\n");
				u->ed->rtext.offset = -1;
				return;
			}
			add_strbuf(&u->ed->rtext, buf);
		}
		return;
	}

	strcpy(buf2, "%");
	/* Here follows the code to wrap words - it's even commented.. */
	p = buf;
	q = buf2 + 1;
	while (*p != '\0')
	{
		/* Skip ahead to find word length. */
		for (wlen = 0, r = p; *r != '\0' && *r != '\n' && !isspace(*r);
		    r++)
			wlen++;

		/* If word length + current column > screenwidth, then insert
		 * a newline before the word. */
		if (u->col + wlen > (int)u->screenwidth - 1)
		{
			/* If the word is longer than your screenwidth, no
			 * point trying to wrap it around a space so just
			 * print it and break it with a continuation
			 * character (ie. '-')
			 * This shouldn't happen very often! */
			if (wlen > (int)u->screenwidth - 1)
			{
				while (wlen > (int)u->screenwidth - 1)
				{
					/* -2 to allow for the '-' */
					while (u->col++ <
					    (int)u->screenwidth - 2)
					{
						wlen--;
						/* If we have a 'given' hyphen
						 * use it! */
						if (*p == '-')
						{
							p++;
							break;
						}
						*q++ = *p++;
					}
					*q++ = '-';
					*q++ = '\r';
					*q++ = '\n';
					u->col = 0;
				}
			}
			else
			{
				*q++ = '\r';
				*q++ = '\n';
				u->col = 0;
			}
		}
		/* Now copy the word into the buffer */
		while (wlen--)
		{
			*q++ = *p++;
			u->col++;
		}

		if (*p == '\0')
			/* finished! */
			break;
		if (*p == '\n')
		{
			/* Normal newline, add a \r */
			u->col = 0;
			*q++ = '\r';
		}
		else if (*p == '\t')
		{
			/* Handle tabs...
			 * Tabs are one character but look like anything
			 * between 1 and 8 when output
			 * Assumption: Tab stops are every 8 characters
			 *             across the screen */
			if ((u->col += TAB_LENGTH - u->col % TAB_LENGTH)
			    > (int)u->screenwidth - 1)
			{
				*q++ = '\r';
				*q++ = '\n';
				u->col = TAB_LENGTH;
			}
		}
		else
			u->col++;
		*q++ = *p++;
	}
	*q = '\0';

	FUN_LINE;


#ifdef SUPER_SNOOP
	/* Not very interested in errors here.... */
	if (u->snoop_fd != -1)
		write(u->snoop_fd, buf2 + 1, strlen(buf2) - 1);
#endif

	if ((length = write_tcpsock(&u->socket, buf2 + 1, -1)) == -1)
	{
		if (errno == EMSGSIZE || errno == EWOULDBLOCK)
		{
			FUN_END;
			return;
		}
		disconnect_user(u, 1);
		notify_levelabu_wrt_flags(u, L_OVERSEER, EAR_TCP,
		    "[ *TCP* Write error: %s [%s] (%s) %s ]",
		    u->capname, capfirst(level_name(u->level, 0)),
		    lookup_ip_name(u), perror_text());
		log_perror("write_to_socket");
		if (IN_GAME(u))
			log_file("tcp", "Error writing to %s: %s",
			    u->capname, buf2);
	}

	if (u->snooped_by != (struct user *)NULL)
		write_socket(u->snooped_by, "%s", buf2);

	FUN_END;
}

static void
write_jlm(struct jlm *j, char *fmt, va_list argp, int raw)
{
	char *p = buf + 1;

	if (j->flags & JLM_TODEL)
		return;

	vsprintf(p, fmt, argp);

	/* Escape any possible control sequences. */
	if (!raw && (*p == '#' || *p == '\\'))
		*--p = '\\';

	if (write(j->outfd, p, strlen(p)) == -1)
		log_perror("write_jlm");
}

void
write_a_jlm(struct jlm *j, int raw, char *fmt, ...)
{
	va_list argp;

	va_start(argp, fmt);
	write_jlm(j, fmt, argp, raw);
	va_end(argp);
}

void
fwrite_socket(struct user *u, char *fmt, ...)
{
	long oflags = u->flags;

	va_list argp;
	va_start(argp, fmt);
	u->flags |= U_UNBUFFERED_TEXT;
	write_to_socket(u, fmt, argp);
	u->flags = oflags;
	va_end(argp);
}

void
write_socket(struct user *u, char *fmt, ...)
{
	va_list argp;
	va_start(argp, fmt);
	write_to_socket(u, fmt, argp);
	va_end(argp);
}

void
write_prompt(struct user *u, char *fmt, ...)
{
	static char buff[BUFFER_SIZE];
	int len;
	va_list argp;

	va_start(argp, fmt);

	/* Assumption that a prompt will never be more than 1K long! */
	strcpy(buff, fmt);

	/* TUsh (along with some other clients) was written for the way
	 * EW talkers send EOR codes.
	 * NB: For some reason best known to the author, TUsh needs these
	 *     codes to be at the end of the prompt packets and not just
	 *     after them.
	 *     (otherwise this would be better implemented in socket.c
	 *      and could work for all prompts)
	 */
	if ((u->socket.flags & TCPSOCK_EOR_OK) && (u->saveflags & U_IACEOR))
	{
		len = strlen(buff);
		buff[len++] = (char)IAC;
		buff[len++] = (char)EOR;
		buff[len] = '\0';
	}
	if ((u->socket.flags & TCPSOCK_GA_OK) && (u->saveflags & U_IACGA))
	{
		len = strlen(buff);
		buff[len++] = (char)IAC;
		buff[len++] = (char)GA;
		buff[len] = '\0';
	}

	write_to_socket(u, buff, argp);
}

void
fwrite_prompt(struct user *u, char *fmt, ...)
{
	static char buff[BUFFER_SIZE];
	unsigned long oflags = u->flags;
	int len;
	va_list argp;

	u->flags |= U_UNBUFFERED_TEXT;

	va_start(argp, fmt);

	/* Assumption that a prompt will never be more than 1K long! */
	strcpy(buff, fmt);

	if ((u->socket.flags & TCPSOCK_EOR_OK) && (u->saveflags & U_IACEOR))
	{
		len = strlen(buff);
		buff[len++] = (char)IAC;
		buff[len++] = (char)EOR;
		buff[len] = '\0';
	}
	if ((u->socket.flags & TCPSOCK_GA_OK) && (u->saveflags & U_IACGA))
	{
		len = strlen(buff);
		buff[len++] = (char)IAC;
		buff[len++] = (char)GA;
		buff[len] = '\0';
	}

	write_to_socket(u, buff, argp);
	u->flags = oflags;
}

void
write_all(char *fmt, ...)
{
	struct user *p;
	va_list argp;

	va_start(argp, fmt);

	for (p = users->next; p != (struct user *)NULL; p = p->next)
		if (IN_GAME(p))
			write_to_socket(p, fmt, argp);
	va_end(argp);
}

void
write_allabu(struct user *u, char *fmt, ...)
{
	struct user *p;
	va_list argp;

	va_start(argp, fmt);
	for (p = users->next; p != (struct user *)NULL; p = p->next)
		if (IN_GAME(p) && p != u)
			write_to_socket(p, fmt, argp);
	va_end(argp);
}

void
write_room_wrt_uattr(struct user *u, char *fmt, ...)
{
	struct object *ob;
	va_list argp;

	va_start(argp, fmt);
	for (ob = u->super->ob->contains; ob != (struct object *)NULL;
	    ob = ob->next_contains)
		if (ob->type == OT_USER && IN_GAME(ob->m.user))
		{
			uattr_colour(ob->m.user, u->rlname);
			write_to_socket(ob->m.user, fmt, argp);
			reset(ob->m.user);
		}
		else if (ob->type == OT_JLM)
			write_jlm(ob->m.jlm, fmt, argp, 0);
	va_end(argp);
}

void
write_room(struct user *u, char *fmt, ...)
{
	struct object *ob;
	va_list argp;

	va_start(argp, fmt);
	for (ob = u->super->ob->contains; ob != (struct object *)NULL;
	    ob = ob->next_contains)
		if (ob->type == OT_USER && IN_GAME(ob->m.user))
			write_to_socket(ob->m.user, fmt, argp);
		else if (ob->type == OT_JLM)
			write_jlm(ob->m.jlm, fmt, argp, 0);
	va_end(argp);
}

void
write_roomabu_wrt_uattr(struct user *u, char *fmt, ...)
{
	struct object *ob;
	va_list argp;

	va_start(argp, fmt);
	for (ob = u->super->ob->contains; ob != (struct object *)NULL;
	    ob = ob->next_contains)
		if (ob->type == OT_USER && ob->m.user != u &&
		    IN_GAME(ob->m.user))
		{
			uattr_colour(ob->m.user, u->rlname);
			write_to_socket(ob->m.user, fmt, argp);
			reset(ob->m.user);
		}
		else if (ob->type == OT_JLM)
			write_jlm(ob->m.jlm, fmt, argp, 0);
	va_end(argp);
}

void
write_roomabu(struct user *u, char *fmt, ...)
{
	struct object *ob;
	va_list argp;

	va_start(argp, fmt);
	for (ob = u->super->ob->contains; ob != (struct object *)NULL;
	    ob = ob->next_contains)
		if (ob->type == OT_USER && ob->m.user != u &&
		    IN_GAME(ob->m.user))
			write_to_socket(ob->m.user, fmt, argp);
		else if (ob->type == OT_JLM)
			write_jlm(ob->m.jlm, fmt, argp, 0);
	va_end(argp);
}

void
write_roomabu2(struct user *u, struct user *u2, char *fmt, ...)
{
	struct object *ob;
	va_list argp;

	va_start(argp, fmt);
	for (ob = u->super->ob->contains; ob != (struct object *)NULL;
	    ob = ob->next_contains)
		if (ob->type == OT_USER && ob->m.user != u && ob->m.user != u2
		    && IN_GAME(ob->m.user))
			write_to_socket(ob->m.user, fmt, argp);
		else if (ob->type == OT_JLM)
			write_jlm(ob->m.jlm, fmt, argp, 0);
	va_end(argp);
}

void
tell_room(struct room *r, char *fmt, ...)
{
	struct object *ob;
	va_list argp;

	va_start(argp, fmt);
	for (ob = r->ob->contains; ob != (struct object *)NULL;
	    ob = ob->next_contains)
		if (ob->type == OT_USER && IN_GAME(ob->m.user))
			write_to_socket(ob->m.user, fmt, argp);
		else if (ob->type == OT_JLM)
			write_jlm(ob->m.jlm, fmt, argp, 0);
	va_end(argp);
}

void
tell_object_njlm(struct object *o, char *fmt, ...)
{
	va_list argp;
	struct object *ob;

	va_start(argp, fmt);

	if (o->type == OT_USER && IN_GAME(o->m.user))
		write_to_socket(o->m.user, fmt, argp);

	for (ob = o->contains; ob != (struct object *)NULL;
	    ob = ob->next_contains)
		if (ob->type == OT_USER && IN_GAME(ob->m.user))
			write_to_socket(ob->m.user, fmt, argp);
	va_end(argp);
}

void
write_level_wrt_attr(int lev, char *attr, char *fmt, ...)
{
	struct user *p;
	struct svalue *sv;
	va_list argp;

	va_start(argp, fmt);
	for (p = users->next; p != (struct user *)NULL; p = p->next)
	{
		/* User list is in level order.. this is a speedup */
		if (p->level < lev)
			break;

		if (!IN_GAME(p))
			continue;

		if ((sv = map_string(p->attr, attr)) !=
		    (struct svalue *)NULL && sv->type == T_STRING)
			parse_colour(p, sv->u.string,
			    (struct strbuf *)NULL);

		write_to_socket(p, fmt, argp);
		/* Reset needs to be before newline.. */
		reset(p);
		write_socket(p, "\n");
	}
}

void
write_level(int lev, char *fmt, ...)
{
	struct user *p;
	va_list argp;

	va_start(argp, fmt);
	for (p = users->next; p != (struct user *)NULL; p = p->next)
	{
		/* User list is in level order.. this is a speedup */
		if (p->level < lev)
			break;
		if (IN_GAME(p))
			write_to_socket(p, fmt, argp);
	}
	va_end(argp);
}

void
write_levelabu(struct user *u, int lev, char *fmt, ...)
{
	struct user *p;
	va_list argp;

	va_start(argp, fmt);
	for (p = users->next; p != (struct user *)NULL; p = p->next)
	{
		/* User list is in level order.. this is a speedup */
		if (p->level < lev)
			break;
		if (!IN_GAME(p) || p == u)
			continue;
		write_to_socket(p, fmt, argp);
	}
	va_end(argp);
}

void
notify_level(int lev, char *fmt, ...)
{
	struct user *p;
	va_list argp;

	va_start(argp, fmt);
	for (p = users->next; p != (struct user *)NULL; p = p->next)
	{
		/* User list is in level order.. this is a speedup */
		if (p->level < lev)
			break;

		if (!(p->earmuffs & EAR_NOTIFY) && IN_GAME(p))
		{
			attr_colour(p, "notify");
			write_to_socket(p, fmt, argp);
			reset(p);
			write_socket(p, "\n");
		}
	}
	va_end(argp);
}

void
notify_level_wrt_flags(int lev, unsigned long flag, char *fmt, ...)
{
        struct user *p;
        va_list argp;

        va_start(argp, fmt);
        for (p = users->next; p != (struct user *)NULL; p = p->next)
	{
		/* User list is in level order.. this is a speedup */
		if (p->level < lev)
			break;
                if (!(p->earmuffs & flag) && IN_GAME(p))
                {
			attr_colour(p, "notify");
                        write_to_socket(p, fmt, argp);
                        reset(p);
			write_socket(p, "\n");
                }
	}
        va_end(argp);
}

void
notify_levelabu_wrt_flags(struct user *u, int lev, unsigned long flag,
    char *fmt, ...)
{
        struct user *p;
        va_list argp;

        va_start(argp, fmt);
        for (p = users->next; p != (struct user *)NULL; p = p->next)
	{
		/* User list is in level order.. this is a speedup */
		if (p->level < lev)
			break;
                if (!(p->earmuffs & flag) && IN_GAME(p) && p != u)
                {
			attr_colour(p, "notify");
                        write_to_socket(p, fmt, argp);
                        reset(p);
			write_socket(p, "\n");
                }
	}
        va_end(argp);
}

void
notify_levelabu(struct user *u, int lev, char *fmt, ...)
{
	struct user *p;
	va_list argp;

	va_start(argp, fmt);
	for (p = users->next; p != (struct user *)NULL; p = p->next)
	{
		/* User list is in level order.. this is a speedup */
		if (p->level < lev)
			break;
	
		if (!(p->earmuffs & EAR_NOTIFY) && IN_GAME(p) && p != u)
		{
			attr_colour(p, "notify");
			write_to_socket(p, fmt, argp);
			reset(p);
			write_socket(p, "\n");
		}
	}
	va_end(argp);
}
	
void
write_room_level(struct user *u, int lev, char *fmt, ...)
{
	struct object *ob;
	va_list argp;

	va_start(argp, fmt);
	for (ob = u->super->ob->contains; ob != (struct object *)NULL;
	    ob = ob->next_contains)
		if (ob->type == OT_USER && IN_GAME(ob->m.user) &&
		    ob->m.user->level >= lev)
			write_to_socket(ob->m.user, fmt, argp);
	va_end(argp);
}

void
write_room_levelabu(struct user *u, int lev, char *fmt, ...)
{
	struct object *ob;
	va_list argp;

	va_start(argp, fmt);
	for (ob = u->super->ob->contains; ob != (struct object *)NULL;
	    ob = ob->next_contains)
		if (ob->type == OT_USER && IN_GAME(ob->m.user) &&
		    ob->m.user->level >= lev && ob->m.user != u)
			write_to_socket(ob->m.user, fmt, argp);
	va_end(argp);
}

int
dump_file(struct user *p, char *dir, char *fname, enum dump_mode op)
{
	FILE *fp;
	struct stat st;
	long offset;
	int line = 0;
	struct strbuf mt;

	sprintf(buf, "%s/%s", dir, fname);

	if ((fp = fopen(buf, "r")) == (FILE *)NULL)
		return 0;

	if (op == DUMP_TAIL)
	{
		if (fstat(fileno(fp), &st) == -1)
			fatal("Could not stat an open file.");
		offset = st.st_size - 54 * 20;
		if (offset < 0)
			offset = 0;
		fseek(fp, offset, SEEK_SET);
		if (offset)
			while (fgetc(fp) != '\n')
				;
	}
	
	init_strbuf(&mt, 0, "dump_file");
	while (fgets(buf, sizeof(buf), fp) != (char *)NULL)
	{
		if (op == DUMP_MORE)
		{
			add_strbuf(&mt, buf);
			continue;
		}
		line++;
		if (op == DUMP_HEAD && line >= 20)
		{
			write_socket(p, "--- TRUNCATED ---\n");
			break;
		}
		write_socket(p, "%s", buf);
	}
	fclose(fp);
	if (op == DUMP_MORE && mt.offset)
	{
		pop_strbuf(&mt);
		more_start(p, mt.str, NULL);
		return 1;
	}
	free_strbuf(&mt);
	return 1;
}

/* Other communication functions.. */
void
print_prompt(struct user *p)
{
	char *str;

	if (p->prompt == (char *)NULL)
		return;

	str = parse_chevron_cookie(p, p->prompt);
	write_prompt(p, "%s", str);
	xfree(str);
}

