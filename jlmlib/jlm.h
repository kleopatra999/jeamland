/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	jlm.h
 * Function:
 **********************************************************************/
struct line {
	char	*user;
	char	*cmd;
	enum { T_SAY, T_EMOTE, T_CMD } action;
	char	*text;
	};

/* Prototypes */
struct line *read_line(void);
void free_line(struct line *);
int register_ident(char *);
int claim_command(char *);
void write_user(char *, char *, ...);
void write_level(char *, char *, ...);
void notify_level(char *, char *, ...);
void force(char *, char *);
void chattr(char *, char *);

