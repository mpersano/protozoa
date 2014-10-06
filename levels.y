%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "list.h"
#include "vector.h"
#include "common.h"
#include "yy_to_levels.h"
#include "yyerror.h"
#include "panic.h"

static struct level *level;

int yylex(void);

enum field_type {
	FT_ARENA,
	FT_VERTS,
	FT_FOE_COUNT,
	FT_LABEL,
	FT_EVENTS,
	FT_FOE_TYPE,
	FT_TIME,
	FT_RADIUS,
	FT_CENTER,
	FT_WALL,
	FT_POWERUP_TYPE,
	FT_MAX_FOES,
};

struct field {
	enum field_type type;
	union {
		int intval;
		float realval;
		void *ptrval;
	};
};

vector2 *
vec2_make(float x, float y)
{
	vector2 *p = malloc(sizeof *p);
	p->x = x;
	p->y = y;
	return p;
}

static struct field *
field_intval_make(enum field_type type, int intval)
{
	struct field *p = malloc(sizeof *p);
	p->type = type;
	p->intval = intval;
	return p;
}

static struct field *
field_realval_make(enum field_type type, float realval)
{
	struct field *p = malloc(sizeof *p);
	p->type = type;
	p->realval = realval;
	return p;
}

static struct field *
field_ptrval_make(enum field_type type, void *ptrval)
{
	struct field *p = malloc(sizeof *p);
	p->type = type;
	p->ptrval = ptrval;
	return p;
}

static void
arena_initialize_verts(struct arena *a, struct list *vec2_list)
{
	struct list_node *ln;

	a->nverts = 0;

	for (ln = vec2_list->first; ln; ln = ln->next)
		a->verts[a->nverts++] = *(vector2 *)ln->data;
}

static struct arena *
arena_make_from_fields(struct list *field_list)
{
	struct list_node *ln;
	struct arena *a;

	a = malloc(sizeof *a);

	a->nverts = 0;

	for (ln = field_list->first; ln; ln = ln->next) {
		struct field *p = (struct field *)ln->data;

		switch (p->type) {
			case FT_VERTS:
				arena_initialize_verts(a,
				  (struct list *)p->ptrval);
				break;

			case FT_CENTER:
				a->center = *(vector2 *)p->ptrval;
				break;

			default:
				assert(0);
				break;
		}
	}

	return a;
}

static struct event *
event_make_from_fields(enum event_type type, struct list *field_list)
{
	struct list_node *ln;
	struct event *e;

	e = calloc(1, sizeof *e);

	e->type = type;
	e->powerup_type = -1;

	for (ln = field_list->first; ln; ln = ln->next) {
		struct field *p = (struct field *)ln->data;

		switch (p->type) {
			case FT_FOE_TYPE:
				e->foe_type = p->intval;
				break;

			case FT_TIME:
				e->time = p->intval;
				break;

			case FT_FOE_COUNT:
				e->foe_count = p->intval;
				break;

			case FT_RADIUS:
				e->radius = p->realval;
				break;

			case FT_CENTER:
				e->center = (vector2 *)p->ptrval;
				break;

			case FT_WALL:
				e->wall = p->intval;
				break;

			case FT_POWERUP_TYPE:
				e->powerup_type = p->intval;
				break;

			default:
				assert(0);
		}
	}

	return e;
}

static struct wave *
wave_make_from_fields(struct list *field_list)
{
	const struct list_node *ln;
	struct wave *l;

	l = calloc(1, sizeof *l);

	for (ln = field_list->first; ln; ln = ln->next) {
		const struct field *p = (const struct field *)ln->data;

		switch (p->type) {
			case FT_LABEL:
				l->label = (char *)p->ptrval;
				break;

			case FT_MAX_FOES:
				l->max_active_foes = p->intval;
				break;

			case FT_ARENA:
				l->arena = (struct arena *)p->ptrval;
				break;

			case FT_EVENTS:
				l->events = (struct list *)p->ptrval;
				break;

			default:
				assert(0);
				break;
		}
	}

	if (l->events == NULL)
		panic("wave missing events?");

	l->foe_count = 0;

	for (ln = l->events->first; ln; ln = ln->next) {
		const struct event *ev = (const struct event *)ln->data;
		l->foe_count += ev->foe_count;
	}

	return l;
}

static void
initialize_level(struct list *wave_list)
{
	struct list_node *ln;
	int num_waves;
	struct wave **waves;

	waves = malloc(wave_list->length*sizeof *waves);
	num_waves = 0;

	for (ln = wave_list->first; ln; ln = ln->next) {
		struct wave *p = (struct wave *)ln->data;
		waves[num_waves++] = p;
	}

	level = malloc(sizeof *level);
	level->waves = waves;
	level->num_waves = num_waves;
}

%}

%union {
	long intval;
	double realval;
	char *strval;
	vector2 *vec2val;
	struct list *listval;
	struct field *fieldval;
	struct arena *arenaval;
	struct wave *waveval;
	struct event *eventval;
}

%token WAVE LABEL ARENA FOE_COUNT VERTS MAX_FOES
%token EVENTS CIRCLE CLUSTER WALL FOE_TYPE TIME RADIUS CENTER POWERUP_TYPE
%token <intval> INTEGER
%token <realval> REAL
%token <strval> STRINGLIT IDENTIFIER

%type <strval> string_or_bareword
%type <realval> real
%type <fieldval> wave_field arena_field
%type <fieldval> label_field foe_count_field max_foes_field
%type <fieldval> verts_field
%type <listval> wave_list
%type <listval> wave_field_list arena_field_list vec2_list multi_vec2
%type <listval> events multi_events event_list
%type <listval> event_field_list
%type <vec2val> vec2
%type <waveval> wave
%type <arenaval> arena
%type <fieldval> event_field foe_type_field time_field
%type <fieldval> radius_field center_field wall_field powerup_field
%type <eventval> event circle_event cluster_event wall_event

%%

input
: wave_list				{ initialize_level($1); }
| /* nothing */
;

wave_list
: wave_list wave			{ $$ = list_append($1, $2); }
| wave					{ $$ = list_append(list_make(), $1); }
;

wave
: WAVE '{' wave_field_list '}'		{ $$ = wave_make_from_fields($3); }
;

wave_field_list
: wave_field_list wave_field		{ $$ = list_append($1, $2); }
| wave_field				{ $$ = list_append(list_make(), $1); }
;

wave_field
: label_field
| max_foes_field
| arena				{ $$ = field_ptrval_make(FT_ARENA, $1); }
| events			{ $$ = field_ptrval_make(FT_EVENTS, $1); }
;

label_field
: LABEL string_or_bareword	{ $$ = field_ptrval_make(FT_LABEL, $2); }
;

max_foes_field
: MAX_FOES INTEGER		{ $$ = field_intval_make(FT_MAX_FOES, $2); }
;

foe_count_field
: FOE_COUNT INTEGER		{ $$ = field_intval_make(FT_FOE_COUNT, $2); }
;

arena
: ARENA '{' arena_field_list '}'	{ $$ = arena_make_from_fields($3); }
;

arena_field_list
: arena_field_list arena_field		{ $$ = list_append($1, $2); }
| arena_field				{ $$ = list_append(list_make(), $1); }
;

arena_field
: verts_field
| center_field
;

verts_field
: VERTS multi_vec2		{ $$ = field_ptrval_make(FT_VERTS, $2); }
;

multi_vec2
: '[' vec2_list ']'			{ $$ = $2; }
| '[' vec2_list ',' ']'			{ $$ = $2; }
;

vec2_list
: vec2_list ',' vec2			{ $$ = list_append($1, $3); }
| vec2					{ $$ = list_append(list_make(), $1); }
;

vec2
: real real				{ $$ = vec2_make($1, $2); }
;

events
: EVENTS multi_events			{ $$ = $2; }
;

multi_events
: '[' event_list ']'			{ $$ = $2; }
| '[' event_list ',' ']'		{ $$ = $2; }
;

event_list
: event_list ',' event			{ $$ = list_append($1, $3); }
| event					{ $$ = list_append(list_make(), $1); }
;

event
: circle_event
| cluster_event
| wall_event
;

circle_event
: CIRCLE '{' event_field_list '}'	{ $$ = event_make_from_fields(EV_CIRCLE, $3); }
;

cluster_event
: CLUSTER '{' event_field_list '}'	{ $$ = event_make_from_fields(EV_CLUSTER, $3); }
;

wall_event
: WALL '{' event_field_list '}'		{ $$ = event_make_from_fields(EV_WALL, $3); }
;

event_field_list
: event_field_list event_field		{ $$ = list_append($1, $2); }
| event_field				{ $$ = list_append(list_make(), $1); }
;

event_field
: foe_type_field
| time_field
| foe_count_field
| radius_field
| center_field
| wall_field
| powerup_field
;

foe_type_field
: FOE_TYPE INTEGER		{ $$ = field_intval_make(FT_FOE_TYPE, $2); }
;

time_field
: TIME INTEGER			{ $$ = field_intval_make(FT_TIME, $2); }
;

radius_field
: RADIUS real			{ $$ = field_realval_make(FT_RADIUS, $2); }
;

center_field
: CENTER vec2			{ $$ = field_ptrval_make(FT_CENTER, $2); }
;

wall_field
: WALL INTEGER			{ $$ = field_intval_make(FT_WALL, $2); }
;

powerup_field
: POWERUP_TYPE INTEGER		{ $$ = field_intval_make(FT_POWERUP_TYPE, $2); }
;

real
: INTEGER				{ $$ = $1; }
| REAL
;

string_or_bareword
: STRINGLIT
| IDENTIFIER
;

%%

extern FILE *yyin;
extern int lineno;

struct level *
levels_parse_file(const char *filename)
{
	level = NULL;

	lineno = 1;

	if ((yyin = fopen(filename, "r")) == NULL)
		panic("could not open `%s': %s", filename, strerror(errno));

	yyparse();

	fclose(yyin);

	return level;
}
