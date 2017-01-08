/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	sent.h
 * Function:
 **********************************************************************/

struct sent {
	int 		 ref;
	struct object	*ob;
	char		*cmd;
	struct sent	*next_export;
	struct sent	*next_inv;
};

