/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	more.c
 * Function:	The pager.
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include "jeamland.h"

void
tfree_more(struct user *p)
{
	xfree(p->more->text);
	xfree(p->more);
	p->more = (struct more_buf *)NULL;
}

int
more_line(struct user *p)
{
	char c, *q;
	static char buf[3 * BUFFER_SIZE];

	q = buf;
	while ((c = *p->more->ptr++) != '\0' && c != '\n' &&
	    (unsigned int)(q - buf) < sizeof(buf))
		*q++ = c;
	*q = '\0';
	write_socket(p, "%s\n", buf);
	if ((unsigned int)(q - buf) >= sizeof(buf))
		write_socket(p, "\n<!! PAGER BUFFER OVERFLOW !!>\n");
	if (c == '\0' || ++p->more->line >= p->more->lines)
		return 0;
	return (unsigned int)(q - buf) / p->screenwidth + 1;
}

void
more_text(struct user *p, char *c)
{
	int lines = 0, len;
	void (*exit_func)(struct user *);

	if (!strlen(c))
		c = "n";
	switch (*c)
	{
	    case ' ':
	    case 'n':
		while ((unsigned int)lines++ < p->morelen)
		{
			if (!(len = more_line(p)))
			{
				exit_func = p->more->more_exit;
				tfree_more(p);
				p->input_to = NULL;
				if (exit_func != NULL)
					(exit_func)(p);
				return;
			}
			lines += len - 1;
		}
		break;
	    case 'q':
		exit_func = p->more->more_exit;
		tfree_more(p);
		p->input_to = NULL;
		if (exit_func != NULL)
			exit_func(p);
		return;
	}
	write_prompt(p, "--- [ %d / %d ] n(ext), q(uit) : ", p->more->line,
	    p->more->lines);
}

static void
do_head(struct user *p, char *text)
{
	char *c;
	int lines;

	for (lines = 0, c = text; *c != '\0'; c++)
		if (*c == '\n' && (unsigned int)++lines > p->morelen)
			break;
	*c = '\0';
	fwrite_socket(p, text);
}

int
more_start(struct user *p, char *text, void (*exit_func)(struct user *))
{
	char *c;

	if (p->input_to != NULL_INPUT_TO)
	{
		/* be nice, show the top page */
		do_head(p, text);
		xfree(text);
		if (exit_func != (void (*)(struct user *))NULL)
			exit_func(p);
		return 0;
	}
		
#ifdef DEBUG
	if (p->more != (struct more_buf *)NULL || text == (char *)NULL)
		fatal("more_start called with current more buffer or with "
		    "null text");
#endif
	if (!strlen(text))
	{
		xfree(text);
		if (exit_func != (void (*)(struct user *))NULL)
			exit_func(p);
		return 0;
	}
	p->more = (struct more_buf *)xalloc(sizeof(struct more_buf),
	    "more struct");
	p->more->ptr = p->more->text = text;
	p->more->more_exit = exit_func;
	p->more->lines = p->more->line = 0;
	for (c = strchr(text, '\n'); c != (char *)NULL;
	    c = strchr(c + 1, '\n'))
		p->more->lines++;

	p->input_to = more_text;
	p->col = 0;
	more_text(p, "");
	return 1;
}

