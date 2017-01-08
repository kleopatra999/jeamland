/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	erqd.c
 * Function:	External Request Daemon. Handles ip and user ident
 *		resolution and external email.
 **********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#ifdef _AIX
#include <sys/select.h>
#endif
#ifndef NeXT
#include <unistd.h>
#endif
#ifdef HPUX
#include <time.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "config.h"
#include "erq.h"
#include "files.h"

#ifdef RUSAGE
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

#if defined(sun) || defined(BSD)
#ifndef sunc
#define memmove(a, b, c) bcopy(b, a, c)
#endif
#endif

#ifdef HPUX_SB
#define FD_CAST (int *)
#else
#define FD_CAST (fd_set *)
#endif

#define IDENT_TIMEOUT 15
#define REPLY_ERROR(xx) printf("%d,%d:%s\n", ERQ_ERROR, id, xx); fflush(stdout)

char 	user_name[0x100];
int 	failed;

static int
nonblock(int fd)
{
	int tmp = 1;
#if defined(_AIX) || defined(HPUX)
    	if (ioctl(fd, FIONBIO, &tmp) == -1)
		return 0;
#else
	fcntl(fd, F_SETOWN, getpid());
	if ((tmp = fcntl(fd, F_GETFL)) == -1)
		tmp = 0;
	tmp |= O_NDELAY;
	if (fcntl(fd, F_SETFL, tmp) == -1)
		return 0;
#endif /* _AIX || HPUX */
	return 1;
}

char *
ident(int remote, int local_port, int remote_port)
{
    	struct sockaddr_in addr;
    	int s;
    	char buf[0x100];
    	int buflen;
    	unsigned int i;
    	struct timeval timeout;
    	fd_set fs;

	failed = 0;

    	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		return "";
    	memset((char *)&addr, '\0', sizeof(addr));
    	addr.sin_family = AF_INET;
    	addr.sin_port = htons(113);
    	addr.sin_addr.s_addr = remote;
	alarm((unsigned)IDENT_TIMEOUT);
    	if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    	{
		close(s);
		return "";
    	}
	alarm((unsigned)0);
	if (failed)
	{
		close(s);
		return "";
	}
	if (!nonblock(s))
	{
		close(s);
		return "";
	}

    	sprintf(buf, "%u , %u\r\n", remote_port, local_port);
    	buflen = strlen(buf);

    	FD_ZERO(&fs);
    	FD_SET(s, &fs);
    	timeout.tv_sec = 5;
    	timeout.tv_usec = 0;
    	if (select(s + 1, FD_CAST NULL, FD_CAST&fs, FD_CAST NULL,
	    (struct timeval *)&timeout) == 0 || !FD_ISSET(s, &fs))
    	{
		close(s);
		return "";
    	}
    	if (write(s, buf, buflen) != buflen)
    	{
		close(s);
		return "";
    	}

    	i = 0;
    	buf[0] = 0;
    	FD_ZERO(&fs);
    	FD_SET(s, &fs);
    	timeout.tv_sec = 10;
    	timeout.tv_usec = 0;
    	if (select(s + 1, FD_CAST&fs, FD_CAST NULL, FD_CAST NULL,
	    (struct timeval *)&timeout) == 0 ||
	    !FD_ISSET(s, &fs))
    	{
		close(s);
		return "";
    	}
    
    	do
    	{
		FD_ZERO(&fs);
		FD_SET(s, &fs);
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		if (select(s + 1, FD_CAST&fs, FD_CAST NULL, FD_CAST NULL,
		    (struct timeval *)&timeout) == 0 ||
	    	    !FD_ISSET(s, &fs))
	    		break;
	
		if (read(s, buf + i, 1) != 1)
	    		break;

		if (buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\r')
	    		i++;
    	} while (buf[i - 1] != '\n' && i < sizeof(buf));
    	buf[i] = '\0';
    	close(s);
    
    	if (sscanf(buf, "%*d,%*d: USERID :%*[^:]:%s", user_name) != 1)
		return "";
    	else
		return user_name;
}

void
alarm_trap(int sig)
{
	signal(sig, alarm_trap);
	failed++;
}

#define EMAIL_REPLY(xx)	printf("%d,%d:%s;%d;\n", ERQ_EMAIL, id, sender, xx)

static int email_id;
static char *email_sender;
void
email_pipe(int sig)
{
	int id = email_id;
	char *sender = email_sender;

	EMAIL_REPLY(0);
	REPLY_ERROR("Pipe error; sendmail missing ?");
}

int
main()
{
    	char buf[0x100];
	int request, id;
	char *cmd;

	signal(SIGSTOP, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
#if defined(sgi) && defined(SIGPOLL)
	signal(SIGPOLL, SIG_IGN);
#endif

	signal(SIGALRM, alarm_trap);

	/* Let's tell the main server who we are.. */
	printf("%d,0:External request daemon v.%s\n",
	    ERQ_IDENTIFY, ERQD_VERSION);
	fflush(stdout);

    	for (;;) /* boring life being an erqd... */
	{
		/* Is the main server still there ? */
		if (getppid() == (pid_t)1)
			exit(0);

		if (gets(buf) == (char *)NULL)
			continue;

		if (sscanf(buf, "%d,%d", &request, &id) != 2)
		{
			REPLY_ERROR("no request code or no id");
			continue;
		}
		if ((cmd = strchr(buf, ':')) == (char *)NULL || *(++cmd) ==
		    '\0')
		{
			REPLY_ERROR("bad format");
			continue;
		}
		switch (request)
		{
		    case ERQ_DIE:
			/* ARRGGGHHHHH!!!!!! *gurgle* */
			exit(2);
		    case ERQ_RESOLVE_NUMBER:
		    {
			char ipnum[0x100];
			unsigned long addr;
			struct hostent *hp;
			struct in_addr saddr;

			if (!sscanf(cmd, "%[^;]", ipnum))
				break;
			if ((addr = inet_addr(ipnum)) == -1)
				break;
	    		hp = gethostbyaddr((char *)&addr, 4, AF_INET);
	    		if (hp == (struct hostent *)NULL) 
			{
				sleep(5);
	        		hp = gethostbyaddr((char *)&addr, 4, AF_INET);
	    		}
	    		if (hp == (struct hostent *)NULL) 
			{
				printf("%d,%d:%s;\n", ERQ_FAILED_RESOLVE, id,
				    ipnum);
				fflush(stdout);
				break;
			}
			memmove(&(saddr.s_addr), hp->h_addr, 4);
			printf("%d,%d:%s %s;\n", ERQ_RESOLVE_NUMBER, id,
			    ipnum, hp->h_name);
			fflush(stdout);
			break;
		    }
		    case ERQ_RESOLVE_NAME:
		    {
			char ipname[0x100];
			struct hostent *hp;
			struct in_addr saddr;

			if (!sscanf(cmd, "%[^;]", ipname))
				break;
	    		hp = gethostbyname(ipname);
	    		if (hp == (struct hostent *)NULL) 
			{
				sleep(5);
	        		hp = gethostbyname(ipname);
	    		}
	    		if (hp == (struct hostent *)NULL) 
			{
				printf("%d,%d:%s;\n", ERQ_FAILED_RESOLVE, id,
				    ipname);
				fflush(stdout);
				break;
			}
			memmove(&(saddr.s_addr), hp->h_addr, 4);
			printf("%d,%d:%s %s;\n", ERQ_RESOLVE_NAME, id,
			    inet_ntoa(saddr), hp->h_name);
			fflush(stdout);
			break;
		    }
		    case ERQ_IDENT:
		    {
			int lport, rport;
			char ipnum[0x100];
			char *name;
			unsigned long addr;

			if (sscanf(cmd, "%[^;];%d,%d", ipnum, &lport, &rport)
			    != 3)
				break;
			if ((addr = inet_addr(ipnum)) == -1)
				break;
			name = ident(addr, lport, rport);
			printf("%d,%d:%s;%d,%d;%s;\n", ERQ_IDENT, id, ipnum,
			    lport, rport, name);
			fflush(stdout);
			break;
		    }
#ifdef RUSAGE
		    case ERQ_RUSAGE:
		    {
#if defined(SOLARIS) || defined(HPUX) || defined(sunc)
			/* awkward machine.... *grumble* */
			struct tms buffer;

			if (times(&buffer) == -1)
			{
				REPLY_ERROR("times problem");
				break;
			}
			printf("%d,%d:%ld:%ld\n",
			    ERQ_RUSAGE, id,
			    (long)buffer.tms_utime,
			    (long)buffer.tms_stime);
#else
			struct rusage rus;
	
			if (getrusage(RUSAGE_SELF, &rus) < 0)
			{
				REPLY_ERROR("getrusage problem");
				break;
			}
			printf("%d,%d:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld"
			    ":%ld:%ld:%ld:%ld:%ld:%ld\n",
			    ERQ_RUSAGE, id,
			    (long)rus.ru_utime.tv_sec,
			    (long)rus.ru_utime.tv_usec,
			    (long)rus.ru_stime.tv_sec,
			    (long)rus.ru_stime.tv_usec,
			    (long)rus.ru_maxrss,
			    (long)rus.ru_idrss,
			    (long)rus.ru_minflt,
			    (long)rus.ru_majflt,
			    (long)rus.ru_nswap,
			    (long)rus.ru_inblock,
			    (long)rus.ru_oublock,
			    (long)rus.ru_msgsnd,
			    (long)rus.ru_msgrcv,
			    (long)rus.ru_nsignals,
			    (long)rus.ru_nvcsw,
			    (long)rus.ru_nivcsw);
#endif /* SOLARIS || HPUX || sunc */
			fflush(stdout);
			break;
		    }
#endif /* RUSAGE */
		    case ERQ_EMAIL:
		    {
			char file[0x100];
			static char sender[0x100];
			char realfile[MAXPATHLEN + 1];
			FILE *sm, *smi;
			char buff[0x200];
			int delete_after;
			
			if (sscanf(cmd, "%[^;];%[^;];%d;", sender, file,
			    &delete_after) != 3)
				break;

			sprintf(realfile, LIB_PATH "/" F_EMAILD "%s",
			    file);

			if ((smi = fopen(realfile, "r")) == (FILE *)NULL)
			{
				EMAIL_REPLY(0);
				REPLY_ERROR("Email file missing.");
				break;
			}

			/* Error recovery */
			email_id = id;
			email_sender = sender;
			signal(SIGPIPE, email_pipe);
			
			if ((sm = popen("sendmail -t", "w")) == (FILE *)NULL)
			{
				fclose(smi);
				signal(SIGPIPE, SIG_IGN);
				EMAIL_REPLY(0);
				REPLY_ERROR("sendmail missing.");
				break;
			}
			while (fgets(buff, sizeof(buff), smi) != (char *)NULL)
				fprintf(sm, buff);
			fclose(smi);
			pclose(sm);
			if (delete_after)
				unlink(realfile);
			signal(SIGPIPE, SIG_IGN);
			EMAIL_REPLY(1);
			fflush(stdout);
			break;
		    }
		    default:
			REPLY_ERROR("unknown request");
			break;
		}
	}
}

