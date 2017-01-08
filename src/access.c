/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	access.c
 * Function:	Access control support.
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "jeamland.h"

struct banned_site *bannedsites = (struct banned_site *)NULL;

int
offensive(char *buf)
{
	/* List of substrings considered offensive */
	static const char *offend[] = {
	    "fuck", "dick", "cunt", "arse", "shit", "pussy", "slut", "rape",
	    "penis", "slag", "bitch", "clit", "cock", "fart",
	    "bastard", "wank", "tosser", "twat", "bollocks", "bollox",
	    (char *)NULL };
        int i;

        for (i = 0; offend[i] != (char *)NULL; i++)
		if (strstr(buf, offend[i]) != (char *)NULL)
			return 1;
        return 0;
}

char *
banished(char *name)
{
	static char buf[MAXPATHLEN + 1];
	time_t tim;
	FILE *fp;

	sprintf(buf, "banish/%s", name);
	if ((tim = file_time(buf)) == -1)
		return (char *)NULL;
	if ((fp = fopen(buf, "r")) == (FILE *)NULL)
	{
		log_file("error", "Could not open: %s", buf);
		return (char *)NULL;
	}
	fgets(buf, sizeof(buf) - 20, fp);
	fclose(fp);
	strcat(buf, " ");
	strcat(buf, nctime(&tim));
	return buf;
}

void
banish(struct user *p, char *name)
{
	char buf[MAXPATHLEN + 1];
	FILE *fp;

	if (banished(name) != (char *)NULL)
		return;

	sprintf(buf, "banish/%s", name);
	if ((fp = fopen(buf, "w")) == (FILE *)NULL)
	{
		log_file("error", "Could not open: %s", buf);
		return;
	}
	fprintf(fp, "%s", p->rlname);
	fclose(fp);
	log_file("banish", "%s by %s", name, p->capname);
}

char *
unbanish(char *name)
{
	char buf[MAXPATHLEN + 1];
	char *l;

	if ((l = banished(name)) == (char *)NULL)
		return (char *)NULL;
	sprintf(buf, "banish/%s", name);
	unlink(buf);
	return l;
}

/**********************
 * Siteban code.
 */

/*
 * Do the necessary parsing of a siteban argument.
 * The argument will be an internet address[/netmask]
 * and the netmask part may be specified as a network mask or as a plain
 * number.
 * The function will also accept a wildcard version of a hostname,
 * e.g. 127.0.*.*
 *
 * Return values:	 1	success.
 *			-1	bad netmask.
 *			-2	bad host.
 */
int
parse_banned_site(char *addr, unsigned long *a, unsigned long *n)
{
	unsigned long host;
	unsigned long netmask;
	char *p;

	/* First see if we have been passed a wildcard version */
	if (strchr(addr, '*') != (char *)NULL)
	{
		/* Need to do two things, find out how many bits are 
		 * significant and replace the *'s with 0's */
		int i;

		for (i = 0, p = addr; *p != '*'; p++)
			if (*p == '.')
				i++;
		if (i > 3)
			return -2;

		for (; *p != '\0'; p++)
			if (*p == '*')
				*p = '0';

		i *= 8;

		if (!i)
			netmask = 0;
		else
			netmask = htonl(0xffffffff << (32 - i));
	}
	else
	{
		/* Default netmask is 32 bits */
		netmask = 0xffffffff;

		/* First, see if we have specified a netmask. */
		if ((p = strrchr(addr, '/')) != (char *)NULL)
		{
			*p++ = '\0';

			/* Now, could have specified this as a dotted quad
			 * or as a number.. */
			if (strchr(p, '.') == (char *)NULL ||
			    (netmask = inet_addr(p)) == -1)
			{
				/* Assume it's a number..
				 * and construct the appropriate netmask */
				int bits = atoi(p);

				if (bits > 32 || bits < 0)
					return -1;
				if (!bits)
					netmask = 0;
				else
					netmask = htonl(0xffffffff <<
					    (32 - bits));
			}
		}
	}

	/* If a mask of 0 is given, then no host will match and so ignore
	 * the ip number part of the supplied argument, it doesn't matter */
	if (!netmask)
		host = 0L;
	else
		host = inet_addr(addr);

	if (host == -1)
		return -2;

	*a = host;
	*n = netmask;

	return 1;
}

struct banned_site *
sitebanned(unsigned long addr)
{
	struct banned_site *h, *last;
	int flag = 0;

	for (h = bannedsites; h != (struct banned_site *)NULL; h = h->next)
	{
		if ((addr & h->netmask) == h->addr)
		{
			if (h->level == BANNED_INV)
				return (struct banned_site *)NULL;
			if (!flag)
				flag = 1, last = h;
		}
	}
	if (flag)
		return last;
	return (struct banned_site *)NULL;
}

void
dump_siteban_table(struct user *p, int entry)
{
	struct banned_site *h;
	char b[16];
	int i, flag;

	if (bannedsites == (struct banned_site *)NULL)
	{
		write_socket(p, "No entries in siteban table.\n");
		return;
	}

	write_socket(p, "      Host            Netmask           Level\n");
	write_socket(p, "---------------------------------------------\n");

	i = flag = 0;
	for (h = bannedsites; h != (struct banned_site *)NULL; h = h->next)
	{
		i++;

		if (entry != -1 && entry != i)
			continue;

		strcpy(b, ip_addrtonum(h->addr));
		write_socket(p, "%-3d %c %-15s/%-15s   ", i,
		    h->level == BANNED_INV ? '!' : ' ', b,
		    ip_addrtonum(h->netmask));

		switch (h->level)
		{
		    case BANNED_ALL:
			write_socket(p, "All.\n");
			break;
		    case BANNED_ABA:
			write_socket(p, "All but administrators.\n");
			break;
		    case BANNED_NEW:
			write_socket(p, "New users.\n");
			break;
		    default:
			write_socket(p, "\n");
		}
		flag = 1;
		if (entry != -1)
		{
			write_socket(p, "%s\n", h->reason);
			return;
		}
	}
	if (!flag)
		write_socket(p, "No matching entries found.\n");
}

struct banned_site *
create_banned_site()
{
	struct banned_site *h = (struct banned_site *)xalloc(sizeof(
	    struct banned_site), "*banned site");
	h->addr = h->netmask = 0;
	h->reason = (char *)NULL;
	h->next = (struct banned_site *)NULL;
	return h;
}

void
free_banned_site(struct banned_site *h)
{
	FREE(h->reason);
	xfree(h);
}

void
load_banned_hosts()
{
	FILE *fp;
	struct banned_site *h, *i, **hh;
	char *buf;
	struct stat st;

	for (h = bannedsites; h != (struct banned_site *)NULL; h = i)
	{
		i = h->next;
		free_banned_site(h);
	}
	bannedsites = (struct banned_site *)NULL;

	hh = &bannedsites;

	if ((fp = fopen(F_SITEBAN, "r")) == (FILE *)NULL)
		return;
	if (fstat(fileno(fp), &st) == -1)
		fatal("Couldn't stat open file!");
	if (!st.st_size)
	{
		unlink(F_SITEBAN);
		return;
	}
	buf = (char *)xalloc(st.st_size, "banned tmp");
	while (fgets(buf, (int)st.st_size, fp) != (char *)NULL)
	{
		if (ISCOMMENT(buf))
			continue;
		if (!strncmp(buf, "rule ", 5))
		{
			struct svalue *sv;

			if ((sv = decode_one(buf, "decode_one siteban")) ==
                            (struct svalue *)NULL)
                                continue;
                        if (sv->type == T_VECTOR && sv->u.vec->size == 4 &&
                            sv->u.vec->items[0].type == T_UNUMBER &&
			    sv->u.vec->items[1].type == T_UNUMBER &&
			    sv->u.vec->items[2].type == T_STRING &&
			    sv->u.vec->items[3].type == T_NUMBER)
			{
				h = create_banned_site();
				h->addr = sv->u.vec->items[0].u.unumber;
				h->netmask = sv->u.vec->items[1].u.unumber;
				h->reason = string_copy(
				    sv->u.vec->items[2].u.string,
				    "*bsite reason");
				h->level = sv->u.vec->items[3].u.number;
				*hh = h;
				hh = &h->next;
			}
			else
				log_file("error", "Error in siteban line: "
				    "line '%s'", buf);
			free_svalue(sv);
		}
	}
	xfree(buf);
	fclose(fp);
}

void
save_banned_hosts()
{
	FILE *fp;
	struct banned_site *h;

	if (bannedsites == (struct banned_site *)NULL)
	{
		unlink(F_SITEBAN);
		return;
	}
	if ((fp = fopen(F_SITEBAN, "w")) == (FILE *)NULL)
		return;

	fprintf(fp, "# addr, netmask, reason, level\n");
	for (h = bannedsites; h != (struct banned_site *)NULL; h = h->next)
	{
		char *reason = code_string(h->reason);
		fprintf(fp, "rule ({L%lx,L%lx,\"%s\",%d,})\n", h->addr,
		    h->netmask, reason, h->level);
		xfree(reason);
	}
	fclose(fp);
}

void
add_siteban(unsigned long addr, unsigned long netmask, int lev, char *reason)
{
	struct banned_site *h;

	h = create_banned_site();

	h->addr = addr;
	h->netmask = netmask;
	h->level = lev;
	h->reason = reason;

	h->next = bannedsites;
	bannedsites = h;

	save_banned_hosts();
}

int
remove_siteban(unsigned long addr, unsigned long netmask)
{
	struct banned_site **h, *j;

	for (h = &bannedsites; *h != (struct banned_site *)NULL;
	    h = &((*h)->next))
	{
		if ((*h)->addr == addr && (*h)->netmask == netmask)
		{
			j = *h;
			*h = j->next;
			free_banned_site(j);
			save_banned_hosts();
			return 1;
		}
	}
	return 0;
}

