/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	socket.h
 * Function:	Network socket specific code.
 **********************************************************************/

#define USER_BUFSIZE		2048
#define SERVICE_BUFSIZE		100

/* Allow up to 4K in the tcpsock output_buffer before panicing (sp?) */
#define TCPSOCK_MAXOB		0x1000

#define TCPSOCK_SPECIAL		0x1
#define TCPSOCK_TELNET		0x2
#define TCPSOCK_EOR_OK		0x4
#define TCPSOCK_BLOCKED		0x8
#define TCPSOCK_BINARY		0x10
#define TCPSOCK_BINHEAD		0x20
#define TCPSOCK_SHUTDOWN	0x40
#define TCPSOCK_RESTART		0x80
#define TCPSOCK_GA_OK		0x100

struct tcpsock {
	int			 fd;
	struct sockaddr_in	 addr;
	int			 lport;
	int			 rport;
	int			 sendq, recvq;
	char			*input_buffer;
	int			 ib_size;
	int			 ib_offset;
	int			 lines;
	time_t			 con_time;

	struct strbuf		 output_buffer;
	int			 ob_offset;

	/* Telnet code handling */
	struct {
		enum { TS_IDLE, TS_IAC, TS_WILL, TS_WONT, TS_DO, TS_DONT
			} state;
#define TN_EXPECT_ECHO	0x1
#define TN_EXPECT_EOR	0x2
#define TN_EXPECT_SGA	0x4
		int expect;
		} telnet;

	unsigned long flags;
	};

/* This is just a simple case of the tcpsock struct
 * It is only used when it is necessary to read from a udp packet. */

#define DEF_UDPSOCK_BUF		1024
#define MAX_UDPSOCK_ADDRS	5
struct udpsock {
	int			 fd;
	char			*input_buffer;
	char			*host;

	int			 ib_size;
	int			 ib_offset;
	};

struct ipentry {
	unsigned long		 addr;
	char 			*name;
	};

