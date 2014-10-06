#include <stdio.h>
#include "siod.h"

static LISP
frob(LISP arg)
{
	printf("frob called: ");

	if FLONUMP(arg)
		printf("%d\n", (int)FLONM(arg));
	else
		printf("`%s'\n", get_c_string(arg));

	return NIL;
}

const char *code = \
	"(define foo "
	"  (lambda (n) "
	"    (cond ((> n 0) (begin (foo (- n 1)) (frob n)))))) "
	"(foo 10) "
	"(frob 'howdy) ";

int
main(int argc, char **argv)
{
	static char *sargv[4];
	static char buf[1024];
	int rv;

	sargv[0] = argv[0];
	sargv[1] = "-v0";
	sargv[2] = "-g0";
	sargv[3] = "-h10000:10";

	siod_init(4, sargv);

	init_subr_1("frob", frob);

	sprintf(buf, "(*catch 'errobj (begin %s))", code);
	rv = repl_c_string(buf, 0, 0, sizeof buf);

	printf("rv = %d\nbuf = '%s'\n", rv, buf);

	return rv;
}
