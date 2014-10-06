#ifndef SHIP_H_
#define SHIP_H_

#include <stdio.h>
#include "vector.h"
#include "powerup.h"

#define SHIP_RADIUS .4f
#define SHIELD_RADIUS .9f

struct ship {
	vector2 pos;
	float angle;
	float cur_angle;
	float tilt;
	int refire_count;
	int powerup_ttl[NUM_POWERUP_TYPES];
	int is_alive;
	int tics;
};

void
reset_ship(void);

void
reset_ship_powerups(void);

void
draw_ship(void);

void
update_ship(void);

void
initialize_ship(void);

void
ship_add_powerup(int type);

int
ship_has_powerup(int type);

void
hit_ship(const vector2 *pos, float radius);

extern struct ship ship;

extern int ship_texture_id;
extern int ship_outline_texture_id;

extern int initial_powerup_ttl[NUM_POWERUP_TYPES];

void
gen_ship_implosion(void);

void
serialize_ship(FILE *out);

int
unserialize_ship(const char *data);

#endif /* SHIP_H_ */
