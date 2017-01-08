/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	ed.c
 * Function:	The line editor.
 **********************************************************************/
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include "jeamland.h"

/* Adds a ~D command showing line links, does no harm 
 * but is probably of little interest to anyone else but me ;-) */
#define D_COMMAND
/* Not recommended for normal use, increases CPU load */
#undef DEBUG_EDITOR
#undef REALLY_DEBUG_EDITOR

#define RULER "------|------|------|------|------|" \
	      "------|------|------|------|------|\n"
#define VALID_LINE(p, l) (l > 0 && l < p->ed->curr)
#define AUTOLIST(p) (!(p->ed->flags & ED_NO_AUTOLIST) && \
		    (p->medopts & U_EDOPT_AUTOLIST))

/* Local prototypes */
static int insert_line(struct user *, char *);
static void ed_next(struct user *, char *);
static void dump_lines(struct user *, int, int);
static struct line *find_line(struct user *p, int line);

#if defined(D_COMMAND) || defined(DEBUG_EDITOR)
static void
dump_ed_buffer(struct user *p)
{
	struct line *l;

	fwrite_socket(p, "-- l->last   l         l->next\n");
	fwrite_socket(p, "-- ---------------------------\n");
	for (l = p->ed->start; l != (struct line *)NULL; l = l->next)
		fwrite_socket(p, "-- %9p %9p %9p\n", (void *)l->last,
		    (void *)l, (void *)l->next);

	fwrite_socket(p, "\n-- start = %p\n", (void *)p->ed->start);
	fwrite_socket(p, "-- last  = %p\n", (void *)p->ed->last);
}
#endif /* D_COMMAND */

#ifdef REALLY_DEBUG_EDITOR
/*
 * Checks that all of the lines are correctly double-linked.
 */
static void
check_ed_integrity(struct user *p)
{
	struct line *l;

	if (p->ed->last != (l = find_line(p, p->ed->curr - 1)))
	{
		dump_ed_buffer(p);
		fatal("ed: Current last line pointer (%p) != last line (%p).",
		    (void *)p->ed->last, (void *)l);
	}

	for (l = p->ed->start; l != (struct line *)NULL; l = l->next)
		if (l->next != (struct line *)NULL && l->next->last != l)
		{
			dump_ed_buffer(p);
			fatal("ed: line last pointer wrong in (%p).",
			    (void *)l);
		}
}
#endif /* REALLY_DEBUG_EDITOR */

static void
ed_prompt(struct user *p)
{
#ifdef DEBUG_EDITOR
	check_ed_integrity(p);
#endif
	fwrite_prompt(p, "%c%-3d%s", p->ed->insert ? '*' : ' ', p->ed->curr,
	    ED_PROMPT);
}

static struct line *
create_line()
{
	struct line *l = (struct line *)xalloc(sizeof(struct line),
	    "ed: line struct");
	l->text = (char *)NULL;
	l->next = l->last = (struct line *)NULL;
	return l;
}

static void
free_line(struct line *l)
{
	FREE(l->text);
	xfree(l);
}

static void
unlink_line(struct user *p, struct line *l)
{
	if (l->last == (struct line *)NULL)
		p->ed->start = l->next;
	else
		l->last->next = l->next;

	if (l->next == (struct line *)NULL)
		p->ed->last = l->last;
	else
		l->next->last = l->last;
}

static void
ed_clean(struct user *p, FILE *fp)
{
	struct line *l, *n;

	for (l = p->ed->start; l != (struct line *)NULL; l = n)
	{
		if (fp != (FILE *)NULL)
			fprintf(fp, "%s\n", l->text);
		n = l->next;
		free_line(l);
	}
	for (l = p->ed->tmp; l != (struct line *)NULL; l = n)
	{
		if (fp != (FILE *)NULL)
			fprintf(fp, "%s\n", l->text);
		n = l->next;
		free_line(l);
	}
	p->ed->start = p->ed->last = p->ed->tmp = p->ed->tmp_last =
	    (struct line *)NULL;
	p->ed->curr = 1;
	p->ed->insert = 0;
}

void
tfree_ed(struct user *p, int dying)
{
	FILE *fp = (FILE *)NULL;

	if (dying)
	{
		char buf[MAXPATHLEN + 1];
		sprintf(buf, "dead_ed/%s", p->rlname);

		if ((fp = fopen(buf, "w")) == (FILE *)NULL)
			log_perror("popen dead_ed");
	}

	ed_clean(p, fp);
	if (fp != (FILE *)NULL)
		fclose(fp);
	if (p->ed->rtext.len)
		free_strbuf(&p->ed->rtext);
	xfree(p->ed);
	p->ed = (struct ed_buffer *)NULL;
}

static struct line *
find_line(struct user *p, int line)
{
	struct line *l;

	for (l = p->ed->start, line--; l != (struct line *)NULL && line;
	    line--)
		l = l->next;
	return l;
}

static void
delete_lines(struct user *p, char *c)
{
	struct vector *v;
	struct svalue sv;
	struct line *l, *m;
	int i, done;

	p->input_to = ed_next;

	v = parse_range(p, c, p->ed->curr - 1);
	if (!v->size)
	{
		/* Ok to do this as parse_range uses a vecbuf */
		v->size++;
		v->items[0].type = T_NUMBER;
		v->items[0].u.number = p->ed->curr - 1;
	}

	/* Check the values in the vector */
	for (i = v->size; i--; )
	{
		if (!VALID_LINE(p, v->items[i].u.number))
		{
			fwrite_socket(p, "-- Invalid line number (%d).\n",
			    v->items[i].u.number);
			ed_prompt(p);
			free_vector(v);
			return;
		}
	}

	sv.type = T_NUMBER;

	/* Remove the requested lines. */
	for (done = i = 0, l = p->ed->start; l != (struct line *)NULL; l = m)
	{
		m = l->next;

		sv.u.number = ++i;
		if (member_vector(&sv, v) == -1)
			continue;

		unlink_line(p, l);
		free_line(l);
		done++;
	}

	p->ed->curr -= done;
	fwrite_socket(p, "-- Deleted %d line%s.\n", done,
	    done == 1 ? "" : "s");
	dump_lines(p, 0, 0);
	free_vector(v);
}

static void
dump_lines_ret(struct user *p)
{
	p->flags &= ~U_UNBUFFERED_TEXT;
	p->input_to = ed_next;
	ed_prompt(p);
}

/* Something else requested by Mr Gosnell... */
static char *
ruler_line(struct user *p)
{
	static char ruler[100];

	*ruler = '\0';

	if (!(p->medopts & U_EDOPT_LEFT_RULER))
		strcat(ruler, "      ");

	strcat(ruler, RULER);

	return ruler;
}

static void
dump_lines(struct user *p, int start, int end)
{
	struct strbuf buf;
	struct line *l;
	int i;

	if (!start && !end)
	{
		if ((i = p->ed->start == (struct line *)NULL) ||
		    !AUTOLIST(p))
		{
			if (i && (p->medopts & U_EDOPT_RULER))
				fwrite_socket(p, "%s", ruler_line(p));
			dump_lines_ret(p);
			return;
		}
		start = p->ed->curr - p->morelen - 1;
	}

	init_strbuf(&buf, 0, "editor: dump_lines");

	if (p->medopts & U_EDOPT_RULER)
		add_strbuf(&buf, ruler_line(p));

	for (i = 1, l = p->ed->start; l != (struct line *)NULL;
	    l = l->next, i++)
	{
		if (start && i < start)
			continue;
		if (end && i > end)
			break;
		sadd_strbuf(&buf, " %-3d%s%s\n", i, ED_PROMPT, l->text);
	}
	if (!buf.offset)
	{
		fwrite_socket(p, "-- No lines to list.\n");
		free_strbuf(&buf);
		dump_lines_ret(p);
		return;
	}
	p->input_to = NULL_INPUT_TO;
	p->flags |= U_UNBUFFERED_TEXT;
	write_socket(p, "-- Reviewing.\n");
	pop_strbuf(&buf);
	more_start(p, buf.str, dump_lines_ret);
}

static void
append_text(struct user *p, char *c, char *tok)
{
	char *d;
	int lenc, lend;

	lenc = strlen(c);
	d = strtok(c, tok);
	if (d == (char *)NULL)
		return;
	do
	{
		if (!insert_line(p, d))
			break;

		/* Handle consecutive blank lines */
		lend = strlen(d) + 1;
		while (d + lend < c + lenc &&
		    (strchr(tok, d[lend]) != (char *)NULL))
		{
			if (!insert_line(p, ""))
				break;
			lend++;
		}
	} while((d = strtok((char *)NULL, tok)) != (char *)NULL);
}

static int
insert_line(struct user *p, char *c)
{
	struct line *new;

	if (p->ed->max != -1 && p->ed->curr + p->ed->insert > p->ed->max)
	{
		fwrite_socket(p, "-- Too many lines.\n");
		return 0;
	}

	new = create_line();
	new->text = string_copy(c, "line text");
	p->ed->curr++;

	/* First line.. */
	if (p->ed->start == (struct line *)NULL)
		p->ed->start = p->ed->last = new;
	else
	{
		p->ed->last->next = new;
		new->last = p->ed->last;
		p->ed->last = new;
	}
	return 1;
}

static void
insert_mode(struct user *p, char *c)
{
	int i = atoi(c);
	int j = i;
	struct line *l;

	if (!VALID_LINE(p, i))
	{
		fwrite_socket(p, "-- Invalid line number (%d).\n", i);
		ed_prompt(p);
		return;
	}

	l = find_line(p, i);
	p->ed->tmp_last = p->ed->last;
	p->ed->tmp = l;

	if (l->last == (struct line *)NULL)
		p->ed->start = p->ed->last = (struct line *)NULL;
	else
	{
		p->ed->last = l->last;
		l->last->next = (struct line *)NULL;
	}

	p->ed->insert = p->ed->curr - j;
	p->ed->curr = j;
	ed_prompt(p);
}

static void
join_lines(struct user *p, char *c)
{
	struct line *l;
	int i;

	while (*c == ' ')
		c++;

	if (*c == '\0')
		/* Default: Join the last line to the one before it.. */
		i = p->ed->curr - 2;
	else
		i = atoi(c);

	if (!VALID_LINE(p, i))
	{
		fwrite_socket(p, "-- Invalid line number (%d).\n", i);
		ed_prompt(p);
		return;
	}
	l = find_line(p, i);

	/* Make sure the line has a next one.. */
	if (l->next == (struct line *)NULL)
	{
		fwrite_socket(p, "-- Nothing after line %d to join.\n", i);
		ed_prompt(p);
		return;
	}

	/* Concatenate the two lines.. */
	l->text = (char *)xrealloc(l->text,
	    strlen(l->text) + strlen(l->next->text) + 1);
	strcat(l->text, l->next->text);

	/* Now remove the next line.. */
	l = l->next;
	unlink_line(p, l);
	free_line(l);
	p->ed->curr--;

	fwrite_socket(p, "-- Joined line %d to %d.\n", i, i + 1);

	/* And, if not autodump, show the line we've just joined. */
	if (!AUTOLIST(p))
		dump_lines(p, i, i);
	else
		dump_lines(p, 0, 0);
}

static void
leave_insert(struct user *p)
{
	if (p->ed->start == (struct line *)NULL)
	{
		p->ed->tmp->last = (struct line *)NULL;
		p->ed->start = p->ed->tmp;
	}
	else
	{
		p->ed->tmp->last = p->ed->last;
		p->ed->last->next = p->ed->tmp;
	}

	p->ed->last = p->ed->tmp_last;
	p->ed->tmp = (struct line *)NULL;

	p->ed->curr += p->ed->insert;
	p->ed->insert = 0;
	dump_lines(p, 0, 0);
}

static void
change_line2(struct user *p, char *c)
{
	struct line *l;

	if (c[0] == '~')
	{
		fwrite_socket(p,
		    "-- Tilde escapes not supported in change mode.\n");
		fwrite_prompt(p, "$%-3d%s", p->stack.sp->u.number, ED_PROMPT);
		return;
	}
	if (strlen(c) >= 2 && c[0] == '\\' && c[1] == '~')
		c++;

	l = find_line(p, p->stack.sp->u.number);
	dec_stack(&p->stack);

#ifdef DEBUG_EDITOR
	if (l == (struct line *)NULL)
		fatal("ed::change_line2(): line missing.");
#endif

	COPY(l->text, c, "line text");

	/* dump_lines will reset the input_to */
	dump_lines(p, 0, 0);
}

static void
change_line(struct user *p, char *c)
{
	int i;

	while (*c == ' ')
		c++;

	if (*c == '\0')
		i = p->ed->curr - 1;
	else
		i = atoi(c);

	if (!VALID_LINE(p, i))
	{
		fwrite_socket(p, "-- Invalid line number (%d).\n", i);
		ed_prompt(p);
		return;
	}
	p->input_to = change_line2;
	push_number(&p->stack, i);
	fwrite_prompt(p, "$%-3d%s", i, ED_PROMPT);
}

static void
move_lines(struct user *p, char *c)
{
	int start, end, to, i;
	struct line *startl, *endl, *tol, *l;

	if (sscanf(c, "%d,%d %d", &start, &end, &to) != 3)
	{
		end = 0;
		if (sscanf(c, "%d %d", &start, &to) != 2)
		{
			start = p->ed->curr - 1;
			if (!(to = atoi(c)))
			{
				fwrite_socket(p, "-- Syntax error.\n");
				ed_prompt(p);
				return;
			}
		}
	}
	if (!end)
		end = start;

	i = 0;
	if (!VALID_LINE(p, start) || ((i = 1) && !VALID_LINE(p, to)) ||
	    ((i = 2) && !VALID_LINE(p, end)))
	{
		switch(i)
		{
		    case 0:
			fwrite_socket(p,
			    "-- Invalid 'start' line number, %d\n", start);
			break;
		    case 1:
			fwrite_socket(p,
			    "-- Invalid 'to' line number, %d\n", to);
			break;
		    case 2:
			fwrite_socket(p,
			    "-- Invalid 'end' line number, %d\n", end);
			break;
		    default:
			fatal("invalid 'i' in ed::move_lines().");
		}
		ed_prompt(p);
		return;
	}

	if (end && end < start)
	{
		fwrite_socket(p, "-- End line cannot be before start.\n");
		ed_prompt(p);
		return;
	}

	if (to >= start && to <= end)
	{
		fwrite_socket(p,
		    "-- To line cannot be between start and end.\n");
		ed_prompt(p);
		return;
	}

	/* By the above checks, none of these should be NULL
	 * (if they are, we are in trouble!) */
	startl = find_line(p, start);
	endl = find_line(p, end);
	tol = find_line(p, to);

#ifdef DEBUG_EDITOR
	if (startl == (struct line *)NULL)
		fatal("ed::move_lines() startl is NULL.");
	if (endl == (struct line *)NULL)
		fatal("ed::move_lines() endl is NULL.");
	if (tol == (struct line *)NULL)
		fatal("ed::move_lines() tol is NULL.");
#endif

	/* Remove our block.. */
	if (startl->last == (struct line *)NULL)
		p->ed->start = endl->next;
	else
		startl->last->next = endl->next;

	if (endl->next == (struct line *)NULL)
		p->ed->last = startl->last;
	else
		endl->next->last = startl->last;

	/* Insert it again.. above tol, last pointer is safe for once */
	l = tol->last;
	tol->last = endl;
	endl->next = tol;
	startl->last = l;
	if (l != (struct line *)NULL)
		l->next = startl;
	else
		p->ed->start = startl;

	/* Done! */
	dump_lines(p, 0, 0);
}

static void
ed_options(struct user *p, char *c)
{
	/* Expandable.. */
	static struct {
		char *opt;
		int flag;
	} opts[] = {
	{ "autolist",	U_EDOPT_AUTOLIST },
	{ "ruler",	U_EDOPT_RULER },
	{ "ruler_left",	U_EDOPT_LEFT_RULER},
	{ "info",	U_EDOPT_INFO },
	{ (char *)NULL,	0 },
	};
	int i, no = 0;

	while (isspace(*c))
		c++;

	/* If no arguments, just list current settings. */
	if (*c == '\0')
	{
		fwrite_socket(p, "-- Current settings:\n");
		for (i = 0; opts[i].opt != (char *)NULL; i++)
			if (p->medopts & opts[i].flag)
				fwrite_socket(p, "--\t%s\n", opts[i].opt);
			else
				fwrite_socket(p, "--\tno%s\n", opts[i].opt);
		ed_prompt(p);
		return;
	}

	/* Else, we want to set or unset one.. */
	if (!strncmp(c, "no", 2))
	{
		no = 1;
		c += 2;
	}

	/* Now find the option we're trying to change. */
	for (i = 0; opts[i].opt != (char *)NULL; i++)
		if (!strcmp(c, opts[i].opt))
		{
			if (no)
				p->medopts &= ~opts[i].flag;
			else
				p->medopts |= opts[i].flag;

			fwrite_socket(p, "-- %s option %s.\n",
			    no ? "Unset" : "Set", c);
			break;
		}
	if (opts[i].opt == (char *)NULL)
		fwrite_socket(p, "-- Unknown option, %s.\n", c);

	/* Some option specific stuff.. */
	else switch(opts[i].flag)
	{
	    case U_EDOPT_RULER:
		/* If we're unsetting this one, we need to also unset
		 * LEFT_RULER */
		if (no)
			p->medopts &= ~U_EDOPT_LEFT_RULER;
		break;
	    case U_EDOPT_LEFT_RULER:
		/* If setting this, set RULER */
		if (!no)
			p->medopts |= U_EDOPT_RULER;
		break;
	}

	ed_prompt(p);
}

static void
ed_search_replace(struct user *p, char *c)
{
	int l;
	char *r, *s;
	char *new;
	struct line *lp;
	
	if (!strlen(c))
	{
		fwrite_socket(p, "-- Syntax error, arguments required.\n");
		ed_prompt(p);
		return;
	}

	if (!isdigit(*c))
		l = p->ed->curr - 1;
	else
	{
		if (!VALID_LINE(p, (l = atoi(c))))
		{
			fwrite_socket(p, "-- Invalid line, %d.\n", l);
			ed_prompt(p);
			return;
		}
	}

	/* You guessed it, this shouldn't happen. */
	if ((lp = find_line(p, l)) == (struct line *)NULL)
		fatal("Null find_line in ed search & replace, l = %d.\n", l);

	/* Skip the line number, */
	while (isdigit(*c))
		c++;

	/* Skip the optional '^' character */
	if (*c == '^')
		c++;

	/* Find our replace string. */
	if ((r = strchr(c, '^')) == (char *)NULL)
	{
		fwrite_socket(p,
		    "-- Syntax error, replacement text missing.\n");
		ed_prompt(p);
		return;
	}

	/* Terminate search string, increment replace string pointer. */
	*r++ = '\0';

	/* Find our search string */
	if ((s = strstr(lp->text, c)) == (char *)NULL)
	{
		fwrite_socket(p, "-- Search text not found.\n");
		ed_prompt(p);
		return;
	}

	/* Same code as in modify_command()..
	 * It just sticks a couple of string terminators in our original
	 * text line so that it contains two chunks - that before the string
	 * we are replacing and that after. The string we were searching on
	 * has been corrupted but it doesn't matter. */
	*s = '\0';
	s += strlen(c) - 1;
	*s++ = '\0';

	new = (char *)xalloc(strlen(lp->text) + strlen(r) +
	    strlen(s) + 1, "ed s&r line");
	sprintf(new, "%s%s%s", lp->text, r, s);

	xfree(lp->text);
	lp->text = new;

	fwrite_socket(p, "-- Substitution succeeded.\n");

	/* Done! */
	if (!AUTOLIST(p))
		dump_lines(p, l, l);
	else
		dump_lines(p, 0, 0);
}

static void
ed_ret2(struct user *p)
{
	void (*func)(struct user *, int);
	int op;

	func = (void (*)(struct user *, int))p->stack.sp->u.fpointer;
	dec_stack(&p->stack);
	op = p->stack.sp->u.number;
	dec_stack(&p->stack);

	write_socket(p, "-- MESSAGES END\n");
	/* I don't know why this is required, but it seems that other reset
	 * packets can get misplaced.. */
	reset(p);

	func(p, op);
}

static void
ed_ret(struct user *p, int op)
{
	struct line *l;

	p->flags &= ~U_INED;

	if (op == EDX_NORMAL)
	{
		struct strbuf str;

		init_strbuf(&str, 0, "ed_ret str");

		/* Implode buffer. */
		for (l = p->ed->start; l != (struct line *)NULL; l = l->next)
		{
			add_strbuf(&str, l->text);
			cadd_strbuf(&str, '\n');
		}
		if (!str.offset)
		{
			/* No text in buffer.. */
			free_strbuf(&str);
			op = EDX_NOTEXT;
			fwrite_socket(p, "-- No text!\n");
		}
		else
		{
			pop_strbuf(&str);
			push_malloced_string(&p->stack, str.str);
		}
	}

	p->input_to = NULL_INPUT_TO;

	/* Urgle */
	if (p->ed->rtext.offset)
	{
		/* Stop tfree_ed from freeing this string. */
		p->ed->rtext.len = 0;

		push_number(&p->stack, op);
		push_fpointer(&p->stack, (void (*)())p->ed->ed_exit);

		write_socket(p, "-- MESSAGES WHICH OCCURED WHILST YOU"
		    " WERE EDITING\n");

		pop_strbuf(&p->ed->rtext);
		more_start(p, p->ed->rtext.str, ed_ret2);
	}
	else
		p->ed->ed_exit(p, op);
	tfree_ed(p, 0);
}

static void
ed_recover(struct user *p)
{
	char buff[MAXPATHLEN + 1];
	char *t;

	sprintf(buff, "dead_ed/%s", p->rlname);

	if ((t = read_file(buff)) == (char *)NULL)
	{
		fwrite_socket(p, "-- No buffer to recover.\n");
		ed_prompt(p);
		return;
	}
	ed_clean(p, (FILE *)NULL);

	append_text(p, t, "\n");
	fwrite_socket(p, "-- Buffer recovered.\n");
	dump_lines(p, 0, 0);
	xfree(t);
	unlink(buff);
}

static void
ed_next(struct user *p, char *c)
{
#ifdef REALLY_DEBUG_EDITOR
	check_ed_integrity(p);
#endif

	if (!strcmp(c, "."))
	{
		if (p->ed->insert)
		{
			fwrite_socket(p, "-- Leaving insert mode.\n");
			leave_insert(p);
		}
		else
		{
			fwrite_socket(p, "-- Saving...\n");
			ed_ret(p, EDX_NORMAL);
		}
		return;
	}
	if (*c == '~')
	{

#define EXIST_LINES_CHECK       if (p->ed->start == (struct line *)NULL) \
				do { \
					fwrite_socket(p, "-- No lines!\n"); \
					ed_prompt(p); \
					return; \
				} while (0)

#define ARG_CHECK		do { \
					do \
						c++; \
					while (*c == ' '); \
					if (*c == '\0') \
					{ \
						fwrite_socket(p, \
						    "-- Must supply a line " \
						    "number.\n"); \
						ed_prompt(p); \
						return; \
					} \
				} while (0)


		switch(*(++c))
		{
		    case 'c':	/* Change a line */
			EXIST_LINES_CHECK;
			change_line(p, ++c);
			return;
						     
#if defined(D_COMMAND) || defined(DEBUG_EDITOR)
		    case 'D':	/* Dump ed buffer. */
			dump_ed_buffer(p);
			break;
#endif

		    case 'd':	/* Delete lines */
			EXIST_LINES_CHECK;
			delete_lines(p, ++c);
			return;

		    case 'h':	/* Show help */
			/* Bypass the buffering system.. */
			p->flags |= U_UNBUFFERED_TEXT;
			dump_file(p, "msg", "edit", DUMP_CAT);
			p->flags &= ~U_UNBUFFERED_TEXT;
			break;

		    case 'i':	/* Go to insert mode. */
			EXIST_LINES_CHECK;
			if (p->ed->insert)
			{
				fwrite_socket(p,
				    "-- Already in insert mode!\n");
				break;
			}
			ARG_CHECK;
			insert_mode(p, c);
			return;

		    case 'j':	/* Join lines */
			EXIST_LINES_CHECK;
			join_lines(p, ++c);
			return;
			

		    case 'l':	/* List lines */
		    {
			int start, end, i;

			if (*++c == '\0' || !(i = sscanf(c, "%d*[ ,\t]%d",
			    &start, &end)))
			{
				start = p->ed->curr - p->morelen + 1;
				end = p->ed->curr;
			}
			else if (i != 2)
				end = start + p->morelen - 1;
			dump_lines(p, start, end);
			return;
		    }

		    case 'm':	/* Move lines */
			EXIST_LINES_CHECK;
			move_lines(p, ++c);
			return;

		    case 'o':	/* Options */
			ed_options(p, ++c);
			return;

		    case 'q':	/* Exit without saving */
			fwrite_socket(p, "-- Aborted.\n");
			ed_ret(p, EDX_ABORT);
			return;

		    case 'r':	/* Recover */
			ed_recover(p);
			return;

		    case 's':	/* Search & Replace */
			EXIST_LINES_CHECK;
			ed_search_replace(p, ++c);
			return;

		    default:
			fwrite_socket(p, "-- Unknown tilde escape.\n");
			break;
		}
		ed_prompt(p);
		return;
	}

	/* Just for Mr. Wallis..
	 * Allow lines starting \~... to be inserted as ~...
	 */
	if (strlen(c) >= 2 && c[0] == '\\' && c[1] == '~')
		c++;

	insert_line(p, c);
	ed_prompt(p);
}

int
ed_start(struct user *p, void (*exit_func)(struct user *, int), int max,
    int flags)
{
	/* Multiple input_to protection.. */
	if (p->input_to != NULL_INPUT_TO)
	{
		if (flags & ED_STACKED_TEXT)
			pop_stack(&p->stack);
		if (flags & ED_STACKED_TOK)
			pop_stack(&p->stack);
		if (exit_func != (void (*)(struct user *, int))NULL)
			exit_func(p, EDX_ABORT);
		return 0;
	}
#ifdef DEBUG
	if (p->ed != (struct ed_buffer *)NULL)
		fatal("ed_start called with current ed buffer.");
	if ((flags & ED_STACKED_TOK) && !(flags & ED_STACKED_TEXT))
		fatal("ed_start called with stacked token and no "
		    "stacked text.");
#endif

	p->ed = (struct ed_buffer *)xalloc(sizeof(struct ed_buffer),
	    "ed buffer");
	p->flags |= U_INED;
	p->ed->start = p->ed->last = p->ed->tmp = p->ed->tmp_last =
	    (struct line *)NULL;
	p->ed->max = max;
	p->ed->curr = 1;
	p->ed->insert = 0;
	p->ed->ed_exit = exit_func;
	p->ed->flags = flags;

	init_strbuf(&p->ed->rtext, 0, "ed_start rtext");

	fwrite_socket(p, "-- Jeamland editor, type '~h' for help.\n");
	if (flags & ED_STACKED_TEXT)
	{
		int sl = strlen(p->stack.sp->u.string);

		append_text(p, p->stack.sp->u.string,
		    (flags & ED_STACKED_TOK) ? (p->stack.sp - 1)->u.string :
		    "\n");
		pop_stack(&p->stack);
		if (flags & ED_STACKED_TOK)
			pop_stack(&p->stack);

		if ((flags & ED_INFO) || (p->medopts & U_EDOPT_INFO))
			fwrite_socket(p, "-- %d line%s, %d byte%s\n",
			    p->ed->curr - 1, p->ed->curr == 2 ? "" : "s",
			    sl, sl == 1 ? "" : "s");
	}
	dump_lines(p, 0, 0);
	return 1;
}

