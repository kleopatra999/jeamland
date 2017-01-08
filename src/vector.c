/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	vector.c
 * Function:	Vector (general array) support including vecbuf datatype
 **********************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "jeamland.h"

struct vector *
allocate_vector(int size, int type, char *id)
{
	struct vector *v;
	int i;

	/* Snnneeeeaaaaaky! */
	v = (struct vector *)xalloc(sizeof(struct vector) +
	    sizeof(struct svalue) * (size - 1), id);
	v->size = size;
	for (i = size; i--; )
		switch (type)
		{
		    case T_STRING:
			TO_STRING(v->items[i]);
			break;
		    case T_NUMBER:
			TO_NUMBER(v->items[i]);
			break;
		    case T_UNUMBER:
			TO_UNUMBER(v->items[i]);
			break;
		    case T_POINTER:
			TO_POINTER(v->items[i]);
			break;
		    case T_EMPTY:
			TO_EMPTY(v->items[i]);
			break;
		    default:
			fatal("Bad type in allocate_vector");
		}
	return v;
}

void
expand_vector(struct vector **v, int s)
{
	int i;

	*v = (struct vector *)xrealloc(*v, sizeof(struct vector) +
	    sizeof(struct svalue) * ((*v)->size + s - 1));
	for (i = (*v)->size; i < (*v)->size + s; i++)
	{
		TO_EMPTY((*v)->items[i]);
	}
	(*v)->size += s;
}

void
free_vector(struct vector *v)
{
	int i;

	if (v == (struct vector *)NULL)
		return;

	for (i = v->size; i--; )
		free_svalue(&v->items[i]);
	xfree(v);
}

struct svalue *
vector_svalue(struct vector *vec)
{
	static struct svalue val;

	val.type = T_VECTOR;
	val.u.vec = vec;
	return &val;
}

int
member_vector(struct svalue *sv, struct vector *vec)
{
	int i;

	for (i = vec->size; i--; )
		if (equal_svalue(sv, &vec->items[i]))
			return i;
	return -1;
}

void
init_vecbuf(struct vecbuf *v, int size, char *id)
{
	if (!size)
		size = VECBUF_CHUNK;

	v->offset = -1;
	v->vec = allocate_vector(size, T_EMPTY, id);
}

int
vecbuf_index(struct vecbuf *v)
{
	if (++v->offset >= v->vec->size)
		expand_vector(&v->vec, VECBUF_CHUNK);
	return v->offset;
}

struct vector *
vecbuf_vector(struct vecbuf *v)
{
	if (v->vec->size > v->offset)
		/*expand_vector(v->vec, v->size - v->offset - 1);*/
		v->vec->size = v->offset + 1;
	return v->vec;
}

void
free_vecbuf(struct vecbuf *v)
{
	free_vector(v->vec);
}

