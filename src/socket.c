/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
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

static int	sockfd = -1;
#ifdef UDP_SUPPORT
static int udp_s = -1, udp_send = -1;
#ifdef CDUDP_SUPPORT
static int cdudp_s = -1;
#endif
#endif
#ifdef SERVICE_PORT
static int service_s = -1;
#endif
static int console_used = 1;
char 		*host_name, *host_ip;
FILE	*erqp = (FILE *)NULL,
	*erqw;
extern struct user *users;
extern struct room *rooms;
extern struct jlm *jlms;

struct ipentry iptable[IPTABLE_SIZE];
int ipcur;

extern int errno, port;
extern time_t current_time;
extern char *currently_executing;
extern struct user *current_executor;
extern int sysflags;

int handle_telnet(struct user *, char);
void add_username(int, int, char *, char *);
void add_ip_entry(char *, char *);
void incoming_udp(void), incoming_cdudp(void);

/* Stats on socket communication. */
int bytes_out, bytes_in, bytes_outt, bytes_int;
float out_bps, in_bps;

void
dump_netstat(struct user *p)
{
	struct user *ptr;

	write_socket(p, " Fd     Send-Q     Recv-Q  Foreign Address\n");
	for (ptr = users->next; ptr != (struct user *)NULL; ptr = ptr->next)
	{
		/* *sigh*, I suppose it has to be done though.. */
		if (IN_GAME(ptr) && (ptr->saveflags & U_INVIS) &&
		    ptr->level > p->level)
			continue;
		write_socket(p, "%3d  %9d  %9d  %s.%d\n", ptr->fd,
		    ptr->sendq, ptr->recvq, lookup_ip_name(ptr),
		    ptr->rport);
	}
	
	write_socket(p, "-----\n");
	write_socket(p, "Total bytes transmitted:       %d\n", bytes_out);
	write_socket(p, "Total bytes received:          %d\n", bytes_in);
	write_socket(p, "Bytes per second transmitted:  %.2f\n", out_bps);
	write_socket(p, "Bytes per second received:     %.2f\n", in_bps);
}

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

/* Technically speaking, it should not be necessary to set the sockets
 * non-blocking as I only call read() on them if select() shows they have
 * data ready.
 * Sod's law states, it's better to be safe ;-)
 */
static void
nonblock(int fd)
{
	int tmp = 1;
#if defined(_AIX) || defined(HPUX)
    	if (ioctl(fd, FIONBIO, &tmp) == -1)
    	{
		log_perror("ioctl socket FIONBIO");
		fatal("Could not set socket non-blocking !\n");
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
		fatal("Could not set socket non-blocking.\n");
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
	setpgrp();
	background_process();
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
    	if (type == SOCK_STREAM && listen(sockfd, 5) == -1)
    	{
		log_perror("listen");
		fatal("Could not listen on port\n");
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
	memmove((char *)&saddr.s_addr, (char *)hp->h_addr, 4);
	host_ip = string_copy(inet_ntoa(saddr), "*localhost ipnum");
	log_file("syslog", "Local hostname: %s", host_name);
	log_file("syslog", "Local hostip:   %s", host_ip);

	signal(SIGPIPE, SIG_IGN);
	if (!(sysflags & SYS_NOIPC))
	{
		log_file("syslog", "Setting up ipc... [%d]", port);
		sockfd = setup_socket(port, SOCK_STREAM, hp);
	}
	else
		sockfd = -1;
#ifdef UDP_SUPPORT
	if (!(sysflags & SYS_NOUDP))
	{
		log_file("syslog", "Setting up ijp... [%d]", INETD_PORT);
		udp_s = setup_socket(INETD_PORT, SOCK_DGRAM, hp);
#ifdef CDUDP_SUPPORT
		log_file("syslog", "Setting up icp... [%d]", CDUDP_PORT);
		cdudp_s = setup_socket(CDUDP_PORT, SOCK_DGRAM, hp);
#endif
		if ((udp_send = socket(hp->h_addrtype, SOCK_DGRAM, 0)) == -1)
		{
			log_perror("udp send socket");
			fatal("Could not create UDP sending socket");
		}
	}
	else
		udp_s = cdudp_s = udp_send = -1;
#endif /* UDP_SUPPORT */

#ifdef SERVICE_PORT
	if (!(sysflags & SYS_NOSERVICE))
	{
		log_file("syslog", "Setting up jsp... [%d]", SERVICE_PORT);
		service_s = setup_socket(SERVICE_PORT, SOCK_STREAM, hp);
	}
	else
		service_s = -1;
#endif /* SERVICE_PORT */
}

void 
remove_ipc()
{
	log_file("syslog", "Shutting down ipc...");
	close(sockfd);
#ifdef UDP_SUPPORT
	close(udp_send);
	close(udp_s);
#ifdef CDUDP_SUPPORT
	close(cdudp_s);
#endif
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
			p->flags |= U_SOCK_QUITTING;
                }
	}
}

#ifdef SERVICE_PORT
static int services[MAX_SERVICES];
static char *serv_names[] = {
	"who",
	"finger",
	"valemail",
	(char *)NULL
	};

void
init_services()
{
	int i;

	for (i = MAX_SERVICES; i--; )
		services[i] = -1;
}

static void
new_service(int fd)
{
	struct sockaddr_in	addr;
	int 			length, newfd;
	int i;

	length = sizeof(addr);
	getsockname(fd, (struct sockaddr *)&addr, &length);
	if ((newfd = accept(fd, (struct sockaddr *)&addr, &length)) < 0)
	{
		log_perror("service accept");
		return;
	}
	nonblock(newfd);

	/* Any free service slots ? */
	for (i = MAX_SERVICES; i--; )
		if (services[i] == -1)
			break;
	if (i < 0)
	{
		write(newfd, "ERROR: All service slots in use.\n", 33);
		while (close(newfd) == -1 && errno == EINTR)
			;
		return;
	}
	services[i] = newfd;

	send_erq(ERQ_RESOLVE_NUMBER, "%s;\n", inet_ntoa(addr.sin_addr));

	/* Well, I abuse the dummy user for everything else, so... */
	memcpy((char *)&(users->addr), (char *)&addr, length);

#ifdef DEBUG_SERVICES
	notify_level_wrt_flags(L_CONSUL, EAR_TCP,
	    "[ *Service* socket: %d (%s:%d:%s) ].\n", newfd,
	    lookup_ip_number(users), ntohs(addr.sin_port),
	    lookup_ip_name(users));
#endif
	log_file("services", "[%-3d] Connect from %s.", newfd,
	    lookup_ip_name(users));
}

static void
handle_service(int i)
{
	/* No complex packet buffering for services.. */
	char buf[BUFFER_SIZE];
	int c;
	char *p;

	if ((c = read(services[i], buf, sizeof(buf))) <= 0)
	{
		if (!c)
			log_file("syslog", "Service flush");
		else
			log_perror("service read");
		close(services[i]);
		services[i] = -1;
		return;
	}
	buf[c] = '\0';
	if ((p = strpbrk(buf, "\r\n")) != (char *)NULL)
		*p = '\0';

	for (c = 0; serv_names[c] != (char *)NULL; c++)
		if (!strncasecmp(serv_names[c], buf, strlen(serv_names[c])))
			break;

	switch(c)
	{
	    case 0:	/* who */
	    {
		extern char *inet_who_text(void);

#ifdef DEBUG_SERVICES
		notify_level_wrt_flags(L_CONSUL, EAR_TCP,
		    "[ *Service* socket: %d (who) ].\n", services[i]);
#endif
		log_file("services", "[%-3d] who.", services[i]);

		p = inet_who_text();
		write(services[i], p, strlen(p));
		xfree(p);
		break;
	    }

	    case 1:	/* finger */
	    {
		if ((p = strchr(buf, ' ')) == (char *)NULL)
			write(services[i], "Syntax: finger <user>.\n", 23);
		else
		{
#ifdef DEBUG_SERVICES
			notify_level_wrt_flags(L_CONSUL, EAR_TCP,
			    "[ *Service* socket: %d (finger %s) ].\n",
			    services[i], p + 1);
#endif
			log_file("services", "[%-3d] finger %s.", services[i],
			    p + 1);

			p = finger_text((struct user *)NULL, p + 1,
			    FINGER_SERVICE);
			write(services[i], p, strlen(p));
			xfree(p);
		}
		break;
	    }

	    case 2:	/* Valemail */
#ifdef AUTO_VALEMAIL
		/* Format is: valemail password id */

		log_file("services", "[%-3d] valemail.", services[i]);
		if (valemail_service(buf))
		{
#ifdef DEBUG_SERVICES
			notify_level_wrt_flags(L_CONSUL, EAR_TCP,
			    "[ *Service* socket: %d (valemail request: "
			    "accepted) ].\n", services[i]);
#endif
			write(services[i], "Ok.\n", 4);
		}
		else
		{
#ifdef DEBUG_SERVICES
			notify_level_wrt_flags(L_CONSUL, EAR_TCP,
			    "[ *Service* socket: %d (valemail request: "
			    "rejected) ].\n", services[i]);
#endif
			write(services[i], "Denied.\n", 8);
		}
		break;
#endif /* AUTO_VALEMAIL */
		/* Fallthrough if AUTO_VALEMAIL undefined. */
	    default:
#ifdef DEBUG_SERVICES
		notify_level_wrt_flags(L_CONSUL, EAR_TCP,
		    "[ *Service* socket: %d (Unknown service: %s) ]\n",
		    services[i], buf);
#endif
		log_file("services", "[%-3d] Unknown service: %s.", 
		    services[i], buf);
		write(services[i], "ERROR: Unknown service.\n", 24);
		break;
	}
	close(services[i]);
	services[i] = -1;
}
#endif /* SERVICE_PORT */

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
		getsockname(fd, (struct sockaddr *)&addr, &length);
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
		while (close(newfd) == -1 && errno == EINTR)
			;
		return;
	}
#endif

	/* Initialize user */
	init_user(p = create_user());
	ob = create_object();
	ob->type = T_USER;
	ob->m.user = p;
	p->ob = ob;
	/* Get into the void ;) */
	move_object(ob, rooms->ob);

	/* Add user to END of list. */
	for (q = users; q->next != (struct user *)NULL; q = q->next)
		;
	p->next = (struct user *)NULL;
	q->next = p;

	/* Initialise telnet state */
	p->fd = newfd;
	p->telnet.state = TS_IDLE;
	p->telnet.expect = 0;

	p->input_to = get_name;
	p->login_time = current_time;
	p->level = L_VISITOR;

	/* Be on the safe side.. */
	logon_name(p);

	if (!console)
	{
		memcpy((char *)&(p->addr), (char *)&addr, length);
		p->lport = port;
		p->rport = ntohs(addr.sin_port);
		send_erq(ERQ_RESOLVE_NUMBER, "%s;\n",
		    inet_ntoa(addr.sin_addr));
		send_erq(ERQ_IDENT, "%s;%d,%d\n", inet_ntoa(addr.sin_addr),
		    p->lport, p->rport);
	}
	else
	{
		extern char *runas_username;
		memset((char *)&(p->addr), '\0', sizeof(p->addr));
		add_ip_entry(inet_ntoa(p->addr.sin_addr), "console");
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
	    "[ *TCP* Connect: %s (%s) ]\n",
	    lookup_ip_number(p), lookup_ip_name(p));

	dump_file(p, "etc", F_WELCOME, DUMP_CAT);
	f_version(p, 0, (char **)NULL);
	/* Required for some terminals, does no harm to others */
	echo(p);
	write_socket(p, "Enter your name: ");
	p->flags |= U_LOGGING_IN;
	add_event(create_event(), kick_logons, LOGIN_TIMEOUT + 1, "login");
}

void
replace_interactive(struct user *p, struct user *q)
{
	int tmp = p->fd;
	p->fd = q->fd;
	q->fd = tmp;

	tmp = p->flags;
	p->flags ^= q->flags & U_CONSOLE;
	q->flags ^= tmp & U_CONSOLE;

	memcpy((char *)&p->addr, (char *)&q->addr, sizeof(p->addr));
	p->lport = q->lport;
	p->rport = q->rport;
	FREE(p->uname);
	p->uname = q->uname;
	q->uname = (char *)NULL;
	close_socket(q);
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

void
send_erq(int request, char *fmt, ...)
{
	va_list argp;

	va_start(argp, fmt);
	do_send_erq(request, fmt, argp);
	va_end(argp);
}

void
send_user_erq(char *uname, int request, char *fmt, ...)
{
	int id, i;
	va_list argp;

	va_start(argp, fmt);
	id = do_send_erq(request, fmt, argp);
	va_end(argp);
	if (id == -1)
		return;

	/* Find a free slot.. */
	if ((i = find_pending_erq(-1)) == -1)
		return;	/* Ignore silently */
	pending_erqs[i].id = id;
	pending_erqs[i].type = ERQ_USER;
	pending_erqs[i].user = string_copy(uname, "erq_table uname");
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
		notify_level(L_CONSUL, "Bad Erqd packet: %s\n", buf);
		return;
	}

	/* See if anyone wanted this packet.. */
	if ((i = find_pending_erq(id)) != -1)
	{
		switch(pending_erqs[i].type)
		{
		    case ERQ_USER:
		    {
			struct user *p;

			if ((p = find_user((struct user *)NULL,
			    pending_erqs[i].user)) != (struct user *)NULL)
			{
				yellow(p);
				write_socket(p, "Erqd response: %s\n", buf);
				reset(p);
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

	switch(response)
	{
	    case ERQ_ERROR:
	    {
		char error[BUFFER_SIZE];
		if (sscanf(buf, "%*d,%*d:%[^\n]", error))
		{
			log_file("syslog", "Erqd error: %s", error);
#ifdef DEBUG
			notify_level(L_CONSUL, "Erqd error: %s\n", error);
#endif
		}
		else
		{
			log_file("syslog", "Erqd error: %s", buf);
#ifdef DEBUG
			notify_level(L_CONSUL, "Erqd error: %s\n", buf);
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

			if ((who = find_user((struct user *)NULL, name)) !=
			    (struct user *)NULL)
			{
				bold(who);
				if (success)
					write_socket(who,
					    "\nYour email request has been "
					    "successfully processed%s.\n",
					    who->level >= L_WARDEN ?
					    " by erqd" : "");
				else
					write_socket(who,
					    "\nYour email request has been "
					    "rejected%s.\n", who->level >=
					    L_WARDEN ? " by erqd (see syslog "
					    "for details)" : "");
				reset(who);
			}
		}

		break;
	    }

	    default:
#ifdef DEBUG
		notify_level(L_CONSUL, "Unrecognised erqd reqponse [%d]\n",
		    response);
#endif
		log_file("error", "Unrecognised erqd response [%d]",
		    response);
	}
}

void
scan_socket(struct user *p)
{
	int length;
	int flag;
	int actual;
	int nls;
	char *q, *r;
	char buf[BUFFER_SIZE];

	FUN_START("scan_socket");

	/* Don't flood their buffer */
	actual = (USER_BUFSIZE - p->ib_offset - 1) / 3;

	if ((length = read(p->fd, buf, actual)) == -1)
	{
		if (errno != EMSGSIZE)
		{
			notify_levelabu_wrt_flags(p, SEE_LOGIN(p) ? L_CONSUL :
			    L_OVERSEER, EAR_TCP,
			    "[ *TCP* Read error: %s [%s] (%s) %s ]\n",
                    	    p->capname, capfirst(level_name(p->level, 0)),
			    lookup_ip_name(p), perror_text());
			if (errno != EPIPE)
				log_perror("socket read");
			save_user(p, 1, 0);
			p->flags |= U_SOCK_CLOSING;
			FUN_END;
			return;
		}
	}
#ifdef DEBUG_TELNET
	log_file("debug_telnet", "Length: %d", length);
#endif
	if (!length)
	{
		if (!IN_GAME(p))
		{
			p->flags |= U_SOCK_QUITTING;
			FUN_END;
			return;
		}
		notify_levelabu_wrt_flags(p, SEE_LOGIN(p) ? L_CONSUL :
		    L_OVERSEER, EAR_TCP,
		    "[ *TCP* Read error: %s [%s] (%s) Remote Flush ]\n",
		    p->capname, capfirst(level_name(p->level, 0)),
		    lookup_ip_name(p));
		save_user(p, 1, 0);
		p->flags |= U_SOCK_CLOSING;
		FUN_END;
		return;
	}
	bytes_int += length;
	p->recvq += length;
	p->last_command = current_time;
	buf[length] = '\0';
#ifdef DEBUG_TELNET
	log_file("debug_telnet", "Socket: [%s]", buf);
#endif
	q = p->input_buffer + p->ib_offset;
	flag = nls = 0;
	for (r = buf; *r != '\0'; r++)
	{
		if (handle_telnet(p, *r))
			continue;
		switch(*r)
		{
		    case '\b':
		    case 0x7f:
			if (q > p->input_buffer && *(q - 1) != '\0')
			{
				q--;
				flag--;
			}
			break;
#ifdef EMBEDDED_NEWLINES
		    case '\\':
			if (r[1] == '\r')
			{
				r += 2;
				*q = '\n';
				*++q = '\t';
				q++;
				flag += 2;
			}
			else
			{
				*q = *r;
				q++, flag++;
			}
			break;
#endif
		    case '\r':
		    case '\n':
			if (++nls > 1 ||
			    *(r + 1) != (*r == '\r' ? '\n' : '\r'))
			{
				*q = '\0';
				q++, flag++;
				nls = 0;
			}
			break;
		    default:
			*q = *r;
			q++, flag++;
			break;
		}
	}
	p->ib_offset += flag;
	/*p->input_buffer[p->ib_offset] = '\0';*/
	FUN_END;
}

int
insert_command(struct user *p, char *c)
{
	int len = strlen(c);
	char *q = p->input_buffer + p->ib_offset;
	char *r;

	if (p->ib_offset + len > USER_BUFSIZE)
		return 0;
	for (r = c; *r != '\0'; r++)
		*q++ = *r;
	p->ib_offset += len;
	p->input_buffer[p->ib_offset++] = '\0';
	p->input_buffer[p->ib_offset] = '\0';
	return 1;
}

int
process_input(struct user *p)
{
	char *buf, buf2[USER_BUFSIZE];
	char *r, *s;
	int flag;
	extern void f_say(struct user *, int, char **);

	FUN_START("process_input");
	FUN_ARG(p->capname);

	flag = 0;
	for (r = p->input_buffer; r - p->input_buffer < p->ib_offset; r++)
		if (*r == '\0') /* Got a command! */
		{
			strcpy(buf2, p->input_buffer);
			FUN_LINE;
			if (p->ib_offset - 1 > r - p->input_buffer)
				memmove(p->input_buffer, r + 1,
				    p->ib_offset - (r - p->input_buffer) - 1);
			p->ib_offset -= r - p->input_buffer + 1;
			flag++;
			break;
		}
	if (!flag)
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

	/* Support for daft terminals */
	if (p->saveflags & U_EXTRA_NL)
		fwrite_socket(p, "\n");

	if (p->input_to != NULL_INPUT_TO)
	{
		current_executor = p;
		/* Just for Mr Persson.. */
		if (*buf == '!' && IN_GAME(p) && !(p->flags & U_NOESCAPE))
		{
			long oflags = p->flags & U_UNBUFFERED_TEXT;
			currently_executing = buf;
			strcpy(buf, buf + 1);
			p->flags |= U_UNBUFFERED_TEXT;
			parse_command(p, &buf);
			if (!oflags)
				p->flags &= ~U_UNBUFFERED_TEXT;
		}
		else
		{
			currently_executing = "Input to";
			FUN_START("input_to");
			FUN_ARG(buf);
			FUN_ADDR(p->input_to);
			p->input_to(p, buf);
			FUN_END;
		}
		currently_executing = (char *)NULL;
		current_executor = (struct user *)NULL;
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
		fatal("Bad user stack after evaluation.\n");
	if (!STACK_EMPTY(&p->atexit))
		fatal("Bad atexit stack after evaluation.\n");
	if (p->sudo)
		fatal("Sudo set in process_input().\n");
	if (p->flags & U_INHIBIT_QUIT)
		fatal("Inhibit quit set in process_input().\n");
#endif

	reset_eval_depth();

	/* Support for those strange talkers... */
	if (p->saveflags & U_CONVERSE_MODE)
	{
		switch(*buf)
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
process_sockets()
{
	fd_set readfds, exfds;
	struct user *p, *next_p;
	struct jlm *j;
	struct timeval delay;
	int ret, i;

	FUN_START("process_sockets");
	FUN_ARG("phase 1");

	FD_ZERO(&readfds);
	FD_ZERO(&exfds);
	if (sockfd != -1)
	{
		FD_SET(sockfd, &readfds);
	}
	if (!console_used)
	{
		FD_SET(fileno(stdin), &readfds);
	}
	if (erqp != (FILE *)NULL)
	{
		FD_SET(fileno(erqp), &readfds);
	}
#ifdef UDP_SUPPORT
	if (udp_s != -1)
	{
		FD_SET(udp_s, &readfds);
	}
#ifdef CDUDP_SUPPORT
	if (cdudp_s != -1)
	{
		FD_SET(cdudp_s, &readfds);
	}
#endif /* CDUDP_SUPPORT */
#endif /* UDP_SUPPORT */

#ifdef SERVICE_PORT
	if (service_s != -1)
	{
		FD_SET(service_s, &readfds);
	}
	for (i = MAX_SERVICES; i--; )
		if (services[i] != -1)
		{
			FD_SET(services[i], &readfds);
		}
#endif /* SERVICE_PORT */

	for (j = jlms; j != (struct jlm *)NULL; j = j->next)
		if (j->ob != (struct object *)NULL)
		{
			FD_SET(j->infd, &readfds);
		}

	for (p = users->next; p != (struct user *)NULL; p = p->next)
	{
		FD_SET(p->fd, &readfds);
		FD_SET(p->fd, &exfds);
	}

	delay.tv_usec = 10;
	/* If there is an event pending, don't sleep in the select. */
	if (sysflags & SYS_EVENT)
		delay.tv_sec = 0;
	else
		delay.tv_sec = 1200;
	ret = select(FD_SETSIZE, FD_CAST&readfds, FD_CAST NULL,
	    FD_CAST&exfds, &delay);
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

	FUN_ARG("phase 2");

	/* Check for console activity */
	if ((sysflags & SYS_CONSOLE) && !console_used &&
	    FD_ISSET(fileno(stdin), &readfds))
		new_user(fileno(stdin), 1);

	/* Check for new connection. */
	if (FD_ISSET(sockfd, &readfds))
		new_user(sockfd, 0);

	/* Check for a reply from erqd */
	if (erqp != (FILE *)NULL && FD_ISSET(fileno(erqp), &readfds))
		erq_reply();

	FUN_ARG("phase 2.1 (services)");
#ifdef SERVICE_PORT
	/* Check for service connection. */
	if (FD_ISSET(service_s, &readfds))
		new_service(service_s);
	for (i = MAX_SERVICES; i--; )
		if (services[i] != -1 && FD_ISSET(services[i], &readfds))
			handle_service(i);
#endif

#ifdef UDP_SUPPORT
	/* Check for UDP packets */
	if (FD_ISSET(udp_s, &readfds))
		incoming_udp();
#ifdef CDUDP_SUPPORT
	if (FD_ISSET(cdudp_s, &readfds))
		incoming_cdudp();
#endif
#endif

	FUN_ARG("phase 2.2 (jlm's)");
	/* Check for jlm input */
	for (j = jlms; j != (struct jlm *)NULL; j = j->next)
		if (j->ob != (struct object *)NULL &&
		    FD_ISSET(j->infd, &readfds))
			jlm_reply(j);

	FUN_ARG("phase 3 (badconn)");
	/* Remove bad connections */
	for (p = users->next; p != (struct user *)NULL; p = next_p)
	{
		next_p = p->next;
		if (FD_ISSET(p->fd, &exfds))
		{
			FD_CLR(p->fd, &readfds);
			if (p->name != (char *)NULL)
			{
				notify_levelabu(p, SEE_LOGIN(p) ?
				    L_VISITOR : p->level,
				    "[ %s [%s] has lost connection. ]\n",
			    	    p->name, capfirst(level_name(p->level, 0)));
				notify_levelabu_wrt_flags(p,
				    SEE_LOGIN(p) ? L_CONSUL : L_OVERSEER,
				    EAR_TCP,
				    "[ *TCP* Socket exception: %s (%s) ].\n",
				    p->name, lookup_ip_name(p));
				if (!(p->saveflags & U_INVIS))
					write_roomabu(p,
					    "%s has lost connection.\n",
				    	    p->name);
			}
			close_socket(p);
		}
	}

	FUN_ARG("phase 4");
	/* Handle input */
	for (p = users->next; p != (struct user *)NULL; p = p->next)
		if (FD_ISSET(p->fd, &readfds))
			scan_socket(p);

	FUN_ARG("phase 5");
	for (p = users->next; p != (struct user *)NULL; p = p->next)
		while (process_input(p))
			;
	
	FUN_ARG("phase 6");
	/* Remove bad connections */
	for (p = users->next; p != (struct user *)NULL; p = next_p)
	{
		next_p = p->next;
		if (p->flags & U_SOCK_CLOSING)
		{
			if (p->name != (char *)NULL)
			{
				notify_levelabu(p, SEE_LOGIN(p) ? L_VISITOR :
				    p->level,
				    "[ %s [%s] has lost connection. ]\n", 
			    	    p->name, capfirst(level_name(p->level, 0)));
				if (!(p->saveflags & U_INVIS))
					write_roomabu(p,
					    "%s has lost connection.\n",
				    	    p->name);
			}
			close_socket(p);
		}
		else if (p->flags & U_SOCK_QUITTING)
			close_socket(p);
	}
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

void
close_socket(struct user *p)
{
	struct user *ptr;
	extern void lastlog(struct user *);

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

	/* Handle atexit functions */
	execute_atexit(p);

	/* Remove user from global user list */
	for (ptr = users; ptr != (struct user *)NULL; ptr = ptr->next)
	{
		if (ptr->next == p)
		{
			ptr->next = ptr->next->next;
			/* Make sure the close is not interrupted by a
			 * heartbeat */
			if (p->flags & U_CONSOLE)
				console_used = 0;
			else
			{
				shutdown(p->fd, 2);
				while (close(p->fd) == -1 && errno == EINTR)
					;
			}
			if (p->snoop_fd != -1)
				while (close(p->snoop_fd) == -1 &&
				    errno == EINTR)
					;
			free_object(p->ob);
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

		if (tcgetattr(p->fd, &term) == -1)
		{
			log_perror("tcgetattr noecho");
			return;
		}
		term.c_lflag &= ~ECHO;
		if (tcsetattr(p->fd, TCSANOW, &term) == -1)
			log_perror("tcsetattr noecho");
	}
	else
	{
        	write_socket(p, "%c%c%c", IAC, WILL, TELOPT_ECHO);
#ifdef DEBUG_TELNET
		log_file("telnet", "-> IAC WILL TELOPT_ECHO");
#endif
		p->telnet.expect |= TN_EXPECT_ECHO;
	}
}

void
echo(struct user *p)
{
        p->flags &= ~U_NO_ECHO;
	if (p->flags & U_CONSOLE)
	{
		struct termios term;

		if (tcgetattr(p->fd, &term) == -1)
		{
			log_perror("tcgetattr echo");
			return;
		}
		term.c_lflag |= ECHO;
		if (tcsetattr(p->fd, TCSANOW, &term) == -1)
			log_perror("tcsetattr echo");
	}
	else
	{
        	write_socket(p, "%c%c%c", IAC, WONT, TELOPT_ECHO);
#ifdef DEBUG_TELNET
		log_file("telnet", "-> IAC WONT TELOPT_ECHO");
#endif
		p->telnet.expect |= TN_EXPECT_ECHO;
	}
}

int
handle_telnet(struct user *p, char ch)
{
	switch (p->telnet.state)
	{
	    case TS_IDLE:
		if (ch != (char)IAC)
			return 0;
		p->telnet.state = TS_IAC;
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
			write_socket(p, "\n[Yes]\n");
			p->telnet.state = TS_IDLE;
			return 1;
		    case (char)WILL:
			p->telnet.state = TS_WILL;
			return 1;
		    case (char)WONT:
			p->telnet.state = TS_WONT;
			return 1;
		    case (char)DO:
			p->telnet.state = TS_DO;
			return 1;
		    case (char)DONT:
			p->telnet.state = TS_DONT;
			return 1;
		    default:
			p->telnet.state = TS_IDLE;
			return 0;
		}
		/* NOTREACHED */
	    case TS_WILL:
#ifdef DEBUG_TELNET
		if (TELOPT_OK(ch))
			log_file("telnet", "<- IAC WILL %s", TELOPT(ch));
#endif
		write_socket(p, "%c%c%c", IAC, DONT, ch);
		p->telnet.state = TS_IDLE;
		return 1;
	    case TS_WONT:
#ifdef DEBUG_TELNET
		if (TELOPT_OK(ch))
			log_file("telnet", "<- IAC WONT %s", TELOPT(ch));
#endif
		write_socket(p, "%c%c%c", IAC, DONT, ch);
		p->telnet.state = TS_IDLE;
		return 1;
	    case TS_DO:
#ifdef DEBUG_TELNET
		if (TELOPT_OK(ch))
			log_file("telnet", "<- IAC DO %s", TELOPT(ch));
#endif
		switch (ch)
		{
		    case TELOPT_EOR:
			if (p->telnet.expect & TN_EXPECT_EOR)
			{
				p->telnet.expect &= ~TN_EXPECT_EOR;
				p->flags |= U_EOR_OK;
			}
			else
				write_socket(p, "%c%c%c", IAC, WONT, EOR);
			break;
		    case TELOPT_ECHO:
			if (p->telnet.expect & TN_EXPECT_ECHO)
				p->telnet.expect &= ~TN_EXPECT_ECHO;
			else
				write_socket(p, "%c%c%c", IAC, WONT, ch);
			break;
		    case TELOPT_TM:
			write_socket(p, "%c%c%c", IAC, WILL, ch);
			break;
		    default:
			write_socket(p, "%c%c%c", IAC, WONT, ch);
			break;
		}
		p->telnet.state = TS_IDLE;
		return 1;
	    case TS_DONT:
#ifdef DEBUG_TELNET
		if (TELOPT_OK(ch))
			log_file("telnet", "<- IAC DONT %s", TELOPT(ch));
#endif
		switch (ch)
		{
		    case TELOPT_EOR:
			if (p->telnet.expect & TN_EXPECT_EOR)
			{
				p->telnet.expect &= ~TN_EXPECT_EOR;
				p->flags &= ~U_EOR_OK;
				bold(p);
				write_socket(p,
				    "\nYour client does not support EOR "
				    "handling - turned off.\n");
				reset(p);
				/*p->saveflags &= ~TUSH;*/
			}
			else
				write_socket(p, "%c%c%c", IAC, WONT, EOR);
			break;
		    case TELOPT_ECHO:
			if (p->telnet.expect & TN_EXPECT_ECHO)
				p->telnet.expect &= ~TN_EXPECT_ECHO;
			else
				write_socket(p, "%c%c%c", IAC, WONT, ch);
			break;
		    default:
			write_socket(p, "%c%c%c", IAC, WONT, ch);
			break;
		}
		p->telnet.state = TS_IDLE;
		return 1;
	    default:
		fatal("Bad state in handle_telnet: %d", p->telnet.state);
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
		if (p->rport == rport && p->lport == lport &&
		    p->addr.sin_addr.s_addr == addr)
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
lookup_ip_number(struct user *p)
{
	return inet_ntoa(p->addr.sin_addr);
}

char *
lookup_ip_name(struct user *p)
{
	int i;
	
	for (i = 0; i < IPTABLE_SIZE; i++)
	{
		if (iptable[i].addr == (unsigned long)p->addr.sin_addr.s_addr
		    && iptable[i].name != (char *)NULL)
			return iptable[i].name;
	}
	return lookup_ip_number(p);
}

void
erqd_died()
{
	erqw = erqp = (FILE *)NULL;
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
		fatal("Erqd missing.");

	erqw = fdopen(j->outfd, "w");
	erqp = fdopen(j->infd, "r");
}

void
stop_erqd()
{
	send_erq(ERQ_DIE, "die\n");
}

#ifdef SUPER_SNOOP
int
check_supersnooped(char *name)
{
	char buff[BUFFER_SIZE];
	int fd;
#ifndef SUPERSNOOP_ALL
	FILE *fp;

	if ((fp = fopen(F_SNOOPED, "r")) == (FILE *)NULL)
		return -1;

	while (fscanf(fp, "%s", buff) != EOF)
	{
		if (!strcmp(buff, name))
		{
			fclose(fp);
#endif
			sprintf(buff, F_SNOOPS "%s", name);
			if ((fd = open(buff, O_WRONLY | O_APPEND | O_CREAT,
			    0600)) == -1)
			{
				log_perror("supersnoop open");
				return -1;
			}
			sprintf(buff, "\n\n#### Supersnoop started: %s####\n",
			    ctime(&current_time));
			write(fd, buff, strlen(buff));
			return fd;
#ifndef SUPERSNOOP_ALL
		}
	}
	fclose(fp);
	return -1;
#endif
}
#endif /* SUPER_SNOOP */

#ifdef UDP_SUPPORT
void
incoming_udp()
{
	extern void 		inter_parse(char *, char *);
	struct sockaddr_in	addr;
	int 			length, cnt;
	char			udp_buf[BUFFER_SIZE],
				*host;

	length = sizeof(addr);

	if ((cnt = recvfrom(udp_s, udp_buf, sizeof(udp_buf), 0,
	    (struct sockaddr *)&addr, &length)) != -1)
	{
		udp_buf[sizeof(udp_buf) - 1] = '\0';
		udp_buf[cnt] = '\0';

		/*log_file("debug", "Got: %s", udp_buf);*/

		host = inet_ntoa(addr.sin_addr);
		inter_parse(host, udp_buf);
	}
	else
		log_perror("UDP recvfrom");
}

#ifdef CDUDP_SUPPORT
void
incoming_cdudp()
{
        extern void             cdudp_parse(char *, char *);
        struct sockaddr_in      addr;
        int                     length, cnt;
        char                    udp_buf[BUFFER_SIZE],
                                *host;

        length = sizeof(addr);

        if ((cnt = recvfrom(cdudp_s, udp_buf, sizeof(udp_buf), 0,
            (struct sockaddr *)&addr, &length)) != -1)
        {
                udp_buf[sizeof(udp_buf) - 1] = '\0';
                udp_buf[cnt] = '\0';

                /*log_file("debug", "Got: %s", udp_buf);*/

                host = inet_ntoa(addr.sin_addr);
                cdudp_parse(host, udp_buf);
        }
        else
                log_perror("UDP recvfrom");
}
#endif /* CDUDP_SUPPORT */

/* Send a udp packet.. if it gets there it gets there... */
void
send_udp_packet(char *host, int port, char *msg)
{
    	struct sockaddr_in name;

	if (udp_send == -1)
		return;

    	name.sin_addr.s_addr = inet_addr(host);
    	name.sin_family = AF_INET;
    	name.sin_port = htons(port);

	/*log_file("udp_debug", "Sending: %s", msg);*/

    	if (sendto(udp_send, msg, strlen(msg), 0, 
	    (struct sockaddr *)&name, sizeof(name)) == -1)
		return;
		/*log_perror("UDP sendto");*/
}

#endif /* UDP_SUPPORT */

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

