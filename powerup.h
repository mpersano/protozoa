#ifndef POWERUP_H_
#define POWERUP_H_

#include "vector.h"

enum {
	PU_EXTRA_CANNON	= 0,
	PU_BOUNCY_SHOTS,
	PU_SIDE_CANNON,
	PU_POWERSHOT,
	PU_SHIELD,
	PU_EXTRA_SHIP,
	PU_HOMING_MISSILE,
	PU_LASER,
	NUM_POWERUP_TYPES
};

void
reset_powerups(void);

void
draw_powerups(void);

void
update_powerups(void);

void
initialize_powerups(void);

void
add_powerup(const vector2 *origin, int type);

#endif /* POWERUP_H_ */
