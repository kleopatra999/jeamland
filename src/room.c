/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	room.c
 * Function:	Virtual room support
 **********************************************************************/
/*
 * Room code.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include "jeamland.h"

extern struct user *users;
extern int sysflags;
extern int loads;
extern int rooms_created;

struct room *rooms = (struct room *)NULL;
int num_rooms = 0, peak_rooms = 0;

struct vector *start_rooms = (struct vector *)NULL;

void
init_room_list()
{
	struct object *ob;

	/* Create a 'void' room at the head of the rooms list */
	init_room(rooms = create_room());
	COPY(rooms->name, "the void", "*void name");
	COPY(rooms->long_desc, VOID_DESC, "*void desc");
	COPY(rooms->owner, ROOT_USER, "*void owner");
	COPY(rooms->fname, "<void>", "*void fname");
	ob = create_object();
	ob->type = OT_ROOM;
	ob->m.room = rooms;
	rooms->ob = ob;
}

struct room *
create_room()
{
	struct room *r = (struct room *)xalloc(sizeof(struct room),
	    "room struct");
	return r;
}

void
init_room(struct room *r)
{
	r->name = (char *)NULL;
	r->fname = (char *)NULL;
	r->owner = (char *)NULL;
	r->exit = (struct exit *)NULL;
	r->alias = (struct alias *)NULL;
	r->board = (struct board *)NULL;
	r->long_desc = r->lock_grupe = (char *)NULL;
	r->flags = 0;
	r->inhibit_cleanup = 0;
	r->alias_lim = ALIAS_LIMIT;
	r->next = (struct room *)NULL;
}

struct exit *
create_exit()
{
	struct exit *e = (struct exit *)xalloc(sizeof(struct exit),
	    "room exit");
	e->name = e->file = (char *)NULL;
	e->next = (struct exit *)NULL;
	return e;
}

void
free_exit(struct exit *e)
{
	FREE(e->name);
	FREE(e->file);
	xfree(e);
}

void
free_room(struct room *r)
{
	struct exit *e, *n;

#ifdef DEBUG
	if (r->inhibit_cleanup)
		fatal("free_room(%s): inhibit_cleanup: %d", r->fname,
		    r->inhibit_cleanup);
#endif

	for (e = r->exit; e != (struct exit *)NULL; e = n)
	{
		n = e->next;
		free_exit(e);
	}
	r->exit = (struct exit *)NULL;
	free_board(r->board);
	r->board = (struct board *)NULL;
	free_aliases(&r->alias);
	FREE(r->long_desc);
	FREE(r->name);
	FREE(r->owner);
	FREE(r->fname);
	FREE(r->lock_grupe);
}

void
tfree_room(struct room *r)
{
	free_room(r);
	xfree(r);
}

struct room *
find_room(char *name)
{
	struct room *r;
	int num, n;

	if (name == (char *)NULL)
		return (struct room *)NULL;

	if (name[0] == '#')
		num = atoi(name + 1);
	else
		num = -1;
	for (n = 0, r = rooms->next; r != (struct room *)NULL; r = r->next)
		if (!strcmp(name, r->fname) || num == ++n)
			return r;
	return (struct room *)NULL;
}

void
add_exit(struct room *r, struct exit *e)
{
        struct exit **b;

        for (b = &r->exit; *b != (struct exit *)NULL; b = &((*b)->next))
                if (strcmp((*b)->name, e->name) > 0)
			break;
	e->next = *b;
	*b = e;
}

struct room *
restore_room(char *name)
{
	FILE *fp;
	int ok;
	struct room *r;
	struct object *ob;
	struct stat st;
	char fname[MAXPATHLEN + 1];
	char *buf, *ptr;

	sprintf(fname, "room/%c/%s.o", name[0], name);
	if ((fp = fopen(fname, "r")) == (FILE *)NULL)
		return (struct room *)NULL;
	if (fstat(fileno(fp), &st) == -1)
		fatal("Can't stat open file.");
	/* Definitely big enough ;-) */
	buf = (char *)xalloc((size_t)st.st_size, "restore room");
	ok = 0;
	r = create_room();
	init_room(r);
	COPY(r->fname, name, "room fname");
	while (fgets(buf, (int)st.st_size - 1, fp) != (char *)NULL)
	{
		if (!strncmp(buf, "long_desc \"", 11))
		{
			DECODE(r->long_desc, "long desc");
			ok += 2;
			continue;
		}
		if (!strncmp(buf, "name \"", 6))
		{
			DECODE(r->name, "room name");
			continue;
		}
		if (!strncmp(buf, "owner \"", 7))
		{
			DECODE(r->owner, "owner name");
			continue;
		}
		if (!strncmp(buf, "lock_grupe \"", 12))
		{
			DECODE(r->lock_grupe, "lock grupe");
			continue;
		}
		if (!strncmp(buf, "exit ", 5))
		{
			struct svalue *sv;

			if ((sv = decode_one(buf, "decode_one exit")) ==
			    (struct svalue *)NULL)
				continue;

                        if (sv->type == T_VECTOR && sv->u.vec->size == 2 &&
                            sv->u.vec->items[0].type == T_STRING &&
                            sv->u.vec->items[1].type == T_STRING)
			{
				struct exit *e = create_exit();
				e->name = string_copy(
				    sv->u.vec->items[0].u.string,
				    "exit name");
				e->file = string_copy(
				    sv->u.vec->items[1].u.string,
				    "exit file");
				add_exit(r, e);
			}
			else
                                log_file("error", "Error in exit line: "
                                    "room %s, line '%s'", name, buf);
                        free_svalue(sv);
			continue;
		}
		if (!strncmp(buf, "alias ", 6))
		{
			restore_alias(&r->alias, buf, "room", name);
			continue;
		}
		if (sscanf(buf, "flags %d", &r->flags))
		{
			ok++;
			continue;
		}
		if (sscanf(buf, "alias_lim %d", &r->alias_lim))
			continue;
	}
	fclose(fp);
	xfree(buf);
	if (ok != 3)
	{
		tfree_room(r);
		return (struct room *)NULL;
	}
	if (sysflags & SYS_SHOWLOAD)
		log_file("syslog", "Loaded room %s", r->fname);
	r->next = rooms->next;
	rooms->next = r;
	loads++;
	if (++num_rooms > peak_rooms)
		peak_rooms = num_rooms;
	if (r->owner == (char *)NULL)
	{
		COPY(r->owner, ROOT_USER, "room owner");
	}
	if (r->flags & R_BOARD)
		r->board = restore_board("board", r->fname);

	ob = create_object();
	ob->type = OT_ROOM;
	ob->m.room = r;
	r->ob = ob;
	return r;
}

int
store_room(struct room *r)
{
	FILE *fp;
	char *ptr = (char *)NULL;
	char fname[MAXPATHLEN + 1];
	struct exit *e;
	struct alias *a;

	if (r->fname == (char *)NULL)
		return 0;
	sprintf(fname, "room/%c/%s.o", r->fname[0], r->fname);
	if ((fp = fopen(fname, "w")) == (FILE *)NULL)
		return 0;
	ENCODE(r->long_desc);
	fprintf(fp, "long_desc \"%s\"\n", ptr);
	ENCODE(r->owner);
	fprintf(fp, "owner \"%s\"\n", ptr);
	ENCODE(r->name);
	fprintf(fp, "name \"%s\"\n", ptr);
	fprintf(fp, "flags %d\n", r->flags);
	fprintf(fp, "alias_lim %d\n", r->alias_lim);
	if (r->lock_grupe != (char *)NULL)
	{
		ENCODE(r->lock_grupe);
		fprintf(fp, "lock_grupe \"%s\"\n", ptr);
	}
	for (e = r->exit; e != (struct exit *)NULL; e = e->next)
	{
		char *name = code_string(e->name);
		char *file = code_string(e->file);
		fprintf(fp, "exit ({\"%s\",\"%s\",})\n", e->name, e->file);
		xfree(name);
		xfree(file);
	}
        for (a = r->alias; a != (struct alias *)NULL; a = a->next)
        {
                char *key, *fob;

                key = code_string(a->key);
                fob = code_string(a->fob);
                fprintf(fp, "alias ({\"%s\",\"%s\",})\n", key, fob);
                xfree(key);
                xfree(fob);
        }

	fclose(fp);
	FREE(ptr);

	return 1;
}

struct room *
new_room(char *name, char *owner)
{
	struct room *r = create_room();
	struct object *ob = create_object();
	char tmp[BUFFER_SIZE];

	init_room(r);
	r->flags = 0;
	r->long_desc = (char *)xalloc(strlen(NEW_ROOM_DESC)+2, "long desc");
	strcpy(r->long_desc, NEW_ROOM_DESC);
	sprintf(tmp, "%s's room", name);
	COPY(r->fname, name, "room fname");
	COPY(r->name, tmp, "room name");
	COPY(r->owner, owner, "room owner");
	if (*name == '_')
		store_room(r);
	r->next = rooms->next;
	rooms->next = r;
	if (++num_rooms > peak_rooms)
		peak_rooms = num_rooms;
	if (sysflags & SYS_SHOWLOAD)
		log_file("syslog", "Created room %s", r->fname);
	rooms_created++;
	ob->type = OT_ROOM;
	ob->m.room = r;
	r->ob = ob;
	return r;
}

void
remove_exit(struct room *r, struct exit *e)
{
	struct exit **p;

	for (p = &r->exit; *p != (struct exit *)NULL; p = &((*p)->next))
		if (*p == e)
		{
			*p = e->next;
			free_exit(e);
			return;
		}
#ifdef DEBUG
	fatal("remove_exit: exit not found.");
#endif
}

void
destroy_room(struct room *r)
{
	struct object *ob;
	struct room *ptr, *safe_room;

	if (r == (struct room *)NULL)
		fatal("Destroy room on NULL room.");

	if ((safe_room = get_entrance_room()) == r)
		safe_room = rooms;

	for (ob = r->ob->contains; ob != (struct object *)NULL;
	    ob = ob->next_contains)
	{
		if (ob->type == OT_USER)
		{
			write_socket(ob->m.user, "Moving you to safety.\n");
			move_user(ob->m.user, safe_room);
		}
		else if (ob->type == OT_JLM)
		{
			/* Remove from env, to be safe. */
			move_object(ob, rooms->ob);
			kill_jlm(ob->m.jlm);
		}
		else
			fatal("None user/jlm object inside room.");
	}
		
	for (ptr = rooms; ptr != (struct room *)NULL; ptr = ptr->next)
	{
		if (ptr->next == r)
                {
			ptr->next = ptr->next->next;
			if (sysflags & SYS_SHOWLOAD)
				log_file("syslog", 
			 	    "Destroyed room %s", r->fname);
			free_object(r->ob);
			num_rooms--;
			return;
                }
        }
	fatal("destroy room called on non-existant room.");
}

void
reload_room(struct room *r)
{
	struct room *new;
	struct object *ob, *nob;

	if (r == (struct room *)NULL)
		fatal("Reload room on a NULL room.");

	if (r->fname == (char *)NULL)
		return;

	if ((new = restore_room(r->fname)) == (struct room *)NULL)
		return;
	for (ob = r->ob->contains; ob != (struct object *)NULL;
	    ob = nob)
	{
		nob = ob->next_contains;
		move_object(ob, new->ob);
	}
	destroy_room(r);
}

int
handle_exit(struct user *p, char *where, int barge)
{
	struct exit *e;
	struct room *r;

	FUN_START("handle_exit");
	FUN_ARG(where);

	for (e = p->super->exit; e != (struct exit *)NULL; e = e->next)
	{
		if (!strcmp(e->name, where))
		{
			if (!ROOM_POINTER(r, e->file))
			{
				write_socket(p, "That exit leads nowhere.\n");
				log_file("error", 
				    "Illegal exit in %s (%s:%s)", 
				    p->super->fname, e->name, e->file);
				FUN_END;
				return -1;
			}
			if ((r->flags & R_LOCKED) && !barge &&
			    strcasecmp(p->name, r->owner))
			{
				write_socket(p, "That room is locked.\n");
				FUN_END;
				return -1;
			}

			FUN_LINE;

			if (r->lock_grupe != (char *)NULL && !ISROOT(p) &&
			    !member_sysgrupe(r->lock_grupe, p->rlname))
			{
				write_socket(p, "That room is locked.\n");
				FUN_END;
				return -1;
			}
			move_user(p, r);
			FUN_END;
			return 1;
		}
	}
	FUN_END;
	return 0;
}

void
init_start_rooms()
{
	/* A start room is a room with a name prefix of ENTRANCE_PREFIX */
	struct vector *v;
	int i, c, l;

#if defined(ENTRY_ALLOC_RROBIN) && defined(ENTRY_ALLOC_MAX)
	fatal("Cannot define both ENTRY_ALLOC_xx methods at once.");
#endif

#if !defined(ENTRY_ALLOC_RROBIN) && !defined(ENTRY_ALLOC_MAX)
	fatal("Must define an ENTRY_ALLOC_xx method.");
#endif

	if (start_rooms != (struct vector *)NULL)
	{
		free_vector(start_rooms);
		start_rooms = (struct vector *)NULL;
	}

	if (ENTRANCE_PREFIX[0] != '_')
		fatal("ENTRANCE_PREFIX is not a system room (prefix of _)");

	if ((v = get_dir("room/_", 0)) == (struct vector *)NULL)
		fatal("Can not get list of system rooms.");

	l = strlen(ENTRANCE_PREFIX);

	for (c = 0, i = v->size; i--; )
	{
		if (v->items[i].type != T_STRING)
			continue;

		if (!strncmp(v->items[i].u.string, ENTRANCE_PREFIX, l))
			c++;
	}

	start_rooms = allocate_vector(c, T_STRING, "start_rooms");

	for (c = i = 0; i < v->size; i++)
	{
		if (v->items[i].type != T_STRING)
			continue;

		if (!strncmp(v->items[i].u.string, ENTRANCE_PREFIX, l))
		{
			char *t;

			/* Strip the .o */
			if ((t = strrchr(v->items[i].u.string, '.')) !=
			    (char *)NULL)
				*t = '\0';

			start_rooms->items[c].u.string =
			    string_copy(v->items[i].u.string, "start room");

			log_file("syslog", "Found start room: %s",
			    start_rooms->items[c].u.string);
			c++;
		}
	}
	log_file("syslog", "Start rooms found: %d", c);
#ifdef ENTRY_ALLOC_RROBIN
	log_file("syslog", "Start rooms allocated on round-robin basis.");
#endif
#ifdef ENTRY_ALLOC_MAX
	log_file("syslog", "Start rooms allocated on %d user basis.",
	    ENTRY_ALLOC_MAX);
#endif
	free_vector(v);
}

struct room *
get_entrance_room()
{
	struct room *r;
	char *q;

#ifdef ENTRY_ALLOC_RROBIN
	static int c = 0;

	if (c >= start_rooms->size)
		c = 0;

	q = start_rooms->items[c++].u.string;

	if (!ROOM_POINTER(r, q))
	{
		log_file("syslog", "Cannot find entrance room: %s", q);
		return rooms;
	}
	return r;
#endif /* ENTRY_ALLOC_RROBIN */

#ifdef ENTRY_ALLOC_MAX
	int c;
	unsigned int min, min_room;

	min = min_room = 0;

	for (c = 0; c < start_rooms->size; c++)
	{
		q = start_rooms->items[c].u.string;

		if (!ROOM_POINTER(r, q))
		{
			log_file("syslog", "Cannot find entrance room: %s", q);
			return rooms;
		}

		/* If within limit, use this room. */
		if (r->ob->num_contains < ENTRY_ALLOC_MAX)
			return r;

		/* If lowest occupancy yet, store this room */
		if (r->ob->num_contains < min)
		{
			min = r->ob->num_contains;
			min_room = c;
		}
	}

	/* Got to here, didn't find a room within max occupancy,
	 * use the min room. */

	q = start_rooms->items[min_room].u.string;

	log_file("syslog", "get_entrance_room: no rooms within limit, "
	    "using %s at %d", q, min);

	if (!ROOM_POINTER(r, q))
	{
		log_file("syslog", "Cannot find entrance room: %s", q);
		return rooms;
	}

	return r;
#endif /* ENTRY_ALLOC_MAX */
}

void
delete_room(char *r)
{
	char buf[MAXPATHLEN + 1];

	sprintf(buf, "room/%c/%s.o", r[0], r);
	unlink(buf);
	sprintf(buf, "board/%s", r);
	unlink(buf);
}

