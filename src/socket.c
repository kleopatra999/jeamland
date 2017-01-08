/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	socket.c
 * Function:	All of the main socket code, including Erqd comms.
 **********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/types.h>
#ifndef NeXT
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#undef DEBUG_TELNET
#ifdef DEBUG_TELNET
#define TELOPTS
#define TELCMDS
#endif
#include <arpa/telnet.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#if defined(__linux) && defined(LITTLE_ENDIAN)
/* Seems to be a small redefinition problem in the 1.2.6 kernel include files..
 */
#undef LITTLE_ENDIAN
#endif
#ifdef HPUX
#include <time.h>
#endif
#include <sys/file.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <ctype.h>
#ifdef _AIX
#include <sys/select.h>
#endif
#include "jeamland.h"

#ifdef HPUX_SB
#define FD_CAST	(int *)
#else
#define FD_CAST (fd_set *)
#endif

/* Main server listening socket */
static int	sockfd = -1;

#if defined(CDUDP_SUPPORT) || defined(INETD_SUPPORT) || defined(REMOTE_NOTIFY)
static int	udp_send = -1;
#endif
#ifdef INETD_SUPPORT
static struct udpsock *udp_s = (struct udpsock *)NULL;
#endif
#ifdef CDUDP_SUPPORT
static struct udpsock *cdudp_s = (struct udpsock *)NULL;
#endif
#ifdef REMOTE_NOTIFY
static struct udpsock *notify_s = (struct udpsock *)NULL;
#endif

#ifdef SERVICE_PORT
/* Service port listening socket */
static int service_s = -1;
#endif

static int console_used = 1;
static FILE	*erqp = (FILE *)NULL, *erqw;
int	erqd_fd = -1;

char	*host_name, *host_ip;
extern struct user *users;
extern struct room *rooms;
extern struct jlm *jlms;

#ifdef IMUD3_SUPPORT
extern struct tcpsock i3_router;
extern void i3_incoming(struct tcpsock *);
#endif

struct ipentry iptable[IPTABLE_SIZE];
int ipcur;

extern int errno, port;
extern time_t current_time;
extern int sysflags;

static int handle_telnet(struct tcpsock *, char);
void add_username(int, int, char *, char *);
void add_ip_entry(char *, char *);
void incoming_udp(void), incoming_cdudp(void);

/* Stats on socket communication. */
int bytes_out, bytes_in, bytes_outt, bytes_int;
float out_bps, in_bps;
int ubytes_out, ubytes_in, ubytes_outt, ubytes_int;

int num_users = 0;

#ifdef BUGGY_INET_NTOA
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

/*
 * Should failure be considered fatal ?
 */
void
nonblock(int fd)
{
	int tmp = 1;
#if defined(_AIX) || defined(HPUX)
    	if (ioctl(fd, FIONBIO, &tmp) == -1)
    	{
		log_perror("ioctl socket FIONBIO");
		log_file("syslog", "Could not set socket non-blocking !");
    	}
#else
	fcntl(fd, F_SETOWN, getpid());
	if ((tmp = fcntl(fd, F_GETFL)) == -1)
	{
		log_perror("fcntl socket F_GETFL");
		tmp = 0;
	}
	tmp |= O_NDELAY;
	if (fcntl(fd, F_SETFL, tmp) == -1)
	{
		log_perror("fcntl socket FNDELAY");
		log_file("syslog", "Could not set socket non-blocking !");
	}
#endif
}

void
detach_console()
{
	extern void background_process(void);
#ifdef TIOCNOTTY
	if (ioctl(fileno(stdin), TIOCNOTTY) == -1)
		log_perror("detach_console");
#else
	fclose(stdin);	/* Ugh */
#endif
	/* Flag is used to speed up process_sockets */
	console_used = 1;
	sysflags &= ~SYS_CONSOLE;
#ifndef FREEBSD
	setpgrp();
#endif
	background_process();
}

void
init_tcpsock(struct tcpsock *s, int buffer)
{
	s->fd = -1;
	s->input_buffer = (char *)xalloc(buffer, "Tcpsock buffer");
	s->ib_size = buffer;
	s->ib_offset = 0;
	s->lines = 0;
	s->con_time = current_time;
	s->flags = 0;
	s->sendq = s->recvq = 0;
	s->lport = s->rport = 0;
	s->telnet.state = TS_IDLE;
	s->telnet.expect = 0;
}

struct tcpsock *
create_tcpsock(char *id)
{
	struct tcpsock *s = (struct tcpsock *)xalloc(sizeof(struct tcpsock),
	    id);

	return s;
}

void
free_tcpsock(struct tcpsock *s)
{
#ifdef DEBUG
	if (s->fd != -1)
		fatal("free_tcpsock: fd not -1.");
#endif
	xfree(s->input_buffer);
	s->input_buffer = (char *)NULL;
	if (s->flags & TCPSOCK_BLOCKED)
		free_strbuf(&s->output_buffer);
}

void
tfree_tcpsock(struct tcpsock *s)
{
	free_tcpsock(s);
	xfree(s);
}

int
connect_tcpsock(struct tcpsock *s, char *ip, int port)
{
	struct sockaddr_in addr;
	int fd;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		log_perror("connect_tcpsock socket");
		return -1;
	}

	memset((char *)&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	/* This could very well hang the talker while it executes.. */
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		close_fd(fd);
		log_perror("connect_tcpsock connect");
		return -1;
	}

	/* Connected.. */
	s->fd = fd;
	memcpy((char *)&(s->addr), (char *)&addr, sizeof(addr));
	s->rport = port;
	s->con_time = current_time;

	nonblock(fd);

	return 1;
}

struct udpsock *
create_udpsock(char *id, int buffer)
{
	struct udpsock *s = (struct udpsock *)xalloc(sizeof(struct udpsock),
	    id);

	if (!buffer)
		buffer = DEF_UDPSOCK_BUF;

	s->fd = -1;
	s->input_buffer = (char *)xalloc(buffer, "Udpsock buffer");
	s->ib_size = buffer;
	s->ib_offset = 0;

	return s;
}

void
free_udpsock(struct udpsock *s)
{
	xfree(s->input_buffer);
	xfree(s);
}

void
close_udpsock(struct udpsock *s)
{
	if (s == (struct udpsock *)NULL)
		return;
	if (s->fd != -1 && close_fd(s->fd) == -1)
		log_perror("close udpsock");
	free_udpsock(s);
}

static int 
setup_socket(int port_number, int type, struct hostent *hp)
{
	struct sockaddr_in sin;
    	int tmp, sockfd;

    	memset((char *)&sin, '\0', sizeof(sin));
    	memcpy((char *)&sin.sin_addr, hp->h_addr, hp->h_length);
    	sin.sin_port = htons((unsigned short)port_number);
    	sin.sin_family = hp->h_addrtype;
    	sin.sin_addr.s_addr = INADDR_ANY;
    	if ((sockfd = socket(hp->h_addrtype, type, 0)) == -1)
    	{
		log_perror("socket");
		fatal("Could not create socket.");
    	}

    	tmp = 1;
    	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
	    (char *)&tmp, sizeof(tmp)) < 0) 
    	{
		log_perror("setsockopt");
		fatal("Could not set socket re-useable.");
    	}

    	if (bind(sockfd, (struct sockaddr *)&sin, sizeof(sin)) == -1)
    	{
	    	log_perror("bind");
	   	fatal("Could not bind socket."); 
	}

	/* We dinna want to listen on a DATAGRAM socket. */
    	if (type == SOCK_STREAM && listen(sockfd, 7) == -1)
    	{
		log_perror("listen");
		fatal("Could not listen on port");
    	}
	nonblock(sockfd);
	return sockfd;
}

void
prepare_ipc()
{
	char hst_name[100];
	struct hostent *hp;
	struct in_addr saddr;

	bytes_out = bytes_in = bytes_outt = bytes_int = 0;
	in_bps = out_bps = 0.0;
	ubytes_out = ubytes_in = ubytes_outt = ubytes_int = 0;

    	if (gethostname(hst_name, sizeof(hst_name)) == -1)
    	{
        	log_perror("gethostname");
		fatal("Error in gethostname()");
    	}
    	if ((hp = gethostbyname(hst_name)) == (struct hostent *)NULL)
    	{
		log_perror("gethostbyname");
		fatal("Cannot get local hostname.");
    	}
#ifdef OVERRIDE_HOST_NAME
	host_name = string_copy(OVERRIDE_HOST_NAME, "*localhost name");
#else
	host_name = string_copy((char *)hp->h_name, "*localhost name");
#endif
	memmove((char *)&saddr.s_addr, (char *)hp->h_addr,
	    sizeof(saddr.s_addr));
	host_ip = string_copy(inet_ntoa(saddr), "*localhost ipnum");
	log_file("syslog", "Local hostname: %s", host_name);
	log_file("syslog", "Local hostip:   %s", host_ip);

	signal(SIGPIPE, SIG_IGN);
	if (!(sysflags & SYS_NOIPC))
	{
		log_file("syslog", "Setting up ipc... [%d]", port);
		sockfd = setup_socket(port, SOCK_STREAM, hp);
	}

	if (!(sysflags & SYS_NOUDP))
	{
#ifdef INETD_SUPPORT
		log_file("syslog", "Setting up ijp... [%d]", INETD_PORT);
		udp_s = create_udpsock("ijp udpsock", 0);
		udp_s->fd = setup_socket(INETD_PORT, SOCK_DGRAM, hp);
#endif
#ifdef CDUDP_SUPPORT
		log_file("syslog", "Setting up icp... [%d]", CDUDP_PORT);
		cdudp_s = create_udpsock("icp udpsock", 0);
		cdudp_s->fd = setup_socket(CDUDP_PORT, SOCK_DGRAM, hp);
#endif
	}
#if defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT) || defined(REMOTE_NOTIFY)
	if (!(sysflags & (SYS_NOUDP | SYS_NONOTIFY)))
		if ((udp_send = socket(hp->h_addrtype, SOCK_DGRAM, 0)) == -1)
		{
			log_perror("udp send socket");
			fatal("Could not create UDP sending socket");
		}
#endif

#ifdef SERVICE_PORT
	if (!(sysflags & SYS_NOSERVICE))
	{
		log_file("syslog", "Setting up jsp... [%d]", SERVICE_PORT);
		service_s = setup_socket(SERVICE_PORT, SOCK_STREAM, hp);
	}
#endif /* SERVICE_PORT */

#ifdef REMOTE_NOTIFY
	if (!(sysflags & SYS_NONOTIFY))
	{
		log_file("syslog", "Setting up rnp... [%d]", NOTIFY_PORT);
		notify_s = create_udpsock("rnp udpsock", 200);
		notify_s->fd = setup_socket(NOTIFY_PORT, SOCK_DGRAM, hp);
	}
#endif /* REMOTE_NOTIFY */
}

void 
remove_ipc()
{
	close_fd(sockfd);
#ifdef SERVICE_PORT
	close_fd(service_s);
#endif
#ifdef REMOTE_NOTIFY
	close_udpsock(notify_s);
#endif
#ifdef INETD_SUPPORT
	close_udpsock(udp_s);
#endif
#ifdef CDUDP_SUPPORT
	close_udpsock(cdudp_s);
#endif
#if defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT) || defined(REMOTE_SERVICE)
	close_fd(udp_send);
#endif
}

static void
kick_logons(struct event *ev)
{
	struct user *p;

	for (p = users->next; p != (struct user *)NULL; p = p->next)
	{
                if (!IN_GAME(p) && current_time - p->login_time >=
                    (time_t)LOGIN_TIMEOUT)
                {
                        write_socket(p, "\nLogin timed out after %d"
			    " seconds.\n", LOGIN_TIMEOUT);
			disconnect_user(p, 1);
                }
	}
}

#ifdef SERVICE_PORT

enum serv_modes { SM_IDLE, SM_EMAIL, SM_EMAILDATA };

/* Timeout after 60 seconds */
#define SERV_TIMEOUT	60

/* 5K of email is the limit */
#define SERV_EMAIL_LIMIT 0x1400

/* Flags */
#define SERVICE_CLOSING	0x1

/*
 * If MAX_SERVICES ever exceeds 5 or so, consider making services a dynamic
 * linked list instead of static array.
 */

static struct service {
	struct tcpsock socket;

	enum serv_modes mode;
	int event;
	int flags;

#ifdef SERV_EMAIL
	struct strbuf buf;
	struct stack stack;
#endif
	} services[MAX_SERVICES];

static struct {
	char *serv;
	enum serv_modes mode;
	} serv_names[] = {
	{ "who",	SM_IDLE },
	{ "finger",	SM_IDLE },
#ifdef AUTO_VALEMAIL
	{ "valemail",	SM_IDLE },
#endif
#ifdef SERV_EMAIL
	{ "email",	SM_IDLE },
	{ "email-to",	SM_EMAIL },
	{ "email-from",	SM_EMAIL },
	{ "email-subj",	SM_EMAIL },
	{ "email-data",	SM_EMAIL },
#endif
	{ (char *)NULL, 0 }
	};

void
init_services()
{
	int i;

	for (i = MAX_SERVICES; i--; )
		services[i].socket.fd = -1;
}

void
close_service(int i)
{
	close_tcpsock(&services[i].socket);
	free_tcpsock(&services[i].socket);

	remove_event(services[i].event);
	services[i].event = 0;

#ifdef SERV_EMAIL
	switch (services[i].mode)
	{
	    case SM_EMAILDATA:
		free_strbuf(&services[i].buf);
		/* Fallthrough */

	    case SM_EMAIL:
		clean_stack(&services[i].stack);
		break;

	    case SM_IDLE:
	    default:
		break;
	}
#endif
}

static void
idle_service(struct event *ev)
{
	int i = ev->stack.sp->u.number;

	dec_stack(&ev->stack);

	if (services[i].socket.fd == -1 || services[i].event != ev->id)
		return;

	log_file("services", "[%-3d] Idle timeout.", services[i].socket.fd);

	write_tcpsock(&services[i].socket, "ERROR: Idle timeout.\r\n", 22);
	close_service(i);
}

static void
new_service(int fd)
{
	struct event		*ev;
	struct sockaddr_in	addr;
	int 			length, newfd;
	int i;

	length = sizeof(addr);
	if (getsockname(fd, (struct sockaddr *)&addr, &length) == -1)
	{
		log_perror("service getsockname");
		return;
	}
	if ((newfd = accept(fd, (struct sockaddr *)&addr, &length)) < 0)
	{
		log_perror("service accept");
		return;
	}
	nonblock(newfd);

	/* Any free service slots ? */
	for (i = MAX_SERVICES; i--; )
		if (services[i].socket.fd == -1)
			break;
	if (i < 0)
	{
		write(newfd, "ERROR: All service slots in use.\n", 33);
		if (close_fd(newfd) == -1)
			log_perror("close service nospace");
		return;
	}

	init_tcpsock(&services[i].socket, SERVICE_BUFSIZE);
	services[i].socket.fd = newfd;
	memcpy((char *)&(services[i].socket.addr), (char *)&addr, length);
	services[i].socket.flags |= TCPSOCK_TELNET;
	services[i].socket.con_time = current_time;
	services[i].mode = SM_IDLE;
	services[i].flags = 0;

	send_erq(ERQ_RESOLVE_NUMBER, "%s;\n", inet_ntoa(addr.sin_addr));

	log_file("services", "[%-3d] Connect from %s.", newfd,
	    ip_name(&services[i].socket));

	ev = create_event();
	push_number(&ev->stack, i);
	services[i].event = add_event(ev, idle_service, SERV_TIMEOUT,
	    "service");
}

static void
handle_service(int i)
{
	char *buf;
	char *p;
	int c;

	FUN_START("handle_service");

	/* There is something waiting on the socket, read it. */
	if ((c = scan_tcpsock(&services[i].socket)) <= 0)
	{
		if (!c)
			log_file("syslog", "Service flush");
		else
			log_perror("service read");
		close_service(i);
		FUN_END;
		return;
	}

	FUN_LINE;

	/* Process waiting commands. */
	while ((buf = process_tcpsock(&services[i].socket)) != (char *)NULL)
	{
		if ((p = strpbrk(buf, "\n\r")) != (char *)NULL)
			*p = '\0';

#ifdef SERV_EMAIL
		/* If in email data mode, just accept data - up to a point! */
		if (services[i].mode == SM_EMAILDATA)
		{
			if (!strcmp(buf, "."))
			{
				/* End of data... deliver mail */
				if (deliver_mail(
				    (services[i].stack.sp - 2)->u.string,
				    (services[i].stack.sp - 1)->u.string,
				    services[i].stack.sp->u.string,
				    services[i].buf.str, (char *)NULL, 0, 0))
					write_tcpsock(&services[i].socket,
					    "OK: mail accepted.\r\n", 20);
				else
					write_tcpsock(&services[i].socket,
					    "ERROR: mail rejected.\r\n", 23);
				close_service(i);
				xfree(buf);
				FUN_END;
				return;
			}

			add_strbuf(&services[i].buf, buf);
			cadd_strbuf(&services[i].buf, '\n');
			xfree(buf);
			if (services[i].buf.offset > SERV_EMAIL_LIMIT)
			{
				log_file("services",
				    "[%-3d] Error, email too long.",
				    services[i].socket.fd);

				write_tcpsock(&services[i].socket,
				    "ERROR: email too long\r\n", 23);
				close_service(i);
				FUN_END;
				return;
			}
			continue;
		}

		FUN_LINE;

#endif /* SERV_EMAIL */

		/* Find the command. */
		if ((p = strchr(buf, ' ')) != (char *)NULL)
			*p++ = '\0';

		for (c = 0; serv_names[c].serv != (char *)NULL; c++)
			if (!strcmp(serv_names[c].serv, buf))
			{
				if (serv_names[c].mode != services[i].mode)
				{
					write_tcpsock(&services[i].socket,
					    "ERROR: command invalid in this "
					    "mode\r\n", 37);
					close_service(i);
					xfree(buf);
					FUN_END;
					return;
				}
				break;
			}

		FUN_LINE;

		switch (c)
		{
		    case 0:	/* who */
		    {
			log_file("services", "[%-3d] who.",
			    services[i].socket.fd);

			p = who_text(FINGER_SERVICE);
			write_tcpsock(&services[i].socket, p, -1);
			xfree(p);
			/* Don't close now, might be data waiting to go
			 * out */
			services[i].flags |= SERVICE_CLOSING;
			xfree(buf);
			break;
		    }

		    case 1:	/* finger */
		    {
			if (p == (char *)NULL)
			{
				write_tcpsock(&services[i].socket,
				    "ERROR: No user.\r\n", 17);
				close_service(i);
				xfree(buf);
				FUN_END;
				return;
			}
			else
			{
				log_file("services", "[%-3d] finger %s.",
				    services[i].socket.fd, p);

				p = finger_text((struct user *)NULL, p,
				    FINGER_SERVICE);
				write_tcpsock(&services[i].socket, p, -1);
				xfree(p);
				/* Don't close now, might be data waiting to
				 * go out */
				services[i].flags |= SERVICE_CLOSING;
				xfree(buf);
				break;
			}

			/* Notreached */

			FUN_END;
			return;
		    }

#ifdef AUTO_VALEMAIL
		    case 2:	/* Valemail */
			/* Format is: valemail password id */

			log_file("services", "[%-3d] valemail.",
			    services[i].socket.fd);
			if (p != (char *)NULL && valemail_service(buf, p))
				write_tcpsock(&services[i].socket,
				    "OK: Valemail accepted.\r\n", 24);
			else
				write_tcpsock(&services[i].socket,
				    "ERROR: Valemail Denied.\r\n", 25);
			close_service(i);
			xfree(buf);
			FUN_END;
			return;
#endif

#ifdef SERV_EMAIL
		    case 3:	/* Email */
			/* Argument is the password */
			if (p == (char *)NULL ||
			    strcmp(p, SERV_EMAIL))
			{
				write_tcpsock(&services[i].socket,
				    "ERROR: Email Denied.\r\n", 22);
				close_service(i);
				xfree(buf);
				FUN_END;
				return;
			}

			/* Password is ok.. set up stack and go into
			 * EMAIL mode */
			init_stack(&services[i].stack);
			services[i].mode = SM_EMAIL;
			xfree(buf);
			log_file("services", "[%-3d] email.",
			    services[i].socket.fd);
			continue;

		    case 4:	/* email-to */
			if (services[i].stack.el != 0)
			{
				write_tcpsock(&services[i].socket,
				    "ERROR: Email commands out of sync.\r\n",
				    37);
				close_service(i);
				xfree(buf);
				FUN_END;
				return;
			}

			if (p == (char *)NULL || !exist_user(p))
			{
				write_tcpsock(&services[i].socket,
				    "ERROR: No such user.\r\n", 22);
				close_service(i);
				xfree(buf);
				FUN_END;
				return;
			}

			push_string(&services[i].stack, p);
			xfree(buf);
			continue;

		    case 5:	/* email-from */
			if (services[i].stack.el != 1)
			{
				write_tcpsock(&services[i].socket,
				    "ERROR: Email commands out of sync.\r\n",
				    37);
				close_service(i);
				xfree(buf);
				FUN_END;
				return;
			}

			if (p == (char *)NULL)
			{
				write_tcpsock(&services[i].socket,
				    "ERROR: Bad sender.\r\n", 20);
				close_service(i);
				xfree(buf);
				FUN_END;
				return;
			}

			push_string(&services[i].stack, p);
			xfree(buf);
			continue;

		    case 6:	/* email-subj */
			if (services[i].stack.el != 2)
			{
				write_tcpsock(&services[i].socket,
				    "ERROR: Email commands out of sync.\r\n",
				    37);
				close_service(i);
				xfree(buf);
				FUN_END;
				return;
			}

			if (p == (char *)NULL)
			{
				write_tcpsock(&services[i].socket,
				    "ERROR: Bad subject.\r\n", 21);
				close_service(i);
				xfree(buf);
				FUN_END;
				return;
			}

			push_string(&services[i].stack, p);
			xfree(buf);
			continue;

		    case 7:	/* email-data */
			if (services[i].stack.el != 3)
			{
				write_tcpsock(&services[i].socket,
				    "ERROR: Email commands out of sync.\r\n",
				    37);
				close_service(i);
				xfree(buf);
				FUN_END;
				return;
			}

			services[i].mode = SM_EMAILDATA;
			init_strbuf(&services[i].buf, 0, "sp emaildata");
			xfree(buf);
			continue;
#endif /* SERV_EMAIL */

		    default:
			log_file("services", "[%-3d] Unknown service: %s.", 
			    services[i].socket.fd, buf);
			write_tcpsock(&services[i].socket,
			    "ERROR: Unknown service.\r\n", 25);
			close_service(i);
			xfree(buf);
			FUN_END;
			return;
		}

		if ((services[i].flags & SERVICE_CLOSING) &&
		    !(services[i].socket.flags & TCPSOCK_BLOCKED))
		{
			close_service(i);
			FUN_END;
			return;
		}
	}
	FUN_END;
}
#endif /* SERVICE_PORT */

#ifdef REMOTE_NOTIFY

static struct {
	char		*host;
	char 		*user;
	char		*ft;
	int 		 port;
	unsigned long	 id;
	time_t 		 last_contact;
	unsigned long 	 version;
	} remote_notify[MAX_REMOTE_NOTIFY];

static unsigned long rnc_id = 0;

void
init_rnotify()
{
	int i;

	for (i = MAX_REMOTE_NOTIFY; i--; )
		remote_notify[i].host =
		    remote_notify[i].user =
		    remote_notify[i].ft = (char *)NULL;
}

static void
free_rnotify(int i)
{
	FREE(remote_notify[i].host);
	FREE(remote_notify[i].user);
	FREE(remote_notify[i].ft);
}

void
dump_rnotify_table(struct user *p)
{
	char buf[0x100];
	int i, l;

	write_socket(p,
	    "No Host                             Port  Ver  "
	    "Last     Free text\n"
	    "-----------------------------------------------"
	    "------------------\n");

	for (i = MAX_REMOTE_NOTIFY; i--; )
	{
		if (remote_notify[i].host == (char *)NULL)
			continue;

		if (current_time - remote_notify[i].last_contact >
		    REMOTE_NOTIFY_TIMEOUT)
		{
			free_rnotify(i);
			continue;
		}

		l = strlen(remote_notify[i].user);
		strcpy(buf, remote_notify[i].user);
		strcat(buf, "@");
		my_strncpy(buf + l + 1, ip_numtoname(remote_notify[i].host),
		    100 - l - 2);

		write_socket(p, "%-2d %-32s %-5d %#x %s %s\n",
		    i, buf,
		    remote_notify[i].port,
		    remote_notify[i].version,
		    shnctime(&remote_notify[i].last_contact),
		    remote_notify[i].ft == (char *)NULL ? "" :
		    remote_notify[i].ft);
	}
}

void
closedown_rnotify()
{
	int i;

	for (i = MAX_REMOTE_NOTIFY; i--; )
		send_a_rnotify_msg(i, "shutdown");
}

static void
rnotify_event(struct event *ev)
{
	send_udp_packet(
	    (ev->stack.sp - 2)->u.string,
	    (ev->stack.sp - 1)->u.number,
	    ev->stack.sp->u.string);
}

static void
send_rnotify_msg(char *host, int port, unsigned long id,
    unsigned long version, char *fmt, ...)
{
	char buf[BUFFER_SIZE];
	char buf2[BUFFER_SIZE];
	va_list argp;
	int i;

	va_start(argp, fmt);
	vsprintf(buf, fmt, argp);
	va_end(argp);


	sprintf(buf2, "### %s : %ld : %ld : %s\n", LOCAL_NAME, ++rnc_id, id,
	    buf);

	for (i = RNCLIENT_RETRANSMIT_COUNT; i--; )
	{
		struct event *ev;

		ev = create_event();
		push_string(&ev->stack, host);
		push_number(&ev->stack, port);
		push_string(&ev->stack, buf2);
		add_event(ev, rnotify_event,
		    (i + 1) * RNCLIENT_RETRANSMIT_DELAY, "rnotify");
	}

	send_udp_packet(host, port, buf2);
}

int
send_a_rnotify_msg(int i, char *msg)
{
	if (remote_notify[i].host == (char *)NULL)
		return 0;

	send_rnotify_msg(remote_notify[i].host, remote_notify[i].port,
	    remote_notify[i].id, remote_notify[i].version, "%s", msg);

	if (!strcmp(msg, "die"))
		free_rnotify(i);

	return 1;
}

static void
send_a_rnotify(struct user *p, char *code, int i)
{
	if (remote_notify[i].host == (char *)NULL)
		return;

	if (current_time - remote_notify[i].last_contact >
	    REMOTE_NOTIFY_TIMEOUT)
	{
		free_rnotify(i);
		return;
	}

	send_rnotify_msg(remote_notify[i].host, remote_notify[i].port,
	    remote_notify[i].id, remote_notify[i].version,
	    "%s : %s", p->name, code);
}

void
send_rnotify(struct user *p, char *code)
{
	int i;

	if (!SEE_LOGIN(p))
		return;

	for (i = MAX_REMOTE_NOTIFY; i--; )
		send_a_rnotify(p, code, i);
}

static void
handle_rnotify()
{
	char			*buf;
	int 			 i;
	int			 port, st;
	unsigned long		 version, id;
	struct user 		*p;
	char 			 un[0x400];
	char			 ft[0x400];

#define RNMSG(xx) send_rnotify_msg(notify_s->host, port, id, version, xx)

	FUN_START("handle_rnotify");

	if ((buf = scan_udpsock(notify_s)) == (char *)NULL)
	{
		FUN_END;
		return;
	}

	if (sscanf(buf, "### JL NOTIFY CLIENT %lx : %ld : %s : %d : %d",
	    &version, &id, un, &port, &st) != 5 &&
	    sscanf(buf, "### %lx : %ld : %s : %d : %d : %[^\n]",
	    &version, &id, un, &port, &st, ft) != 6)
	{
		log_file("rnotify", "Bad rnotify packet from %s: %s",
		    ip_numtoname(notify_s->host), buf);
		log_file("error", "Bad rnotify packet received from %s.",
		    ip_numtoname(notify_s->host));
		RNMSG("!Badly formatted packet received");
		RNMSG("die");
		FUN_END;
		return;
	}

	if (version < 0x15)
		*ft = '\0';

	while (isspace(*ft))
		strcpy(ft, ft + 1);

	if (*ft == '\n')
		*ft = '\0';

	if ((i = strlen(ft)) > 22)
	{
		log_file("rnotify", "Bad rnotify free text from %s: %s",
		    ip_numtoname(notify_s->host), buf);
		log_file("error", "Bad rnotify packet received from %s.",
		    ip_numtoname(notify_s->host));
		RNMSG("!Freetext field too long");
		RNMSG("die");
		FUN_END;
		return;
	}

	/* *Sigh*, the things some people will do... */
	for (buf = un; *buf != '\0'; buf++)
		if (!isalnum(*buf))
		{
			log_file("rnotify",
			    "Bad un: rnotify packet from %s: %s",
			    ip_numtoname(notify_s->host), buf);
			log_file("error",
			    "Bad un: rnotify packet received from %s.",
			    ip_numtoname(notify_s->host));
			RNMSG("!Bad packet: Please inform "
			    "JeamLand@twikki.demon.co.uk");
			RNMSG("die");
			FUN_END;
			return;
		}

	FUN_LINE;

	for (i = MAX_REMOTE_NOTIFY; i--; )
		if (remote_notify[i].host != (char *)NULL &&
		    !strcmp(remote_notify[i].host, notify_s->host) &&
		    remote_notify[i].port == port)
			break;

	/* Found this client in table. */
	if (i >= 0)
	{
		/* Client closing down */
		if (!st)
		{
#ifdef DEBUG_RNCLIENT
			notify_level_wrt_flags(L_CONSUL, EAR_RNCLIENT,
			    "[ RNClient shutdown: v.%x %s@%s <%d> ]",
			    version, un, ip_numtoname(notify_s->host), port);
#endif
			/* Client is shutting down.. */
			free_rnotify(i);
			FUN_END;
			return;
		}

		/* Has the client switched version on us ?! */
		if (version != remote_notify[i].version)
		{
			RNMSG("!Illegal protocol version change.");
			RNMSG("die");
			log_file("rnotify",
			    "Protocol version change: %s@%s : %ul -> %ul",
			    un, ip_numtoname(notify_s->host),
			    remote_notify[i].version, version);
			log_file("error",
			    "RNC: Protocol version change: %s@%s : %ul -> %ul",
			    un, ip_numtoname(notify_s->host),
			    remote_notify[i].version, version);
			free_rnotify(i);
			FUN_END;
			return;
		}

		/* Resync request */
		if (st == 2)
			free_rnotify(i);

		/* Startup ping.
		 * Check to see if this host is pinging too fast.
		 */
		if (st == 1 &&
		    current_time - remote_notify[i].last_contact < 110)
		{
			log_file("rnotify",
			    "Startup pings too frequent from %s@%s",
			    un, ip_numtoname(notify_s->host));
			send_rnotify_msg(notify_s->host, port, id, version,
			    "!Startup pings too frequent (delay: %ds).",
			    current_time - remote_notify[i].last_contact);
			RNMSG("$You are in breach of protocol.");
			RNMSG("die");
			log_file("error",
			    "RNC: Startup pings too frequent from %s@%s",
			    un, ip_numtoname(notify_s->host));
			free_rnotify(i);
			FUN_END;
			return;
		}
	}
	/* Client is shutting down but we have no record of it! */
	else if (!st)
	{
		log_file("rnotify", "Unknown client shutdown.");
		FUN_END;
		return;
	}

	FUN_LINE;

	/* If not existing client, find it a new slot. */
	if (i < 0)
		for (i = MAX_REMOTE_NOTIFY; i--; )
		{
			if (remote_notify[i].host == (char *)NULL)
				break;

			if (current_time - remote_notify[i].last_contact >
			    REMOTE_NOTIFY_TIMEOUT)
			{
				free_rnotify(i);
				break;
			}
		}
			

	/* Could not find a slot. */
	if (i < 0)
	{
		log_file("rnotify", "Client list overflow.");
		log_file("error", "RNOTIFY client list overflow.");
		RNMSG("!Too many people are currently using rnclient.");
		RNMSG("die");
		FUN_END;
		return;
	}

	remote_notify[i].port = port;
	remote_notify[i].version = version;
	remote_notify[i].last_contact = current_time;
	remote_notify[i].id = id;

	FUN_LINE;

	/* If new connection.. */
	if (remote_notify[i].host == (char *)NULL)
	{
		int j, flag;

		if (version < 0x15)
		{
			RNMSG("$You have an old version of rnclient.");
			RNMSG("$The current protocol version is 15.");
			/* Nothing we can do to help.. */
			if (version < 0x14)
			{
				FUN_END;
				return;
			}
			RNMSG("$Your client will still work.");
		}
		if (version > 0x15)
		{
			RNMSG("!Unknown protocol version.");
			RNMSG("die");
			FUN_END;
			return;
		}

		/* Make sure we have no more than two connections
		 * with this userid from this host already.. */

		FUN_LINE;

		for (flag = 0, j = MAX_REMOTE_NOTIFY; j--; )
		{
			if (remote_notify[j].host == (char *)NULL)
				continue;
			if (!strcmp(notify_s->host, remote_notify[j].host) &&
			    !strcmp(un, remote_notify[j].user) && ++flag >= 2)
				break;
		}
		if (flag >= 2)
		{
			/* Reject this connection.. */
			log_file("rnotify",
			    "More than two connections from %s@%s",
			    un, ip_numtoname(notify_s->host));
			log_file("error",
			    "RNC: More than two connections from %s@%s",
			    un, ip_numtoname(notify_s->host));
			RNMSG("!You already have two rnclient connections.");
			RNMSG("die");
			FUN_END;
			return;
		}

		FUN_LINE;
		
		remote_notify[i].host = string_copy(notify_s->host,
		    "rnotify_host");
		remote_notify[i].user = string_copy(un, "rnotify user");
		if (*ft != '\0')
			remote_notify[i].ft = string_copy(ft, "rnotify ft");
		if (st == 1)
		{
			RNMSG("$Connection accepted.");
			send_rnotify_msg(notify_s->host, port, id, version,
			    "$You are RNClient user %d of %d.",
			    MAX_REMOTE_NOTIFY - i, MAX_REMOTE_NOTIFY);
		}
		send_erq(ERQ_RESOLVE_NUMBER, "%s;\n", notify_s->host);

		FUN_LINE;

#ifdef DEBUG_RNCLIENT
		notify_level_wrt_flags(L_CONSUL, EAR_RNCLIENT,
		    "[ RNClient %s: v.%x %s@%s <%d> ]",
		    st == 1 ? "startup" : "resync",
		    version, un, ip_numtoname(notify_s->host), port);
#endif
		/* Send out the current who list */
		for (p = users->next; p != (struct user *)NULL;
		    p = p->next)
			if (IN_GAME(p))
			{
				send_a_rnotify(p, "+startup", i);
				if (p->flags & U_AFK)
					send_a_rnotify(p, "+startupafk", i);
			}
	}
	FUN_END;
}

#endif /* REMOTE_NOTIFY */

static char *
netstat_string(struct tcpsock *s)
{
	static char buf[100];

	sprintf(buf, "%3d %s %9d  %9d  %s.%d\n", s->fd, shnctime(&s->con_time),
	    s->sendq, s->recvq, ip_name(s), s->rport);

	return buf;
}

void
dump_netstat(struct user *p)
{
	struct user *ptr;
#ifdef SERVICE_PORT
	int i, flag;
#endif

	write_socket(p, " Fd Time          Sent      Recvd  "
	    "Foreign Address\n");
	write_socket(p, "----- Users -----\n");
	for (ptr = users->next; ptr != (struct user *)NULL; ptr = ptr->next)
	{
		/* *sigh*, I suppose it has to be done though.. */
		if (IN_GAME(ptr) && (ptr->saveflags & U_INVIS) &&
		    ptr->level > p->level)
			continue;
		write_socket(p, "%s", netstat_string(&ptr->socket));
	}

#ifdef SERVICE_PORT
	for (flag = 0, i = MAX_SERVICES; i--; )
		if (services[i].socket.fd != -1)
		{
			if (!flag)
			{
				write_socket(p, "----- Service Ports -----\n");
				flag = 1;
			}
			write_socket(p, "%s",
			    netstat_string(&services[i].socket));
		}
#endif

#ifdef IMUD3_SUPPORT
	if (i3_router.fd != -1)
	{
		write_socket(p, "----- Intermud - III -----\n");
		write_socket(p, "%s", netstat_string(&i3_router));
	}
#endif
	
	write_socket(p, "-----\n");
	write_socket(p, "Total bytes transmitted, TCP:  %d\n", bytes_out);
	write_socket(p, "                         UDP:  %d\n", ubytes_out);
	write_socket(p, "Total bytes received,    TCP:  %d\n", bytes_in);
	write_socket(p, "                         UDP:  %d\n", ubytes_in);
	write_socket(p, "Bytes per second transmitted:  %.2f\n", out_bps);
	write_socket(p, "Bytes per second received:     %.2f\n", in_bps);
}

void
new_user(int fd, int console)
{
	struct sockaddr_in	addr;
	struct user		*p, *q;
	struct object		*ob;
	int 			length, newfd;
	extern void get_name(struct user *, char *);
	extern void f_version(struct user *, int, char **);

	if (!console)
	{
		length = sizeof(addr);
		if (getsockname(fd, (struct sockaddr *)&addr, &length) == -1)
		{
			log_perror("getsockname");
			return;
		}
		if ((newfd = accept(fd, (struct sockaddr *)&addr, &length)) < 0)
		{
			log_perror("accept");
			return;
		}
		nonblock(newfd);
	}
	else
		newfd = fd;

#ifdef MAX_USERS
	if (!console && count_users((struct user *)NULL) >= MAX_USERS)
	{
		write(newfd, FULL_MSG, strlen(FULL_MSG));
		if (close_fd(newfd) == -1)
			log_perror("close newfd talkerfull");
		return;
	}
#endif

	/* Initialize user */
	init_user(p = create_user());
	ob = create_object();
	ob->type = OT_USER;
	ob->m.user = p;
	p->ob = ob;
	/* Get into the void ;) */
	move_object(ob, rooms->ob);

	/* Add user to END of list. */
	for (q = users; q->next != (struct user *)NULL; q = q->next)
		;
	p->next = (struct user *)NULL;
	q->next = p;
	num_users++;

	p->socket.fd = newfd;
	p->socket.con_time = current_time;

	p->input_to = get_name;
	p->login_time = current_time;
	p->level = L_VISITOR;
	p->flags |= U_LOGGING_IN;

	/* Be on the safe side.. */
	logon_name(p);

	if (!console)
	{
		memcpy((char *)&(p->socket.addr), (char *)&addr, length);
		p->socket.lport = port;
		p->socket.rport = ntohs(addr.sin_port);
		send_erq(ERQ_RESOLVE_NUMBER, "%s;\n",
		    inet_ntoa(addr.sin_addr));
		send_erq(ERQ_IDENT, "%s;%d,%d\n", inet_ntoa(addr.sin_addr),
		    p->socket.lport, p->socket.rport);
	}
	else
	{
		extern char *runas_username;
		memset((char *)&(p->socket.addr), '\0',
		    sizeof(p->socket.addr));
		if (runas_username != (char *)NULL)
		{
			COPY(p->uname, runas_username, "uname");
		}
		p->flags |= U_CONSOLE;
		console_used = 1;
	}

#ifdef LOG_CONNECTIONS
	log_file("secure/connect", "%s", lookup_ip_name(p));
#endif
	notify_level_wrt_flags(L_OVERSEER, EAR_TCP,
	    "[ *TCP* Connect: %s (%s) ]",
	    lookup_ip_number(p), lookup_ip_name(p));

	dump_file(p, "etc", F_WELCOME, DUMP_CAT);
	f_version(p, 0, (char **)NULL);

	/* Required for some terminals, does no harm to others */
	echo(p);

	add_event(create_event(), kick_logons, LOGIN_TIMEOUT + 1, "login");
	get_name(p, "");
}

void
replace_interactive(struct user *p, struct user *q)
{
	struct tcpsock s;
	unsigned long tmp;

	FUN_START("replace_interactive");
	FUN_ARG(p->rlname);

	/* Switch the socket structs around.. */
	memcpy((char *)&s, (char *)&p->socket, sizeof(struct tcpsock));
	memcpy((char *)&p->socket, (char *)&q->socket, sizeof(struct tcpsock));
	memcpy((char *)&q->socket, (char *)&s, sizeof(struct tcpsock));

	tmp = p->flags;
	p->flags ^= q->flags & U_CONSOLE;
	q->flags ^= tmp & U_CONSOLE;

	FREE(p->uname);
	p->uname = q->uname;
	q->uname = (char *)NULL;

	disconnect_user(q, 1);

	FUN_END;
}

#define ERQ_STACKSIZE	15
static struct {
	int id;
	enum { ERQ_USER, ERQ_FUNC } type;
	char *user;
	void (*func)();
	} pending_erqs[ERQ_STACKSIZE];

static int
find_pending_erq(int id)
{
	int i;

	for (i = ERQ_STACKSIZE; i--; )
		if (pending_erqs[i].id == id)
			return i;
	return -1;
}

void
init_erq_table()
{
	int i;

	for (i = ERQ_STACKSIZE; i--; )
		pending_erqs[i].id = -1;
}

static int
do_send_erq(int request, char *fmt, va_list argp)
{
	static int id = 0;
	
	if (erqw != (FILE *)NULL)
	{
		fprintf(erqw, "%d,%d:", request, ++id);
		vfprintf(erqw, fmt, argp);
		fflush(erqw);
		return id;
	}
	return -1;
}

int
send_erq(int request, char *fmt, ...)
{
	int ret;
	va_list argp;

	va_start(argp, fmt);
	ret = do_send_erq(request, fmt, argp);
	va_end(argp);
	return ret;
}

int
send_user_erq(char *uname, int request, char *fmt, ...)
{
	int id, i;
	va_list argp;

	va_start(argp, fmt);
	id = do_send_erq(request, fmt, argp);
	va_end(argp);
	if (id == -1)
		return -1;

	/* Find a free slot.. */
	if ((i = find_pending_erq(-1)) == -1)
		return id;
	pending_erqs[i].id = id;
	pending_erqs[i].type = ERQ_USER;
	pending_erqs[i].user = string_copy(uname, "erq_table uname");

	return id;
}

static void
erq_reply()
{
	char buf[BUFFER_SIZE];
	int response, id, i;

	/* Use fgets to ensure only one line retrieved at a time. */
	if (fgets(buf, sizeof(buf), erqp) == (char *)NULL)
		return;

	if (sscanf(buf, "%d,%d:", &response, &id) != 2)
	{
		log_file("syslog", "Bad Erqd packet: %s", buf);
		notify_level(L_CONSUL, "Bad Erqd packet: %s", buf);
		return;
	}

	/* See if anyone wanted this packet.. */
	if ((i = find_pending_erq(id)) != -1)
	{
		switch (pending_erqs[i].type)
		{
		    case ERQ_USER:
		    {
			struct user *p;

			if ((p = find_user_absolute((struct user *)NULL,
			    pending_erqs[i].user)) != (struct user *)NULL)
			{
				attr_colour(p, "notify");
				write_socket(p, "Erqd response: %s", buf);
				reset(p);
				write_socket(p, "\n");
			}
			xfree(pending_erqs[i].user);
			break;
		    }
		    case ERQ_FUNC:
			pending_erqs[i].func(buf);
			break;
		    default:
			fatal("Bad pending_erqs type, %d",
			    pending_erqs[i].type);
		}
		pending_erqs[i].id = -1;
	}

	/* Pass through, for now */

	switch (response)
	{
	    case ERQ_ERROR:
	    {
		char error[BUFFER_SIZE];
		if (sscanf(buf, "%*d,%*d:%[^\n]", error))
		{
			log_file("syslog", "Erqd error: %s", error);
#ifdef DEBUG
			notify_level(L_CONSUL, "Erqd error: %s", error);
#endif
		}
		else
		{
			log_file("syslog", "Erqd error: %s", buf);
#ifdef DEBUG
			notify_level(L_CONSUL, "Erqd error: %s", buf);
#endif
		}
		break;
	    }

	    case ERQ_IDENTIFY:
	    {
		struct jlm *j;
		char *p;

		if ((p = strchr(buf, ':')) != (char *)NULL &&
		    *++p != '\0' &&
		    (j = find_jlm("erqd")) != (struct jlm *)NULL)
		{
			char *q;

			if ((q = strchr(p, '\n')) != (char *)NULL)
				*q = '\0';
			COPY(j->ident, p, "erqd ident");
		}
		break;
	    }

	    case ERQ_RESOLVE_NUMBER:
	    case ERQ_RESOLVE_NAME:
	    {
		char ipname[MAX_INET_ADDR], ipnum[MAX_INET_ADDR];

		if (sscanf(buf, "%*d,%*d:%[^ ] %[^;];", ipnum, ipname) != 2)
			log_file("syslog", 
			    "Erqd: Illegal RESOLVE response [%s]\n", buf);
		else
			add_ip_entry(ipnum, ipname);
		break;
	    }

	    case ERQ_IDENT:
	    {
		char ipnum[MAX_INET_ADDR], username[MAX_INET_ADDR];
		int rport, lport;

		if (sscanf(buf, "%*d,%*d:%[^;];%d,%d;%[^;]s;", ipnum, &lport,
		    &rport, username) != 4 || !strlen(username))
			break;
		add_username(lport, rport, ipnum, username);
		break;
	    }

#ifdef RUSAGE
	    case ERQ_RUSAGE:
	    {
		extern void erqd_rusage_reply(char *);
		erqd_rusage_reply(buf);
		break;
	    }
#endif /* RUSAGE */

	    case ERQ_FAILED_RESOLVE:
		break;

	    case ERQ_EMAIL:
	    {
		char name[NAME_LEN + 1];
		int success;

		if (sscanf(buf, "%*d,%*d:%[^;];%d;", name, &success) != 2)
			log_file("syslog",
			    "Erqd: Illegal email response [%s]", buf);
		else
		{
			struct user *who;

			if ((who = find_user_absolute((struct user *)NULL,
			    name)) != (struct user *)NULL)
			{
				attr_colour(who, "notify");
				if (success)
					write_socket(who,
					    "\nYour email request has been "
					    "successfully processed%s.",
					    who->level >= L_WARDEN ?
					    " by erqd" : "");
				else
					write_socket(who,
					    "\nYour email request has been "
					    "rejected%s.", who->level >=
					    L_WARDEN ? " by erqd (see syslog "
					    "for details)" : "");
				reset(who);
				write_socket(who, "\n");
			}
		}

		break;
	    }

	    default:
		log_file("error", "Unrecognised erqd response [%d]",
		    response);
	}
}

static int
flush_tcpsock(struct tcpsock *s)
{
	int length, len;

	FUN_START("flush_tcpsock");

#ifdef DEBUG
	if (!(s->flags & TCPSOCK_BLOCKED))
		fatal("flush non-blocked tcpsock.");
#endif

	/* No of characters which need writing.. */
	len = s->output_buffer.offset - s->ob_offset;

	length = write(s->fd, s->output_buffer.str + s->ob_offset, len);

	if (length == -1)
	{
		log_perror("flush_tcpsock");
		FUN_END;
		return -1;
	}

	log_file("tcp", "Tcpsock flushed: %d/%d", length, len);

	if (length == len)
	{
		/* Socket is clear */
		s->flags &= ~TCPSOCK_BLOCKED;

		free_strbuf(&s->output_buffer);

		FUN_END;
		return 0;
	}

	/* Increment offset to point to the rest of the string. */
	s->ob_offset += length;

	return len - length;

	FUN_END;
}

int
write_tcpsock(struct tcpsock *s, char *buf, int len)
{
	int length;

	FUN_START("write_tcpsock");

#ifdef DEBUG
	if ((s->flags & TCPSOCK_BINARY) && len == -1)
		fatal("No length supplied for binary tcpsock write.");
#endif

	if (len == -1)
		len = strlen(buf);

/* TEMP DEBUG */
	if (!(s->flags & TCPSOCK_BINARY) && len != strlen(buf))
	{
		log_file("error", "Bad length to write_tcpsock: %s", buf);
		log_file("error",
		    "Please report to jeamland@twikki.demon.co.uk");
		len = strlen(buf);
	}
/* END TEMP DEBUG */

	if (s->flags & TCPSOCK_BLOCKED)
	{
		/* Socket is already blocked.. append this text unless
		 * it is too long. */

		if (s->output_buffer.offset + len > TCPSOCK_MAXOB)
		{
			/* It's really too big, give up. */
			free_strbuf(&s->output_buffer);
			s->flags &= ~TCPSOCK_BLOCKED;

			/* Fiddle.. */
			errno = EWOULDBLOCK;
			FUN_END;
			return -1;
		}
		if (s->flags & TCPSOCK_BINARY)
			/* Got to do this the (relatively) long way
			 * as the packet could contain null bytes.
			 */
			binary_add_strbuf(&s->output_buffer, buf, len);
		else
			add_strbuf(&s->output_buffer, buf);
		
		FUN_END;

		/* Hmm.. */
		return 1;
	}

	length = write(s->fd, buf, len);
	if (length == -1)
	{
		FUN_END;
		return -1;
	}

	bytes_outt += length;
	s->sendq += length;

	if (length != len)
	{
		/* Socket blocked.. buffer the remaining output until the
		 * socket clears.. */

		s->flags |= TCPSOCK_BLOCKED;

		log_file("tcp", "write_tcpsock blocked: %d/%d", length, len);

		init_strbuf(&s->output_buffer, len - length, "tcpsock ob");
		if (s->flags & TCPSOCK_BINARY)
			binary_add_strbuf(&s->output_buffer, buf + length,
			    len - length);
		else
			add_strbuf(&s->output_buffer, buf + length);
		s->ob_offset = 0;
	}
		
	FUN_END;

	/* And return the number of characters buffered. */
	return len - length;
}

int
scan_tcpsock(struct tcpsock *s)
{
	int cnt;

	FUN_START("scan_tcpsock");

	if (s->flags & TCPSOCK_BINARY)
	{
		/* Just read directly into input_buffer assuming that
		 * ib_size is the amount of data we really want to obtain.
		 */
		if ((cnt = read(s->fd, s->input_buffer + s->ib_offset,
		    s->ib_size - s->ib_offset)) <= 0)
		{
			/* Error.. */
			FUN_END;
			return cnt;
		}
	}
	else
	{
		char *buf;
		char *q, *r;
		int actual;
		int spec, tel;
		int nls;

		actual = (s->ib_size - s->ib_offset - 1);

		if (actual < 3)
		{
			/* Drastic measures... dump the packet */
			log_file("error",
			    "Tcpsock input: received %d characters w/out EOL.",
			    s->ib_size - actual);

			s->ib_offset = s->lines = 0;
			FUN_END;
			return 0;
		}

		/* Don't flood the buffer */
		actual /= 3;

		buf = (char *)xalloc(actual + 1, "scan_tcpsock buf");

		if ((cnt = read(s->fd, buf, actual)) <= 0)
		{
			xfree(buf);
			FUN_END;
			return cnt;
		}

		buf[cnt] = '\0';

		q = s->input_buffer + s->ib_offset;
		cnt = nls = 0;

		/* Handle special characters ? */
		spec = s->flags & TCPSOCK_SPECIAL;
		tel = s->flags & TCPSOCK_TELNET;

		for (r = buf; *r != '\0'; r++)
		{
			if (tel && handle_telnet(s, *r))
				continue;
			switch (*r)
			{
			    case '\b':
			    case 0x7f:
				if (q > s->input_buffer && q[-1] != '\0')
					q--, cnt--;
				break;

			    /* Parse ^U as erase line */
			    case 0x15:
				while (q > s->input_buffer && q[-1] != '\0')
					q--, cnt--;
				break;

			    /* Parse ^D as disconnect
			     * do some clients sent an IAC sequence for this?
			     */
			    case 0x04:
				s->flags |= TCPSOCK_SHUTDOWN;
				break;

#ifdef EMBEDDED_NEWLINES
			    case '\\':
				if (spec && r[1] == '\r')
				{
					r += 2;
					*q++ = '\n';
					*q++ = '\t';
					cnt += 2;
				}
				else
				{
					*q = *r;
					q++, cnt++;
				}
				break;
#endif
			    case '\r':
			    case '\n':
				if (++nls > 1 ||
				    *(r + 1) != (*r == '\r' ? '\n' : '\r'))
				{
					*q = '\0';
					q++, cnt++;
					nls = 0;
					s->lines++;
				}
				break;
			    default:
				*q = *r;
				q++, cnt++;
				break;
			}
		}
		xfree(buf);
	}

	s->ib_offset += cnt;
	/* This isn't EXACTLY correct if we just received a backspace from
	 * a character mode client, but the CPU time involved in an abs()
	 * call is just not worth it */
	bytes_int += cnt;
	s->recvq += cnt;

	FUN_END;
	return 1;
}

char *
process_tcpsock(struct tcpsock *s)
{
	char *buf;
	char *r;

	FUN_START("process_tcpsock");

	if (s->fd == -1)
	{
		log_file("error",
		    "process_tcpsock: s->fd == -1, backtrace dumped");
#ifdef CRASH_TRACE
		backtrace("process_tcpsock s->fd == -1", 0);
#endif
		FUN_END;
		return (char *)NULL;
	}

	if (s->flags & TCPSOCK_BINARY)
	{
		FUN_END;
		/* This is the easy case.. */
		if (s->ib_offset == s->ib_size)
			return s->input_buffer;
		return (char *)NULL;
	}

	/* Quick return if no lines are waiting */
	if (!s->lines)
	{
		FUN_END;
		return (char *)NULL;
	}

	for (r = s->input_buffer; r - s->input_buffer < s->ib_offset; r++)
		if (*r == '\0') /* Got a complete string! */
		{
			buf = string_copy(s->input_buffer, "process_tcpsock");

			FUN_LINE;

			if (s->ib_offset - 1 > r - s->input_buffer)
			{
				memmove(s->input_buffer, r + 1,
				    s->ib_offset - (r - s->input_buffer) - 1);
				s->ib_offset -= r - s->input_buffer + 1;
			}
			else
				s->ib_offset = 0;

			s->lines--;
			FUN_END;
			return buf;
		}

	fatal("parse_tcpsock: Waiting command not found.");

	FUN_END;
	return (char *)NULL;
}

char *
scan_udpsock(struct udpsock *s)
{
	struct sockaddr_in addr;
	int length, cnt;

	FUN_START("scan_udpsock");

	length = sizeof(addr);

	if ((cnt = recvfrom(s->fd, s->input_buffer, s->ib_size,
	    0, (struct sockaddr *)&addr, &length)) <= 0)
	{
		if (cnt == -1)
			log_perror("Scan udpsock");
		FUN_END;
		return (char *)NULL;
	}

	s->input_buffer[cnt] = '\0';

	s->host = inet_ntoa(addr.sin_addr);

	ubytes_int += cnt;

	FUN_END;

	return s->input_buffer;
}

int
insert_command(struct tcpsock *s, char *c)
{
	int len = strlen(c);
	char *q = s->input_buffer + s->ib_offset;
	char *r;

	if (s->ib_offset + len >= s->ib_size)
		return 0;
	for (r = c; *r != '\0'; r++)
		*q++ = *r;
	s->ib_offset += len;
	s->input_buffer[s->ib_offset++] = '\0';
	s->input_buffer[s->ib_offset] = '\0';
	s->lines++;
	return 1;
}

void
scan_socket(struct user *p)
{
	FUN_START("scan_socket");

	switch (scan_tcpsock(&p->socket))
	{
	    case -1:
		notify_levelabu_wrt_flags(p, SEE_LOGIN(p) ? L_CONSUL :
		    L_OVERSEER, EAR_TCP,
		    "[ *TCP* Read error: %s [%s] (%s) %s ]",
		    p->capname, capfirst(level_name(p->level, 0)),
		    lookup_ip_name(p), perror_text());
		log_perror("socket read");
		save_user(p, 1, 0);
		disconnect_user(p, 1);
		FUN_END;
		return;

	    case 0:
		notify_levelabu_wrt_flags(p, SEE_LOGIN(p) ? L_CONSUL :
		    L_OVERSEER, EAR_TCP,
		    "[ *TCP* Read error: %s [%s] (%s) Remote Flush ]",
		    p->capname, capfirst(level_name(p->level, 0)),
		    lookup_ip_name(p));
		save_user(p, 1, 0);
		disconnect_user(p, 1);
		FUN_END;
		return;
	}

	p->last_command = current_time;

	if (p->socket.flags & TCPSOCK_SHUTDOWN)
	{
		/* User pressed ^D.. call quit */
		extern void f_quit(struct user *, int, char **);
		char *argv[1];

		write_socket(p, "\n");
		argv[0] = "quit";
		f_quit(p, 1, argv);

		/* Just in case the quit fails.. */
		p->socket.flags &= ~TCPSOCK_SHUTDOWN;
	}

	FUN_END;
}

int
process_input(struct user *p)
{
	char *buf, *buf2;
	char *r, *s;
	extern void f_say(struct user *, int, char **);

	if (p->flags & U_SOCK_CLOSING)
		return 0;

	FUN_START("process_input");
	FUN_ARG(p->rlname);

	if ((buf2 = process_tcpsock(&p->socket)) == (char *)NULL)
	{
		FUN_END;
		return 0;
	}

	/* Remove unprintable characters. */
	buf = (char *)xalloc(strlen(buf2) + 1, "process input");
	for (r = buf2, s = buf; *r != '\0'; r++)
		if (isprint(*r) || isspace(*r) || *r == '\n')
			*s++ = *r;
	*s = '\0';

	xfree(buf2);

	if (p->snooped_by != (struct user *)NULL && !(p->flags & U_NO_ECHO))
		write_socket(p->snooped_by, "%%%s\n", buf);

#ifdef SUPER_SNOOP
	if (p->snoop_fd != -1 && !(p->flags & U_NO_ECHO))
	{
		write(p->snoop_fd, buf, strlen(buf));
		write(p->snoop_fd, "\n", 1);
	}
#endif

	if (!(p->flags & U_NO_ECHO))
		p->col = 0;

	/* Support for daft terminals (eg. Novell TNVT220) */
	if (p->saveflags & U_EXTRA_NL)
		fwrite_socket(p, "\n");

	if (p->input_to != NULL_INPUT_TO)
	{
		/* Just for Mr Persson.. */
		if (*buf == '!' && IN_GAME(p) && !(p->flags & U_NOESCAPE))
		{
			long oflags = p->flags & U_UNBUFFERED_TEXT;
			strcpy(buf, buf + 1);
			p->flags |= U_UNBUFFERED_TEXT;
			parse_command(p, &buf);
			if (!oflags)
				p->flags &= ~U_UNBUFFERED_TEXT;
		}
		else
		{
			extern void get_password(struct user *, char *);

			FUN_START("input_to");
			/* Don't want to record passwords in the backtrace! */
			if (p->input_to == get_password)
				FUN_ARG("<PASSWORD>");
			else
				FUN_ARG(buf);
			FUN_ADDR(p->input_to);
			p->input_to(p, buf);
			FUN_END;
		}
		if (p->input_to == NULL_INPUT_TO)
			print_prompt(p);
		xfree(buf);
		FUN_END;
		return 1;
	}

	if (!strlen(buf))
	{
		print_prompt(p);
		xfree(buf);
		FUN_END;
		return 1;
	}

#ifdef DEBUG
	if (!STACK_EMPTY(&p->stack))
		fatal("Bad user stack after evaluation.");
	if (!STACK_EMPTY(&p->atexit))
		fatal("Bad atexit stack after evaluation.");
	if (p->flags & U_SUDO)
		fatal("Sudo set in process_input().");
	if (p->flags & U_INHIBIT_QUIT)
		fatal("Inhibit quit set in process_input().");
#endif

	reset_eval_depth();

	/* Compatility mode for talkers such as NUTS */
	if (p->saveflags & U_CONVERSE_MODE)
	{
		switch (*buf)
		{
		    case '.':
			strcpy(buf, buf + 1);
			/* Fall through */
		    case ':':
		    case ';':
		    case '>':
		    case '<':
		    case '=':
			parse_command(p, &buf);
			break;
		    default:
		    {
			static char *argv[] = { "say", "x" };

			/* Copy history here as f_say is being called
			  * directly */
			COPY(p->history->items[p->history_ptr % HISTORY_SIZE].
			    u.string, buf, "*history");
			p->history_ptr++;

			argv[1] = buf;
			f_say(p, 2, argv);

		    }
		}
	}
	else
		parse_command(p, &buf);

	if (p->input_to == NULL_INPUT_TO)
		print_prompt(p);
	xfree(buf);
	FUN_END;
	return 1;
}

void
disconnect_user(struct user *p, int lost)
{
	if (p->name != (char *)NULL && IN_GAME(p))
	{
		if (lost)
		{
			notify_levelabu(p, SEE_LOGIN(p) ? L_VISITOR :
			    p->level, "[ %s [%s] has lost connection. ]", 
			    p->name, capfirst(level_name(p->level, 0)));

			if (!(p->saveflags & U_INVIS))
				write_roomabu(p, "%s has lost connection.\n",
				    p->name);
#ifdef REMOTE_NOTIFY
			send_rnotify(p, "+lostconn");
#endif
		}
		else
		{
			notify_levelabu(p, SEE_LOGIN(p) ? L_VISITOR :
			    p->level, "[ %s [%s] has disconnected. ]",
			    p->capname, capfirst(level_name(p->level, 0)));
			write_roomabu(p, "%s has disconnected.\n", p->name);
#ifdef REMOTE_NOTIFY
			send_rnotify(p, "-login");
#endif
		}
	}
	p->flags |= U_SOCK_CLOSING;
}

void
process_sockets()
{
	fd_set readfds, writefds, exfds;
	struct user *p, *next_p;
	struct jlm *j;
	struct timeval delay;
	int ret;
#ifdef SERVICE_PORT
	int i;
#endif

	FUN_START("process_sockets");

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&exfds);

	/* Main login port */
	if (sockfd != -1)
	{
		FD_SET(sockfd, &readfds);
	}

	/*  Console */
	if (!console_used)
	{
		FD_SET(fileno(stdin), &readfds);
	}

	/* Erqd */
	if (erqd_fd != -1)
	{
		FD_SET(erqd_fd, &readfds);
	}

	/* Incoming UDP */
#ifdef INETD_SUPPORT
	if (udp_s != (struct udpsock *)NULL)
	{
		FD_SET(udp_s->fd, &readfds);
	}
#endif /* INETD_SUPPORT */
#ifdef CDUDP_SUPPORT
	if (cdudp_s != (struct udpsock *)NULL)
	{
		FD_SET(cdudp_s->fd, &readfds);
	}
#endif /* CDUDP_SUPPORT */

	/* Incoming Intermud-III */
#ifdef IMUD3_SUPPORT
	if (i3_router.fd != -1)
	{
		FD_SET(i3_router.fd, &readfds);
		if (i3_router.flags & TCPSOCK_BLOCKED)
			FD_SET(i3_router.fd, &writefds);
	}
#endif /* IMUD3_SUPPORT */

	/* Service port */
#ifdef SERVICE_PORT
	if (service_s != -1)
	{
		FD_SET(service_s, &readfds);
		for (i = MAX_SERVICES; i--; )
			if (services[i].socket.fd != -1)
			{
				if (!(services[i].flags & SERVICE_CLOSING))
					FD_SET(services[i].socket.fd,
					    &readfds);
				if (services[i].socket.flags & TCPSOCK_BLOCKED)
					FD_SET(services[i].socket.fd,
					    &writefds);
			}
	}
#endif /* SERVICE_PORT */

#ifdef REMOTE_NOTIFY
	/* Remote RNClient packets */
	if (notify_s != (struct udpsock *)NULL)
	{
		FD_SET(notify_s->fd, &readfds);
	}
#endif /* REMOTE_NOTIFY */

	/* JLM's */
	cleanup_jlms();
	for (j = jlms; j != (struct jlm *)NULL; j = j->next)
		if (j->ob != (struct object *)NULL)
		{
			FD_SET(j->infd, &readfds);
		}

	/* Finally get around to the users ;-) */
	for (p = users->next; p != (struct user *)NULL; p = p->next)
	{
		FD_SET(p->socket.fd, &readfds);
		FD_SET(p->socket.fd, &exfds);
		if (p->socket.flags & TCPSOCK_BLOCKED)
			FD_SET(p->socket.fd, &writefds);
	}

	FUN_LINE;

	delay.tv_usec = 10;
	/* If there is an event pending, don't sleep too long in the select. */
	if (sysflags & SYS_EVENT)
		delay.tv_sec = 0;
	else
		delay.tv_sec = 1200;
	ret = select(FD_SETSIZE, FD_CAST &readfds, FD_CAST &writefds,
	    FD_CAST &exfds, &delay);
	if (ret < 0)
	{
		if (errno != EINTR)
			log_perror("select poll");
		FUN_END;
		return;
	}

	delay.tv_sec = 0;
	delay.tv_usec = 10;
	if (select(0, FD_CAST NULL, FD_CAST NULL, FD_CAST NULL,
	    (struct timeval *)&delay) < 0 && errno != EINTR)
	{
		log_perror("select sleep");
		fatal("Couldn't sleep.");
	}

	update_current_time();

	if (!ret)
	{
		FUN_END;
		return;
	}

	FUN_LINE;

	/* Check for console activity */
	if (!console_used && FD_ISSET(fileno(stdin), &readfds))
		new_user(fileno(stdin), 1);

	/* Check for a reply from erqd */
	if (erqd_fd != -1 && FD_ISSET(erqd_fd, &readfds))
		erq_reply();

	FUN_LINE;

#ifdef SERVICE_PORT
	/* Check for service connection. */
	if (service_s != -1)
	{
		if (FD_ISSET(service_s, &readfds))
			new_service(service_s);

		for (i = MAX_SERVICES; i--; )
			if (services[i].socket.fd != -1)
			{
				if (FD_ISSET(services[i].socket.fd, &readfds))
					handle_service(i);

				if (services[i].socket.fd != -1 &&
				    (services[i].socket.flags &
				    TCPSOCK_BLOCKED) &&
				    FD_ISSET(services[i].socket.fd, &writefds))
				{
					/* Left as two if statements for
					 * readability! */
					if (flush_tcpsock(&services[i].socket)
					    == -1 ||
					    !(services[i].socket.flags &
					    TCPSOCK_BLOCKED))
						close_service(i);
				}
			}
	}
#endif

	FUN_LINE;

	/* Check for RNClient packets */
#ifdef REMOTE_NOTIFY
	if (notify_s != (struct udpsock *)NULL &&
	    FD_ISSET(notify_s->fd, &readfds))
		handle_rnotify();
#endif

	FUN_LINE;

#ifdef INETD_SUPPORT
	if (udp_s != (struct udpsock *)NULL &&
	    FD_ISSET(udp_s->fd, &readfds))
		incoming_udp();
#endif
#ifdef CDUDP_SUPPORT
	if (cdudp_s != (struct udpsock *)NULL &&
	    FD_ISSET(cdudp_s->fd, &readfds))
		incoming_cdudp();
#endif

	FUN_LINE;

	/* Check for jlm input */
	cleanup_jlms();
	for (j = jlms; j != (struct jlm *)NULL; j = j->next)
		if (j->ob != (struct object *)NULL &&
		    FD_ISSET(j->infd, &readfds))
			jlm_reply(j);

	FUN_LINE;

	/* Handle users */
	for (p = users->next; p != (struct user *)NULL; p = next_p)
	{
		next_p = p->next;

		if (p->flags & U_SOCK_CLOSING)
		{
			close_socket(p);
			continue;
		}

		if (FD_ISSET(p->socket.fd, &exfds))
		{
			FD_CLR(p->socket.fd, &readfds);
			FD_CLR(p->socket.fd, &writefds);

			disconnect_user(p, 1);
			notify_levelabu_wrt_flags(p,
			    SEE_LOGIN(p) ? L_CONSUL : L_OVERSEER, EAR_TCP,
			    "[ *TCP* Socket exception: %s (%s) ]",
			    p->name, lookup_ip_name(p));

			close_socket(p);
			continue;
		}

		FUN_LINE;

		if (FD_ISSET(p->socket.fd, &readfds))
			scan_socket(p);

		FUN_LINE;

		/* process_input checks U_SOCK_CLOSING */
		while (process_input(p))
			;

		FUN_LINE;

		if (!(p->flags & U_SOCK_CLOSING) &&
		    (p->socket.flags & TCPSOCK_BLOCKED) &&
		    FD_ISSET(p->socket.fd, &writefds))
			if (flush_tcpsock(&p->socket) == -1)
			{
				disconnect_user(p, 1);
				notify_levelabu_wrt_flags(p, L_OVERSEER,
				    EAR_TCP,
				    "[ *TCP* Flush error: %s [%s] (%s) %s ]",
				    p->capname,
				    capfirst(level_name(p->level, 0)),
				    lookup_ip_name(p), perror_text());
			}

		if (p->flags & U_SOCK_CLOSING)
			close_socket(p);
	}

	FUN_LINE;

	/* Check for new connection.
	 * After the user processing loops deliberately */
	if (sockfd != -1 && FD_ISSET(sockfd, &readfds))
		new_user(sockfd, 0);

	FUN_LINE;

#ifdef IMUD3_SUPPORT
	/* Check for imud3 router input */
	if (i3_router.fd != -1)
	{
		if (FD_ISSET(i3_router.fd, &readfds))
			i3_incoming(&i3_router);

		if ((i3_router.flags & TCPSOCK_BLOCKED) &&
		    FD_ISSET(i3_router.fd, &writefds))
			if (flush_tcpsock(&i3_router) == -1)
				i3_lostconn(&i3_router);
	}
#endif /* IMUD3_SUPPORT */

	FUN_END;
}

static void
execute_atexit(struct user *p)
{
	while (p->atexit.sp > p->atexit.stack)
	{
		void (*func)(struct user *);

#ifdef DEBUG
		if (p->atexit.sp->type != T_FPOINTER)
			fatal("Executing non fpointer atexit function.");
#endif
		func = p->atexit.sp->u.fpointer;
		dec_stack(&p->atexit);
		func(p);
	}
}

int
close_fd(int fd)
{
	int e;

	if (fd == -1)
		return 0;

	shutdown(fd, 2);
	while ((e = close(fd) == -1) && errno == EINTR)
		;
	return e;
}

void
close_tcpsock(struct tcpsock *s)
{
	if (close_fd(s->fd) == -1)
		log_perror("close tcpsock");
	s->fd = -1;
}

void
close_socket(struct user *p)
{
	struct user *ptr;
	struct object *ob;
	char fname[MAXPATHLEN + 1];

	FUN_START("close_socket");
	FUN_ARG(p->rlname);

#ifdef DEBUG
	if (!(p->flags & U_SOCK_CLOSING))
		fatal("close_socket: Socket not closing (%s)", p->rlname);
#endif

	if (p->snooped_by != (struct user *)NULL)
	{
		write_socket(p->snooped_by, 
		    "%s has disconnected - snoop terminated.\n", p->name);
		p->snooped_by->snooping = (struct user *)NULL;
	}
	if (p->snooping != (struct user *)NULL)
		p->snooping->snooped_by = (struct user *)NULL;

	if (IN_GAME(p))
		lastlog(p);

	/* Unlink their sticky note file. */
	sprintf(fname, "mail/%s#sticky", p->rlname);
	unlink(fname);

	FUN_LINE;

	/* Handle atexit functions */
	execute_atexit(p);

	FUN_LINE;

	/* Handle the user's inventory */
	for (ob = p->ob->contains; ob != (struct object *)NULL;
	    ob = ob->next_contains)
	{
		if (ob->type == OT_JLM)
		{
			/* Remove from env, to be safe. */
			move_object(ob, rooms->ob);
			kill_jlm(ob->m.jlm);
		}
		else
			fatal("None jlm object inside user.");
	}

	FUN_LINE;

	/* Remove user from global user list */
	for (ptr = users; ptr != (struct user *)NULL; ptr = ptr->next)
	{
		if (ptr->next == p)
		{
			ptr->next = ptr->next->next;
			/* Make sure the close is not interrupted by a
			 * heartbeat */
			if (p->flags & U_CONSOLE)
			{
				console_used = 0;
				p->socket.fd = -1;
			}
			else
				close_tcpsock(&p->socket);
			close_fd(p->snoop_fd);
			free_object(p->ob);
			num_users--;
			FUN_END;
			return;
		}
	}
	fatal("Close socket on non-existant user.");
}

void
noecho(struct user *p)
{
        p->flags |= U_NO_ECHO;
	if (p->flags & U_CONSOLE)
	{
		struct termios term;

		if (tcgetattr(p->socket.fd, &term) == -1)
		{
			log_perror("tcgetattr noecho");
			return;
		}
		term.c_lflag &= ~ECHO;
		if (tcsetattr(p->socket.fd, TCSANOW, &term) == -1)
			log_perror("tcsetattr noecho");
	}
	else
	{
        	write_socket(p, "%c%c%c", IAC, WILL, TELOPT_ECHO);
#ifdef DEBUG_TELNET
		log_file("telnet", "-> IAC WILL TELOPT_ECHO");
#endif
		p->socket.telnet.expect |= TN_EXPECT_ECHO;
	}
}

void
echo(struct user *p)
{
        p->flags &= ~U_NO_ECHO;
	if (p->flags & U_CONSOLE)
	{
		struct termios term;

		if (tcgetattr(p->socket.fd, &term) == -1)
		{
			log_perror("tcgetattr echo");
			return;
		}
		term.c_lflag |= ECHO;
		if (tcsetattr(p->socket.fd, TCSANOW, &term) == -1)
			log_perror("tcsetattr echo");
	}
	else
	{
        	write_socket(p, "%c%c%c", IAC, WONT, TELOPT_ECHO);
#ifdef DEBUG_TELNET
		log_file("telnet", "-> IAC WONT TELOPT_ECHO");
#endif
		p->socket.telnet.expect |= TN_EXPECT_ECHO;
	}
}

static int
handle_telnet(struct tcpsock *s, char ch)
{
	char iac[4];

	switch (s->telnet.state)
	{
	    case TS_IDLE:
		if (ch != (char)IAC)
			return 0;
		s->telnet.state = TS_IAC;
		return 1;

	    case TS_IAC:
#ifdef DEBUG_TELNET
		if (TELCMD_OK(ch))
			log_file("telnet", "<- IAC %s", TELCMD(ch));
#endif
		switch (ch)
		{
		    case (char)IAC:	/* Is this protocol ? */
			return 1;
		    case (char)AYT:
			write_tcpsock(s, "\n[Yes]\n", 7);
			s->telnet.state = TS_IDLE;
			return 1;
		    case (char)WILL:
			s->telnet.state = TS_WILL;
			return 1;
		    case (char)WONT:
			s->telnet.state = TS_WONT;
			return 1;
		    case (char)DO:
			s->telnet.state = TS_DO;
			return 1;
		    case (char)DONT:
			s->telnet.state = TS_DONT;
			return 1;
		    default:
			s->telnet.state = TS_IDLE;
			return 0;
		}
		/* NOTREACHED */

	    case TS_WILL:
#ifdef DEBUG_TELNET
		if (TELOPT_OK(ch))
			log_file("telnet", "<- IAC WILL %s", TELOPT(ch));
#endif
		sprintf(iac, "%c%c%c", IAC, DONT, ch);
		write_tcpsock(s, iac, 3);
		s->telnet.state = TS_IDLE;
		return 1;

	    case TS_WONT:
#ifdef DEBUG_TELNET
		if (TELOPT_OK(ch))
			log_file("telnet", "<- IAC WONT %s", TELOPT(ch));
#endif
		sprintf(iac, "%c%c%c", IAC, DONT, ch);
		write_tcpsock(s, iac, 3);
		s->telnet.state = TS_IDLE;
		return 1;

	    case TS_DO:
#ifdef DEBUG_TELNET
		if (TELOPT_OK(ch))
			log_file("telnet", "<- IAC DO %s", TELOPT(ch));
#endif
		switch (ch)
		{
		    case TELOPT_EOR:
			if (s->telnet.expect & TN_EXPECT_EOR)
				s->telnet.expect &= ~TN_EXPECT_EOR;
			{
				sprintf(iac, "%c%c%c", IAC, WILL, ch);
#ifdef DEBUG_TELNET
				log_file("telnet", "-> IAC WILL TELOPT_EOR");
#endif
				write_tcpsock(s, iac, 3);
			}
			s->flags |= TCPSOCK_EOR_OK;
			break;

		    case TELOPT_SGA:
			if (s->telnet.expect & TN_EXPECT_SGA)
			{
				write_tcpsock(s,
				    "IAC GA Rejected by client.\r\n", 28);
				s->telnet.expect &= ~TN_EXPECT_SGA;
			}
			else
			{
				sprintf(iac, "%c%c%c", IAC, WILL, ch);
#ifdef DEBUG_TELNET
				log_file("telnet", "-> IAC WILL TELOPT_SGA");
#endif
				write_tcpsock(s, iac, 3);
			}
			s->flags &= ~TCPSOCK_GA_OK;
			break;

		    case TELOPT_ECHO:
			if (s->telnet.expect & TN_EXPECT_ECHO)
				s->telnet.expect &= ~TN_EXPECT_ECHO;
			else
			{
#ifdef DEBUG_TELNET
				if (TELOPT_OK(ch))
					log_file("telnet", "-> IAC WONT %s",
					    TELOPT(ch));
#endif
				sprintf(iac, "%c%c%c", IAC, WONT, ch);
				write_tcpsock(s, iac, 3);
			}
			break;

		    case TELOPT_TM:
#ifdef DEBUG_TELNET
			log_file("telnet", "-> IAC WILL TM");
#endif
			sprintf(iac, "%c%c%c", IAC, WILL, ch);
			write_tcpsock(s, iac, 3);
			break;

		    default:
#ifdef DEBUG_TELNET
			if (TELOPT_OK(ch))
				log_file("telnet", "-> IAC WONT %s",
				    TELOPT(ch));
#endif
			sprintf(iac, "%c%c%c", IAC, WONT, ch);
			write_tcpsock(s, iac, 3);
			break;
		}
		s->telnet.state = TS_IDLE;
		return 1;

	    case TS_DONT:
#ifdef DEBUG_TELNET
		if (TELOPT_OK(ch))
			log_file("telnet", "<- IAC DONT %s", TELOPT(ch));
#endif
		switch (ch)
		{
		    case TELOPT_EOR:
			if (s->telnet.expect & TN_EXPECT_EOR)
			{
				s->telnet.expect &= ~TN_EXPECT_EOR;
				write_tcpsock(s,
				    "IAC EOR Rejected by client.\r\n", 29);
			}
			else
			{
#ifdef DEBUG_TELNET
			log_file("telnet", "-> IAC WONT EOR");
#endif
				sprintf(iac, "%c%c%c", IAC, WONT, ch);
				write_tcpsock(s, iac, 3);
			}
			s->flags &= ~TCPSOCK_EOR_OK;
			break;

		    case TELOPT_SGA:
			if (s->telnet.expect & TN_EXPECT_SGA)
				s->telnet.expect &= ~TN_EXPECT_SGA;
			else
			{
				sprintf(iac, "%c%c%c", IAC, WONT, ch);
#ifdef DEBUG_TELNET
				log_file("telnet", "-> IAC WONT TELOPT_SGA");
#endif
				write_tcpsock(s, iac, 3);
			}
			s->flags |= TCPSOCK_GA_OK;
			break;

		    case TELOPT_ECHO:
			if (s->telnet.expect & TN_EXPECT_ECHO)
				s->telnet.expect &= ~TN_EXPECT_ECHO;
			else
			{
#ifdef DEBUG_TELNET
				if (TELOPT_OK(ch))
					log_file("telnet", "-> IAC WONT %s",
					    TELOPT(ch));
#endif
				sprintf(iac, "%c%c%c", IAC, WONT, ch);
				write_tcpsock(s, iac, 3);
			}
			break;

		    default:
#ifdef DEBUG_TELNET
			if (TELOPT_OK(ch))
				log_file("telnet", "-> IAC WONT %s",
				    TELOPT(ch));
#endif
			sprintf(iac, "%c%c%c", IAC, WONT, ch);
			write_tcpsock(s, iac, 3);
			break;
		}
		s->telnet.state = TS_IDLE;
		return 1;

	    default:
		fatal("Bad state in handle_telnet: %d", s->telnet.state);
	}
	/* NOTREACHED */
	return 0;
}

void
initialise_ip_table()
{
	int i;

	for (i = 0; i < IPTABLE_SIZE; i++)
	{
		iptable[i].addr = (unsigned long)0;
		iptable[i].name = (char *)NULL;
	}
	ipcur = 0;
}

void
add_username(int lport, int rport, char *ipnum, char *uname)
{
	struct user *p;
	unsigned long addr = inet_addr(ipnum);

	for (p = users->next; p != (struct user *)NULL; p = p->next)
		if (p->socket.rport == rport && p->socket.lport == lport &&
		    p->socket.addr.sin_addr.s_addr == addr)
		{
			log_file("secure/ident", "User %s (%s); Username %s",
			    p->capname, lookup_ip_name(p), uname);
			COPY(p->uname, uname, "username");
			return;
		}
	log_file("syslog", "Lost username '%s' for %s", uname, ipnum);
}

void
add_ip_entry(char *num, char *name)
{
	unsigned long addr;
	int i;

	addr = inet_addr(num);

	for (i = 0; i < IPTABLE_SIZE; i++)
	{
		if (iptable[i].addr && iptable[i].addr == addr)
		{
			COPY(iptable[i].name, name, "Iptable name");
			return;
		}
	}
	iptable[ipcur].addr = addr;
	COPY(iptable[ipcur].name, name, "Iptable name");
	ipcur = (ipcur + 1) % IPTABLE_SIZE;
}

char *
ip_number(struct tcpsock *s)
{
	return inet_ntoa(s->addr.sin_addr);
}

char *
ip_addrtonum(unsigned long addr)
{
	struct in_addr saddr;

	memmove((char *)&(saddr.s_addr), (char *)&addr, sizeof(saddr.s_addr));
	return inet_ntoa(saddr);
}

char *
ip_numtoname(char *num)
{
	int i;
	unsigned long addr = inet_addr(num);

	if (!addr)
		return "console";
	
	for (i = 0; i < IPTABLE_SIZE; i++)
		if (iptable[i].addr == addr &&
		    iptable[i].name != (char *)NULL)
			return iptable[i].name;
	send_erq(ERQ_RESOLVE_NUMBER, "%s;\n", num);
	return num;
}

char *
ip_name(struct tcpsock *s)
{
	int i;

	if (!s->addr.sin_addr.s_addr)
		return "console";
	
	for (i = 0; i < IPTABLE_SIZE; i++)
		if (iptable[i].addr ==
		    (unsigned long)s->addr.sin_addr.s_addr
		    && iptable[i].name != (char *)NULL)
			return iptable[i].name;
	send_erq(ERQ_RESOLVE_NUMBER, "%s;\n", ip_number(s));
	return ip_number(s);
}

char *
lookup_ip_name(struct user *p)
{
	return ip_name(&p->socket);
}

char *
lookup_ip_number(struct user *p)
{
	return ip_number(&p->socket);
}

void
erqd_died()
{
	erqw = erqp = (FILE *)NULL;
	erqd_fd = -1;
}

void
start_erqd()
{
	struct jlm *j;

	if (erqp != (FILE *)NULL)
	{
		log_file("syslog", "Erqd already running in start_erqd.");
		return;
	}

	if ((j = jlm_pipe("erqd")) == (struct jlm *)NULL)
	{
		log_file("syslog", "Erqd missing.");
		return;
	}

	/* *ponder* - make erqd use the raw fd sometime ? */
	erqw = fdopen(j->outfd, "w");
	erqp = fdopen(j->infd, "r");
	erqd_fd = j->infd;
}

void
stop_erqd()
{
	send_erq(ERQ_DIE, "die\n");
}

#ifdef SUPER_SNOOP

int
start_supersnoop(char *name)
{
	char buff[BUFFER_SIZE];
	int fd;

	sprintf(buff, F_SNOOPS "%s", name);
	if ((fd = open(buff, O_WRONLY | O_APPEND | O_CREAT, 0600)) == -1)
	{
		log_perror("supersnoop open");
		return -1;
	}
	sprintf(buff, "\n\n#### Supersnoop started: %s####\n",
	    nctime(&current_time));
	write(fd, buff, strlen(buff));
	return fd;
}

int
check_supersnooped(char *name)
{
#ifdef SUPERSNOOP_ALL
	return start_supersnoop(name);
#else
	char buff[BUFFER_SIZE];
	FILE *fp;

	if ((fp = fopen(F_SNOOPED, "r")) == (FILE *)NULL)
		return -1;

	while (fscanf(fp, "%s", buff) != EOF)
	{
		if (!strcmp(buff, name))
		{
			fclose(fp);
			return start_supersnoop(name);
		}
	}
	fclose(fp);
	return -1;
#endif /* SUPERSNOOP_ALL */
}
#endif /* SUPER_SNOOP */

#ifdef INETD_SUPPORT
void
incoming_udp()
{
	extern void inter_parse(char *, char *);
	char *buf;

	if ((buf = scan_udpsock(udp_s)) != (char *)NULL)
		inter_parse(udp_s->host, buf);
}
#endif

#ifdef CDUDP_SUPPORT
void
incoming_cdudp()
{
        extern void cdudp_parse(char *, char *);
	char *buf;

	if ((buf = scan_udpsock(cdudp_s)) != (char *)NULL)
		cdudp_parse(cdudp_s->host, buf);
}
#endif /* CDUDP_SUPPORT */

#if defined(INETD_SUPPORT) || defined(CDUDP_SUPPORT) || defined(REMOTE_NOTIFY)
/* Send a udp packet.. if it gets there it gets there... */
void
send_udp_packet(char *host, int port, char *msg)
{
    	struct sockaddr_in name;
	int len;

	if (udp_send == -1)
		return;

    	name.sin_addr.s_addr = inet_addr(host);
    	name.sin_family = AF_INET;
    	name.sin_port = htons(port);

	/*log_file("udp_debug", "Sending to %s:%d, %s", host, port, msg);*/

	len = strlen(msg);

	ubytes_outt += len;

    	if (sendto(udp_send, msg, len, 0, 
	    (struct sockaddr *)&name, sizeof(name)) == -1)
		return;
		/*log_perror("UDP sendto");*/
}

#endif

/*
 * Sleep implemented using select().
 * On many systems, sleep() is implemented internally using alarm(). As
 * JeamLand uses alarm() for events, I cannot use sleep() portably.
 * JeamLand only sleeps on startup when booted with -k and on shutdown
 * while waiting for the JLMs to exit.
 */
void
my_sleep(unsigned int s)
{
	struct timeval delay;

	delay.tv_sec = s;
	delay.tv_usec = 0;

	log_file("syslog", "my_sleep(%u)", s);

	/* If sleep terminated early due to a signal, restart it */
	while (select(0, FD_CAST NULL, FD_CAST NULL, FD_CAST NULL,
	    (struct timeval *)&delay) == -1)
	{
		if (errno != EINTR)
			log_perror("my_sleep");
		log_file("syslog", "my_sleep: Interrupted, retrying.");
		delay.tv_sec = s;
		delay.tv_usec = 0;
	}
}

