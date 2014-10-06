%{
#include <stdlib.h>
#include <string.h>
#include "vector.h"
#include "levels_y_tab.h"
#include "yyerror.h"
%}
%%

"-"?[0-9]+		{
				yylval.intval = atol(yytext);
				return INTEGER;
			}
"-"?[0-9]*"."?[0-9]*([eE][+-]?[0-9]+)?	{
				yylval.realval = atof(yytext);
				return REAL;
			}
wave			{ return WAVE; }
label			{ return LABEL; }
arena			{ return ARENA; }
foe-count		{ return FOE_COUNT; }
verts			{ return VERTS; }
events			{ return EVENTS; }
circle			{ return CIRCLE; }
cluster			{ return CLUSTER; }
wall			{ return WALL; }
foe-type		{ return FOE_TYPE; }
time			{ return TIME; }
radius			{ return RADIUS; }
center			{ return CENTER; }
powerup-type		{ return POWERUP_TYPE; }
max-foes		{ return MAX_FOES; }
[a-zA-Z_][a-zA-Z0-9_-]*	{ yylval.strval = strdup(yytext); return IDENTIFIER; }
"{"|"}"|","|"["|"]"	{ return *yytext; }
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
[ \t\r]+
\n			{ ++lineno; }
.			{
				printf("invalid char '%c'\n", *yytext);
				yyerror("invalid character");
			}
%%

int yywrap(void) { return 1; }
