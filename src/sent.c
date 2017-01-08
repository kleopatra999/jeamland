/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	sent.c
 * Function:	Sentence related functions.
 *		A sentence is a dynamic command.
 **********************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include "jeamland.h"

extern struct user *users;

struct sent *
create_sent()
{
	struct sent *s = (struct sent *)xalloc(sizeof(struct sent),
	    "sentence struct");

	s->ob = (struct object *)NULL;
	s->cmd = (char *)NULL;
	s->next_export = s->next_inv = (struct sent *)NULL;
	return s;
}

void
free_sent(struct sent *s)
{
	FREE(s->cmd);
	xfree(s);
}

void
add_sent_to_export_list(struct object *ob, struct sent *s)
{
	s->next_export = ob->export;
	ob->export = s;
}

void
add_sent_to_inv_list(struct object *ob, struct sent *s)
{
	s->next_inv = ob->inv;
	ob->inv = s;
}

void
remove_sent_from_inv_list(struct object *ob, struct sent *s)
{
	struct sent **p;

	for (p = &ob->inv; *p != (struct sent *)NULL; p = &((*p)->next_inv))
		if (*p == s)
		{
			*p = s->next_inv;
			return;
		}
#ifdef DEBUG
	fatal("remove_sent_from_inv_list: not in list.");
#endif
}

/* An object enters an environment..
 * If the object is exporting any sentences, they need adding to the
 * environment's list.
 */
void
enter_env(struct object *ob, struct object *env)
{
	struct sent *s;

	for (s = ob->export; s != (struct sent *)NULL; s = s->next_export)
		add_sent_to_inv_list(env, s);
}

/* An object leaves an environment..
 * If the environment is holding any sentences exported by the object,
 * they need removing.
 */
void
leave_env(struct object *ob)
{
	struct sent *s;

	for (s = ob->export; s != (struct sent *)NULL; s = s->next_export)
		remove_sent_from_inv_list(ob->super, s);
}

void
free_sentences(struct object *ob)
{
	struct sent *s, *t;

	for (s = ob->export; s != (struct sent *)NULL; s = t)
	{
		t = s->next_export;
		free_sent(s);
	}
	ob->export = (struct sent *)NULL;
}

struct sent *
find_sent(struct object *ob, char *str)
{
	struct sent *s;

	for (s = ob->inv; s != (struct sent *)NULL; s = s->next_inv)
		if (!strcmp(s->cmd, str))
			return s;
	return (struct sent *)NULL;
}

struct sent *
find_sent_cmd(struct user *p, char *cmd)
{
	struct sent *s;

	/* Check the inv list belonging to the user's environment. */
	for (s = p->ob->super->inv; s != (struct sent *)NULL; s = s->next_inv)
		if (!strcmp(s->cmd, cmd))
			return s;

	/* Check the inv list of the user..
	 * this is for jlm's which are inside the user! */
	for (s = p->ob->inv; s != (struct sent *)NULL; s = s->next_inv)
		if (!strcmp(s->cmd, cmd))
			return s;

	/* Check the global sent list... 
	 * This is stored as the inv list of the dummy user... as good a
	 * place as any!
	 */
	for (s = users->ob->inv; s != (struct sent *)NULL; s = s->next_inv)
		if (!strcmp(s->cmd, cmd))
			return s;

	return (struct sent *)NULL;
}

int
sent_cmd(struct user *p, char *cmd, char *arg)
{
	struct sent *s;

#ifdef DEBUG
	if (cmd == (char *)NULL)
		fatal("Null cmd in sent_cmd");
#endif

	if ((s = find_sent_cmd(p, cmd)) != (struct sent *)NULL)
	{
		switch (s->ob->type)
		{
		    case OT_JLM:
			jlm_start_service(s->ob->m.jlm, "cmd");
			jlm_service_param(s->ob->m.jlm, "user", p->rlname);
			jlm_service_param(s->ob->m.jlm, "cmd", cmd);
			if (arg != (char *)NULL)
				write_a_jlm(s->ob->m.jlm, 0, "%s\n", arg);
			jlm_end_service(s->ob->m.jlm);
			break;
		    default:
			fatal("Sentence command trapped by unsupported ob.");
		}
		return 1;
	}

	return 0;
}

#define DSL_HANDLE \
	if (!flag) \
	{ \
		cadd_strbuf(text, '\n'); \
		add_strbuf(text, "[ LOCAL    ] "); \
		flag = 1; \
	} \
	if (num) \
		add_strbuf(text, ", "); \
	add_strbuf(text, s->cmd); \
	num = 1

void
dump_sent_list(struct user *p, struct strbuf *text)
{
	struct sent *s;
	int flag = 0;
	int num = 0;

	/* Check the inv list belonging to the user's environment. */
	for (s = p->ob->super->inv; s != (struct sent *)NULL; s = s->next_inv)
	{
		DSL_HANDLE;
	}

	/* Check the inv list of the user..
	 * this is for jlm's which are inside the user! */
	for (s = p->ob->inv; s != (struct sent *)NULL; s = s->next_inv)
	{
		DSL_HANDLE;
	}

	/* Check the global sent list... 
	 * This is stored as the inv list of the dummy user... as good a
	 * place as any!
	 */
	for (s = users->ob->inv; s != (struct sent *)NULL; s = s->next_inv)
	{
		DSL_HANDLE;
	}
}

