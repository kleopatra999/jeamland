/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	stack.h
 * Function:	JeamLand variable stack system.
 **********************************************************************/

#define STACK_ELS	10
struct stack {
	struct svalue	stack[STACK_ELS];
	struct svalue 	*sp;
	int el;
	};

#define STACK_SIZE(xx) ((xx)->el)
#define STACK_EMPTY(xx)	(STACK_SIZE(xx) == 0)
#define STACK_FULL(xx) (STACK_SIZE(xx) >= STACK_ELS - 1)

