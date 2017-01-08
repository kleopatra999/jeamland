/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	strbuf.h
 * Function:
 **********************************************************************/

/* 80 provides a good balance between speed and memory overhead - probably.. */
#define STRBUF_CHUNK    80

struct strbuf {
	int offset;
	int len;
	char *str;
#ifdef STRBUF_STATS
	int xpnd;
#endif
	};

enum sl_mode { SL_HEAD = 0, SL_TAIL, SL_SORT };
struct strlist {
	char *str;
	struct strlist *next;
	};

