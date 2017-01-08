/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	more.h
 * Function:
 **********************************************************************/

struct user;

struct more_buf {
        void            (*more_exit)(struct user *);
        char            *text;
        char            *ptr;
        int             lines;
        int             line;
        };

