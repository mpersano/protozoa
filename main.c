#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <GL/glew.h>
#ifdef USE_CG
#include <Cg/cg.h>
#include <Cg/cgGL.h>
#endif
#include <sys/param.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

#include "panic.h"
#include "gl_util.h"
#include "audio.h"
#include "mesh.h"
#include "in_game.h"
#include "in_game_menu.h"
#include "main_menu.h"
#include "arena.h"
#include "foe.h"
#include "particle.h"
#include "explosion.h"
#include "ship.h"
#include "missile.h"
#include "bomb.h"
#include "laser.h"
#include "water.h"
#include "background.h"
#include "powerup.h"
#include "in_game_text.h"
#include "font.h"
#include "settings.h"
#include "common.h"
#include "highscores.h"
#include "highscore_input.h"
#include "bubbles.h"
#include "playback.h"
#include "credits.h"
#include "cursor.h"

enum {
	JOYSTICK_AXIS_MAX = 32768,
	JOYSTICK_AXIS_MIN = 32768/3,
	JOYSTICK_NUM_AXES = 7,
	MAX_FRAME_SKIP = 5,
};

#define FAR_Z 5000.f

int window_width;
int window_height;
int running;

int joystick_available;
float joystick_axis[4];

int last_mouse_x;
int last_mouse_y;

unsigned pad_state;
vector2 crosshair;
struct settings settings;

#ifdef USE_CG
CGcontext cg_context;
CGprofile cg_vert_profile;
CGprofile cg_frag_profile;
#endif

static SDL_Joystick *joystick;
static SDL_Cursor *cursor;

static const char *WINDOW_CAPTION = "Protozoa";
static const char *SETTINGS_FILE = "options.txt";

struct game_state *cur_state;

static unsigned char *capture_buffer = NULL;
static unsigned char *half_capture_buffer = NULL;

static int sdl_flags;

#ifdef WIN32
PFNGLACTIVETEXTUREARBPROC glActiveTextureARB = NULL;
PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB = NULL;
#endif

extern void settings_parse_file(const char *);
extern struct level *levels_parse_file(const char *);

struct resolution_info resolution_info[NUM_RESOLUTIONS] = {
	{ 640, 480 },
	{ 800, 600 },
	{ 1024, 768 },
};

void
set_video_mode(void)
{
	int flags = sdl_flags;

	if (settings.fullscreen_enabled)
		flags |= SDL_FULLSCREEN;

	window_width = resolution_info[settings.resolution].width;
	window_height = resolution_info[settings.resolution].height;

	if (!SDL_SetVideoMode(window_width, window_height, 0, flags))
		panic("SDL_SetVideoMode: %s", SDL_GetError());
}

static void
initialize_sdl(void)
{
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) < 0)
		panic("SDL_Init: %s", SDL_GetError());
	
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) == 0 &&
		  SDL_NumJoysticks() > 0) {
		joystick_available = 1;
		joystick = SDL_JoystickOpen(0);
	} else {
		joystick_available = 0;
	}

/*
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
*/
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

	sdl_flags = SDL_OPENGL;

	set_video_mode();

	SDL_WM_SetCaption(WINDOW_CAPTION, NULL);
}

static void
initialize_glew(void)
{
	int rv;

	if ((rv = glewInit()) != GLEW_OK)
		panic("glewInit failed: %s", glewGetErrorString(rv));

	if (!glewGetExtension("GL_ARB_multitexture"))
		panic("multitexture not supported");

#if 1
	if (glewGetExtension("GL_EXT_framebuffer_object")) {
		settings.static_settings->use_fbo = 1;
		fprintf(stderr, "FBO available.\n");
	} else {
		fprintf(stderr, "FBO not available.\n");
	}
#endif

#if 1
#ifdef WIN32
	if (!settings.static_settings->use_fbo) {
		if (wglewGetExtension("WGL_ARB_pbuffer") &&
		  wglewGetExtension("WGL_ARB_pixel_format")) {
			settings.static_settings->use_pbuffer = 1;
			fprintf(stderr, "p-buffer available.\n");

		  	if (wglewGetExtension("WGL_ARB_render_texture")) {
				settings.static_settings->use_pbuffer_render_texture = 1;
				fprintf(stderr, "p-buffer/WGL_ARB_render_texture available.\n");
			} else {
				fprintf(stderr, "p-buffer/WGL_ARB_render_texture not available.\n");
			}
		} else {
			fprintf(stderr, "p-buffer not available.\n");
		}
	}
#endif
#endif

	if (glewGetExtension("GL_ARB_vertex_buffer_object")) {
		settings.static_settings->use_vbo = 1;
		fprintf(stderr, "VBO available.\n");
	} else {
		fprintf(stderr, "VBO not available.\n");
	}
}

#ifdef USE_CG
static void
cg_error_callback(void)
{
	CGerror err = cgGetError();

	if (err != CG_NO_ERROR)
		panic("cg error: %s", cgGetErrorString(err));
}

static void
initialize_cg(void)
{
	cgSetErrorCallback(cg_error_callback);

	cg_context = cgCreateContext();

	cg_vert_profile = cgGLGetLatestProfile(CG_GL_VERTEX);
	cgGLSetOptimalOptions(cg_vert_profile);

	cg_frag_profile = cgGLGetLatestProfile(CG_GL_FRAGMENT);
	cgGLSetOptimalOptions(cg_frag_profile);
}
#endif

static void
tear_down_sdl(void)
{
	SDL_Quit();
}

#ifdef USE_CG
static void
tear_down_cg(void)
{
	cgDestroyContext(cg_context);
}
#endif

void
resize_viewport(void)
{
	glViewport(0, 0, window_width, window_height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.f, (GLfloat)window_width/window_height, .1f, FAR_Z);
	glMatrixMode(GL_MODELVIEW);

	if (capture_buffer)
		free(capture_buffer);

	if (half_capture_buffer)
		free(half_capture_buffer);

	capture_buffer = malloc(3*window_width*window_height);
	half_capture_buffer = malloc(3*(window_width/2)*(window_height/2));
}

static void
initialize_opengl(void)
{
	glClearColor(0, 0, 0, 0);

	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_COLOR_MATERIAL);

	resize_viewport();
}

void
continue_game(void)
{
	cur_state = &in_game_state;
}

void
start_game(void)
{
	reset_game_state();
	cur_state = &in_game_state;
}

void
start_main_menu(void)
{
	reset_main_menu_state();
	cur_state = &main_menu_state;
}

void
start_in_game_menu(void)
{
	reset_in_game_menu_state();
	cur_state = &in_game_menu_state;
}

void
start_highscore_input(void)
{
	reset_highscore_input_state();
	cur_state = &highscore_input_state;
}

void
start_playback(void)
{
	reset_playback_state();
	cur_state = &playback_state;
}

void
start_credits(void)
{
	reset_credits_state();
	cur_state = &credits_state;
}

static void
initialize_options(void)
{
	if (!load_settings(&settings, SETTINGS_FILE)) {
		settings.control_type = CONTROL_HYBRID; /* CONTROL_JOYSTICK; */

		settings.pad_keysyms[0] = SDLK_w; /* up */
		settings.pad_keysyms[1] = SDLK_s; /* down */
		settings.pad_keysyms[2] = SDLK_a; /* left */
		settings.pad_keysyms[3] = SDLK_d; /* right */

		settings.pad_keysyms[4] = SDLK_UP; /* up */
		settings.pad_keysyms[5] = SDLK_DOWN; /* down */
		settings.pad_keysyms[6] = SDLK_LEFT; /* left */
		settings.pad_keysyms[7] = SDLK_RIGHT; /* right */

		settings.pad_sticks[0] = 3;
		settings.pad_sticks[1] = 2;
		settings.pad_sticks[2] = 1;
		settings.pad_sticks[3] = 0;

		settings.pad_sticks[4] = 9;
		settings.pad_sticks[5] = 8;
		settings.pad_sticks[6] = 7;
		settings.pad_sticks[7] = 6;

		settings.fullscreen_enabled = 1;
		settings.resolution = RES_800X600;
		settings.water_detail = 2;
	}
}

static void
initialize_settings(void)
{
	pad_state = 0u;
	vec2_set(&crosshair, 0.f, 0.f);
	settings_parse_file("settings.txt");
}

static void
initialize_levels(void)
{
	int i;

	for (i = 0; i < NUM_LEVELS; i++) {
		static char name[80];
		struct wave **p;

		sprintf(name, "level-%d.txt", i);
		levels[i] = levels_parse_file(name);

		for (p = levels[i]->waves; p != &(levels[i]->waves[levels[i]->num_waves]); p++) {
			if (i & 1)
				(*p)->arena->center.y += 180;

			(*p)->arena->border_type = !(i & 1) ? AB_STANDARD_BLUE : AB_STANDARD_RED;
		}
	}
}

static void
parse_args(int argc, char *argv[])
{
	int i;

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-window")) {
			settings.fullscreen_enabled = 0;
		} else if (!strcmp(argv[i], "-fullscreen")) {
			settings.fullscreen_enabled = 1;
		} else if (!strcmp(argv[i], "-nosound")) {
			settings.static_settings->sound = 0;
		} else if (!strcmp(argv[i], "-sound")) {
			settings.static_settings->sound = 1;
		}
	}
}

static void
cd_to_data_dir(void)
{
	if (chdir(DATA_DIR) != 0)
		panic("chdir to `%s' failed: %s\n", DATA_DIR, strerror(errno));
}

extern unsigned long genrand_int32(void);

static void
initialize_srand(void)
{
	extern void init_genrand(unsigned long);
	init_genrand(0x1337babe);
}

static void
initialize_cursor(void)
{
	cursor = cursor_create_from_png("cursor.png");
	SDL_SetCursor(cursor);
}

static void
initialize(int argc, char *argv[])
{
	cd_to_data_dir();
	initialize_settings();
	initialize_options();
	parse_args(argc, argv);
	initialize_sdl();
	initialize_glew();
	initialize_opengl();
#ifdef USE_CG
	initialize_cg();
#endif
	initialize_srand();
	initialize_font();
	initialize_mesh();
	initialize_audio();
	initialize_main_menu();
	initialize_in_game_menu();
	initialize_highscores();
	initialize_highscore_input();
	initialize_levels();
	initialize_arena();
	initialize_foes();
	initialize_ship();
	initialize_powerups();
	initialize_in_game_texts();
	initialize_in_game_state();
	initialize_missiles();
	initialize_bombs();
	initialize_lasers();
	initialize_particles();
	initialize_explosions();
	initialize_background();
	initialize_water();
	initialize_bubbles();
	initialize_playback();
	initialize_credits();
	initialize_cursor();

	start_main_menu();
 	/* start_game(); */
	/* start_playback(); */
}

int
get_cur_ticks(void)
{
	return SDL_GetTicks();
}

float
frand(void)
{
	return (double)genrand_int32()/(double)UINT_MAX;
}

int
irand(int n)
{
	return genrand_int32()%n;
}

static void
dump_ppm(FILE *out)
{
	int i;

	/* capture */
	glReadPixels(0, 0, window_width, window_height, GL_RGB,
	  GL_UNSIGNED_BYTE, capture_buffer);

	fprintf(out, "P6\n%d %d\n255\n", window_width, window_height);

	for (i = window_height - 1; i >= 0; i--)
		fwrite(&capture_buffer[i*window_width*3], 3, window_width, out);
}

static void
dump_yuv420p(void)
{
	int i, j, k;
	unsigned char *p;

	/* capture */
	glReadPixels(0, 0, window_width, window_height, GL_RGB,
	  GL_UNSIGNED_BYTE, capture_buffer);

	/* average */
	p = half_capture_buffer;

	for (i = window_height - 2; i >= 0; i -= 2) {
		const unsigned char *q = &capture_buffer[i*3*window_width];

		for (j = 0; j < window_width; j += 2) {
			for (k = 0; k < 3; k++) {
				int c =
				  (int)q[0] +
				  (int)q[3] +
				  (int)q[3*window_width] +
				  (int)q[3*window_width + 3];
				*p++ = c/4;
				q++;
			}
			q += 3;
		}
	}

	/* y plane */
	p = half_capture_buffer;
	for (i = 0; i < window_height/2; i++) {
		for (j = 0; j < window_width/2; j++) {
			int r = *p++;
			int g = *p++;
			int b = *p++;
			int y = r*.299 + g*.587 + b*.114;
			if (y < 0)
				y = 0;
			if (y > 255)
				y = 255;
			printf("%c", y);
		}
	}

	/* u plane */
	for (i = 0; i < window_height/2; i += 2) {
		const unsigned char *p =
		  &half_capture_buffer[i*3*(window_width/2)];
		for (j = 0; j < window_width/2; j += 2) {
			int r = *p++;
			int g = *p++;
			int b = *p++;
			int u = r*-.169 + g*-.332 + b*.5 + 128;
			if (u < 0)
				u = 0;
			if (u > 255)
				u = 255;
			printf("%c", u);
			p += 3;
		}
	}

	/* v plane */
	for (i = 0; i < window_height/2; i += 2) {
		const unsigned char *p =
		  &half_capture_buffer[i*3*(window_width/2)];
		for (j = 0; j < window_width/2; j += 2) {
			int r = *p++;
			int g = *p++;
			int b = *p++;
			int v = r*.5 + g*-.419 + b*-.0813 + 128;
			if (v < 0)
				v = 0;
			if (v > 255)
				v = 255;
			printf("%c", v);
			p += 3;
		}
	}
}

static void
draw(void)
{
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	cur_state->draw(cur_state);
	SDL_GL_SwapBuffers();

	if (settings.static_settings->dump_frames)
		dump_yuv420p();

#if 0
	{
	static int frame_count = 0;
	if (frame_count++%20 == 0) {
		static int shot_number = 0;
		static char file_name[80];
		FILE *out;
		sprintf(file_name, "shot-%d.ppm", shot_number++);
		
		if ((out = fopen(file_name, "wb")) != NULL) {
			dump_ppm(out);
			fclose(out);
		}
	}
	}
#endif
}

static void
update(void)
{
	update_music();
	cur_state->update(cur_state);
}

static void
handle_mouse_motion(int mouse_x, int mouse_y)
{
	cur_state->on_mouse_motion(cur_state, mouse_x, mouse_y);
}

static void
handle_mouse_button_pressed(int mouse_x, int mouse_y)
{
	cur_state->on_mouse_button_pressed(cur_state, mouse_x, mouse_y);
}

static void
handle_mouse_button_released(int mouse_x, int mouse_y)
{
	cur_state->on_mouse_button_released(cur_state, mouse_x, mouse_y);
}

static void
handle_key_pressed(int key)
{
	cur_state->on_key_pressed(cur_state, key);
}

static void
handle_stick_pressed(int stick)
{
	cur_state->on_stick_pressed(cur_state, stick);
}

struct keymap {
	unsigned char bitmap[(SDLK_LAST + 7)/8];
};

static void
keymap_set(struct keymap *map, int key) {
	map->bitmap[key/8] |= (1u << (key%8));
}

static void
keymap_unset(struct keymap *map, int key) {
	map->bitmap[key/8] &= ~(1u << (key%8));
}

static int
keymap_is_set(struct keymap *map, int key) {
	return !!(map->bitmap[key/8] & (1u << (key%8)));
}

static void
handle_events(void)
{
	static struct keymap key_state = {{0}};
	static struct keymap prev_key_state = {{0}};
	static unsigned joystick_state = 0u;
	static unsigned prev_joystick_state = 0u;
	SDL_Event event;
	int mouse_x, mouse_y;
	int mouse_moved, mouse_button_pressed, mouse_button_released;
	int esc_pressed;
	int i;

	mouse_moved = mouse_button_pressed = mouse_button_released = 0;
	mouse_x = mouse_y = 0;
	esc_pressed = 0;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				running = 0;
				break;

			case SDL_KEYDOWN:
				keymap_set(&key_state,
				  event.key.keysym.sym);
				break;

			case SDL_KEYUP:
				keymap_unset(&key_state,
						event.key.keysym.sym);
				break;

			case SDL_MOUSEMOTION:
				mouse_moved = 1;
				mouse_x = event.motion.x;
				mouse_y = event.motion.y;
				break;

			case SDL_MOUSEBUTTONDOWN:
				pad_state |= PAD_BTN1;
				mouse_button_pressed = 1;
				mouse_x = event.button.x;
				mouse_y = event.button.y;
				break;

			case SDL_MOUSEBUTTONUP:
				pad_state &= ~PAD_BTN1;
				mouse_button_released = 1;
				mouse_x = event.button.x;
				mouse_y = event.button.y;
				break;
		}
	}

	if (joystick_available) {
		joystick_state = 0u;

		for (i = 0; i < JOYSTICK_NUM_AXES; i++) {
			int v = SDL_JoystickGetAxis(joystick, i);

			if (v >= JOYSTICK_AXIS_MIN)
				joystick_state |= (1u << 2*i);

			if (v <= -JOYSTICK_AXIS_MIN)
				joystick_state |= (1u << (2*i + 1));
		}
	}

	if (mouse_moved) {
		last_mouse_x = mouse_x;
		last_mouse_y = mouse_y;
		handle_mouse_motion(mouse_x, mouse_y);
	}

	if (mouse_button_pressed)
		handle_mouse_button_pressed(mouse_x, mouse_y);

	if (mouse_button_released)
		handle_mouse_button_released(mouse_x, mouse_y);

	for (i = 0; i < SDLK_LAST; i++) {
		if (keymap_is_set(&key_state, i) &&
				!keymap_is_set(&prev_key_state, i))
			handle_key_pressed(i);
	}

	for (i = 0; i < 2*JOYSTICK_NUM_AXES; i++) {
		if ((joystick_state & (1u << i)) &&
				!(prev_joystick_state & (1u << i)))
			handle_stick_pressed(i);
	}

	pad_state &= ~(PAD_1_UP|PAD_1_DOWN|PAD_1_LEFT|PAD_1_RIGHT|
			PAD_2_UP|PAD_2_DOWN|PAD_2_LEFT|PAD_2_RIGHT);

	for (i = 0; i < 8; i++) {
		if (settings.control_type == CONTROL_JOYSTICK) {
			if (joystick_state & (1u << settings.pad_sticks[i]))
				pad_state |= (1u << i);
		} else {
			if (keymap_is_set(&key_state, settings.pad_keysyms[i]))
				pad_state |= (1u << i);
		}
	}

	prev_key_state = key_state;
	prev_joystick_state = joystick_state;
}

static void
event_loop(void)
{
	int prev_ticks, cur_ticks, frame_count;
#ifndef FRAMESKIP
	int delay;
#endif
	static const int frame_interval_ticks = 1000/FPS;

	running = 1;

	prev_ticks = get_cur_ticks();

	while (running) {
		int i;

#ifndef FRAMESKIP
		prev_ticks = get_cur_ticks();
#endif
		handle_events();

#ifdef FRAMESKIP
		cur_ticks = get_cur_ticks();

		frame_count = (cur_ticks - prev_ticks)/frame_interval_ticks;

		if (frame_count <= 0) {
			frame_count = 1;
			SDL_Delay(frame_interval_ticks -
			  (cur_ticks - prev_ticks));
			prev_ticks += frame_interval_ticks;
		} else if (frame_count > MAX_FRAME_SKIP) {
			frame_count = MAX_FRAME_SKIP;
			prev_ticks = cur_ticks;
		} else {
			prev_ticks += frame_count*frame_interval_ticks;
		}

#if 0
		if (frame_count > 1)
			printf("skipped %d frames\n", frame_count - 1);
#endif

		for (i = 0; i < frame_count; i++)
			update();
		draw();
#else
		update();
		draw();

		delay = 1000/FPS - (get_cur_ticks() - prev_ticks);

		if (delay > 0)
			SDL_Delay(delay);
#endif
	}
}

static void
tear_down(void)
{
	tear_down_audio();
	tear_down_background();
	tear_down_water();
	tear_down_bubbles();
	tear_down_particles();
	tear_down_foes();
	tear_down_mesh();
#ifdef USE_CG
	tear_down_cg();
#endif
	destroy_texture_pool();
	font_destroy_resources();
	tear_down_sdl();

	save_settings(&settings, SETTINGS_FILE);
	save_highscores();
}

int
main(int argc, char *argv[])
{
	initialize(argc - 1, argv + 1);
	event_loop();
	tear_down();

	return 0;
}
