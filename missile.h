#ifndef MISSILE_H_
#define MISSILE_H_

#include "vector.h"

enum missile_type {
	MT_NORMAL,
	MT_POWER,
	MT_HOMING,
	NUM_MISSILE_TYPES
};

void
reset_missiles(void);

void
draw_missiles(void);

void
update_missiles(void);

void
add_missile(const vector2 *origin, const vector2 *direction);

int
add_homing_missile(const vector2 *origin, const vector2 *direction);

void
initialize_missiles(void);

#include <stdio.h>
void
serialize_missiles(FILE *out);

int
unserialize_missiles(const char *data);

#endif /* MISSILE_H_ */
