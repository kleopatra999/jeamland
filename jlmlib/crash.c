#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include "jlm.h"

int
main()
{
	struct line *l;

	register_ident("Crash - Jlm crash on demand.");
	claim_command("crash");
	claim_command("flood");

	while ((l = read_line()) != (struct line *)NULL)
	{
		if (l->action == T_CMD)
		{
			if (!strcmp(l->cmd, "crash"))
				kill(getpid(), SIGSEGV);
			else if (!strcmp(l->cmd, "flood"))
			{
				int i;

				if (l->text == (char *)NULL)
					write_user(l->user,
					    "Syntax: flood "
					    "<number of characters>.");
				else
					for (i = 0; i < atoi(l->text); i++)
					{
						printf("x");
						fflush(stdout);
					}
			}
		}
		free_line(l);
	}
	exit(0);
}

