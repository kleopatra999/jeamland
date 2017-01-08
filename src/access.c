/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	access.c
 * Function:	Access control support.
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#ifndef NeXT
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include "jeamland.h"

struct banned_site *bannedsites = (struct banned_site *)NULL;

int
offensive(char *buf)
{
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

struct banned_site *
sitebanned(char *ipnum)
{
	char *c;
	struct banned_site *h;

	for (h = bannedsites; h != (struct banned_site *)NULL; h = h->next)
		if ((c = strchr(h->host, '*')) != (char *)NULL &&
		    !strncmp(h->host, ipnum, c - h->host))
			return h;
		else if (!strcmp(h->host, ipnum))
			return h;
		else if (strstr(ipnum, h->host) != (char *)NULL)
			return h;
	return (struct banned_site *)NULL;
}

struct banned_site *
create_banned_site()
{
	struct banned_site *h = (struct banned_site *)xalloc(sizeof(
	    struct banned_site), "*banned site");
	h->host = h->reason = (char *)NULL;
	h->next = (struct banned_site *)NULL;
	return h;
}

void
free_banned_site(struct banned_site *h)
{
	FREE(h->host);
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
		if (!strncmp(buf, "site ", 5))
		{
			struct svalue *sv;

			if ((sv = decode_one(buf, "decode_one siteban")) ==
                            (struct svalue *)NULL)
                                continue;
                        if (sv->type == T_VECTOR && sv->u.vec->size == 3 &&
                            sv->u.vec->items[0].type == T_STRING &&
			    sv->u.vec->items[1].type == T_STRING &&
			    sv->u.vec->items[2].type == T_NUMBER)
			{
				h = create_banned_site();
				h->host = string_copy(
				    sv->u.vec->items[0].u.string,
				    "*bsite host");
				h->reason = string_copy(
				    sv->u.vec->items[1].u.string,
				    "*bsite reason");
				h->level = sv->u.vec->items[2].u.number;
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

	fprintf(fp, "# site, reason, level\n");
	for (h = bannedsites; h != (struct banned_site *)NULL; h = h->next)
	{
		char *site = code_string(h->host);
		char *reason = code_string(h->reason);
		fprintf(fp, "site ({\"%s\",\"%s\",%d,})\n", site, reason,
		    h->level);
		xfree(site);
		xfree(reason);
	}
	fclose(fp);
}

