#ifndef PARTICLE_H_
#define PARTICLE_H_

#include "vector.h"

enum {
	PAL_BLUE,
	PAL_RED,
	PAL_GREEN,
	PAL_YELLOW,
	PAL_ORANGE,
	PAL_BRIGHT_YELLOW,
	PAL_PURPLE,
	NUM_PALETTES
};

typedef struct particle {
	int ttl;
	int t;
	int palette;
	float width;
	float height;
	vector2 pos;
	vector2 dir;
	vector2 acc;		/* acceleration */
	float speed;
	float friction;
} particle;

void
reset_particles(void);

void
draw_particles(void);

void
update_particles(void);

particle *
add_particle(void);

void
initialize_particles(void);

void
tear_down_particles(void);

void
serialize_particles(FILE *out);

int
unserialize_particles(const char *data);

#endif /* PARTICLE_H_ */
