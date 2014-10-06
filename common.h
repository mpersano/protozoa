#ifndef COMMON_H_
#define COMMON_H_

#include "vector.h"

#define DATA_DIR "data/"

enum {
	FPS = 30,
};

enum {
	MAX_ARENA_VERTS = 20,
};

typedef struct arena {
	int nverts;
	vector2 verts[MAX_ARENA_VERTS];
	vector2 center;
	int border_type;
} arena;

enum event_type {
	EV_CIRCLE,
	EV_CLUSTER,
	EV_WALL
};

struct event {
	enum event_type type;
	int foe_type;
	int time;
	int foe_count;
	vector2 *center;
	float radius;
	int wall;
	int powerup_type;
};

struct wave {
	int foe_count;
	int max_active_foes;
	char *label;
	struct arena *arena;
	struct list *events;
};

struct level {
	int num_waves;
	struct wave **waves;
};

enum {
	NUM_LEVELS = 2
};

extern struct level *levels[NUM_LEVELS];

enum {
	PAD_1_UP	= 1,
	PAD_1_DOWN	= 2,
	PAD_1_LEFT	= 4,
	PAD_1_RIGHT	= 8,

	PAD_2_UP	= 16,
	PAD_2_DOWN	= 32,
	PAD_2_LEFT	= 64,
	PAD_2_RIGHT	= 128,

	PAD_BTN1	= 256,
};

extern int window_width;
extern int window_height;
extern int running;

extern int last_mouse_x;
extern int last_mouse_y;

extern float joystick_axis[4];

extern unsigned pad_state;
extern vector2 crosshair;
extern int allow_joystick;

struct game_state {
	void (*update)(struct game_state *);
	void (*draw)(struct game_state *);
	void (*on_mouse_motion)(struct game_state *, int, int);
	void (*on_mouse_button_pressed)(struct game_state *, int, int);
	void (*on_mouse_button_released)(struct game_state *, int, int);
	void (*on_key_pressed)(struct game_state *, int);
	void (*on_stick_pressed)(struct game_state *, int);
};

int
get_cur_ticks(void);

void
start_game(void);

void
continue_game(void);

void
start_main_menu(void);

void
start_in_game_menu(void);

void
start_highscores(void);

void
start_highscore_input(void);

void
start_playback(void);

void
start_credits(void);

float
frand(void);

int
irand(int n);

#endif /* COMMON_H_ */
