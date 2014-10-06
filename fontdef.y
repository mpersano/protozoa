%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "yy_to_fontdef.h"
#include "yyerror.h"
#include "panic.h"
#include "font.h"
#include "list.h"
#include "dict.h"

int yylex(void);

enum font_property_type {
	FT_GLYPH,
	FT_SOURCE,
	FT_SIZE
};

struct font_property {
	enum font_property_type type;
	union {
		void *ptrval;
		long longval;
	};
};

static struct font_property *
font_property_ptrval_make(enum font_property_type type, void *ptrval)
{
	struct font_property *p = malloc(sizeof *p);

	p->type = type;
	p->ptrval = ptrval;

	return p;
}

static struct font_property *
font_property_longval_make(enum font_property_type type, long longval)
{
	struct font_property *p = malloc(sizeof *p);

	p->type = type;
	p->longval = longval;

	return p;
}

void
font_property_free(struct font_property *fp)
{
	switch (fp->type) {
		case FT_GLYPH:
		case FT_SOURCE:
			free(fp->ptrval);
			break;

		case FT_SIZE:
			break;
	}

	free(fp);
}

static struct glyph_info *
glyph_info_make(int code, int width, int height, int left, int top,
  int advance_x, int advance_y, int texture_x, int texture_y, int transposed)
{
	struct glyph_info *g = malloc(sizeof *g);

	g->code = code;
	g->width = width;
	g->height = height;
	g->left = left;
	g->top = top;
	g->advance_x = advance_x;
	g->advance_y = advance_y,
	g->texture_x = texture_x;
	g->texture_y = texture_y;
	g->transposed = transposed;

	return g;
}

static struct font_info *
font_info_make(struct list *font_properties)
{
	struct list_node *ln;
	struct font_info *fi;
	int num_glyphs;
	long font_size;
	const char *font_source;

	/* count glyphs, get source */

	num_glyphs = 0;
	font_source = NULL;
	font_size = -1;

	for (ln = font_properties->first; ln != NULL; ln = ln->next) {
		struct font_property *p = (struct font_property *)ln->data;

		switch (p->type) {
			case FT_GLYPH:
				++num_glyphs;
				break;

			case FT_SOURCE:
				font_source = strdup(p->ptrval);
				break;

			case FT_SIZE:
				font_size = p->longval;
				break;
		}
	}

	if (font_source == NULL)
		panic("font missing source?");

	if (font_size == -1)
		panic("font missing size?");

	fi = malloc(sizeof *fi);

	fi->texture_filename = font_source;
	fi->size = font_size;
	fi->num_glyphs = 0;
	fi->glyphs = malloc(num_glyphs*sizeof *fi->glyphs);

	/* collect glyphs */

	for (ln = font_properties->first; ln != NULL; ln = ln->next) {
		struct font_property *p = (struct font_property *)ln->data;

		if (p->type == FT_GLYPH)
			fi->glyphs[fi->num_glyphs++] =
			  *(struct glyph_info *)p->ptrval;
	}

	assert(fi->num_glyphs == num_glyphs);

	return fi;
}

static struct dict *font_dict;

%}

%union {
	long longval;
	char *strval;
	struct font_property *propval;
	struct glyph_info *glyphval;
	struct font_info *fontval;
	struct list *listval;
};

%token GLYPH SOURCE SIZE
%token <longval> INTEGER
%token <strval> IDENTIFIER STRINGLIT
%type <longval> glyph_code size
%type <propval> font_property
%type <strval> source_name source
%type <glyphval> glyph
%type <listval> font_properties
%%

input
: font_defs
| /* nothing */
;

font_defs
: font_defs font_def
| font_def
;

font_def
: IDENTIFIER '{' font_properties '}'
				{ 
					struct font_info *fi =
					  font_info_make($3);

					dict_put(font_dict, $1, fi);

					free($1);

					list_free($3,
					  (void(*)(void *))font_property_free);
				}
;

font_properties
: font_properties font_property	{ $$ = list_append($1, $2); }
| font_property			{ $$ = list_append(list_make(), $1); }
;

font_property
: source			{ $$ = font_property_ptrval_make(FT_SOURCE, $1); }
| glyph				{ $$ = font_property_ptrval_make(FT_GLYPH, $1); }
| size				{ $$ = font_property_longval_make(FT_SIZE, $1); }
;

source
: SOURCE source_name		{ $$ = $2; }
;

size
: SIZE INTEGER			{ $$ = $2; }
;

source_name
: IDENTIFIER
| STRINGLIT
;

glyph
: GLYPH glyph_code INTEGER INTEGER INTEGER INTEGER INTEGER INTEGER INTEGER INTEGER INTEGER
				{
					$$ = glyph_info_make($2, $3, $4, $5,
					  $6, $7, $8, $9, $10, $11);
				  }
;

glyph_code
: INTEGER
| STRINGLIT			{ $$ = *$1; free($1); }
;

%%

#include <stdio.h>

extern FILE *yyin;
extern int lineno;

void
fontdef_parse_file(struct dict *font_dict_, const char *filename)
{
	FILE *in;

	lineno = 1;

	if ((in = fopen(filename, "r")) == NULL)
		panic("could not open `%s': %s", filename, strerror(errno));

	yyin = in;

	font_dict = font_dict_;

	yyparse();

	fclose(in);
}
