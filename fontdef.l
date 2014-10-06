%{
#include <stdlib.h>
#include <string.h>

#include "fontdef_y_tab.h"
#include "yyerror.h"

%}

%%

"-"?[0-9]+		{
				yylval.longval = atol(yytext);
				return INTEGER;
			}
glyph			{ return GLYPH; }
source			{ return SOURCE; }
size			{ return SIZE; }
[a-zA-Z_][a-zA-Z0-9_\-]*	{ yylval.strval = strdup(yytext); return IDENTIFIER; }
"{"|"}"			{ return *yytext; }
"#"[^\n]*		{ }
"\""			{
				static char *buf;
				static int buflen = 0;
				int i, c;

				if (buflen == 0)
					buf = malloc(1 + (buflen = 1));

				i = 0;

				while ((c = input()) != '"' && c != EOF) {
					if (c == '\\')
						c = input();

					if (i == buflen)
						buf = realloc(buf,
						  1 + (buflen *= 2));

					buf[i++] = c;
				}

				buf[i] = '\0';

				yylval.strval = strdup(buf);

				return STRINGLIT;
			}
[ \t]+			/* eat up whitespace */
\n			{ ++lineno; }
.			{
				printf("invalid char '%c'\n", *yytext);
				yyerror("invalid character");
			}

%%
int yywrap(void)
{
	return 1;
} 
