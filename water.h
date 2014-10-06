#ifndef WATER_H_
#define WATER_H_

#include "vector.h"

void
reset_water(void);

void
draw_water(void);

void
update_water(void);

void
initialize_water(void);

void
tear_down_water(void);

void
perturb_water(const vector2 *pos, float amount);

#include <stdio.h>
void
serialize_water(FILE *out);

int
unserialize_water(const char *data);

#endif /* WATER_H_ */
