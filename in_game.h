#ifndef IN_GAME_H_
#define IN_GAME_H_

struct game_state;

#include "vector.h"

struct game_context {
	int score;
	int ships_left;
	int foes_left;
	int cur_wave;
	int cur_level;
	int level_tics;
	int multiplier;
	int tics_remaining;
	vector3 eye;

	/* nothing to see here, move along. */
	int multiplier_tic_count;
	int combo_message_tics;
	int last_combo;
};

extern struct game_context gc;

struct stat_counters {
	int tics;
	int combo_count;
	int missiles_missed;
	int missiles_fired;
	int waves;
};

extern struct stat_counters level_stat_counters;
extern struct stat_counters game_stat_counters;

#include "vector.h"
extern vector3 eye;

extern struct game_state in_game_state;

void
reset_game_state(void);

void
initialize_in_game_state(void);

#define BASE_EYE_Z 12.37f

enum {
	WAVE_TITLE_TICS = 80,
};

enum {
	IS_WAVE_TITLE,
	IS_IN_GAME,
	IS_RESPAWNING_SHIP,
	IS_PRE_WAVE_CLEARED,
	IS_WAVE_CLEARED,
	IS_GAME_OVER,
	IS_RANK,
	IS_HIGHSCORE_INPUT,
	IS_LEVEL_CLEARED,
	IS_LEVEL_TRANSITION,
};

struct inner_state {
	int state;
	int tics;
};

extern struct inner_state inner_state;

int
unserialize(const char *data);

#endif /* IN_GAME_H_ */
