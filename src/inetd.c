/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	inetd.c
 * Function:	The intermud / cdudp / interjl system.
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#ifndef NeXT
#include <unistd.h>
#endif
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "jeamland.h"

#if defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT)


extern int port;
extern struct user *users;
extern time_t current_time, start_time;
extern char *host_name, *host_ip;

#ifdef INETD_SUPPORT
static void add_timeout(char *, unsigned long, char *, char *, char *);
struct host *hosts = (struct host *)NULL;
struct packet_field *lookup_field(struct packet *, char *);
#endif

#ifdef CDUDP_SUPPORT
struct host *cdudp_hosts = (struct host *)NULL;
#endif

#define TRSUCC	write_socket(p, "Request transmitted.\n")
#define CLEANUP_PACKET free_packet(pkt); xfree(msgt); return

#ifdef MALLOC_STATS
int	num_hosts, host_size;
#endif

struct pending *pendings = (struct pending *)NULL;

struct channel {
	char *name;
	int level;
	enum is { S_INETD, S_JLINETD, S_CDUDP } system;
	} channels[] = {
#ifdef INETD_SUPPORT
	/* Jeamland only channels */
	{ "interjl",		L_RESIDENT, 	S_JLINETD },
	{ "interjladmin",	L_CONSUL, 	S_JLINETD },
	/* Global channels */
	{ "intermud",		L_RESIDENT,	S_INETD },
	{ "intercode",		L_RESIDENT,	S_INETD },
	{ "d-chat",		L_WARDEN,	S_INETD },
	{ "intertest",		L_OVERSEER,	S_INETD },
	{ "interadmin",		L_CONSUL,	S_INETD },
#endif
	/* Global channels on cdudp */
#ifdef CDUDP_SUPPORT
	{ "CREATOR",		L_RESIDENT,	S_CDUDP },
	{ "HUNGARY",		L_WARDEN,	S_CDUDP },
#endif
	{ 0, 0 },
	};

int
inter_channel(char *channel, int lev)
{
	int i;

	for (i = 0; channels[i].name; i++)
		if (!strcmp(channels[i].name, channel) &&
		    channels[i].level <= lev)
			return 1;
	return 0;
}

#ifdef NEWBIE_TUNEOUT
void
inetd_newbie_tuneout(struct user *p)
{
	int i;

	for (i = 0; channels[i].name; i++)
		if (channels[i].level <= L_RESIDENT)
		{
			struct strlist *s;

			s = create_strlist("chanblock");
			s->str = string_copy(channels[i].name, "chanblock");
			add_strlist(&p->chan_earmuff, s, SL_SORT);
		}
}
#endif /* NEWBIE_TUNEOUT */

/* For debugging mainly */
#define Printf(xx, yy) if (yy != (char *)NULL) printf(xx, yy)
void
dump_packet(struct packet *p)
{
	struct packet_field *f;

	printf("Packet dump\n-----------\n");

	Printf("Host: %s\n", p->host);
	printf("Port: %d\n", p->port);
	Printf("Req:  %s\n", p->request);
	Printf("Snd:  %s\n", p->sender);
	Printf("Rcpt: %s\n", p->recipient);
	Printf("Name: %s\n", p->name);
	printf("ID:   %d\n", p->id);
	Printf("Data: %s\n", p->data);

	for (f = p->extra; f != (struct packet_field *)NULL; f = f->next)
	{
		printf("%s:\t", f->name);
		print_svalue((struct user *)NULL, &f->data);
		printf("\n");
	}
}

struct host *
create_host()
{
	struct host *h = (struct host *)xalloc(sizeof(struct host),
	    "*host struct");
#ifdef MALLOC_STATS
	host_size += sizeof(struct host);
#endif
	h->name = h->host = (char *)NULL;
	h->is_jeamland = 0;
	h->port = 0;
	h->status = h->last = 0;
	h->next = (struct host *)NULL;
	return h;
}

void
free_host(struct host *h)
{
	FREE(h->name);
	FREE(h->host);
	xfree(h);
}

void
parse_host_name(char *name)
{
	char *c;

	for (c = name; *c != '\0'; c++)
		if (isspace(*c) || !isprint(*c))
			*c = '.';
}

void
add_host(struct host **list, struct host *h)
{
	parse_host_name(h->name);

	for (; *list != (struct host *)NULL; list = &((*list)->next))
		if (strcmp((*list)->name, h->name) > 0)
			break;
	h->next = *list;
	*list = h;
}

int
remove_host_by_name(struct host **list, char *name)
{
	struct host **h, *i;

	for (h = list; *h != (struct host *)NULL; h = &((*h)->next))
	{
		if (!strcmp((*h)->name, name))
		{
			i = *h;
			*h = i->next;
			free_host(i);
			return 1;
		}
	}
	return 0;
}

static struct host *
lookup_host_by_ip(struct host *list, char *ip, int port)
{
	struct host *h;

	for (h = list; h != (struct host *)NULL; h = h->next)
		if (!strcmp(ip, h->host) && h->port == port)
			return h;
	return (struct host *)NULL;
}

static struct host *
lookup_host_by_name(struct host *list, char *name, int exact)
{
	struct host *h, *got;
	int flag = 0;

	for (h = list; h != (struct host *)NULL; h = h->next)
	{
		if (!strcmp(h->name, name))
			return h;
		else if (!exact && !strcasecmp(h->name, name))
			return h;
		else if (!exact &&
		    !strncasecmp(h->name, name, strlen(name)))
		{
			flag++;
			got = h;
		}
	}
	if (flag != 1)
		return (struct host *)NULL;
	return got;
}

#ifdef INETD_SUPPORT
struct host *
lookup_inetd_host(char *host)
{
	return lookup_host_by_name(hosts, host, 0);
}
#endif

#ifdef CDUDP_SUPPORT
struct host *
lookup_cdudp_host(char *host)
{
	return lookup_host_by_name(cdudp_hosts, host, 0);
}
#endif

void
save_hosts(struct host *list, char *fname)
{
	FILE *fp;
	struct host *h;

	if ((fp = fopen(fname, "w")) == (FILE *)NULL)
		return;

	for (h = list; h != (struct host *)NULL; h = h->next)
	{
		if (h->is_jeamland)
			fputc('!', fp);
		fprintf(fp, "%s:%s:%d:%ld\n", h->name, h->host,
		    h->port, (long)h->last);
	}
	fclose(fp);
}

void
load_hosts(int cdudp)
{
	FILE *fp;
	
	struct host *h, *hs, *newhosts, *list;
	char buf[BUFFER_SIZE];
	char host[BUFFER_SIZE], name[BUFFER_SIZE];
	int port, line = 0;
	time_t stime;
	char *nptr;

#ifdef CDUDP_SUPPORT
	if (cdudp)
	{
		if ((fp = fopen(F_CDUDP_HOSTS, "r")) == (FILE *)NULL)
			return;
	}
	else
#endif
	{
		if ((fp = fopen(F_INETD_HOSTS, "r")) == (FILE *)NULL)
			return;
	}

	newhosts = (struct host *)NULL;
#ifdef MALLOC_STATS
	num_hosts = host_size = 0;
#endif

	while (fgets(buf, sizeof(buf), fp) != (char *)NULL)
	{
		line++;
		if (ISCOMMENT(buf))
			continue;
		stime = 0;
		if (sscanf(buf, "%[^:]:%[^:]:%d:%ld", name, host, &port,
		    (long *)&stime) == 4 ||
		    sscanf(buf, "%[^:]:%[^:]:%d", name, host, &port) == 3)
		{
			if (!stime)
				stime = current_time;
			h = create_host();
#ifdef MALLOC_STATS
			num_hosts++;
#endif
			if (name[0] == '!')
			{
				h->is_jeamland = 1;
				COPY(h->name, name + 1, "*host name");
				nptr = name + 1;
#ifdef MALLOC_STATS
				host_size += strlen(name + 1);
#endif
			}
			else
			{
				COPY(h->name, name, "*host name");
				nptr = name;
#ifdef MALLOC_STATS
				host_size += strlen(name);
#endif
			}
			COPY(h->host, host, "*host ip");
#ifdef MALLOC_STATS
			host_size += strlen(host);
#endif
			h->port = port;
			h->last = stime;
			/* Retain existing status as long as IP and port
			 * are unchanged. */
#ifdef CDUDP_SUPPORT
			if (cdudp)
				hs = lookup_host_by_name(cdudp_hosts, nptr, 1);
#endif
#ifdef INETD_SUPPORT
			if (!cdudp)
				hs = lookup_host_by_name(hosts, nptr, 1);
#endif
			if (hs != (struct host *)NULL && hs->port == port
			    && !strcmp(hs->host, host))
				h->status = hs->status;
			add_host(&newhosts, h);
		}
		else
			log_file("error", "Error in hosts file at line %d",
			    line);
	}
	fclose(fp);
#ifdef CDUDP_SUPPORT
	if (cdudp)
		list = cdudp_hosts;
#endif
#ifdef INETD_SUPPORT
	if (!cdudp)
		list = hosts;
#endif

		for (h = list; h != (struct host *)NULL; h = hs)
		{
			hs = h->next;
			free_host(h);
		}

#ifdef CDUDP_SUPPORT
	if (cdudp)
		cdudp_hosts = newhosts;
#endif
#ifdef INETD_SUPPORT
	if (!cdudp)
		hosts = newhosts;
#endif
}

void
inetd_decode(struct svalue *s, char *q)
{
	int t;
	char buf[BUFFER_SIZE];

	if (q[0] == '$')
	{
		s->type = T_STRING;
		s->u.string = string_copy(q + 1, "inetd decode");
		return;
	}
	t = atoi(q);
	sprintf(buf, "%d", t);
	if (!strcmp(buf, q))
	{
		s->type = T_NUMBER;
		s->u.number = t;
		return;
	}
	s->type = T_STRING;
	s->u.string = string_copy(q, "inetd decode");
}

struct pending *
create_pending()
{
	struct pending *p = (struct pending *)xalloc(sizeof(struct pending),
	    "pending struct");

	p->name = p->packet = p->who = p->request = (char *)NULL;
	p->retries = p->id = 0;
	p->next = (struct pending *)NULL;
	return p;
}

void
free_pending(struct pending *p)
{
	FREE(p->name);
	FREE(p->packet);
	FREE(p->who);
	FREE(p->request);
	xfree(p);
}

struct pending *
find_pending(char *name, int id)
{
	struct pending *p;

	for (p = pendings; p != (struct pending *)NULL; p = p->next)
		if (!strcmp(name, p->name) && id == p->id)
			return p;
	return (struct pending *)NULL;
}

void
remove_pending(struct pending *p)
{
	struct pending **ptr;

	for (ptr = &pendings; *ptr != (struct pending *)NULL;
	    ptr = &((*ptr)->next))
	{
		if (*ptr == p)
		{
			*ptr = p->next;
			free_pending(p);
			return;
		}
	}
#ifdef DEBUG
	fatal("Remove pending: pending not in list.");
#endif
}

struct packet *
create_packet()
{
	struct packet *p = (struct packet *)xalloc(sizeof(struct packet),
	    "packet struct");

	p->host = p->request = p->sender = p->name = p->data =
	    p->recipient = (char *)NULL;
	p->port = p->id = 0;
	p->extra = (struct packet_field *)NULL;
	return p;
}

struct packet_field *
create_packet_field()
{
	struct packet_field *p = (struct packet_field *)xalloc(
	    sizeof(struct packet_field), "packet field struct");

	p->name = (char *)NULL;
	TO_EMPTY(p->data);
	p->next = (struct packet_field *)NULL;
	return p;
}

struct packet_field *
lookup_field(struct packet *p, char *field)
{
	struct packet_field *f;

	for (f = p->extra; f != (struct packet_field *)NULL; f = f->next)
		if (!strcmp(f->name, field))
			return f;
	return (struct packet_field *)NULL;
}

void
free_packet_field(struct packet_field *p)
{
	FREE(p->name);
	free_svalue(&p->data);
	xfree(p);
}

void
free_packet(struct packet *p)
{
	struct packet_field *f, *g;

	FREE(p->host);
	FREE(p->request);
	FREE(p->sender);
	FREE(p->name);
	FREE(p->data);
	FREE(p->recipient);
	for (f = p->extra; f != (struct packet_field *)NULL; f = g)
	{
		g = f->next;
		free_packet_field(f);
	}
	xfree(p);
}

void
deliver_channel_message(char *channel, int emote, struct packet *pkt)
{
	struct user *u;
	int level = L_OVERSEER;
	struct strbuf b;
	enum is sys = S_INETD;
	int i;

	for (i = 0; channels[i].name; i++)
		if (!strcasecmp(channels[i].name, channel))
		{
			level = channels[i].level;
			sys = channels[i].system;
			break;
		}

	init_strbuf(&b, 0, "deliv_chan_msg");

	if (emote)
	{
		sadd_strbuf(&b, "[%s:%s@%s ",
		    capitalise(channel), capitalise(pkt->sender),
		    pkt->name);
		add_strbuf(&b, pkt->data);
		cadd_strbuf(&b, ']');
	}
	else
	{
		sadd_strbuf(&b, "[%s:%s@%s] ",
		    capitalise(channel), capitalise(pkt->sender),
		    pkt->name);
		add_strbuf(&b, pkt->data);
	}

	for (u = users->next; u != (struct user *)NULL; u = u->next)
	{
		if (!IN_GAME(u) || u->level < level)
			continue;
		if (member_strlist(u->chan_earmuff, channel) !=
		    (struct strlist *)NULL)
			continue;

		switch (sys)
		{
		    case S_CDUDP:
			attr_colour(u, "cdudpchan");
			break;
		    case S_INETD:
			attr_colour(u, "inetchan");
			break;
		    case S_JLINETD:
			attr_colour(u, "jlchan");
			break;
		}
		write_socket(u, "%s", b.str);
		reset(u);
		write_socket(u, "\n");
	}

#ifdef INETD_CHANHIST
	if (sys == S_INETD || sys == S_JLINETD)
	{
		char fname[MAXPATHLEN + 1];

		sprintf(fname, "channel/inetd/%s", channel);
		log_file(fname, "%s", b.str + 2 +
		    strlen(channel));
	}
#endif
#ifdef CDUDP_CHANHIST
	if (sys == S_CDUDP)
	{
		char fname[MAXPATHLEN + 1];

		sprintf(fname, "channel/cdudp/%s", channel);
		log_file(fname, "%s", b.str);
	}
#endif

	free_strbuf(&b);
}

/* Check the host .. 
 * a lot of things to check...
 * order here is the same as in nostra's inetd.c */
void
check_host(struct packet *pkt, struct host **mlist, char *log, char *recv)
{
	struct host *h, *list;
	int is_jl;
#ifdef INETD_SUPPORT
	struct packet_field *f;

	is_jl = (f = lookup_field(pkt, "is_jeamland")) !=
	    (struct packet_field *)NULL && f->data.type == T_NUMBER &&
	    f->data.u.number == JL_MAGIC;
#endif

	list = *mlist;

	if ((h = lookup_host_by_name(list, pkt->name, 1)) !=
	    (struct host *)NULL)
	{
		if (strcmp(h->host, pkt->host))
		{
			if (!strcmp(pkt->name, LOCAL_NAME))
				log_file(log,
				    "*** FAKE HOST ***\n%s - %s\n", pkt->host,
				    recv);
			else
			{
				log_file(log,
				    "Host change:\n%s: %s -> %s\n",
				    h->name, h->host, pkt->host);
				COPY(h->host, pkt->host, "host ip");
			}
		}
		if (h->port != pkt->port)
		{
			if (!strcmp(pkt->name, LOCAL_NAME))
				log_file(log, "*** FAKE HOST ***\n%s - %s\n",
				    pkt->host, recv);
			else
			{
				log_file(log, "Port change:\n%s: %d -> %d\n",
				    h->name, h->port, pkt->port);
				h->port = pkt->port;
			}
		}
	}
	else
	{	/* Host is not in current table.. */
		if (!strcasecmp(pkt->name, LOCAL_NAME) &&
		    strcmp(pkt->host, host_ip)  &&
		    strcmp(pkt->host, "127.0.0.1") && pkt->port != port)
		{
			log_file(log, "*** FAKE HOST ***\n%s - %s\n",
			    pkt->host, recv);
			return;
		}
		else if ((h = lookup_host_by_ip(list, pkt->host, pkt->port)) !=
		    (struct host *)NULL)
		{
			log_file(log, "Name change:\n%s (%s) -> %s\n", h->name,
			    pkt->host, pkt->name);
			COPY(h->name, pkt->name, "host name");
		}
		else
		{
			log_file(log, "New host\n%s: %s %d\n", pkt->name,
			    pkt->host, pkt->port);
			h = create_host();
			COPY(h->name, pkt->name, "host name");
			COPY(h->host, pkt->host, "host ip");
			h->port = pkt->port;
			add_host(mlist, h);
		}
	}

	/* Is it another JeamLand talker ? */
	if (is_jl)
		h->is_jeamland = 1;

	/* We heard from it, mark it as up */
	h->status = h->last = current_time;
}


/* End of global stuff */


#ifdef INETD_SUPPORT

#define DICT_HOST	"Enulal"
struct split_packet *pending_splits = (struct split_packet *)NULL;
/* Yes, I know the grammar is incorrect. */
struct past_id *past_ids = (struct past_id *)NULL;
struct spool *mailspool = (struct spool *)NULL;
static int mailspool_id = -1;
static unsigned long packet_id = 0;

static void inetd_import1(struct user *, char *);
static void inetd_import2(struct user *, char *, char *, char *);
static int check_import_key(char *, char *, char *);

static char *inetd_cmds[] = {
	"ping",
	"reply",
	"tell",
	"who",
	"query",
	"channel",
	"locate",
	"finger",
	"mail",
	"dictr",
	"pwd-seed",
	"import",
	"import-data",
	0 };

static char *query_tab[] = {
	"commands",
	"email",
	"hosts",
	"inetd",
	"mud_port",
	"time",
	"version",
	"list",
	"info",
	"uptime",
	0 };

/* Packet encoding and decoding... */
char *
inetd_encode(char *str)
{
	int i;
	static char s[BUFFER_SIZE];

	i = atoi(str);
	sprintf(s, "%d", i);
	if (!strcmp(s, str))
		sprintf(s, "$%s", str);
	else
		strcpy(s, str);
	return s;
}

void
send_udp(char *host, int port, char *msg, unsigned long id)
{
	char sbuf[MAX_PACKET_LEN];
	char tbuf[MAX_PACKET_LEN];
	char *ptr;
	int total, current;

	if (strlen(msg) < MAX_PACKET_LEN)
	{
		send_udp_packet(host, port, msg);
		return;
	}
	total = strlen(msg) / FIT_PACKET_LEN + 1;
	for (current = 1, ptr = msg; strlen(ptr) > FIT_PACKET_LEN;
	    ptr += FIT_PACKET_LEN)
	{
		my_strncpy(tbuf, ptr, FIT_PACKET_LEN);
		sprintf(sbuf, "PKT:%s:%ld:%d/%d|%s", LOCAL_NAME, id, current++,
		    total, tbuf);
		send_udp_packet(host, port, sbuf);
	}
	sprintf(sbuf, "PKT:%s:%ld:%d/%d|%s", LOCAL_NAME, id, current++, total,
	    ptr);
	send_udp_packet(host, port, sbuf);
}

struct split_packet *
create_split_packet()
{
	struct split_packet *p = (struct split_packet *)xalloc(sizeof(
	    struct split_packet), "split packet struct");

	p->id = p->data = (char *)NULL;
	p->elem = p->total = 0;
	return p;
}

void
free_split_packet(struct split_packet *p)
{
	FREE(p->id);
	FREE(p->data);
	xfree(p);
}

struct past_id *
create_past_id()
{
	struct past_id *p = (struct past_id *)xalloc(sizeof(
	    struct past_id), "past id struct");
	p->name = (char *)NULL;
	p->id = 0;
	p->next = (struct past_id *)NULL;
	return p;
}

void
free_past_id(struct past_id *p)
{
	FREE(p->name);
	xfree(p);
}

struct past_id *
find_past_id(char *name, int id)
{
	struct past_id *p;

	for (p = past_ids; p != (struct past_id *)NULL; p = p->next)
		if (!strcmp(name, p->name) && id == p->id)
			return p;
	return (struct past_id *)NULL;
}

void
remove_past_id(struct past_id *p)
{
	struct past_id **r;

	for (r = &past_ids; *r != (struct past_id *)NULL; r = &((*r)->next))
	{
		if (*r == p)
		{
			*r = p->next;
			free_past_id(p);
			return;
		}
	}
#ifdef DEBUG
	fatal("remove_past_id: not in main list.");
#endif
}

void
remove_past_id_timeout(struct event *ev)
{
	remove_past_id((struct past_id *)ev->stack.sp->u.pointer);
}

void
add_past_id(char *name, int id)
{
	struct past_id *p = create_past_id();
	struct event *ev = create_event();

	COPY(p->name, name, "past id name");
	p->id = id;
	p->next = past_ids;
	past_ids = p;
	push_pointer(&ev->stack, (void *)p);
	add_event(ev, remove_past_id_timeout, PAST_ID_TIMEOUT, "pastid");
}

void
add_pending(char *name, int id, char *packet, char *who, char *req)
{
	struct pending *p = create_pending();
	
	COPY(p->name, name, "pending name");
	COPY(p->packet, packet, "pending packet");
	if (who != (char *)NULL)
	{
		COPY(p->who, who, "pending who");
	}
	COPY(p->request, req, "pending request");
	p->id = id;
	p->next = pendings;
	pendings = p;
}

struct spool *
create_spool()
{
	struct spool *s = (struct spool *)xalloc(sizeof(struct spool),
	    "spool struct");
	s->who = s->pkt = s->host = s->message = s->to = (char *)NULL;
	s->next = (struct spool *)NULL;
	return s;
}

void
free_spool(struct spool *s)
{
	FREE(s->who);
	FREE(s->pkt);
	FREE(s->host);
	FREE(s->message);
	FREE(s->to);
	xfree(s);
}

void
remove_mailspool(struct spool *s)
{
	struct spool **p;

	for (p = &mailspool; *p != (struct spool *)NULL; p = &((*p)->next))
		if (*p == s)
		{
			*p = s->next;
			free_spool(s);
			return;
		}
#ifdef DEBUG
	fatal("remove_mailspool: not in main list.");
#endif
}

void
handle_mailspool(struct event *ev)
{
	void save_mailspool(void);
	struct spool *s, *t;
	struct host *h;
	char *buf;

	for (s = mailspool; s != (struct spool *)NULL; s = t)
	{
		t = s->next;
		if (!--s->retries)
		{
			log_file("secure/imail", "Mail timed out (%s -> %s@%s)",
			    s->who, s->to, s->host);
			deliver_mail(s->who, ROOT_USER,
			    "Undelivered mail", s->message, (char *)NULL,
			    0, 1);
			remove_mailspool(s);
		}
		else if ((h = lookup_host_by_name(hosts, s->host, 1)) !=
		    (struct host *)NULL)
		{
			buf = (char *)xalloc(0x100 + strlen(s->pkt),
			    "mailspool tmp");

			sprintf(buf, "NAME:%s|UDP:%d|ID:%ld|%s", LOCAL_NAME,
			    INETD_PORT, ++packet_id, s->pkt);
			add_timeout(h->name, packet_id, buf, (char *)NULL,
			    "mail");
			send_udp(h->host, h->port, buf, packet_id);
			xfree(buf);
		}
	}
	save_mailspool();

	if (mailspool == (struct spool *)NULL)
	{
		if (ev == (struct event *)NULL)
			remove_event(mailspool_id);
		mailspool_id = -1;
	}
	else if (ev != (struct event *)NULL)
		mailspool_id = add_event(create_event(), handle_mailspool,
		    MAIL_RETRY, "mspool");
}

void
load_mailspool()
{
	FILE *fp;
	struct stat st;
	char *buf;

	log_file("syslog", "Loading mailspool.");

	mailspool = (struct spool *)NULL;

	if ((fp = fopen(F_MAILSPOOL, "r")) == (FILE *)NULL)
		return;

        if (fstat(fileno(fp), &st) == -1)
        {
                log_perror("fstat");
                fatal("Couldn't stat open file.");
        }

        if (!st.st_size)
        {
                unlink(F_MAILSPOOL);
                return;
        }

        buf = (char *)xalloc((size_t)st.st_size, "restore spool");
        while (fgets(buf, (int)st.st_size - 1, fp) != (char *)NULL)
        {
		if (!strncmp(buf, "mail ", 5))
		{
			struct svalue *sv;

			if ((sv = decode_one(buf, "decode_one mailspool")) ==
			    (struct svalue *)NULL)
				continue;

			/* who, to, pkt, host, msg, retries */
			if (sv->type == T_VECTOR && sv->u.vec->size == 6 &&
			    sv->u.vec->items[0].type == T_STRING &&
                            sv->u.vec->items[1].type == T_STRING &&
                            sv->u.vec->items[2].type == T_STRING &&
                            sv->u.vec->items[3].type == T_STRING &&
                            sv->u.vec->items[4].type == T_STRING &&
			    sv->u.vec->items[5].type == T_NUMBER)
			{
				struct spool *s = create_spool();

				s->who = string_copy(
				    sv->u.vec->items[0].u.string,
				    "mailspool who");
				s->to = string_copy(
				    sv->u.vec->items[1].u.string,
				    "mailspool to");
				s->pkt = string_copy(
				    sv->u.vec->items[2].u.string,
				    "mailspool pkt");
				s->host = string_copy(
				    sv->u.vec->items[3].u.string,
				    "mailspool host");
				s->message = string_copy(
				    sv->u.vec->items[4].u.string,
				    "mailspool msg");
				s->retries = sv->u.vec->items[5].u.number;
				s->next = mailspool;
				mailspool = s;
			}
			else
				log_file("error", "Error in mailspool line: "
				    "'%s'", buf);
			free_svalue(sv);
		}
	}
	fclose(fp);
	xfree(buf);
	if (mailspool != (struct spool *)NULL)
		mailspool_id = add_event(create_event(), handle_mailspool,
		    MAIL_RETRY, "mspool");
	else
		mailspool_id = -1;
}

void
save_mailspool()
{
        FILE *fp;
        struct spool *s;

	if (mailspool == (struct spool *)NULL)
	{
		unlink(F_MAILSPOOL);
		return;
	}

	if ((fp = fopen(F_MAILSPOOL, "w")) == (FILE *)NULL)
		return;
	fprintf(fp, "# who, to, pkt, host, msg, retries\n");
	for (s = mailspool; s != (struct spool *)NULL; s = s->next)
	{
		char *who = code_string(s->who);
		char *to = code_string(s->to);
		char *pkt = code_string(s->pkt);
		char *host = code_string(s->host);
		char *msg = code_string(s->message);
		fprintf(fp, "mail ({\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%d,})\n",
		    who, to, pkt, host, msg, s->retries);
		xfree(who);
		xfree(to);
		xfree(pkt);
		xfree(host);
		xfree(msg);
	}
	fclose(fp);
}

void
kill_mailspool(struct user *p, char *arg)
{
	struct spool *s, *t;
	int entry = -1;
	int num = 0;

	if (strlen(arg) && !(entry = atoi(arg)))
	{
		write_socket(p, "No such spool entry.\n");
		return;
	}

	for (s = mailspool; s != (struct spool *)NULL; s = t)
	{
		t = s->next;

		if (entry == -1) /* Just list spool entries. */
		{
			if (!strcmp(s->who, p->rlname) || ISROOT(p))
				write_socket(p, "[%3d] %s -> %s@%s  %d "
				    "remaining.\n", ++num, s->who, s->to,
				    s->host, s->retries);
			continue;
		}
		if (strcmp(s->who, p->rlname) && !ISROOT(p))
			continue;
		if (++num == entry)
		{
			remove_mailspool(s);
			save_mailspool();
			write_socket(p, "Removed entry %d\n", num);
			return;
		}
	}
	if (entry == -1)
	{
		if (!num)
			write_socket(p, "No entries.\n");
	}
	else
		write_socket(p, "Entry %d not found.\n", entry);
}

void
initialise_inetd()
{
	hosts = (struct host *)NULL;
	pendings = (struct pending *)NULL;
	pending_splits = (struct split_packet *)NULL;
	past_ids = (struct past_id *)NULL;

	load_mailspool();
}

struct split_packet *
find_part(char *id, int part)
{
	struct split_packet *p;

	for (p = pending_splits; p != (struct split_packet *)NULL; p = p->next)
		if (!strcasecmp(p->id, id) && (part == -1 || p->elem == part))
			return p;
	return (struct split_packet *)NULL;
}

void
remove_split_packet(struct split_packet *p)
{
	struct split_packet **q;

	for (q = &pending_splits; *q != (struct split_packet *)NULL;
	    q = &((*q)->next))
		if (*q == p)
		{
			*q = p->next;
			free_split_packet(p);
			return;
		}
#ifdef DEBUG
	fatal("remove_split_packet: packet not found.");
#endif
}

void
inter_parse(char *host, char *recv)
{
	char buf[BUFFER_SIZE];
	char *msg, *msgt, *p, *q, *pbuf;
	struct packet *pkt;
	struct packet_field *field;
	struct strbuf sb;
	int i;
	int repeat = 0;

	/* Check to see if the packet has been split into chunks..
	 * if it has, it will start with PKT:mudname:id:num/total| */
	if (!strncmp(recv, "PKT:", 4))
	{
		struct split_packet *spkt, *spkt2;
		char host_name[50];
		int num, total;

		if (sscanf(recv, "PKT:%[^:]:%d:%d/%d|", host_name, &i, &num,
		    &total) != 4)
		{
			log_file("inetd", "Bad partial packet:\n%s", recv);
			return;
		}
		if ((p = strchr(recv, '|')) == (char *)NULL)
		{
			log_file("inetd", "No data in split packet:\n%s", recv);
			return;
		}
		sprintf(buf, "%s:%d", host_name, i);
		/* Have we already got this bit ? */
		if (find_part(buf, num) != (struct split_packet *)NULL)
			return;

		spkt = create_split_packet();
		COPY(spkt->id, buf, "split packet id");
		spkt->elem = num;
		spkt->total = total;
		COPY(spkt->data, p + 1, "split data");
		spkt->next = pending_splits;
		pending_splits = spkt;

		init_strbuf(&sb, 0, "inetd_parse joining");
		
		/* Now, perhaps we have all the constituent parts.. */
		for (i = 1; i <= total; i++)
		{
			if ((spkt2 = find_part(spkt->id, i)) ==
			    (struct split_packet *)NULL)
			{
				free_strbuf(&sb);
				return;		/* Nopers */
			}
			add_strbuf(&sb, spkt2->data);
		}
		/* Yahoo! we've rebuilt the total packet.. */
		inter_parse(host, sb.str);

		/* Remove stuff from memory */
		while ((spkt2 = find_part(buf, -1)) !=
		    (struct split_packet *)NULL)
			remove_split_packet(spkt2);
		free_strbuf(&sb);
		return;
	}
		
	pkt = create_packet();
	COPY(pkt->host, host, "packet host");
	msgt = msg = string_copy(recv, "inetd packet copy");

	/* DATA is always at end if it exists and may contain delimiters,
	 * handle it specially and first! */
	if ((q = strstr(msg, "|DATA:")) != (char *)NULL)
	{
		if (q == msg)
		{
			log_file("inetd", "Bad packet from %s\n%s", host, recv);
			CLEANUP_PACKET;
		}
		*q = '\0';
		q += 6;
		if (*q == '$')
			q++;
		COPY(pkt->data, q, "packet data");
	}

	if ((p = strtok(msg, "|")) == (char *)NULL)
	{
		log_file("inetd", "Bad packet from %s\n%s", host, recv);
		CLEANUP_PACKET;
	}

	do
	{
		/* *poke FinalFrontier* */
		if (*p == '$' && p[1] != '\0')
			p++;

		/* Gawd, what a carry on.. */
#define INETD_CHECK(req, len, field, desc) if (!strncmp(p, req, len)) { \
		if (p[len] == '$')  p++; COPY(field, p + len, "packet" desc); }

		INETD_CHECK("REQ:", 4, pkt->request, "request")
		else INETD_CHECK("NAME:", 5, pkt->name, "name")
		else INETD_CHECK("SND:", 4, pkt->sender, "sender")
		else INETD_CHECK("RCPNT:", 6, pkt->recipient, "rcpnt")
#undef INETD_CHECK
		else if (!strncmp(p, "UDP:", 4))
			pkt->port = atoi(p + 4);
		else if (!strncmp(p, "ID:", 3))
			pkt->id = atoi(p + 3);
		else if ((q = strchr(p, ':')) != (char *)NULL)
		{
			*q = '\0';
			field = create_packet_field();
			COPY(field->name, p, "field name");
			inetd_decode(&field->data, ++q);
			field->next = pkt->extra;
			pkt->extra = field;
		}
	}
	while ((p = strtok((char *)NULL, "|")) != (char *)NULL);

	if (pkt->name == (char *)NULL || pkt->request == (char *)NULL)
	{
		log_file("inetd", "Bad packet from %s\n%s", host, recv);
		CLEANUP_PACKET;
	}

	parse_host_name(pkt->name);
	
	check_host(pkt, &hosts, "inetd_host", recv);

	/* Check to see if it is a repeat.. */
	if (strcmp(pkt->request, "reply"))
	{
		if (find_past_id(pkt->name, pkt->id) !=
		    (struct past_id *)NULL)
			repeat++;
		else
	    		add_past_id(pkt->name, pkt->id);
	}

	for (i = 0; inetd_cmds[i]; i++)
		if (!strcmp(inetd_cmds[i], pkt->request))
			break;

#ifdef DEBUG_UDP
#if 1
	/* who, query, finger, pwd-seed, import */
	if (!repeat && (i == 3 || i == 4 || i == 7 || i == 10 || i == 11))
#endif
		notify_level_wrt_flags(L_CONSUL, EAR_UDP,
		     "[ UDP - %s ]", recv);
#endif

	pbuf = (char *)NULL;
	switch (i)
	{
	    case 0: /* ping */
		if (pkt->sender != (char *)NULL)
			sprintf(buf, "REQ:reply|RCPNT:%s|ID:%d|is_jeamland:%d"
			    "|DATA:%s is alive.\n", pkt->sender, pkt->id,
			    JL_MAGIC, LOCAL_NAME);
		else
			sprintf(buf, "REQ:reply|ID:%d|is_jeamland:%d"
			    "|DATA:%s is alive.\n", pkt->id, JL_MAGIC,
			    LOCAL_NAME);
		pbuf = string_copy(buf, "interparse ping tmp");
		break;
	    case 1: /* reply */
	    {

		/* XXX: Possible recipient table cf. erqd */

		struct user *who;
		struct pending *p;

		if ((p = find_pending(pkt->name, pkt->id)) !=
		    (struct pending *)NULL)
			remove_pending(p);
		else
		{
			CLEANUP_PACKET;
		}

		if (pkt->recipient == (char *)NULL || pkt->data == (char *)NULL)
		{
			CLEANUP_PACKET;
		}
		if (!strcmp(pkt->recipient, "__LOCATOR__"))
		{
			struct packet_field *f, *g, *h;
			struct user *who;

			if ((f = lookup_field(pkt, "fnd"))  ==
			    (struct packet_field *)NULL ||
			    (g = lookup_field(pkt, "user")) ==
			    (struct packet_field *)NULL ||
			    (h = lookup_field(pkt, "vbs")) ==
			    (struct packet_field *)NULL ||
			    f->data.type != T_NUMBER ||
			    g->data.type != T_STRING ||
			    h->data.type != T_NUMBER ||
			    (who = find_user_absolute((struct user *)NULL,
			    g->data.u.string)) == (struct user *)NULL)
			{
				CLEANUP_PACKET;
			}
			if (f->data.u.number || h->data.u.number)
			{
				attr_colour(who, "incoming");
				write_socket(who, "%s", pkt->data);
				reset(who);
			}
			CLEANUP_PACKET;
		}
		else if (!strcmp(pkt->recipient, "__MAILER__"))
		{
			struct user *who;
			struct spool *s;
			struct packet_field *f, *g;

			if ((f = lookup_field(pkt, "udpm_writer")) ==
			    (struct packet_field *)NULL ||
			    (g = lookup_field(pkt, "udpm_status"))  ==
			    (struct packet_field *)NULL ||
			    f->data.type != T_STRING ||
			    g->data.type != T_NUMBER ||
			    pkt->data == (char *)NULL)
			{
				CLEANUP_PACKET;
			}
			switch ((int)g->data.u.number)
			{
			    case UDPM_STATUS_DELIVERED_OK:
				if ((who =
				    find_user_absolute((struct user *)NULL,
				    f->data.u.string)) != 
				    (struct user *)NULL)
					write_socket(who,
					    "\ninetd: Mail to %s@%s delivered"
					    " ok.\n", pkt->data, pkt->name);
				log_file("secure/imail",
				    "Mail successfully delivered"
				    " (%s -> %s@%s)", f->data.u.string,
				    pkt->data, pkt->name);
				break;
			    case UDPM_STATUS_UNKNOWN_USER:
				sprintf(buf, "Mailer@%s", pkt->name);
				deliver_mail(f->data.u.string, buf,
				    "Bounced Mail", pkt->data, (char *)NULL,
				    0, 1);
				log_file("secure/imail", "Mail failed (%s"
				    " -> @%s)\n\t%s", f->data.u.string,
				    pkt->name, pkt->data);
				break;
			}
			/* Remove message from mailspool */
			for (s = mailspool; s != (struct spool *)NULL;
			    s = s->next)
			{
				if (!strcmp(f->data.u.string, s->who) &&
				    !strcmp(pkt->name, s->host))
				{
					remove_mailspool(s);
					save_mailspool();
					break;
				}
			}
			CLEANUP_PACKET;
		}
		else if (!strcmp(pkt->recipient, "__IMPORT1__"))
		{
			struct packet_field *f, *g;
			struct user *who;

			if (pkt->data == (char *)NULL ||
			    (f = lookup_field(pkt, "user")) ==
			    (struct packet_field *)NULL ||
			    f->data.type != T_STRING ||
			    (g = lookup_field(pkt, "targ")) ==
			    (struct packet_field *)NULL ||
			    g->data.type != T_STRING)
			{
				log_file("inetd", "Bad pwd-seed reply packet"
				    " from %s\n%s\n", host, recv);
				CLEANUP_PACKET;
			}
			else if ((who = find_user_absolute((struct user *)NULL,
			    f->data.u.string)) == (struct user *)NULL)
			{
				CLEANUP_PACKET;

			}
			if (who->input_to != NULL_INPUT_TO)
			{
				write_socket(who,
				    "Cannot continue import request while in"
				    " an input_to.\n");
				CLEANUP_PACKET;
			}
			noecho(who);
			write_socket(who, "Enter password for %s@%s: ",
			    g->data.u.string, pkt->name);
			push_string(&who->stack, g->data.u.string);
			push_string(&who->stack, pkt->data);
			push_string(&who->stack, pkt->name);
			who->input_to = inetd_import1;
			CLEANUP_PACKET;
		}
		if ((who = find_user_absolute((struct user *)NULL,
		    pkt->recipient)) != (struct user *)NULL)
			write_socket(who, "\n%s\n", pkt->data);
		CLEANUP_PACKET;
	    }
	    case 2: /* tell */
	    {
		struct user *who;
		struct packet_field *f = lookup_field(pkt, "reply");
		int reply = f != (struct packet_field *)NULL &&
		    f->data.type == T_NUMBER && f->data.u.number == 1;

		if (pkt->recipient == (char *)NULL ||
	 	    pkt->data == (char *)NULL || pkt->sender == (char *)NULL)
		{
			log_file("inetd", "Bad tell packet from %s\n%s", host,
			    recv);
			CLEANUP_PACKET;
		}

		who = find_user((struct user *)NULL,
		    lower_case(pkt->recipient));

		if (who == (struct user *)NULL ||
		    (who->saveflags & U_INVIS))
		{
			sprintf(buf,
			    "REQ:reply|RCPNT:%s|ID:%d|DATA:"
			    "Root@%s tells you: No such user, %s on %s.\n",
			    pkt->sender, pkt->id, LOCAL_NAME,
			    capitalise(pkt->recipient), LOCAL_NAME);
			pbuf = string_copy(buf, "interparse tell tmp");
			if (who != (struct user *)NULL && !repeat)
			{
				attr_colour(who, "incoming");
				write_socket(who,
				    "%s@%s is unaware of telling you: %s",
				    capitalise(pkt->sender), pkt->name,
				    pkt->data);
				reset(who);
				write_socket(who, "\n");
				sprintf(buf, "%s@%s", pkt->sender, pkt->name);
				COPY(who->reply_to, buf, "reply");
			}
		}
		else
		{
			/* Visitors cannot use interhost tells */
			if (who->level < L_RESIDENT)
			{
				sprintf(buf,
				    "REQ:reply|RCPNT:%s|ID:%d|DATA:"
				    "Root@%s tells you: User %s does not have"
				    " access to interhost tells.\n",
				    pkt->sender, pkt->id, LOCAL_NAME,
				    who->name);
				pbuf = string_copy(buf, "interparse tell tmp");
			}
			else
			{
				/* Don't use buf here,
				 * data could be massive! */
				init_strbuf(&sb, 0, "inter_parse tell rep");
				sadd_strbuf(&sb, "REQ:reply|RCPNT:%s|ID:%d|"
				    "DATA:You tell %s@%s: ", pkt->sender,
				    pkt->id, who->name, LOCAL_NAME);
				add_strbuf(&sb, pkt->data);
				cadd_strbuf(&sb, '\n');
				if (who->flags & U_AFK)
				{
					sadd_strbuf(&sb, "Warning: %s is AFK",
					    who->name);
					if (who->stack.sp->type == T_STRING)
						sadd_strbuf(&sb, ": %s",
						    who->stack.sp->u.string);
					cadd_strbuf(&sb, '\n');
				}
				pop_strbuf(&sb);
				pbuf = sb.str;

				if (!repeat)
				{
					attr_colour(who, "incoming");
					write_socket(who, "%s@%s %s: %s",
					    capitalise(pkt->sender), pkt->name,
					    reply ? "replies" : "tells you",
					    pkt->data);
					reset(who);
					write_socket(who, "\n");
					sprintf(buf, "%s@%s", pkt->sender,
					    pkt->name);
					COPY(who->reply_to, buf, "reply");
				}
			}
		}
		break;
	    }
	    case 3: /* who */
	    {
		char *t;

		if (pkt->sender == (char *)NULL)
		{
			CLEANUP_PACKET;
		}
		t = who_text(FINGER_INETD);
		init_strbuf(&sb, 0, "interparse tmp who");
		sadd_strbuf(&sb, "REQ:reply|ID:%d|RCPNT:%s|DATA:", pkt->id,
		    pkt->sender);
		add_strbuf(&sb, t);
		cadd_strbuf(&sb, '\n');
		pop_strbuf(&sb);
		pbuf = sb.str;
		xfree(t);
		break;
	    }
	    case 4: /* query */
	    {
		int i;

		if (pkt->data == (char *)NULL || pkt->sender == (char *)NULL)
		{
			CLEANUP_PACKET;
		}
		init_strbuf(&sb, 0, "interparse query tmp");
		sadd_strbuf(&sb, "REQ:reply|ID:%d|RCPNT:%s|DATA:", pkt->id,
		    pkt->sender);
		for (i = 0; query_tab[i]; i++)
			if (!strcmp(query_tab[i], pkt->data))
				break;
		switch (i)
		{
		    case 0: /* commands */
			for (i = 0; inetd_cmds[i]; i++)
			{
				add_strbuf(&sb, inetd_cmds[i]);
				cadd_strbuf(&sb, ':');
			}
			break;
		    case 1: /* email */
			add_strbuf(&sb, OVERSEER_EMAIL);
			break;
		    case 2: /* hosts */
			/* XXX - todo */
			add_strbuf(&sb,
			    "Host queries currently unimplemented.");
			break;
		    case 3: /* inetd */
			add_strbuf(&sb, INETD_VERSION);
			break;
		    case 4: /* mud_port */
			sadd_strbuf(&sb, "%d", port);
			break;
		    case 5: /* time */
			add_strbuf(&sb, nctime(&current_time));
			break;
		    case 6: /* version */
			add_strbuf(&sb, "JeamLand "VERSION);
			break;
		    case 7: /* list */
			for (i = 0; query_tab[i]; i++)
			{
				if (!strcmp(query_tab[i], "list"))
					continue;
				add_strbuf(&sb, query_tab[i]);
				cadd_strbuf(&sb, ':');
			}
			break;
		    case 8: /* info */
			sadd_strbuf(&sb,
			    "-----------------------------------------\n"
			    " TALKER NAME  : %s\n"
			    " IPNUMBER     : %s\n"
			    " IPNAME       : %s\n"
			    " MAIN PORT    : %d\n"
			    " UDP PORT     : %d\n"
			    " DRIVER       : Jeamland %s\n"
			    " INETD        : %s\n"
			    " EMAIL        : %s\n"
			    "-----------------------------------------\n",
			    LOCAL_NAME, host_ip, host_name, port, INETD_PORT,
			    VERSION, INETD_VERSION, OVERSEER_EMAIL);
			break;
		    case 9: /* uptime */
			add_strbuf(&sb, conv_time(current_time - start_time));
			cadd_strbuf(&sb, '\n');
			break;
		    default:
			break;
			/* Ignore the request.. */
		}
		pop_strbuf(&sb);
		pbuf = sb.str;
		break;
	    }
	    case 5: /* channel */
	    {
		struct packet_field *f, *g;

		if ((f = lookup_field(pkt, "channel")) == 
		    (struct packet_field *)NULL)
			f = lookup_field(pkt, "CHANNEL");
		if ((g = lookup_field(pkt, "cmd")) ==
		    (struct packet_field *)NULL)
			g = lookup_field(pkt, "CMD");

		if (pkt->sender == (char *)NULL || pkt->data == (char *)NULL ||
		    f == (struct packet_field *)NULL ||
		    f->data.type != T_STRING)
		{
			CLEANUP_PACKET;
		}
		if (!repeat)
			deliver_channel_message(f->data.u.string, g !=
			    (struct packet_field *)NULL && g->data.type
			    == T_STRING && !strcmp(g->data.u.string, "emote"),
			    pkt);
		/* Channels have a piddly reply packet.. 
		 * While this is in the intermud protocol description, it is
		 * currently only used by the inter-JeamLand channels. */
		sprintf(buf, "REQ:reply|ID:%d", pkt->id);
		COPY(pbuf, buf, "channel tmp");
		break;
	    }
	    case 6: /* locate */
	    {
		struct user *who;
		struct packet_field *f, *g;

		if (pkt->data == (char *)NULL ||
		    pkt->sender == (char *)NULL ||
		    (f = lookup_field(pkt, "user")) ==
		    (struct packet_field *)NULL ||
		    (g = lookup_field(pkt, "vbs")) ==
		    (struct packet_field *)NULL ||
		    f->data.type != T_STRING ||
		    g->data.type != T_NUMBER)
		{
			CLEANUP_PACKET;
		}
		who = find_user_absolute((struct user *)NULL, pkt->data);
		sprintf(buf, "REQ:reply|ID:%d|RCPNT:%s|vbs:%ld|user:%s|"
		    "fnd:%d|DATA:locate@%s: %s ", pkt->id, pkt->sender,
		    g->data.u.number, f->data.u.string,
		    who != (struct user *)NULL, LOCAL_NAME,
		    capitalise(pkt->data));
		if (who != (struct user *)NULL && !strcmp(who->rlname,
		    pkt->data))
		{
			attr_colour(who, "notify");
			write_socket(who,
			    "inetd: %s@%s is looking for you.",
			    capitalise(f->data.u.string), pkt->name);
			reset(who);
			write_socket(who, "\n");
			if (!(who->saveflags & U_INVIS))
			{
				if (who->title != (char *)NULL)
					strcat(buf, who->title);
				else
					strcat(buf, "is logged on.");
				strcat(buf, "\n");
			}
			else
				strcat(buf, "is not logged on.\n");
		}
		pbuf = string_copy(buf, "locate tmp");
		break;
	    }
	    case 7: /* finger */
	    {
		char *fing;

		if (pkt->sender == (char *)NULL || pkt->data == (char *)NULL)
		{
			CLEANUP_PACKET;
		}
		fing = finger_text((struct user *)NULL, lower_case(pkt->data),
		    FINGER_INETD);
		init_strbuf(&sb, 0, "finger tmp");
		sadd_strbuf(&sb, "REQ:reply|ID:%d|RCPNT:%s|DATA:", pkt->id,
		    pkt->sender);
		add_strbuf(&sb, fing);
		pop_strbuf(&sb);
		pbuf = sb.str;
		xfree(fing);
		break;
	    }
	    case 8: /* mail */
	    {
		struct packet_field *f, *g, *h, *i, *tmp;
		struct vecbuf grupe;
		struct vector *v;
		int failed = 0;
		int rec;

		f = lookup_field(pkt, "udpm_writer");
		g = lookup_field(pkt, "udpm_subject");
		h = lookup_field(pkt, "udpm_status");
		i = lookup_field(pkt, "udpm_spool_name");
		if (pkt->data == (char *)NULL || pkt->recipient ==
		    (char *)NULL ||
		    f == (struct packet_field *)NULL ||
		    g == (struct packet_field *)NULL ||
		    f->data.type != T_STRING ||
		    g->data.type != T_STRING)
		{
			CLEANUP_PACKET;
		}

		if ((tmp = lookup_field(pkt, "udpm_rec")) !=
		    (struct packet_field *)NULL && tmp->data.type == T_NUMBER)
			rec = tmp->data.u.number;
		else
			rec = 0;

		init_vecbuf(&grupe, 0, "inetd mail grupe");
		if (pkt->recipient[0] == '#')
		{
			if (!expand_grupe((struct user *)NULL, &grupe,
			    pkt->recipient + 1))
				failed = 1;
		}
		else if (!exist_user(lower_case(pkt->recipient)))
			failed = 1;
		else
		{
			/* Ok to do this with an _empty_ vecbuf.. */
			grupe.offset++;
			grupe.vec->items[0].type = T_STRING;
			grupe.vec->items[0].u.string = string_copy(
			    pkt->recipient, "inetd mail tmpr");
		}

		if (failed)
		{
			sprintf(buf,
			    "REQ:reply|ID:%d|RCPNT:%s|udpm_status:%d|"
			    "udpm_writer:%s|udpm_spool_name:%s|"
			    "DATA: Reason: Unknown %s %s.\n",
			    pkt->id, pkt->sender, UDPM_STATUS_UNKNOWN_USER,
			    f->data.u.string, i->data.type == T_STRING ?
			    i->data.u.string : "0",
			    pkt->recipient[0] == '#' ? "grupe" : "user",
			    capitalise(pkt->recipient));
			COPY(pbuf, buf, "mail temp");
			free_vecbuf(&grupe);
			break;
		}

		sprintf(buf, "%s@%s", f->data.u.string, pkt->name);
		v = vecbuf_vector(&grupe);
		if (!repeat)
		{
			int i;

			/* If we can deliver to just one of the
			 * recipients, consider it a success!
			 * Most of the time, there will be only one anyway.
			 */
			failed = 1;
			for (i = v->size; i--; )
			{
				if (v->items[i].type != T_STRING)
					continue;
				if (deliver_mail(lower_case(
				    v->items[i].u.string), buf,
				    g->data.u.string, pkt->data,
				    (char *)NULL, rec, 0))
					failed = 0;
			}
		}
		if (failed)
		{
			/* Don't like the UNKNOWN_USER bit..
			 * don't look at me, I didn't design the mail
			 * protocol! :) */
			sprintf(buf,
			    "REQ:reply|ID:%d|RCPNT:%s|udpm_status:%d|"
			    "udpm_writer:%s|udpm_spool_name:%s|"
			    "DATA: Reason: Problem delivering mail to %s.\n",
			    pkt->id, pkt->sender,
			    UDPM_STATUS_UNKNOWN_USER,
			    f->data.u.string, i->data.type == T_STRING ?
			    i->data.u.string : "0",
			    capitalise(pkt->recipient));
			COPY(pbuf, buf, "mail temp");
			free_vector(v);
			break;
		}
		sprintf(buf, "REQ:reply|ID:%d|RCPNT:%s|udpm_status:%d|"
		    "udpm_writer:%s|udpm_spool_name:%s|DATA:%s",
		    pkt->id, pkt->sender, UDPM_STATUS_DELIVERED_OK,
		    f->data.u.string, i->data.type == T_STRING ?
		    i->data.u.string : "0", pkt->recipient);
		COPY(pbuf, buf, "mail temp");
		free_vector(v);
		break;
	    }
	    case 9:	/* dictr */
	    {
		struct user *who;

		if (pkt->recipient == (char *)NULL ||
                    pkt->data == (char *)NULL || pkt->sender == (char *)NULL)
                {
                        log_file("inetd", "Bad dictr packet from %s\n%s", host,
                            recv);
                        CLEANUP_PACKET;
                }

		if (!repeat)
		{
			who = find_user_absolute((struct user *)NULL,
			    lower_case(pkt->recipient));

			if (who != (struct user *)NULL)
			{
				write_socket(who,
				    "Dictionary reply:\n%s\n", pkt->data);
			}
		}
		sprintf(buf, "REQ:reply|ID:%d", pkt->id);
		pbuf = string_copy(buf, "dictr tmp");
		break;
	    }
	    case 10:	/* pwd-seed */
	    {
		struct user *who;
		struct packet_field *f;

		if (pkt->data == (char *)NULL ||
		    pkt->sender == (char *)NULL ||
		    (f = lookup_field(pkt, "user")) ==
		    (struct packet_field *)NULL ||
		    f->data.type != T_STRING)
                {
                        log_file("inetd", "Bad pwd-seed packet from %s\n%s",
			    host, recv);
                        CLEANUP_PACKET;
                }

		if ((who = doa_start((struct user *)NULL,
		    lower_case(pkt->data))) == (struct user *)NULL)
			sprintf(buf, "REQ:reply|ID:%d|RCPNT:%s|"
			    "DATA:No such user, %s@%s", pkt->id,
			    f->data.u.string, pkt->data, LOCAL_NAME);
		else if (who->passwd == (char *)NULL)
			sprintf(buf, "REQ:reply|ID:%d|RCPNT:%s|"
			    "DATA:%s@%s has no password.", pkt->id,
			    f->data.u.string, pkt->data, LOCAL_NAME);
		else
			sprintf(buf, "REQ:reply|ID:%d|RCPNT:%s|user:%s|targ:%s"
			    "|DATA:%c%c",
			    pkt->id, pkt->sender, f->data.u.string,
			    pkt->data,
			    who->passwd[0], who->passwd[1]);

		if (who != (struct user *)NULL)
			doa_end(who, 0);
		pbuf = string_copy(buf, "pwd-seed tmp");
		break;
	    }
	    case 11:	/* import */
	    {
		struct user *who;
		struct packet_field *f, *g;

		if (pkt->data == (char *)NULL ||
		    pkt->sender == (char *)NULL ||
		    (f = lookup_field(pkt, "user")) ==
		    (struct packet_field *)NULL ||
		    f->data.type != T_STRING ||
		    (g = lookup_field(pkt, "key")) ==
		    (struct packet_field *)NULL ||
		    g->data.type != T_STRING)
                {
                        log_file("inetd", "Bad import packet from %s\n%s",
			    host, recv);
                        CLEANUP_PACKET;
                }

		if ((who = doa_start((struct user *)NULL,
		    lower_case(f->data.u.string))) == (struct user *)NULL)
			sprintf(buf, "REQ:reply|ID:%d|RCPNT:%s|"
			    "DATA:No such user, %s@%s", pkt->id,
			    pkt->sender, f->data.u.string, LOCAL_NAME);
		else if (who->passwd == (char *)NULL ||
		    strcmp(who->passwd, pkt->data))
			sprintf(buf, "REQ:reply|ID:%d|RCPNT:%s|"
			    "DATA:Password incorrect for %s@%s", pkt->id,
			    pkt->sender, f->data.u.string, LOCAL_NAME);
		else
		{
			sprintf(buf, "REQ:reply|ID:%d|RCPNT:%s|"
			    "DATA:Password accepted, import complete.",
			    pkt->id, pkt->sender);

			inetd_import2(who, pkt->sender, pkt->name,
			    g->data.u.string);
		}
		if (who != (struct user *)NULL)
			doa_end(who, 0);
		pbuf = string_copy(buf, "import tmp");
		break;
	    }
	    case 12:	/* import-data */
	    {
		struct packet_field *f, *g, *h;
		struct user *p;

		if (pkt->data == (char *)NULL ||
		    pkt->recipient == (char *)NULL ||
		    (f = lookup_field(pkt, "key")) ==
		    (struct packet_field *)NULL ||
		    f->data.type != T_STRING ||
		    (g = lookup_field(pkt, "akey")) ==
		    (struct packet_field *)NULL ||
		    g->data.type != T_STRING ||
		    (h = lookup_field(pkt, "type")) ==
		    (struct packet_field *)NULL ||
		    h->data.type != T_STRING)
		
		{
                        log_file("inetd", "Bad import-data packet from %s\n%s",
			    host, recv);
                        CLEANUP_PACKET;
		}

		if ((p = find_user_absolute((struct user *)NULL,
		    lower_case(pkt->recipient))) == (struct user *)NULL)
		{
			CLEANUP_PACKET;
		}

		if (!check_import_key(p->rlname, pkt->name, f->data.u.string))
		{
			log_file("illegal", "Import-data request from "
			    "%s (%s)\n%s\n", host, f->data.u.string, recv);
                        CLEANUP_PACKET;
		}

		if (!repeat)
		{
			if (!strcmp(h->data.u.string, "alias"))
			{
				extern void f_alias(struct user *,
				    int, char **);
				char *argv[3];

				argv[0] = "alias";
				argv[1] = g->data.u.string;
				argv[2] = pkt->data;
				f_alias(p, 3, argv);
			}
			else if (!strcmp(h->data.u.string, "attr"))
			{
				extern void f_attr(struct user *,
				    int, char **);
				char *argv[3];

				argv[0] = "attr";
				argv[1] = g->data.u.string;
				argv[2] = pkt->data;
				f_attr(p, 3, argv);
			}
			else if (!strcmp(h->data.u.string, "prompt"))
			{
				COPY(p->prompt, pkt->data, "prompt");
			}
		}
		sprintf(buf, "REQ:reply|ID:%d", pkt->id);
		pbuf = string_copy(buf, "import-data tmp");
		break;
	    }
	    default:
		log_file("inetd", "Unknown request from %s\n%s", host, recv);
		sprintf(buf,
		    "REQ:reply|RCPNT:%s|ID:%d|DATA:%s request failed at %s.\n",
		    pkt->sender, pkt->id, pkt->request, LOCAL_NAME);
		COPY(pbuf, buf, "interparse tmp");
	}
	sprintf(buf, "NAME:%s|UDP:%d|", LOCAL_NAME, INETD_PORT);
	xfree(msgt);
	msg = (char *)xalloc(strlen(buf) + strlen(pbuf) + 5, "interparse ftmp");
	sprintf(msg, "%s%s", buf, pbuf);
	xfree(pbuf);
	send_udp(pkt->host, pkt->port, msg, pkt->id);
	free_packet(pkt);
	xfree(msg);
}

void
handle_timeout(struct event *ev)
{
	struct event *e;
	struct pending *p;
	struct user *u;
	struct host *h;
	struct split_packet *spkt;
	char buf[BUFFER_SIZE];

	if ((p = find_pending((ev->stack.sp - 1)->u.string,
	    ev->stack.sp->u.number)) != (struct pending *)NULL)
	{
		if ((h = lookup_host_by_name(hosts, p->name, 1)) ==
		    (struct host *)NULL)
		{
			remove_pending(p);
			return;
		}
		if (++p->retries > RETRIES)
		{
			if (p->who != (char *)NULL &&
			    (u = find_user_absolute((struct user *)NULL,
			    p->who)) !=
			    (struct user *)NULL && strcmp(p->request, "locate"))
			{
				if (!strcmp(p->request, "mail"))
					write_socket(u,
					    "inetd: mail request to %s timed"
					    " out. Mail placed in spool.\n",
					    p->name);
				else
					write_socket(u,
				    	    "inetd: %s request to %s timed"
					    " out.\n",
				    	    capitalise(p->request), p->name);
			}
			h->status = -current_time;

			/* Remove any split packet bits. */
			sprintf(buf, "%s:%d", p->name, p->id);
			while ((spkt = find_part(buf, -1)) !=
		    	    (struct split_packet *)NULL)
				remove_split_packet(spkt);

			remove_pending(p);
			return;
		}
		e = create_event();
		push_malloced_string(&e->stack, (ev->stack.sp - 1)->u.string);
		push_number(&e->stack, ev->stack.sp->u.number);
		/* dec to avoid freeing above string.. */
		dec_n_elems(&ev->stack, 2);
		send_udp(h->host, h->port, p->packet, p->id);
		add_event(e, handle_timeout, REPLY_TIMEOUT, "inetd");
	}
}

static void
add_timeout(char *name, unsigned long id, char *packet, char *who, char *req)
{
	struct event *e = create_event();

	add_pending(name, id, packet, who, req);
	push_string(&e->stack, name);
	push_number(&e->stack, id);
	add_event(e, handle_timeout, REPLY_TIMEOUT, "inetd");
}

void inetd_ping_host(struct user *, char *, int, int);

void
ping_all()
{
	struct host *h;
	char buf[BUFFER_SIZE];

	for (h = hosts; h != (struct host *)NULL; h = h->next)
	{
		sprintf(buf, "NAME:%s|UDP:%d|ID:%ld|REQ:ping|is_jeamland:%d",
		    LOCAL_NAME, INETD_PORT, ++packet_id, JL_MAGIC);
		add_timeout(h->name, packet_id, buf, (char *)NULL, "ping");
		send_udp(h->host, h->port, buf, packet_id);
	}
	/* Ping myself, to add myself to jlhosts */
	sprintf(buf, "NAME:%s|UDP:%d|ID:%ld|REQ:ping|SND:%s|is_jeamland:%d",
	    LOCAL_NAME, INETD_PORT, ++packet_id, "__SYSTEM__", JL_MAGIC);
	send_udp(host_ip, INETD_PORT, buf, packet_id);
}

void
inetd_dict(struct user *p, char *word)
{
	struct host *h;
	char buf[BUFFER_SIZE];

	if ((h = lookup_host_by_name(hosts, DICT_HOST, 1)) ==
	    (struct host *)NULL)
	{
		write_socket(p,
		    "Dictionary host '%s' not in your host table.\n",
		    DICT_HOST);
		return;
	}
	sprintf(buf,
	    "NAME:%s|UDP:%d|ID:%ld|REQ:dict|SND:%s|DATA:%s",
	    LOCAL_NAME, INETD_PORT, ++packet_id, p->capname,
	    inetd_encode(word));
	add_timeout(h->name, packet_id, buf, p->rlname, "dict");
	send_udp(h->host, h->port, buf, packet_id);
	TRSUCC;
}

void
inetd_channel(struct user *p, char *channel, char *message, int jl)
{
	char buf[BUFFER_SIZE];
	struct host *h;
	int emote = 0;
	struct packet pkt;

#ifdef INETD_CHANHIST
	if (*message == '!')
	{
		dump_file(p, "log/channel/inetd", channel, DUMP_TAIL);
		return;
	}
#endif

	if (*message == '@')
	{
		write_socket(p, "Remote channel who is not implemented on "
		    "the inetd network.\n");
		return;
	}

	if (message[0] == ':')
		emote++, message++;
	else if (message[0] == '.' && message[1] != '.')
	{
		char *q;
		char *argv[2];
		int argc;

		argc = 1;
		if ((q = strchr(++message, ' ')) != (char *)NULL)
		{
			*q++ = '\0';
			argc = 2;
			argv[1] = q;
		}
		argv[0] = message;

		if ((q = expand_feeling(p, argc, argv, 0)) == (char *)NULL ||
		    q == (char *)1)
		{
			write_socket(p, "Error in channel feeling, %s.\n",
			    argv[0]);
			return;
		}
		if ((message = strchr(q, ' ')) == (char *)NULL)
		{
			write_socket(p, "Error in channel feeling, %s.\n",
			    argv[0]);
			return;
		}
		message++;
		emote++;
	}

	pkt.sender = p->capname;
	pkt.name = LOCAL_NAME;
	pkt.data = message;
	deliver_channel_message(channel, emote, &pkt);

	for (h = hosts; h != (struct host *)NULL; h = h->next)
	{
		if (jl && !h->is_jeamland)
			continue;
		if (strcmp(h->name, LOCAL_NAME))
		{
			sprintf(buf,
			    "NAME:%s|UDP:%d|ID:%ld|channel:%s|"
			    "REQ:channel|%sSND:%s|DATA:$%s",
			    LOCAL_NAME, INETD_PORT, ++packet_id,
			    channel, emote ? "cmd:emote|" : "",
			    p->name, message);
			send_udp(h->host, h->port, buf, packet_id);
			if (jl)
				add_timeout(h->name, packet_id, buf,
				    "__SYSTEM__", "jlchan");
		}
	}
}

void
inetd_mail(char *rlname, char *to, char *subject, char *data, int rec)
{
	/*struct user *p;*/
	struct host *h;
	struct spool *s;
	char *buf = (char *)xalloc(BUFFER_SIZE + strlen(data),
	    "inetd mail tmp");
	char *buf2 = (char *)xalloc(BUFFER_SIZE + strlen(data),
	    "inetd mail tmp2");
	char *user, *host;

	user = string_copy(to, "inetd mail tmp");
	host = strchr(user, '@');
	*host = '\0';
	host++;
	if ((h = lookup_host_by_name(hosts, host, 0)) == (struct host *)NULL)
	{
		xfree(user);
		xfree(buf);
		xfree(buf2);
		return;
	}
	sprintf(buf, "REQ:mail|RCPNT:%s|SND:__MAILER__|"
	    "udpm_status:0|udpm_writer:%s|udpm_subject:%s|"
	    "udpm_rec:%d|udpm_spool_name:0|"
	    "DATA:%s", user, rlname, subject, rec, data);
	s = create_spool();
	COPY(s->who, rlname, "mailspool who");
	COPY(s->pkt, buf, "mailspool pkt");
	COPY(s->host, h->name, "mailspool host");
	COPY(s->to, lower_case(user), "mailspool to");
	s->message = (char *)xalloc(0x100 + strlen(data) +
	    strlen(subject) + strlen(h->name) + strlen(user), "mailspool msg");
	sprintf(s->message, "Reason: Error in connection to remote site %s.\n\n"
	    "INCLUDED MESSAGE FOLLOWS :-\n\n"
	    "To: %s\n"
	    "Subject: %s\n\n%s", h->name, user, subject, data);
	s->retries = MAIL_RETRIES;
	s->next = mailspool;
	mailspool = s;
	save_mailspool();
	if (mailspool_id == -1)
		mailspool_id = add_event(create_event(), handle_mailspool,
		    MAIL_RETRY, "mspool");

	sprintf(buf2, "NAME:%s|UDP:%d|ID:%ld|%s", LOCAL_NAME, INETD_PORT,
	    ++packet_id, buf);
	add_timeout(h->name, packet_id, buf2, rlname, "mail");
	send_udp(h->host, h->port, buf2, packet_id);
	xfree(buf);
	xfree(buf2);
}

/* 5 simultaneous "import" users... should be sufficient. */
#define IMPORT_KEY_SIZE	5
static struct {
	char *user;
	char *host;
	char key[7];
	int event;
	} import_keys[IMPORT_KEY_SIZE];

static void
remove_import_key(struct event *ev)
{
	int i = ev->stack.sp->u.number;

#ifdef DEBUG
	if (import_keys[i].user == (char *)NULL)
		fatal("remove_import_key: User null.");
#endif

	xfree(import_keys[i].user);
	import_keys[i].user = (char *)NULL;
	xfree(import_keys[i].host);
}

static char *
set_import_key(char *user, char *host)
{
	static int initted = 0;
	struct event *ev;
	int i;

	if (!initted)
	{
		for (i = IMPORT_KEY_SIZE; i--; )
			import_keys[i].user = (char *)NULL;
		initted = 1;
	}

	for (i = IMPORT_KEY_SIZE; i--; )
		if (import_keys[i].user != (char *)NULL &&
		    !strcmp(import_keys[i].user, user))
		{
			xfree(import_keys[i].user);
			xfree(import_keys[i].host);
			remove_event(import_keys[i].event);
			break;
		}

	if (i < 0)
		for (i = IMPORT_KEY_SIZE; i--; )
			if (import_keys[i].user == (char *)NULL)
				break;

	if (i < 0)
	{
		log_file("error", "Import stack overflow.");
		return (char *)NULL;
	}
	import_keys[i].user = string_copy(user, "import_stack user");
	import_keys[i].host = string_copy(host, "import_stack host");
	strcpy(import_keys[i].key, random_password(6));

	ev = create_event();
	push_number(&ev->stack, i);
	import_keys[i].event = add_event(ev, remove_import_key, 300, "import");
	return import_keys[i].key;
}

static int
check_import_key(char *user, char *host, char *key)
{
	int i;

	for (i = IMPORT_KEY_SIZE; i--; )
		if (import_keys[i].user != (char *)NULL &&
		    !strcmp(import_keys[i].user, user))
			break;

	if (i < 0)
		return 0;

	return !strcmp(import_keys[i].host, host) &&
	    !strcmp(import_keys[i].key, key);
}

static void
inetd_import2(struct user *p, char *user, char *host, char *key)
{
	char buf[BUFFER_SIZE];
	struct host *h;
	struct alias *a;

	if ((h = lookup_host_by_name(hosts, host, 0)) == (struct host *)NULL)
		return;

	/* Send off aliases.. */
	for (a = p->alias; a != (struct alias *)NULL; a = a->next)
	{
		sprintf(buf, "NAME:%s|UDP:%d|ID:%ld|REQ:import-data|"
		    "RCPNT:%s|type:alias|key:%s|akey:%s|DATA:$%s",
		    LOCAL_NAME, INETD_PORT, ++packet_id, user, key,
		    a->key, a->fob);
		add_timeout(h->name, packet_id, buf, (char *)NULL,
		    "import-data");
		send_udp(h->host, h->port, buf, packet_id);
	}
	/* Send off attributes.. */
	if (p->attr != (struct mapping *)NULL)
	{
		int i;

		for (i = p->attr->size; i--; )
		{
			sprintf(buf, "NAME:%s|UDP:%d|ID:%ld|REQ:import-data|"
			    "RCPNT:%s|type:attr|key:%s|akey:%s|DATA:$%s",
			    LOCAL_NAME, INETD_PORT, ++packet_id, user, key,
			    p->attr->indices->items[i].u.string,
			    p->attr->values->items[i].u.string);
			add_timeout(h->name, packet_id, buf, (char *)NULL,
			    "import-data");
			send_udp(h->host, h->port, buf, packet_id);
		}
	}
	/* Send prompt */
	if (p->prompt != (char *)NULL)
	{
		sprintf(buf, "NAME:%s|UDP:%d|ID:%ld|REQ:import-data|"
		    "RCPNT:%s|type:prompt|key:%s|akey:prompt|DATA:$%s",
		    LOCAL_NAME, INETD_PORT, ++packet_id, user, key,
		    p->prompt);
		add_timeout(h->name, packet_id, buf, (char *)NULL,
		    "import-data");
		send_udp(h->host, h->port, buf, packet_id);
	}
}

static void
inetd_import1(struct user *p, char *c)
{
	char buf[BUFFER_SIZE];
	struct host *h;
	char *key;

	echo(p);
	p->input_to = NULL_INPUT_TO;
	write_socket(p, "\n");

	if ((h = lookup_host_by_name(hosts, p->stack.sp->u.string, 0)) ==
	    (struct host *)NULL)
	{
		write_socket(p, "Host %s has vanished!\n",
		    p->stack.sp->u.string);
		pop_n_elems(&p->stack, 3);
		return;
	}
	if ((key = set_import_key(p->rlname, h->name)) == (char *)NULL)
	{
		write_socket(p,
		    "Too many people are currently using 'import'.\n"
		    "Please try again later.\n");
		pop_n_elems(&p->stack, 3);
		return;
	}

	sprintf(buf, "NAME:%s|UDP:%d|ID:%ld|REQ:import|SND:%s|user:%s|key:%s|"
	    "DATA:%s",
	    LOCAL_NAME, INETD_PORT, ++packet_id, p->capname,
	    (p->stack.sp - 2)->u.string, key,
	    crypt(c, (p->stack.sp - 1)->u.string));
	add_timeout(h->name, packet_id, buf, p->rlname, "import");
	send_udp(h->host, h->port, buf, packet_id);
	write_socket(p, "Import request sent.\n");
	pop_n_elems(&p->stack, 3);
}

void
inetd_import(struct user *p, char *user, char *host)
{
	char buf[BUFFER_SIZE];
	struct host *h;

	if (*host == '^')
		host++;
	if ((h = lookup_host_by_name(hosts, host, 0)) == (struct host *)NULL)
	{
		write_socket(p, "Unknown or ambiguous host name.\n");
		return;
	}
	sprintf(buf, "NAME:%s|UDP:%d|ID:%ld|REQ:pwd-seed|SND:__IMPORT1__|"
	    "user:%s|DATA:%s", LOCAL_NAME, INETD_PORT, ++packet_id,
	    p->capname, user);
	add_timeout(h->name, packet_id, buf, p->rlname, "password seed");
	send_udp(h->host, h->port, buf, packet_id);
	write_socket(p, "Password seed request sent, please wait.\n");
}

void
inetd_who(struct user *p, struct host *h)
{
	char buf[BUFFER_SIZE];

	sprintf(buf, "NAME:%s|UDP:%d|ID:%ld|REQ:who|SND:%s", LOCAL_NAME,
	    INETD_PORT, ++packet_id, p->capname);
	add_timeout(h->name, packet_id, buf, p->rlname, "who");
	send_udp(h->host, h->port, buf, packet_id);
}

void
inetd_tell(struct user *p, struct host *h, char *targ, char *msg)
{
	char buf[BUFFER_SIZE];

	sprintf(buf, "NAME:%s|UDP:%d|ID:%ld|REQ:tell|SND:%s|RCPNT:%s|DATA:$%s",
	    LOCAL_NAME, INETD_PORT, ++packet_id, p->capname,
	    targ, msg);
	add_timeout(h->name, packet_id, buf, p->rlname, "tell");
	send_udp(h->host, h->port, buf, packet_id);
}

void
inetd_finger(struct user *p, struct host *h, char *targ)
{
	char buf[BUFFER_SIZE];

	sprintf(buf, "NAME:%s|UDP:%d|ID:%ld|REQ:finger|SND:%s|DATA:$%s",
	    LOCAL_NAME, INETD_PORT, ++packet_id, p->capname, targ);
	add_timeout(h->name, packet_id, buf, p->rlname, "finger");
	send_udp(h->host, h->port, buf, packet_id);
}

#endif /* INETD_SUPPORT */



#ifdef CDUDP_SUPPORT

static char *cdudp_cmds[] = {
	"ping_q",
	"gtell",
	"rwho_a",
	"affirmation_a",
	"gfinger_q",
	"gfinger_a",
	"rwho_q",
	"ping_a",
	"mudlist_q",
	"startup",
	"gwizmsg",
	"support_q",
	"locate_a",
	"locate_q",
	"shutdown",
	0 };

void
cdudp_parse(char *host, char *recv)
{
	struct packet *pkt;
	struct packet_field *field;
	int i;
	char *msgt;
	char buf[BUFFER_SIZE];
	char *p, *q;

	if (strncmp(recv, "@@@", 3))
		return;
	msgt = string_copy(recv + 3, "cdudp_parse");
	if ((p = strstr(msgt, "@@@")) == (char *)NULL)
	{
		xfree(msgt);
		return;
	}
	*p = '\0';
	if ((p = strrchr(recv, '\n')) != (char *)NULL)
		*p = '\0';
	pkt = create_packet();
	COPY(pkt->host, host, "packet host");
	if ((p = strstr(msgt, "||")) == (char *)NULL)
	{
		/* Simple command.. */
		COPY(pkt->request, msgt, "packet request");
	}
	else
	{
		*p = '\0';
		COPY(pkt->request, msgt, "packet request");
		p += 2;
		if ((p = strtok(p, "||")) == (char *)NULL)
        	{
                	log_file("cdudp", "Bad packet from %s\n%s", host, recv);
			CLEANUP_PACKET;
        	}
        	do
        	{
			if (!strncmp(p, "MSG:", 4))
			{
				TCOPY(pkt->data, p + 4, "packet data");
			}
                	if (!strncmp(p, "NAME:", 5))
                	{
				TCOPY(pkt->name, p + 5, "packet name");
                	}
			else if (!strncmp(p, "ASKWIZ:", 7))
			{
				TCOPY(pkt->sender, p + 7, "packet sender");
			}
			else if (!strncmp(p, "WIZFROM:", 8))
			{
				TCOPY(pkt->sender, p + 8, "packet sender");
			}
			else if (!strncmp(p, "WIZNAME:", 8))
			{
				TCOPY(pkt->sender, p + 8, "packet sender");
			}
			else if (!strncmp(p, "WIZTO:", 6))
			{
				TCOPY(pkt->recipient, p + 6, "packet rcpnt");
			}
			else if (!strncmp(p, "PORTUDP:", 8))
				pkt->port = atoi(p + 8);
			else if ((q = strchr(p, ':')) != (char *)NULL)
			{
				*q = '\0';
				field = create_packet_field();
				COPY(field->name, p, "field name");
				inetd_decode(&field->data, ++q);
				field->next = pkt->extra;
				pkt->extra = field;
			}
		}
		while ((p = strtok((char *)NULL, "||")) != (char *)NULL);
	}

	if (pkt->name == (char *)NULL || pkt->request == (char *)NULL)
	{
		log_file("cdudp", "Bad packet from %s\n%s", host, recv);
		CLEANUP_PACKET;
	}

	parse_host_name(pkt->name);

	check_host(pkt, &cdudp_hosts, "cdudp_host", recv);

	for (i = 0; cdudp_cmds[i]; i++)
		if (!strcmp(cdudp_cmds[i], pkt->request))
			break;

#ifdef DEBUG_UDP
#if 1
	if (i == 4 || i == 6)
#endif
		notify_level_wrt_flags(L_CONSUL, EAR_UDP,
		     "[ UDP - %s ]", recv);
#endif

	switch (i)
	{
	    case 0: /* ping_q */
	    {
		char tim[30];

		strcpy(tim, ctime(&current_time));
		*strchr(tim, '\n') = '\0';
		sprintf(buf, "@@@ping_a||VERSION:JeamLand %s||"
		    "MUDLIB:N/A||DRIVER:JeamLand %s||"
		    "HOST:%s||TIME:%s||PORT:%d||NAME:%s||PORTUDP:%d@@@",
		    VERSION, VERSION, host_name, tim, port, LOCAL_NAME,
		    CDUDP_PORT);
		send_udp_packet(pkt->host, pkt->port, buf);
		break;
	    }
	    case 1: /* gtell */
	    {
		struct user *who;

                if (pkt->recipient == (char *)NULL ||
                    pkt->data == (char *)NULL || pkt->sender == (char *)NULL)
                {
                        log_file("cdudp", "Bad tell packet from %s\n%s", host,
                            recv);
                        CLEANUP_PACKET;
                }

                who = find_user((struct user *)NULL,
                    lower_case(pkt->recipient));

                if (who == (struct user *)NULL ||
                    (who->saveflags & U_INVIS))
                {
                        sprintf(buf,
			    "@@@affirmation_a||NAME:%s||PORTUDP:%d||WIZTO:%s||"
			    "TYPE:gtell||"
			    "WIZFROM:Root@%s||MSG:No such user, %s on %s.\n"
			    "@@@", LOCAL_NAME, CDUDP_PORT, pkt->sender,
			    LOCAL_NAME, capitalise(pkt->recipient),
			    LOCAL_NAME);
                        if (who != (struct user *)NULL)
                        {
				char buf2[BUFFER_SIZE];

				attr_colour(who, "incoming");
                                write_socket(who,
                                    "\n%s@%s is unaware of telling you: %s",
                                    capitalise(pkt->sender), pkt->name,
                                    pkt->data);
                                reset(who);
				write_socket(who, "\n");
				/* Don't really like putting this here.. */
				sprintf(buf2, "%s@%s", pkt->sender, pkt->name);
				COPY(who->reply_to, buf2, "reply");
                        }
                }
                else
                {
			if (who->level < L_RESIDENT)
			{
				sprintf(buf,
				    "@@@affirmation_a||NAME:%s||PORTUDP:%d||"
				    "WIZTO:%s||TYPE:gtell||"
				    "WIZFROM:Root@%s||MSG:User %s does not"
				    " have access to interhost tells.\n@@@",
				    LOCAL_NAME, CDUDP_PORT, pkt->sender,
				    LOCAL_NAME, capitalise(pkt->recipient));
			}
			else
			{
				/* Don't really like putting this here.. */
				sprintf(buf, "%s@%s", pkt->sender, pkt->name);
				COPY(who->reply_to, buf, "reply");
				sprintf(buf,
				    "@@@affirmation_a||NAME:%s||PORTUDP:%d||"
				    "WIZTO:%s||TYPE:gtell||WIZFROM:Root@%s||"
				    "MSG:You tell %s@%s: %s\n@@@",
				    LOCAL_NAME, CDUDP_PORT, pkt->sender,
				    LOCAL_NAME, capitalise(pkt->recipient),
				    LOCAL_NAME, pkt->data);
				attr_colour(who, "incoming");
				write_socket(who, "\n%s@%s tells you: %s",
				    capitalise(pkt->sender), pkt->name,
				    pkt->data);
				reset(who);
				write_socket(who, "\n");
			}
		}
		send_udp_packet(pkt->host, pkt->port, buf);
		break;
	    }
	    case 2: /* rwho_a */
	    {
		struct user *who;
		struct packet_field *f;

		if (pkt->sender == (char *)NULL ||
		    (f = lookup_field(pkt, "RWHO")) ==
		    (struct packet_field *)NULL ||
		    f->data.type != T_STRING ||
		    (who = find_user_absolute((struct user *)NULL,
		    pkt->sender)) == (struct user *)NULL)
			break;
		write_socket(who, "\n%s", f->data.u.string);
		break;
	    }
	    case 3: /* affirmation_a */
	    {
		struct user *who;

		if (pkt->sender == (char *)NULL ||
		    pkt->recipient == (char *)NULL ||
		    pkt->data == (char *)NULL ||
		    (who = find_user_absolute((struct user *)NULL,
		    pkt->recipient)) == (struct user *)NULL)
			break;
		write_socket(who, "\n%s tells you: %s\n", pkt->sender,
		    pkt->data);
		break;
	    }
	    case 4: /* gfinger_q */
	    {
		struct packet_field *f;
		char *fing, *t;

		if (pkt->sender == (char *)NULL ||
		    (f = lookup_field(pkt, "PLAYER")) ==
		    (struct packet_field *)NULL ||
		    f->data.type != T_STRING)
			break;
		fing = finger_text((struct user *)NULL,
		    lower_case(f->data.u.string), FINGER_INETD);
		t = (char *)xalloc(BUFFER_SIZE + strlen(fing),
		    "gfinger_q tmp");
		sprintf(t, "@@@gfinger_a||NAME:%s||PORTUDP:%d||ASKWIZ:%s||"
		    "MSG:%s@@@", LOCAL_NAME, CDUDP_PORT, pkt->sender, fing);
		send_udp_packet(pkt->host, pkt->port, t);
		xfree(t);
		xfree(fing);
		break;
	    }
	    case 5: /* gfinger_a */
	    {
		struct user *who;
		
		if (pkt->sender == (char *)NULL ||
		    pkt->data == (char *)NULL ||
		    (who = find_user_absolute((struct user *)NULL,
		    pkt->sender)) == (struct user *)NULL)
			break;
		write_socket(who, "\n%s", pkt->data);
		break;
	    }
	    case 6: /* rwho_q */
	    {
		char *t, *tm;

		if (pkt->sender == (char *)NULL)
			break;
		t = who_text(FINGER_INETD);
		tm = (char *)xalloc(BUFFER_SIZE + strlen(t), "rwho_q tmp");
		sprintf(tm, "@@@rwho_a||NAME:%s||PORTUDP:%d||ASKWIZ:%s||"
		    "RWHO:%s@@@", LOCAL_NAME, CDUDP_PORT, pkt->sender,
		    t);
		send_udp_packet(pkt->host, pkt->port, tm);
		xfree(tm);
		xfree(t);
		break;
	    }
	    case 10: /* gwizmsg */
	    {
		struct packet_field *f, *g, *h;

		if ((g = lookup_field(pkt, "CHANNEL")) ==
		    (struct packet_field *)NULL ||
		    g->data.type != T_STRING ||
		    (h = lookup_field(pkt, "GWIZ")) ==
		    (struct packet_field *)NULL ||
		    h->data.type != T_STRING)
			break;
		COPY(pkt->data, h->data.u.string, "gwizmsg tmp");
		f = lookup_field(pkt, "EMOTE");
		deliver_channel_message(g->data.u.string, f !=
		    (struct packet_field *)NULL && f->data.type == T_NUMBER
		    && f->data.u.number == 1, pkt);
		break;
	    }
	    case 12: /* locate_a */
	    {
		struct user *who;
		struct packet_field *f, *g;

		if ((f = lookup_field(pkt, "LOCATE")) ==
		    (struct packet_field *)NULL ||
		    f->data.type != T_STRING ||
		    (g = lookup_field(pkt, "TARGET")) ==
		    (struct packet_field *)NULL ||
		    g->data.type != T_STRING ||
		    (who = find_user_absolute((struct user *)NULL,
		    pkt->sender)) == (struct user *)NULL)
			break;

		if (!strcmp(f->data.u.string, "YES"))
			write_socket(who, "%s located on %s.\n",
			    capitalise(g->data.u.string), pkt->name);
		break;
	    }
	    case 13: /* locate_q */
	    {
		struct user *who;
		struct packet_field *f;
		int here = 0;

		if ((f = lookup_field(pkt, "TARGET")) ==
		    (struct packet_field *)NULL ||
		    f->data.type != T_STRING)
			break;

		if ((who = find_user_absolute((struct user *)NULL,
		    f->data.u.string)) != (struct user *)NULL)
		{
			attr_colour(who, "notify");
			write_socket(who, "cdudp: %s@%s is looking for you.",
			    capitalise(pkt->sender), pkt->name);
			reset(who);
			write_socket(who, "\n");
			if (!(who->saveflags & U_INVIS))
				here = 1;
		}

		sprintf(buf, "@@@locate_a||NAME:%s||PORTUDP:%d||LOCATE:%s||"
		    "TARGET:%s||ASKWIZ:%s@@@", LOCAL_NAME, CDUDP_PORT,
		    here ? "YES" : "NO", f->data.u.string,
		    pkt->sender);
		send_udp_packet(pkt->host, pkt->port, buf);
		break;
	    }
	    case 7: /* ping_a */
	    {
		struct pending *pn;
		struct user *p;
		struct packet_field *f;

		if ((pn = find_pending(pkt->name, -2)) ==
		    (struct pending *)NULL)
			break;
		if (pn->who == (char *)NULL ||
		    !strcmp(pn->who, "__SYSTEM__") ||
		    (p = find_user_absolute((struct user *)NULL, pn->who)) ==
		    (struct user *)NULL)
		{
			remove_pending(pn);
			break;
		}
		if ((f = lookup_field(pkt, "DRIVER")) !=
		    (struct packet_field *)NULL)
		{
			write_socket(p, "\nDRIVER:  ");
			print_svalue(p, &f->data);
		}
		if ((f = lookup_field(pkt, "VERSION")) !=
		    (struct packet_field *)NULL)
		{
			write_socket(p, "\nVERSION: ");
			print_svalue(p, &f->data);
		}
		if ((f = lookup_field(pkt, "MUDLIB")) !=
		    (struct packet_field *)NULL)
		{
			write_socket(p, "\nMUDLIB:  ");
			print_svalue(p, &f->data);
		}
		write_socket(p, "\nHOST IP: (string)%s", pkt->host);
		if ((f = lookup_field(pkt, "HOST")) !=
		    (struct packet_field *)NULL)
		{
			write_socket(p, "\nHOST:    ");
			print_svalue(p, &f->data);
		}
		if ((f = lookup_field(pkt, "PORT")) !=
		    (struct packet_field *)NULL)
		{
			write_socket(p, "\nPORT:    ");
			print_svalue(p, &f->data);
		}
		write_socket(p, "\nPORTUDP: (int)%d", pkt->port);
		if ((f = lookup_field(pkt, "TIME")) !=
		    (struct packet_field *)NULL)
		{
			write_socket(p, "\nTIME:    ");
			print_svalue(p, &f->data);
		}
		write_socket(p, "\n");
		remove_pending(pn);
		break;
	    }
	    case 8: /* mudlist_q */
	    case 9: /* startup */
	    case 11: /* support_q */
	    case 14: /* shutdown */
		break;
	    default:
		log_file("cdudp", "Unknown request from %s\n%s", host, recv);
		break;
	}
	CLEANUP_PACKET;
}

void
ping_all_cdudp()
{
	struct host *h;
	char buf[BUFFER_SIZE];
	void add_pinga(char *, char *);

	sprintf(buf, "@@@ping_q||NAME:%s||PORTUDP:%d@@@", LOCAL_NAME,
	    CDUDP_PORT);
	for (h = cdudp_hosts; h != (struct host *)NULL; h = h->next)
	{
		add_pinga(h->name, "__SYSTEM__");
		send_udp_packet(h->host, h->port, buf);
	}
}

void
ping_all_cdudp_event(struct event *ev)
{
	ping_all_cdudp();
}

void
pinga_timeout(struct event *ev)
{
	struct pending *pn;
	struct host *h;

	if ((pn = find_pending(ev->stack.sp->u.string, -2)) ==
	    (struct pending *)NULL)
		return;
	if ((h = lookup_host_by_name(cdudp_hosts, pn->name, 1)) !=
	    (struct host *)NULL)
		h->status = -current_time;
	remove_pending(pn);
}

void
add_pinga(char *name, char *user)
{
	struct event *ev = create_event();
	struct pending *pn = create_pending();

	COPY(pn->name, name, "pending name");
	COPY(pn->who, user, "pending who");
	pn->id = -2;
	pn->next = pendings;
	pendings = pn;
	push_string(&ev->stack, name);
	add_event(ev, pinga_timeout, PAST_ID_TIMEOUT, "ping_a");
}

void
cdudp_channel(struct user *p, char *channel, char *message)
{
        char buf[BUFFER_SIZE];
	struct packet pkt;
        struct host *h;
        int emote = 0;
	char *q;

#ifdef CDUDP_CHANHIST
	if (*message == '!')
	{
		dump_file(p, "log/channel/cdudp", channel, DUMP_TAIL);
		return;
	}
#endif

	if (*message == '@')
	{
		write_socket(p, "Remote channel who is not implemented on "
		    "the cdudp network.\n");
		return;
	}

        if (message[0] == ':')
                emote++, message++;

	for (q = channel; *q != '\0'; q++)
		*q = toupper(*q);

	pkt.sender = p->capname;
	pkt.name = LOCAL_NAME;
	pkt.data = message;
	deliver_channel_message(channel, emote, &pkt);

	sprintf(buf, "@@@gwizmsg||NAME:%s||PORTUDP:%d||WIZNAME:%s||GWIZ:%s||"
	    "CHANNEL:%s@@@", LOCAL_NAME, CDUDP_PORT, p->capname, message,
	    channel);
        for (h = cdudp_hosts->next; h != (struct host *)NULL; h = h->next)
                if (strcmp(h->name, LOCAL_NAME))
                        send_udp_packet(h->host, h->port, buf);
}

void
cdudp_who(struct user *p, struct host *h)
{
	char buf[BUFFER_SIZE];

	sprintf(buf, "@@@rwho_q||NAME:%s||PORTUDP:%d||ASKWIZ:%s@@@",
	    LOCAL_NAME, CDUDP_PORT, p->capname);
	send_udp_packet(h->host, h->port, buf);
}

void
cdudp_tell(struct user *p, struct host *h, char *targ, char *msg)
{
	char buf[BUFFER_SIZE];

	sprintf(buf, "@@@gtell||NAME:%s||PORTUDP:%d||WIZFROM:%s||"
	    "WIZTO:%s||MSG:%s@@@", LOCAL_NAME, CDUDP_PORT,
	    p->capname, targ, msg);
	send_udp_packet(h->host, h->port, buf);
}

void
cdudp_finger(struct user *p, struct host *h, char *targ)
{
	char buf[BUFFER_SIZE];

	sprintf(buf, "@@@gfinger_q||NAME:%s||PORTUDP:%d||"
	    "ASKWIZ:%s||PLAYER:%s@@@", LOCAL_NAME, CDUDP_PORT,
	    p->capname, targ);
	send_udp_packet(h->host, h->port, buf);
}

#endif /* CDUDP_SUPPORT */


void
inetd_query(struct user *p, char *host, char *req)
{
	char buf[BUFFER_SIZE];
	struct host *h;

#ifdef INETD_SUPPORT
	if ((h = lookup_host_by_name(hosts, host, 0)) != (struct host *)NULL)
	{
		sprintf(buf, "NAME:%s|UDP:%d|ID:%ld|REQ:query|SND:%s|DATA:$%s",
		    LOCAL_NAME, INETD_PORT, ++packet_id, p->capname, req);
		add_timeout(h->name, packet_id, buf, p->rlname, "query");
		send_udp(h->host, h->port, buf, packet_id);
		TRSUCC;
	}
#endif
#ifdef CDUDP_SUPPORT
	if ((h = lookup_host_by_name(cdudp_hosts, host, 0)) !=
	    (struct host *)NULL)
	{
		sprintf(buf, "@@@ping_q||NAME:%s||PORTUDP:%d@@@",
		    LOCAL_NAME, CDUDP_PORT);
		add_pinga(h->name, p->rlname);
		send_udp_packet(h->host, h->port, buf);
		TRSUCC;
	}
#endif
}

void
inetd_ping_host(struct user *p, char *ip, int tport, int cdudp)
{
	char buf[BUFFER_SIZE];

	if (cdudp)
	{
		sprintf(buf, "@@@ping_q||NAME:%s||PORTUDP:%d@@@",
		    LOCAL_NAME, CDUDP_PORT);
		send_udp_packet(ip, tport, buf);
		if (p != (struct user *)NULL)
			TRSUCC;
		return;
	}
#ifdef INETD_SUPPORT
	sprintf(buf, "NAME:%s|UDP:%d|ID:%ld|REQ:ping|SND:%s|is_jeamland:%d",
	    LOCAL_NAME, INETD_PORT, ++packet_id, p != (struct user *)NULL ?
	    p->capname : "__SYSTEM__", JL_MAGIC);
	send_udp(ip, tport, buf, packet_id);
	if (p != (struct user *)NULL)
		TRSUCC;
#endif
}

void
inetd_ping(struct user *p, char *host)
{
	char buf[BUFFER_SIZE];
	struct host *h;

#ifdef INETD_SUPPORT
	if (*host == '^')
		host++;
	if ((h = lookup_host_by_name(hosts, host, 0)) == (struct host *)NULL)
	{
#endif
#ifdef CDUDP_SUPPORT
		if (host[0] == '%')
			host++;
		if ((h = lookup_host_by_name(cdudp_hosts, host, 0)) ==
		    (struct host *)NULL)
			return;
		sprintf(buf, "@@@ping_q||NAME:%s||PORTUDP:%d@@@",
		    LOCAL_NAME, CDUDP_PORT);
		add_pinga(h->name, "__SYSTEM__");
		send_udp_packet(h->host, h->port, buf);
		TRSUCC;
#endif
		return;
#ifdef INETD_SUPPORT
	}
	sprintf(buf, "NAME:%s|UDP:%d|ID:%ld|REQ:ping|SND:%s|is_jeamland:%d",
	    LOCAL_NAME, INETD_PORT, ++packet_id, p->capname, JL_MAGIC);
	add_timeout(h->name, packet_id, buf, p->rlname, "ping");
	send_udp(h->host, h->port, buf, packet_id);
	TRSUCC;
#endif
}

void
inetd_locate(struct user *p, char *user, int vbs)
{
	char buf[BUFFER_SIZE];
	struct host *h;

#ifdef CDUDP_SUPPORT
	sprintf(buf, "@@@locate_q||NAME:%s||PORTUDP:%d||TARGET:%s||"
	    "ASKWIZ:%s@@@", LOCAL_NAME, CDUDP_PORT, lower_case(user),
	    p->rlname);
	for (h = cdudp_hosts; h != (struct host *)NULL; h = h->next)
		if (strcmp(h->name, LOCAL_NAME))
			send_udp_packet(h->host, h->port, buf);
#endif
#ifdef INETD_SUPPORT
	sprintf(buf, "NAME:%s|UDP:%d|ID:%ld|REQ:locate|SND:__LOCATOR__|user:%s"
	    "|vbs:%d|DATA:%s",
	    LOCAL_NAME, INETD_PORT, ++packet_id, p->rlname, vbs,
	    lower_case(inetd_encode(user)));
	for (h = hosts; h != (struct host *)NULL; h = h->next)
		if (strcmp(h->name, LOCAL_NAME))
		{
			add_timeout(h->name, packet_id, buf,
			    p->rlname, "locate");
			send_udp(h->host, h->port, buf, packet_id);
		}
	TRSUCC;
#endif
}

#endif /* INETD_SUPPORT || CDUDP_SUPPORT */

