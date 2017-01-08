/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	jlm.h
 * Function:
 **********************************************************************/

#define JLM_MAX_TEXT	200
#define JLM_MAX_OFFSET	1024

struct jlm {
	int		 outfd;
	int 		 infd;
	int 		 pid;
	char 		*id;
	char 		*ident;
	enum { JL_S_NONE, JL_S_IDENT, JL_S_CLAIM, JL_S_FUNC } state;
	struct stack	 stack;
	struct strbuf	 text;
	struct object	*ob;
	struct jlm 	*next;
};

