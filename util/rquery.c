/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	rquery.c
 * Function:	Small program to retrieve text from JeamLand's service
 *		port. Designed initially to be run from a cgi script.
 **********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#ifdef _AIX
#include <sys/select.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include "config.h"

#ifdef HPUX_SB
#define FD_CAST (int *)
#else
#define FD_CAST (fd_set *)
#endif

/* This may need changing, especially for remote use! */
#define SERVICE_ADDR	"127.0.0.1"
#define port DEFAULT_PORT

#ifndef SERVICE_PORT
#define SERVICE_PORT 79
#endif

static void
nonblock(int fd)
{
	int tmp = 1;
#if defined(_AIX) || defined(HPUX)
    	if (ioctl(fd, FIONBIO, &tmp) == -1)
    	{
		perror("ioctl socket FIONBIO");
		exit(1);
    	}
#else
	fcntl(fd, F_SETOWN, getpid());
	if ((tmp = fcntl(fd, F_GETFL)) == -1)
	{
		perror("fcntl socket F_GETFL");
		tmp = 0;
	}
	tmp |= O_NDELAY;
	if (fcntl(fd, F_SETFL, tmp) == -1)
	{
		perror("fcntl socket FNDELAY");
		exit(1);
	}
#endif /* _AIX || HPUX */
}

int buflen;
struct timeval timeout;
fd_set fs;
int i;

int
send_cmd(int s, char *cmd)
{
	char buf[0x100];
	int retries = 0;

	sprintf(buf, "%s\n", cmd);
	buflen = strlen(buf);

	/*
	 * See if we may write.. 
	 * If not, will try five times at two second intervals
	 */
	while (retries++ < 5)
	{
		FD_ZERO(&fs);
		FD_SET(s, &fs);
		timeout.tv_sec = 2;
		timeout.tv_usec = 0;

		if (select(s + 1, FD_CAST NULL, FD_CAST&fs, FD_CAST NULL,
		    &timeout) == 0 || !FD_ISSET(s, &fs))
			continue;	/* Try again */

		if ((i = write(s, buf, buflen)) != buflen)
		{
			if (i == -1)
				perror("write");
			printf("Error writing to server.\n");
			return 0;
		}
		return 1;
	}
	return 0;
}

int
main(int argc, char **argv)
{
    	struct sockaddr_in addr;
	FILE *fp = (FILE *)NULL;
	char buf[0x1000];
    	int s, j;

	if (argc < 2)
	{
		printf("Syntax: %s <cmd> [cmd]... | @<file>\n",
		    *argv);
		exit(1);
	}

	if (argv[1][0] == '@')
	{
		argv[1]++;
		if ((fp = fopen(argv[1], "r")) == (FILE *)NULL)
		{
			perror("fopen");
			printf("Cannot open %s.\n", argv[1]);
			exit(1);
		}
	}

	/* Connect to remote host. */

    	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		printf("Cannot create stream socket.\n");
		exit(1);
	}

    	memset((char *)&addr, '\0', sizeof(addr));
    	addr.sin_family = AF_INET;
    	addr.sin_port = htons(SERVICE_PORT);
    	addr.sin_addr.s_addr = inet_addr(SERVICE_ADDR);

    	if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    	{
		close(s);
		perror("connect");
		printf("Cannot connect to server.\n");
		exit(1);
    	}
	nonblock(s);

	/* Send request */

	if (fp == (FILE *)NULL)
	{
		while (argc > 1)
		{
			if (!send_cmd(s, argv[1]))
			{
				printf("Cannot send service command.\n");
				close(s);
				exit(1);
			}
			argc--, argv++;
		}
	}
	else
	{
		while (fgets(buf, sizeof(buf), fp) != (char *)NULL)
		{
			char *p;

			if ((p = strchr(buf, '\n')) != (char *)NULL)
				*p = '\0';

			if (!send_cmd(s, buf))
			{
				printf("Cannot send service command.\n");
				fclose(fp);
				close(s);
				exit(1);
			}
		}
		fclose(fp);
	}

	/* Read response */

    	i = 0;
    	buf[0] = '\0';
    	FD_ZERO(&fs);
    	FD_SET(s, &fs);

	/* Wait for 30 seconds... overkill ;-) */

    	timeout.tv_sec = 30;
    	timeout.tv_usec = 0;
    	if (select(s + 1, FD_CAST&fs, FD_CAST NULL, FD_CAST NULL,
	    &timeout) == 0 ||
	    !FD_ISSET(s, &fs))
    	{
		close(s);
		printf("No response from server.\n");
		exit(1);
    	}
    
	j = 0;
    	do
    	{
		FD_ZERO(&fs);
		FD_SET(s, &fs);
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;
		if (select(s + 1, FD_CAST&fs, FD_CAST NULL, FD_CAST NULL,
		    &timeout) == 0 ||
	    	    !FD_ISSET(s, &fs))
	    		break;
	
		if (read(s, buf + i, 1) != 1)
	    		break;

		if (buf[i] != '\r')
	    		i++;
		if (i >= sizeof(buf) - 1)
		{
			buf[i] = '\0';
			printf("%s", buf);
			i = 0;
			buf[0] = '\0';
			j++;
		}
    	} while (j < 5);

    	close(s);

	buf[i] = '\0';
	printf("%s", buf);
	exit(0);
}

