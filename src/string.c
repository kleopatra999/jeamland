/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	string.c
 * Function:	Functions which manipulate strings
 *		The strbuf datatype
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <netinet/in.h>
#include "jeamland.h"

extern time_t current_time;

/* strncpy() varies between different unix platforms, not all implementations
 * terminate the string with a null character; my version does.
 */
void
my_strncpy(char *dest, char *src, int n)
{
	while (n-- && *src != '\0')
		*dest++ = *src++;
	*dest = '\0';
}

void
deltrail(char *p)
{
	while (!isspace(*p) && *p != '\n' && *p != '\0')
		p++;
	*p = '\0';
}

void
delheadtrail(char **p)
{
	while (isspace(**p))
		(*p)++;
	deltrail(*p);
}

char *
nctime(time_t *tm)
{
	static char tmbuf[25];

	strcpy(tmbuf, ctime(tm));
	*strchr(tmbuf, '\n') = '\0';
	return tmbuf;
}

char *
string_copy(char *str, char *ident)
{
	char *p = (char *)xalloc(strlen(str) + 1, ident);
	strcpy(p, str);
	return p;
}

#define SBUFFERS	10
#define SBUFF_SIZE	0x100

static char *
get_sbuffer()
{
	static char sbuffers[SBUFFERS][SBUFF_SIZE];
	static int sbuffer_ptr = 0;

	return sbuffers[sbuffer_ptr++ % SBUFFERS];
}

char *
capitalise(char *str)
{
	char *buf = get_sbuffer();

	strcpy(buf, str);
	*buf = toupper(*buf);
	return buf;
}

char *
lower_case(char *str)
{
	char *buf = get_sbuffer();
	char *p;

	strcpy(buf, str);
	for (p = buf; *p != '\0'; p++)
		*p = tolower(*p);
	return buf;
}

char *
capfirst(char *str)
{
	char *buf = get_sbuffer();
	char *p;

	strcpy(buf, str);
	for (p = buf; *p != '\0'; p++)
		*p = tolower(*p);
	if (*buf != '\0')
		*buf = toupper(*buf);
	return buf;
}

char *
conv_time(time_t t)
{
	time_t i;
	int flag = 0;
	static char buf[50];
	char tmp[20];
	strcpy(buf, "");

	if (!t)
	{
		strcpy(buf, "no time");
		return buf;
	}
	if ((i = t / 86400))
	{
		sprintf(tmp, "%ld day%s", (long)i, i == 1 ? "" : "s");
		strcat(buf, tmp);
		t = t % 86400;
		flag++;
	}
	if ((i = t / 3600))
	{
		if (flag)
			strcat(buf, ", ");
		sprintf(tmp, "%ld hour%s", (long)i, i == 1 ? "" : "s");
		strcat(buf, tmp);
		t = t % 3600;
		flag++;
	}
	if ((i = t / 60))
	{
		if (flag)
			strcat(buf, ", ");
		sprintf(tmp, "%ld minute%s", (long)i, i == 1 ? "" : "s");
		strcat(buf, tmp);
		t = t % 60;
		flag++;
	}
	if (t)
	{
		if (flag)
			strcat(buf, ", ");
		sprintf(tmp, "%ld second%s", (long)t, t == (time_t)1 ?
		    "" : "s");
		strcat(buf, tmp);
	}
	return buf;
}

#ifdef STRBUF_STATS
/* Basic statistics on string buffers.. largely for interest.. can be undefined
 * if required.
 */
static int strbuf_total = 0;		/* Total string buffers */
static int strbuf_current = 0;		/* ^ in memory now */
static int strbuf_no_expand = 0;	/* How many didn't need expanding */
#define STRBUF_STATS_LOTS 25
static int strbuf_lots_expand = 0;	/* How many needed expanding a lot */

void
strbuf_stats(struct user *p)
{
	write_socket(p, "String Buffer Statistics.\n");
	write_socket(p, "-------------------------\n");
	write_socket(p, "Buffers processed:               %6d\n", strbuf_total);
	write_socket(p, "Current buffers:                 %6d\n\n",
	    strbuf_current);
	write_socket(p, "Buffers which needed:\n");
	write_socket(p, "\tNo expansion:            %6d\n", strbuf_no_expand);
	write_socket(p, "\tSome expansion:          %6d\n",
	    strbuf_total - strbuf_no_expand - strbuf_lots_expand);
	write_socket(p, "\tLots of expansion (>%2d): %6d\n",
	    STRBUF_STATS_LOTS, strbuf_lots_expand);
}
#endif

void
init_strbuf(struct strbuf *buf, int len, char *id)
{
	if (!len)
		len = STRBUF_CHUNK;

	buf->offset = 0;
	buf->len = len;
	buf->str = (char *)xalloc(len + 1, id);
	buf->str[0] = '\0';
#ifdef STRBUF_STATS
	buf->xpnd = 0;
	strbuf_total++, strbuf_current++;
#endif
}

void
reinit_strbuf(struct strbuf *buf)
{
	buf->offset = 0;
	buf->str[0] = '\0';
}

#ifdef STRBUF_STATS
void
pop_strbuf(struct strbuf *buf)
{
	buf->offset = buf->len = 0;
	strbuf_current--;
	if (!buf->xpnd)
		strbuf_no_expand++;
	else if (buf->xpnd > STRBUF_STATS_LOTS)
		strbuf_lots_expand++;
}
#endif

void
free_strbuf(struct strbuf *buf)
{
	pop_strbuf(buf);
	xfree(buf->str);
}

void
expand_strbuf(struct strbuf *buf, int len)
{
	buf->len = buf->offset + len + STRBUF_CHUNK;
	buf->str = (char *)xrealloc(buf->str, buf->len + 1);
#ifdef STRBUF_STATS
	buf->xpnd++;
#endif
}

void
shrink_strbuf(struct strbuf *buf, int len)
{
	if (buf->offset >= len - 1)
		return;	/* No can do.. */
	buf->len = len;
	buf->str = (char *)xrealloc(buf->str, len + 1);
}

void
add_strbuf(struct strbuf *buf, char *str)
{
	int len = strlen(str);

	if (buf->offset + len > buf->len)
		expand_strbuf(buf, len);
	strcat(buf->str, str);
	buf->offset += len;
}

void
cadd_strbuf(struct strbuf *buf, char ch)
{
	if (buf->offset >= buf->len)
		expand_strbuf(buf, 1);
	buf->str[buf->offset++] = ch;
	buf->str[buf->offset] = '\0';
}

void
sadd_strbuf(struct strbuf *sbuf, char *fmt, ...)
{
	/* Static for speed */
	static char buf[BUFFER_SIZE];
	va_list argp;

#ifdef DEBUG
	if (strlen(fmt) > sizeof(buf))
		fatal("sadd_strbuf: sbuf too long!");
#endif

	va_start(argp, fmt);
	vsprintf(buf, fmt, argp);
	va_end(argp);

	add_strbuf(sbuf, buf);
}

void
val2str(struct strbuf *buf, struct svalue *val)
{
	switch(val->type)
	{
	    case T_EMPTY:
		add_strbuf(buf, "!!EMPTY!!");
		break;

	    case T_STRING:
	    {
		char *str = code_string(val->u.string);
		add_strbuf(buf, "\"");
		add_strbuf(buf, str);
		add_strbuf(buf, "\"");
		xfree(str);
		break;
	    }

	    case T_NUMBER:
		sadd_strbuf(buf, "%ld", val->u.number);
		break;

	    case T_VECTOR:
	    {
		int i;

		add_strbuf(buf, "({");
		for (i = 0; i < val->u.vec->size; i++)
		{
			val2str(buf, &val->u.vec->items[i]);
			add_strbuf(buf, ",");
		}
		add_strbuf(buf, "})");
		break;
	    }

	    case T_POINTER:
		sadd_strbuf(buf, "%p", (void *)val->u.pointer);
		break;

	    case T_FPOINTER:
		add_strbuf(buf, "!!Cannot represent function pointers!!");
		break;

	    default:
		fatal("Invalid value of svalue in val2str, %d", val->type);
	}
}

char *
svalue_to_string(struct svalue *val)
{
	struct strbuf buf;

	init_strbuf(&buf, 0, "svalue_to_string");

	val2str(&buf, val);
	pop_strbuf(&buf);
	return buf.str;
}

static int str2val(struct svalue *, char **, char *);

struct vector *
decode_vector(char **str, char *id)
{
	struct vecbuf v;
    	int i;

	init_vecbuf(&v, 0, id);

    	for(;;)
    	{
		if (**str == '}')
		{
	    		if (*++*str == ')')
			{
				++*str;
				return vecbuf_vector(&v);
			}
			else
				break;
		}
		else
		{
			i = vecbuf_index(&v);
	    		if (!str2val(&v.vec->items[i], str, id))
				break;
	    		if (*(*str)++ != ',')
				break;
		}
    	}
	free_vecbuf(&v);
    	return 0;
}

static int
str2val(struct svalue *v, char **sp, char *id)
{
    	char *q, *p, *s;

    	s = *sp;
    	switch(*s)
	{
    	    case '(':
		switch(*++s)
		{
		    case '{':
	    	    {
			struct vector *vec;
			s++;
			vec = decode_vector(&s, id);
			if (vec == (struct vector *)NULL)
				return 0;
			free_svalue(v);
			v->type = T_VECTOR;
			v->u.vec = vec;
	    	 	break;
	    	    }
	   	    default:
	    		return 0;
		}
		break;
    	    case '"':
		for(p = s + 1, q = s; *p != '\0' && *p != '"'; p++) 
		{
	    		if (*p == '\\')
			{
			    	switch (*++p)
			    	{
				    case 'n':
		    			*q++ = '\n';
		    			break;
				    default:
		    			*q++ = *p;
		    			break;
				}
	    		}
			else
		    		*q++ = *p;
	    	}
		*q = 0;
		if (*p != '"')
            		return 0;
		free_svalue(v);
		v->type = T_STRING;
		v->u.string = string_copy(s, id);
		s = p + 1;
		break;
    	    default:
		if (!isdigit(*s) && *s != '-')
	    		return 0;
		free_svalue(v);
		v->type = T_NUMBER;
		v->u.number = atol(s);
		while(isdigit(*s) || *s == '-')
	    	    s++;
		break;
    	}
    	*sp = s;
    	return 1;
}

struct svalue *
string_to_svalue(char *str, char *id)
{
	static struct svalue val;

	if (!str2val(&val, &str, id))
		return (struct svalue *)NULL;
	return &val;
}

struct svalue *
decode_one(char *str, char *id)
{
	char *p;

	/* First remove the variable name. */
	if ((p = strchr(str, ' ')) == (char *)NULL || *++p == '\0')
		return (struct svalue *)NULL;
	return string_to_svalue(p, id);
}

char *
decode_string(char *str, char *id)
{
	struct svalue *sv;

	if ((sv = decode_one(str, id)) == (struct svalue *)NULL)
		return (char *)NULL;
	if (sv->type != T_STRING)
	{
		free_svalue(sv);
		return (char *)NULL;
	}
	/* To avoid freeing on next round... */
	sv->type = T_EMPTY;
	return sv->u.string;
}

char *
code_string(char *str)
{
	char *p;
	struct strbuf buf;

	if (str == (char *)NULL)
		return string_copy("", "code_string dummy");

	/* Good estimate to avoid need for a realloc */
	init_strbuf(&buf, strlen(str) + STRBUF_CHUNK, "code_string");

	for (p = str; *p != '\0'; p++)
	{
		switch(*p)
		{
		    case '"':
			add_strbuf(&buf, "\\\"");
			break;
		    case '\\':
			add_strbuf(&buf, "\\\\");
			break;
		    case '\n':
			add_strbuf(&buf, "\\n");
			break;
		    default:
			cadd_strbuf(&buf, *p);
			break;
		}
	}
	pop_strbuf(&buf);
	return buf.str;
}

struct vector *
parse_range(struct user *p, char *str, int max)
{
	char *r;
	struct vecbuf v;
	int from, to, i;

	init_vecbuf(&v, 0, "parse_range");
	if (str == (char *)NULL || !strlen(str))
		return vecbuf_vector(&v);
	r = strtok(str, ",");

	do
	{
		switch(sscanf(str, "%d - %d", &from, &to))
		{
		    case 1:
		    	if (str[strlen(str) - 1] == '-')
				to = max;
			else
			{
				i = vecbuf_index(&v);
				v.vec->items[i].type = T_NUMBER;
				v.vec->items[i].u.number = from;
				break;
			}
			/* Fallthrough */
		    case 2:
			if (to <= from)
				write_socket(p, "Bad range: %s\n", str);
			else
			{
				for ( ; from <= to; from++)
				{
					i = vecbuf_index(&v);
					v.vec->items[i].type = T_NUMBER;
					v.vec->items[i].u.number = from;
				}
			}
			break;
		    default:
			break;
		}
	} while ((str = strtok((char *)NULL, ",")) != (char *)NULL);
	return vecbuf_vector(&v);
}

/*
 * Function to expand a list of users into a vecbuf.
 * If the 'on' parameter is true, users who are currently logged on are
 * inserted as 'pointers' and omitted if not logged on. Otherwise, all
 * entries become strings.
 * This function destroys the 'str' argument.
 */
#define ADD_STRING(arg, desc) \
			i = vecbuf_index(v); \
			v->vec->items[i].type = T_STRING; \
			v->vec->items[i].u.string = string_copy(arg, desc)
#define DUPL_STRING(str) \
			sv.type = T_STRING; \
			sv.u.string = str; \
			if (member_vector(&sv, v->vec) != -1) \
				continue
#define ADD_POINTER(arg) \
			i = vecbuf_index(v); \
			v->vec->items[i].type = T_POINTER; \
			v->vec->items[i].u.pointer = (void *)arg
#define DUPL_POINTER(ptr) \
			sv.type = T_POINTER; \
			sv.u.pointer = ptr; \
			if (member_vector(&sv, v->vec) != -1) \
				continue

void
expand_user_list(struct user *p, char *str, struct vecbuf *v, int on)
{
	struct user *who;
	struct svalue sv;
	int i;
	char *c;

	if ((c = strtok(str, ",")) == (char *)NULL)
		return;
	do
	{
		delheadtrail(&c);

#ifdef UDP_SUPPORT
		if (p->level > L_VISITOR && strchr(c, '@') != (char *)NULL)
		{
			ADD_STRING(c, "expand_user_list strel");
			continue;
		}
#endif
		/* Handle grupes */
		if (*c == '#')
		{
			int j;
			struct vector *vec;
			struct vecbuf grupe;

			init_vecbuf(&grupe, 0, "f_tell grupe");
			expand_grupe(p, &grupe, c + 1);
			vec = vecbuf_vector(&grupe);
			for (j = vec->size; j--; )
			{
				if (vec->items[j].type != T_STRING)
					continue;
#ifdef UDP_SUPPORT
				if (p->level > L_VISITOR &&
				    strchr(vec->items[j].u.string,
				    '@') != (char *)NULL)
				{
					DUPL_STRING(vec->items[j].u.string);
					ADD_STRING(vec->items[j].u.string,
					    "expand_user_list strel");
					continue;
				}
#endif
				if (on)
				{
					if ((who = find_user(p,
					    vec->items[j].u.string)) ==
					    (struct user *)NULL)
						continue;
					DUPL_POINTER(who);
					ADD_POINTER(who);
				}
				else
				{
					DUPL_STRING(vec->items[j].u.string);
					ADD_STRING(vec->items[j].u.string,
					    "expand_user_list grpel");
				}
			}
			free_vector(vec);
		}
		else
		{
			/* Exclusion */
			if (c[0] == '!')
			{
				int k;

				for (k = v->vec->size; k--; )
					if (v->vec->items[k].type ==
					    T_STRING &&
					    !strcmp(v->vec->items[k].u.string,
					    c + 1))
					{
						xfree(v->vec->items[k].
						    u.string);
						v->vec->items[k].type = T_EMPTY;
					}
				continue;
			}

			if (on)
			{
				if ((who = find_user(p, c)) ==
				    (struct user *)NULL)
					continue;
				DUPL_POINTER(who);
				ADD_POINTER(who);
			}
			else
			{
				DUPL_STRING(c);
				ADD_STRING(c, "expand_user_list nmlel");
			}
		}
	} while ((c = strtok((char *)NULL, ",")) != (char *)NULL);
}

char *
parse_chevron(char *s)
{
	struct strbuf buf;

	/* Save CPU in simple case.. */
	if (strchr(s, '^') == (char *)NULL)
		return string_copy(s, "parse_chevron");

	init_strbuf(&buf, 0, "parse_chevron");
	while (*s != '\0')
	{
		switch (*s)
		{
		    case '\\':
			if (s[1] != '\0')
				cadd_strbuf(&buf, *++s);
			break;
		    case '^':
			if (s[1] != '\0')
				cadd_strbuf(&buf, *++s & 31);
			break;
		    default:
			cadd_strbuf(&buf, *s);
			break;
		}
		s++;
	}
	pop_strbuf(&buf);
	return buf.str;
}

int
exist_cookie(char *s, char c)
{
	/* Save CPU in simple case.. */
	if (strchr(s, '%') == (char *)NULL)
		return 0;

	while (*s != '\0')
	{
		if (*s == '\\')
			s++;
		else if (*s == '%' && *++s == c)
			return 1;
		s++;
	}
	return 0;
}

char *
parse_cookie(struct user *p, char *s)
{
	struct strbuf buf;

	/* Save CPU in simple case.. */
	if (strchr(s, '%') == (char *)NULL)
		return string_copy(s, "parse_cookie");

	init_strbuf(&buf, 0, "parse_cookie");
	while (*s != '\0')
	{
		switch (*s)
		{
		    case '\\':
			if (s[1] != '\0')
				cadd_strbuf(&buf, *++s);
			break;
		    case '%':
			if (s[1] != '\0')
				switch (*++s)
				{
				    case '!':	/* Command number */
					sadd_strbuf(&buf, "%d",
					    p->history_ptr);
					break;
				    case 'b':	/* Newline */
					cadd_strbuf(&buf, '\n');
					break;
				    case 'i':	/* Invis '*' */
					add_strbuf(&buf,
					    p->saveflags & U_INVIS ? "*" : " ");
					break;
				    case 'n':	/* Talker name */
					add_strbuf(&buf, LOCAL_NAME);
					break;
				    case 'N':	/* Your name */
					add_strbuf(&buf, p->capname);
					break;
				    case 'p':	/* Prompt character */
					cadd_strbuf(&buf, p->saveflags &
					    U_CONVERSE_MODE ? ']' : '>');
					break;
				    case 'r':	/* Room name */
					if (p->super->name != (char *)NULL)
						add_strbuf(&buf,
						    p->super->name);
					else
						add_strbuf(&buf, "outside");
					break;
				    case 'R':	/* Room fname */
					if (p->level < A_SEE_EXINFO)
						cadd_strbuf(&buf, ' ');
					else
						add_strbuf(&buf,
						    p->super->fname);
					break;
				    case 't':	/* Time */
				    {
					char *t = nctime(&current_time) + 11;
					t[5] = '\0';
					add_strbuf(&buf, t);
					break;
				    }
				    default:
#ifdef IGNORE_UNKNOWN_COOKIES
					cadd_strbuf(&buf, '%');
					s--;
#else
					sadd_strbuf(&buf,
					    "<UNKNOWN TOKEN, %c>", *s);
#endif
					break;
				}
			break;
		    default:
			cadd_strbuf(&buf, *s);
			break;
		}
		s++;
	}
	pop_strbuf(&buf);
	return buf.str;
}

char *
parse_chevron_cookie(struct user *p, char *s)
{
	char *chevs = parse_chevron(s);
	char *cooks = parse_cookie(p, chevs);

	xfree(chevs);
	return cooks;
}

int
valid_name(struct user *p, char *name, int room)
{
	char *q;

        if ((tolower(*name) < 'a' || tolower(*name) > 'z') &&
	    (!room || *name != '_'))
        {
                write_socket(p, "First character of name must be in the "
                    "range a-z%s\n", room ? " or _." : ".");
                return 0;
        }
        for (q = name; *q != '\0'; q++)
        {
                if (*q == ' ')
                        *q = '_';
                else if (!isprint(*q) || (!room && *q == '#'))
                {
                        write_socket(p, "Invalid character (%d).\n", q -
			    name + 1);
                        return 0;
                }
        }
	return 1;
}

struct strlist *
create_strlist(char *id)
{
	struct strlist *s = (struct strlist *)xalloc(sizeof(struct strlist),
	    id);
	s->str = (char *)NULL;
	s->next = (struct strlist *)NULL;
	return s;
}

void
free_strlist(struct strlist *s)
{
	FREE(s->str);
	xfree(s);
}

void
free_strlists(struct strlist **list)
{
	struct strlist *s, *t;

	for (s = *list; s != (struct strlist *)NULL; s = t)
	{
		t = s->next;
		free_strlist(s);
	}
	*list = (struct strlist *)NULL;
}

void
add_strlist(struct strlist **list, struct strlist *s, enum sl_mode mode)
{
	switch (mode)
	{
	    case SL_HEAD:	/* Add to start of list */
		s->next = *list;
		*list = s;
		break;

	    case SL_TAIL:	/* Add to end of list */
		while (*list != (struct strlist *)NULL)
			list = &((*list)->next);
		*list = s;
		break;

	    case SL_SORT:	/* Add alphabetically to list */
		for (; *list != (struct strlist *)NULL;
		    list = &((*list)->next))
			if (strcmp((*list)->str, s->str) > 0)
				break;
		s->next = *list;
		*list = s;
		break;

	    default:
		fatal("add_strlist: Unknown mode.");
	}
}

void
remove_strlist(struct strlist **list, struct strlist *s)
{
	for (; *list != (struct strlist *)NULL; list = &((*list)->next))
		if (*list == s)
		{
			*list = s->next;
			free_strlist(s);
			return;
		}
#ifdef DEBUG
	fatal("remove_strlist: list element not found.");
#endif
}

struct strlist *
member_strlist(struct strlist *s, char *str)
{
	for (; s != (struct strlist *)NULL; s = s->next)
		if (!strcmp(s->str, str))
			break;
	return s;
}

void
write_strlist(FILE *fp, char *id, struct strlist *s)
{
	if (s == (struct strlist *)NULL)
		return;

	fprintf(fp, "%s ({", id);

	for (; s != (struct strlist *)NULL; s = s->next)
		fprintf(fp, "\"%s\",", s->str);
	fprintf(fp, "})\n");
}

struct strlist *
decode_strlist(char *str, enum sl_mode mode)
{
	struct strlist *s, *list = (struct strlist *)NULL;
	struct svalue *sv;
	int i;

	if ((sv = decode_one(str, "decode_strlist sv")) ==
	    (struct svalue *)NULL)
		return (struct strlist *)NULL;

	if (sv->type != T_VECTOR)
	{
		free_svalue(sv);
		return (struct strlist *)NULL;
	}

	for (i = 0; i < sv->u.vec->size; i++)
	{
		if (sv->u.vec->items[i].type != T_STRING)
			continue;
		s = create_strlist("decode_strlist");
		s->str = sv->u.vec->items[i].u.string;
		/* Just copy the pointer; don't want to waste time copying
		 * it. To stop free_vector from freeing the string, change
		 * the type to empty
		 */
		TO_EMPTY(sv->u.vec->items[i]);
		add_strlist(&list, s, mode);
	}

	free_svalue(sv);
	return list;
}

