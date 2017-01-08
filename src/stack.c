/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	stack.c
 * Function:	Generic stack data type
 **********************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include "jeamland.h"

#define INC_STACK(p)		p->el++, p->sp++
#define DEC_STACK(p)		p->el--, p->sp--
#define CHECK_STACK_INC(p)	if (STACK_FULL(p)) \
					fatal("Stack overflow.")
#define CHECK_STACK_DEC(p)	if (STACK_EMPTY(p)) \
					fatal("Stack underflow.");

void
push_string(struct stack *p, char *str)
{
#ifdef DEBUG
	CHECK_STACK_INC(p);
#endif
	INC_STACK(p);
	p->sp->type = T_STRING;
	p->sp->u.string = string_copy(str, "push string");
}

void
push_malloced_string(struct stack *p, char *str)
{
#ifdef DEBUG_MALLOC
	if (xalloced((void *)str) < 0)
		fatal("push_malloced_string: not malloced.");
#endif

#ifdef DEBUG
	CHECK_STACK_INC(p);
#endif
	INC_STACK(p);
	p->sp->type = T_STRING;
	p->sp->u.string = str;
}

void
push_number(struct stack *p, int n)
{
#ifdef DEBUG
	CHECK_STACK_INC(p);
#endif
	INC_STACK(p);
	p->sp->type = T_NUMBER;
	p->sp->u.number = n;
}

void
push_pointer(struct stack *p, void *ptr)
{
#ifdef DEBUG
	CHECK_STACK_INC(p);
#endif
	INC_STACK(p);
	p->sp->type = T_POINTER;
	p->sp->u.pointer = ptr;
}

void
push_fpointer(struct stack *p, void (*ptr)())
{
#ifdef DEBUG
	CHECK_STACK_INC(p);
#endif
	INC_STACK(p);
	p->sp->type = T_FPOINTER;
	p->sp->u.fpointer = ptr;
}

void
dec_stack(struct stack *p)
{
	CHECK_STACK_DEC(p);
	DEC_STACK(p);
}

void
pop_stack(struct stack *p)
{
	CHECK_STACK_DEC(p);
	free_svalue(p->sp);
	DEC_STACK(p);
}

void
pop_n_elems(struct stack *p, int n)
{
#ifdef DEBUG
	if (n <= 0)
		fatal("pop_n_elems(%d)", n);
#endif
	while (n--)
		pop_stack(p);
}

void
dec_n_elems(struct stack *p, int n)
{
#ifdef DEBUG
	if (n <= 0)
		fatal("dec_n_elems(%d)", n);
#endif
	while (n--)
		dec_stack(p);
}

void
clean_stack(struct stack *p)
{
	while (p->el > 0)
                pop_stack(p);
}

void
init_stack(struct stack *p)
{
	p->el = 0;
	p->sp = &p->stack[-1];
}

struct svalue *
stack_svalue(struct stack *p, int el)
{
#ifdef DEBUG
	if (el > p->el)
		fatal("stack_svalue: request for non-existant stack element.");
#endif
	return &p->stack[el - 1];
}

