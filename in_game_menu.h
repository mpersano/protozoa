#ifndef IN_GAME_MENU_H_
#define IN_GAME_MENU_H_

struct game_state;

void
reset_in_game_menu_state(void);

void
initialize_in_game_menu(void);

extern struct game_state in_game_menu_state;

#endif /* IN_GAME_MENU_H_ */
