/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	comm.c
 * Function:	Outgoing socket communication functions
 **********************************************************************/
/*
 * Communication functions.
 */

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
reset(struct user *p)
{
	if (p->saveflags & U_ANSI)
		write_socket(p, "%s", termcap[p->terminal].reset);
}

void
bold(struct user *p)
{
	if (p->saveflags & U_ANSI)
		write_socket(p, "%s", termcap[p->terminal].bold);
}

void
yellow(struct user *p)
{
	if (p->saveflags & U_ANSI)
		write_socket(p, "%s", termcap[p->terminal].yellow);
}

void
red(struct user *p)
{
	if (p->saveflags & U_ANSI)
		write_socket(p, "%s", termcap[p->terminal].red);
}

/* socket functions.. */
static char buf[BUFFER_SIZE * 50];

#define TAB_LENGTH	8

static void
write_to_socket(struct user *u, char *fmt, va_list argp)
{
	static char buf2[BUFFER_SIZE * 10];
	int length, len, wlen;
	char *p, *q, *r;

	vsprintf(buf, fmt, argp);

	if (u == (struct user *)NULL)
	{
		printf(buf);
		return;
	}

	if (u->flags & (U_SOCK_CLOSING | U_SOCK_QUITTING))
		return;

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
		if (u->col + wlen > u->screenwidth - 1)
		{
			/* If the word is longer than your screenwidth, no
			 * point trying to wrap it around a space so just
			 * print it and break it with a continuation
			 * character (ie. '-')
			 * This shouldn't happen very often! */
			if (wlen > u->screenwidth - 1)
			{
				while (wlen > u->screenwidth - 1)
				{
					/* -2 to allow for the '-' */
					while (u->col++ < u->screenwidth - 2)
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
			    > u->screenwidth - 1)
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

	length = write(u->fd, buf2 + 1, (len = strlen(buf2) - 1));
	if (length == -1)
	{
		if (errno == EMSGSIZE || errno == EWOULDBLOCK)
		{
			FUN_END;
			return;
		}
		u->flags |= U_SOCK_CLOSING;
		notify_levelabu_wrt_flags(u, L_OVERSEER, EAR_TCP,
		    "[ *TCP* Write error: %s [%s] (%s) %s ]\n",
		    u->capname, capfirst(level_name(u->level, 0)),
		    lookup_ip_name(u), perror_text());
		log_perror("write");
		if (IN_GAME(u))
			log_file("tcp", "Error writing to %s: %s",
			    u->capname, buf2);
	}
	/* Perhaps buffer this stuff at some point.. */
	else if (length != len)
		log_file("tcp", "Wrote %d, should have been %d", length,
		    len);
	bytes_outt += length;
	u->sendq += length;
	if (u->snooped_by != (struct user *)NULL)
		write_socket(u->snooped_by, "%s", buf2);

	FUN_END;
}

static void
write_jlm(struct jlm *j, char *fmt, va_list argp, int raw)
{
	char *p = buf + 1;

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

	va_list argp;
	va_start(argp, fmt);

	/* TUsh was written for the way EW talkers send EOR codes.
	 * users may have this behaviour if they wish..
	 * NB: For some reason best known to the author, TUsh needs these
	 *     codes to be at the end of the prompt packets and not just
	 *     at the start of a following packet.
	 *     (otherwise this would be better implemented in socket.c
	 *      and could work for all prompts)
	 */
	if ((u->flags & U_EOR_OK) && (u->saveflags & U_TUSH))
	{
		sprintf(buff, "%s%c%c", fmt, IAC, EOR);
		write_to_socket(u, buff, argp);
	}
	else
		write_to_socket(u, fmt, argp);
}

void
fwrite_prompt(struct user *u, char *fmt, ...)
{
	static char buff[BUFFER_SIZE];
	long oflags = u->flags;
	va_list argp;

	va_start(argp, fmt);

	u->flags |= U_UNBUFFERED_TEXT;
	if ((u->flags & U_EOR_OK) && (u->saveflags & U_TUSH))
	{
		sprintf(buff, "%s%c%c", fmt, IAC, EOR);
		write_to_socket(u, buff, argp);
	}
	else
		write_to_socket(u, fmt, argp);
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
write_room(struct user *u, char *fmt, ...)
{
	struct object *ob;
	va_list argp;

	va_start(argp, fmt);
	for (ob = u->super->ob->contains; ob != (struct object *)NULL;
	    ob = ob->next_contains)
		if (ob->type == T_USER && IN_GAME(ob->m.user))
			write_to_socket(ob->m.user, fmt, argp);
		else if (ob->type == T_JLM)
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
		if (ob->type == T_USER && ob->m.user != u &&
		    IN_GAME(ob->m.user))
			write_to_socket(ob->m.user, fmt, argp);
		else if (ob->type == T_JLM)
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
		if (ob->type == T_USER && ob->m.user != u && ob->m.user != u2
		    && IN_GAME(ob->m.user))
			write_to_socket(ob->m.user, fmt, argp);
		else if (ob->type == T_JLM)
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
		if (ob->type == T_USER && IN_GAME(ob->m.user))
			write_to_socket(ob->m.user, fmt, argp);
		else if (ob->type == T_JLM)
			write_jlm(ob->m.jlm, fmt, argp, 0);
	va_end(argp);
}

void
tell_object_njlm(struct object *o, char *fmt, ...)
{
	va_list argp;
	struct object *ob;

	va_start(argp, fmt);

	for (ob = o->contains; ob != (struct object *)NULL;
	    ob = ob->next_contains)
		if (ob->type == T_USER && IN_GAME(ob->m.user))
			write_to_socket(ob->m.user, fmt, argp);
	va_end(argp);
}

void
write_level(int lev, char *fmt, ...)
{
	struct user *p;
	va_list argp;

	va_start(argp, fmt);
	for (p = users->next; p != (struct user *)NULL; p = p->next)
		if (p->level >= lev && IN_GAME(p))
			write_to_socket(p, fmt, argp);
	va_end(argp);
}

void
write_levelabu(struct user *u, int lev, char *fmt, ...)
{
	struct user *p;
	va_list argp;

	va_start(argp, fmt);
	for (p = users->next; p != (struct user *)NULL; p = p->next)
		if (p->level >= lev && IN_GAME(p) && p != u)
			write_to_socket(p, fmt, argp);
	va_end(argp);
}

void
notify_level(int lev, char *fmt, ...)
{
	struct user *p;
	va_list argp;

	va_start(argp, fmt);
	for (p = users->next; p != (struct user *)NULL; p = p->next)
		if (!(p->earmuffs & EAR_NOTIFY) && IN_GAME(p) &&
		    p->level >= lev)
		{
			yellow(p);
			write_to_socket(p, fmt, argp);
			reset(p);
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
                if (!(p->earmuffs & flag) && IN_GAME(p) &&
                    p->level >= lev)
                {
                        yellow(p);
                        write_to_socket(p, fmt, argp);
                        reset(p);
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
                if (!(p->earmuffs & flag) && IN_GAME(p) && p != u &&
                    p->level >= lev)
                {
                        yellow(p);
                        write_to_socket(p, fmt, argp);
                        reset(p);
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
		if (!(p->earmuffs & EAR_NOTIFY) && IN_GAME(p) && p != u &&
		    p->level >= lev)
		{
			yellow(p);
			write_to_socket(p, fmt, argp);
			reset(p);
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
		if (ob->type == T_USER && IN_GAME(ob->m.user) &&
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
		if (ob->type == T_USER && IN_GAME(ob->m.user) &&
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
	while(fgets(buf, sizeof(buf), fp) != (char *)NULL)
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

