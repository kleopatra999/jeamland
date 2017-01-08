/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	xalloc.c
 * Function:	Memory management wrapper.
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <ctype.h>
#include <malloc.h>
#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#endif
#include <netinet/in.h>
#include "jeamland.h"

#ifdef RUSAGE
#ifdef sunc
extern long _sysconf(int);
#endif /* sunc */
#if defined(SOLARIS) || defined(HPUX) || defined(sunc)
#include <sys/times.h>
#include <limits.h>
#else
#include <sys/resource.h>
#ifndef RUSAGE_SELF
#define RUSAGE_SELF	0
#endif
#endif /* SOLARIS || HPUX || sunc */
#endif /* RUSAGE */

extern time_t current_time;
extern float utime_av[], stime_av[];

#ifdef DEBUG_MALLOC
#define MALLSIZE 10000
struct mallelem {
	void *ptr;
	size_t size;
	char desc[50];
	} malltable[MALLSIZE];

#ifdef DEBUG_MALLOC_FUDGE
/* Allocate 64 extra bytes; 32 before and 32 after the memory segment. */
#define MALLOC_FUDGE_FACTOR	64
#define HMALLOC_FUDGE_FACTOR	32
/* 0xcc happens to be the instruction code for 'INT 3' in 80386 assembler. If
 * this value is 'executed', the program should stop, helping to identify
 * rogue memory accesses
 * For other platforms, you may want to choose another value
 */
#define MALLOC_FUDGE_VALUE	0xcc
#endif /* DEBUG_MALLOC_FUDGE */

#endif /* DEBUG_MALLOC */

#ifdef __linux__
	struct jl_mallinfo {
	    int size;
	    int resident;
	    int shared;
	    int trs;
	    int lrs;
	    int drs;
	    int dt;
		};

struct jl_mallinfo *
jl_mallinfo()
{
	static struct jl_mallinfo mem;
	char fname[MAXPATHLEN + 1];
	char ret[BUFFER_SIZE];
	int fd, num_read;

	sprintf(fname, "/proc/%u/statm", getpid());
	if ((fd = open(fname, O_RDONLY, 0)) == -1)
	{
		log_perror("open proc");
		mem.size = -1;
		return &mem;
	}
	if ((num_read = read(fd, ret, sizeof(ret) - 1)) == -1)
	{
		log_perror("proc read");
		mem.size = -1;
		return &mem;
	}
	ret[num_read] = '\0';
	close(fd);
	sscanf(ret, "%d %d %d %d %d %d %d", &mem.size, &mem.resident,
	    &mem.shared, &mem.trs, &mem.lrs, &mem.drs, &mem.dt);
	return &mem;
}
#endif

#ifdef MALLOC_STATS
static int tmalloc, trealloc;
static int nmalloc, nrealloc, nfree;
#ifdef DEBUG_MALLOC
static int tfree;
#endif /* DEBUG_MALLOC */
#endif /* MALLOC_STATS */

void
malloc_init()
{
#ifdef DEBUG_MALLOC
	int i;
	for (i = 0; i < MALLSIZE; i++)
		malltable[i].ptr = (void *)NULL;
#endif

#ifdef MALLOC_STATS
	tmalloc = trealloc = 0;
	nmalloc = nrealloc = nfree = 0;
#ifdef DEBUG_MALLOC
	tfree = 0;
#endif /* DEBUG_MALLOC */
#endif /* MALLOC_STATS */

#ifndef __linux__
    	mallopt(M_MXFAST, 50);
    	mallopt(M_NLBLKS, 100);
    	mallopt(M_GRAIN, 8);
#endif
}

void
dump_meminfo(struct user *p)
{
#if !defined(__linux__) || defined(DEBUG_MALLOC)
	int usage, tusage;
#endif

#ifdef __linux__
	size_t pagesize = getpagesize();
	struct jl_mallinfo *mem = jl_mallinfo();

	/* As of libc 5.4.7 (maybe earlier) linux supports the mallinfo()
	 * call.. However, for backwards compatibility (and as it is still
	 * largely unimplemented, I continue to use this code which reads
	 * memory use from /proc */

	write_socket(p, "Total pages:       %10d        %10d\n",
	    mem->size, mem->size * pagesize);
	write_socket(p, "Resident pages:    %10d        %10d\n",
	    mem->resident, mem->resident * pagesize);
	write_socket(p, "Shared pages:      %10d        %10d\n",
	    mem->shared, mem->shared * pagesize);
	write_socket(p, "Text pages:        %10d        %10d\n",
	    mem->trs, mem->trs * pagesize);
	write_socket(p, "Library pages:     %10d        %10d\n",
	    mem->lrs, mem->lrs * pagesize);
	write_socket(p, "Data pages:        %10d        %10d\n",
	    mem->drs, mem->drs * pagesize);
	write_socket(p, "Dirty pages:       %10d        %10d\n\n",
	    mem->dt, mem->dt * pagesize);

#else /* __linux__ */

	struct mallinfo mem = mallinfo();

    	write_socket(p, "sbrk requests:     %10d        %10d\n",
	    mem.ordblks, mem.arena);
    	write_socket(p, "large blocks:      %10d        %10d\n",
	    0, mem.uordblks);
    	write_socket(p, "large free blocks: %10d        %10d\n",
	    0, mem.fordblks);
    	write_socket(p, "small blocks:      %10d        %10d\n",
	    mem.smblks, mem.usmblks);
    	write_socket(p, "small free blocks: %10d        %10d\n",
	    0, mem.fsmblks);
	tusage = mem.arena;
	usage = mem.uordblks + mem.usmblks;

	write_socket(p, "Total usage:       %10d /      %10d\n\n",
	    usage, tusage);
#endif /* !linux */
#ifdef MALLOC_STATS
	write_socket(p, "Total malloced:    %10d        %10d\n", nmalloc,
	    tmalloc);
	write_socket(p, "Total realloced:   %10d        %10d\n", nrealloc,
	    trealloc);
#ifdef DEBUG_MALLOC
	write_socket(p, "Total freed:       %10d        %10d\n", nfree,
	    tfree);
	write_socket(p, "Free/malloc ratio: %10.2f%%\n",
	    tmalloc ? (float)(tfree * 100) / (float)tmalloc : 0);
#else
	write_socket(p, "Total freed:       %10d\n", nfree);
	write_socket(p, "Free/malloc ratio: %10.2f%%\n",
	    nmalloc ? (float)(nfree * 100) / (float)nmalloc : 0);
#endif /* DEBUG_MALLOC */
#endif /* MALLOC_STATS */
#ifdef DEBUG_MALLOC
	for (tusage = usage = 0; usage < MALLSIZE; usage++)
	{
		if (malltable[usage].ptr == (void *)NULL)
			continue;
		tusage += malltable[usage].size;
	}
	write_socket(p, "\nDebug malloc:      %10d\n", tusage);
#endif /* DEBUG_MALLOC */
}

#ifdef DEBUG_MALLOC
int
xalloced(void *r)
{
	int i;

	for (i = 0; i < MALLSIZE; i++)
		if (malltable[i].ptr == r)
			return i;
	return -1;
}
#endif /* DEBUG_MALLOC */

void *
xalloc(size_t size, char *desc)
{
	void *r;
#ifdef DEBUG_MALLOC
	int i;
#endif

	if (!size)
#ifdef DEBUG
		fatal("xalloc(0)");
#else
		size++;
#endif
#if defined(DEBUG_MALLOC) && defined(DEBUG_MALLOC_FUDGE)
	r = malloc(size + MALLOC_FUDGE_FACTOR);
#else
	r = malloc(size);
#endif
	if (r == (void *)NULL)
		fatal("Out of memory.");
#ifdef DEBUG_MALLOC

#ifdef DEBUG_MALLOC_FUDGE
	/* Fill the entire memory segment with the value. */
	memset(r, MALLOC_FUDGE_VALUE, size + MALLOC_FUDGE_FACTOR);

	/* Move our pointer up a bit to leave a fudged buffer
	 * This is the part that is platform specific.. */
	r = (void *)((unsigned char *)r + HMALLOC_FUDGE_FACTOR);
#endif

	if ((i = xalloced((void *)NULL)) == -1)
		fatal("Malloc table overflow");

	strcpy(malltable[i].desc, desc);
	malltable[i].ptr = r;
	malltable[i].size = size;

#endif /* DEBUG_MALLOC */
#ifdef MALLOC_STATS
	tmalloc += size;
	nmalloc++;
#endif
	return r;
}

void
xfree(void *r)
{
#ifdef DEBUG_MALLOC
	int i;
#ifdef DEBUG_MALLOC_FUDGE
	int j, flag;
	unsigned char *p;
#endif
#endif /* DEBUG_MALLOC */
	if (r == (void *)NULL)
		fatal("Xfree on NULL pointer");

#ifdef DEBUG_MALLOC
	if ((i = xalloced(r)) == -1)
		fatal("Freeing unallocated memory.");
	malltable[i].ptr = (void *)NULL;

#ifdef DEBUG_MALLOC_FUDGE
	flag = 0;

	r = (void *)((unsigned char *)r - HMALLOC_FUDGE_FACTOR);

	/* Need to check if we still have two intact buffers */
	for (p = (unsigned char *)r, j = 0; j < HMALLOC_FUDGE_FACTOR; p++, j++)
		if (*p != MALLOC_FUDGE_VALUE)
		{
			log_file("error",
			    "Memory segment front buffer corrupt: "
			    "[%s] %d(%p) %x'%c'", malltable[i].desc, j, p, *p,
			    isprint(*p) ? *p : '?');
			flag = 1;
		}

	for (p = (unsigned char *)r + malltable[i].size +
	    HMALLOC_FUDGE_FACTOR, j = 0; j < HMALLOC_FUDGE_FACTOR; p++, j++)
		if (*p != MALLOC_FUDGE_VALUE)
		{
			log_file("error",
			    "Memory segment back buffer corrupt: "
			    "[%s] %d(%p) %x'%c'", malltable[i].desc, j, p, *p,
			    isprint(*p) ? *p : '?');
			flag = 1;
		}

	if (flag)
		fatal("Memory corrupt.");
#endif /* DEBUG_MALLOC_FUDGE */

#ifdef MALLOC_STATS
	tfree += malltable[i].size;
#endif /* MALLOC_STATS */
#endif /* DEBUG_MALLOC */
	free(r);
#ifdef MALLOC_STATS
	nfree++;
#endif
}

void *
xrealloc(void *old, size_t size)
{
#ifdef DEBUG_MALLOC
	int i;
#ifdef DEBUG_MALLOC_FUDGE
	unsigned char *p;
	int j;
#endif
#endif /* DEBUG_MALLOC */
	void *r;

	if (!size)
		fatal("Xrealloc(0)");

#ifdef DEBUG_MALLOC
	if ((i = xalloced(old)) == -1)
		fatal("Reallocating unallocated memory.");
#endif

#if defined(DEBUG_MALLOC) && defined(DEBUG_MALLOC_FUDGE)
	old = (void *)((unsigned char *)old - HMALLOC_FUDGE_FACTOR);
	r = realloc(old, size + MALLOC_FUDGE_FACTOR);
#else
	r = realloc(old, size);
#endif

	if (r == (void *)NULL)
		fatal("Out of memory.\n");

#ifdef DEBUG_MALLOC
#ifdef DEBUG_MALLOC_FUDGE
	r = (void *)((unsigned char *)r + HMALLOC_FUDGE_FACTOR);

	/* Only need to reset the upper guard buffer. */
	for (p = (unsigned char *)r + size, j = 0; j < HMALLOC_FUDGE_FACTOR;
	    p++, j++)
		*p = MALLOC_FUDGE_VALUE;
#endif /* DEBUG_MALLOC_FUDGE */

	malltable[i].size = size;
	malltable[i].ptr = r;
#endif /* DEBUG_MALLOC */

#ifdef MALLOC_STATS
	tmalloc += size;
	trealloc += size;
	nrealloc++;
#endif
	return r;
}

#ifdef DEBUG_MALLOC
void
dump_malloc_table(int opt)
{
	FILE *fp;
	int i, sum;
	char *p, *q;

	if ((fp = fopen("log/debug_malloc", "w")) == (FILE *)NULL)
	{
		log_perror("dump_malloc_table");
		return;
	}
	for (sum = i = 0; i < MALLSIZE; i++)
	{
		if (malltable[i].ptr == (void *)NULL)
			continue;
		if (!opt && malltable[i].desc[0] == '*')
			continue;
		fprintf(fp, "%p : %s (%d) [", malltable[i].ptr,
		    malltable[i].desc, malltable[i].size);
		q = malltable[i].ptr;
		for (p = q; (unsigned)(p - q) < malltable[i].size &&
		    p - q < 10; p++)
			if (isprint(*p))
				fputc(*p, fp);
			else
				fputc('.', fp);
		fprintf(fp, "]\n");
		sum += malltable[i].size;
	}
	fprintf(fp, "Total size %d\n", sum);
	fclose(fp);
}

void
dump_malloc_table_sig(int sig)
{
	signal(sig, dump_malloc_table_sig);
	dump_malloc_table(1);
}
#endif /* DEBUG_MALLOC */

/* This should not really be in this file, but seeing as all the other
 * very-system-specific code is in here, why not ! */
#ifdef RUSAGE

time_t erqd_rusage_time = -1;
#if defined(SOLARIS) || defined(HPUX) || defined(sunc)
struct tms rq;
#else
struct rusage rq;
#endif

void
erqd_rusage_reply(char *buf)
{
#if defined(SOLARIS) || defined(HPUX) || defined(sunc)
	struct tms buffer;

	if (sscanf(buf, "%*d,%*d:%ld:%ld\n", (long *)&buffer.tms_utime,
	    (long *)&buffer.tms_stime) != 2)
	{
		log_file("syslog", "Illegal tms reply from erqd.");
		return;
	}
	memcpy((char *)&rq, (char *)&buffer, sizeof(struct tms));
#else
	struct rusage rus;

	if (sscanf(buf, "%*d,%*d:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld"
	    ":%ld:%ld:%ld:%ld:%ld:%ld\n",
	    (long *)&rus.ru_utime.tv_sec,
	    (long *)&rus.ru_utime.tv_usec,
	    (long *)&rus.ru_stime.tv_sec,
	    (long *)&rus.ru_stime.tv_usec,
	    (long *)&rus.ru_maxrss,
	    (long *)&rus.ru_idrss,
	    (long *)&rus.ru_minflt,
	    (long *)&rus.ru_majflt,
	    (long *)&rus.ru_nswap,
	    (long *)&rus.ru_inblock,
	    (long *)&rus.ru_oublock,
	    (long *)&rus.ru_msgsnd,
	    (long *)&rus.ru_msgrcv,
	    (long *)&rus.ru_nsignals,
	    (long *)&rus.ru_nvcsw,
	    (long *)&rus.ru_nivcsw) != 16)
	{
		log_file("syslog", "Illegal erqd rusage response.");
		return;
	}
	memcpy((char *)&rq, (char *)&rus, sizeof(struct rusage));
#endif /* SOLARIS || HPUX || sunc */
	erqd_rusage_time = current_time;
	return;
}

void
set_times(long *utime, long *stime, long *rss)
{
#if defined (SOLARIS) || defined(HPUX) || defined(sunc)
	struct tms buffer;

	if (times(&buffer) == -1)
	{
		log_perror("getrusage");
		*utime = *stime = *rss = 0;
		return;
	}
	*utime = (long)buffer.tms_utime;
	*stime = (long)buffer.tms_stime;
	*rss = 0;
#else
        struct rusage rus;

        if (getrusage(RUSAGE_SELF, &rus) < 0)
        {
                log_perror("getrusage");
		*utime = *stime = *rss = 0;
                return;
        }
	/* Use Milliseconds */
        *utime = rus.ru_utime.tv_sec * 1000 + rus.ru_utime.tv_usec / 1000;
        *stime = rus.ru_stime.tv_sec * 1000 + rus.ru_stime.tv_usec / 1000;
        *rss = rus.ru_maxrss;
#ifdef sun
        *rss *= getpagesize() / 1024;
#endif
#endif /* HPUX | SOLARIS | sunc */
}

void
print_rusage(struct user *p)
{
#if defined(SOLARIS) || defined(HPUX) || defined(sunc)
	/* awkward machine.... *grumble* */
	struct tms buffer;

	if (times(&buffer) == -1)
	{
		log_perror("getrusage");
		write_socket(p, "Error querying times information.\n");
		return;
	}
	write_socket(p, "                          Driver"
	    "                 Erqd (Elapsed: %ld)\n",
	    erqd_rusage_time == -1 ? -1 : (long)(current_time -
	    erqd_rusage_time));
	write_socket(p, "User time:            %10ld           %10ld\n",
	    (long)buffer.tms_utime, (long)rq.tms_utime);
	write_socket(p, "System time:          %10ld           %10ld\n",
	    (long)buffer.tms_stime, (long)rq.tms_stime);
	write_socket(p, "Clock tick:           %10ld\n", (long)CLK_TCK);
#else
	struct rusage rus;
	long maxrss, rq_maxrss;
	
	if (getrusage(RUSAGE_SELF, &rus) < 0)
	{
		log_perror("getrusage");
		write_socket(p, "Error querying rusage information.\n");
		return;
	}
	maxrss = rus.ru_maxrss;
	rq_maxrss = rq.ru_maxrss;
#ifdef sun
	maxrss *= getpagesize() / 1024;
	rq_maxrss *= getpagesize() / 1024;
#endif
	write_socket(p, "                          Driver"
	    "                 Erqd (Elapsed: %ld)\n",
	    erqd_rusage_time == -1 ? -1 : (long)(current_time -
	    erqd_rusage_time));
	write_socket(p, "User time:            %10.3fs         %10.3fs\n",
	    (float)rus.ru_utime.tv_sec + (float)rus.ru_utime.tv_usec / 1000000,
	    (float)rq.ru_utime.tv_sec + (float)rq.ru_utime.tv_usec / 1000000);
	write_socket(p, "System time:          %10.3fs         %10.3fs\n",
	    (float)rus.ru_stime.tv_sec + (float)rus.ru_stime.tv_usec / 1000000,
	    (float)rq.ru_stime.tv_sec + (float)rq.ru_stime.tv_usec / 1000000);
	write_socket(p, "Max res. set size:    %10ldKb         %10ldKb\n",
	    maxrss, rq_maxrss);
	write_socket(p, "Int res. set size:    %10ldKb         %10ldKb\n",
	    (long)rus.ru_idrss, (long)rq.ru_idrss);
	write_socket(p, "Minor page faults:    %10ld           %10ld\n",
	    (long)rus.ru_minflt, (long)rq.ru_minflt);
	write_socket(p, "Major page faults:    %10ld           %10ld\n",
	    (long)rus.ru_majflt, (long)rq.ru_majflt);
	write_socket(p, "Times swapped out:    %10ld           %10ld\n",
	    (long)rus.ru_nswap, (long)rq.ru_nswap);
	write_socket(p, "FS inputs:            %10ld           %10ld\n",
	    (long)rus.ru_inblock, (long)rq.ru_inblock);
	write_socket(p, "FS outputs:           %10ld           %10ld\n",
	    (long)rus.ru_oublock, (long)rq.ru_oublock);
	write_socket(p, "IPC's sent:           %10ld           %10ld\n",
	    (long)rus.ru_msgsnd, (long)rq.ru_msgsnd);
	write_socket(p, "IPC's received:       %10ld           %10ld\n",
	    (long)rus.ru_msgrcv, (long)rq.ru_msgrcv);
	write_socket(p, "Signals delivered:    %10ld           %10ld\n",
	    (long)rus.ru_nsignals, (long)rq.ru_nsignals);
	write_socket(p, "Voluntary lapses:     %10ld           %10ld\n",
	    (long)rus.ru_nvcsw, (long)rq.ru_nvcsw);
	write_socket(p, "Involuntary lapses:   %10ld           %10ld\n",
	    (long)rus.ru_nivcsw, (long)rq.ru_nivcsw);

#endif /* SOLARIS || HPUX || sunc */
	write_socket(p, "\nUser time average:    (seconds / hour).\n");
	write_socket(p, "    %8.3f   %8.3f   %8.3f\n", utime_av[0],
	    utime_av[1], utime_av[2]);
	write_socket(p, "System time average:    (seconds / hour).\n");
	write_socket(p, "    %8.3f   %8.3f   %8.3f\n", stime_av[0],
	    stime_av[1], stime_av[2]);
	write_socket(p, "\n");
	send_erq(ERQ_RUSAGE, "getrusage\n");
}
#endif /* RUSAGE */

