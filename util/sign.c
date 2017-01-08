/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	sign.c
 * Function:	Displays a message on a port
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#ifdef _AIX
#include <sys/select.h>
#endif
#include <fcntl.h>
#include <netdb.h>
#include "files.h"
#include "config.h"

#ifdef HPUX_SB
#define FD_CAST (int *)
#else
#define FD_CAST (fd_set *)
#endif /* HPUX */

#define TRY(xx, yy) if ((xx) == -1) { perror(yy); exit(1); }

#ifdef BUGGY_INET_NTOA
#define MAX_INET_ADDR 30
char *
inet_ntoa(struct in_addr ad)
{
        unsigned long s_ad, a, b, c, d;
        static char addr[MAX_INET_ADDR];

        s_ad = ad.s_addr;
        d = s_ad % 256;
        s_ad /= 256;
        c = s_ad % 256;
        s_ad /= 256;
        b = s_ad % 256;
        a = s_ad / 256;
        sprintf(addr, "%d.%d.%d.%d", a, b, c, d);
        return addr;
}
#endif

void
nonblock(int fd)
{
#if defined(_AIX) || defined(HPUX)
	int tmp = 1;
	TRY(ioctl(fd, FIONBIO, &tmp), "nonblock")
#else
#ifdef F_SETOWN
	fcntl(fd, F_SETOWN, getpid());
#endif
	TRY(fcntl(fd, F_SETFL, FNDELAY), "nonblock")
#endif
}

int
prepare_ipc(int port)
{
	char hostname[100];
	struct sockaddr_in sin;
	struct hostent *hp;
	int tmp, sockfd;

	TRY(gethostname(hostname, sizeof(hostname)), "gethostname")
	if ((hp = gethostbyname(hostname)) == (struct hostent *)NULL)
	{
		perror("gethostbyname");
		exit(1);
	}
	memset((char *)&sin, '\0', sizeof(sin));
	memcpy((char *)&sin.sin_addr, hp->h_addr, hp->h_length);
	sin.sin_port = htons((unsigned short)port);
	sin.sin_family = hp->h_addrtype;
	sin.sin_addr.s_addr = INADDR_ANY;
	TRY(sockfd = socket(hp->h_addrtype, SOCK_STREAM, 0), "socket")

	tmp = 1;
	TRY(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&tmp,
	    sizeof(tmp)), "setsockopt")
	TRY(bind(sockfd, (struct sockaddr *)&sin, sizeof(sin)), "bind")
	TRY(listen(sockfd, 7), "listen")
	nonblock(sockfd);
	printf("Listening on port %d.\n", port);
	return sockfd;
}

char *
read_file(char *fname)
{
	char *msg, line[0x100];
	FILE *fp;
	struct stat st;

	if (stat(fname, &st) == -1)
	{
		fprintf(stderr, "%s: file not found.\n", fname);
		exit(1);
	}
	if ((fp = fopen(fname, "r")) == (FILE *)NULL)
	{
		perror("fopen");
		fprintf(stderr, "%s: cannot read file.\n", fname);
		exit(1);
	}
	/* This is an overestimate.. */
	if ((msg = (char *)malloc(st.st_size * 2 + 1)) == (char *)NULL)
	{
		fprintf(stderr, "Out of memory!\n");
		exit(1);
	}
	msg[0] = '\0';

	while (fgets(line, sizeof(line), fp) != (char *)NULL)
	{
		strcat(msg, line);
		strcat(msg, "\r");
	}
	fclose(fp);
	/* Fix the length of the msg string */
	msg = (char *)realloc(msg, strlen(msg) + 1);
	return msg;
}

int
new_connection(int fd)
{
	struct sockaddr_in addr;
	int length, newfd;

	length = sizeof(addr);
	getsockname(fd, (struct sockaddr *)&addr, &length);
	if ((newfd = accept(fd, (struct sockaddr *)&addr, &length)) < 0)
	{
		perror("accept");
		return -1;
	}
	printf("Connection from %s\n", inet_ntoa(addr.sin_addr));
	nonblock(newfd);
	return newfd;
}

int
process_socket(int fd)
{
	fd_set readfds;
	struct timeval delay;

	for(;;)
	{
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);

		delay.tv_sec = 1200;
		delay.tv_usec = 0;

		if (select(FD_SETSIZE, FD_CAST&readfds, FD_CAST NULL,
		    FD_CAST NULL, (struct timeval *)NULL) < 0)
		{
			perror("select");
			exit(1);
		}
		if (FD_ISSET(fd, &readfds))
			return new_connection(fd);
	}
}

void
write_socket(int fd, char *msg)
{
	int length = strlen(msg);
	int l, c;

	for (l = 0; l < length; l += c)
	{
		if ((c = write(fd, msg + l, length - l)) < 0)
		{
			perror("write");
			return;
		}
	}
	sleep(5);
}

int
main(int argc, char **argv)
{
	int fd, nfd, port, log = 0;
	char *msg;

	if (argc < 3)
	{
		fprintf(stderr, "Usage: %s [-l] <port> <filename>\n", *argv);
		exit(0);
	}

#ifdef SETPROCTITLE
        /* Kludge, but does the job */
        if (strncmp(argv[0], "-=>", 3))
        {
                char *tmp = (char *)malloc(0x50);
		char *old = argv[0];

                sprintf(tmp, "-=> %s Sign <=-", LOCAL_NAME);
                argv[0] = tmp;
                execv(BIN_PATH "/bin/sign", argv);
		/* Failed - try current directory */
		execv(old, argv);
		/* Failed - continue anyway */
		free(tmp);
        }
#endif

#ifdef NICE
	nice(NICE);
#endif

	if (argc > 3 && *argv[1] == '-')
	{
		switch(argv[1][1])
		{
		    case 'l':
			log = 1;
			break;
		    default:
			fprintf(stderr, "Unknown option '%c'.\n", argv[1][1]);
			break;
		}
		argv++;
	}
	if (log)
	{
		switch((int)fork())
		{
		    case -1:
			perror("fork");
			exit(1);
		    case 0:
			break;
		    default:
			exit(0);
		}
		freopen(TOP_DIR "/" F_SIGN_LOG, "w", stdout);
		freopen(TOP_DIR "/" F_SIGN_LOG, "w", stderr);
	}

	if ((port = atoi(argv[1])) < 1024 || port > 65535)
	{
		fprintf(stderr, "Bad port number, %d\n", port);
		exit(1);
	}
	msg = read_file(argv[2]);
	fd = prepare_ipc(port);

	for(;;)
	{
		if ((nfd = process_socket(fd)) == -1)
			continue;
		write_socket(nfd, msg);
		close(nfd);
	}
}

