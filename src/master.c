/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	master.c
 * Function:	Main security functions
 *		Also the level database
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <time.h>
#include "jeamland.h"

static struct db_entry *mastersave = (struct db_entry *)NULL;
extern time_t current_time;
extern struct grupe *sys_grupes;

static struct db_entry *
create_db_entry()
{
	struct db_entry *p = (struct db_entry *)xalloc(sizeof(struct db_entry),
	    "*db_entry");
	p->user = p->setter = (char *)NULL;
	p->level = 0;
	p->next = (struct db_entry *)NULL;
	return p;
}

static void
free_db_entry(struct db_entry *p)
{
	FREE(p->user);
	FREE(p->setter);
	xfree(p);
}

static void
add_db_entry(struct db_entry *p)
{
	p->next = mastersave;
	mastersave = p;
}

static void
load_mastersave_defaults()
{
	void store_mastersave(void);

	log_file("syslog", "Loading mastersave defaults.");
	mastersave = create_db_entry();
	mastersave->user = string_copy(ROOT_USER, "*db root user");
	mastersave->setter = string_copy(ROOT_USER, "*db root setter");
	mastersave->level = L_OVERSEER;
	store_mastersave();
}

void
load_mastersave()
{
	FILE *fp;
	struct stat st;
	char *buf;
	struct db_entry *p, *q;

	for (p = mastersave; p != (struct db_entry *)NULL; p = q)
	{
		q = p->next;
		free_db_entry(p);
	}
	if ((fp = fopen(F_MASTERSAVE, "r")) == (FILE *)NULL)
	{
		log_file("syslog", "Mastersave non-existant.");
		load_mastersave_defaults();
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
		log_file("syslog", "Mastersave empty.");
		load_mastersave_defaults();
		return;
	}

	mastersave = (struct db_entry *)NULL;
	p = (struct db_entry *)NULL;
	buf = (char *)xalloc((size_t)st.st_size, "load mastersave");
	while (fgets(buf, (int)st.st_size - 1, fp) != (char *)NULL)
        {
		if (ISCOMMENT(buf))
			continue;
		if (!strncmp(buf, "user ", 5))
		{
			struct svalue *sv;

			if ((sv = decode_one(buf, "decode_one mastersave")) ==
			    (struct svalue *)NULL)
				continue;
			if (sv->type == T_VECTOR && sv->u.vec->size == 3 &&
			    sv->u.vec->items[0].type == T_STRING &&
			    sv->u.vec->items[1].type == T_NUMBER &&
			    sv->u.vec->items[2].type == T_STRING)
			{
				p = create_db_entry();
				p->user = string_copy(
				    sv->u.vec->items[0].u.string,
				    "*db user");
				p->level = sv->u.vec->items[1].u.number;
				p->setter = string_copy(
				    sv->u.vec->items[2].u.string,
				    "*db setter");
				add_db_entry(p);
			}
			else
				log_file("error", "Error in mastersave file: "
				    "line '%s'", buf);
			free_svalue(sv);
			continue;
		}
		if (!strncmp(buf, "grupe ", 6))
		{
			restore_grupes(&sys_grupes, buf);
			continue;
		}
	}
	fclose(fp);
	xfree(buf);
}

void
store_mastersave()
{
	FILE *fp;
	struct db_entry *p;

	if ((fp = fopen(F_MASTERSAVE, "w")) == (FILE *)NULL)
	{
		log_perror("store mastersave");
		return;
	}
	fprintf(fp, "# user, level, level_setter\n");
	for (p = mastersave; p != (struct db_entry *)NULL; p = p->next)
		fprintf(fp, "user ({\"%s\",%d,\"%s\",})\n", p->user, p->level,
		    p->setter);
	store_grupes(fp, sys_grupes);
	fclose(fp);
}

static struct db_entry *
get_db_entry(char *name)
{
	struct db_entry *p;

	for (p = mastersave; p != (struct db_entry *)NULL; p = p->next)
		if (!strcasecmp(p->user, name))
			return p;
	return (struct db_entry *)NULL;
}

int
query_real_level(char *name)
{
	struct db_entry *p;

	if ((p = get_db_entry(name)) == (struct db_entry *)NULL)
		return -1;
	return p->level;
}

char *
query_level_setter(char *name)
{
	struct db_entry *p;

	if ((p = get_db_entry(name)) == (struct db_entry *)NULL)
		return (char *)NULL;
	return p->setter;
}

void
change_user_level(char *user, int level, char *setter)
{
	struct db_entry **p, *q;

	for (p = &mastersave; *p != (struct db_entry *)NULL;
	    p = &((*p)->next))
		if (!strcasecmp((*p)->user, user))
		{
			if (level <= L_RESIDENT)
			{
				q = *p;
				*p = q->next;
				free_db_entry(q);
			}
			else
			{
				(*p)->level = level;
				COPY((*p)->setter, setter, "*db setter");
			}
			store_mastersave();
			return;
		}

	if (level <= L_RESIDENT)
		return;
	q = create_db_entry();
	COPY(q->user, user, "*db user");
	COPY(q->setter, setter, "*db setter");
	q->level = level;
	add_db_entry(q);
	store_mastersave();
}

int
level_users(struct vecbuf *vbuf, int level)
{
	struct db_entry *q;

	if (level <= L_RESIDENT || level > L_MAX)
		return 0;

	for (q = mastersave; q != (struct db_entry *)NULL; q = q->next)
	{
		if (q->level == level)
		{
			int i = vecbuf_index(vbuf);
			vbuf->vec->items[i].type = T_STRING;
			vbuf->vec->items[i].u.string = string_copy(q->user,
			    "level_users item");
		}
	}
	return 1;
}

int
count_level_users(int level)
{
	struct db_entry *q;
	int num;

	for (num = 0, q = mastersave; q != (struct db_entry *)NULL;
	    q = q->next)
		if (q->level == level)
			num++;
	return num;
}

int
count_all_users()
{
	char fname[MAXPATHLEN + 1];
	char a;
	int count, i;

	for (count = 0, a = 'a'; a <= 'z'; a++)
	{
		sprintf(fname, "users/%c", a);

		if ((i = count_files(fname)) == -1)
			log_file("error",
			    "Count users, can't open directory %s.", fname);
		else
			count += i;
	}
	return count;
}

/*
 * Read and write accesses for different levels..
 * Used for 'ls', 'cat' etc.
 */
int
valid_path(struct user *p, char *path, enum valid_mode action)
{
	if (path[0] == '/' || (path[0] == '.' && strcmp(path, ".")) ||
	    strstr(path, "//") != (char *)NULL || strstr(path, "..")
	    != (char *)NULL)
		return 0;

	/* Full read and write for root */
	if (ISROOT(p))
		return 1;

	/* Nobody can access the secure directory */
	if (!strncmp(path, "secure", 6))
		return 0;

	if (p->level > L_CONSUL)
	{
		if (action == VALID_READ)
			return 1;
		if (!strcmp(path, F_MASTERSAVE) ||
		    !strcmp(path, F_SUDO) ||
		    !strcmp(path, F_CRON) ||
		    !strncmp(path, "users", 5) ||
		    !strncmp(path, "log", 3))
			return 0;
		return 1;
	}

	/* Block access to intermud3 db */
	if (!strcmp(path, F_IMUD3))
		return 0;

	if (p->level > L_WARDEN)
	{
		if (!strncmp(path, "log/channel", 11) ||
		    !strncmp(path, "dead_ed", 7) ||
		    !strncmp(path, "mail", 4))
				return 0;

		if (action == VALID_READ)
		{
			if (!strncmp(path, "help/overseer", 13))
				return 0;
			return 1;
		}
		if (!strncmp(path, "help", 4))
			return 1;
		return 0;
	}
	else if (p->level < L_WARDEN)
		return 0;

	/* Remaining code applies to Wardens */

	/* Wardens can not write anywhere */
	if (action == VALID_WRITE)
		return 0;

	/* Wardens cannot read these dirs.. */
	if (!strncmp(path, "users", 5) ||
	    !strncmp(path, "board", 5) ||
	    !strncmp(path, "mail", 4) ||
	    !strncmp(path, "log/secure", 10) ||
	    !strncmp(path, "banish", 6) ||
	    !strncmp(path, "dead_ed", 7) ||
	    !strncmp(path, "log/channel", 11) ||
	    !strncmp(path, "help/consul", 11) ||
	    !strncmp(path, "help/overseer", 13))
		return 0;
	return 1;
}

void
valid_room_name(struct user *p, char *r)
{
	char *s;

	/* If the room name begins with a _ character then it is a system room
	 * and pretty much anything is allowed.
	 */
	if (*r == '_')
		return;

	/* Get a writeable copy of r */
	r = string_copy(r, "valid_room_name");

	/* Find a # character in the room name */
	if ((s = strchr(r, '#')) != (char *)NULL)
		*s = '\0';

	if (!exist_user(r))
		write_socket(p,
		    "WARNING: Room naming convention violated.\n"
		    "         Non-system rooms should be named after the\n"
		    "         owner followed by a number.\n"
		    "         So user blog's home room should be named 'blog'\n"
		    "         and any subsequent rooms blog acquires should\n"
		    "         be named 'blog#1', 'blog#2' etc.\n"
		    "         See 'help roomnames' for more information.\n\n"
		    "User '%s' does not exist.\n", r);

	/* Need to free the writeable copy. */
	xfree(r);
}

char *
random_password(int len)
{
#define RP_BUFLEN	15
	static char buf[RP_BUFLEN];
	static char *choise =
	    "./abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	int clen = strlen(choise) - 1;

#ifdef DEBUG
	if (len <= 0 || len >= RP_BUFLEN)
		fatal("Bad args to random_password.");
#endif

	buf[len] = '\0';
	while (len--)
		buf[len] = choise[rand() % clen];
	return buf;
#undef RP_BUFLEN
}

char *
crypted(char *c)
{
	static char buf[15];

	strcpy(buf, crypt(c, random_password(2)));
	return buf;
}

void
insecure_passwd_text(struct user *p, int i)
{
#ifdef SECURE_PASSWORDS
	write_socket(p, "\n"
	    "Password security requirements:\n"
	    "\t1. Password must be at least six characters in length\n"
	    "\t2. Password must contain at least one a-z character\n"
	    "\t   and at least one non a-z character.\n"
	    "\t3. Password must not contain any character repeated\n"
	    "\t   more than thrice.\n"
	    "\t4. Password must not be a pseudo-anagram of your\n"
	    "\t   username.\n"
	    "\t5. Password must not contain consecutive sequences of\n"
	    "\t   more than three characters.\n"
	    "\t6. Password must not contain more than four\n"
	    "\t   characters from your old password.\n"
	    "\n"
	    "Your password failed to meet the above requirements on "
	    "point: %d.\n\n", i);
#else
	write_socket(p,
	    "\nPassword too short, must be at least 4 characters.\n");
#endif
}

/* Check whether a password is considered secure.
 * Current return values are:
 * 0   Password is ok.
 * 1   Password has less than six characters.
 * 2   Password does not have at least one a-z and at least one non a-z
 *      character.
 * 3   Password contains at least one character which is repeated more than
 *      thrice.
 * 4   Password is a pseudo-anagram of the users name.
 * 5   Password contains too many sequential characters.
 * 6   Password contains more than four characters from the old password.
 */
int
secure_password(struct user *p, char *passwd, char *old)
{
#ifdef SECURE_PASSWORDS
	static char keyboard[] = "qwertyuiopasdfghjkl;zxcvbnm,./";
	char table[9][2];
	int index, nalpha, alpha, i;
	char *q, lq;

	/* Easy check first! */
	if (strlen(passwd) < 6)
		return 1;

	/* Check for characters from the old password. */
	if (old != (char *)NULL)
		for (i = 0, q = old; *q != '\0'; q++)
			if (strchr(passwd, *q) != (char *)NULL && ++i > 4)
					return 6;

	/* Build the frequency table. */
	index = nalpha = alpha = 0;
	for (q = passwd; *q != '\0' && q - passwd <= 8; q++)
	{
		lq = tolower(*q);

		if (lq < 'a' || lq > 'z')
			nalpha++;
		else
			alpha++;

		for (i = index; i--; )
			if (table[i][0] == lq)
			{
				table[i][1]++;
				break;
			}
		if (i < 0)
		{
			table[index][0] = lq;
			table[index++][1] = 1;
		}
	}

	/* Do we have at least one alpha and at least one non-alpha ? */
	if (!alpha || !nalpha)
		return 2;

	/* Any character repeated more than thrice ? */
	for (i = index; i--; )
		if (table[i][1] > 3)
			return 3;

	/* Pseudo-anagram of name ? */
	for (alpha = 0, i = index; i--; )
		if (strchr(p->rlname, table[i][0]) != (char *)NULL &&
		    ++alpha > 4)
			return 4;

	/* Check for too many sequential characters... this probably does
	 * things I never intended but... ;) */

	/* Two characters are considered adjacent if their positions within
	 * the keyboard array differ by 0, 1, 9 or 10.
	 */
	for (alpha = 0, lq = 'q', i = index; i--; )
	{
		char *lqp, *tp;
		int t;

		if (abs(table[i][0] - lq) <= 1 ||
		    ((lqp = strchr(keyboard, lq)) != (char *)NULL &&
		    (tp = strchr(keyboard, table[i][0])) != (char *)NULL &&
		    ((t = abs(lqp - tp)) <= 1 || t == 9 || t == 10)))
		{
			if (++alpha > 2)
				return 5;
			lq = table[i][0];
		}
		else
			alpha = 0;
	}

#endif /* SECURE_PASSWORDS */

	/* Still check the length */
	if (strlen(passwd) < 4)
		return 1;
	return 0;
}

int
valid_email(char *e)
{
	int host;
	char *p;
	enum { EM_UNAME = 0, EM_BITNET, EM_MACHINE, EM_DOMAIN } state;

	host = 0;
	state = EM_UNAME;

	for (p = e; *p != '\0'; p++)
	{
		switch (*p)
		{
		    case '.':
			if (!host)
				return 0;
			if (state == EM_MACHINE)
				state = EM_DOMAIN;
			host = 0;
			break;
		    case '@':
			if (!host || state > EM_BITNET ||
			    !strncasecmp("ftp", e, host))
				return 0;
			host = 0;
			state = EM_MACHINE;
			break;
		    case '!':
		    case '%':
			if (!host || state > EM_BITNET)
				return 0;
			state = EM_BITNET;
			host = 0;
			break;
		    default:
			host++;
		}
	}
	if ((state == EM_BITNET || state >= EM_DOMAIN) && host > 1)
		return 1;
	return 0;
}

void
delete_users_rooms(char *name)
{
	char buf[MAXPATHLEN + 1];
	struct vector *v;
	struct room *r;
	int i, namel;

	namel = strlen(name);
	sprintf(buf, "room/%c", name[0]);

	if ((v = get_dir(buf, 0)) != (struct vector *)NULL)
	{
		for (i = v->size; i--; )
                        if (!strncmp(v->items[i].u.string, name, namel) &&
                            sscanf(v->items[i].u.string, "%[^.].o", buf) &&
                            ROOM_POINTER(r, buf) &&
                            !strcmp(r->owner, name))
			{
				destroy_room(r);
				sprintf(buf, "room/%c/%s", name[0],
				    v->items[i].u.string);
				unlink(buf);
			}
		free_vector(v);
	}
}

void
delete_user(char *name)
{
	char buf[MAXPATHLEN + 1];

	change_user_level(name, L_RESIDENT, ROOT_USER);
	sprintf(buf, "users/%c/%s.o", name[0], name);
	unlink(buf);
	delete_users_rooms(name);
	sprintf(buf, "room/%c/%s.o", name[0], name);
	unlink(buf);
	sprintf(buf, "mail/%s", name);
	unlink(buf);
	sprintf(buf, "board/%s", name);
	unlink(buf);

#ifdef AUTO_VALEMAIL
	remove_any_valemail(name);
#endif
}

void start_purge_event(void);

void
purge_users(struct event *ev)
{
	char a;
	int i;
	int total, purged;
	struct vector *v;
	char fname[MAXPATHLEN + 1];
	struct user *p;

	notify_level(L_VISITOR, "[ Users are being purged - please wait. ]");

	total = purged = 0;

	init_user(p = create_user());
	for (a = 'a'; a <= 'z'; a++)
	{
		sprintf(fname, "users/%c", a);
		if ((v = get_dir(fname, 0)) == (struct vector *)NULL)
			continue;
		for (i = v->size; i--; )
		{
			int level;

			if (!sscanf(v->items[i].u.string, "%[^.].o", fname))
				continue;
			if (!restore_user(p, fname))
				continue;
			total++;

			level = query_real_level(p->rlname);
			if (level == -1)
				level = L_RESIDENT;

			if (!p->last_login)	/* Never logged in */
			{
				sprintf(fname, "users/%c/%s", a,
				    v->items[i].u.string);

				p->last_login = file_time(fname);
				if (p->last_login == -1)
					p->last_login = 0;
			}

			if (!(p->saveflags & U_NO_PURGE) && p->last_login && (0
#ifdef PURGE_NEWBIE
			    || (level == L_RESIDENT && p->time_on <
			    NEWBIE_TIME_ON && current_time - p->last_login >
			    PURGE_NEWBIE)
#endif
#ifdef PURGE_RESIDENT
			    || (level == L_RESIDENT && current_time -
			    p->last_login > PURGE_RESIDENT)
#endif
#ifdef PURGE_CITIZEN
			    || (level == L_CITIZEN && current_time -
			    p->last_login > PURGE_CITIZEN)
#endif
#ifdef PURGE_WARDEN
			    || (level == L_WARDEN && current_time -
			    p->last_login > PURGE_WARDEN)
#endif
#ifdef PURGE_CONSUL
			    || (level == L_CONSUL && current_time -
			    p->last_login > PURGE_CONSUL)
#endif
#ifdef PURGE_OVERSEER
			    || (level == L_OVERSEER && current_time -
			    p->last_login > PURGE_OVERSEER)
#endif
			    ))
			{
				log_file("purge", "Removed user %s (%d) - %s",
				    capitalise(p->rlname), level,
				    conv_time(current_time - p->last_login));
				purged++;
				delete_user(p->rlname);
			}
			free_user(p);
			init_user(p);
		}
		free_vector(v);
	}
	tfree_user(p);
	if (ev != (struct event *)NULL)
		start_purge_event();
	notify_level(L_VISITOR, "[ Purge done. ]");
	notify_level(L_OVERSEER, "[ Purged %d/%d ; %d]", purged, total,
	    total - purged);
	if (purged)
		log_file("purge", "Purged %d of %d ; leaving %d.\n", purged,
		    total, total - purged);
}

void
start_purge_event()
{
#ifdef PURGE_HOUR
	struct stat st;

#ifdef F_NOPURGE
	if (stat(F_NOPURGE, &st) != -1)
		return;
#endif

	add_event(create_event(), purge_users,
	    (int)calc_time_until(PURGE_HOUR, 0), "purge");
#endif
}

#ifdef AUTO_VALEMAIL
/* Auto-valemail stuff */

static struct valemail *valemails = (struct valemail *)NULL;

static struct valemail *
create_valemail()
{
	struct valemail *v = (struct valemail *)xalloc(sizeof(struct valemail),
	    "valemail struct");
	v->user = v->id = v->email = (char *)NULL;
	v->next = (struct valemail *)NULL;
	v->date = 0;
	return v;
}

static void
free_valemail(struct valemail *v)
{
	FREE(v->user);
	FREE(v->id);
	FREE(v->email);
}

static void
store_valemail()
{
	FILE *fp;
	struct valemail *v;

	if (valemails == (struct valemail *)NULL)
	{
		unlink(F_VALEMAIL);
		return;
	}

	if ((fp = fopen(F_VALEMAIL, "w")) == (FILE *)NULL)
	{
		log_perror("store valemail");
		return;
	}
	fprintf(fp, "# user, email, id\n");
	for (v = valemails; v != (struct valemail *)NULL; v = v->next)
		fprintf(fp, "valemail ({\"%s\",\"%s\",\"%s\",%ld,})\n",
		    v->user, v->email, v->id, (long)v->date);
	fclose(fp);
}

void
restore_valemail()
{
	FILE *fp;
	struct stat st;
	char *buf;
	struct valemail *v, *w;

	for (v = valemails; v != (struct valemail *)NULL; v = w)
	{
		w = v->next;
		free_valemail(v);
	}
	valemails = (struct valemail *)NULL;

	if ((fp = fopen(F_VALEMAIL, "r")) == (FILE *)NULL)
		return;

	if (fstat(fileno(fp), &st) == -1)
	{
		log_perror("fstat");
		fatal("Cannot stat open file.");
	}

	if (!st.st_size)
	{
		unlink(F_VALEMAIL);
		fclose(fp);
		return;
	}

	buf = (char *)xalloc((size_t)st.st_size, "restore valemail");
	while (fgets(buf, (int)st.st_size - 1, fp) != (char *)NULL)
        {
		if (ISCOMMENT(buf))
			continue;
		if (!strncmp(buf, "valemail ", 9))
		{
			struct svalue *sv;

			if ((sv = decode_one(buf, "decode_one valemail")) ==
			    (struct svalue *)NULL)
				continue;
			if (sv->type == T_VECTOR && sv->u.vec->size == 4 &&
			    sv->u.vec->items[0].type == T_STRING &&
			    sv->u.vec->items[1].type == T_STRING &&
			    sv->u.vec->items[2].type == T_STRING &&
			    sv->u.vec->items[3].type == T_NUMBER)
			{
				v = create_valemail();
				v->user = string_copy(
				    sv->u.vec->items[0].u.string,
				    "valemail user");
				v->email = string_copy(
				    sv->u.vec->items[1].u.string,
				    "valemail email");
				v->id = string_copy(
				    sv->u.vec->items[2].u.string,
				    "valemail id");
				v->date = sv->u.vec->items[3].u.number;
				v->next = valemails;
				valemails = v;
			}
			else
				log_file("error", "Error in valemail file: "
				    "line '%s'", buf);
			free_svalue(sv);
			continue;
		}
	}
	fclose(fp);
	xfree(buf);
}

static void
remove_valemail(struct valemail *v)
{
	struct valemail **list;

	for (list = &valemails; *list != (struct valemail *)NULL;
	    list = &((*list)->next))
		if (*list == v)
		{
			*list = v->next;
			free_valemail(v);
			store_valemail();
			return;
		}
#ifdef DEBUG
	fatal("remove_valemail: valemail not found.");
#endif
}

static struct valemail *
find_valemail_by_user(char *name)
{
	struct valemail *v;

	for (v = valemails; v != (struct valemail *)NULL; v = v->next)
		if (!strcmp(v->user, name))
			break;
	return v;
}

static struct valemail *
find_valemail_by_id(char *id)
{
	struct valemail *v;

	for (v = valemails; v != (struct valemail *)NULL; v = v->next)
		if (!strcmp(v->id, id))
			break;
	return v;
}

int
remove_any_valemail(char *name)
{
	struct valemail *v;

	if ((v = find_valemail_by_user(name)) != (struct valemail *)NULL)
	{
		remove_valemail(v);
		return 1;
	}
	return 0;
}

void
register_valemail_id(struct user *p, char *id)
{
	struct valemail *v;

	if ((v = find_valemail_by_user(p->rlname)) == (struct valemail *)NULL)
	{
		v = create_valemail();
		v->user = string_copy(p->rlname, "valemail user");
		v->next = valemails;
		valemails = v;
	}
	v->date = current_time;
	COPY(v->email, p->email, "valemail email");
	COPY(v->id, id, "valemail id");
	store_valemail();
}

int
valemail_service(char *str, char *args)
{
	struct valemail *v;
	struct user *u;
	char *p;

	/* Str looks like: valemail <password> <id> <yes|no> */

	if (strcmp(str, "valemail"))
		return 0;

	str = args;

	if ((p = strchr(str, ' ')) == (char *)NULL)
		return 0;
	*p++ = '\0';

	/* Check valemail password */
	if (strcmp(str, AUTO_VALEMAIL))
	{
		log_file("illegal", "Bad valemail password on service port.");
		log_file("services", "Bad valemail password on service port.");
		notify_level(L_CONSUL,
		    "[ !ILLEGAL! Bad valemail password on service port. ]");
		return 0;
	}

	if (strlen(p) < 15)
	{
		log_file("valemail", "Bad id string on service port.");
		return 0;
	}

	str = p + 12;
	*str++ = '\0';

	/* Now, p points to our id string. */
	if ((v = find_valemail_by_id(p)) == (struct valemail *)NULL)
	{
		/* Not really an error.. */
		log_file("valemail", "Unknown valemail id on service port.");
		return 0;
	}

	if (strcmp(str, "yes"))
	{
		remove_valemail(v);
		return 1;
	}

	/* Grab the user */
	if ((u = doa_start((struct user *)NULL, v->user)) ==
	    (struct user *)NULL)
	{
		log_file("valemail",
		    "Valemail request received for %s %s %s; no such user.",
		    v->user, v->email, v->id);
		remove_valemail(v);
		return 0;
	}

	/* Check that their email address still matches this one */
	if (strcmp(u->email, v->email))
	{
		log_file("valemail",
		    "Valemail request %s %s %s rejected; "
		    "email address is now %s.", v->user, v->email, v->id,
		    u->email);
		remove_valemail(v);
		doa_end(u, 0);
		return 0;
	}

	if (u->saveflags & U_EMAIL_VALID)
	{
		log_file("valemail",
		    "Valemail request %s %s %s rejected; "
		    "address already validated.", v->user, v->email,
		    v->id);
		remove_valemail(v);
		doa_end(u, 0);
		return 0;
	}

	remove_valemail(v);

	u->saveflags |= U_EMAIL_VALID;

	notify_levelabu(u, L_CONSUL,
	    "[ Auto-validated the email address for %s. ]", u->capname);

	log_file("valemail", "%s (%s) validated by system", u->capname,
	    u->email);

	if (IN_GAME(u))
	{
		attr_colour(u, "notify");
		write_socket(u,
		    "Your email address has been auto-validated.");
		reset(u);
		write_socket(u, "\n");
	}

	doa_end(u, 1);
	return 1;
}

void
pending_valemails(struct user *p)
{
	struct strbuf str;
	struct valemail *v;
	int flag;

	init_strbuf(&str, 0, "pending valemails");

	add_strbuf(&str, "Pending valemail requests.\n");
	add_strbuf(&str, "--------------------------\n");

	for (flag = 0, v = valemails; v != (struct valemail *)NULL;
	    flag = 1, v = v->next)
		sadd_strbuf(&str, "%s: %-20s %s\n"
		    "                          %s\n",
		    nctime(user_time(p, v->date)),
		    v->user, v->id, v->email);

	if (!flag)
	{
		write_socket(p, "No pending requests.\n");
		free_strbuf(&str);
	}
	else
	{
		pop_strbuf(&str);
		more_start(p, str.str, NULL);
	}
}

#endif /* AUTO_VALEMAIL */

/*
 * Sudo related stuff...
 */

#define SUDO_RECURSE_DEPTH 10

static struct sudo_entry {
	char *name;
	struct vector *v;
	struct sudo_entry *next;
	} *sudos = (struct sudo_entry *)NULL;

static struct sudo_entry *
create_sudo()
{
	struct sudo_entry *s;

	s = (struct sudo_entry *)xalloc(sizeof(struct sudo_entry), "sudo ent");
	s->name = (char *)NULL;
	s->v = (struct vector *)NULL;
	s->next = (struct sudo_entry *)NULL;
	return s;
}

static void
free_sudo(struct sudo_entry *s)
{
	FREE(s->name);
	free_vector(s->v);
	xfree(s);
}

static struct sudo_entry *
find_sudo(char *name)
{
	struct sudo_entry *s;

	for (s = sudos; s != (struct sudo_entry *)NULL; s = s->next)
		if (!strcmp(name, s->name))
			break;
	return s;
}

static void
load_sudos()
{
	struct sudo_entry *s, *t;
	static time_t lst = 0;
	struct vecbuf vec;
	struct stat st;
	char *buf, *p;
	int line, ent;
	time_t tm;
	FILE *fp;

	/* Don't need to reload if nothing has changed.
	 * Note: if the file is non-existant, then file_time returns -1 and
	 *       this works.
	 */
	if ((tm = file_time(F_SUDO)) <= lst)
		return;

	lst = tm;

	/* Clean out old entries.. */
	for (s = sudos; s != (struct sudo_entry *)NULL; s = t)
	{
		t = s->next;
		free_sudo(s);
	}
	sudos = (struct sudo_entry *)NULL;

	if ((fp = fopen(F_SUDO, "r")) == (FILE *)NULL)
		return;

	if (fstat(fileno(fp), &st) == -1)
	{
		log_perror("fstat");
		fatal("Cannot stat open file.");
	}

	if (!st.st_size)
	{
		fclose(fp);
		unlink(F_SUDO);
		return;
	}

	buf = (char *)xalloc((size_t)st.st_size, "load_sudos");
	line = ent = 0;

	while (fgets(buf, (int)st.st_size, fp) != (char *)NULL)
	{
		line++;

		if (ISCOMMENT(buf))
			continue;

		if ((p = strchr(buf, '\n')) != (char *)NULL)
			*p = '\0';

		if ((p = strtok(buf, ":")) == (char *)NULL)
		{
			log_file("error", "Error in sudoers at line %d, "
			    "no : separator.", line);
			continue;
		}

		if (find_sudo(buf) != (struct sudo_entry *)NULL)
		{
			log_file("error", "Duplicate sudo entry at line %d.",
			    line);
			continue;
		}

		s = create_sudo();
		COPY(s->name, buf, "sudo name");

		init_vecbuf(&vec, 0, "sudo vector");

		while ((p = strtok((char *)NULL, ":,")) != (char *)NULL)
		{
			int i = vecbuf_index(&vec);
			vec.vec->items[i].type = T_STRING;
			vec.vec->items[i].u.string = string_copy(p,
			    "sudo vec el");
		}
		s->v = vecbuf_vector(&vec);
		s->next = sudos;
		sudos = s;
		ent++;
	}
	xfree(buf);
	fclose(fp);
	log_file("syslog", "Sudo: loaded %d entr%s.", ent,
	    ent == 1 ? "y" : "ies");
}

void
list_sudos(struct user *p, char *name)
{
	static int indent = 0, rec = 0;
	struct sudo_entry *s;
	int i, j;

	if (!rec)
		load_sudos();
	else if (rec > SUDO_RECURSE_DEPTH)
	{
		log_file("error", "Sudo: recursion too deep for %s.", name);
		return;
	}

	if ((s = find_sudo(name)) == (struct sudo_entry *)NULL)
	{
		if (!rec)
			write_socket(p, "Permission denied.\n");
		return;
	}

	if (!rec)
		write_socket(p, "Sudo permissions for %s.\n", p->capname);

	indent += 8, rec++;

	for (i = 0; i < s->v->size; i++)
	{
		for (j = indent; j--; )
			write_socket(p, " ");
		write_socket(p, "%s\n", s->v->items[i].u.string);
		if (*s->v->items[i].u.string == '[')
		{
			list_sudos(p, s->v->items[i].u.string);
			continue;
		}
	}
	indent -= 8, rec--;
}

int
check_sudo(char *user, char *cmd, char *args)
{
	struct sudo_entry *s;
	int ok = 0;
	static int rec = 0;
	char *p;
	int i;

	if (!rec)
		load_sudos();
	else if (rec > SUDO_RECURSE_DEPTH)
	{
		log_file("error", "Sudo: recursion too deep for %s.", user);
		return 0;
	}

	if ((s = find_sudo(user)) == (struct sudo_entry *)NULL)
		return 0;

	rec++;

	for (i = 0; i < s->v->size; i++)
	{
		char *q, *o;
		int n;

		/* Check for recursion.. */
		if (*s->v->items[i].u.string == '[')
		{
			if (check_sudo(s->v->items[i].u.string, cmd, args) == 1)
				ok = 1;
			continue;
		}

		o = q = string_copy(s->v->items[i].u.string, "check_sudo tmp");

		/* Check for allowed arguments */
		if ((p = strchr(q, '(')) != (char *)NULL)
			*p++ = '\0';

		if (*q == '!')
			n = 0, q++;
		else
			n = 1;

		/* Check the command.. */
		if (!strcmp(q, cmd) || !strcmp(q, "*"))
		{
			/* This is the required command..
			 * any arguments to check ? */
			if (p != (char *)NULL)
			{
				if (args != (char *)NULL)
				{
					q = strtok(p, "|)");
					do
					{
						if (!strcmp(q, args))
						{
							ok = n;
							break;
						}
					} while ((q = strtok((char *)NULL,
					    "|)")) != (char *)NULL);
				}
			}
			else
				ok = n;
		}

		xfree(o);
	}
	rec--;
	return ok;
}

