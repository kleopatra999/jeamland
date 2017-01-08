/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	soul.c
 * Function:	The feelings database
 **********************************************************************/
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/param.h>
#include <string.h>
#include <netinet/in.h>
#include "jeamland.h"

extern struct user *users;

struct feeling *feelings = (struct feeling *)NULL;
#ifdef MALLOC_STATS
int num_feelings, feeling_size;
#endif
#ifdef HASH_FEELINGS
static struct hash *fhash = (struct hash *)NULL;
#endif

struct feeling *
create_feeling()
{
	struct feeling *f = (struct feeling *)xalloc(sizeof(struct feeling),
	    "*feeling struct");
#ifdef MALLOC_STATS
	feeling_size += sizeof(struct feeling);
#endif

	f->type = f->no_verb = 0;
#ifdef PROFILING
	f->calls = 0;
#endif
	f->name = f->adverb = f->prep = (char *)NULL;
	f->next = (struct feeling *)NULL;
	return f;
}

void
free_feeling(struct feeling *f)
{
	FREE(f->name);
	FREE(f->adverb);
	FREE(f->prep);
	xfree(f);
}

struct feeling *
exist_feeling(char *name)
{
#ifdef HASH_FEELINGS
	return (struct feeling *)lookup_hash(fhash, name);
#else
	struct feeling *f;

	for (f = feelings; f != (struct feeling *)NULL; f = f->next)
		if (!strcmp(name, f->name))
			return f;
	return (struct feeling *)NULL;
#endif
}

void
load_feelings()
{
	struct feeling *f, *next_f;
	struct feeling **ff;	/* Speeds up feeling addition */
	char *buf, *q;
	FILE *fp;
	int section, line;
	char buff[BUFFER_SIZE];

	for (f = feelings; f != (struct feeling *)NULL; f = next_f)
	{
		next_f = f->next;
		free_feeling(f);
	}
#ifdef MALLOC_STATS
	num_feelings = feeling_size = 0;
#endif
	feelings = (struct feeling *)NULL;
#ifdef HASH_FEELINGS
	if (fhash != (struct hash *)NULL)
		free_hash(fhash);
	fhash = create_hash(0, "feelings", NULL, 0);
#endif
	if ((fp = fopen(F_FEELINGS, "r")) == (FILE *)NULL)
	{
		log_file("syslog", "Could not open feelings database.");
		return;
	}
	ff = &feelings;
	section = line = 0;
	while (fgets(buff, sizeof(buff), fp) != (char *)NULL)
	{
		line++;
		if ((buf = strchr(buff, '\n')) != (char *)NULL)
			*buf = '\0';
		else
		{
			int c;
			log_file("syslog",
			    "Line %d too long in feelings database.", line);
			while ((c = getc(fp)) != '\n' && c != EOF)
				;
			continue;
		}
		buf = buff;
		while (isspace(*buf) && *buf != '\0')
			buf++;
		if (ISCOMMENT(buf))
			continue;
		if (!strncmp(buf, "Section ", 8))
		{
			buf += 8;
			if (section)
			{
				log_file("syslog", "Unexpected 'Section' "
				    "directive in feelings database at "
				    "line %d.", line);
				continue;
			}
			if (!strcmp(buf, "std"))
				section = S_STD;
			else if (!strcmp(buf, "std2"))
				section = S_STD2;
			else if (!strcmp(buf, "no_arg"))
				section = S_NOARG;
			else if (!strcmp(buf, "no_targ"))
				section = S_NOTARG;
			else if (!strcmp(buf, "targ"))
				section = S_TARG;
			else if (!strcmp(buf, "opt_targ"))
				section = S_OPT_TARG;
			else
			{
				log_file("syslog",
				    "Unrecognised section name in feeling"
				    " database at line %d, %s.", line, buf);
				continue;
			}
		}
		else if (!strncmp(buf, "Endsection", 10))
		{
			if (!section)
				log_file("syslog",
				    "Unexpected 'Endsection' directive in "
				    "feeling database at line %d.", line);
			section = 0;
		}
		else if (!section)
		{
			log_file("syslog",
			    "Error in feeling database, line %d", line);
			continue;
		}
		else switch (section)
		{
		    case S_STD:
		    case S_STD2:
		    case S_NOARG:
		    case S_NOTARG:
		    case S_TARG:
		    case S_OPT_TARG:
		    {
		    	char name[BUFFER_SIZE], adv[BUFFER_SIZE],
			     prep[BUFFER_SIZE];
			f = create_feeling();
#ifdef MALLOC_STATS
			num_feelings++;
#endif
			if (sscanf(buf, "%[^:]::%[^\n]", name, prep) == 2)
			{
				COPY(f->prep, prep, "*feeling prep");
#ifdef MALLOC_STATS
				feeling_size += strlen(prep);
#endif
			}
			else if (sscanf(buf, "%[^:]:%[^:]:%[^\n]", name, adv,
			    prep) == 3)
			{
				if (strcmp(prep, ":"))
				{
					COPY(f->prep, prep, "*feeling prep");
#ifdef MALLOC_STATS
					feeling_size += strlen(prep);
#endif
				}
				COPY(f->adverb, adv, "*feeling adverb");
#ifdef MALLOC_STATS
				feeling_size += strlen(adv);
#endif
			}
			else if (sscanf(buf, "%[^:]:%[^\n]", name, adv) == 2)
			{
				COPY(f->adverb, adv, "*feeling adverb");
#ifdef MALLOC_STATS
				feeling_size += strlen(adv);
#endif
			}
			else
				strcpy(name, buf);
			for (q = name; *q != '\0'; q++)
			{
				if (isspace(*q))
				{
					log_file("syslog",
					    "Illegal space character in "
					    "feeling name, truncating. "
					    "(line %d)", line);
					*q = '\0';
					break;
				}
			}
			q = name + (f->no_verb = *name == '*');
			COPY(f->name, q, "*feeling name");
#ifdef MALLOC_STATS
			feeling_size += strlen(name);
#endif
			f->type = section;
			/* Add it to the database.. */
			*ff = f;
			ff = &f->next;
#ifdef HASH_FEELINGS
			/* Add it to the hash table */
			insert_hash(&fhash, (void *)f);
#endif
			break;
		    }
		    default:
			log_file("syslog",
			    "Feeling type %d not implemented at line %d.",
			    section, line);
			break;
		}
	}
	if (section)
		log_file("syslog",
		    "EOF while in section of feelings database.");
	fclose(fp);
}

/*
 * Attempt to pluralise a verb..
 */
char *
pluralise_verb(char *str)
{
	int len = strlen(str);
	static char foo[100];
	char a, b;

	if (strlen(str) > 90)
	{
		strcpy(foo, "Buffer overflow");
		return foo;
	}

	strcpy(foo, str);
	if (len < 2)
	{
		strcat(foo, "s");
		return foo;
	}
	a = str[len - 2];
	b = str[len - 1];

	if (a == 's' && b == 'h')
		strcat(foo, "es");
	else if (a == 'e' && b == 'y')
		strcat(foo, "s");
	else if (b == 'y')
	{
		foo[len - 1] = '\0';
		strcat(foo, "ies");
	}
	else if (a == 's' && b == 's')
		strcat(foo, "es");
	else if (a == 'c' && b == 'h')
		strcat(foo, "es");
	else
		strcat(foo, "s");
	return foo;
}

/* Valid escapes are:
	# - Literal '#'
	' - Literal ':'
	p - actor possessive
	P - target possessive
	o - actor objective
	O - target objective
	r - actor pronoun
	R - target pronoun.
	s - actor self reference.
	S - target self reference.
	I - Insert remaining text here.
	n - Newline
 */

static struct user *actor, *target;

static char *
parse_adverb(char *str, char *insert, int end)
{
	char adv[BUFFER_SIZE];
	static char adv2[BUFFER_SIZE];
	char *p;
	char dummy[] = "x";
	int inserted = 0;

	if (strlen(adv) > BUFFER_SIZE * 2 / 3)
	{
		strcpy(adv2, "Buffer overflow.\n");
		return adv2;
	}

	if (target == (struct user *)NULL)
		target = users;

	strcpy(adv, "");

	for (p = str; *p != '\0'; p++)
	{
		if (*p == '#')
		{
			switch (*++p)
			{
			    case '#':
				strcat(adv, "#");
				break;
			    case 'p':
				strcat(adv, query_gender(actor->gender,
				    G_POSSESSIVE));
				break;
			    case 'P':
				strcat(adv, query_gender(target->gender,
				    G_POSSESSIVE));
				break;
			    case 'o':
				strcat(adv, query_gender(actor->gender,
				    G_OBJECTIVE));
				break;
			    case 'O':
				strcat(adv, query_gender(target->gender,
				    G_OBJECTIVE));
				break;
			    case 'r':
				strcat(adv, query_gender(actor->gender,
				    G_PRONOUN));
				break;
			    case 'R':
				strcat(adv, query_gender(target->gender,
				    G_PRONOUN));
				break;
			    case 's':
				strcat(adv, query_gender(actor->gender,
				    G_SELF));
				break;
			    case 'S':
				strcat(adv, query_gender(target->gender,
				    G_SELF));
				break;
			    case 'i':
			    case 'I':
				strcat(adv, insert);
				inserted = 1;
				break;
			    case '\'':
				strcat(adv, ":");
				break;
			    case 'n':
				strcat(adv, "\n");
				break;
			    default:
				strcat(adv, "<UNKNOWN ESCAPE>");
			}
		}
		else
		{
			*dummy = *p;
			strcat(adv, dummy);
		}
	}
	if (!inserted && insert != (char *)NULL && strlen(insert))
	{
		if (end)
			sprintf(adv2, "%s %s", adv, insert);
		else
			sprintf(adv2, "%s %s", insert, adv);
	}
	else
		strcpy(adv2, adv);

	while ((p = strstr(adv2, "  ")) != (char *)NULL)
		strcpy(p, p + 1);

	p = adv2 + strlen(adv2) - 1;
	if (*p != '!' && *p != '?' && *p != ')')
		strcat(adv2, ".");

	return adv2;
}

static char *
feeling_user_list(struct user *p, char *id, int lcl)
{
	struct user *who;
	struct vecbuf vb;
	struct vector *vec;
	struct strbuf buff;
	static char *str = (char *)NULL;
	char *tmp;
	int i;

	FREE(str);

	init_strbuf(&buff, 0, "feeling_user_list");

	target = users;
	users->gender = G_PLURAL;

	/* Special case.. */
	if (!strcmp(id, "all"))
	{
		struct object *ob;

		init_composite_words(&buff);

		if (lcl)
			for (ob = p->super->ob->contains;
			    ob != (struct object *)NULL;
			    ob = ob->next_contains)
			{
				if (ob->type != OT_USER ||
				    !IN_GAME(ob->m.user))
					continue;
				if (ob->m.user == p)
					continue;
				add_composite_word(&buff, ob->m.user->name);
			}
		else
			for (who = users->next; who != (struct user *)NULL;
			    who = who->next)
			{
				if (!IN_GAME(who))
					continue;
				if (who == p)
					continue;
				if (who->super != p->super && !CAN_SEE(p, who))
					continue;
				add_composite_word(&buff, who->name);
			}

		end_composite_words(&buff);

		if (!buff.offset)
		{
			free_strbuf(&buff);
			return (char *)NULL;
		}
		pop_strbuf(&buff);
		return str = buff.str;
	}

	/* Not /All/ */

	tmp = string_copy(id, "feeling_user_list idcp");
	init_vecbuf(&vb, 0, "feeling_user_list vb");
	/* NB: Not verbose.. */
	expand_user_list(p, tmp, &vb, 1, lcl, 0);
	vec = vecbuf_vector(&vb);
	xfree(tmp);

	if (!vec->size)
	{
		free_vector(vec);
		/* Do some checks of our own. */
		if (lcl)
			who = find_user_common(p, id, &buff);
		else
			who = with_user_common(p, id, &buff);

		if (who == (struct user *)1)
		{
			write_socket(p, "Ambiguous username, %s [%s]\n",
			    id, buff.str);
			free_strbuf(&buff);
			return (char *)1;
		}
		free_strbuf(&buff);
		return (char *)NULL;
	}

	init_composite_words(&buff);

	for (i = 0; i < vec->size; i++)
	{
		if (vec->items[i].type != T_POINTER)
			continue;

		if ((struct user *)vec->items[i].u.pointer == p)
			add_composite_word(&buff,
			    query_gender(p->gender, G_SELF));
		else
			add_composite_word(&buff,
			    ((struct user *)(vec->items[i].u.pointer))->name);
		/* Doesn't mean much for multiple targets but... */
		target = (struct user *)vec->items[i].u.pointer;
	}
	end_composite_words(&buff);

	free_vector(vec);

	if (!buff.offset)
	{
		free_strbuf(&buff);
		return (char *)NULL;
	}
	pop_strbuf(&buff);
	return str = buff.str;
}

/*
 * There are three cases.
 * str is empty, str contains a name at the end, and str is only text.
 * noverb feelings are treated normally here.
 */
int
std_feeling(struct user *p, int argc, char **argv, struct feeling *f,
    struct strbuf *str, int lcl)
{
	char *lword, *rest = (char *)NULL;
	char *nm;

	if (argc == 1)
	{
		if (f->adverb != (char *)NULL)
			if (f->no_verb)
				sadd_strbuf(str, "%s %s", p->name,
			    	    parse_adverb(f->adverb, "", 0));
			else
				sadd_strbuf(str, "%s %s %s", p->name,
			    	    pluralise_verb(argv[0]),
			    	    parse_adverb(f->adverb, "", 0));
		else
			sadd_strbuf(str, "%s %s.", p->name,
			    pluralise_verb(argv[0]));
		return 1;
	}
	if ((lword = strrchr(argv[1], ' ')) != (char *)NULL)
	{		
		*lword++ = '\0';
		rest = argv[1];
	}
	else
		lword = argv[1];
	if ((nm = feeling_user_list(p, lword, lcl)) != (char *)NULL)
	{
		if (nm == (char *)1)
			return 0;
		if (rest != (char *)NULL)
			if (f->no_verb)
				sadd_strbuf(str, "%s %s %s",
			    	    p->name, rest,
			    	    f->prep == (char *)NULL ? 
			    	    parse_adverb("at", nm, 1) :
			    	    parse_adverb(f->prep, nm, 1));
			else
				sadd_strbuf(str, "%s %s %s %s", p->name,
				    pluralise_verb(argv[0]), rest,
			    	    f->prep == (char *)NULL ? 
			    	    parse_adverb("at", nm, 1) :
			    	    parse_adverb(f->prep, nm, 1));
		else
			if (f->no_verb)
				sadd_strbuf(str, "%s %s", p->name,
				    f->prep == (char *)NULL ?
			    	    parse_adverb("at", nm, 1) :
			    	    parse_adverb(f->prep, nm, 1));
			else
				sadd_strbuf(str, "%s %s %s", p->name,
				    pluralise_verb(argv[0]),
				    f->prep == (char *)NULL ?
			    	    parse_adverb("at", nm, 1) :
			    	    parse_adverb(f->prep, nm, 1));
		return 1;
	}
	if (rest != (char *)NULL)
		sadd_strbuf(str, "%s %s %s %s.", p->name,
		    pluralise_verb(argv[0]), rest, lword);
	else
		sadd_strbuf(str, "%s %s %s.", p->name,
		    pluralise_verb(argv[0]), lword);
	return 1;
}

/* There is only one valid case.
 * str contains a name at the start */
int
std2_feeling(struct user *p, int argc, char **argv, struct feeling *f,
    struct strbuf *str, int lcl)
{
	char *rest = (char *)NULL;
	char *nm;
	
	if (argc == 1)
	{
		write_socket(p, "%s who ?\n", argv[0]);
		return 0;
	}
	if ((rest = strchr(argv[1], ' ')) != (char *)NULL)
		*rest++ = '\0';
	if ((nm = feeling_user_list(p, argv[1], lcl)) == (char *)NULL)
	{
		write_socket(p, "%s who ?\n", argv[0]);
		return 0;
	}
	if (nm == (char *)1)
		return 0;
	if (rest != (char *)NULL)
		sadd_strbuf(str, "%s %s %s %s.", p->name,
		    pluralise_verb(argv[0]), nm, rest);
	else if (f->adverb != (char *)NULL)
		sadd_strbuf(str, "%s %s %s %s", p->name,
		    pluralise_verb(argv[0]), nm,
		    parse_adverb(f->adverb, "", 0));
	else if (f->prep != (char *)NULL && f->no_verb)
		sadd_strbuf(str, "%s %s", p->name,
		    parse_adverb(f->prep, nm, 1));
	else
		sadd_strbuf(str, "%s %s %s.", p->name,
		    pluralise_verb(argv[0]), nm);
	return 1;
}

int
no_arg_feeling(struct user *p, int argc, char **argv, struct feeling *f,
    struct strbuf *str)
{
	if (argc != 1)
	{
		write_socket(p, "%s what ?\n", argv[0]);
		return 0;
	}
	if (f->adverb != (char *)NULL)
		if (f->no_verb)
			sadd_strbuf(str, "%s %s", p->name,
		    	    parse_adverb(f->adverb, "", 0));
		else
			sadd_strbuf(str, "%s %s %s", p->name,
		    	    pluralise_verb(argv[0]),
			    parse_adverb(f->adverb, "", 0));
	else
		sadd_strbuf(str, "%s %s.", p->name,
		    pluralise_verb(argv[0]));
	return 1;
}

int
no_targ_feeling(struct user *p, int argc, char **argv, struct feeling *f,
    struct strbuf *str)
{
	if (argc == 1)
	{
		if (f->adverb != (char *)NULL)
			if (f->no_verb)
				sadd_strbuf(str, "%s %s",
				    p->name,
			    	    parse_adverb(f->adverb, "", 0));
			else
				sadd_strbuf(str, "%s %s %s",
				    p->name,
			    	    pluralise_verb(argv[0]),
			    	    parse_adverb(f->adverb, "", 0));
		else
			sadd_strbuf(str, "%s %s.", p->name,
			    pluralise_verb(argv[0]));
		return 1;
	}
	if (f->no_verb)
		sadd_strbuf(str, "%s %s", p->name,
		    f->prep == (char *)NULL ?
		    parse_adverb(argv[1], "", 1) :
	    	    parse_adverb(f->prep, argv[1], 1));
	else
		sadd_strbuf(str, "%s %s %s", p->name,
		    pluralise_verb(argv[0]), f->prep == (char *)NULL ?
		    parse_adverb(argv[1], "", 1) :
	    	    parse_adverb(f->prep, argv[1], 1));
	return 1;
}

int
targ_feeling(struct user *p, int argc, char **argv, struct feeling *f,
    struct strbuf *str, int lcl)
{
	char *nm;

	if (argc == 1)
	{
		write_socket(p, "%s who ?\n", argv[0]);
		return 0;
	}
	if ((nm = feeling_user_list(p, argv[1], lcl)) == (char *)NULL)
	{
		write_socket(p, "%s is not here.\n", capitalise(argv[1]));
		return 0;
	}
	if (nm == (char *)1)
		return 0;
	if (f->prep != (char *)NULL)
		if (f->no_verb)
			sadd_strbuf(str, "%s %s", p->name,
		    	    parse_adverb(f->prep, nm, 1));
		else
			sadd_strbuf(str, "%s %s %s", p->name,
		    	    pluralise_verb(argv[0]),
			    parse_adverb(f->prep, nm, 1));
	else
		sadd_strbuf(str, "%s %s %s.", p->name,
		    pluralise_verb(argv[0]), nm);
	return 1;
}

int
opt_targ_feeling(struct user *p, int argc, char **argv, struct feeling *f,
    struct strbuf *str, int lcl)
{
	char *nm;
	
	if (argc == 1)
	{
		if (f->adverb != (char *)NULL)
			if (f->no_verb)
				sadd_strbuf(str, "%s %s",
				    p->name,
			    	    parse_adverb(f->adverb, "", 0));
			else
				sadd_strbuf(str, "%s %s %s",
				    p->name,
			    	    pluralise_verb(argv[0]),
			    	    parse_adverb(f->adverb, "", 0));
		else
			sadd_strbuf(str, "%s %s.", p->name,
			    pluralise_verb(argv[0]));
		return 1;
	}
	if ((nm = feeling_user_list(p, argv[1], lcl)) == (char *)NULL)
	{
		write_socket(p, "%s is not here.\n", capitalise(argv[1]));
		return 0;
	}
	if (nm == (char *)1)
		return 0;
	if (f->no_verb)
		sadd_strbuf(str, "%s %s", p->name, f->prep ==
		    (char *)NULL ? parse_adverb("at", nm, 1) :
		    parse_adverb(f->prep, nm, 1));
	else
		sadd_strbuf(str, "%s %s %s", p->name,
		    pluralise_verb(argv[0]), f->prep == (char *)NULL ?
		    parse_adverb("at", nm, 1) :
		    parse_adverb(f->prep, nm, 1));
	return 1;
}

char *
expand_feeling(struct user *p, int argc, char **argv, int lcl)
{
	struct strbuf str;
	struct feeling *f;
	int ret;

	FUN_START("expand_feeling");

	actor = p;
	target = (struct user *)NULL;

#ifdef HASH_FEELINGS
	if ((f = (struct feeling *)lookup_hash(fhash, argv[0])) ==
	    (struct feeling *)NULL)
	{
		FUN_END;
		return (char *)NULL;
	}
#else
	for (f = feelings; f != (struct feeling *)NULL; f = f->next)
		if (!strcmp(argv[0], f->name))
			break;
	if (f == (struct feeling *)NULL)
	{
		FUN_END;
		return (char *)NULL;
	}
#endif /* HASH_FEELINGS */

#ifdef PROFILING
	f->calls++;
#endif
	init_strbuf(&str, 0, "feeling strbuf");
	switch (f->type)
	{
	    case S_STD:
		ret = std_feeling(p, argc, argv, f, &str, lcl);
		break;
	    case S_STD2:
		ret = std2_feeling(p, argc, argv, f, &str, lcl);
		break;
	    case S_NOARG:
		ret = no_arg_feeling(p, argc, argv, f, &str);
		break;
	    case S_NOTARG:
		ret = no_targ_feeling(p, argc, argv, f, &str);
		break;
	    case S_TARG:
		ret = targ_feeling(p, argc, argv, f, &str, lcl);
		break;
	    case S_OPT_TARG:
		ret = opt_targ_feeling(p, argc, argv, f, &str, lcl);
		break;
	    default:
		log_file("error", "Feeling type %d not yet implemented.",
		    f->type);
		ret = 0;
		break;
	}
	FUN_END;
	if (ret)
	{
		pop_strbuf(&str);
		return str.str;
	}
	free_strbuf(&str);
	/* Ugh. */
	return (char *)1;
}

int
parse_feeling(struct user *p, int argc, char **argv)
{
	char *str;

	FUN_START("parse_feeling");

	if ((str = expand_feeling(p, argc, argv, 1)) != (char *)NULL)
	{
		if (str != (char *)1)
		{
			write_room_wrt_uattr(p, "%s\n", str);
			xfree(str);
		}
		FUN_END;
		return 1;
	}
	FUN_END;
	return 0;
}

#ifdef HASH_FEELINGS
void
feeling_hash_stats(struct user *p, int verbose)
{
	if (fhash == (struct hash *)NULL)
	{
		write_socket(p, "Feelings hash table is empty.\n");
		return;
	}
	hash_stats(p, fhash, verbose);
}
#endif

