/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	jlm.h
 * Function:
 **********************************************************************/

#define JLM_MAX_TEXT	200
#define JLM_MAX_OFFSET	1024
#define JLM_IB_SIZE	0x100

#define JLM_TODEL	0x1

struct jlm {
	int		 infd, outfd;
	int 		 pid;
	int		 flags;

	char		 ibuf[JLM_IB_SIZE];
	int		 ib_offset;
	
	char 		*id;
	char 		*ident;
	enum { JL_S_NONE, JL_S_IDENT, JL_S_CLAIM, JL_S_FUNC } state;
	struct stack	 stack;
	struct strbuf	 text;
	struct object	*ob;
	struct jlm 	*next;
};

