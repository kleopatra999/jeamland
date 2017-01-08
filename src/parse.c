/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	parse.c
 * Function:	The command parser (not including alias expansion)
 **********************************************************************/
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <sys/param.h>
#include <netinet/in.h>
#include "jeamland.h"

extern struct command commands[], partial_commands[];
extern struct alias *galiases;
extern int sysflags, eval_depth;

int command_count;
#ifdef HASH_COMMANDS
struct hash *chash;
#endif

#ifdef HASH_COMMANDS
void
command_hash_stats(struct user *p, int verbose)
{
	hash_stats(p, chash, verbose);
}

void
hash_commands()
{
	int i;

	/* use prime4 - We know the commands table size in advance. */
	chash = create_hash(3, "commands", NULL, 0);

	for (i = 0; commands[i].command != (char *)NULL; i++)
		insert_hash(&chash, (void *)&commands[i]);
}
#endif

struct command *
find_command(struct user *p, char *cmd)
{
	int i, level;
#ifdef HASH_COMMANDS
	struct command *fcmd;
#endif

	if (p == (struct user *)NULL)
		level = L_MAX;
	else
		level = p->level;

	for (i = 0; partial_commands[i].command != (char *)NULL; i++)
	{
		if (partial_commands[i].level > level)
			break;
		if (!strncmp(partial_commands[i].command, cmd,
		    strlen(partial_commands[i].command)))
			return &partial_commands[i];
	}

#ifdef HASH_COMMANDS
	if ((fcmd = (struct command *)lookup_hash(chash, cmd)) ==
	    (struct command *)NULL || fcmd->level > level)
		return (struct command *)NULL;
	return fcmd;
#else
	for (i = 0; commands[i].command != (char *)NULL; i++)
	{
		if (commands[i].level > level)
			break;
		if (!strcmp(commands[i].command, cmd))
			return &commands[i];
	}
	return (struct command *)NULL;
#endif /* HASH_COMMANDS */
}

/* Not pretty... */
void
check_help_files()
{
	struct vector *vec;
	struct command *cmd;
	char buff[MAXPATHLEN + 1];
	int level, i;
	char *levelname = (char *)NULL;
	struct svalue sv;

	sv.type = T_STRING;

	for (level = L_VISITOR; level <= L_MAX; level++)
	{
		/* Copy to avoid lower_case buffer clashes. */
		COPY(levelname, lower_case(level_name(level, 0)),
		    "check helps levelname");
		sprintf(buff, "help/%s", levelname);

		if ((vec = get_dir(buff, 0)) == (struct vector *)NULL)
		{
			log_file("helps",
			    "No help files found for %s commands!", levelname);
			continue;
		}
		log_file("helps", "Checking help files for %s commands.",
		    levelname);
		/* First check this way.. */
		for (i = vec->size; i--; )
		{
			if ((cmd = find_command((struct user *)NULL,
			    vec->items[i].u.string)) == (struct command *)NULL)
				log_file("helps", "\tExtra file: %s",
				    vec->items[i].u.string);
			else if (cmd->level != level)
				log_file("helps",
				    "\tMisplaced file: %s (is %s command)",
				    cmd->command,
				    lower_case(level_name(cmd->level, 0)));
		}
		/* Then this way! */
		for (i = 0; partial_commands[i].command != (char *)NULL; i++)
		{
			if (partial_commands[i].level > level)
				break;
			else if (partial_commands[i].level == level)
			{
				sv.u.string = partial_commands[i].command;
				if (member_vector(&sv, vec) == -1)
					log_file("helps", "\tMissing file: %s",
					    partial_commands[i].command);
			}
		}
		for (i = 0; commands[i].command != (char *)NULL; i++)
		{
			if (commands[i].level > level)
				break;
			else if (commands[i].level == level)
			{
				sv.u.string = commands[i].command;
				if (member_vector(&sv, vec) == -1)
					log_file("helps", "\tMissing file: %s",
					    commands[i].command);
			}
		}
		free_vector(vec);
	}
	FREE(levelname);
}

/* This is decidedly yucky */
void
check_cmd_table()
{
	int i, j, lev, l, m;

	for (lev = i = 0; commands[i].command != (char *)NULL; i++)
	{
#ifdef PROFILING
		commands[i].calls = 0;
#endif

		if (commands[i].args >= MAX_ARGV)
			fatal("Too large ARGV in '%s'", commands[i].command);
		if (commands[i].level < lev)
			fatal("Cmdtable level integrity breached at '%s'",
			    commands[i].command);
		lev = commands[i].level;
		l = strlen(commands[i].command);
		for (j = 0; j < i; j++)
		{
			m = strlen(commands[j].command);

			if (!strcmp(commands[i].command, commands[j].command)
			    || ((commands[l > m ? j : i].flags & CMD_PARTIAL) &&
			    !strncmp(commands[i].command, commands[j].command,
			    l > m ? m : l)))
			fatal("Cmdtable integrity breached at '%s' vs. '%s'",
			    commands[l < m ? i : j].command,
			    commands[l < m ? j : i].command);
		}
	}
}

int
modify_command(struct user *p, char **rbuff, char **buffer)
{
	int i;
	char *buff = *rbuff;

	FUN_START("modify_command");

	if (++eval_depth >= EVAL_DEPTH)
	{
		/* Only want to see this message once! */
		if (eval_depth == EVAL_DEPTH)
			write_socket(p,
			    "\n*** Evaluation too long, execution aborted.\n");
		log_file("syslog", "Evaluation too long: %s (%s)",
		    p->capname, buff);
		FUN_END;
		return 0;
	}

	if (buff[0] == '^')
	{
		char *buf = string_copy(buff, "modifier buff copy");
		char *repl = strchr(buf + 1, '^');
		char *hbuf = p->history->items[(p->history_ptr - 1) %
		    HISTORY_SIZE].u.string;
		char *hptr, *ind;

		if (buf[1] == '^')
		{
			xfree(buf);
			write_socket(p, "Missing search string.\n");
			FUN_END;
			return 0;
		}
		if (repl == (char *)NULL)
		{
			xfree(buf);
			write_socket(p, "Missing ^\n");
			FUN_END;
			return 0;
		}
		if (hbuf == (char *)NULL)
		{
			xfree(buf);
			write_socket(p, "No history.\n");
			FUN_END;
			return 0;
		}
		hptr = string_copy(hbuf, "Modifier hist copy");
		*repl++ = '\0';
		if ((ind = strstr(hptr, buf + 1)) == (char *)NULL)
		{
			xfree(buf);
			xfree(hptr);
			write_socket(p, "Modifier failed.\n");
			FUN_END;
			return 0;
		}
		*ind = '\0';
		ind += strlen(buf + 1) - 1;
		*ind++ = '\0';
		xfree(*buffer);
		*buffer = (char *)xalloc(strlen(hptr) + strlen(repl) +
		    strlen(ind) + 1, "parse_command modifier");
		sprintf(*buffer, "%s%s%s", hptr, repl, ind);
		xfree(buf);
		xfree(hptr);
		write_socket(p, "Doing: %s\n", *buffer);
		*rbuff = *buffer;
	}
	else if (buff[0] == '!')
	{
		char *hptr = (char *)NULL;
		char *rest = strchr(buff, ' ');
		char *mod = (char *)NULL, *mptr;
		int num, aft = 0;

		/* Can corrupt buffer here, it is no longer needed. */

		/* Ugh.... handle !arg^mod1^mod2 extra
		 * Set buff to arg, mod to ^mod1^mod2 extra
		 */
		if ((mptr = strchr(buff, '^')) != (char *)NULL)
		{
			mod = string_copy(mptr, "history modifier");
			*mptr = '\0';
		}
		else if (rest != (char *)NULL)
			*rest++ = '\0';

		/* Simple case.. last entry */
		if ((num = strlen(buff)) == 1 || buff[1] == '!')
		{
			hptr = p->history->items[(p->history_ptr - 1) %
			    HISTORY_SIZE].u.string;
			if (num > 1 && buff[2] != '\0' &&
			    buff[2] != ' ')
			{
				if (rest != (char *)NULL)
					*--rest = ' ';
				rest = buff + 2;
				aft = 1;
			}
		}
		/* More complicated case... history by number */
		else if ((num = atoi(buff + 1)))        /*Poke -Wall*/
		{
			if (num >= p->history_ptr ||
			    num < p->history_ptr - HISTORY_SIZE ||
			    num < 0)
				hptr = (char *)NULL;
			else
				hptr = p->history->items[num %
				    HISTORY_SIZE].u.string;
		}
		/* Most complicated case, history by partial string. */
		else
		{
			for (i = p->history_ptr - 1; i > p->history_ptr -
			    HISTORY_SIZE && i > 0; i--)
				if (p->history->items[i %
				    HISTORY_SIZE].u.string !=
				    (char *)NULL && !strncmp(buff + 1,
				    p->history->items[i %
				    HISTORY_SIZE].u.string, strlen(buff + 1)))
				{
					hptr = p->history->items[i %
					    HISTORY_SIZE].u.string;
					break;
				}
		}
		if (hptr == (char *)NULL)
		{
			write_socket(p, "%s: Event not found.\n", buff + 1);
			FREE(mod);
			FUN_END;
			return 0;
		}
		if (mod != (char *)NULL)
		{
			/* History modifiers... 'orrible things */
			COPY(p->history->items[p->history_ptr %
			    HISTORY_SIZE].u.string, hptr, "tmp_history");
			p->history_ptr++;
			/* Recursion!! :) */
			if (!modify_command(p, &mod, &mod))
			{
				xfree(mod);
				FUN_END;
				return 0;
			}
			p->history_ptr--;
			xfree(*buffer);
			*buffer = *rbuff = mod;
			/* Do not want it to write 'Doing: ' twice
			 * so return here.. */
			FUN_END;
			return 1;
		}
		if (rest != (char *)NULL)
		{
			/* rest is in buff. Copy it to prevent problems. */
			char *t = string_copy(rest, "history rest copy");
			xfree(*buffer);
			*buffer = (char *)xalloc(strlen(hptr) + strlen(t) + 2,
			    "parse_command history");
			if (aft)
				sprintf(*buffer, "%s%s", hptr, t);
			else
				sprintf(*buffer, "%s %s", hptr, t);
			xfree(t);
		}
		else
		{
			COPY(*buffer, hptr, "parse_command");
		}
		write_socket(p, "Doing: %s\n", *buffer);
		*rbuff = *buffer;
	}
	FUN_END;
	return 1;
}

void
parse_command(struct user *p, char **buffer)
{
	int i, j, k;
	int argc;
	char **argv;
	char *pargv[MAX_ARGV + 1];
	char *buff = *buffer, *q;
	struct command *cmd;
	static int expanding_alias = 0;
	unsigned long al_flags;
	extern int parse_feeling(struct user *, int, char **);
	extern void audit(char *, ...);

	command_count++;

	if (!strlen(buff))
		return;

	FUN_START("parse_command");
	FUN_ARG(buff);

	for (q = buff + strlen(buff) - 1; isspace(*q); q--)
	{
		if (q[-1] == '\\')
		{
			q[-1] = *q;
			*q = '\0';
			break;
		}
		*q = '\0';
	}
	while (isspace(*buff))
		buff++;

	if (!strlen(buff))
	{
		FUN_END;
		return;
	}

	if (!modify_command(p, &buff, buffer))
	{
		FUN_END;
		return;
	}

	/* Don't like hardcoding these... */
	if (strcmp(buff, "history") && strncmp(buff, "sudo ", 5) &&
	    !expanding_alias)
	{
		COPY(p->history->items[p->history_ptr % HISTORY_SIZE].u.string,
		    buff, "*history");
		p->history_ptr++;
	}

	/*write_socket(p, "* Parser got command: [%s]\n", buff);*/

	argv = &pargv[1];

	/* Personal aliases.. */
	if (!expanding_alias)
	{
		clean_stack(&p->alias_stack);
		p->flags &= ~U_ALIAS_FB;
		p->alias_indent = 0;
	}

	al_flags = p->flags & U_ALIAS_FB;

	expanding_alias++;
	do
	{
		j = k = 0;
		/*write_socket(p, "* Expanding alias: [%s]\n", buff);*/
		argv[0] = buff;
		argc = 1;
		for (i = 1; i < MAX_ARGV; i++)
			argv[i] = (char *)NULL;
		if ((q = strchr(buff, ' ')) != (char *)NULL)
		{
			*q = '\0';
			do
				argv[argc++] = ++q;
			while ((q = strchr(q, ' ')) != (char *)NULL && argc <
			    MAX_ARGV);
		}

		if (p->flags & U_INHIBIT_ALIASES)
			p->flags |= U_ALIAS_FB;

	/* I'm sure I understood this when I wrote it :-(.. 
	 * Let's see..
	 * expand_alias() returns:
	 *	-1	Error.
	 * 	 0	Couldn't expand alias.
	 *	 1	Successfully expanded alias.
	 *	 2	Further expansion overidden.
	 *
	 * So, as long as each expand_alias() returns 0, we try the next
	 * type (user, room then global).
	 * Loop will exit if:
	 *	U_ALIAS_FB flag is set (Forced break).
	 *	Any expand_alias() call returns 2 or -1.
	 *	All expand_alias() calls return 0.
	 */
	} while (!(p->flags & U_ALIAS_FB) &&
	    ((i = expand_alias(p, p->alias, argc, argv, &buff, buffer)) == 1 ||
	    (i == 0 && (j = expand_alias(p, p->super->alias, argc, argv,
	    &buff, buffer)) == 1) ||
	    (i == 0 && j == 0 &&
	    (k = expand_alias(p, galiases, argc, argv, &buff, buffer)) == 1)));

	expanding_alias--;
	if (!expanding_alias && !STACK_EMPTY(&p->alias_stack))
		write_socket(p,
		    "Warning: Elements remaining on alias stack.\n");

	if (!(p->flags & U_ALIAS_FB) &&
	    (i == -1 || j == -1 || k == -1 || !strlen(buff)))
	{
		/* Error in alias expansion */
		FUN_END;
		return;
	}

	if ((p->flags & U_ALIAS_FB) && !al_flags)
		p->flags &= ~U_ALIAS_FB;

	/* Allow sentences to override room exits */
	if (!(p->flags & U_SKIP_SENTENCES) && sent_cmd(p, argv[0], argv[1]))
	{
		FUN_END;
		return;
	}

	if (handle_exit(p, buff, 0))
	{
		FUN_END;
		return;
	}

	if ((cmd = find_command(p, argv[0])) != (struct command *)NULL)
	{
		i = strlen(cmd->command);

		/* Handle partial commands, modify argv */
		if ((cmd->flags & CMD_PARTIAL) &&
		    (unsigned int)i != strlen(argv[0]))
		{
			/* While there should be a space after argv[0],
			 * the end of the old argv[0] is now argv[1] and so
			 * should no longer have a space */
			if (argc > 1)
				argv[1][-1] = ' ';
			argv[0] += i;
			argv[-1] = cmd->command;
			argv--, argc++;
		}
#ifdef AUDIT_COMMANDS
		if (cmd->flags & CMD_AUDIT)
			audit("[%s@%s]\n\t%s %s", p->capname,
			    p->super->fname, argv[0],
			    argc > 1 ? argv[1] : "<no-args>");
#endif
		if (argc <= cmd->args)
		{
			if (cmd->syntax != NULL)
				write_socket(p, "Syntax: %s%s%s\n",
				    argv[0],
				    cmd->flags & CMD_PARTIAL ? "" : " ",
				    cmd->syntax);
			else
				write_socket(p,
				    "Insufficient arguments to '%s'.\n",
				    cmd->command);
			FUN_END;
			return;
		}

		for (i = 1; i < cmd->args; i++)
		{
#ifdef DEBUG
			char *c;
			if ((c = strchr(argv[i], ' ')) == (char *)NULL)
				fatal("No space found in argv.");
			else
				*c = '\0';
#else
			*strchr(argv[i], ' ') = '\0';
#endif
		}

#ifdef PROFILING
		cmd->calls++;
#endif
		FUN_START(cmd->command);
		FUN_ARG(argv[1]);
		FUN_ADDR(cmd->func);
		cmd->func(p, argc, argv);
		FUN_END;
	}
	else if (!parse_feeling(p, argc, argv))
		if (p->level < L_RESIDENT)
			write_socket(p,
			    "Unknown command, %s; try typing: help\n", buff);
		else
			write_socket(p, "Unknown command, %s.\n", buff);

	FUN_END;
}

