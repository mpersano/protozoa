%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "matrix.h"
#include "list.h"

#include "yy_to_settings.h"
#include "settings.h"
#include "yyerror.h"
#include "panic.h"

int yylex(void);

enum field_type {
	FT_TTL,
	FT_SPRITE,
	FT_SPEED,
	FT_REFIRE_INTERVAL,
	FT_SOURCE,
	FT_WIDTH,
	FT_HEIGHT,
	FT_COLOR,
	FT_TEXT,
	FT_STRING,
	FT_SCALE,
	FT_GOD_MODE,
	FT_BACKGROUND,
	FT_RANDOM_FOES,
	FT_START_LEVEL,
	FT_START_WAVE,
	FT_ALL_POWERUPS,
	FT_SOUND,
	FT_WATER_COLOR,
	FT_TRANSFORM,
	FT_MESH,
	FT_SERIALIZATION
};

struct field {
	enum field_type type;
	union {
		long intval;
		double realval;
		void *ptrval;
	};
};

enum st_node_type {
	SN_GROUP,
	SN_POWERUP,
	SN_MISSILE,
	SN_SPRITE,
	SN_TEXT,
	SN_GENERAL_SETTINGS,
	SN_SHIP,
	SN_SHIELD,
	SN_MESH
};

struct st_node_common {
	enum st_node_type type;
	char *name;
};

struct st_node_group {
	struct st_node_common common;
	struct list *children;
};

struct st_node_powerup {
	struct st_node_common common;
	struct powerup_settings *powerup;
};

struct st_node_missile {
	struct st_node_common common;
	struct missile_settings *missile;
};

struct st_node_sprite {
	struct st_node_common common;
	struct sprite_settings *sprite;
};

struct st_node_text {
	struct st_node_common common;
	struct text_settings *text;
};

struct st_node_general_settings {
	struct st_node_common common;
	struct general_settings *general_settings;
};

struct st_node_ship {
	struct st_node_common common;
	struct ship_settings *ship;
};

struct st_node_mesh {
	struct st_node_common common;
	struct mesh_settings *mesh;
};

union st_node {
	struct st_node_common common;
	struct st_node_general_settings general_settings;
	struct st_node_group group;
	struct st_node_powerup powerup;
	struct st_node_missile missile;
	struct st_node_sprite sprite;
	struct st_node_text text;
	struct st_node_ship ship;
	struct st_node_mesh mesh;
};

static struct rgba *
rgba_make(float r, float g, float b, float a)
{
	struct rgba *p = malloc(sizeof *p);
	p->r = r;
	p->g = g;
	p->b = b;
	p->a = a;
	return p;
}

static struct matrix *
matrix_make(float m11, float m12, float m13, float m14,
	    float m21, float m22, float m23, float m24,
	    float m31, float m32, float m33, float m34)
{
	struct matrix *p = malloc(sizeof *p);
	p->m11 = m11; p->m12 = m12; p->m13 = m13; p->m14 = m14;
	p->m21 = m21; p->m22 = m22; p->m23 = m23; p->m24 = m24;
	p->m31 = m31; p->m32 = m32; p->m33 = m33; p->m34 = m34;
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

static struct sprite_settings *
sprite_settings_make(struct list *fields)
{
	struct list_node *ln;
	struct sprite_settings *p;
	
	p = malloc(sizeof *p);

	for (ln = fields->first; ln; ln = ln->next) {
		struct field *field = (struct field *)ln->data;

		switch (field->type) {
			case FT_SOURCE:
				p->source = (char *)field->ptrval;
				break;

			case FT_WIDTH:
				p->width = field->realval;
				break;

			case FT_HEIGHT:
				p->height = field->realval;
				break;

			case FT_COLOR:
				p->color = (struct rgba *)field->ptrval;
				break;

			default:
				assert(0);
		}
	}

	return p;
}

static struct text_settings *
text_settings_make(struct list *fields)
{
	struct list_node *ln;
	struct text_settings *p;

	p = malloc(sizeof *p);

	for (ln = fields->first; ln; ln = ln->next) {
		struct field *field = (struct field *)ln->data;

		switch (field->type) {
			case FT_STRING:
				p->text = (char *)field->ptrval;
				break;

			case FT_COLOR:
				p->color = (struct rgba *)field->ptrval;
				break;

			case FT_SCALE:
				p->scale = field->realval;
				break;

			case FT_TTL:
				p->ttl = field->intval;
				break;

			default:
				assert(0);
		}
	}

	return p;
}

static struct powerup_settings *
powerup_settings_make(struct list *fields)
{
	struct list_node *ln;
	struct powerup_settings *p;

	p = malloc(sizeof *p);

	for (ln = fields->first; ln; ln = ln->next) {
		struct field *field = (struct field *)ln->data;

		switch (field->type) {
			case FT_TTL:
				p->ttl = field->intval;
				break;

			case FT_SPRITE:
				p->sprite =
				  (struct sprite_settings *)field->ptrval;
				break;

			case FT_TEXT:
				p->text =
				  (struct text_settings *)field->ptrval;
				break;

			default:
				assert(0);
		}
	}

	return p;
}

static struct missile_settings *
missile_settings_make(struct list *fields)
{
	struct list_node *ln;
	struct missile_settings *p;

	p = malloc(sizeof *p);

	for (ln = fields->first; ln; ln = ln->next) {
		struct field *field = (struct field *)ln->data;

		switch (field->type) {
			case FT_SPEED:
				p->speed = field->realval;
				break;

			case FT_REFIRE_INTERVAL:
				p->refire_interval = field->intval;
				break;

			case FT_SPRITE:
				p->sprite =
				  (struct sprite_settings *)field->ptrval;
				break;

			default:
				assert(0);
		}
	}

	return p;
}

static struct general_settings *
general_settings_make(struct list *fields)
{
	struct list_node *ln;
	struct general_settings *p;

	p = calloc(1, sizeof *p);

	for (ln = fields->first; ln; ln = ln->next) {
		struct field *field = (struct field *)ln->data;

		switch (field->type) {
			case FT_GOD_MODE:
				p->god_mode = field->intval;
				break;

			case FT_BACKGROUND:
				p->background = field->intval;
				break;

			case FT_RANDOM_FOES:
				p->random_foes = field->intval;
				break;

			case FT_SOUND:
				p->sound = field->intval;
				break;

			case FT_START_LEVEL:
				p->start_level = field->intval;
				break;

			case FT_START_WAVE:
				p->start_wave = field->intval;
				break;

			case FT_ALL_POWERUPS:
				p->all_powerups = field->intval;
				break;

			case FT_WATER_COLOR:
				p->water_color = (struct rgba *)field->ptrval;
				break;

			case FT_SERIALIZATION:
				p->serialization = field->intval;
				break;

			default:
				assert(0);
		}
	}

	return p;
}

static struct ship_settings *
ship_settings_make(struct list *fields)
{
	struct list_node *ln;
	struct ship_settings *p;
	struct sprite_settings *s;

	p = malloc(sizeof *p);

	for (ln = fields->first; ln; ln = ln->next) {
		struct field *field = (struct field *)ln->data;

		switch (field->type) {
			case FT_SPRITE:
				s = (struct sprite_settings *)field->ptrval;

				if (!strcmp(s->name, "shield"))
					p->shield_sprite = s;
				break;

			case FT_MESH:
				p->mesh = (struct mesh_settings *)field->ptrval;
				break;

			case FT_SPEED:
				p->speed = field->realval;
				break;

			default:
				assert(0);
		}
	}

	return p;
}

static struct mesh_settings *
mesh_settings_make(struct list *fields)
{
	struct list_node *ln;
	struct mesh_settings *p;

	p = malloc(sizeof *p);

	for (ln = fields->first; ln; ln = ln->next) {
		struct field *field = (struct field *)ln->data;

		switch (field->type) {
			case FT_TRANSFORM:
				p->transform = (struct matrix *)field->ptrval;
				break;

			case FT_SOURCE:
				p->source = (char *)field->ptrval;
				break;

			default:
				assert(0);
		}
	}

	return p;
}

static union st_node *
powerup_node_make(struct powerup_settings *powerup)
{
	union st_node *p = malloc(sizeof *p);
	p->common.type = SN_POWERUP;
	p->powerup.powerup = powerup;
	return p;
}

static union st_node *
sprite_node_make(struct sprite_settings *sprite)
{
	union st_node *p = malloc(sizeof *p);
	p->common.type = SN_SPRITE;
	p->sprite.sprite = sprite;
	return p;
}

static union st_node *
text_node_make(struct text_settings *text)
{
	union st_node *p = malloc(sizeof *p);
	p->common.type = SN_TEXT;
	p->text.text = text;
	return p;
}

static union st_node *
missile_node_make(struct missile_settings *missile)
{
	union st_node *p = malloc(sizeof *p);
	p->common.type = SN_MISSILE;
	p->missile.missile = missile;
	return p;
}

static union st_node *
general_settings_node_make(struct general_settings *general_settings)
{
	union st_node *p = malloc(sizeof *p);
	p->common.type = SN_GENERAL_SETTINGS;
	p->general_settings.general_settings = general_settings;
	return p;
}

static union st_node *
ship_node_make(struct ship_settings *ship)
{
	union st_node *p = malloc(sizeof *p);
	p->common.type = SN_SHIP;
	p->ship.ship = ship;
	return p;
}

static union st_node *
group_node_make(struct list *children)
{
	union st_node *p = malloc(sizeof *p);
	p->common.type = SN_GROUP;
	p->group.children = children;
	return p;
}

static union st_node *
mesh_node_make(struct mesh_settings *mesh)
{
	union st_node *p = malloc(sizeof *p);
	p->common.type = SN_MESH;
	p->mesh.mesh = mesh;
	return p;
}

static void
write_tabs(int ntabs)
{
	int i;
	for (i = 0; i < ntabs; i++)
		putchar('\t');
}

static void
handle_node_list(struct list *nodes, int indent);

static void
handle_missile_node(struct st_node_missile *missile, int indent)
{
	static struct name_to_settings {
		const char *name;
		struct missile_settings **settings;
	} name_to_settings[NUM_MISSILE_TYPES] = {
		{ "standard", &missile_settings[MT_NORMAL] },
		{ "power", &missile_settings[MT_POWER] },
		{ "homing", &missile_settings[MT_HOMING] },
	};
	int i;

#ifdef TEST
	write_tabs(indent);
	printf("missile: %s\n", missile->common.name);
#endif

	for (i = 0; i < NUM_MISSILE_TYPES; i++) {
		if (!strcmp(missile->common.name, name_to_settings[i].name)) {
			(*name_to_settings[i].settings) = missile->missile;
			break;
		}
	}
}

static void
handle_sprite_node(struct st_node_sprite *sprite, int indent)
{
#ifdef TEST
	write_tabs(indent);
	printf("sprite: %s\n", sprite->common.name);
#endif
}

static void
handle_powerup_node(struct st_node_powerup *powerup, int indent)
{
	static struct name_to_settings {
		const char *name;
		struct powerup_settings **settings;
	} name_to_settings[NUM_POWERUP_TYPES] = {
		{ "extra-cannon", &powerup_settings[PU_EXTRA_CANNON] },
		{ "bouncy-shots", &powerup_settings[PU_BOUNCY_SHOTS] },
		{ "side-cannon", &powerup_settings[PU_SIDE_CANNON] },
		{ "power-shots", &powerup_settings[PU_POWERSHOT] },
		{ "shield", &powerup_settings[PU_SHIELD] },
		{ "extra-ship", &powerup_settings[PU_EXTRA_SHIP] },
		{ "homing-missile", &powerup_settings[PU_HOMING_MISSILE] },
		{ "laser", &powerup_settings[PU_LASER] },
	};
	int i;

#ifdef TEST
	write_tabs(indent);
	printf("powerup: %s (%s)\n", powerup->common.name,
	  powerup->powerup->text->text);
#endif

	for (i = 0; i < NUM_POWERUP_TYPES; i++) {
		if (!strcmp(powerup->common.name, name_to_settings[i].name)) {
			*(name_to_settings[i].settings) = powerup->powerup;
			break;
		}
	}
}

static void
handle_mesh_node(struct st_node_mesh *mesh, int indent)
{
	static struct name_to_settings {
		const char *name;
		struct mesh_settings **settings;
	} name_to_settings[NUM_FOE_TYPES] = {
		{ "sitting-duck", &foe_mesh_settings[FOE_SITTING_DUCK] },
		{ "jarhead", &foe_mesh_settings[FOE_JARHEAD] },
		{ "brute", &foe_mesh_settings[FOE_BRUTE] },
		{ "evolved-duck", &foe_mesh_settings[FOE_EVOLVED_DUCK] },
		{ "ninja", &foe_mesh_settings[FOE_NINJA] },
		{ "fast-duck", &foe_mesh_settings[FOE_FAST_DUCK] },
		{ "fast-jarhead", &foe_mesh_settings[FOE_FAST_JARHEAD] },
		{ "bomber", &foe_mesh_settings[FOE_BOMBER] },
	};
	int i;

#ifdef TEST
	write_tabs(indent);
	printf("mesh: %s (%s)\n", mesh->common.name, mesh->mesh->source);
#endif

	for (i = 0; i < NUM_FOE_TYPES; i++) {
		if (!strcmp(mesh->common.name, name_to_settings[i].name)) {
			*(name_to_settings[i].settings) = mesh->mesh;
			break;
		}
	}
}

static void
handle_group_node(struct st_node_group *group, int indent)
{
#ifdef TEST
	write_tabs(indent);
	printf("group: %s\n", group->common.name);
#endif
	handle_node_list(group->children, indent + 1);
}

static void
handle_general_settings_node(struct st_node_general_settings *general_settings,
  int indent)
{
#ifdef TEST
	write_tabs(indent);
	printf("general_settings: %s\n", general_settings->common.name);
#endif
	settings.static_settings = general_settings->general_settings;
}

static void
handle_ship_node(struct st_node_ship *ship, int indent)
{
#ifdef TEST
	printf("ship: %s %s\n", ship->ship->ship_sprite->source, ship->ship->shield_sprite->source);
#endif
	ship_settings = ship->ship;
}

static void
handle_node_list(struct list *nodes, int indent)
{
	struct list_node *ln;

	for (ln = nodes->first; ln; ln = ln->next) {
		union st_node *node = (union st_node *)ln->data;

		switch (node->common.type) {
			case SN_GROUP:
				handle_group_node(&node->group, indent);
				break;

			case SN_POWERUP:
				handle_powerup_node(&node->powerup, indent);
				break;

			case SN_MISSILE:
				handle_missile_node(&node->missile, indent);
				break;

			case SN_SPRITE:
				handle_sprite_node(&node->sprite, indent);
				break;

			case SN_GENERAL_SETTINGS:
				handle_general_settings_node(&node->general_settings, indent);
				break;

			case SN_SHIP:
				handle_ship_node(&node->ship, indent);
				break;

			case SN_MESH:
				handle_mesh_node(&node->mesh, indent);
				break;

			default:
				assert(0);
		}
	}
}

static void
initialize_settings(struct list *nodes)
{
	handle_node_list(nodes, 0);
}

%}

%union {
	long intval;
	double realval;
	char *strval;
	struct list *listval;
	struct rgba *rgbaval;
	struct field *fieldval;
	struct sprite_settings *spriteval;
	struct powerup_settings *powerupval;
	struct missile_settings *missileval;
	struct general_settings *gensetsval;
	struct ship_settings *shipsetval; /* funnay */
	struct text_settings *textval;
	struct mesh_settings *meshsetval;
	union st_node *nodeval;
	struct matrix *matrixval;
}

%token DEF GROUP POWERUP MISSILE
%token GENERAL_SETTINGS GOD_MODE RANDOM_FOES BACKGROUND START_LEVEL SOUND ALL_POWERUPS ON OFF WATER_COLOR SERIALIZATION START_WAVE
%token TTL SPRITE COLOR TRANSFORM MESH
%token SPEED REFIRE_INTERVAL
%token SOURCE WIDTH HEIGHT
%token TEXT STRING SCALE
%token SHIP SHIELD

%token <realval> REAL
%token <intval> INTEGER
%token <strval> STRINGLIT IDENTIFIER

%type <realval> real
%type <intval> integer on_off
%type <rgbaval> color
%type <strval> node_spec sprite_spec
%type <fieldval> source_field width_field height_field color_field sprite_field
%type <fieldval> speed_field refire_interval_field missile_field
%type <fieldval> ttl_field powerup_field string_field scale_field text_field text_settings_field god_mode_field background_field general_settings_field ship_field random_foes_field sound_field start_level_field all_powerups_field water_color_field transform_field mesh_field sprite_child_field serialization_field start_wave_field
%type <listval> sprite_child_field_list powerup_field_list missile_field_list text_field_list general_settings_field_list ship_field_list mesh_field_list
%type <listval> node_list
%type <spriteval> sprite sprite_body
%type <powerupval> powerup
%type <missileval> missile
%type <shipsetval> ship
%type <meshsetval> mesh
%type <textval> text_settings
%type <gensetsval> general_settings
%type <nodeval> sprite_node powerup_node group_node missile_node node node_body text_settings_node general_settings_node ship_node mesh_node
%type <matrixval> matrix

%%

input
: node_list				{ initialize_settings($1); }
| /* nothing */
;

node_list
: node_list node			{ $$ = list_append($1, $2); }
| node					{ $$ = list_append(list_make(), $1); }
;

node
: node_spec node_body			{ $$ = $2; $$->common.name = $1; }
;

node_spec
: DEF IDENTIFIER			{ $$ = $2; }
| /* nothing */				{ $$ = NULL; }
;

node_body
: group_node
| powerup_node
| sprite_node
| missile_node
| text_settings_node
| general_settings_node
| ship_node
| mesh_node
;

group_node
: GROUP '{' node_list '}'		{ $$ = group_node_make($3); }
;

powerup_node
: powerup				{ $$ = powerup_node_make($1); }
;

sprite_node
: sprite				{ $$ = sprite_node_make($1); }
;

missile_node
: missile				{ $$ = missile_node_make($1); }
;

text_settings_node
: text_settings				{ $$ = text_node_make($1); }
;

general_settings_node
: general_settings			{ $$ = general_settings_node_make($1); }
;

ship_node
: ship					{ $$ = ship_node_make($1); }
;

powerup
: POWERUP '{' powerup_field_list '}'	{ $$ = powerup_settings_make($3); }
;

powerup_field_list
: powerup_field_list powerup_field	{ $$ = list_append($1, $2); }
| powerup_field				{ $$ = list_append(list_make(), $1); }
;

powerup_field
: ttl_field
| sprite_field
| text_settings_field
;

ttl_field
: TTL integer			{ $$ = field_intval_make(FT_TTL, $2); }
;

sprite_field
: sprite			{ $$ = field_ptrval_make(FT_SPRITE, $1); }
;

sprite
: sprite_spec sprite_body	{ $$ = $2; $$->name = $1; }
;

sprite_spec
: DEF IDENTIFIER		{ $$ = $2; }
| /* nothing */			{ $$ = NULL; }
;

sprite_body
: SPRITE '{' sprite_child_field_list '}'	{ $$ = sprite_settings_make($3); }
;

sprite_child_field_list
: sprite_child_field_list sprite_child_field	{ $$ = list_append($1, $2); }
| sprite_child_field			{ $$ = list_append(list_make(), $1); }
;

sprite_child_field
: source_field
| width_field
| height_field
| color_field
;

source_field
: SOURCE STRINGLIT		{ $$ = field_ptrval_make(FT_SOURCE, $2); }
;

width_field
: WIDTH real			{ $$ = field_realval_make(FT_WIDTH, $2); }
;

height_field
: HEIGHT real			{ $$ = field_realval_make(FT_HEIGHT, $2); }
;

color_field
: COLOR color			{ $$ = field_ptrval_make(FT_COLOR, $2); }
;

color
: INTEGER INTEGER INTEGER INTEGER { const float s = 1.f/255; $$ = rgba_make(s*$1, s*$2, s*$3, s*$4); }
;

missile
: MISSILE '{' missile_field_list '}'	{ $$ = missile_settings_make($3); }
;

missile_field_list
: missile_field_list missile_field	{ $$ = list_append($1, $2); }
| missile_field				{ $$ = list_append(list_make(), $1); }
;

missile_field
: speed_field
| refire_interval_field
| sprite_field
;

speed_field
: SPEED real			{ $$ = field_realval_make(FT_SPEED, $2); }
;

refire_interval_field
: REFIRE_INTERVAL integer	{ $$ = field_intval_make(FT_REFIRE_INTERVAL, $2); }
;

text_settings_field
: text_settings			{ $$ = field_ptrval_make(FT_TEXT, $1); }

text_settings
: TEXT '{' text_field_list '}'		{ $$ = text_settings_make($3); }
;

text_field_list
: text_field_list text_field		{ $$ = list_append($1, $2); }
| text_field				{ $$ = list_append(list_make(), $1); }
;

text_field
: string_field
| scale_field
| color_field
| ttl_field
;

string_field
: STRING STRINGLIT		{ $$ = field_ptrval_make(FT_STRING, $2); }
;

scale_field
: SCALE real			{ $$ = field_realval_make(FT_SCALE, $2); }
;

general_settings
: GENERAL_SETTINGS '{' general_settings_field_list '}'	{ $$ = general_settings_make($3); }
;

general_settings_field_list
: general_settings_field_list general_settings_field { $$ = list_append($1, $2); }
| general_settings_field { $$ = list_append(list_make(), $1); }
;

general_settings_field
: god_mode_field
| background_field
| random_foes_field
| sound_field
| start_level_field
| start_wave_field
| all_powerups_field
| water_color_field
| serialization_field
;

god_mode_field
: GOD_MODE on_off		{ $$ = field_intval_make(FT_GOD_MODE, $2); }
;

background_field
: BACKGROUND on_off		{ $$ = field_intval_make(FT_BACKGROUND, $2); }
;

random_foes_field
: RANDOM_FOES on_off		{ $$ = field_intval_make(FT_RANDOM_FOES, $2); }
;

sound_field
: SOUND on_off			{ $$ = field_intval_make(FT_SOUND, $2); }
;

start_level_field
: START_LEVEL INTEGER		{ $$ = field_intval_make(FT_START_LEVEL, $2); }
;

start_wave_field
: START_WAVE INTEGER		{ $$ = field_intval_make(FT_START_WAVE, $2); }
;

all_powerups_field
: ALL_POWERUPS on_off		{ $$ = field_intval_make(FT_ALL_POWERUPS, $2); }
;

water_color_field
: WATER_COLOR color		{ $$ = field_ptrval_make(FT_WATER_COLOR, $2); }
;

serialization_field
: SERIALIZATION on_off		{ $$ = field_intval_make(FT_SERIALIZATION, $2); }
;

ship
: SHIP '{' ship_field_list '}'	{ $$ = ship_settings_make($3); }
;

ship_field_list
: ship_field_list ship_field	{ $$ = list_append($1, $2); }
| ship_field			{ $$ = list_append(list_make(), $1); }
;

ship_field
: speed_field
| sprite_field
| mesh_field
;

mesh_field
: mesh				{ $$ = field_ptrval_make(FT_MESH, $1); }
;

mesh_node
: mesh				{ $$ = mesh_node_make($1); }
;

mesh
: MESH '{' mesh_field_list '}'	{ $$ = mesh_settings_make($3); }
;

mesh_field_list
: mesh_field_list mesh_field	{ $$ = list_append($1, $2); }
| mesh_field			{ $$ = list_append(list_make(), $1); }
;

mesh_field
: source_field 
| transform_field
;

transform_field
: TRANSFORM matrix		{ $$ = field_ptrval_make(FT_TRANSFORM, $2); }
;

matrix
: real real real real
  real real real real
  real real real real		{ $$ = matrix_make($1, $2, $3, $4,
  						   $5, $6, $7, $8,
						   $9, $10, $11, $12); }
;

on_off
: ON				{ $$ = 1; }
| OFF				{ $$ = 0; }

real
: INTEGER			{ $$ = $1; }
| REAL
;

integer
: INTEGER
;

%%

extern FILE *yyin;
extern int lineno;

void
settings_parse_file(const char *filename)
{
	lineno = 1;

	if ((yyin = fopen(filename, "r")) == NULL)
		panic("could not open `%s': %s", filename, strerror(errno));

	yyparse();

	fclose(yyin);
}
