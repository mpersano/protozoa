#ifndef SETTINGS_H_
#define SETTINGS_H_

struct rgba {
	float r;
	float g;
	float b;
	float a;
};

struct sprite_settings {
	char *name;
	char *source;
	float width;
	float height;
	struct rgba *color;
};

struct text_settings {
	char *text;
	float scale;
	struct rgba *color;
	int ttl;
};

struct powerup_settings {
	int ttl;
	struct sprite_settings *sprite;
	struct text_settings *text;
};

struct missile_settings {
	float speed;
	int refire_interval;
	struct sprite_settings *sprite;
};

struct matrix;

struct mesh_settings {
	struct matrix *transform;
	char *source;
};

struct ship_settings {
	float speed;
	struct mesh_settings *mesh;
	struct sprite_settings *shield_sprite;
};

struct general_settings {
	int god_mode;
	int background;
	int random_foes;
	int sound;
	int start_level;
	int start_wave;
	int all_powerups;
	int dump_stick_state;
	int playback_stick_state;
	int dump_frames;
	int use_vbo;
	int use_fbo;
	int use_pbuffer;
	int use_pbuffer_render_texture;
	int water_texture_width;
	int serialization;
	struct rgba *water_color;
};

enum resolution {
	RES_640X480,
	RES_800X600,
	RES_1024X768,
	NUM_RESOLUTIONS
};

extern struct resolution_info {
	int width;
	int height;
} resolution_info[NUM_RESOLUTIONS];

enum control_type {
	CONTROL_KEYBOARD,
	CONTROL_HYBRID,
	CONTROL_JOYSTICK,
	NUM_CONTROL_TYPES
};

struct settings {
	struct general_settings *static_settings;
	
	/* control */
	enum control_type control_type;
	int pad_keysyms[8];
	int pad_sticks[8];

	/* display */
	int fullscreen_enabled;
	enum resolution resolution;
	int water_detail;
};

extern struct settings settings;

extern struct ship_settings *ship_settings;

#include "powerup.h"
extern struct powerup_settings *powerup_settings[NUM_POWERUP_TYPES];

#include "missile.h"
extern struct missile_settings *missile_settings[NUM_MISSILE_TYPES];

#include "foe.h"
extern struct mesh_settings *foe_mesh_settings[NUM_FOE_TYPES];

int
load_settings(struct settings *settings, const char *file_name);

void
save_settings(const struct settings *settings, const char *file_name);

#endif /* SETTINGS_H_ */
