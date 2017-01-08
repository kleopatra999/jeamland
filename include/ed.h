/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	ed.h
 * Function:
 **********************************************************************/

/* Ed flags */
#define ED_STACKED_TEXT 0x1
#define ED_NO_AUTOLIST  0x2
#define ED_INFO		0x4
#define ED_STACKED_TOK	0x8

/* Ed exit flags */
#define EDX_NORMAL      0
#define EDX_ABORT       1
#define EDX_NOTEXT      2

struct line {
	char		*text;
	struct line	*next, *last;
	};

struct user;

struct ed_buffer {
	int		flags;
	int		max;
	void		(*ed_exit)(struct user *, int);

	int		curr;
	struct line	*start, *last;

	/* Used during insert mode */
	struct line	*tmp, *tmp_last;
	int		insert;

#define MAX_RTEXT 0x1000
	struct strbuf	rtext;

	};

