/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	imud.c
 * Function:	Intermud stuff not specific to a protocol.
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "jeamland.h"

#if defined(IMUD3_SUPPORT) || defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT)

/*
 * Generic routine to find the best protocol with which to send a message
 * to a remote host.
 */
static struct imud_route *
request_route(char *host, char *req)
{
	static struct imud_route r;
	char spec;
#if defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT)
	struct host *h = (struct host *)NULL;
#endif
#ifdef IMUD3_SUPPORT
	struct i3_host *hh;
#endif

	if (*host == '^' && ((spec = host[1]) == 'i' ||
	    spec == 'c' || spec == '3'))
		host += 2;
	else
		spec = '\0';

#ifdef INETD_SUPPORT
	/* If it is a JeamLand host, use inetd */
	if ((spec == '\0' || spec == 'i') &&
	    (h = lookup_inetd_host(host)) != (struct host *)NULL &&
	    h->is_jeamland)
	{
		r.route = ROUTE_INETD;
		r.h.inetd_host = h;
		return &r;
	}
#endif

#ifdef IMUD3_SUPPORT
	if ((spec == '\0' || spec == '3') &&
	    (hh = lookup_i3_host(host, req)) != (struct i3_host *)NULL)
	{
		r.route = ROUTE_IMUD3;
		r.h.imud3_host = hh;
		return &r;
	}
#endif

#ifdef INETD_SUPPORT
	/* Have already looked up the host as an inetd... */
	if (h != (struct host *)NULL)
	{
		r.route = ROUTE_INETD;
		r.h.inetd_host = h;
		return &r;
	}
#endif

#ifdef CDUDP_SUPPORT
	if ((spec == '\0' || spec == 'c') &&
	    (h = lookup_cdudp_host(host)) != (struct host *)NULL)
	{
		r.route = ROUTE_CDUDP;
		r.h.inetd_host = h;
		return &r;
	}
#endif
	return (struct imud_route *)NULL;
}

#define GET_HOST(xx, req) \
	if ((r = request_route(xx, req)) == (struct imud_route *)NULL) \
		{ \
			write_socket(p, "Unknown or ambiguous host name.\n"); \
			return; \
		}

#ifdef IMUD3_SUPPORT
#define EXEC_IMUD3(ff, args) \
	case ROUTE_IMUD3: \
		ff args ; \
		write_socket(p, "Imud3: Request transmitted.\n"); \
		break
#else
#define EXEC_IMUD3(xx, yy)
#endif

#ifdef INETD_SUPPORT
#define EXEC_INETD(ff, args) \
	case ROUTE_INETD: \
		ff args ; \
		write_socket(p, "Inetd: Request transmitted.\n"); \
		break
#else
#define EXEC_INETD(xx, yy)
#endif

#ifdef CDUDP_SUPPORT
#define EXEC_CDUDP(ff, args) \
	case ROUTE_CDUDP: \
		ff args ; \
		write_socket(p, "Cdudp: Request transmitted.\n"); \
		break
#else
#define EXEC_CDUDP(xx, yy)
#endif

#define EXEC_DUMMY case ROUTE_DUMMY: break

void
imud_who(struct user *p, char *host)
{
	struct imud_route *r;

	GET_HOST(host, "who")

	switch (r->route)
	{
		EXEC_IMUD3(i3_cmd_who, (p, r->h.imud3_host));
		EXEC_INETD(inetd_who, (p, r->h.inetd_host));
		EXEC_CDUDP(cdudp_who, (p, r->h.inetd_host));
		EXEC_DUMMY;
	}
}

void
imud_tell(struct user *p, char *targ, char *msg)
{
	struct imud_route *r;
	char *h;

	if ((h = strchr(targ, '@')) == (char *)NULL)
		return;
	*h++ = '\0';

	GET_HOST(h, "tell")

	switch (r->route)
	{
		EXEC_IMUD3(i3_cmd_tell, (p, r->h.imud3_host, targ, msg));
		EXEC_INETD(inetd_tell, (p, r->h.inetd_host, targ, msg));
		EXEC_CDUDP(cdudp_tell, (p, r->h.inetd_host, targ, msg));
		EXEC_DUMMY;
	}
}

void
imud_finger(struct user *p, char *targ)
{
	struct imud_route *r;
	char *h;

	if ((h = strchr(targ, '@')) == (char *)NULL)
		return;
	*h++ = '\0';

	GET_HOST(h, "finger")

	switch (r->route)
	{
		EXEC_IMUD3(i3_cmd_finger, (p, r->h.imud3_host, targ));
		EXEC_INETD(inetd_finger, (p, r->h.inetd_host, targ));
		EXEC_CDUDP(cdudp_finger, (p, r->h.inetd_host, targ));
		EXEC_DUMMY;
	}
}

#ifdef NEWBIE_TUNEOUT
void
newbie_tuneout(struct user *p)
{
	extern void inetd_newbie_tuneout(struct user *);
	extern void i3_newbie_tuneout(struct user *);

	free_strlists(&p->chan_earmuff);

#if defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT)
	inetd_newbie_tuneout(p);
#endif

#ifdef IMUD3_SUPPORT
	i3_newbie_tuneout(p);
#endif
}
#endif /* NEWBIE_TUNEOUT */

#endif /* IMUD3_SUPPORT || INETD_SUPPORT || CDUDP_SUPPORT */

