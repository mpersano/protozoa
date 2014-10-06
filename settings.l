%{
#include <stdlib.h>
#include <string.h>
#include "settings_y_tab.h"
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
def			{ return DEF; }
group			{ return GROUP; }
powerup			{ return POWERUP; }
missile			{ return MISSILE; }
ttl			{ return TTL; }
sprite			{ return SPRITE; }
text			{ return TEXT; }
string			{ return STRING; }
scale			{ return SCALE; }
source			{ return SOURCE; }
width			{ return WIDTH; }
height			{ return HEIGHT; }
color			{ return COLOR; }
speed			{ return SPEED; }
refire-interval		{ return REFIRE_INTERVAL; }
general-settings	{ return GENERAL_SETTINGS; }
god-mode		{ return GOD_MODE; }
background		{ return BACKGROUND; }
ship-settings		{ return SHIP; }
random-foes		{ return RANDOM_FOES; }
sound			{ return SOUND; }
start-level		{ return START_LEVEL; }
start-wave		{ return START_WAVE; }
all-powerups		{ return ALL_POWERUPS; }
water-color		{ return WATER_COLOR; }
mesh			{ return MESH; }
transform		{ return TRANSFORM; }
on			{ return ON; }
off			{ return OFF; }
serialization		{ return SERIALIZATION; }
[a-zA-Z_][a-zA-Z0-9_-]*	{ yylval.strval = strdup(yytext); return IDENTIFIER; }
"{"|"}"|","		{ return *yytext; }
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
int yywrap(void)
{
	return 1;
} 
