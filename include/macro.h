/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	macro.h
 * Function:	Global macros
 **********************************************************************/

/* String macros.. */

#define COPY(xx, yy, zz) 	if (xx != (char *)NULL) xfree(xx); \
		 		xx = (char *)xalloc(strlen(yy) + 1, zz); \
				strcpy(xx, yy)

#define TCOPY(xx, yy, zz)	if (xx == (char *)NULL) COPY(xx, yy, zz)

#define FREE(xx) if (xx != (char *)NULL) do \
			{ xfree(xx); xx = (char *)NULL; } while(0)

#define IN_GAME(p) 	(p->last_command && !(p->flags & U_LOGGING_IN) && \
			p->rlname != (char *)NULL)

#define DECODE(xx, zz)	ptr = decode_string(buf, zz); \
			if (ptr == (char *)NULL) continue; \
			FREE(xx); xx = ptr; ptr = (char *)NULL

#define ENCODE(xx) 	FREE(ptr); \
			ptr = code_string(xx)

/* Executable macros */

#define ROOM_POINTER(roomp, fname) \
			((roomp = find_room(fname)) != (struct room *)NULL || \
			(roomp = restore_room(fname)) != (struct room *)NULL)

#define CHECK_INPUT_TO(user) \
		if (user->input_to != NULL_INPUT_TO) do \
		{ \
			write_socket(user, \
			    "Recursive input_to's are not permitted.\n"); \
			return; \
		} while(0)

/* Boolean macros */

#define ISCOMMENT(xx)	(xx[0] == '#' || xx[0] == '\0' || xx[0] == '\n')
#define ISROOT(xx)	(!strcmp(xx->rlname, ROOT_USER) || xx->sudo)
#define SYSROOM(xx)	(xx->fname[0] == '_' && strcmp(xx->owner, ROOT_USER))

#define CAN_CHANGE_ROOM(user, room) \
			(!strcmp(user->rlname, room->owner) || \
			(user->level > L_WARDEN && (SYSROOM(room) || \
		    	iaccess_check(user, query_real_level(room->owner)))))

#define CAN_SEE(user, target) \
			(!(target->saveflags & U_INVIS) || user->level >= \
			target->level)

#define SEE_LOGIN(user) (!(user->saveflags & (U_SLY_LOGIN | U_INVIS)))

#define CAN_READ_BOARD(user, board) \
			(board->read_grupe == (char *)NULL || \
			member_sysgrupe(board->read_grupe, user->rlname, 0))

#define CAN_WRITE_BOARD(user, board) \
			(board->write_grupe == (char *)NULL || \
			member_sysgrupe(board->write_grupe, user->rlname, 0))

/* Svalue macros */

#define TO_NUMBER(xx) xx.type = T_NUMBER; xx.u.number = 0
#define TO_STRING(xx) xx.type = T_STRING; xx.u.string = (char *)NULL
#define TO_POINTER(xx) xx.type = T_POINTER; xx.u.pointer = (void *)NULL
#define TO_EMPTY(xx) xx.type = T_EMPTY;

/* Debugging.. */
#ifdef CRASH_TRACE
#ifndef __FILE__
#define __FILE__ "File not supported"
#endif
#ifndef __LINE__
#define __LINE__ -1
#endif
#define FUN_START(xxx) add_function(xxx, __FILE__, __LINE__)
#define FUN_END end_function()
#define FUN_LINE add_function_line(__LINE__)
#define FUN_ARG(xxx) add_function_arg(xxx)
#define FUN_ADDR(xxx) add_function_addr(xxx)
#else
#define FUN_START(xxx) (void)0;
#define FUN_END (void)0
#define FUN_LINE (void)0
#define FUN_ARG(xxx) (void)0
#define FUN_ADDR(xxx) (void)0
#endif /* CRASH_TRACE */

