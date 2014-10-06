#ifndef FOE_H_
#define FOE_H_

#include "vector.h"

enum {
	FOE_SITTING_DUCK,
	FOE_JARHEAD,
	FOE_BRUTE,
	FOE_EVOLVED_DUCK,
	FOE_NINJA,
	FOE_FAST_DUCK,
	FOE_FAST_JARHEAD,
	FOE_BOMBER,
	NUM_FOE_TYPES
};

struct foe_common {
	int used;
	int tics;
	int type;
	vector2 pos;
	vector2 dir;
	vector3 rot_axis;
	float angle;
	float angle_speed;
	int powerup_type;
};

void
reset_foes(void);

void
draw_foes(void);

void
update_foes(void);

int
hit_foes(const vector2 *pos);

void
initialize_foes(void);

void
tear_down_foes(void);

void
spawn_new_foes(void);

const struct foe_common *
get_random_foe(void);

const struct foe_common *
get_closest_foe(const vector2 *pos);

#include <stdio.h>
void
serialize_foes(FILE *out);

int
unserialize_foes(const char *data);

#endif /* FOE_H_ */
