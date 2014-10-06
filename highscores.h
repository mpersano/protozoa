#ifndef HIGHSCORES_H_
#define HIGHSCORES_H_

enum {
	MAX_NAME_LEN = 3
};

struct rank {
	const char *description;
	float color[3];
};

const struct rank *
score_to_rank(int score);

void
initialize_highscores(void);

void
draw_highscore_table(float base_x, float base_y, float spread);

void
highscores_add(const char *name, int score, int level, int wave);

int
is_highscore(int score);

void
save_highscores(void);

#endif /* HIGHSCORES_H_ */
