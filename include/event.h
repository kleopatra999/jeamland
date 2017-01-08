/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	event.h
 * Function:
 **********************************************************************/

struct event {
	char 		*fname;

	int		id;
	time_t		time;
	int		delay;
	void		(*func)(struct event *);

	struct stack 	stack;

	struct event 	*next;
	};

