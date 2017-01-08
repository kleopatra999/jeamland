/* Doom.jlm - A joke ;-)
 * (c) A. Fiddaman 1996
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "jlm.h"

#define PAUSE2 usleep(rand() % 900)
#define PAUSE PAUSE2;PAUSE2;PAUSE2

int
main()
{
	struct line *l;
	int n;

	register_ident("JLM-Doom - (c) A.Fiddaman 1996");
	claim_command("doom");

	for (;;)
	{
		if ((l = read_line()) == (struct line *)NULL ||
		    l->action != T_CMD || l->user == (char *)NULL)
			continue;

		/* We only claimed one command - this must be it */

		/* Tee-hee */

		/* Blinking bold red */
		chattr(l->user, "~!r");
		write_user(l->user, "                             "
		    "\n\n\n                    DOOM System Startup v1.666\n");
		chattr(l->user, "z");
		sleep(2);

		write_user(l->user, "V_Init: allocate screens.");
		PAUSE;
		write_user(l->user, "M_LoadDefaults: Load system defaults.");
		PAUSE;
		write_user(l->user,
		    "Z_Init: Init zone memory allocation daemon.");
		PAUSE;
		write_user(l->user,
		    "DPMI memory: 0xb88000, 0x800000 allocated for zone");
		PAUSE;
		write_user(l->user, "W_Init: Init WADfiles.");
		PAUSE;
		write_user(l->user, "        adding doom.wad");
		PAUSE;
		write_user(l->user, "        registered version.");
		PAUSE;

		/* Bold white */
		chattr(l->user, "!w");
		write_user(l->user, "======================================="
		    "====================================");
		PAUSE;
		write_user(l->user, "	     This version is NOT SHAREWARE, "
		    "do not distribute!");
		PAUSE;
		write_user(l->user, "         Please report software piracy "
		    "to the SPA: 1-800-388-PIR8");
		PAUSE;
		write_user(l->user, "======================================="
		    "====================================");
		chattr(l->user, "z");
		sleep(5);
		PAUSE;
		write_user(l->user, "M_Init: Init miscellaneous info.");
		PAUSE;
		write_user_nonl(l->user,
		    "R_Init: Init DOOM refresh daemon - [");

		/* Bold yellow */
		chattr(l->user, "!y");
		for (n = 25; n--; )
		{
			write_user_nonl(l->user, ".");
			PAUSE;
			PAUSE;
		}
		chattr(l->user, "z");
		write_user(l->user, "]\nP_Init: Init Playloop state.");
		PAUSE;
		write_user(l->user, "I_Init: Setting up machine state.");
		PAUSE;
		write_user(l->user, "I_StartupDPMI");
		PAUSE;
		write_user(l->user, "I_StartupMouse");
		PAUSE;
		write_user(l->user, "Mouse: detected");
		PAUSE;
		write_user(l->user, "I_StartupJoystick");
		PAUSE;
		write_user(l->user, "I_StartupKeyboard");
		PAUSE;
		write_user(l->user, "I_StartupSound");
		PAUSE;
		write_user(l->user, "I_StartupTimer()");
		PAUSE;
		write_user(l->user, "calling DMX_Init");
		PAUSE;
		write_user(l->user,
		    "D_CheckNetGame: Checking network game status.");
		PAUSE;
		write_user(l->user, "startskill 2  deathmatch: 0  "
		    "startmap: 1  startepisode: 1");
		PAUSE;
		write_user(l->user, "player 1 of 1 (1 nodes)");
		PAUSE;
		write_user(l->user, "S_Init: Setting up sound.");
		PAUSE;
		write_user(l->user, "HU_Init: Setting up heads up display.");
		PAUSE;
		write_user(l->user, "ST_Init: Init status bar.");
		PAUSE;
		sleep(5);
		write_user(l->user, "\nS_Init: No sound device found.");
		PAUSE;
		write_user(l->user, "I_StartupKeyboard: No keyboard found.");
		PAUSE;
		write_user(l->user, "HU_Init: No display device found.");
		PAUSE;
		write_user(l->user, "D_Init: Boot aborted.");
	}
	return 0;
}

