/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	sortsnoops.c
 * Function:	Small program to send snoop logs to an email address.
 **********************************************************************/
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "config.h"
#include "files.h"

void
do_mail(char *fname, char *swho, char *mailaddr)
{
	char tmp[0x100];

	/* Use system... lazy lazy ;-) */
	sprintf(tmp, "cat %s | mail -s '%s snoop log' %s", fname, swho,
	    mailaddr);
	system(tmp);
}

int
main(int argc, char **argv)
{
	FILE *fp;
	struct stat st;
	char who[0x100], fname[0x100], *p;

	if (argc != 2)
	{
		fprintf(stderr, "Syntax: sortsnoops <email_address>\n");
		exit(-1);
	}

	if ((fp = fopen(LIB_PATH "/" F_SNOOPED, "r")) == (FILE *)NULL)
		exit(0);

	while (fgets(who, sizeof(who), fp) != (char *)NULL)
	{
		if ((p = strchr(who, '\n')) != (char *)NULL)
			*p = '\0';

		sprintf(fname, LIB_PATH "/" F_SNOOPS "%s", who);

		if (stat(fname, &st) != -1 && st.st_size > 0)
		{
			do_mail(fname, who, argv[1]);
			sprintf(who, "%s~", fname);
			if (rename(fname, who) == -1)
				perror("rename");
		}
	}
	fclose(fp);
	return 0;
}
	
