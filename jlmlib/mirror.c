#include <stdio.h>
#include "jlm.h"

int
main()
{
	struct line *l;

	register_ident("Mirror jlm (c) A. Fiddaman");
	claim_command("mirror");
	claim_command("mirror2");

	while ((l = read_line()) != (struct line *)NULL)
	{
		switch(l->action)
		{
		    case T_SAY:
			printf("%s said: %s\n", l->user, l->text);
			break;

		    case T_EMOTE:
			printf("%s emoted: %s\n", l->user, l->text);
			break;

		    case T_CMD:
			printf("%s commanded %s with arguments: %s\n",
			    l->user, l->cmd, l->text ? l->text : "<None>");
			break;

		    default:
			printf("Unknown action.\n");
			break;
		}
		free_line(l);
		fflush(stdout);
	}
	exit(0);
}

