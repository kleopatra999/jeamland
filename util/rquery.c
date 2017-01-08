/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
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

static void
nonblock(int fd)
{
	int tmp = 1;
#if defined(_AIX) || defined(HPUX)
    	if (ioctl(fd, FIONBIO, &tmp) == -1)
    	{
		perror("ioctl socket FIONBIO");
		exit(-1);
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
		exit(-1);
	}
#endif /* _AIX || HPUX */
}

int
main(int argc, char **argv)
{
    	struct sockaddr_in addr;
    	int s;
    	char buf[0x1000];
    	int buflen;
    	int i;
    	struct timeval timeout;
    	fd_set fs;

	if (argc < 2)
	{
		fprintf(stderr, "Syntax: %s <service>\n", *argv);
		exit(-1);
	}

    	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		exit(-1);
	}

    	memset((char *)&addr, '\0', sizeof(addr));
    	addr.sin_family = AF_INET;
    	addr.sin_port = htons(SERVICE_PORT);
    	addr.sin_addr.s_addr = inet_addr(SERVICE_ADDR);

    	if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    	{
		close(s);
		perror("connect");
		exit(-1);
    	}
	nonblock(s);

    	sprintf(buf, "%s\r\n", argv[1]);
    	buflen = strlen(buf);

    	FD_ZERO(&fs);
    	FD_SET(s, &fs);
    	timeout.tv_sec = 5;
    	timeout.tv_usec = 0;
    	if (select(s + 1, FD_CAST NULL, FD_CAST&fs, FD_CAST NULL,
	    (struct timeval *)&timeout) == 0 || !FD_ISSET(s, &fs))
    	{
		close(s);
		fprintf(stderr, "Could not connect.\n");
		exit(-1);
    	}
    	if ((i = write(s, buf, buflen)) != buflen)
    	{
		close(s);
		if (i == -1)
			perror("write");
		fprintf(stderr, "Write error.\n");
		exit(-1);
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
		fprintf(stderr, "No response.\n");
		exit(-1);
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

		if (buf[i] != '\r')
	    		i++;
    	} while ((unsigned int)i < sizeof(buf));
    	buf[i] = '\0';
    	close(s);
    
	printf("%s", buf);
	exit(0);
}

