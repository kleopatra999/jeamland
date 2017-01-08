/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	jlm.c
 * Function:	JeamLand Loadable Module system
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include "jeamland.h"

extern void erqd_died(void);

extern char *bin_path;

struct jlm *jlms = (struct jlm *)NULL;

#ifdef HASH_JLM_FUNCS
struct hash *jhash = (struct hash *)NULL;
#endif

struct jlm *
create_jlm()
{
	struct jlm *j = (struct jlm *)xalloc(sizeof(struct jlm),
	    "jlm struct");

	j->infd = j->outfd = j->pid = 0;
	j->id = j->ident = (char *)NULL;
	j->state = JL_S_NONE;
	init_stack(&j->stack);
	init_strbuf(&j->text, 1, "jlm strbuf");
	j->ob = (struct object *)NULL;

	/* Add to global list. */
	j->next = jlms;
	jlms = j;
	return j;
}

void
free_jlm(struct jlm *j)
{
	struct jlm **p;

	/* Remove from global list */
	for (p = &jlms; *p != (struct jlm *)NULL; p = &((*p)->next))
		if (*p == j)
		{
			*p = j->next;
			FREE(j->id);
			FREE(j->ident);
			free_strbuf(&j->text);
			clean_stack(&j->stack);
			xfree(j);
			return;
		}

#ifdef DEBUG
	fatal("free_jlm: jlm not found.");
#endif
}

struct jlm *
find_jlm(char *id)
{
	struct jlm *j;

	for (j = jlms; j != (struct jlm *)NULL; j = j->next)
		if (!strcmp(j->id, id))
			return j;
	return (struct jlm *)NULL;
}

void
dead_child(int sig)
{
	struct jlm *j;
	int status, pid;

	if ((pid = wait(&status)) == -1)
	{
		log_perror("wait jlm");
		return;
	}

	/* Find out which module it was.. */
	for (j = jlms; j != (struct jlm *)NULL; j = j->next)
	{
		if (j->pid == pid)
			break;
	}
	if (j == (struct jlm *)NULL)
		fatal("dead_child: jlm never started!");

	if (WIFEXITED(status))
	{
		/* Not the best way of checking if it is missing or
		 * not, but it will work most of the time! */
		if (WEXITSTATUS(status) == 24)
		{
			log_file("syslog", "JLM %s missing.", j->id);
			notify_level(L_CONSUL, "[ JLM %s missing. ]\n", j->id);
		}
		else
		{
			log_file("syslog", "JLM %s (%d) exited %d.", j->id,
			    pid, WEXITSTATUS(status));
			notify_level(L_CONSUL, "[ JLM %s (%d) exited %d. ]\n",
			    j->id, pid, WEXITSTATUS(status));
		}
	}
	else
	{
		log_file("syslog", "JLM %s (%d) died from signal %d.", j->id,
		    pid, WTERMSIG(status));
		notify_level(L_CONSUL, "[ JLM %s (%d) died from signal %d. ]\n",
		    j->id, pid, WTERMSIG(status));
	}

	/* Special case */
	if (!strcmp(j->id, "erqd"))
		erqd_died();

	if (close(j->infd) == -1)
		log_perror("close jlm infd (%s)", j->id);
	if (close(j->outfd) == -1)
		log_perror("close jlm outfd (%s)", j->id);

	if (j->ob != (struct object *)NULL)
		free_object(j->ob);
	else
		free_jlm(j);

	/* This has to be after the wait() call and all actions based upon the
	 * pid returned by wait because Solaris and AIX would
	 * automatically call dead_child() again here if there were any
	 * processes left wait for.
	 */
	signal(sig, dead_child);
}

struct jlm *
jlm_pipe(char *id)
{
	char path[MAXPATHLEN + 1];
	char buf[BUFFER_SIZE];
	int tochild[2], fromchild[2];
	struct jlm *j;
	int pid;

	signal(SIGCHLD, dead_child);

    	if(pipe(tochild) < 0)
	{
		log_perror("pipe out");
        	fatal("Could not create pipe");
	}
    	if(pipe(fromchild) < 0)
    	{
		log_perror("pipe in");
        	fatal("Could not create pipe");
    	}
	switch(pid = fork())
	{
	    case 0:
		/* Child */
        	dup2(tochild[0], 0);
        	dup2(fromchild[1], 1);
		close(tochild[0]), close(tochild[1]);
		close(fromchild[0]), close(fromchild[1]);
		sprintf(path, "%s/jlm/%s.jlm", bin_path, id);
		sprintf(buf, "-=> JLM: %s <=-", id);
		/* Not pretty, but we want the parent to correctly form the
		 * object pointer before continuing in case the binary is non
		 * existant, or the child dies quickly */
		sleep(2);
	    	if (execl((char *)path, buf, (char *)NULL)
		    == -1)
			_exit(24); /* beware of flushing buffers... */
		/* NOTREACHED .. */
		else
			exit(0);
	   	break;
	    case -1:
		log_perror("fork");
		fatal("Could not fork.");
	    default:
		log_file("syslog", "Started JLM %s (%d).", id, pid);
		notify_level(L_CONSUL, "[ JLM %s started (%d) ].\n", id, pid);

    		close(tochild[0]);
    		close(fromchild[1]);

		j = create_jlm();
		j->pid = pid;
		j->infd = fromchild[0];
		j->outfd = tochild[1];
		j->id = string_copy(id, "jlm id");
		return j;
    	}
	/* NOTREACHED */
	return (struct jlm *)NULL;
}

void
attach_jlm(struct room *r, char *id)
{
	struct object *ob;
	struct jlm *j;

	if ((j = jlm_pipe(id)) == (struct jlm *)NULL)
		return;

	ob = create_object();
	ob->type = T_JLM;
	ob->m.jlm = j;
	j->ob = ob;

	add_to_env(ob, r->ob);
	log_file("syslog", "Attached JLM %s to room %s.", id, r->fname);
}

void
preload_jlms()
{
	FILE *fp;
	char buf[BUFFER_SIZE];
	char *p;
	struct room *r;
	struct object *ob;
	int flag;
	int line = 0;

	if ((fp = fopen(F_JLM, "r")) == (FILE *)NULL)
		return;

	while (fgets(buf, sizeof(buf), fp) != (char *)NULL)
	{
		line++;

		if (ISCOMMENT(buf))
			continue;

		deltrail(buf);

		if ((p = strchr(buf, ':')) == (char *)NULL)
		{
			log_file("error",
			    "Error in JLM config file, line %d.", line);
			continue;
		}
		*p++ = '\0';

		if (!ROOM_POINTER(r, p))
		{
			log_file("syslog",
			    "JLM: Room %s does not exist, skipping.", p);
			continue;
		}
		/* Check that this jlm is not already in this room. */
		for (flag = 0, ob = r->ob->contains;
		    ob != (struct object *)NULL;
		    ob = ob->next_contains)
			if (ob->type == T_JLM &&
			    !strcmp(ob->m.jlm->id, buf))
			{
				flag = 1;
				break;
			}

		if (!flag)
			attach_jlm(r, buf);
		else
			log_file("syslog",
			    "JLM: %s already attached to %s, skipping.",
			    buf, p);
	}
	fclose(fp);
}

static void
j_ident(struct jlm *j)
{
	COPY(j->ident, j->stack.sp->u.string, "jlm ident");
	notify_level(L_CONSUL, "[ JLM %s identified itself as: %s ]\n",
	    j->id, j->ident);
	pop_stack(&j->stack);
}

static void
j_claim(struct jlm *j)
{
	struct sent *s;

	s = create_sent();

	notify_level(L_CONSUL, "[ JLM %s claimed command %s. ]\n",
	    j->id, j->stack.sp->u.string);

	s->ob = j->ob;
	s->cmd = string_copy(j->stack.sp->u.string, "sent cmd");
	add_sent_to_export_list(j->ob, s);
	add_sent_to_inv_list(j->ob->super, s);

	pop_stack(&j->stack);
}

static void
j_write_user(struct jlm *j)
{
	struct user *p;

	if ((p = find_user((struct user *)NULL, j->stack.sp->u.string)) !=
	    (struct user *)NULL)
		write_socket(p, "%s", j->text.str);
	pop_stack(&j->stack);
}

static void
j_write_level(struct jlm *j)
{
	int i;

	if ((i = level_number(j->stack.sp->u.string)) != -1)
		write_level(i, "%s", j->text.str);
	pop_stack(&j->stack);
}

static void
j_notify_level(struct jlm *j)
{
	int i;

	if ((i = level_number(j->stack.sp->u.string)) != -1)
		notify_level(i, "%s", j->text.str);
	pop_stack(&j->stack);
}

static void
j_chattr(struct jlm *j)
{
	struct user *p;

	if ((p = find_user((struct user *)NULL, (j->stack.sp - 1)->u.string))
	    != (struct user *)NULL)
	{
		if (!strcmp(j->stack.sp->u.string, "bold"))
			bold(p);
		else if (!strcmp(j->stack.sp->u.string, "yellow"))
			yellow(p);
		else if (!strcmp(j->stack.sp->u.string, "red"))
			red(p);
		else if (!strcmp(j->stack.sp->u.string, "reset"))
			reset(p);
		else
			log_file("error", "JLM_chattr: %s, Unknown attribute"
			    " %s.", j->id, j->stack.sp->u.string);
	}
	pop_n_elems(&j->stack, 2);
}

static void
j_force(struct jlm *j)
{
	struct user *p;

	if ((find_user((struct user *)NULL, (j->stack.sp - 1)->u.string)) !=
	    (struct user *)NULL)
		insert_command(p, j->stack.sp->u.string);
	pop_n_elems(&j->stack, 2);
}

#define JFUNC_TEXT	0x1
/* FUNCS */
static struct jfunc {
	char *name;
	int args;
	int flags;
	void (*func)(struct jlm *);
	} jfuncs[] = {

	{ "ident",		1,	0,		j_ident },
	{ "claim",		1,	0,		j_claim },

	{ "write_user",		1,	JFUNC_TEXT,	j_write_user },
	{ "write_level",	1,	JFUNC_TEXT,	j_write_level },
	{ "notify_level",	1,	JFUNC_TEXT,	j_notify_level },

	{ "chattr",		2,	0,		j_chattr },

	{ "force",		2,	0,		j_force },

	{ (char *)NULL,		0,	0,		NULL } };


#ifdef HASH_JLM_FUNCS
void
jlm_hash_stats(struct user *p, int verbose)
{
	hash_stats(p, jhash, verbose);
}

void
hash_jlm_funcs()
{
	int i;

	if (jhash != (struct hash *)NULL)
		free_hash(jhash);

	jhash = create_hash(0, "jlm_funcs");

	for (i = 0; jfuncs[i].name != (char *)NULL; i++)
		insert_hash(&jhash, (void *)&jfuncs[i]);
}
#endif

static void
call_jlm_func(struct jlm *j)
{
	struct jfunc *jf;
	char *fname;
	int i;

	/* The function name must be first on the stack. */
	if (STACK_EMPTY(&j->stack))
	{
		log_file("error", "Jlm_Func: %s, stack empty.", j->id);
		return;
	}

	fname = stack_svalue(&j->stack, 1)->u.string;

#ifdef HASH_JLM_FUNCS
	/* *sigh* The first element on the stack is at location 1.. I should
	 * fix this some time. */
	if ((jf = (struct jfunc *)lookup_hash(jhash, fname))
	    == (struct jfunc *)NULL)
		i = -1;
#else
	for (i = 0; jfuncs[i].name != (char *)NULL; i++)
		if (!strcmp(jfuncs[i].name, fname))
			break;
	if (jfuncs[i].name == (char *)NULL)
		i = -1;
	else
		jf = &jfuncs[i];
#endif
	if (i == -1)
		log_file("error", "Jlm_Func: %s, unknown function %s().",
		    j->id, fname);
	else if (j->stack.el != jf->args + 1)
		log_file("error", "Jlm_Func: %s, wrong number of args to "
		    "%s() [ %d vs. %d ].", j->id, jf->name, j->stack.el - 1,
		    jf->args);
	/* Do we need text, and if so, have we got it ? */
	else
	{
		if ((jf->flags & JFUNC_TEXT) && !j->text.offset)
			log_file("error", "Jlm_Func: %s, no text in %s().",
			    j->id, jf->name);
		else if (!(jf->flags & JFUNC_TEXT) && j->text.offset)
			log_file("error", "Jlm_Func: %s, text when non "
			    "required in %s().", j->id, jf->name);
		else
			jf->func(j);
	}
	pop_stack(&j->stack);
}

/* Lots of checks, but we can't allow a module to crash the main server!
 * There is no guarantee that a module is transmitting valid data
 * This is a finite state machine.
 * There are different packets which can be received; these are:
 *	SOS:		Start of service.
 *	EOS:		End of service.
 *	SPA:		Service parameter packet.
 *	TXT:		Plain text.
 */
static void
jlm_parse_line(struct jlm *j, char *buf)
{
	FUN_START("jlm_parse_line");
	FUN_ARG(buf);

	log_file("debug", "DEBUG, %s: got [%s].", j->id, buf);

	/* End of service..
	 * Do something depending on which service we were in.
	 */
	if (!strcmp(buf, "#!"))
	{	/* EOS Packet */
		switch (j->state)
		{
		    case JL_S_NONE:	/* Eh ? */
			log_file("error", "jlm: %s, EOS packet in "
			    "idle state.", j->id);
			break;

		    case JL_S_FUNC:
			/* Function call */
			call_jlm_func(j);
			break;
			
		    default:
			fatal("Uknown state in jlm_parse_line.");
		}
		j->state = JL_S_NONE;
		FUN_END;
		return;
	}

	FUN_LINE;

	/* A service parameter.
	 * Just push it onto the stack.
	 */
	if (!strncmp(buf, "#!(", 3))
	{
		/* SPA Packet */
		char *p;

		if (j->state == JL_S_NONE)
		{
			log_file("error", "Jlm_SPA: %s, in idle state.",
			    j->id);
			FUN_END;
			return;
		}

		if ((p = strrchr(buf, ')')) == (char *)NULL)
		{
			log_file("error", "Jlm_SPA: %s, no terminating ).",
			    j->id);
			FUN_END;
			return;
		}
		*p = '\0';
		buf += 3;

		/* I'll check this for now.. */
		if (strchr(buf, ':') != (char *)NULL)
		{
			log_file("error", "Jlm_SPA: %s, old style packet "
			    "received - recompile jlm!", j->id);
			FUN_END;
			return;
		}

		if (STACK_FULL(&j->stack))
		{
			/* Unusual step of checking for this.. otherwise
			 * a module could quite happily cause an overflow
			 * which is considered a fatal error..
			 * Of course, the module would have to be written
			 * to deliberately do this, but still..
			 */
			log_file("error", "Jlm_SPA: %s, Stack overflow.",
			    j->id);
			clean_stack(&j->stack);
			FUN_END;
			return;
		}
		push_string(&j->stack, buf);
		FUN_END;
		return;
	}

	/* Start a service..
	 * Need to work out which service it is looking for..
	 * Valid services are currently only:
	 *	func		Call a function.
	 */
	if (!strncmp(buf, "#!", 2))
	{
		/* SOS Packet */

		/* We must be idle.. */
		if (j->state != JL_S_NONE)
		{
			/* Panic ;-) */
			log_file("error", "Jlm: %s, SOS Packet while "
			    "already in service.", j->id);
			FUN_END;
			return;
		}
		reinit_strbuf(&j->text);

		/* Take the opportunity to do some housekeeping.
		 * Shrink the strbuf if it is too large..
		 * shouldn't happen too often so log it */
		if (j->text.len > JLM_MAX_TEXT)
		{
			log_file("error",
			    "Jlm_SOS: %s, shrinking strbuf (%d).",
			    j->id, j->text.len);
			shrink_strbuf(&j->text, 1);
		}

		/* Check that the stack is clean.. */
		if (j->stack.el)
		{
			log_file("error",
			    "Jlm_SOS: %s, stack contains %d elements, "
			    "cleaning.", j->id, j->stack.el);
			clean_stack(&j->stack);
		}
			
		buf += 2;
		/* Not worth anything fancy. */
		if (!strcmp(buf, "func"))
			j->state = JL_S_FUNC;
		else
			log_file("error", "Jlm: %s, "
			    "Unknown service request: %s.", j->id, buf);
		FUN_END;
		return;
	}

	/* TXT packet */

	/* Was an escaped leading \ or # character transmitted? */
	if (strlen(buf) > 2 && buf[0] == '\\' &&
	    (buf[1] == '#' || buf[1] == '\\'))
		buf++;

	/* Generic text parameter */
	if (j->state == JL_S_NONE)
		/* Default behaviour, write it to the environment. */
		tell_object_njlm(j->ob->super, "%s\n", buf);
	else
	{
		/* Add to the text buffer.
		 * Check it isn't getting too big!
		 * The module could have forgotten to send a #!
		 * sequence.
		 */
		if (j->text.offset > JLM_MAX_OFFSET)
		{
			/* Should not happen ;-) */
			log_file("error", "Jlm_TXT: %s, "
			    "Max text offset exceeded.", j->id);
			reinit_strbuf(&j->text);
			j->state = JL_S_NONE;
			FUN_END;
			return;
		}
		add_strbuf(&j->text, buf);
		cadd_strbuf(&j->text, '\n');
	}
	FUN_END;
}

void
jlm_start_service(struct jlm *j, char *s)
{
	write_a_jlm(j, 1, "#!%s\n", s);
}

void
jlm_end_service(struct jlm *j)
{
	write_a_jlm(j, 1, "#!\n");
}

void
jlm_service_param(struct jlm *j, char *param, char *value)
{
	write_a_jlm(j, 1, "#!(%s:%s)\n", param, value);
}

void
jlm_reply(struct jlm *j)
{
	char buf[BUFFER_SIZE];
	char *d, *buff = buf;
	int lenc, lend;
	int cnt;

	FUN_START("jlm_reply");
	FUN_ARG(j->id);

	cnt = read(j->infd, buf, sizeof(buf) - 1);
	if (!cnt)
	{
		/* It died.. */
		log_file("syslog", "JLM %s (%d) output flushed.", j->id,
		    j->pid);
		notify_level(L_CONSUL, "[ JLM %s (%d) output flushed. ]\n",
		    j->id, j->pid);
		erqd_died();
		kill(SIGKILL, j->pid);
		/*dead_child(SIGCHLD);*/
		FUN_END;
		return;
	}
	buf[cnt] = '\0';

	/* Special case - leading newlines */
	while (*buff == '\n')
	{
		jlm_parse_line(j, "");
		buff++;
	}

	/* Break it into lines, and parse them. */
	lenc = strlen(buff);
	d = strtok(buff, "\n");
	if (d == (char *)NULL)
	{
		FUN_END;
		return;	/* Should never happen - trapped for above */
	}
	do
	{
		if (!strlen(d))
			jlm_parse_line(j, "");
		else
		{
			/* jlm_parse_line can stick a \0 character in the
			 * middle of d, so shortening it - get the length
			 * before the call */
			lend = strlen(d) + 1;

			jlm_parse_line(j, d);

			/* Handle consecutive blank lines */
			while (d + lend < buff + lenc && d[lend] == '\n')
			{
				jlm_parse_line(j, "");
				lend++;
			}
		}
	} while ((d = strtok((char *)NULL, "\n")) != (char *)NULL);
	FUN_END;
}

static void
really_kill_jlm(struct event *ev)
{
	if (kill(ev->stack.sp->u.number, SIGTERM) != -1)
	{
		log_file("syslog", "Forcibly terminated JLM %s.",
		    (ev->stack.sp - 1)->u.string);
	}
	/* Just in case ;-) */
	kill(ev->stack.sp->u.number, SIGKILL);
}

void
kill_jlm(struct jlm *j)
{
	struct event *ev;
	int pid = j->pid;
	char *str = string_copy(j->id, "kill_jlm id");

	/* Well behaved modules will respond to this. */
	jlm_start_service(j, "die");

	/* To be on the safe side, it will be killed in 5 seconds 
	 * NB: At this point, don't rely on the jlm struct still being
	 *     safe to access, (which is why pid and str are copied above).
	 */
	ev = create_event();
	push_malloced_string(&ev->stack, str);
	push_number(&ev->stack, pid);
	add_event(ev, really_kill_jlm, 5, "kill jlm");
}

void
closedown_jlms()
{
	struct jlm *j;

	for (j = jlms; j != (struct jlm *)NULL; j = j->next)
	{
		log_file("syslog", "Shutting down JLM %s.", j->id);
		kill_jlm(j);
	}
}

