#ifndef EXPLOSION_H_
#define EXPLOSION_H_

#include <stdio.h>

enum {
	EFF_EXPLOSION,
	EFF_FLARE,
	EFF_RING,
	EFF_BLUE_EXPLOSION,
	EFF_BLUE_RING,
	NUM_EXPLOSION_TYPES
};

void
reset_explosions(void);

void
draw_explosions(void);

void
update_explosions(void);

void
add_explosion(const vector2 *pos, float radius, int type, int ttl);

void
initialize_explosions(void);

void
serialize_explosions(FILE *out);

int
unserialize_explosions(const char *data);

#endif /* EXPLOSION_H_ */
