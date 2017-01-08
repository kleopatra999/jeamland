/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	imud3.c
 * Function:	Implementation of Intermud-III protocol.
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

#ifdef IMUD3_SUPPORT

#undef I3_DEBUG

#undef IMUD3_LOOPBACK

#define I3_IREQ		0
#define I3_ITTL		1
#define I3_IRMUD	2
#define I3_IRUSER	3
#define I3_ILMUD	4
#define I3_ILUSER	5

#define I3_QSTRING(sv) (sv.type == T_STRING ? sv.u.string : (char *)NULL)

extern int port;
extern time_t current_time;
extern struct user *users;

struct tcpsock i3_router;
static struct strlist *routers = (struct strlist *)NULL;
static struct strlist *crouter = (struct strlist *)NULL;
static int i3_passwd = 0, i3_mudlist_id = 0, i3_chanlist_id = 0;
struct i3_host *i3hosts = (struct i3_host *)NULL;
static int i3_hosts_mod = 0, i3_save_lock = 0;
struct i3_channel *i3chans = (struct i3_channel *)NULL;
static int i3_recon = 0;
static int i3_recon_ev = -1;

#ifdef HASH_I3_HOSTS
struct hash *i3hash = (struct hash *)NULL;
#endif

#ifdef HASH_I3_HOSTS
void
i3_hash_stats(struct user *p, int verbose)
{
	hash_stats(p, i3hash, verbose);
}

int
i3_hash_string(char *str, int hsize)
{
	unsigned hvalue = 0;
	int len = 0;

	FUN_START("i3_hash_string");
	FUN_ARG(str);

	while (*str != '\0')
		hvalue = (hvalue << 2) + (*str++ ^ len++);

	FUN_END;

	return hvalue % hsize;
}
#endif /* HASH_I3_HOSTS */


/******
 * Host struct specifics
 ******/

static struct i3_host *
i3_create_host()
{
	struct i3_host *i = (struct i3_host *)xalloc(sizeof(struct i3_host),
	    "*i3_host");

	i->name = i->ip_addr = i->mudlib = i->base_mudlib = i->driver =
	    i->mud_type = i->open_status = i->admin_email = (char *)NULL;

	i->services = (struct mapping *)NULL;
	i->is_jl = 0;

	i->next = (struct i3_host *)NULL;

	return i;
}

static void
i3_free_host(struct i3_host *i, int complete)
{
	FREE(i->name);
	FREE(i->ip_addr);
	FREE(i->mudlib);
	FREE(i->base_mudlib);
	FREE(i->driver);
	FREE(i->mud_type);
	FREE(i->open_status);
	FREE(i->admin_email);

	if (i->services != (struct mapping *)NULL)
		free_mapping(i->services);

	if (complete)
		xfree(i);
}

int
i3_del_hosts()
{
	struct i3_host *c, *d;

	if (i3_router.fd != -1)
		return -1;

	for (c = i3hosts; c != (struct i3_host *)NULL; c = d)
	{
		d = c->next;
		i3_free_host(c, 1);
	}

	i3hosts = (struct i3_host *)NULL;
#ifdef HASH_I3_HOSTS
	free_hash(i3hash);
	/* No need to assign i3hash to NULL as i3_init will reallocate it */
#endif
	i3_init();
	return 1;
}

static void
i3_remove_host(char *mud)
{
	struct i3_host **list;

	FUN_START("i3_remove_host");

	for (list = &i3hosts; *list != (struct i3_host *)NULL;
	    list = &((*list)->next))
		if (!strcmp((*list)->name, mud))
		{
			struct i3_host *i = *list;

#ifdef HASH_I3_HOSTS
			remove_hash(i3hash, mud);
#endif

			*list = i->next;
			i3_free_host(i, 1);
			i3_hosts_mod = 1;
			FUN_END;
			return;
		}

	FUN_END;
}

struct i3_host *
i3_find_host(char *mud, int cas)
{
	struct i3_host *i, *got;
	int flag = 0;

	FUN_START("i3_find_host");

#ifdef HASH_I3_HOSTS
	if (cas)
	{
		FUN_ARG("hash");
		got = (struct i3_host *)lookup_hash(i3hash, mud);
		FUN_END;
		return got;
	}
#endif
	FUN_ARG("nohash");
	for (i = i3hosts; i != (struct i3_host *)NULL; i = i->next)
	{
		/* The !cas is here in case HASH_I3_HOSTS is not defined.. */
		if (!strcmp(mud, i->name) ||
		    (!cas && !strcasecmp(mud, i->name)))
		{
			FUN_END;
			return i;
		}
		else if (!cas && !strncasecmp(i->name, mud, strlen(mud)))
		{
			flag++;
			got = i;
		}
	}
	if (flag != 1)
	{
		FUN_END;
		return (struct i3_host *)NULL;
	}

	FUN_END;
	return got;
}

static void
i3_update_host(char *mud, struct svalue *info)
{
	struct i3_host *i;
	struct vector *v;
	struct svalue *sv;
	char *p;
	int upd = 0;

	for (p = mud; *p != '\0'; p++)
		if (!isprint(*p))
			/* Currently, disallow hosts containing illegal
			 * characters. */
			return;

	FUN_START("i3_update_host");
	FUN_ARG(mud);

	if (info == (struct svalue *)NULL)
	{
		log_file("imud3", "Info svalue is NULL in i3_update_host: %s",
		    mud);
		FUN_END;
		return;
	}

	/* Easy case first, has this mud been deleted? */
	if (info->type == T_NUMBER && info->u.number == 0)
	{
		i3_remove_host(mud);
		/*log_file("imud3", "Deleted host: %s",mud);*/
		FUN_END;
		return;
	}

	FUN_LINE;

	/* Got to be done sometime... */
	if (info->type != T_VECTOR || (v = info->u.vec)->size != 13 ||
	    v->items[0].type != T_NUMBER ||
	    v->items[1].type != T_STRING ||
	    v->items[2].type != T_NUMBER ||
	    v->items[3].type != T_NUMBER ||
	    v->items[4].type != T_NUMBER ||
	    v->items[5].type != T_STRING ||
	    v->items[6].type != T_STRING ||
	    v->items[7].type != T_STRING ||
	    v->items[8].type != T_STRING ||
	    v->items[9].type != T_STRING ||
	    v->items[10].type != T_STRING ||
	    v->items[11].type != T_MAPPING)
	{
		char *str;

		str = svalue_to_string(info);
		log_file("imud3", "Bad mudlist entry: %s - %s", mud, str);
		xfree(str);
		FUN_END;
		return;
	}

	FUN_LINE;
	    
	/* See if we are just to update an existing host.. */
	if ((i = i3_find_host(mud, 1)) != (struct i3_host *)NULL)
	{
		i3_free_host(i, 0);
		upd = 1;
	}
	else
		i = i3_create_host();

	/* Geronimo... */

	i->name = string_copy(mud, "i3host");
	i->state = v->items[0].u.number;
	i->ip_addr = v->items[1].u.string;
	i->player_port = v->items[2].u.number;
	i->imud_tcp_port = v->items[3].u.number;
	i->imud_udp_port = v->items[4].u.number;
	i->mudlib = v->items[5].u.string;
	i->base_mudlib = v->items[6].u.string;
	i->driver = v->items[7].u.string;
	i->mud_type = v->items[8].u.string;
	i->open_status = v->items[9].u.string;
	i->admin_email = v->items[10].u.string;
	i->services = v->items[11].u.map;

	/* Now, need to stop all of the above from being freed when the
	 * svalue is..
	 * Easiest way is to set the size of the array to 0, and then none
	 * of its elements will be freed when it is..
	 * Need to check the "other_data" mapping by hand. */
	v->size = 0;

	FUN_LINE;

	if (v->items[12].type == T_MAPPING)
		sv = map_string(v->items[12].u.map, "is_jeamland");
	else
		sv = &v->items[12];

	if (sv != (struct svalue *)NULL && sv->type == T_NUMBER &&
	    sv->u.number == JL_MAGIC)
		i->is_jl = 1;

	free_svalue(&v->items[12]);

	FUN_LINE;

	if (!upd)
	{
		struct i3_host **list;
		/* Need to add the host to the list.. alphabetical order */

#ifdef HASH_I3_HOSTS
		if (insert_hash(&i3hash, (void *)i))
		{
#endif
			for (list = &i3hosts; *list != (struct i3_host *)NULL;
			    list = &((*list)->next))
				if (strcmp((*list)->name, i->name) > 0)
					break;
			i->next = *list;
			*list = i;
#ifdef HASH_I3_HOSTS
		}
#endif

		/*log_file("imud3", "Added host:   %s", mud);*/
	}
	/*else log_file("imud3", "Updated host: %s",mud);*/

	i3_hosts_mod = 1;
	FUN_END;
}


/******
 * Channel specifics.
 ******/

static struct i3_channel *
i3_create_channel()
{
	struct i3_channel *c = (struct i3_channel *)xalloc(
	    sizeof(struct i3_channel), "i3_channel");

	c->name = c->owner = (char *)NULL;
	c->status = I3C_UNKNOWN;
	c->listening = 0;
	c->next = (struct i3_channel *)NULL;

	return c;
}

static void
i3_free_channel(struct i3_channel *c)
{
	FREE(c->name);
	FREE(c->owner);
	xfree(c);
}

int
i3_del_channels()
{
	struct i3_channel *c, *d;

	if (i3_router.fd != -1)
		return -1;

	for (c = i3chans; c != (struct i3_channel *)NULL; c = d)
	{
		d = c->next;
		i3_free_channel(c);
	}

	i3chans = (struct i3_channel *)NULL;
	return 1;
}

static void
i3_remove_channel(char *chan)
{
	struct i3_channel **list;

	for (list = &i3chans; *list != (struct i3_channel *)NULL;
	    list = &((*list)->next))
		if (!strcmp((*list)->name, chan))
		{
			struct i3_channel *i = *list;

			*list = i->next;
			i3_free_channel(i);
			return;
		}
}

static void
i3_insert_channel(struct i3_channel *c)
{
	struct i3_channel **list;

	for (list = &i3chans; *list != (struct i3_channel *)NULL;
	    list = &((*list)->next))
		if (strcmp((*list)->name, c->name) > 0)
			break;
	c->next = *list;
	*list = c;
}

struct i3_channel *
i3_find_channel(char *chan)
{
	struct i3_channel *c;

	for (c = i3chans; c != (struct i3_channel *)NULL; c = c->next)
		if (!strcmp(c->name, chan))
			break;
	return c;
}

#ifdef NEWBIE_TUNEOUT
void
i3_newbie_tuneout(struct user *p)
{
	struct i3_channel *c;

	for (c = i3chans; c != (struct i3_channel *)NULL; c = c->next)
	{
		struct strlist *s;

		s = create_strlist("chanblock");
		s->str = string_copy(c->name, "chanblock");
		add_strlist(&p->chan_earmuff, s, SL_SORT);
	}
}
#endif /* NEWBIE_TUNEOUT */

static void
i3_add_channel(char *chan, struct vector *v)
{
	struct i3_channel *c;

	/* Simple case first.. existing channel */
	if ((c = i3_find_channel(chan)) != (struct i3_channel *)NULL)
	{
		if (!strcmp(c->owner, v->items[0].u.string) &&
		    c->status == v->items[1].u.number)
			/* Nothing has changed.. */
			return;
		xfree(c->owner);
	}
	else
	{
		c = i3_create_channel();
		c->name = string_copy(chan, "i3 chan name");

		i3_insert_channel(c);

		/* New channel, default is to tune it out. */
		i3_tune_channel(c, 0);
	}

	c->owner = v->items[0].u.string;
	/* To prevent double free. */
	v->items[0].type = T_EMPTY;
	c->status = v->items[1].u.number;
}

char *
i3chan_stat(enum i3stats s)
{
	switch (s)
	{
	    case I3C_UNKNOWN:
		return "Unknown ";
	    case I3C_BANNED:
		return "Banned  ";
	    case I3C_ADMITTED:
		return "Admitted";
	    case I3C_FILTERED:
		return "Filtered";
	    default:
		return "<BAD STATUS>";
	}
}

static void
i3_save_rest(struct event *ev)
{
	i3_save_lock = 0;
}

void
i3_save()
{
	/* This save operation takes a LONG time...
	 * so I fork() and use a child process to do it..
	 * (Only happens once every 16 minutes)
	 * We don't want to have two forked processes running so disallow
	 * further saves for 2 minutes.
	 */

	if (i3_save_lock || !i3_hosts_mod)
		return;

	i3_save_lock = 1;
	add_event(create_event(), i3_save_rest, 120, "i3save_unlock");

	if (!fork())
	{
		FILE *fp;
		struct i3_host *i;
		struct i3_channel *c;
		char *ptr = (char *)NULL;

		/* Disconnect the child from the real world... */
		remove_ipc();
		if (i3_router.fd != -1)
			close(i3_router.fd);

		if ((fp = fopen(F_IMUD3, "w")) == (FILE *)NULL)
			exit(0);

#define I3_WRITE_STRING(ss) ENCODE(ss); \
		    fprintf(fp, "\"%s\",", ptr)

		write_strlist(fp, "routers", routers);
		fprintf(fp, "passwd %d\n", i3_passwd);
		fprintf(fp, "mudlist %d\n", i3_mudlist_id);
		fprintf(fp, "chanlist %d\n", i3_chanlist_id);

		for (i = i3hosts; i != (struct i3_host *)NULL; i = i->next)
		{
			struct svalue sv;
			char *str;

			fprintf(fp, "host ([");

			ENCODE(i->name);
			fprintf(fp, "\"%s\"", ptr);
			/*I3_WRITE_STRING(i->name);*/

			fprintf(fp, ":({%d,", i->state);
			I3_WRITE_STRING(i->ip_addr);
			fprintf(fp, "%d,%d,%d,", i->player_port,
			    i->imud_tcp_port, i->imud_udp_port);
			I3_WRITE_STRING(i->mudlib);
			I3_WRITE_STRING(i->base_mudlib);
			I3_WRITE_STRING(i->driver);
			I3_WRITE_STRING(i->mud_type);
			I3_WRITE_STRING(i->open_status);
			I3_WRITE_STRING(i->admin_email);

			sv.type = T_MAPPING;
			sv.u.map = i->services;
			str = svalue_to_string(&sv);
			fprintf(fp, "%s,", str);
			xfree(str);

			fprintf(fp, "%d,}),])\n", i->is_jl ? JL_MAGIC : 0);
		}

		for (c = i3chans; c != (struct i3_channel *)NULL; c = c->next)
		{
			fprintf(fp, "channel ({");
			I3_WRITE_STRING(c->name);
			I3_WRITE_STRING(c->owner);
			fprintf(fp, "%d,%d,})\n", c->status, c->listening);
		}

		fclose(fp);
		/* This extra free is important! */
		FREE(ptr);
		exit(0);
	}
	i3_hosts_mod = 0;
}

static void
i3_autosave(struct event *ev)
{
	if (ev != (struct event *)NULL)
		i3_save();

	add_event(create_event(), i3_autosave, I3_AUTOSAVE, "i3save");
}

void
i3_load()
{
	FILE *fp;
	struct stat st;
	int line = 0;
	char *buf;

	i3_autosave((struct event *)NULL);

	if ((fp = fopen(F_IMUD3, "r")) == (FILE *)NULL)
	{
		log_file("syslog", "Imud3 savefile non-existant.");
		return;
	}
	if (fstat(fileno(fp), &st) == -1)
	{
		log_perror("fstat");
		fatal("Cannot stat open file.");
	}
	if (!st.st_size)
	{
		fclose(fp);
		log_file("syslog", "Imud3 savefile empty.");
		return;
	}

	buf = (char *)xalloc((size_t)st.st_size, "i3_load");
	while (fgets(buf, (int)st.st_size - 1, fp) != (char *)NULL)
        {
		line++;

		if (ISCOMMENT(buf))
			continue;
		if (!strncmp(buf, "routers ", 8))
		{
			free_strlists(&routers);
			routers = decode_strlist(buf, SL_TAIL);

			/* Necessary check */
			if (strlist_size(routers) % 2)
			{
				log_file("syslog",
				    "I3: Router list size is not a multiple of "
				    "two.");
				log_file("syslog",
				    "I3: Router list discarded.");
				free_strlists(&routers);
			}

			continue;
		}
		if (!strncmp(buf, "host ", 5))
		{
			struct svalue *sv;

			if ((sv = decode_one(buf, "decode_one i3host")) ==
			    (struct svalue *)NULL)
			{
				log_file("imud3", "I3: Bad line: %d", line);
				continue;
			}

			if (sv->type == T_MAPPING && sv->u.map->size == 1 &&
			    sv->u.map->indices->items[0].type == T_STRING)
				i3_update_host(
				    sv->u.map->indices->items[0].u.string,
				    &sv->u.map->values->items[0]);

			free_svalue(sv);
			continue;
		}
		if (!strncmp(buf, "channel ", 8))
		{
			struct i3_channel *c;
			struct svalue *sv;

			if ((sv = decode_one(buf, "decode_one i3chan")) ==
			    (struct svalue *)NULL)
			{
				log_file("imud3", "I3: Bad line: %d", line);
				continue;
			}

			if (sv->type == T_VECTOR && sv->u.vec->size == 4 &&
			    sv->u.vec->items[0].type == T_STRING &&
			    sv->u.vec->items[1].type == T_STRING &&
			    sv->u.vec->items[2].type == T_NUMBER &&
			    sv->u.vec->items[3].type == T_NUMBER)
			{
				c = i3_create_channel();

				c->name = sv->u.vec->items[0].u.string;
				c->owner = sv->u.vec->items[1].u.string;
				c->status = sv->u.vec->items[2].u.number;
				c->listening = sv->u.vec->items[3].u.number;
				/* To avoid double free.. */
				sv->u.vec->size = 0;

				i3_insert_channel(c);
			}

			free_svalue(sv);
			continue;
		}

		if (sscanf(buf, "passwd %d", &i3_passwd))
			continue;
		if (sscanf(buf, "mudlist %d", &i3_mudlist_id))
			continue;
		if (sscanf(buf, "chanlist %d", &i3_chanlist_id))
			continue;
	}
	fclose(fp);
	xfree(buf);
	i3_hosts_mod = 0;
}

void
i3_init()
{
	i3_router.fd = -1;
#ifdef HASH_I3_HOSTS
	/* This table will need to be at least prime4 */
	i3hash = create_hash(3, "i3hosts", i3_hash_string, 0);
#endif
}

#ifdef IMUD3_LOOPBACK
static void i3_svalue(struct svalue *);
#endif

void
i3_send_string(char *buf)
{
	int length;
	char *nbuf;

	FUN_START("i3_send_string");
	FUN_ARG(buf);

	/* MUD mode TCP needs to send a 'length' byte in advance of the main
	 * packet. */

#ifdef I3_DEBUG
	log_file("debug", "I3: Send: %s", buf);
#endif

#ifdef IMUD3_LOOPBACK
	{
		struct svalue *sv;

		if ((sv = string_to_svalue(buf, "i3_loopback decode")) ==
		    (struct svalue *)NULL)
			log_file("error",
			    "I3: loopback decode failed (%s).", buf);
		else
		{
			i3_svalue(sv);
			free_svalue(sv);
		}
		FUN_END;
		return;
	}
#endif /* IMUD3_LOOPBACK */

	length = strlen(buf);
	if (!length)
	{
		FUN_END;
		return;
	}

	/* NB: Allocating another bit of memory just bigger than the string we
	 * want to send.... Look at this later.. XXX */

	nbuf = (char *)xalloc(sizeof(long) + length + 1, "imud3_send");
	*(long *)nbuf = htonl((long)length);
	length += sizeof(long);
	nbuf[sizeof(long)] = '\0';

	strcpy(nbuf + sizeof(long), buf);

#ifdef I3_DEBUG
	log_file("debug", "Send packet, length: %d", length - sizeof(long));
	log_file("debug", "%x %x %x %x", nbuf[0], nbuf[1], nbuf[2], nbuf[3]);
#endif

	if (write_tcpsock(&i3_router, nbuf, length) == -1)
		i3_lostconn(&i3_router);

	FUN_END;
}

static void
i3_add_string(char *s, char *str)
{
	if (str == (char *)NULL)
	{
		strcat(s, "0,");
		return;
	}

	str = code_string(str);

	strcat(s, "\"");
	strcat(s, str);
	strcat(s, "\",");

	xfree(str);
}

static void
i3_add_strbuf_string(struct strbuf *s, char *str)
{
	if (str == (char *)NULL)
	{
		add_strbuf(s, "0,");
		return;
	}
	str = code_string(str);

	cadd_strbuf(s, '"');
	add_strbuf(s, str);
	add_strbuf(s, "\",");

	xfree(str);
}

static void
i3_add_svalue(char *s, struct svalue *sv)
{
	switch (sv->type)
	{
	    case T_STRING:
		i3_add_string(s, sv->u.string);
		break;
	    case T_NUMBER:
	    {
		static char buf[100];

		sprintf(buf, "%ld,", sv->u.number);
		strcat(s, buf);
		break;
	    }
	    default:
		fatal("Unknown svalue type in i3_add_svalue: %d.", sv->type);
		break;
	}
}

static char *
i3_packet_header(char *req, char *orig_user, char *dest_mud, char *dest_user)
{
	static char buf[BUFFER_SIZE];

	sprintf(buf, "({\"%s\",%d,\"%s\",", req, I3_TTL, LOCAL_NAME);
	i3_add_string(buf, orig_user);
	i3_add_string(buf, dest_mud);
	i3_add_string(buf, dest_user);

	return buf;
}

static char *
i3_reply_header(char *req, struct svalue *sv)
{
	static char buf[BUFFER_SIZE];

	sprintf(buf, "({\"%s\",%d,\"%s\",0,", req, I3_TTL, LOCAL_NAME);

	i3_add_svalue(buf, &sv->u.vec->items[I3_IRMUD]);
	i3_add_svalue(buf, &sv->u.vec->items[I3_IRUSER]);

	return buf;
}

static void
i3_startup_packet()
{
	struct strbuf pkt;

	if (i3_router.fd == -1)
		return;

	init_strbuf(&pkt, 0, "i3_startup_pkt");

	add_strbuf(&pkt, i3_packet_header("startup-req-3", (char *)NULL,
	    crouter->str, (char *)NULL));

	sadd_strbuf(&pkt, "%d,", i3_passwd);

	sadd_strbuf(&pkt, "%d,%d,", i3_mudlist_id, i3_chanlist_id);

	/* Ports: main login, tcp, udp */
	sadd_strbuf(&pkt, "%d,%d,%d,", port, 0, 0);
	/* Mudlib, Base mudlib */
	add_strbuf(&pkt, "\"Not Applicable\",\"Not Applicable\",");
	/* Driver */
	sadd_strbuf(&pkt, "\"JeamLand %s\",", VERSION);
	/* Driver type, open status */
	sadd_strbuf(&pkt, "\"JL\",\"%s\",", IMUD3_STATUS);
	/* Admin email */
	sadd_strbuf(&pkt, "\"%s\",", OVERSEER_EMAIL);

	/* Services.. */
	add_strbuf(&pkt, "(["
	    "\"who\":1,"
	    "\"finger\":1,"
	    "\"locate\":1,"
	    "\"tell\":1,"
	    "\"channel\":1,"
	    "]),");
	/* Other.. */
	sadd_strbuf(&pkt, "([\"is_jeamland\":%d,]),", JL_MAGIC);

	add_strbuf(&pkt, "})");

	i3_send_string(pkt.str);

	free_strbuf(&pkt);
}

int
i3_startup()
{
	static int initted = 0;
	struct strlist *s;

	if (i3_router.fd != -1)
		return -1;

#ifdef IMUD3_LOOPBACK
	log_file("syslog", "I3: Loopback test mode.");
	return 1;
#endif

	FUN_START("i3_startup");

	/* If we are here, we are not connected to a router! */
	init_tcpsock(&i3_router, sizeof(long));
	i3_router.flags |= TCPSOCK_BINARY;

	/* Need to connect to a router - we have a list to try.
	 * NB: This relies on the strlist having a size which is a multiple
	 *     of two!
	 */
	crouter = (struct strlist *)NULL;
	for (s = routers; s != (struct strlist *)NULL; s = s->next)
	{
		struct strlist *r = s;
		char *addr, *p;
		int port;

		if (!initted)
			log_file("syslog", "I3: Contacting router %s.",
			    s->str);

		FUN_ARG(s->str);

		if ((s = s->next) == (struct strlist *)NULL)
		{
			log_file("syslog", "I3: No address for router.");
			free_tcpsock(&i3_router);
			FUN_END;
			return 0;
		}

		addr = string_copy(s->str, "i3_startup addr");

		if ((p = strchr(addr, ' ')) == (char *)NULL)
		{
			log_file("syslog", "I3: No port number for router.");
			free_tcpsock(&i3_router);
			FUN_END;
			return 0;
		}

		*p++ = '\0';

		port = atoi(p);

		if (!initted)
			log_file("syslog", "I3:     Connecting to %s:%d",
			    addr, port);

		FUN_LINE;

		if (connect_tcpsock(&i3_router, addr, port) == 1)
		{
			crouter = r;
			i3_router.flags |= TCPSOCK_BINHEAD;
			if (!initted)
				log_file("syslog", "I3: Connected to %s [%d].",
				    r->str, i3_router.fd);

			/* And why not... improves the look of "netstat" */
			send_erq(ERQ_RESOLVE_NUMBER, "%s;\n", addr);

			xfree(addr);
			break;
		}

		FUN_LINE;

		/*log_perror("I3");*/
		if (!initted)
			log_file("syslog", "I3: Connection failed.");
		xfree(addr);
	}

	if (crouter == (struct strlist *)NULL)
	{
		log_file("syslog", "I3: No router found.");
		free_tcpsock(&i3_router);
		FUN_END;
		return 0;
	}

	notify_level(L_CONSUL, "[ Connected to I3-router. ]");

	i3_startup_packet();

	initted = 1;

	FUN_END;
	return 1;
}

int
i3_shutdown()
{
	struct strbuf pkt;

	if (i3_router.fd == -1)
		return -1;

	init_strbuf(&pkt, 0, "i3_shutdown_pkt");

	add_strbuf(&pkt, i3_packet_header("shutdown", (char *)NULL,
	    crouter->str, (char *)NULL));

	add_strbuf(&pkt, "1,})");

	i3_send_string(pkt.str);

	free_strbuf(&pkt);

	close_tcpsock(&i3_router);
	free_tcpsock(&i3_router);

	notify_level(L_CONSUL, "[ Disconnected from I3-router. ]");

	return 1;
}

enum i3_errors { I3_UNK_TYPE, I3_UNK_USER, I3_UNK_CHANNEL, I3_BAD_PKT };

static void
i3_error(enum i3_errors err, struct svalue *sv, char *str)
{
	struct strbuf pkt;
	int tf = 0;

	init_strbuf(&pkt, 0, "i3_error pkt");

	if (str == (char *)NULL)
	{
		tf = 1;
		str = svalue_to_string(sv);
	}

	add_strbuf(&pkt, i3_packet_header("error", (char *)NULL, 
	    sv->u.vec->items[I3_IRMUD].u.string,
	    I3_QSTRING(sv->u.vec->items[I3_IRUSER])));

	switch (err)
	{
	    case I3_UNK_TYPE:
		add_strbuf(&pkt, "\"unk-type\",\"unknown packet type\",");
		break;
	    case I3_UNK_USER:
		add_strbuf(&pkt, "\"unk-user\",\"unknown target user\",");
		break;
	    case I3_UNK_CHANNEL:
		add_strbuf(&pkt, "\"unk-channel\",\"unknown channel name\",");
		break;
	    case I3_BAD_PKT:
		add_strbuf(&pkt, "\"bad-pkt\",\"bad packet format\",");
		break;
	}

	add_strbuf(&pkt, str);
	add_strbuf(&pkt, ",})");

	i3_send_string(pkt.str);

	free_strbuf(&pkt);
	if (tf)
		xfree(str);
}

void
i3_tune_channel(struct i3_channel *c, int stat)
{
	char *s;

	s = i3_packet_header("channel-listen", (char *)NULL, crouter->str,
	    (char *)NULL);

	i3_add_string(s, c->name);
	if (stat)
		strcat(s, "1,})");
	else
		strcat(s, "0,})");

	i3_send_string(s);

	c->listening = stat;
	i3_hosts_mod = 1;
}

static char *
i3_parse_string(char *str, char *ruser, char *rmud, char *luser, char *lmud)
{
	struct strbuf s;

	init_strbuf(&s, strlen(str) + STRBUF_CHUNK, "i3_parse_string");

	while (*str != '\0')
	{
		switch (*str)
		{
		    case '\\':
			cadd_strbuf(&s, *++str);
			break;

		    case '$':
			if (*++str == 'N')
				sadd_strbuf(&s, "%s@%s", ruser, rmud);
			else if (luser != (char *)NULL && *str == 'O')
			{
				if (!strcmp(lmud, LOCAL_NAME))
					add_strbuf(&s, luser);
				else
					sadd_strbuf(&s, "%s@%s", luser, lmud);
			}
			else
				sadd_strbuf(&s, "$%c", *str);
			break;

		    default:
			cadd_strbuf(&s, *str);
		}
		str++;
	}
	pop_strbuf(&s);
	return s.str;
}

#define RECIPIENT_CHECK \
		if (sv->u.vec->items[I3_ILUSER].type != T_STRING || \
		    (p = find_user_absolute((struct user *)NULL, \
		    sv->u.vec->items[I3_ILUSER].u.string)) == \
		    (struct user *)NULL) \
			break

static void
i3_svalue(struct svalue *sv)
{
	/* This is where the action is.. have received a packet and decoded
	 * it as an svalue. */

	static const char *i3_requests[] = {
		"error",
		"startup-reply",
		"mudlist",
		"who-req",
		"who-reply",
		"finger-req",
		"finger-reply",
		"locate-req",
		"locate-reply",
		"tell",
		"emoteto",
		"chanlist-reply",
		"channel-m",
		"channel-e",
		"channel-t",
		"chan-who-req",
		(char *)NULL };
	struct user *targ;
	int i;

	FUN_START("i3_svalue");

	/* A few sanity checks. */

	/* This one should have been checked by i3_incoming... */
	if (sv == (struct svalue *)NULL)
	{
		log_file("imud3", "I3: Could not decode packet.");
		FUN_END;
		return;
	}

	if (sv->type != T_VECTOR || sv->u.vec->size < 6)
	{
		char *str;

		str = svalue_to_string(sv);

		log_file("imud3", "I3 packet: not a vector or < 6 elements.");
		log_file("imud3", "I3 packet: %s", str);

		xfree(str);

		/* Um.. can't return a bad-packet error as we don't know who
		 * to send it to! */

		FUN_END;
		return;
	}

	if (sv->u.vec->items[I3_IREQ].type != T_STRING ||
	    sv->u.vec->items[I3_IRMUD].type != T_STRING)
	{
		char *str;

		str = svalue_to_string(sv);

		log_file("imud3", "I3 packet: request / host not a string.");
		log_file("imud3", "I3 packet: %s", str);

		i3_error(I3_BAD_PKT, sv, str);

		xfree(str);

		FUN_END;
		return;
	}

	FUN_ARG(sv->u.vec->items[I3_IREQ].u.string);

	if (sv->u.vec->items[I3_ILUSER].type == T_STRING)
		targ = find_user_absolute((struct user *)NULL,
		    sv->u.vec->items[I3_ILUSER].u.string);
	else
		targ = (struct user *)NULL;

	for (i = 0; i3_requests[i] != (char *)NULL; i++)
		if (!strcmp(sv->u.vec->items[I3_IREQ].u.string, i3_requests[i]))
			break;

#ifdef DEBUG_IMUD3
#if 1
	/* who, finger, channel_who_req */
	if (i == 3 || i == 5 || i == 15)
#endif
	{
		char *t = svalue_to_string(sv);

		notify_level_wrt_flags(L_CONSUL, EAR_I3,
		    "[ I3 - %s ]", t);
		xfree(t);
	}
#endif

	FUN_LINE;

	switch (i)
	{
	    case 0:	/* error */
	    {
		struct strbuf buf;
		char *str;

		init_strbuf(&buf, 0, "i3 error");

		if (sv->u.vec->size != 9)
		{
			log_file("imud3", "I3 error packet: not 9 elements.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		if (sv->u.vec->items[6].type != T_STRING ||
		    sv->u.vec->items[7].type != T_STRING)
		{
			log_file("imud3", "I3 error packet: no error strings.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		/* Construct error message. */
		sadd_strbuf(&buf, "I3 Error: %s (%s).",
		    sv->u.vec->items[6].u.string,
		    sv->u.vec->items[7].u.string);

		str = svalue_to_string(&sv->u.vec->items[8]);

		if (targ != (struct user *)NULL)
		{
			/* Direct message to the user */
			attr_colour(targ, "notify");
			write_socket(targ, "%s", buf.str);
/*
			if (targ->level >= L_CONSUL)
				write_socket(targ, "Packet: %s\n", str);
*/
			reset(targ);
			write_socket(targ, "\n");
		}
		else
		{
			notify_level(L_CONSUL, "[ %s ]", buf.str);
			notify_level(L_CONSUL, "[ Packet: %s ]", str);
		}
		xfree(str);
		free_strbuf(&buf);
		break;
	    }

	    case 1:	/* startup-reply */
	    {
		/* All I'm interested in here is the password and
		 * list of routers.. */

		if (sv->u.vec->size != 8)
		{
			log_file("imud3", "I3 startup packet: not 8 elements.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		/* Store the password.. */
		if (sv->u.vec->items[7].type == T_NUMBER)
			i3_passwd = sv->u.vec->items[7].u.number;

		/* Grab the router list */
		if (sv->u.vec->items[6].type != T_VECTOR)
			log_file("imud3",
			    "I3 startup: router list not vector.");
		else
		{
			struct strlist *tmp, *n;
			struct vector *v;
			int i;

			n = (struct strlist *)NULL;

			v = sv->u.vec->items[6].u.vec;

			for (i = 0; i < v->size; i++)
			{
				struct vector *y;

				if (v->items[i].type != T_VECTOR)
					continue;
				y = v->items[i].u.vec;

				if (y->size != 2 ||
			 	    y->items[0].type != T_STRING ||
				    y->items[1].type != T_STRING)
					continue;

				tmp = create_strlist("router");
				tmp->str = y->items[0].u.string;
				TO_EMPTY(y->items[0]);
				add_strlist(&n, tmp, SL_TAIL);

				tmp = create_strlist("router ip");
				tmp->str = y->items[1].u.string;
				TO_EMPTY(y->items[1]);
				add_strlist(&n, tmp, SL_TAIL);
			}

			/* Have we been allocated the preferred router we
			 * tried first ? */
			if (strcmp(routers->str, n->str))
				/* Nope... need to close down and try again.
				 */
				i3_router.flags |= (TCPSOCK_SHUTDOWN |
				    TCPSOCK_RESTART);
			/* Yes - need to find our current router in the new
			 * list. */
			else
			{
				for (tmp = n; tmp != (struct strlist *)NULL;
				    tmp = tmp->next)
					if (!strcmp(tmp->str, crouter->str))
					{
						/* Got it.. */
						crouter = tmp;
						break;
					}
				if (tmp == (struct strlist *)NULL)
					/* Hmm.. we are connected to a router no
					 * longer in the list... better close.
					 */
					i3_router.flags |= TCPSOCK_SHUTDOWN;
					i3_router.flags |= TCPSOCK_RESTART;
			}

			free_strlists(&routers);
			routers = n;
		}
		break;
	    }

	    case 2:	/* mudlist */
	    {
		struct mapping *m;
		int i;

		if (sv->u.vec->size != 8)
		{
			log_file("imud3", "I3 mudlist packet: not 8 elements.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		if (sv->u.vec->items[6].type != T_NUMBER)
		{
			log_file("imud3", "I3 mudlist packet: id not integer.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		if (sv->u.vec->items[7].type != T_MAPPING)
		{
			log_file("imud3",
			    "I3 mudlist packet: list is not mapping.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		i3_mudlist_id = sv->u.vec->items[6].u.number;

		/* For ease of access.. */
		m = sv->u.vec->items[7].u.map;

		for (i = 0; i < m->size; i++)
		{
			if (m->indices->items[i].type != T_STRING)
			{
				log_file("imud3",
				    "I3 mudlist packet: non-string mudname.");
				continue;
			}
			i3_update_host(m->indices->items[i].u.string,
			    &m->values->items[i]);
		}
		break;
	    }

	    case 3:	/* who-req */
	    {
		struct strbuf t;

		init_strbuf(&t, 0, "who-reply t");
		add_strbuf(&t, i3_reply_header("who-reply", sv));
		add_strbuf(&t, "({");

		for (targ = users->next; targ != (struct user *)NULL;
		    targ = targ->next)
			if (IN_GAME(targ) && !(targ->saveflags & U_INVIS))
			{
				add_strbuf(&t, "({");
				i3_add_strbuf_string(&t, targ->name);
				sadd_strbuf(&t, "%d,", current_time -
				    targ->last_command);
				i3_add_strbuf_string(&t, targ->title);
				add_strbuf(&t, "}),");
			}
		add_strbuf(&t, "}),})");

		i3_send_string(t.str);
		free_strbuf(&t);
		break;
	    }

	    case 4:	/* who-reply */
	    {
		struct vector *v;
		struct strbuf b;
		int i;

		if (targ == (struct user *)NULL)
			break;

		if (sv->u.vec->size != 7 ||
		    sv->u.vec->items[6].type != T_VECTOR)
		{
			write_socket(targ, "Bad who reply packet from: %s\n",
			    sv->u.vec->items[I3_IRMUD].u.string);
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		init_strbuf(&b, 0, "i3 who-reply");
		sadd_strbuf(&b, "Who reply from %s:\n",
		    sv->u.vec->items[I3_IRMUD].u.string);

		v = sv->u.vec->items[6].u.vec;
		for (i = 0; i < v->size; i++)
		{
			if (v->items[i].type != T_VECTOR ||
			    v->items[i].u.vec->size != 3 ||
			    v->items[i].u.vec->items[0].type != T_STRING ||
			    v->items[i].u.vec->items[1].type != T_NUMBER ||
			    v->items[i].u.vec->items[2].type != T_STRING)
				add_strbuf(&b, " << BAD >>\n");
			else
				sadd_strbuf(&b, " %-15s (Idle %4ds) %s\n",
				    v->items[i].u.vec->items[0].u.string,
				    v->items[i].u.vec->items[1].u.number,
				    v->items[i].u.vec->items[2].u.string);
		}

		write_socket(targ, "\n%s\n", b.str);
		free_strbuf(&b);
		break;
	    }

	    case 5:	/* finger-req */
	    {
		struct strbuf t;

		if (sv->u.vec->size != 7)
		{
			log_file("imud3",
			    "I3 finger-req packet: not 7 elements.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		if (sv->u.vec->items[6].type != T_STRING)
		{
			log_file("imud3",
			    "I3 finger-req packet: username not a string.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		init_strbuf(&t, 0, "finger-reply t");
		add_strbuf(&t, i3_reply_header("finger-reply", sv));

		users->level = L_VISITOR;

		if ((targ = doa_start(users, sv->u.vec->items[6].u.string))
		    == (struct user *)NULL)
		{
			i3_error(I3_UNK_USER, sv, (char *)NULL);
			break;
		}
		else
		{
			i3_add_strbuf_string(&t, targ->name);
			i3_add_strbuf_string(&t, targ->title);

			/* Real name */
			add_strbuf(&t, "0,");

			if (targ->saveflags & U_EMAIL_PRIVATE)
				add_strbuf(&t, "0,");
			else
				i3_add_strbuf_string(&t, targ->email);

			if (IN_GAME(targ))
				sadd_strbuf(&t, "\"%s\",%d,",
				    nctime(&targ->login_time),
				    current_time - targ->last_command);
			else
				sadd_strbuf(&t, "\"%s\",-1,",
				    nctime(&targ->last_login));

			/* Ip Name */
			add_strbuf(&t, "0,");

			sadd_strbuf(&t, "\"%s\",",
			    capfirst(level_name(targ->level, 0)));

			i3_add_strbuf_string(&t, targ->plan);

			add_strbuf(&t, "})");
		}

		doa_end(targ, 0);

		i3_send_string(t.str);
		free_strbuf(&t);
		break;
	    }

	    case 6:	/* finger-reply */
	    {
		struct strbuf b;

		if (targ == (struct user *)NULL)
			break;

		if (sv->u.vec->size != 15)
		{
			write_socket(targ, "Bad finger reply packet from: %s\n",
			    sv->u.vec->items[I3_IRMUD].u.string);
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

#define FINGER_BANNER add_strbuf(&b, \
	    "------------------------------------------------" \
	    "----------------------------\n")
#define F_VISNAME	6
#define F_TITLE		7
#define F_RLNAME	8
#define F_EMAIL		9
#define F_LOGINOUT	10
#define F_IDLE		11
#define F_IP		12
#define F_LEVEL		13
#define F_EXTRA		14

		init_strbuf(&b, 0, "i3 finger-reply");
		sadd_strbuf(&b, "Finger reply from %s:\n",
		    sv->u.vec->items[I3_IRMUD].u.string);

		FINGER_BANNER;

		if (sv->u.vec->items[F_VISNAME].type == T_STRING)
			add_strbuf(&b, sv->u.vec->items[F_VISNAME].u.string);
		else
			add_strbuf(&b, "<NUL>");
		cadd_strbuf(&b, ' ');

		if (sv->u.vec->items[F_TITLE].type == T_STRING)
			add_strbuf(&b, sv->u.vec->items[F_TITLE].u.string);
		cadd_strbuf(&b, '\n');

		FINGER_BANNER;

		if (sv->u.vec->items[F_LEVEL].type == T_STRING)
			sadd_strbuf(&b, "Level:      %s\n",
			    sv->u.vec->items[F_LEVEL].u.string);

		if (sv->u.vec->items[F_IDLE].type == T_NUMBER)
		{
			if (sv->u.vec->items[F_IDLE].u.number != -1)
				add_strbuf(&b, "On Since:   ");
			else
				add_strbuf(&b, "Last Login: ");
			
			if (sv->u.vec->items[F_LOGINOUT].type ==
			    T_STRING)
			{
				add_strbuf(&b,
				    sv->u.vec->items[F_LOGINOUT] .u.string);
				if (sv->u.vec->items[F_IP].type == T_STRING)
					sadd_strbuf(&b, " From: %s\n",
					    sv->u.vec->items[F_IP].u.string);
				else
					cadd_strbuf(&b, '\n');
			}

			if (sv->u.vec->items[F_IDLE].u.number != -1)
				sadd_strbuf(&b, "Idle for:   %s.\n",
				    conv_time(sv->u.vec->items[F_IDLE]
				    .u.number));
		}

		FINGER_BANNER;

		if (sv->u.vec->items[F_EMAIL].type == T_STRING)
			sadd_strbuf(&b, "Email:      %s\n",
			    sv->u.vec->items[F_EMAIL].u.string);

		if (sv->u.vec->items[F_RLNAME].type == T_STRING)
			sadd_strbuf(&b, "Real Name:  %s\n",
			    sv->u.vec->items[F_RLNAME].u.string);

		FINGER_BANNER;

		if (sv->u.vec->items[F_EXTRA].type == T_STRING)
			add_strbuf(&b, sv->u.vec->items[F_EXTRA].u.string);

		write_socket(targ, "%s\n", b.str);
		free_strbuf(&b);
		break;
	    }

	    case 7:	/* locate-req */
	    {
		if (sv->u.vec->size != 7)
		{
			log_file("imud3",
			    "I3 locate-req packet: not 7 elements.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}
		if (sv->u.vec->items[6].type != T_STRING)
		{
			log_file("imud3",
			    "I3 locate-req packet: username not string.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		users->level = L_VISITOR;

		if ((targ = find_user_absolute(users,
		    sv->u.vec->items[6].u.string)) != (struct user *)NULL)
		{
			struct strbuf b;
			char *s;

			init_strbuf(&b, 0, "locate-reply b");

			add_strbuf(&b, i3_reply_header("locate-reply", sv));

			s = code_string(targ->name);
			sadd_strbuf(&b, "\"%s\",\"%s\",%d,", LOCAL_NAME,
			    s, current_time - targ->last_command);
			xfree(s);

			if (targ->flags & U_AFK)
			{
				if (targ->stack.sp->type == T_STRING)
				{
					s = code_string(
					    targ->stack.sp->u.string);
					sadd_strbuf(&b, "\"AFK %s\"", s);
					xfree(s);
				}
				else
					add_strbuf(&b, "\"AFK\",");
			}
			else
				add_strbuf(&b, "0,");
			add_strbuf(&b, "})");

			i3_send_string(b.str);
			free_strbuf(&b);
		}
		break;
	    }

	    case 8:	/* locate-reply */
	    {
		if (targ == (struct user *)NULL)
			break;

		if (sv->u.vec->size != 10 ||
		    sv->u.vec->items[6].type != T_STRING ||
		    sv->u.vec->items[7].type != T_STRING ||
		    sv->u.vec->items[8].type != T_NUMBER)
		{
			log_file("imud3",
			    "I3 locate-reply packet: Bad.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		write_socket(targ, "Located: %s@%s (idle %ds)\n",
		    sv->u.vec->items[7].u.string,
		    sv->u.vec->items[6].u.string,
		    sv->u.vec->items[8].u.number);
		if (sv->u.vec->items[9].type == T_STRING)
			write_socket(targ, "         %s\n",
			    sv->u.vec->items[9].u.string);
		break;
	    }

	    case 9:	/* tell */
	    case 10:	/* emoteto */
	    {
		if (sv->u.vec->size != 8 ||
		    sv->u.vec->items[6].type != T_STRING ||
		    sv->u.vec->items[7].type != T_STRING)
		{
			log_file("imud3",
			    "I3 tell packet: Bad.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		if (sv->u.vec->items[I3_ILUSER].type == T_STRING)
			targ = find_user((struct user *)NULL,
			    sv->u.vec->items[I3_ILUSER].u.string);

		if (targ == (struct user *)NULL)
		{
			i3_error(I3_UNK_USER, sv, (char *)NULL);
			break;
		}

		/* Got to do it... */
		users->level = L_VISITOR;
		if (!CAN_SEE(users, targ))
			i3_error(I3_UNK_USER, sv, (char *)NULL);

		FREE(targ->reply_to);
		targ->reply_to = (char *)xalloc(
		    strlen(sv->u.vec->items[6].u.string) +
		    strlen(sv->u.vec->items[I3_IRMUD].u.string) + 2,
		    "I3 reply_to");
		sprintf(targ->reply_to, "%s@%s",
		    sv->u.vec->items[6].u.string,
		    sv->u.vec->items[I3_IRMUD].u.string);

		attr_colour(targ, "incoming");
		if (i == 9)
			/* Normal tell */
			write_socket(targ, "%s@%s tells you: %s",
			    sv->u.vec->items[6].u.string,
			    sv->u.vec->items[I3_IRMUD].u.string,
			    sv->u.vec->items[7].u.string);
		else
		{
			/* Emoteto */
			char *s = i3_parse_string(
			    sv->u.vec->items[7].u.string,
			    sv->u.vec->items[6].u.string,
			    sv->u.vec->items[I3_IRMUD].u.string,
			    targ->name, LOCAL_NAME);

			write_socket(targ, " *** %s", s);
			xfree(s);
		}
		reset(targ);
		write_socket(targ, "\n");
		break;
	    }

	    case 11:	/* chanlist-reply */
	    {
		struct mapping *m;
		int i;

		if (sv->u.vec->size != 8 ||
		    sv->u.vec->items[6].type != T_NUMBER ||
		    sv->u.vec->items[7].type != T_MAPPING)
		{
			log_file("imud3", "I3: Bad chanlist-reply packet.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		i3_chanlist_id = sv->u.vec->items[6].u.number;

		m = sv->u.vec->items[7].u.map;

		for (i = m->size; i--; )
		{
			if (m->indices->items[i].type != T_STRING)
				continue;
			if (m->values->items[i].type == T_NUMBER)
				i3_remove_channel(
				    m->indices->items[i].u.string);
			else if (m->values->items[i].type == T_VECTOR &&
			    m->values->items[i].u.vec->size == 2 &&
			    m->values->items[i].u.vec->items[0].type ==
			    T_STRING &&
			    m->values->items[i].u.vec->items[1].type ==
			    T_NUMBER)
				i3_add_channel(m->indices->items[i].u.string,
				    m->values->items[i].u.vec);
		}
		break;
	    }

	    case 12:	/* channel-m */
	    case 13:	/* channel-e */
	    {
		struct user *p;
		int level = L_RESIDENT;
		struct strbuf b;

		if (sv->u.vec->size != 9 ||
		    sv->u.vec->items[6].type != T_STRING ||
		    sv->u.vec->items[7].type != T_STRING ||
		    sv->u.vec->items[8].type != T_STRING)
		{
			log_file("imud3", "I3: Bad channel-[me] packet.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		if (i3_find_channel(sv->u.vec->items[6].u.string) ==
		    (struct i3_channel *)NULL)
		{
			notify_level(L_CONSUL, "[ Unknown i3 channel: %s. ]",
			    sv->u.vec->items[6].u.string);
			i3_error(I3_UNK_CHANNEL, sv, (char *)NULL);
			level = L_CONSUL;
		}

		init_strbuf(&b, 0, "i3_chann");
		sadd_strbuf(&b, "[%s:", sv->u.vec->items[6].u.string);

		if (i == 13)	/* emote */
		{
			char *s;

			cadd_strbuf(&b, ' ');

			FUN_LINE;

			if ((s = strstr(sv->u.vec->items[8].u.string, "$N")) ==
			    (char *)NULL ||
			    (s > sv->u.vec->items[8].u.string &&
			    s[-1] == '\\'))
			{
				/* No $N in string */
				sadd_strbuf(&b, "%s*%s ",
				    sv->u.vec->items[7].u.string,
				    sv->u.vec->items[I3_IRMUD].u.string);
				add_strbuf(&b, sv->u.vec->items[8].u.string);
			}
			else
			{
				s = i3_parse_string(
				    sv->u.vec->items[8].u.string,
				    sv->u.vec->items[7].u.string,
				    sv->u.vec->items[I3_IRMUD].u.string,
				    (char *)NULL, LOCAL_NAME);
				add_strbuf(&b, s);
				xfree(s);
			}
			cadd_strbuf(&b, ']');
		}
		else
		{
			sadd_strbuf(&b, "%s@%s] ",
			    sv->u.vec->items[7].u.string,
			    sv->u.vec->items[I3_IRMUD].u.string);
			add_strbuf(&b, sv->u.vec->items[8].u.string);
		}

		FUN_LINE;

		for (p = users->next; p != (struct user *)NULL; p = p->next)
		{
			if (!IN_GAME(p) || p->level < level)
				continue;
			if (member_strlist(p->chan_earmuff,
			    sv->u.vec->items[6].u.string) !=
			    (struct strlist *)NULL)
				continue;

			attr_colour(p, "i3chan");
			write_socket(p, "%s", b.str);
			reset(p);
			write_socket(p, "\n");
		}
#ifdef I3_CHANHIST
		{
			char fname[MAXPATHLEN + 1];

			sprintf(fname, "channel/i3/%s",
			    sv->u.vec->items[6].u.string);
			log_file(fname, "%s", b.str + 2 +
			    strlen(sv->u.vec->items[6].u.string));
		}
#endif
		free_strbuf(&b);
		break;
	    }
	    case 14:	/* channel-t */
	    {
		struct user *q;
		int level = L_RESIDENT;
		struct strbuf b;
		char *s;

		if (sv->u.vec->size != 13 ||
		    sv->u.vec->items[6].type != T_STRING ||
		    sv->u.vec->items[7].type != T_STRING ||
		    sv->u.vec->items[8].type != T_STRING ||
		    sv->u.vec->items[9].type != T_STRING ||
		    sv->u.vec->items[10].type != T_STRING ||
		    sv->u.vec->items[11].type != T_STRING ||
		    sv->u.vec->items[12].type != T_STRING)
		{
			log_file("imud3", "I3: Bad channel-t packet.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		if (i3_find_channel(sv->u.vec->items[6].u.string) ==
		    (struct i3_channel *)NULL)
		{
			notify_level(L_CONSUL, "[ Unknown i3 channel: %s. ]",
			    sv->u.vec->items[6].u.string);
			i3_error(I3_UNK_CHANNEL, sv, (char *)NULL);
			level = L_CONSUL;
		}

		if (!strcmp(sv->u.vec->items[7].u.string, LOCAL_NAME))
			targ = find_user_absolute((struct user *)NULL,
			    sv->u.vec->items[8].u.string);

		init_strbuf(&b, 0, "i3 channel-t");

		sadd_strbuf(&b, "[%s: ", sv->u.vec->items[6].u.string);

		s = i3_parse_string(
		    sv->u.vec->items[9].u.string,
		    sv->u.vec->items[11].u.string,
		    sv->u.vec->items[I3_IRMUD].u.string,
		    sv->u.vec->items[12].u.string,
		    sv->u.vec->items[7].u.string);

		add_strbuf(&b, s);
		xfree(s);
		cadd_strbuf(&b, ']');

		for (q = users->next; q != (struct user *)NULL; q = q->next)
		{
			if (!IN_GAME(q) || q->level < level)
				continue;
			if (member_strlist(q->chan_earmuff,
			    sv->u.vec->items[6].u.string) !=
			    (struct strlist *)NULL)
				continue;

			attr_colour(q, "i3chan");

			if (q != targ)
				write_socket(q, "%s", b.str);
			else
			{
				s = i3_parse_string(
				    sv->u.vec->items[10].u.string,
				    sv->u.vec->items[11].u.string,
				    sv->u.vec->items[I3_IRMUD].u.string,
				    sv->u.vec->items[12].u.string,
				    sv->u.vec->items[7].u.string);

				write_socket(q, "[%s: %s]",
				    sv->u.vec->items[6].u.string, s);
				xfree(s);
			}
			reset(q);
			write_socket(q, "\n");
		}
#ifdef I3_CHANHIST
		{
			char fname[MAXPATHLEN + 1];

			sprintf(fname, "channel/i3/%s",
			    sv->u.vec->items[6].u.string);
			log_file(fname, "%s", b.str + 2 +
			    strlen(sv->u.vec->items[6].u.string));
		}
#endif
		free_strbuf(&b);
		break;
	    }

	    case 15:	/* chan-who-req */
	    {
		struct strbuf t;
		struct i3_channel *i;
		
		if (sv->u.vec->size != 7)
		{
			log_file("imud3",
			    "I3 chan-who-req packet: not 7 elements.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		if (sv->u.vec->items[6].type != T_STRING)
		{
			log_file("imud3",
			    "I3 chan-who-req packet: channel not a string.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		if ((i = i3_find_channel(sv->u.vec->items[6].u.string)) ==
		    (struct i3_channel *)NULL || i->listening != 1)
		{
			i3_error(I3_UNK_CHANNEL, sv, (char *)NULL);
			break;
		}

		init_strbuf(&t, 0, "chan-who-req t");
		add_strbuf(&t, i3_reply_header("chan-who-reply", sv));
		i3_add_strbuf_string(&t, sv->u.vec->items[6].u.string);

		add_strbuf(&t, "({");
		users->level = L_VISITOR;
		for (targ = users->next; targ != (struct user *)NULL;
		    targ = targ->next)
		{
			if (!IN_GAME(targ) || !CAN_SEE(users, targ))
				continue;
			if (member_strlist(targ->chan_earmuff,
			    sv->u.vec->items[6].u.string) !=
			    (struct strlist *)NULL)
				continue;
			i3_add_strbuf_string(&t, targ->name);
		}
		add_strbuf(&t, "}),})");

		i3_send_string(t.str);
		free_strbuf(&t);
		break;
	    }

	    case 16:	/* chan-who-reply */
	    {
		struct vector *v;
		int i, flag;

		if (targ == (struct user *)NULL)
			break;
		
		if (sv->u.vec->size != 8)
		{
			log_file("imud3",
			    "I3 chan-who-reply packet: not 8 elements.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		if (sv->u.vec->items[6].type != T_STRING)
		{
			log_file("imud3",
			    "I3 chan-who-req packet: channel not a string.");
			i3_error(I3_BAD_PKT, sv, (char *)NULL);
			break;
		}

		write_socket(targ, "Channel who reply from %s "
		    "for channel %s:\n",
		    sv->u.vec->items[I3_IRMUD].u.string,
		    sv->u.vec->items[6].u.string);

		if (sv->u.vec->items[7].type != T_VECTOR)
		{
			write_socket(targ, "Nobody listening.\n");
			break;
		}

		v = sv->u.vec->items[7].u.vec;

		for (i = flag = 0; i < v->size; i++)
		{
			if (v->items[i].type != T_STRING)
				continue;

			if (flag)
				write_socket(targ, ", ");
			else
				flag = 1;

			write_socket(targ, "%s", v->items[i].u.string);
		}
		write_socket(targ, "\n");
		break;
	    }

	    default:
		log_file("imud3", "I3: unknown service request: %s",
		    sv->u.vec->items[0].u.string);
		i3_error(I3_UNK_TYPE, sv, (char *)NULL);
		break;
	}
	FUN_END;
}

static void
i3_recon_tm(struct event *ev)
{
	i3_recon_ev = -1;
	i3_recon = 0;
}

void
i3_event_connect(struct event *ev)
{
	/* Is link up ? */
	if (i3_router.fd != -1)
		return;

	/* If recon event is running then the link failed within 10 minutes
	 * of being set up - so kill the event, and wait 10 minutes before
	 * trying again. ,*/
	if (i3_recon_ev != -1 || i3_startup() == 0)
	{
		if (!i3_recon)
			i3_recon = 60;
		else
			i3_recon *= 2;

		if (i3_recon_ev != -1)
		{
			remove_event(i3_recon_ev);
			i3_recon_ev = -1;
			if (i3_recon < 600)
				i3_recon = 600;
		}
		else if (i3_recon < 60)
			i3_recon = 60;
		else if (i3_recon > 3600)
			i3_recon = 3600;

		add_event(create_event(), i3_event_connect, i3_recon,
		    "i3-recon");
		return;
	}

	/* In ten minutes, we will assume the link is stable and reset the
	 * retry counter.. */
	i3_recon_ev = add_event(create_event(), i3_recon_tm, 600, "recon_tm");
}

void
i3_lostconn(struct tcpsock *s)
{
	log_perror("I3");
	log_file("syslog", "I3: Socket error, closing down.");
	notify_level(L_CONSUL, "[ Connection to I3-router lost. ]");
	close_tcpsock(s);
	free_tcpsock(s);

	i3_event_connect((struct event *)NULL);
}

void
i3_incoming(struct tcpsock *s)
{
	int i;
	char *buf;
	struct svalue *sv;

	FUN_START("i3_incoming");

	if ((i = scan_tcpsock(s)) <= 0)
	{
		/* Error on socket.. :( */
		FUN_ARG("socket error");
		
		i3_lostconn(s);

		FUN_END;
		return;
	}

	if ((buf = process_tcpsock(s)) == (char *)NULL)
	{
		/* We have not yet received a complete packet. */
		FUN_END;
		return;
	}

	FUN_LINE;

	/* Now the fun starts.. */
	if (s->flags & TCPSOCK_BINHEAD)
	{
		/* Waiting to get the length of an incoming packet.. */
		int len;

		FUN_ARG("binhead");

		s->ib_offset = 0;
		len = (int)ntohl(*(long *)buf);

#ifdef I3_DEBUG
		log_file("debug", "I3 header: [%x %x %x %x] = %d",
		    buf[0], buf[1], buf[2], buf[3], len);
#endif

		if (len <= 0 || len > I3_MAXLEN)
		{
			log_file("error",
			    "I3 header: packet size invalid (%d).\n", len);
			/* Ignore this header.. */
			FUN_END;
			return;
		}

		/* Allocate a new input buffer which has the same size as the
		 * packet we are expecting. */
		xfree(s->input_buffer);

		/* Allocate one byte more than we need but only set the
		 * ib_size to len.
		 * I use the extra byte later to null terminate the received
		 * string.
		 */
		s->input_buffer = (char *)xalloc(len + 1, "i3_router_buf");
		s->ib_size = len;

		/* Now get the packet.. */
		s->flags &= ~TCPSOCK_BINHEAD;


		FUN_END;
		return;
	}

	/* A packet has arrived. */

	/* This is what the extra byte was for ;-) */
	buf[s->ib_size] = '\0';

#ifdef I3_DEBUG
	log_file("debug", "I3: Recv: %s", buf);
#endif

	FUN_ARG(buf);

	sv = string_to_svalue(buf, "i3_incoming decode");

	if (sv == (struct svalue *)NULL)
		/* Buf may have been destroyed by the decode, but... */
		log_file("imud3", "I3: decode failed (%s).", buf);
	else
	{
		i3_svalue(sv);
		free_svalue(sv);
	}

	if (s->flags & TCPSOCK_SHUTDOWN)
	{
		/* This flag could have been set because the router has asked
		 * us to use another one etc.
		 */
		close_tcpsock(s);
		free_tcpsock(s);

		if (s->flags & TCPSOCK_RESTART)
			i3_startup();

		FUN_END;
		return;
	}

	/* Prepare the socket for another packet header. */

	xfree(s->input_buffer);
	s->flags |= TCPSOCK_BINHEAD;
	s->input_buffer = (char *)xalloc(sizeof(long), "i3_router hbuf");
	s->ib_size = sizeof(long);
	s->ib_offset = 0;
	FUN_END;
}

struct i3_host *
lookup_i3_host(char *host, char *req)
{
	struct i3_host *h;

#ifndef IMUD3_LOOPBACK
	if (i3_router.fd == -1 || crouter == (struct strlist *)NULL)
		return (struct i3_host *)NULL;
#endif

	if ((h = i3_find_host(host, 0)) == (struct i3_host *)NULL)
		return (struct i3_host *)NULL;

	if (h->state != -1)
		return (struct i3_host *)NULL;

	if (map_string(h->services, req) == (struct svalue *)NULL)
		return (struct i3_host *)NULL;

	return h;
}

#define I3_DOWN_CHECK(h) \
	if (h->state != -1) \
	{ \
		write_socket(p, "%s is currently down.\n", \
		    h->name); \
		return -1; \
	}

#define I3_SUPPORT_CHECK(h, str) \
	if (map_string(h->services, str) == (struct svalue *)NULL) \
	{ \
		write_socket(p, "%s does not support the %s service.\n", \
		    h->name, str); \
		return -1; \
	}

#ifdef IMUD3_LOOPBACK
#define I3_ROUTER_CHECK
#else /* IMUD3_LOOPBACK */
#define I3_ROUTER_CHECK \
	if (i3_router.fd == -1 || crouter == (struct strlist *)NULL) \
		return 0
#endif /* IMUD3_LOOPBACK */

#define I3_GETHOST(hptr, host, cmd) \
		I3_ROUTER_CHECK; \
		if ((hptr = i3_find_host(host, 0)) == (struct i3_host *)NULL) \
			return 0; \
		I3_DOWN_CHECK(hptr) \
		I3_SUPPORT_CHECK(hptr, cmd) 

#define I3_TRSUCC write_socket(p, "I3: Request transmitted.\n")

void
i3_cmd_who(struct user *p, struct i3_host *h)
{
	char *str;

	str = i3_packet_header("who-req", p->rlname, h->name, (char *)NULL);
	strcat(str, "})");

	i3_send_string(str);
}

void
i3_cmd_finger(struct user *p, struct i3_host *h, char *targ)
{
	char *str;

	str = i3_packet_header("finger-req", p->rlname, h->name, (char *)NULL);
	i3_add_string(str, lower_case(targ));
	strcat(str, "})");

	i3_send_string(str);
}

void
i3_cmd_locate(struct user *p, char *arg)
{
	char *c;

	c = i3_packet_header("locate-req", p->rlname, (char *)NULL,
	    (char *)NULL);
	i3_add_string(c, lower_case(arg));
	strcat(c, "})");

	i3_send_string(c);
}

void
i3_cmd_tell(struct user *p, struct i3_host *h, char *targ, char *msg)
{
	struct strbuf m;
	int emote = 0;

	if (*msg == ':')
		msg++, emote = 1;

	init_strbuf(&m, 0, "i3_cmd_tell");

	add_strbuf(&m, i3_packet_header(emote ? "emoteto" : "tell", p->rlname,
	    h->name, lower_case(targ)));

	i3_add_strbuf_string(&m, p->name);
	if (emote)
	{
		char *s;

		add_strbuf(&m, "\"$N ");
		s = code_string(msg);
		add_strbuf(&m, s);
		xfree(s);
		add_strbuf(&m, "\",");
	}
	else
		i3_add_strbuf_string(&m, msg);

	add_strbuf(&m, "})");

	i3_send_string(m.str);
	free_strbuf(&m);
}

/* XXX - implement channel-t packets ? */
int
i3_cmd_channel(struct user *p, char *chan, char *msg)
{
	struct strbuf m;
	struct i3_channel *i;
	int emote = 0;

	if ((i = i3_find_channel(chan)) == (struct i3_channel *)NULL)
	{
		write_socket(p, "No such channel, %s.\n", chan);
		return 0;
	}

	if (i->listening != 1)
	{
		write_socket(p, "The talker is tuned out of that channel.\n");
		return 0;
	}

	if (member_strlist(p->chan_earmuff, chan) != (struct strlist *)NULL)
	{
		write_socket(p, "You are not listening to channel %s.\n",
		    chan);
		return 0;
	}

#ifdef I3_CHANHIST
	if (*msg == '!')
	{
		/* Channel History */
		dump_file(p, "log/channel/i3", chan, DUMP_TAIL);
		return 1;
	}
#endif

	I3_ROUTER_CHECK;

	init_strbuf(&m, 0, "i3_cmd_channel");

	if (*msg == '@')
	{
		struct i3_host *h;
		msg++;

		I3_GETHOST(h, msg, "channel")

		/* Channel who */
		add_strbuf(&m, i3_packet_header("chan-who-req", p->rlname,
		    h->name, (char *)NULL));
		i3_add_strbuf_string(&m, chan);
	}
	else
	{
		if (*msg == ':')
			msg++, emote = 1;
		else if (*msg == '.' && msg[1] != '.')
		{
			char *q;
			char *argv[2];
			int argc;

			argc = 1;
			if ((q = strchr(++msg, ' ')) != (char *)NULL)
			{
				*q++ = '\0';
				argc = 2;
				argv[1] = q;
			}
			argv[0] = msg;

			if ((q = expand_feeling(p, argc, argv, 0)) ==
			    (char *)NULL || q == (char *)1)
			{
				write_socket(p,
				    "Error in channel feeling, %s.\n",
				    argv[0]);
				return 0;
			}
			if ((msg = strchr(q, ' ')) == (char *)NULL)
			{
				write_socket(p,
				    "Error in channel feeling, %s.\n",
				    argv[0]);
				return 0;
			}
			msg++;
			emote++;
		}

		add_strbuf(&m, i3_packet_header(emote ? "channel-e" :
		    "channel-m", p->rlname, (char *)NULL, (char *)NULL));
		i3_add_strbuf_string(&m, chan);
		i3_add_strbuf_string(&m, p->name);
		if (emote)
		{
			char *s;
			add_strbuf(&m, "\"$N ");
			s = code_string(msg);
			add_strbuf(&m, s);
			xfree(s);
			add_strbuf(&m, "\",");
		}
		else
			i3_add_strbuf_string(&m, msg);
	}
	
	add_strbuf(&m, "})");

	i3_send_string(m.str);
	free_strbuf(&m);

	I3_TRSUCC;

	return 1;
}

#endif /* IMUD3_SUPPORT */

