#ifndef IN_GAME_TEXT_H_
#define IN_GAME_TEXT_H_

void
reset_in_game_texts(void);

void
draw_in_game_texts(void);

void
update_in_game_texts(void);

void
initialize_in_game_texts(void);

struct rgba;

void
add_in_game_text(const vector2 *pos, const struct rgba *color,
  float scale, int ttl, const char *fmt, ...);

#include <stdio.h>
void
serialize_in_game_texts(FILE *out);

int
unserialize_in_game_texts(const char *data);

#endif /* IN_GAME_TEXT_H_ */
