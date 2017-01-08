/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	command.h
 * Function:
 **********************************************************************/

#define CMD_PARTIAL	0x1
#define CMD_AUDIT	0x2

struct command {
	char 	*command;
	void 	(*func)(struct user *, int, char **);
	int	level;
	int	flags;
	int	args;
	char	*syntax;
#ifdef PROFILING
	int	calls;
#endif
	};

