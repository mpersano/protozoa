#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "panic.h"
#include "yyerror.h"

int lineno;

void
yyerror(const char *str)
{
	panic("error on line %d: %s", lineno, str);
}
