/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	inetd.h
 * Function:
 **********************************************************************/

#define UDPM_STATUS_TIME_OUT		0
#define UDPM_STATUS_DELIVERED_OK	1
#define UDPM_STATUS_UNKNOWN_USER	2
#define UDPM_STATUS_IN_SPOOL		3

#define JL_MAGIC	397

struct host {
	time_t status;
	time_t last;
	int last_id;

	int is_jeamland;

	char *name;
	char *host;
	int port;

	struct host *next;
	};

struct packet_field {
	char *name;
	struct svalue data;
	struct packet_field *next;
	};

struct packet {
	char *host;
	int port;

	char *request;
	char *sender;
	char *recipient;
	char *name;
	int id;
	char *data;

	struct packet_field *extra;
	};

struct split_packet {
	char *id;
	int elem;
	int total;
	char *data;
	struct	split_packet *next;
	};

struct pending {
	char *name;
	int id;
	
	char *who;
	char *request;

	char *packet;
	int retries;

	struct pending *next;
	};

struct past_id {
	char *name;
	int id;
	struct past_id *next;
	};

struct spool {
	/* There is a bit of redundancy here.. */
	char *who;
	char *to;
	char *pkt;
	char *host;
	char *message;

	int retries;

	struct spool *next;
	};

