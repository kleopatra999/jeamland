#/**********************************************************************
# * The JeamLand talker system
# * (c) Andy Fiddaman, 1994-96
# *
# * File:	mkcmdprot.awk
# * Function:	Generates prototypes for command functions.
# *		Used to create cmd.table.h during compilation
# **********************************************************************/
	BEGIN {
		FS = ",";
		x = 0;
		lines = 0;
		dupl = 0;
		in_c = 0;
		 printf("/* cmd.table.h - prototypes for cmd.table.c\n\
 * WARNING: This is an automatically generated file.\n\
 *          any changes you make to it will be lost.\n\
 */\n\n");
	}
	/*[ ]CT[ ]*/ {
		in_c = !in_c;
	}
	/*.*commands */ {
		if (in_c)
			printf("\n%s\n", $0);
	}
	/f_[a-z_]*/ {
		if (in_c)
		{
			fname = substr($2, index($2, "f_"));
			flag = 1;
			for (i = x; i--; )
			{
				if (arr[i] == fname)
				{
					flag = 0;
					dupl++;
					break;
				}
			}
			if (flag)
			{
				printf("extern void %s(struct user *, int, char **);\n", fname);
				arr[x++] = fname;
			}
		}
	}
	/^#.*$/ {
		if (in_c)
			printf("%s\n", $0);
	}
	END {
		printf("         %d lines processed, %d functions, %d duplicates removed.\n",
		    NR, x, dupl) > "/dev/stderr";
	}

