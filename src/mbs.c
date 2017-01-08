/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	mbs.c
 * Function:	The Multi Board System support functions.
 *		A large amount of relevant code is in f_mbs()
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include "jeamland.h"

extern time_t current_time;
extern struct user *users;

struct mbs *mbs_master = (struct mbs *)NULL;

struct mbs *
create_mbs()
{
	struct mbs *m = (struct mbs *)xalloc(sizeof(struct mbs),
	    "*mbs struct");

	m->id = m->desc = m->room = (char *)NULL;
	m->last = 0;
	m->next = (struct mbs *)NULL;
	return m;
}

void
free_mbs(struct mbs *m)
{
	FREE(m->id);
	FREE(m->desc);
	FREE(m->room);
	xfree(m);
}

struct umbs *
create_umbs()
{
	struct umbs *m = (struct umbs *)xalloc(sizeof(struct umbs),
	    "umbs struct");

	m->id = (char *)NULL;
	m->last = 0;
	m->next = (struct umbs *)NULL;
	return m;
}

void
free_umbs(struct umbs *m)
{
	FREE(m->id);
	xfree(m);
}

void
add_mbs(struct mbs *entry)
{
	struct mbs **m;
	    
	for (m = &mbs_master; *m != (struct mbs *)NULL; m = &((*m)->next))
		if (strcmp((*m)->id, entry->id) > 0)
			break;
	entry->next = *m;
	*m = entry;
}

void
remove_mbs(struct mbs *entry)
{
	struct mbs **m;

	for (m = &mbs_master; *m != (struct mbs *)NULL; m = &((*m)->next))
	{
		if (*m == entry)
		{
			*m = entry->next;
			free_mbs(entry);
			return;
		}
	}
	fatal("remove_mbs: entry not in list.");
}

void
add_umbs(struct user *p, struct umbs *entry)
{
	struct umbs **m;
	    
	for (m = &p->umbs; *m != (struct umbs *)NULL; m = &((*m)->next))
		if (strcmp((*m)->id, entry->id) > 0)
			break;
	entry->next = *m;
	*m = entry;
}

void
remove_umbs(struct user *p, struct umbs *entry)
{
	struct umbs **m;

	for (m = &p->umbs; *m != (struct umbs *)NULL; m = &((*m)->next))
	{
		if (*m == entry)
		{
			*m = entry->next;
			free_umbs(entry);
			return;
		}
	}
	fatal("remove_umbs: entry not in list.");
}

void
load_mbs()
{
	FILE *fp;
	struct stat st;
	char *buf;

	if ((fp = fopen(F_MBS, "r")) == (FILE *)NULL)
		return;
	if (fstat(fileno(fp), &st) == -1)
		fatal("Could not stat open file - load_mbs");
	if (!st.st_size)
	{
		fclose(fp);
		unlink(F_MBS);
		return;
	}

	buf = (char *)xalloc(st.st_size, "load_mbs buf");

	while (fgets(buf, st.st_size, fp) != (char *)NULL)
	{
		if (ISCOMMENT(buf))
			continue;
		if (!strncmp(buf, "mbs ", 4))
		{
			struct svalue *sv;

			if ((sv = decode_one(buf, "decode_one mbs"))
			    == (struct svalue *)NULL)
				continue;

			if (sv->type == T_VECTOR && sv->u.vec->size == 4 &&
			    sv->u.vec->items[0].type == T_STRING &&
			    sv->u.vec->items[1].type == T_STRING &&
			    sv->u.vec->items[2].type == T_STRING &&
			    sv->u.vec->items[3].type == T_NUMBER)
			{
				struct mbs *m = create_mbs();

				m->id = string_copy(
				    sv->u.vec->items[0].u.string, "*mbs id");
				m->room = string_copy(
				    sv->u.vec->items[1].u.string, "*mbs room");
				m->desc = string_copy(
				    sv->u.vec->items[2].u.string, "*mbs desc");
				m->last = (time_t)sv->u.vec->items[3].u.number;
				add_mbs(m);
			}
			else
				log_file("error", "Error in master mbs line: "
				    "%s", buf);
			free_svalue(sv);
			continue;
		}
	}
	fclose(fp);
	xfree(buf);
}

void
store_mbs()
{
	FILE *fp;
	struct mbs *m;

	if (mbs_master == (struct mbs *)NULL)
	{
		unlink(F_MBS);
		return;
	}

	if ((fp = fopen(F_MBS, "w")) == (FILE *)NULL)
	{
		log_perror("fopen F_MBS, write");
		return;
	}

	fprintf(fp, "# id, room, desc, last\n");
	for (m = mbs_master; m != (struct mbs *)NULL; m = m->next)
	{
		char *id = code_string(m->id);
		char *room = code_string(m->room);
		char *desc = code_string(m->desc);

		fprintf(fp, "mbs ({\"%s\",\"%s\",\"%s\",%ld,})\n",
		    id, room, desc, (long)m->last);

		xfree(id);
		xfree(room);
		xfree(desc);
	}

	fclose(fp);
}

struct mbs *
find_mbs(char *id, int fuzzy)
{
	int flag;
	int idl;
	struct mbs *found, *m;

	if (fuzzy)
	{
		flag = 0;
		found = (struct mbs *)NULL;
		idl = strlen(id);
	}

	for (m = mbs_master; m != (struct mbs *)NULL; m = m->next)
	{
		if (!strcasecmp(m->id, id))
			return m;
		else if (fuzzy && !strncasecmp(m->id, id, idl))
		{
			flag++;
			found = m;
		}
	}
	if (fuzzy && flag == 1)
		return found;
	return (struct mbs *)NULL;
}

struct mbs *
find_mbs_by_room(char *rid)
{
	struct mbs *m;

	for (m = mbs_master; m != (struct mbs *)NULL; m = m->next)
		if (!strcmp(m->room, rid))
			return m;
	return (struct mbs *)NULL;
}

struct umbs *
find_umbs(struct user *p, char *id, int fuzzy)
{
	int flag;
	int idl;
	struct umbs *found, *m;

	if (fuzzy)
	{
		flag = 0;
		found = (struct umbs *)NULL;
		idl = strlen(id);
	}

	for (m = p->umbs; m != (struct umbs *)NULL; m = m->next)
	{
		if (!strcasecmp(m->id, id))
			return m;
		else if (fuzzy && !strncasecmp(m->id, id, idl))
		{
			flag++;
			found = m;
		}
	}
	if (fuzzy && flag == 1)
		return found;
	return (struct umbs *)NULL;
}

struct room *
get_mbs_room(struct user *p, struct mbs *m)
{
	struct room *r;

	if (!ROOM_POINTER(r, m->room))
	{
		write_socket(p, "Could not find room for board '%s'.\n",
		    m->id);
		return (struct room *)NULL;
	}

	if (!(r->flags & R_BOARD))
	{
		write_socket(p, "Room contains no board.\n");
		return (struct room *)NULL;
	}
	return r;
}

struct room *
get_current_mbs(struct user *p, struct umbs **ptr, struct mbs **mptr)
{
	struct room *r;
	struct mbs *m;
	struct umbs *n;

	if (p->mbs_current == (char *)NULL ||
	    (n = find_umbs(p, p->mbs_current, 0)) ==
	    (struct umbs *)NULL ||
	    (m = find_mbs(n->id, 0)) == (struct mbs *)NULL)
	{
		write_socket(p, "No board selected.\n");
		return (struct room *)NULL;
	}

	if ((r = get_mbs_room(p, m)) == (struct room *)NULL)
		return (struct room *)NULL;

	*ptr = n;
	*mptr = m;
	return r;
}


struct message *
mbs_get_first_unread(struct user *p, struct board *b, struct umbs *n, int *num)
{
	struct message *m;

	/* Quick check first - if the last message is older than our last
	 * reading, then there are no unread messages. */
	if (b->last_msg == (struct message *)NULL ||
	    b->last_msg->date <= n->last)
		return (struct message *)NULL;

	*num = 1;

	for (m = b->messages; m != (struct message *)NULL;
	    m = m->next, (*num)++)
		if (m->date > n->last)
			return m;
	return (struct message *)NULL;
}

time_t
mbs_get_last_note_time(struct board *b)
{
	if (b->last_msg == (struct message *)NULL)
		return 0;

	return b->last_msg->date;
}

void
remove_bad_umbs(struct user *p)
{
	struct umbs *m, *n;

	for (m = p->umbs; m != (struct umbs *)NULL; m = n)
	{
		n = m->next;

		if (find_mbs(m->id, 0) == (struct mbs *)NULL)
		{
			write_socket(p, "Removed bad board: %s\n", m->id);
			remove_umbs(p, m);
		}
	}
}

void
notify_mbs(char *rid)
{
	struct mbs *m;
	struct user *p;

	log_file("board", "New note in room %s.", rid);

	if ((m = find_mbs_by_room(rid)) == (struct mbs *)NULL)
		return;
	m->last = current_time;
	store_mbs();

	for (p = users->next; p != (struct user *)NULL; p = p->next)
		if (find_umbs(p, m->id, 0) != (struct umbs *)NULL)
		{
			attr_colour(p, "notify");
			write_socket(p, "[ MBS: New note on board '%s' ]",
			    m->id);
			reset(p);
			write_socket(p, "\n");
		}
}

void
free_umbses(struct user *p)
{
	struct umbs *m, *n;

        for (m = p->umbs; m != (struct umbs *)NULL; m = n)
        {
                n = m->next;
                free_umbs(m);
        }
	p->umbs = (struct umbs *)NULL;
}

