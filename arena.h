#ifndef ARENA_H_
#define ARENA_H_

#include "vector.h"
#include "common.h"

enum arena_border {
	AB_STANDARD_BLUE,
	AB_STANDARD_RED,
	AB_DIAGRAM,
	AB_DIAGRAM_SELECTED,
	NUM_ARENA_BORDERS
};

extern arena *cur_arena;

void
reset_arena(void);

void
draw_arena_outline(const struct arena *arena, float alpha,
  enum arena_border border);

void
draw_filled_arena(const struct arena *arena);

void
initialize_arena(void);

int
bounces_with_arena(const vector2 *position, float radius, vector2 *arena_normal);

#endif /* ARENA_H_ */
