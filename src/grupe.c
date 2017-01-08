/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	grupe.c
 * Function:	code to handle grupes
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include "jeamland.h"

struct grupe *sys_grupes = (struct grupe *)NULL;

struct grupe_el *
create_grupe_el()
{
	struct grupe_el *el = (struct grupe_el *)xalloc(
	    sizeof(struct grupe_el), "grupe_el struct");
	el->name = (char *)NULL;
	el->flags = 0;
	el->next = (struct grupe_el *)NULL;
	el->grp = (struct grupe *)NULL;
	return el;
}

void
free_grupe_el(struct grupe_el *el)
{
	FREE(el->name);
	xfree(el);
}

struct grupe *
create_grupe()
{
	struct grupe *g = (struct grupe *)xalloc(sizeof(struct grupe),
	    "grupe struct");
	g->gid = (char *)NULL;
	g->flags = 0;
	g->el = (struct grupe_el *)NULL;
	g->next = (struct grupe *)NULL;
	return g;
}

static void
free_all_grupe_els(struct grupe *g)
{
	struct grupe_el *e, *f;

	for (e = g->el; e != (struct grupe_el *)NULL; e = f)
	{
		f = e->next;
		free_grupe_el(e);
	}
}

void
free_grupe(struct grupe *g)
{
	FREE(g->gid);
	free_all_grupe_els(g);
	xfree(g);
}

void
add_grupe_el(struct grupe *g, char *name, long flags)
{
	struct grupe_el **e, *new;

	new = create_grupe_el();
	COPY(new->name, name, "grupe_el name");
	new->flags = flags;
	new->grp = g;

	for (e = &g->el; *e != (struct grupe_el *)NULL; e = &((*e)->next))
		;
	*e = new;
	return;
}

void
remove_grupe_el(struct grupe *g, struct grupe_el *el)
{
	struct grupe_el **p;

	for (p = &g->el; *p != (struct grupe_el *)NULL; p = &((*p)->next))
		if (*p == el)
		{
			*p = el->next;
			free_grupe_el(el);
			return;
		}
#ifdef DEBUG
	fatal("remove_grupe_el: not found.");
#endif
}

struct grupe *
add_grupe(struct grupe **g, char *id)
{
	struct grupe *new = create_grupe();

	COPY(new->gid, id, "grupe id");

	for (; *g != (struct grupe *)NULL; g = &((*g)->next))
		if (strcmp((*g)->gid, id) > 0)
			break;
	new->next = *g;
	return *g = new;
}

void
remove_grupe(struct grupe **list, struct grupe *g)
{
	struct grupe **p;

	for (p = list; *p != (struct grupe *)NULL; p = &((*p)->next))
		if (*p == g)
		{
			*p = g->next;
			free_grupe(g);
			return;
		}
#ifdef DEBUG
	fatal("remove_grupe: not found.");
#endif
}

void
free_grupes(struct grupe **list)
{
	struct grupe *g, *h;

	for (g = *list; g != (struct grupe *)NULL; g = h)
	{
		h = g->next;
		free_grupe(g);
	}
	*list = (struct grupe *)NULL;
}

struct grupe_el *
find_grupe_el(struct grupe *g, char *name)
{
	struct grupe_el *p;

	for (p = g->el; p != (struct grupe_el *)NULL; p = p->next)
		if (!strcasecmp(p->name, name))
			return p;
	return (struct grupe_el *)NULL;
}

struct grupe *
find_grupe(struct grupe *list, char *id)
{
	struct grupe *p;

	for (p = list; p != (struct grupe *)NULL; p = p->next)
		if (!strcasecmp(p->gid, id))
			return p;
	return (struct grupe *)NULL;
}

int
member_sysgrupe(char *grupe, char *name, int rec)
{
	struct grupe *g;
	struct grupe_el *p;
	char *id;
	int idl = strlen(grupe) - 1;
	int flag = 0;
	int lev;

	if (rec > GRUPE_RECURSE_DEPTH)
	{
		log_file("error", "Grupe recursion limit exceeded. [%s] [%s].",
		    grupe, name);
		return 0;
	}

	/* Is it a level grupe ?
	 * This stuff is self contained and can not be recursive. */

	id = string_copy(grupe, "member_sysgrupe tmp");

	if (id[idl] == '+' || id[idl] == '-')
	{
		flag = id[idl] == '+' ? 1 : -1;
		id[idl] = '\0';
	}
	if ((lev = level_number(id)) > L_RESIDENT)
	{
		int i = query_real_level(name);

		xfree(id);

		if ((i == lev && flag == 0) ||
		    (i >= lev && flag == 1) ||
		    (i <= lev && flag == -1))
			return 1;

		return 0;
	}
	
	/* Ok then, is it a system grupe ? */

	if ((g = find_grupe(sys_grupes, id)) == (struct grupe *)NULL)
	{
		xfree(id);
		return 0;
	}
	xfree(id);

	for (p = g->el; p != (struct grupe_el *)NULL; p = p->next)
		/* Exclusions */
		if (p->name[0] == '!' && !strcasecmp(p->name + 1, name))
			return 0;
		else if (!strcasecmp(p->name, name))
			return 1;
		else if (p->name[0] == '#')
			/* Recursion.. */
			return member_sysgrupe(p->name + 1, name, rec + 1);

	return 0;
}

int
count_grupes(struct grupe *list)
{
	int i;

	for (i = 0; list != (struct grupe *)NULL; list = list->next)
		i++;
	return i;
}

int
count_grupe_els(struct grupe *g)
{
	int i;
	struct grupe_el *e;

	for (i = 0, e = g->el; e != (struct grupe_el *)NULL; e = e->next)
		i++;
	return i;
}

static int
expand_level_grupes(struct vecbuf *vbuf, char *id)
{
	int idl = strlen(id) - 1;
	int flag = 0;
	int lev;

	if (id[idl] == '+' || id[idl] == '-')
	{
		flag = id[idl] == '+' ? 1 : -1;
		id[idl] = '\0';
	}
	if ((lev = level_number(id)) <= L_RESIDENT)
		return 0;
	do
	{
		level_users(vbuf, lev);
		lev += flag;
	} while (flag && lev > L_RESIDENT && lev <= L_MAX);
	return 1;
}

static int
expand_a_grupe(struct vecbuf *vbuf, struct grupe *list, char *id, int rec)
{
	struct grupe *g;
	struct grupe_el *e;
	int i;

	if ((g = find_grupe(list, id)) == (struct grupe *)NULL)
		return 0;

	for (e = g->el; e != (struct grupe_el *)NULL; e = e->next)
	{
		/* Recursion */
		if (*e->name == '#' && rec < GRUPE_RECURSE_DEPTH)
		{
			if (!expand_a_grupe(vbuf, list, e->name + 1, rec + 1) &&
			    (list == sys_grupes ||
			    !expand_a_grupe(vbuf, sys_grupes, e->name + 1,
			    rec + 1)))
			{
				char *s = string_copy(e->name + 1,
				    "expand_a_grupe - level");
				expand_level_grupes(vbuf, s);
				xfree(s);
			}
			continue;
		}

		/* Exclusion */
		if (e->name[0] == '!')
		{
			int k;

			for (k = vbuf->vec->size; k--; )
				if (vbuf->vec->items[k].type == T_STRING &&
				    !strcmp(vbuf->vec->items[k].u.string,
				    e->name + 1))
				{
					xfree(vbuf->vec->items[k].u.string);
					vbuf->vec->items[k].type = T_EMPTY;
				}
			continue;
		}

		i = vecbuf_index(vbuf);
		vbuf->vec->items[i].type = T_STRING;
		vbuf->vec->items[i].u.string = string_copy(e->name,
		    "expand_grupe tmp name");
	}
	return 1;
}

int
expand_grupe(struct user *p, struct vecbuf *vbuf, char *id)
{
	/* Three things to try... */
	if (expand_level_grupes(vbuf, id) ||
	    expand_a_grupe(vbuf, sys_grupes, id, 0) ||
	    (p != (struct user *)NULL &&
	    expand_a_grupe(vbuf, p->grupes, id, 0)))
		return 1;
	return 0;
}

void
store_grupes(FILE *fp, struct grupe *list)
{
	struct grupe *g;
	struct grupe_el *el;
	char *cs;

	for (g = list; g != (struct grupe *)NULL; g = g->next)
	{
		cs = code_string(g->gid);
		fprintf(fp, "grupe ({\"%s\",%ld,({", cs, g->flags);
		xfree(cs);
		for (el = g->el; el != (struct grupe_el *)NULL; el = el->next)
		{
			cs = code_string(el->name);
			fprintf(fp, "\"%s\",", cs);
			xfree(cs);
		}
		fprintf(fp, "}),})\n");
	}
}

void
restore_grupes(struct grupe **list, char *buf)
{
	struct grupe *g;
	struct svalue *sv;
	int i;

	if ((sv = decode_one(buf, "decode_one grupe")) ==
	    (struct svalue *)NULL)
		return;
	if (sv->type == T_VECTOR && sv->u.vec->size == 3 &&
	    sv->u.vec->items[0].type == T_STRING &&
	    sv->u.vec->items[1].type == T_NUMBER &&
	    sv->u.vec->items[2].type == T_VECTOR)
	{
		if (find_grupe(*list, sv->u.vec->items[0].u.string) !=
		    (struct grupe *)NULL)
		{
			free_svalue(sv);
			return;
		}
		g = add_grupe(list, sv->u.vec->items[0].u.string);
		g->flags = sv->u.vec->items[1].u.number;
		for (i = 0; i < sv->u.vec->items[2].u.vec->size; i++)
		{
			if (sv->u.vec->items[2].u.vec->items[i].type !=
			    T_STRING)
				continue;
			if (find_grupe_el(g,
			    sv->u.vec->items[2].u.vec->items[i].u.string) !=
			    (struct grupe_el *)NULL)
				continue;
			add_grupe_el(g,
			    sv->u.vec->items[2].u.vec->items[i].u.string,
			    0);
		}
	}
	free_svalue(sv);
}

