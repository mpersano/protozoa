#ifndef LASER_H_
#define LASER_H_

#include "vector.h"

enum {
	MAX_LASERS = 5
};

void
reset_lasers(void);

void
draw_lasers(void);

void
update_lasers(void);

int
add_laser(const vector2 *origin, const vector2 *direction);

void
initialize_lasers(void);

#endif /* LASER_H_ */
