/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	soul.h
 * Function:
 **********************************************************************/

#define S_STD		1
#define S_STD2		2
#define S_NOARG		3
#define S_NOTARG	4
#define S_TARG		5
#define S_OPT_TARG	6

struct feeling {
	char *name;
	char *adverb;
	char *prep;
	int type;
	int no_verb;
#ifdef PROFILING
	int calls;
#endif

	struct feeling *next;
	};

