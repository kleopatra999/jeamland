/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	imud.h
 * Function:	Intermud stuff not specific to a protocol.
 **********************************************************************/

struct imud_route {
	enum {
#ifdef IMUD3_SUPPORT
		ROUTE_IMUD3,
#endif
#ifdef INETD_SUPPORT
		ROUTE_INETD,
#endif
#ifdef CDUDP_SUPPORT
		ROUTE_CDUDP,
#endif
		ROUTE_DUMMY
	} route;
	union {
		struct host *inetd_host;
		struct i3_host *imud3_host;
	} h;
};

