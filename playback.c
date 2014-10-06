#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <GL/gl.h>

#include "panic.h"
#include "common.h"
#include "in_game.h"
#include "water.h"
#include "background.h"
#include "playback.h"

enum {
	NUM_DUMPS = 1
};

static struct dump {
	char *data;
	int size;
} dumps[NUM_DUMPS];

static int cur_dump;
static int cur_dump_index;

void
reset_playback_state(void)
{
	reset_water();
	cur_dump = cur_dump_index = 0;
}

void
initialize_playback(void)
{
	int i;
	static const char *dump_files[] = {
		"dump.bin"
	};

	for (i = 0; i < NUM_DUMPS; i++) {
		struct stat sb;
		FILE *in;

		if (stat(dump_files[i], &sb))
			panic("stat failed: %s", strerror(errno));

		if ((in = fopen(dump_files[i], "rb")) == NULL)
			panic("fopen failed: %s", strerror(errno));

		dumps[i].size = sb.st_size;
		dumps[i].data = malloc(dumps[i].size);
		fread(dumps[i].data, 1, dumps[i].size, in);

		fclose(in);
	}
}

static void
update(struct game_state *gs)
{
	if (cur_dump_index >= dumps[cur_dump].size) {
		start_main_menu();
	} else {
		update_background();
		cur_dump_index += unserialize(
		  &dumps[cur_dump].data[cur_dump_index]);
	}
}

static void
draw(struct game_state *gs)
{
	if (cur_dump_index > 0)
		in_game_state.draw(NULL);
}

static void
on_mouse_motion(struct game_state *gs, int x, int y)
{
}

static void
on_mouse_button_pressed(struct game_state *gs, int x, int y)
{
	start_main_menu();
}

static void
on_mouse_button_released(struct game_state *gs, int x, int y)
{
	start_main_menu();
}

static void
on_stick_pressed(struct game_state *gs, int stick)
{
	start_main_menu();
}

static void
on_key_pressed(struct game_state *gs, int key)
{
	start_main_menu();
}

struct game_state playback_state =
  { update, draw,
    on_mouse_motion, on_mouse_button_pressed, on_mouse_button_released,
    on_key_pressed,
    on_stick_pressed };
