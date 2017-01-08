/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	vector.h
 * Function:	
 **********************************************************************/

struct vector {
	int size;
	struct svalue items[1];
	};

/* 5 provides a good balance between speed and memory overhead - probably.. */
#define VECBUF_CHUNK    5

struct vecbuf {
	int offset;
	struct vector *vec;
	};

