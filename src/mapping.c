/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	mapping.c
 * Function:	Mapping datatype support.
 **********************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "jeamland.h"

struct mapping *
allocate_mapping(int size, int i_type, int v_type, char *id)
{
	struct mapping *m;
	int rsize;

	rsize = size + MAPPING_CHUNK;

	m = (struct mapping *)xalloc(sizeof(struct mapping), id);
	m->indices = allocate_vector(rsize, i_type, id);
	m->values = allocate_vector(rsize, v_type, id);
	m->size = size;

	return m;
}

void
free_mapping(struct mapping *m)
{
	free_vector(m->indices);
	free_vector(m->values);
	xfree(m);
}

void
expand_mapping(struct mapping **m, int num)
{
	if ((*m)->size + num >= (*m)->indices->size)
	{
		/* Need to grow the mapping.. */
		expand_vector(&((*m)->indices), MAPPING_CHUNK + num);
		expand_vector(&((*m)->values), MAPPING_CHUNK + num);
	}
	(*m)->size += num;
}


struct svalue *
map_value(struct mapping *m, struct svalue *index)
{
	int i;

	if (m == (struct mapping *)NULL)
		return (struct svalue *)NULL;

	for (i = m->size; i--; )
		if (equal_svalue(&m->indices->items[i], index))
			return &m->values->items[i];
	return (struct svalue *)NULL;
}

int
mapping_space(struct mapping **m)
{
	int i;

	/* First try to see if we have an empty gap */
	for (i = (*m)->size; i--; )
		if ((*m)->indices->items[i].type == T_EMPTY)
			return i;

	/* No gap, add a new element to the top. */
	expand_mapping(m, 1);
	return (*m)->size - 1;
}

int
remove_mapping(struct mapping *m, struct svalue *index)
{
	int i;

	for (i = m->size; i--; )
		if (equal_svalue(&m->indices->items[i], index))
		{
			/* free_svalue sets type to T_EMPTY */
			free_svalue(&m->indices->items[i]);
			free_svalue(&m->values->items[i]);
			return 1;
		}
	return 0;
}

int
remove_mapping_string(struct mapping *m, char *str)
{
	struct svalue sv;

	sv.type = T_STRING;
	sv.u.string = str;
	return remove_mapping(m, &sv);
}

struct svalue *
map_string(struct mapping *m, char *index)
{
	struct svalue sv;

	sv.type = T_STRING;
	sv.u.string = index;
	return map_value(m, &sv);
}

void
check_dead_map(struct mapping **m)
{
	int i;

	for (i = (*m)->size; i--; )
		if ((*m)->indices->items[i].type != T_EMPTY)
			return;

	/* All mapping elements are empty... kill it off */
	free_mapping(*m);
	*m = (struct mapping *)NULL;
}

