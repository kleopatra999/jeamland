/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	jlm.c
 * Function:	The jlm function library.
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include "jlm.h"

#define FREE(xx) if (xx != (char *)NULL) free(xx)

static void
deltrail(char *c)
{
	while (isprint(*c) && *c != '\r' && *c != '\n')
		c++;
	*c = '\0';
}

static void
lower_case(char *c)
{
	for ( ; *c != '\0'; c++)
		*c = tolower(*c);
}

/* I don't trust strdup() on all systems.. */
static char *
string_copy(char *c)
{
	char *p = (char *)malloc(strlen(c) + 1);
	strcpy(p, c);
	return p;
}

static char *
expand_valist(char *fmt, va_list argp)
{
	static char buff[0x400];

	vsprintf(buff, fmt, argp);
	return buff;
}

static struct line *
create_line()
{
	struct line *l = (struct line *)malloc(sizeof(struct line));
	l->cmd = l->user = l->text = (char *)NULL;
	return l;
}

void
free_line(struct line *l)
{
	if (l == (struct line *)NULL)
		return;
	FREE(l->user);
	FREE(l->text);
	FREE(l->cmd);
	free(l);
}

#define CHECK_NONL(xxx, yyy)	if (strpbrk(xxx, "\n\r") != (char *)NULL) \
					return yyy

static void
begin_service(char *service)
{
	printf("#!%s\n", service);
}

static void
service_param(char *value)
{
	printf("#!(%s)\n", value);
}

static void
end_service()
{
	printf("#!\n");
	fflush(stdout);
}

int
register_ident(char *id)
{
	CHECK_NONL(id, 0);

	begin_service("func");
	service_param("ident");
	service_param(id);
	end_service();
	return 1;
}

int
claim_command(char *cmd)
{
	CHECK_NONL(cmd, 0);

	if (strlen(cmd) > 20)
		return 0;

	begin_service("func");
	service_param("claim");
	service_param(cmd);
	end_service();
	return 1;
}

static void
write_cmd(char *func, char *user, char *fmt, va_list argp)
{
	char *p;

	CHECK_NONL(user,);

	p = expand_valist(fmt, argp);

	begin_service("func");
	service_param(func);
	service_param(user);
	printf("%s", p);
	printf("\n");
	end_service();
}

void
write_user(char *user, char *fmt, ...)
{
	va_list argp;
	va_start(argp, fmt);
	write_cmd("write_user", user, fmt, argp);
	va_end(argp);
}

void
write_user_nonl(char *user, char *fmt, ...)
{
	va_list argp;
	va_start(argp, fmt);
	write_cmd("write_user_nonl", user, fmt, argp);
	va_end(argp);
}

void
write_level(char *level, char *fmt, ...)
{
	va_list argp;
	va_start(argp, fmt);
	write_cmd("write_level", level, fmt, argp);
	va_end(argp);
}

void
notify_level(char *level, char *fmt, ...)
{
	va_list argp;
	va_start(argp, fmt);
	write_cmd("notify_level", level, fmt, argp);
	va_end(argp);
}

static void
service_func2(char *name, char *arg1, char *arg2)
{
	CHECK_NONL(arg1,);
	CHECK_NONL(arg2,);

	begin_service("func");
	service_param(name);
	service_param(arg1);
	service_param(arg2);
	end_service();
}

void
force(char *user, char *cmd)
{
	service_func2("force", user, cmd);
}

void
move(char *user, char *room)
{
	service_func2("move", user, room);
}

void
join(char *user, char *user2)
{
	service_func2("join", user, user2);
}

void
chattr(char *user, char *attr)
{
	service_func2("chattr", user, attr);
}

static void
add_textbuf(struct line *l, char *text)
{
	if (l->text == (char *)NULL)
	{
		l->text = (char *)malloc(strlen(text) + 1);
		strcpy(l->text, text);
	}
	else
	{
		l->text = (char *)realloc(l->text, strlen(l->text) +
		    strlen(text) + 1);
		strcat(l->text, text);
	}
}

/*
 * Readline returns a line struct constructed from an incoming service
 * it is a simple finite state machine.
 * If a NULL pointer is returned, something went wrong.
 */
struct line *
read_line()
{
	/* Static for speed */
	static char tbuf[0x400];
	char *buf;
	struct line *j = (struct line *)NULL;
	char *p;
	/* We start idle, awaiting a service. */
	enum { S_NONE, S_DUMP, S_CMD } service = S_NONE;

	/* Just in case it ever drops into a loop, we won't hog the CPU. */
	sleep(1);

	for(;;)
	{
		/* Has our parent disappeared */
		if (getppid() == 1)
			exit(0);

		/* Use fgets to prevent buffer overflow
		 * No need to buffer separately as fgets will block until
		 * a newline is received */
		if (fgets(tbuf, sizeof(tbuf), stdin) == (char *)NULL)
			exit(0);

		buf = tbuf;

		deltrail(buf);

		if (!strcmp(buf, "#!"))
			/* EOS packet */
			return j;

		if (!strncmp(buf, "#!(", 3))
		{	/* Service param packet. */
			if (service == S_NONE ||
			    (p = strrchr(buf, ')')) == (char *)NULL)
				return (struct line *)NULL;

			/* NB: At this point, we have a malloced line
			 *     struct - at least, I hope we have
			 */
			if (j == (struct line *)NULL)
				return j;

			*p = '\0';
			buf += 3;
			if ((p = strchr(buf, ':')) == (char *)NULL)
				/* Should never happen, but skip this
				 * parameter */
				continue;
			*p++ = '\0';
			switch(service)
			{
			    case T_CMD:
				if (!strcmp(buf, "user"))
				{
					FREE(j->user);
					j->user = string_copy(p);
				}
				else if (!strcmp(buf, "cmd"))
				{
					FREE(j->cmd);
					j->cmd = string_copy(p);
				}
				break;
			    default:
				/* Only 'cmd' needs parameters.. at the moment
				 * but I'm not going to create a stack in here
				 * yet so I leave the param packets as
				 * #!(name:arg).
				 * even though the talker ones are just
				 * #!(arg)
				 */
				break;
			}
			continue;
		}

		if (!strncmp(buf, "#!", 2))
		{	/* SOS packet */
			if (service != S_NONE)
			{	/* Panic ;-) */
				free_line(j);
				return (struct line *)NULL;
			}
			buf += 2;
			if (!strcmp(buf, "die"))
				exit(0);
			if (!strcmp(buf, "dump"))
			{
				service = S_DUMP;
				j = create_line();
			}
			else if (!strcmp(buf, "cmd"))
			{
				service = S_CMD;
				j = create_line();
				j->action = T_CMD;
			}
			continue;
		}

		/* Was an escaped leading \ or # character transmitted? */
		if (strlen(buf) > 2 && buf[0] == '\\' &&
		    (buf[1] == '#' || buf[1] == '\\'))
			buf++;

		/* Generic text parameter */
		switch(service)
		{
		    case S_CMD:
			add_textbuf(j, buf);
			break;

		    case S_NONE:
			/* Need to allocate a line struct!!!! */
			j = create_line();
		    case S_DUMP:
			/* Try to parse a 'say' or an 'emote' */
			/* One word, useless */
			if ((p = strchr(buf, ' ')) == (char *)NULL)
			{
				free_line(j);
				return (struct line *)NULL;
			}
			*p++ = '\0';
			if (*p == '\0')
			{
				free_line(j);
				return (struct line *)NULL;
			}

			j->user = string_copy(buf);
			lower_case(j->user);

			if (!strncmp(p, "says '", 6))
			{
				char *q;

				p += 6;
				j->action = T_SAY;

				if ((q = strrchr(p, '\'')) != (char *)NULL)
					*q = '\0';
			}
			else
				j->action = T_EMOTE;

			j->text = string_copy(p);
			return j;
		}
	}
}

