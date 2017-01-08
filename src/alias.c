/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	alias.c
 * Function:	User alias parsing and support
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#ifndef NeXT
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <netinet/in.h>
#include "jeamland.h"

extern struct command commands[], partial_commands[];
extern struct feeling *feelings;
extern time_t current_time;

struct alias *galiases = (struct alias *)NULL;
int eval_depth;

#ifdef HASH_ALIAS_FUNCS
struct hash *ahash = (struct hash *)NULL;
#endif

void
reset_eval_depth()
{
	eval_depth = 0;
}

/* Alias flags */
#define AL_ALO		0x1
#define AL_SKIP		0x2
#define AL_FB		0x4
#define AL_FUNC		0x8
#define AL_PFUNC	0x10
#define AL_SKFUNC	0x20
#define AL_COND		0x40
#define AL_FBB		0x80

#define AL_SKIPPING (flags & AL_SKIP)

static void
alias_indent(struct user *p)
{
	int num = p->alias_indent;

	while (num--)
		write_socket(p, "    ");
}

static int
alias_is_int(struct svalue *v)
{
	char buf[0x100];
	int i;

	/* Check for the override character first. */
	if (v->u.string[0] == '$')
	{
		strcpy(v->u.string, v->u.string + 1);
		return 0;
	}

	i = atoi(v->u.string);
	sprintf(buf, "%d", i);

	return !strcmp(buf, v->u.string);
}

static void
alias_conv_int(struct svalue *v)
{
	int i;

	i = atoi(v->u.string);

	free_svalue(v);
	v->type = T_NUMBER;
	v->u.number = i;
}

static int
alias_both_ints(struct svalue *v1, struct svalue *v2)
{

#ifdef DEBUG
	if (v1->type != T_STRING)
		fatal("alias_both_ints: v1 not a string.");
	if (v2->type != T_STRING)
		fatal("alias_both_ints: v2 not a string.");
#endif

	/* NOTE: & not && - I don't want this to short-circuit. */
	if (alias_is_int(v1) & alias_is_int(v2))
	{
		alias_conv_int(v1);
		alias_conv_int(v2);
		return 1;
	}
	return 0;
}

static int
a_push(struct user *p, char *arg)
{
	return 1;
}

static int
a_copy(struct user *p, char *arg)
{
	int a = atoi(arg);

	if (STACK_EMPTY(&p->alias_stack))
	{
		write_socket(p, "copy(): Stack is empty!\n");
		return 0;
	}
	if (a < 1)
	{
		write_socket(p, "copy(): Bad argument, %s\n", arg);
		return 0;
	}
	while (--a)
	{
		if (STACK_FULL(&p->alias_stack))
		{
			write_socket(p, "Alias stack overflow.\n");
			return 0;
		}
		if (p->flags & U_DEBUG_ALIAS)
		{
			alias_indent(p);
			write_socket(p, "  * Pushing [%s].\n",
			    p->alias_stack.sp->u.string);
		}
		push_string(&p->alias_stack,
		    p->alias_stack.sp->u.string);
	}
	strcpy(arg, p->alias_stack.sp->u.string);
	return 1;
}

static int
a_pop(struct user *p, char *arg)
{
	int a = atoi(arg);

	if (a < 0)
	{
		write_socket(p, "pop(): Bad argument, %d.\n", arg);
		return 0;
	}
	while (a--)
	{
		if (STACK_EMPTY(&p->alias_stack))
		{
			write_socket(p, "pop(): Stack underflow.\n");
			return 0;
		}
		pop_stack(&p->alias_stack);
	}
	return 1;
}

static int
a_map(struct user *p, char *arg)
{
	char *q, *r;
	int i = 0, len;
	
	char *str, *tok;

	if (STACK_SIZE(&p->alias_stack) < 2)
	{
		write_socket(p,
		    "map(): Stack does not contain two elements.\n");
		return 0;
	}

	/* Copy these and dec the stack to make it available to sub-aliases */
	str = (p->alias_stack.sp - 1)->u.string;
	tok = p->alias_stack.sp->u.string;
	dec_n_elems(&p->alias_stack, 2);

	q = str;
	len = strlen(arg);

	/* Arghhh. can't use strtok() as this could call other functions
	 * which use it such as f_tell() - simluate it using strpbrk()
	 * Hmm.. this is actually neater. */
	do
	{
		char *cmd;

		r = q;

		if ((q = strpbrk(r, tok)) != (char *)NULL)
			*q++ = '\0';

		cmd = (char *)xalloc(len + strlen(r) + 2, "a_map cmd");

		sprintf(cmd, "%s %s", arg, r);
		parse_command(p, &cmd);

		xfree(cmd);
		i++;
	} while (q != (char *)NULL);

	xfree(str);
	xfree(tok);
	sprintf(arg, "%d", i);
	return 1;
}

static int
a_lower_case(struct user *p, char *arg)
{
	strcpy(arg, lower_case(arg));
	return 1;
}

static int
a_capitalise(struct user *p, char *arg)
{
	strcpy(arg, capitalise(arg));
	return 1;
}

static int
a_capfirst(struct user *p, char *arg)
{
	strcpy(arg, capfirst(arg));
	return 1;
}

static int
a_pluralise(struct user *p, char *arg)
{
	strcpy(arg, pluralise_verb(arg));
	return 1;
}

static int
a_equal(struct user *p, char *arg)
{
	if (STACK_EMPTY(&p->alias_stack))
	{
		write_socket(p, "=(): Nothing on stack to compare with.\n");
		return 0;
	}
	if (!strcmp(arg, p->alias_stack.sp->u.string))
		strcpy(arg, "1");
	else
		strcpy(arg, "0");
	pop_stack(&p->alias_stack);
	return 1;
}

static int
a_tilde(struct user *p, char *arg)
{
	if (STACK_EMPTY(&p->alias_stack))
	{
		write_socket(p, "~(): Nothing on stack to compare with.\n");
		return 0;
	}
	if (strstr(arg, p->alias_stack.sp->u.string) != (char *)NULL)
		strcpy(arg, "1");
	else
		strcpy(arg, "0");
	pop_stack(&p->alias_stack);
	return 1;
}

static int
a_plus(struct user *p, char *arg)
{
	if (STACK_EMPTY(&p->alias_stack))
	{
		write_socket(p, "+(): Nothing on stack to add to.\n");
		return 0;
	}
	sprintf(arg, "%d", atoi(arg) + atoi(p->alias_stack.sp->u.string));
	pop_stack(&p->alias_stack);
	return 1;
}

static int
a_multiply(struct user *p, char *arg)
{
	if (STACK_EMPTY(&p->alias_stack))
	{
		write_socket(p, "*(): Nothing on stack to multiply to.\n");
		return 0;
	}
	sprintf(arg, "%d", atoi(arg) * atoi(p->alias_stack.sp->u.string));
	pop_stack(&p->alias_stack);
	return 1;
}

static int
a_divide(struct user *p, char *arg)
{
	if (STACK_EMPTY(&p->alias_stack))
	{
		write_socket(p, "/(): Nothing on stack to divide.\n");
		return 0;
	}
	sprintf(arg, "%d", atoi(p->alias_stack.sp->u.string) / atoi(arg));
	pop_stack(&p->alias_stack);
	return 1;
}

static int
a_lt(struct user *p, char *arg)
{
	struct svalue sv;
	int cmp;

	if (STACK_EMPTY(&p->alias_stack))
	{
		write_socket(p, "<(): Nothing on stack to compare with.\n");
		return 0;
	}
	sv.type = T_STRING;
	sv.u.string = string_copy(arg, "a_lt tmp");

	if (alias_both_ints(p->alias_stack.sp, &sv))
		cmp = p->alias_stack.sp->u.number < sv.u.number;
	else
		cmp = strcmp(p->alias_stack.sp->u.string, sv.u.string) < 0;

	if (cmp)
		strcpy(arg, "1");
	else
		strcpy(arg, "0");
	pop_stack(&p->alias_stack);
	free_svalue(&sv);
	return 1;
}

static int
a_gt(struct user *p, char *arg)
{
	struct svalue sv;
	int cmp;

	if (STACK_EMPTY(&p->alias_stack))
	{
		write_socket(p, ">(): Nothing on stack to compare with.\n");
		return 0;
	}
	sv.type = T_STRING;
	sv.u.string = string_copy(arg, "a_gt tmp");

	if (alias_both_ints(p->alias_stack.sp, &sv))
		cmp = p->alias_stack.sp->u.number > sv.u.number;
	else
		cmp = strcmp(p->alias_stack.sp->u.string, sv.u.string) > 0;

	if (cmp)
		strcpy(arg, "1");
	else
		strcpy(arg, "0");
	pop_stack(&p->alias_stack);
	free_svalue(&sv);
	return 1;
}

static int
a_strlen(struct user *p, char *arg)
{
	sprintf(arg, "%d", strlen(arg));
	return 1;
}

static int
a_substr(struct user *p, char *arg)
{
	/* Stack contains, String, Start index.
	 * Arg contains - number of elements.
	 */
	int s, e, len;
	char *str;

	if (STACK_SIZE(&p->alias_stack) < 2)
	{
		write_socket(p, "substr(): Insufficient stacked elements.\n");
		return 0;
	}

	str = (p->alias_stack.sp - 1)->u.string;
	len = strlen(str);
	s = atoi(p->alias_stack.sp->u.string);
	e = atoi(arg);

	/* Decisions, decisions.. what to do if s is out of range
	 * How about return a null string ?
	 * Sounds fair enough.. */
	if (s < 0 || s >= len)
	{
		if (p->flags & U_DEBUG_ALIAS)
		{
			alias_indent(p);
			write_socket(p, "  * Start out of range, "
			    "returning null [%d]\n", s);
		}
		*arg = '\0';
		pop_n_elems(&p->alias_stack, 2);
		return 1;
	}

	if (e < 0)
	{
		write_socket(p, "substr(): Bad number of characters, %d.\n",
		    e);
		pop_n_elems(&p->alias_stack, 2);
		return 0;
	}
	
	if (e == 0)
		e = len;

	my_strncpy(arg, str + s, e);
	pop_n_elems(&p->alias_stack, 2);
	return 1;
}

static int
a_time(struct user *p, char *arg)
{
	struct tm *now;

	now = localtime(&current_time);

	if (!strcmp(arg, "hour"))
		sprintf(arg, "%d", now->tm_hour);
	else if (!strcmp(arg, "min"))
		sprintf(arg, "%d", now->tm_min);
	else
	{
		write_socket(p, "Unrecognised argument to time().\n");
		return 0;
	}
	return 1;
}

static int
a_present(struct user *p, char *arg)
{
	struct user *u;

	if ((u = with_user_msg(p, arg)) == (struct user *)NULL)
		return 0;
	strcpy(arg, u->name);
	return 1;
}

static int
a_find_user(struct user *p, char *arg)
{
	struct user *u;

	if ((u = find_user_msg(p, arg)) == (struct user *)NULL)
		return 0;
	strcpy(arg, u->name);
	return 1;
}

static int
a_user_on(struct user *p, char *arg)
{
	if (find_user_absolute(p, arg) == (struct user *)NULL)
		strcpy(arg, "0");
	else
		strcpy(arg, "1");
	return 1;
}

static int
a_environment(struct user *p, char *arg)
{
	struct user *u;

	if ((u = find_user_msg(p, arg)) == (struct user *)NULL)
		return 0;
	strcpy(arg, u->super->fname);
	return 1;
}

static int
a_room_name(struct user *p, char *arg)
{
	struct room *r;

	if (!ROOM_POINTER(r, arg))
	{
		write_socket(p, "Room %s not found.\n", arg);
		return 0;
	}
	strcpy(arg, r->name);
	return 1;
}

static int
a_possessive(struct user *p, char *arg)
{
	struct user *u;

	if ((u = find_user_msg(p, arg)) == (struct user *)NULL)
		return 0;
	strcpy(arg, query_gender(u->gender, G_POSSESSIVE));
	return 1;
}

static int
a_objective(struct user *p, char *arg)
{
	struct user *u;

	if ((u = find_user_msg(p, arg)) == (struct user *)NULL)
		return 0;
	strcpy(arg, query_gender(u->gender, G_OBJECTIVE));
	return 1;
}
	
static int
a_pronoun(struct user *p, char *arg)
{
	struct user *u;

	if ((u = find_user_msg(p, arg)) == (struct user *)NULL)
		return 0;
	strcpy(arg, query_gender(u->gender, G_PRONOUN));
	return 1;
}

static int
a_query_level(struct user *p, char *arg)
{
	struct user *u;

	if ((u = find_user_msg(p, arg)) == (struct user *)NULL)
		return 0;
	sprintf(arg, "%d", u->level);
	return 1;
}

/* Grupe is on the stack.. user is argument. */
static int
a_member_grupe(struct user *p, char *arg)
{
	if (STACK_EMPTY(&p->alias_stack))
	{
		write_socket(p, "member_grupe(): No Grupe on stack.\n");
		return 0;
	}
	if (member_sysgrupe(p->alias_stack.sp->u.string, arg))
		strcpy(arg, "1");
	else
		strcpy(arg, "0");
	pop_stack(&p->alias_stack);
	return 1;
}

/* The alias function table.. */
static struct afunc {
	char	*name;
	int	level;
	int	(*func)(struct user *, char *);
	} afuncs[] = {

	{ "push",		L_VISITOR,	a_push },
	{ "pop",		L_VISITOR,	a_pop },
	{ "copy",		L_VISITOR,	a_copy },
	{ "map",		L_RESIDENT,	a_map },

	{ "lower_case",		L_VISITOR,	a_lower_case },
	{ "capitalise",		L_VISITOR,	a_capitalise },
	{ "capfirst",		L_VISITOR,	a_capfirst },
	{ "pluralise",		L_VISITOR,	a_pluralise },

	{ "=",			L_VISITOR,	a_equal },
	{ "~",			L_VISITOR,	a_tilde },
	{ "+",			L_VISITOR,	a_plus },
	{ "*",			L_VISITOR,	a_multiply },
	{ "/",			L_VISITOR,	a_divide },
	{ "<",			L_VISITOR,	a_lt },
	{ ">",			L_VISITOR,	a_gt },

	{ "strlen",		L_VISITOR,	a_strlen },
	{ "substr",		L_VISITOR,	a_substr },

	{ "time",		L_VISITOR,	a_time },

	{ "present",		L_VISITOR,	a_present },
	{ "find_user",		L_VISITOR,	a_find_user },
	{ "user_on",		L_VISITOR,	a_user_on },
	{ "environment",	L_VISITOR,	a_environment },
	{ "room_name",		L_VISITOR,	a_room_name },

	{ "possessive",		L_VISITOR,	a_possessive },
	{ "objective",		L_VISITOR,	a_objective },
	{ "pronoun",		L_VISITOR,	a_pronoun },

	{ "query_level",	L_VISITOR,	a_query_level },
	{ "member_grupe",	L_VISITOR,	a_member_grupe },

	{ (char *)NULL,		0,		NULL } };

#ifdef HASH_ALIAS_FUNCS
void
alias_hash_stats(struct user *p, int verbose)
{
	hash_stats(p, ahash, verbose);
}

void
hash_alias_funcs()
{
	int i;

	ahash = create_hash(1, "alias_funcs", NULL, 0);

	for (i = 0; afuncs[i].name != (char *)NULL; i++)
		insert_hash(&ahash, (void *)&afuncs[i]);
}
#endif

static int
alias_function(struct user *p, char *func, char *arg)
{
	struct afunc *af;
	int i;

	FUN_START("alias_function");

#ifdef HASH_ALIAS_FUNCS
	if ((af = (struct afunc *)lookup_hash(ahash, func)) ==
	    (struct afunc *)NULL)
		i = -1;
#else
	for (i = 0; afuncs[i].name != (char *)NULL; i++)
		if (!strcmp(afuncs[i].name, func))
			break;
	if (afuncs[i].name == (char *)NULL)
		i = -1;
	else
		af = &afuncs[i];
#endif

	if (i == -1)
	{
		write_socket(p, "Unknown function, %s().\n", func);
		FUN_END;
		return 0;
	}
	if (af->level > p->level)
	{
		write_socket(p, "Permission denied, %s().\n", func);
		FUN_END;
		return 0;
	}
	FUN_START(af->name);
	i = af->func(p, arg);
	FUN_END;
	FUN_END;
	return i;
}

int
expand_alias(struct user *p, struct alias *list, int argc, char **argv,
    char **buff, char **buffer)
{
	struct alias *a;
	char *fob, *execbuf, *cd;
	char *q, *r, *tmp;
	char *curr_func = (char *)NULL;
	char *func_start;
	int num, flags, debug_alias;

	if (list == (struct alias *)NULL)
		return 0;

	FUN_START("expand_alias");

	debug_alias = p->flags & U_DEBUG_ALIAS;

#ifdef DEBUG
	if (p->flags & U_ALIAS_FB)
		fatal("Alias fb set in expand_alias.");
#endif

	/* Inhibit further alias expansion on this verb */
	if (argv[0][0] == '\\')
	{
		(*buff)++;
		argv[0]++;
		if (debug_alias)
		{
			alias_indent(p);
			write_socket(p, "* \\ prefix found, not expanding %s\n",
			    *buff);
		}
		FUN_END;
		return 2;
	}

	FUN_LINE;

	if ((a = find_alias(list, argv[0])) == (struct alias *)NULL)
	{
		FUN_END;
		return 0;
	}

	if (++eval_depth >= EVAL_DEPTH)
	{
		/* Only want to see this message once! */
		if (eval_depth == EVAL_DEPTH)
			write_socket(p,
			    "\n*** Evaluation too long, execution aborted.\n");
		log_file("syslog", "Evaluation too long: %s (%s)",
		    p->capname, argv[0]);
		FUN_END;
		return -1;
	}

	/* Partial alias */
	if (a->key[0] == '\\' && strlen(a->key) > 1)
	{
		unsigned int i;

		/* If no space was supplied.. */
		if (strlen(argv[0]) >= (i = strlen(a->key)))
		{
			if (argc > 1)
				argv[1][-1] = ' ';
			argv[0] += i - 1;
			argv[-1] = &a->key[1];
			argv--, argc++;
		}
	}

	FUN_LINE;

#define AL_ERROR \
		xfree(fob); \
		xfree(execbuf); \
		FREE(curr_func); \
		clean_stack(&p->alias_stack); \
		FUN_END; \
		return -1

	/* Copy fob in case a compound alias unaliases this one *boggle* */
	fob = string_copy(a->fob, "expand_alias fob copy");
	/* Hope this is big enough! */
	r = execbuf = (char *)xalloc(BUFFER_SIZE + strlen(a->fob) +
	    strlen(*buff) + 1, "expand_alias execbuf");
	for (flags = 0, q = fob; *q != '\0'; q++)
	{
		switch (*q)
		{
		    case '\\':
			*r++ = *(++q);
			break;

		    case ';':	/* execute compound alias */
			if (!AL_SKIPPING)
			{
				flags |= AL_ALO;
				*r = '\0';
				if (*execbuf != '\0')
				{
					cd = string_copy(execbuf,
					    "execbuf copy");
					if (debug_alias)
					{
						alias_indent(p);
						write_socket(p,
						    "* Executing command: "
						    "%s\n", cd);
					}
					p->alias_indent++;
					parse_command(p, &cd);
					p->alias_indent--;
					xfree(cd);
				}
			}
			flags &= ~AL_SKIP;
			r = execbuf;
			if (p->flags & U_ALIAS_FB)
			{
				flags |= AL_FBB;
				flags |= AL_FB;
			}
			break;

		    case '#':	/* Comment */
			if (p->flags & U_DEBUG_ALIAS)
			{
				alias_indent(p);
				write_socket(p, "* Found comment, skipping.\n");
			}
			/* Bit nasty...
			 * If we are a # just past a ; then skip this line.
			 * If not, replace # with a ;# and continue.. 
			 * remainder of line will be skipped on next run
			 * through.. */
			if (q != fob && q[-1] != ';')
			{
				*q = ';';
				if (q[1] != '\0' && q[1] != ';')
					q[1] = '#';
			}
			else
				flags |= AL_SKIP;
			break;

		    case ',':	/* Possible implicit push */
			if (!(flags & AL_FUNC))
			{	/* No, it isn't */
				*r++ = *q;
				break;
			}

			*r = '\0';
			if (STACK_FULL(&p->alias_stack))
			{
				write_socket(p, "Alias stack overflow.\n");
				AL_ERROR;
			}

			if (p->flags & U_DEBUG_ALIAS)
			{
				alias_indent(p);
				write_socket(p, "  * Pushing [%s].\n",
				    func_start);
			}
			push_string(&p->alias_stack, func_start);
			r = func_start;
			break;

		    case ')':	/* Possible end of function */
			if (!(flags & AL_FUNC))
			{	/* No, it isn't */
				*r++ = *q;
				break;
			}

			/* Function has ended! */
			*r = '\0';
			r = func_start;

			p->alias_indent++;
			if (debug_alias)
			{
				alias_indent(p);
				write_socket(p,
				    "* Calling %s(%s).\n", curr_func, r);
			}

			if (!alias_function(p, curr_func, r))
			{
				AL_ERROR;
			}

			p->alias_indent--;
			if (flags & AL_PFUNC)
			{
				/* Need to push the result.. */
				if (STACK_FULL(&p->alias_stack))
				{
					write_socket(p,
					    "Alias stack overflow.\n");
					AL_ERROR;
				}
				push_string(&p->alias_stack, r);
				if (debug_alias)
				{
					alias_indent(p);
					write_socket(p,
					    "* Returned [%s] to stack.\n", r);
				}
			}
			else if (debug_alias)
			{
				alias_indent(p);
				write_socket(p,
				    "* Returned [%s].\n", r);
			}

			if (!(flags & AL_SKFUNC))
				while (*r != '\0')
					r++;

			FREE(curr_func);
			flags &= ~AL_FUNC;
			flags &= ~AL_PFUNC;
			flags &= ~AL_SKFUNC;
			flags |= AL_ALO;
			if (flags & AL_COND)
			{
				r = func_start;
				*q = *r;
				goto cond_ret;
			}
			break;
				
		    case '%':	/* Push function call */
		    case '@':	/* Function call */
		    case '&':	/* Discard return function call */
func_hook:
		    {
			struct strbuf func;
			char push;

			if (*q == '&')
			{
				if (*++q != '%' && *q != '@')
				{
					write_socket(p, "Syntax error, & not "
					    "followed by %% or @.\n");
					AL_ERROR;
				}
				flags |= AL_SKFUNC;
			}

			push = *q;

			/* Don't waste time if skipping */
			if (AL_SKIPPING)
				break;

			if (flags & AL_FUNC)
			{
				write_socket(p, "Recursive function calls "
				    "are not supported.\n");
				AL_ERROR;
			}

			init_strbuf(&func, 20, "alias func sbuf");

			while (*++q != '(' && *q != '\0')
				cadd_strbuf(&func, *q);

			if (*q == '\0')
			{
				write_socket(p, "Error in function call: "
				    "( not found.\n");
				free_strbuf(&func);
				AL_ERROR;
			}

			if (debug_alias)
			{
				alias_indent(p);
				write_socket(p,
				    "* Alias function found: [%s]\n",
				    func.str);
			}

			flags |= AL_FUNC;
			if (push == '%')
				flags |= AL_PFUNC;

			/* curr_func must be free at this point... */
			curr_func = func.str;
			pop_strbuf(&func);
			func_start = r;
			break;
		    }

		    case '$':	/* variable substitution */
			FUN_LINE;
			/* Don't waste time if skipping */
			if (AL_SKIPPING)
				break;
			q++;
			if (*q == '!')	/* Forced breakout */
			{
				if (debug_alias && q[1] != '!')
				{
					alias_indent(p);
					write_socket(p,
					    "* $! found, exiting.\n");
				}
				flags |= AL_FB;
				if (*++q == '!')
				{
					if (debug_alias)
					{
						alias_indent(p);
						write_socket(p,
						    "* $!! found, exiting.\n");
					}
					flags |= AL_FBB;
				}
				break;
			}
			if (*q == '%')	/* Pop from stack */
			{
				if (STACK_EMPTY(&p->alias_stack))
				{
					write_socket(p,
					    "Error in alias expansion: "
					    "stack is empty.\n");
					AL_ERROR;
				}
				for (tmp = p->alias_stack.sp->u.string;
				    *tmp != '\0'; tmp++)
					*r++ = *tmp;
				if (debug_alias)
				{
					alias_indent(p);
					write_socket(p, "* Pop stack: %s\n",
					    p->alias_stack.sp->u.string);
				}
				pop_stack(&p->alias_stack);
				break;
			}
			if (*q == '*')	/* Whole line.. if it exists */
			{
				if (argc > 1)
					for (tmp = argv[1]; *tmp != '\0'; tmp++)
						*r++ = *tmp;
				break;
			}
			if (*q == '$')	/* Conditional.. */
			{
				static int not;

				if (flags & AL_FUNC)
				{
					write_socket(p,
					    "Conditionals have no meaning "
					    "inside functions.\n");
					AL_ERROR;
				}
				if (flags & AL_COND)
				{
					write_socket(p,
					    "Recursive conditionals are not "
					    "supported.\n");
					AL_ERROR;
				}
				not = 0;

				flags |= AL_COND;
				flags &= ~AL_SKIP;
				q++;

				if (debug_alias)
				{
					alias_indent(p);
					write_socket(p,
					    "* Conditional parser.\n");
				}
				p->alias_indent++;
cond_ret:
				while (*q != ':')
				{
					if (*q == '!')
					{
						if (debug_alias)
						{
							alias_indent(p);
							write_socket(p,
							    "* Conditional: "
							    "[!]\n");
						}
						q++;
						not = !not;
					}
					if (debug_alias)
					{
						alias_indent(p);
						write_socket(p,
						    "* Conditional: [%c]\n",
						    *q);
					}
					switch (*q)
					{
					    case '0':
						q++;
						flags |= AL_SKIP;
						break;

					    case '1':
						q++;
						flags &= ~AL_SKIP;
						break;

					    case '#':	/* Arg count */
						q++;
						if (debug_alias)
						{
							alias_indent(p);
							write_socket(p,
							    "* Conditional "
							    "(arg): [%c]\n",
							    *q);
						}
						if ((num = *q - '0') >= 0 &&
						    num <= 9)
						{
							q++, num++;
							if (num != argc &&
							    !(num > argc &&
							    *q == '-') &&
							    !(num < argc &&
							    *q == '+'))
								flags |=
								    AL_SKIP;
							if (*q == '+' ||
							    *q == '-')
								q++;
						}
						break;

					    case '@':	/* Function */
					    case '%':
					    case '&':
						goto func_hook;
						break;

					    default:
						write_socket(p,
						    "Unknown character, %c "
						    "in conditional.\n", *q);
						AL_ERROR;

					}
					if (not)
					{
						flags ^= AL_SKIP;
						not = 0;
					}

					if (*q == '|')
					{
						q++;
						if (!(flags & AL_SKIP))
						{
							/* Skip to : */
							while (*q != ':' &&
							    *q != '\0')
								q++;
							break;
						}
					}
					else
					{
						if (*q == '&')
							q++;
						if (flags & AL_SKIP)
						{
							while (*q != ':' &&
							    *q != '\0')
								q++;
							break;
						}
					}
				}
				p->alias_indent--;
				if (debug_alias)
				{
					alias_indent(p);
					write_socket(p,
					    "* Conditional exit: %s\n",
					    flags & AL_SKIP ? "False" : "True");
				}
				flags &= ~AL_COND;
				if (flags & AL_SKIP)
					break;
				if (*q != ':')
				{
					write_socket(p, "Error in conditional "
					    "alias expression, "
					    "'%c' found where ':' expected.\n",
					    *q);
					AL_ERROR;
				}
				break;
			}
			num = *q - '0';
			if (num < 0 || num >= argc)
			{
				switch (*q)
				{
				    case 'N':	/* Name */
					for (tmp = p->name; *tmp != '\0';
					    tmp++)
						*r++ = *tmp;
						break;
				    default:
					write_socket(p,
					    "Error in alias expansion: "
					    "Undefined variable $%c.\n", *q);
					AL_ERROR;
				}
				break;
			}
			cd = strchr(argv[num], ' ');
			for (tmp = argv[num]; *tmp != '\0' &&
			    ((q[1] != '*' && tmp != cd) || q[1] == '*');
			    tmp++)
				*r++ = *tmp;
			if (q[1] == '*')
				q++;
			break;
		    default:
			*r++ = *q;
			break;
		}
		if (flags & AL_FB)
			break;
	}

	if (curr_func != (char *)NULL)
	{
		xfree(curr_func);
		write_socket(p,
		    "Alias warning: execution ended in a function.\n");
	}

	if (flags & AL_SKIP)
	{
		xfree(fob);
		xfree(execbuf);
		FUN_END;

		/* Pretend we get an error.. discards further processing of
		 * this string at least */
		if (flags & AL_ALO)
			return -1;

		/* Pretend we never saw the alias */
		return 0;
	}

	*r = '\0';

	xfree(*buffer);
	*buff = *buffer = execbuf;
	xfree(fob);

	for (q = *buff + strlen(*buff) - 1; isspace(*q); q--)
		*q = '\0';
	while (isspace(**buff))
		(*buff)++;
	if (strlen(*buff) && debug_alias)
	{
		alias_indent(p);
		write_socket(p, "* Alias returning: %s\n", *buff);
	}
	if (flags & AL_FBB)
		p->flags |= U_ALIAS_FB;
	FUN_END;
	return 1;
}

struct alias *
create_alias(char *key, char *fob)
{
	struct alias *a;

	if (key == (char *)NULL || !strlen(key) ||
	    fob == (char *)NULL || !strlen(fob))
		return (struct alias *)NULL;

	a = (struct alias *)xalloc(sizeof(struct alias), "*alias struct");

	a->key = string_copy(key, "alias key");
	a->fob = string_copy(fob, "alias fob");
	a->next = (struct alias *)NULL;
	return a;
}

void
add_alias(struct alias **list, struct alias *a)
{
	for (; *list != (struct alias *)NULL; list = &((*list)->next))
		if (strcmp((*list)->key, a->key) > 0)
			break;
	a->next = *list;
	*list = a;
}

void
free_alias(struct alias *a)
{
	FREE(a->key);
	FREE(a->fob);
	xfree(a);
}

struct alias *
find_alias(struct alias *list, char *key)
{
	struct alias *a;
	int i;

	/* Eek.. handle partial aliases too - messy */
	for (a = list; a != (struct alias *)NULL; a = a->next)
		if (!strcmp(a->key, key) ||
		    (a->key[0] == '\\' && (i = strlen(a->key)) > 1 &&
		    !strncmp(a->key + 1, key, i - 1)))
			return a;
	return (struct alias *)NULL;
}

void
remove_alias(struct alias **list, struct alias *a)
{
	for (; *list != (struct alias *)NULL; list = &((*list)->next))
		if (*list == a)
		{
			*list = a->next;
			free_alias(a);
			return;
		}
#ifdef DEBUG
	fatal("remove_alias: alias not found.");
#endif
}

int
count_aliases(struct alias *list)
{
	struct alias *a;
	int i;

	for (i = 0, a = list; a != (struct alias *)NULL; a = a->next)
		i++;
	return i;
}

void
free_aliases(struct alias **list)
{
	struct alias *a, *b;

	for (a = *list; a != (struct alias *)NULL; a = b)
	{
		b = a->next;
		free_alias(a);
	}
	*list = (struct alias *)NULL;
}

void
restore_alias(struct alias **list, char *c, char *id, char *id2)
{
	struct svalue *sv;
	struct alias *a;

	if ((sv = decode_one(c, "restore_alias")) == (struct svalue *)NULL)
	{
		log_file("error", "Error in alias in %s[%s], line: %s",
		    id, id2, c);
		return;
	}
	if (sv->type == T_VECTOR && sv->u.vec->size == 2 &&
	    sv->u.vec->items[0].type == T_STRING &&
	    sv->u.vec->items[1].type == T_STRING)
	{
		if (find_alias(*list, sv->u.vec->items[0].u.string) !=
		    (struct alias *)NULL)
			log_file("error", "Duplicate alias in %s[%s]: %s (%s)",
			    id, id2, sv->u.vec->items[0].u.string,
			    sv->u.vec->items[1].u.string);
		else
		{
			if ((a = create_alias(sv->u.vec->items[0].u.string,
			    sv->u.vec->items[1].u.string)) !=
			    (struct alias *)NULL)
				add_alias(list, a);
			else
				log_file("error",
				    "Error in alias in %s[%s], line: %s (%s)",
				    id, id2, sv->u.vec->items[0].u.string,
				    sv->u.vec->items[1].u.string);
		}
	}
	else
		log_file("error", "Error in alias in %s[%s], line: %s",
		    id, id2, c);
		
	free_svalue(sv);
}

int
valid_ralias(char *alias)
{
	if (*alias != '\\')
		/* Simple case - alias is not partial, is it a system
		 * command ? If these are hashed, this is very fast! */
		return find_command((struct user *)NULL, alias) ==
		    (struct command *)NULL &&
		    exist_feeling(alias) == (struct feeling *)NULL;
	else
	{
		struct feeling *f;
		int i;
		int al;

		/* Gets complicated - partial aliases.. */
		al = strlen(++alias);

		/* *sigh* Got to scan ALL commands and feelings :( */

		/* Try the partial commands */
		for (i = 0; partial_commands[i].command != (char *)NULL; i++)
			if (!strncmp(partial_commands[i].command, alias, al))
				return 0;

		/* Try normal commands.. */
		for (i = 0; commands[i].command != (char *)NULL; i++)
			if (!strncmp(commands[i].command, alias, al))
				return 0;

		/* Try feelings .. *Yawn* */
		for (f = feelings; f != (struct feeling *)NULL; f = f->next)
			if (!strncmp(f->name, alias, al))
				return 0;

		return 1;
	}
}

void
store_galiases()
{
	FILE *fp;
	struct alias *a;

	if (galiases == (struct alias *)NULL)
	{
		unlink(F_GALIASES);
		return;
	}

	if ((fp = fopen(F_GALIASES, "w")) == (FILE *)NULL)
	{
		log_perror("fopen galiases");
		return;
	}

	for (a = galiases; a != (struct alias *)NULL; a = a->next)
	{
		char *key, *fob;

		key = code_string(a->key);
		fob = code_string(a->fob);
		fprintf(fp, "alias ({\"%s\",\"%s\",})\n", key, fob);
		xfree(key);
		xfree(fob);
	}
	fclose(fp);
}

void
restore_galiases()
{
	FILE *fp;
	char *buf;
	struct stat st;

	free_aliases(&galiases);

	if ((fp = fopen(F_GALIASES, "r")) == (FILE *)NULL)
		return;

	if (fstat(fileno(fp), &st) == -1)
	{
		log_perror("fstat");
		fatal("Couldn't stat open file.");
	}

	if (!st.st_size)
	{
		unlink(F_GALIASES);
		return;
	}

	buf = (char *)xalloc((size_t)st.st_size, "restore galiases");

	while (fgets(buf, (int)st.st_size, fp) != (char *)NULL)
	{
		if (!strncmp(buf, "alias ", 6))
		{
			restore_alias(&galiases, buf, "global", "aliases");
			continue;
		}
	}

	xfree(buf);
	fclose(fp);
}

void
alias_profile(struct user *p, char *fname)
{
	extern void f_alias(struct user *, int, char **);
	FILE *fp;
	struct stat st;
	char *buf;

	if ((fp = fopen(fname, "r")) == (FILE *)NULL)
		return;

	if (fstat(fileno(fp), &st) == -1)
	{
		log_perror("fstat");
		fatal("Couldn't stat open file.");
	}

	if (!st.st_size)
	{
		unlink(fname);
		return;
	}

	buf = (char *)xalloc((size_t)st.st_size, "alias profile");

	while (fgets(buf, (int)st.st_size, fp) != (char *)NULL)
	{
		char *q, *argv[3];

		if (ISCOMMENT(buf))
			continue;

		if ((q = strchr(buf, '\n')) != (char *)NULL)
			*q = '\0';

		if ((q = strchr(buf, ' ')) == (char *)NULL)
		{
			log_file("error", "Bad alias in %s: %s", fname, buf);
			continue;
		}
		*q++ = '\0';
		argv[0] = "alias";
		argv[1] = buf;
		argv[2] = q;
		f_alias(p, 3, argv);
	}
	fclose(fp);
	xfree(buf);
}

