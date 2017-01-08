/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	imud3.h
 * Function:	Intermud-III implementation header file.
 **********************************************************************/

#define I3_TTL		5
#define I3_MAXLEN	200000

struct i3_host {
	char *name;
	int state;
	char *ip_addr;
	int player_port;
	int imud_tcp_port;
	int imud_udp_port;
	char *mudlib;
	char *base_mudlib;
	char *driver;
	char *mud_type;
	char *open_status;
	char *admin_email;

	int is_jl;

	/* For the moment... */
	struct mapping *services;

	struct i3_host *next;
	};

struct i3_channel {
	char *name;
	char *owner;
	enum i3stats
	    { I3C_UNKNOWN = -1, I3C_BANNED = 0, I3C_ADMITTED, I3C_FILTERED }
	    status;

	int listening;

	struct i3_channel *next;
	};

