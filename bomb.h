#ifndef BOMB_H_
#define BOMB_H_

#include "vector.h"

void
reset_bombs(void);

void
draw_bombs(void);

void
update_bombs(void);

void
add_bomb(const vector2 *origin);

void
initialize_bombs(void);

#endif /* BOMB_H_ */
