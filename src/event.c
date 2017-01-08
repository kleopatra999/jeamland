/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	event.c
 * Function:	Event handling
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#ifndef NeXT
#include <unistd.h>
#endif
#include <signal.h>
#include "jeamland.h"

#undef LOG_EVENTS

struct event *events = (struct event *)NULL;
struct event *current_event = (struct event *)NULL;
time_t next_event;
int event_id;
int event_count = 0;

extern int sysflags;
extern time_t current_time;

struct event *
create_event()
{
	struct event *ev = (struct event *)xalloc(sizeof(struct event),
	    "event struct");
	ev->fname = (char *)NULL;
	ev->delay = ev->id = 0;
	ev->time = 0;
	ev->func = (void (*)(struct event *))NULL;
	init_stack(&ev->stack);
	ev->next = (struct event *)NULL;
	return ev;
}

void
free_event(struct event *ev)
{
	clean_stack(&ev->stack);
	FREE(ev->fname);
	xfree(ev);
}

struct event *
find_event(int id)
{
	struct event *ev;

	for (ev = events; ev != (struct event *)NULL; ev = ev->next)
		if (ev->id == id)
			return ev;
	return (struct event *)NULL;
}

int
add_event(struct event *ev, void (*func)(struct event *), int delay,
    char *fname)
{
	struct event **e;

	if (delay <= 0)
		delay = 1;

	COPY(ev->fname, fname, "event fname");
	ev->delay = delay;
	ev->time = current_time + delay;
	ev->func = func;
	ev->id = ++event_id;

#ifdef LOG_EVENTS
	log_file("event", "Event %d(%s) started, %d", event_id, ev->fname,
	    delay);
#endif

	for (e = &events; *e != (struct event *)NULL; e = &((*e)->next))
		;
	*e = ev;

	if (!(sysflags & SYS_EVENT) &&
	    (next_event == -1 || current_time + delay < next_event))
	{
#ifdef LOG_EVENTS
		log_file("event", "(add) Alarm started: %d", delay);
#endif
		alarm((unsigned)delay);
		next_event = current_time + delay;
		current_event = ev;
	}
	return event_id;
}

static void
event_alarm(int sig)
{
	signal(SIGALRM, event_alarm);
#ifdef DEBUG
	if (sysflags & SYS_EVENT)
		fatal("SYS_EVENT set in event_alarm.");
#endif
	sysflags |= SYS_EVENT;
}

static void
start_events()
{
	struct event *ev, *t;
	time_t tm;
	int n;

	FUN_START("start_events");

#ifdef DEBUG
	if (sysflags & SYS_EVENT)
		fatal("start_events with pending event.");
#endif

	update_current_time();
	tm = current_time;

	for (n = 0, ev = events; ev != (struct event *)NULL; ev = ev->next)
		if (!n || ev->time < n)
		{
			t = ev;
			n = ev->time;
		}

	FUN_LINE;

	if (n)
	{
#ifdef LOG_EVENTS
		log_file("event", "(handle) Alarm started: %d", n - tm);
#endif
		next_event = n;
		current_event = t;

		/* Can this happen ? */
		if (n - tm <= 0)
			alarm((unsigned int)1);
		else
			alarm((unsigned int)(n - tm));
	}
	else
	{
#ifdef LOG_EVENTS
		log_file("event", "No more events.");
#endif
		current_event = (struct event *)NULL;
		next_event = -1;
	}

	FUN_END;
}

void
remove_event(int id)
{
	struct event **e, *ev;

	for (e = &events; *e != (struct event *)NULL; e = &((*e)->next))
		if ((*e)->id == id)
		{
#ifdef LOG_EVENTS
			log_file("event", "Removed event %d", id);
#endif
			ev = *e;
			*e = ev->next;
			if (ev == current_event)
			{
				current_event = (struct event *)NULL;
				alarm(0);
				/* If processing an event, leave this to the
				 * event processor. */
				if (!(sysflags & SYS_EVENT))
					start_events();
			}
			free_event(ev);
			return;
		}
}

void
remove_events_by_name(char *name)
{
	struct event **e, *ev, **ev_next;

	for (e = &events; *e != (struct event *)NULL; e = ev_next)
	{
		ev_next = &((*e)->next);

		if (!strcmp((*e)->fname, name))
		{
#ifdef LOG_EVENTS
			log_file("event", "Removed event %d", ev->id);
#endif
			ev = *e;
			*e = ev->next;
			ev_next = e;
			if (ev == current_event)
			{
				alarm(0);
				current_event = (struct event *)NULL;
			}
			free_event(ev);
		}
	}
	/* If processing an event, leave this to the event processor. */
	if (current_event == (struct event *)NULL && !(sysflags & SYS_EVENT))
		start_events();
}

void
handle_event()
{
	struct event *ev, *e;
	time_t tm;

	FUN_START("handle_event");

	update_current_time();
	tm = current_time;

	for (ev = events; ev != (struct event *)NULL; ev = e)
	{
		e = ev->next;
		if (ev->time <= tm)
		{
#ifdef LOG_EVENTS
			log_file("event", "Event %d(%s) occured.", ev->id,
			    ev->fname);
#endif
			event_count++;

			FUN_START("<event>");
			FUN_ARG(ev->fname);
			ev->func(ev);
			FUN_END;

			remove_event(ev->id);
		}
	}

	FUN_LINE;

	sysflags &= ~SYS_EVENT;

	start_events();

	FUN_END;
}

void
initialise_events()
{
	signal(SIGALRM, event_alarm);
	events = current_event = (struct event *)NULL;
	next_event = -1;
	event_id = 0;
}

