#ifndef BACKGROUND_H_
#define BACKGROUND_H_

void
reset_background(void);

void
draw_skybox(void);

void
draw_background(int level, float arena_alpha, int bubbles);

void
update_background(void);

void
initialize_background(void);

void
tear_down_background(void);

#include <stdio.h>
void
serialize_background(FILE *out);

int
unserialize_background(const char *data);

#define CELL_RADIUS 25

#endif /* BACKGROUND_H_ */
