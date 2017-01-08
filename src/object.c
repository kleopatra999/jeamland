/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	object.c
 * Function:	Generic object support.
 **********************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "jeamland.h"

struct object *objects = (struct object *)NULL;
#ifdef MALLOC_STATS
int num_objects = 0;
#endif

extern time_t current_time;

struct object *
create_object()
{
	static unsigned long id = 0;
	struct object *ob = (struct object *)xalloc(sizeof(struct object),
	    "object struct");
#ifdef MALLOC_STATS
	num_objects++;
#endif
	ob->id = ++id;
	ob->type = OT_EMPTY;
	ob->num_contains = 0;
	ob->contains = ob->next_contains = (struct object *)NULL;
	ob->super = (struct object *)NULL;
	ob->export = ob->inv = (struct sent *)NULL;
	ob->create_time = current_time;

	/* Add to global list */
	ob->next = objects;
	objects = ob;
	return ob;
}

void
add_to_env(struct object *ob, struct object *env)
{
	ob->next_contains = env->contains;
	env->contains = ob;
	env->num_contains++;
	ob->super = env;
	enter_env(ob, env);
}

void
remove_from_env(struct object *ob)
{
	struct object **s;

	if (ob->super == (struct object *)NULL)
		return;

	leave_env(ob);

	for (s = &ob->super->contains; *s != (struct object *)NULL;
	    s = &((*s)->next_contains))
		if (*s == ob)
		{
			*s = ob->next_contains;
			ob->super->num_contains--;
			return;
		}
#ifdef DEBUG
	fatal("Remove_from_env, object not found.");
#endif
}

void
free_object(struct object *ob)
{
	struct object **o;

	remove_from_env(ob);
	free_sentences(ob);

	for (o = &objects; *o != (struct object *)NULL; o = &((*o)->next))
		if (*o == ob)
		{
			*o = ob->next;

        		switch (ob->type)
        		{
			    case OT_EMPTY:
				break;
			    case OT_USER:
				tfree_user(ob->m.user);
				break;
			    case OT_ROOM:
				tfree_room(ob->m.room);
				break;
			    case OT_JLM:
				free_jlm(ob->m.jlm);
				break;
			    default:
				fatal("Bad object type in free_object: %d",
				    ob->type);
			}
			xfree(ob);
#ifdef MALLOC_STATS
			num_objects--;
#endif
			return;
		}

#ifdef DEBUG
      fatal("Free object on missing object.");
#endif
}

int
move_object(struct object *ob, struct object *to)
{
	if (ob->type == OT_EMPTY)
		fatal("Moving empty object.");
	if (ob->type == OT_ROOM)
		return 0;
	if (ob->type == OT_USER && to->type != OT_ROOM)
		return 0;

	remove_from_env(ob);
	add_to_env(ob, to);

	/* Update the user environment shortcut */
	if (ob->type == OT_USER)
		ob->m.user->super = to->m.room;

	return 1;
}

char *
obj_name(struct object *ob)
{
	static char buf[0x100];

	switch (ob->type)
	{
	    case OT_EMPTY:
		return "!!EMPTY!!";
	    case OT_USER:
		sprintf(buf, "User:%s", ob->m.user->capname);
		break;
	    case OT_ROOM:
		sprintf(buf, "Room:%s", ob->m.room->fname);
		break;
	    case OT_JLM:
		sprintf(buf, "JLM:%s", ob->m.jlm->id);
		break;
	    default:
		return "!!UNKNOWN!!";
	}
	return buf;
}

static void
object_inv(struct user *p, struct object *o, struct strbuf *b, int indent)
{
	struct object *ob;
	int i;

	/* *sigh*.. */
	if (o->type == OT_USER && !CAN_SEE(p, o->m.user))
		return;

	for (i = indent; i--; )
		cadd_strbuf(b, ' ');

	sadd_strbuf(b, "%s     [%d/%s]\n", obj_name(o), o->id,
	    shnctime(&o->create_time));

	for (ob = o->contains; ob != (struct object *)NULL;
	    ob = ob->next_contains)
		object_inv(p, ob, b, indent + 4);
}

void
object_dump(struct user *p)
{
	struct strbuf buf;
	struct object *ob;

	init_strbuf(&buf, 0, "object_dump");

	for (ob = objects; ob != (struct object *)NULL; ob = ob->next)
		if (ob->super == (struct object *)NULL)
			object_inv(p, ob, &buf, 0);

	pop_strbuf(&buf);
	more_start(p, buf.str, NULL);
}

