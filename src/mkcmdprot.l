%{
/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	mkcmdprot.l
 * Function:	Generates prototypes for command functions.
 *		Used to create cmd.table.h during compilation
 **********************************************************************/

#include <stdio.h>
#include <string.h>

#define HISTORY_MAX	0x200
#define TAG_MAX		0x20

int	ct_found = 0;
int	lines = 0;
int	dupl = 0;
int	funcs = 0;

int	history_cur = 0;
char	history[HISTORY_MAX][TAG_MAX];

/* Prototypes */
void got_cmd();

%}

%%

.					;

\n					lines++;

\*[ ]CT[ ]\*				ct_found ^= 1;

^"#"[^\n]+				if (ct_found) printf("%s\n", yytext);

"/*"[A-Za-z	 ]+" commands ""*/"	printf("\n%s\n", yytext);

"f_"[a-z_]+				got_cmd();

%%

int
yywrap()
{
	return 1;
}

main()
{
	printf("/* cmd.table.h - prototypes for cmd.table.c\n"
	    " * WARNING: This is an automatically generated file.\n"
	    " *          any changes you make to it will be lost.\n"
	    " */\n\n");

  	while(yylex())
    		;

	fprintf(stderr, "         %d lines processed, %d functions, "
	    "%d duplicates removed.\n", lines, funcs, dupl);
	exit(0);
}

void
got_cmd()
{
	int i;
	char *p;

	if (!ct_found)
		return;

	p = yytext + 2;

	/* Have we already prototyped this one ? */
	for (i = history_cur; i--; )
		if (!strcmp(p, history[i]))
		{
			dupl++;
			return;
		}
	funcs++;

	strcpy(history[history_cur], p);
	printf("extern void f_%s(struct user *, int, char **);\n", p);
	if (++history_cur > HISTORY_MAX)
	{
		fprintf(stderr, "Too many functions!\n");
		exit(-1);
	}
}

