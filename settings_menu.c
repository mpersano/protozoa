#include <stdio.h>
#include <string.h>
#include <SDL_keysym.h>

#include "font.h"
#include "background.h"
#include "bubbles.h"
#include "water.h"
#include "mesh.h"
#include "common.h"
#include "list.h"
#include "menu.h"
#include "settings.h"
#include "gl_util.h"
#include "settings_menu.h"

const char *key_names[] = {
	[0] = "NONE",
	[8] = "BACKSPACE",
	[9] = "TAB",
	[12] = "CLEAR",
	[13] = "RETURN",
	[19] = "PAUSE",
	[27] = "ESCAPE",
	[32] = "SPACE",
	[33] = "EXCLAIM",
	[34] = "QUOTEDBL",
	[35] = "HASH",
	[36] = "DOLLAR",
	[38] = "AMPERSAND",
	[39] = "QUOTE",
	[40] = "LEFTPAREN",
	[41] = "RIGHTPAREN",
	[42] = "ASTERISK",
	[43] = "PLUS",
	[44] = "COMMA",
	[45] = "MINUS",
	[46] = "PERIOD",
	[47] = "SLASH",
	[48] = "0",
	[49] = "1",
	[50] = "2",
	[51] = "3",
	[52] = "4",
	[53] = "5",
	[54] = "6",
	[55] = "7",
	[56] = "8",
	[57] = "9",
	[58] = "COLON",
	[59] = "SEMICOLON",
	[60] = "LESS",
	[61] = "EQUALS",
	[62] = "GREATER",
	[63] = "QUESTION",
	[64] = "AT",
	[91] = "LEFTBRACKET",
	[92] = "BACKSLASH",
	[93] = "RIGHTBRACKET",
	[94] = "CARET",
	[95] = "UNDERSCORE",
	[96] = "BACKQUOTE",
	[97] = "A",
	[98] = "B",
	[99] = "C",
	[100] = "D",
	[101] = "E",
	[102] = "F",
	[103] = "G",
	[104] = "H",
	[105] = "I",
	[106] = "J",
	[107] = "K",
	[108] = "L",
	[109] = "M",
	[110] = "N",
	[111] = "O",
	[112] = "P",
	[113] = "Q",
	[114] = "R",
	[115] = "S",
	[116] = "T",
	[117] = "U",
	[118] = "V",
	[119] = "W",
	[120] = "X",
	[121] = "Y",
	[122] = "Z",
	[127] = "DELETE",
	[160] = "WORLD_0",
	[161] = "WORLD_1",
	[162] = "WORLD_2",
	[163] = "WORLD_3",
	[164] = "WORLD_4",
	[165] = "WORLD_5",
	[166] = "WORLD_6",
	[167] = "WORLD_7",
	[168] = "WORLD_8",
	[169] = "WORLD_9",
	[170] = "WORLD_10",
	[171] = "WORLD_11",
	[172] = "WORLD_12",
	[173] = "WORLD_13",
	[174] = "WORLD_14",
	[175] = "WORLD_15",
	[176] = "WORLD_16",
	[177] = "WORLD_17",
	[178] = "WORLD_18",
	[179] = "WORLD_19",
	[180] = "WORLD_20",
	[181] = "WORLD_21",
	[182] = "WORLD_22",
	[183] = "WORLD_23",
	[184] = "WORLD_24",
	[185] = "WORLD_25",
	[186] = "WORLD_26",
	[187] = "WORLD_27",
	[188] = "WORLD_28",
	[189] = "WORLD_29",
	[190] = "WORLD_30",
	[191] = "WORLD_31",
	[192] = "WORLD_32",
	[193] = "WORLD_33",
	[194] = "WORLD_34",
	[195] = "WORLD_35",
	[196] = "WORLD_36",
	[197] = "WORLD_37",
	[198] = "WORLD_38",
	[199] = "WORLD_39",
	[200] = "WORLD_40",
	[201] = "WORLD_41",
	[202] = "WORLD_42",
	[203] = "WORLD_43",
	[204] = "WORLD_44",
	[205] = "WORLD_45",
	[206] = "WORLD_46",
	[207] = "WORLD_47",
	[208] = "WORLD_48",
	[209] = "WORLD_49",
	[210] = "WORLD_50",
	[211] = "WORLD_51",
	[212] = "WORLD_52",
	[213] = "WORLD_53",
	[214] = "WORLD_54",
	[215] = "WORLD_55",
	[216] = "WORLD_56",
	[217] = "WORLD_57",
	[218] = "WORLD_58",
	[219] = "WORLD_59",
	[220] = "WORLD_60",
	[221] = "WORLD_61",
	[222] = "WORLD_62",
	[223] = "WORLD_63",
	[224] = "WORLD_64",
	[225] = "WORLD_65",
	[226] = "WORLD_66",
	[227] = "WORLD_67",
	[228] = "WORLD_68",
	[229] = "WORLD_69",
	[230] = "WORLD_70",
	[231] = "WORLD_71",
	[232] = "WORLD_72",
	[233] = "WORLD_73",
	[234] = "WORLD_74",
	[235] = "WORLD_75",
	[236] = "WORLD_76",
	[237] = "WORLD_77",
	[238] = "WORLD_78",
	[239] = "WORLD_79",
	[240] = "WORLD_80",
	[241] = "WORLD_81",
	[242] = "WORLD_82",
	[243] = "WORLD_83",
	[244] = "WORLD_84",
	[245] = "WORLD_85",
	[246] = "WORLD_86",
	[247] = "WORLD_87",
	[248] = "WORLD_88",
	[249] = "WORLD_89",
	[250] = "WORLD_90",
	[251] = "WORLD_91",
	[252] = "WORLD_92",
	[253] = "WORLD_93",
	[254] = "WORLD_94",
	[255] = "WORLD_95",
	[256] = "KP0",
	[257] = "KP1",
	[258] = "KP2",
	[259] = "KP3",
	[260] = "KP4",
	[261] = "KP5",
	[262] = "KP6",
	[263] = "KP7",
	[264] = "KP8",
	[265] = "KP9",
	[266] = "KP_PERIOD",
	[267] = "KP_DIVIDE",
	[268] = "KP_MULTIPLY",
	[269] = "KP_MINUS",
	[270] = "KP_PLUS",
	[271] = "KP_ENTER",
	[272] = "KP_EQUALS",
	[273] = "ARROW UP",
	[274] = "ARROW DOWN",
	[275] = "ARROW RIGHT",
	[276] = "ARROW LEFT",
	[277] = "INSERT",
	[278] = "HOME",
	[279] = "END",
	[280] = "PAGEUP",
	[281] = "PAGEDOWN",
	[282] = "F1",
	[283] = "F2",
	[284] = "F3",
	[285] = "F4",
	[286] = "F5",
	[287] = "F6",
	[288] = "F7",
	[289] = "F8",
	[290] = "F9",
	[291] = "F10",
	[292] = "F11",
	[293] = "F12",
	[294] = "F13",
	[295] = "F14",
	[296] = "F15",
	[300] = "NUMLOCK",
	[301] = "CAPSLOCK",
	[302] = "SCROLLOCK",
	[303] = "RSHIFT",
	[304] = "LSHIFT",
	[305] = "RCTRL",
	[306] = "LCTRL",
	[307] = "RALT",
	[308] = "LALT",
	[309] = "RMETA",
	[310] = "LMETA",
	[311] = "LSUPER",
	[312] = "RSUPER",
	[313] = "MODE",
	[314] = "COMPOSE",
	[315] = "HELP",
	[316] = "PRINT",
	[317] = "SYSREQ",
	[318] = "BREAK",
	[319] = "MENU",
	[320] = "POWER",
	[321] = "EURO",
	[322] = "UNDO",
};

/***
 *
 *	V I D E O
 *
 */

static struct menu_display_settings {
	int fullscreen_enabled;
	enum resolution resolution;
	int water_detail;
	struct menu_item *apply_changes_item;
} menu_display_settings;

static void
set_menu_display_settings(struct menu *menu)
{
	struct list_node *ln;

	menu_display_settings.water_detail = settings.water_detail;
	menu_display_settings.fullscreen_enabled = settings.fullscreen_enabled;
	menu_display_settings.resolution = settings.resolution;

	menu_display_settings.apply_changes_item = NULL;

	for (ln = menu->items->first; ln; ln = ln->next) {
		struct menu_item *mi = (struct menu_item *)ln->data;

		if (mi->text && !strcmp(mi->text, "Apply Changes")) {
			menu_display_settings.apply_changes_item = mi;
			mi->visible = 0;
			break;
		}
	}
}

static void
menu_toggle_fullscreen(void *extra)
{
	menu_display_settings.fullscreen_enabled ^= 1;
	menu_display_settings.apply_changes_item->visible = 1;
}

static const char *
menu_get_fullscreen(void *extra)
{
	return menu_display_settings.fullscreen_enabled ? "Full screen" : "Windowed";
}

static const char *
menu_get_resolution(void *extra)
{
	static char buffer[80];
	struct resolution_info *ri = &resolution_info[menu_display_settings.resolution];
	sprintf(buffer, "%d x %d", ri->width, ri->height);
	return buffer;
}

static void
menu_toggle_resolution(void *extra)
{
	menu_display_settings.resolution = (menu_display_settings.resolution + 1)%NUM_RESOLUTIONS;
	menu_display_settings.apply_changes_item->visible = 1;
}

static void
menu_toggle_water_detail(void *extra)
{
	menu_display_settings.water_detail =
	  (menu_display_settings.water_detail + 1)%3;
	menu_display_settings.apply_changes_item->visible = 1;
}

static const char *
menu_get_water_detail(void *extra)
{
	static const char *detail_str[] = { "LOW", "MEDIUM", "HIGH" };
	return detail_str[menu_display_settings.water_detail];
}

static void
menu_apply_video_options(void *extra)
{

	if (menu_display_settings.fullscreen_enabled != settings.fullscreen_enabled ||
	  menu_display_settings.resolution != settings.resolution) {
		extern void set_video_mode();
		extern void resize_viewport();

		settings.fullscreen_enabled = menu_display_settings.fullscreen_enabled;
		settings.resolution = menu_display_settings.resolution;

		tear_down_background();
		tear_down_water();
		tear_down_bubbles_resources();
		tear_down_mesh();
		delete_textures();
		font_destroy_resources();

		set_video_mode();
		resize_viewport();

		reload_textures();
		font_initialize_resources();
		initialize_mesh();
		initialize_water();
		initialize_bubbles_resources();
		initialize_background();
	} else if (menu_display_settings.water_detail != settings.water_detail) {
		settings.water_detail = menu_display_settings.water_detail;
		tear_down_water();
		initialize_water();
	}

	menu_display_settings.apply_changes_item->visible = 0;
}

static void
add_video_options_menu(struct menu *menu)
{
	struct menu *video_options = menu_add_submenu_item(menu, "Display", .8,
	  MA_CENTER);

	menu_add_title_item(video_options, "Display Options");

	menu_add_space_item(video_options);

	menu_add_toggle_item(video_options, "Water Detail:",
	  menu_toggle_water_detail, NULL, menu_get_water_detail, NULL);

	menu_add_space_item(video_options);

	menu_add_toggle_item(video_options, "Display Resolution:",
	  menu_toggle_resolution, NULL, menu_get_resolution, NULL);
	menu_add_toggle_item(video_options, "Display mode:",
	  menu_toggle_fullscreen, NULL, menu_get_fullscreen, NULL);

	menu_add_space_item(video_options);

	menu_add_action_item(video_options, "Apply Changes", menu_apply_video_options,
	  NULL);
	menu_add_space_item(video_options);
	menu_add_previous_menu_item(video_options);

	video_options->on_activate = set_menu_display_settings;
}

/***
 *
 *	A U D I O
 *
 */
static void
add_audio_options_menu(struct menu *menu)
{
	struct menu *audio_options = menu_add_submenu_item(menu, "Sound", .8,
	  MA_CENTER);

	menu_add_title_item(audio_options, "Sound Options");
	menu_add_space_item(audio_options);
	menu_add_space_item(audio_options);
	menu_add_previous_menu_item(audio_options);
}

/***
 *
 *	C O N T R O L
 *
 */
static void
set_control_items_visibility(struct menu *menu)
{
	struct list_node *ln;
	int v;

	v = settings.control_type != CONTROL_HYBRID;

	for (ln = menu->items->first; ln; ln = ln->next) {
		struct menu_item *mi = (struct menu_item *)ln->data;

		if (mi->type == MI_DEFINE_KEY
			  && !strncmp(mi->text, "Fire", 4)) {
			mi->visible = v;
		}
	}
}

static void
menu_toggle_control_type(void *extra)
{
	settings.control_type = (settings.control_type + 1)%NUM_CONTROL_TYPES;
	set_control_items_visibility((struct menu *)extra);
}

static const char *
menu_get_control_type(void *extra)
{
	static const char *control_type_str[] =
	  { "Keyboard", "Keyboard/Mouse", "Dual Analog" };

	return control_type_str[settings.control_type];
}

static const char *
get_move_key_status(void *extra)
{
	int index = (int)extra;

	if (settings.control_type == CONTROL_JOYSTICK) {
		int stick = settings.pad_sticks[index];

		if (stick == -1) {
			return "None";
		} else {
			static char str[80];
			sprintf(str, "d-pad %d", settings.pad_sticks[index]);
			return str;
		}
	} else {
		return key_names[settings.pad_keysyms[index]];
	}
}

static int
set_move_key(int key, void *extra)
{
	if (settings.control_type != CONTROL_JOYSTICK) {
		int i;
		int index = (int)extra;

		settings.pad_keysyms[index] = key;

		for (i = 0; i < 8; i++) {
			if (index != i && settings.pad_keysyms[i] == key)
				settings.pad_keysyms[i] = 0;
		}
	}

	return 1;
}

static int
set_move_stick(int stick, void *extra)
{
	if (settings.control_type != CONTROL_JOYSTICK) {
		return 0;
	} else {
		int i;
		int index = (int)extra;

		settings.pad_sticks[index] = stick;

		for (i = 0; i < 8; i++) {
			if (index != i && settings.pad_sticks[i] == stick)
				settings.pad_sticks[i] = -1;
		}

		return 1;
	}
}

static void
add_control_options_menu(struct menu *menu)
{
	static const char *keysym_item_names[] = {
		"Move Up:", "Move Down:",
		"Move Left:", "Move Right:",
		"Fire Up:", "Fire Down:",
		"Fire Left:", "Fire Right:",
	};
	int i;
	struct menu *control_options;
	
	control_options = menu_add_submenu_item(menu, "Controls", .8,
	  MA_CENTER);

	menu_add_title_item(control_options, "Control Options");
	menu_add_space_item(control_options);

	menu_add_toggle_item(control_options, "Type:",
	  menu_toggle_control_type, control_options,
	  menu_get_control_type, NULL);

	for (i = 0; i < 8; i++) {
		if (i % 4 == 0)
			menu_add_space_item(control_options);

		menu_add_define_key_item(
		  control_options, keysym_item_names[i],
		  get_move_key_status, (void *)i,
		  set_move_key, (void *)i,
		  set_move_stick, (void *)i);
	}

	menu_add_space_item(control_options);

	menu_add_previous_menu_item(control_options);

	set_control_items_visibility(control_options);

	control_options->on_activate = set_control_items_visibility;
}

void
add_settings_menu(struct menu *menu)
{
	struct menu *options_menu = menu_add_submenu_item(menu, "Options", 1,
	  MA_CENTER);

	menu_add_title_item(options_menu, "Options");
	menu_add_space_item(options_menu);

	add_control_options_menu(options_menu);
	add_video_options_menu(options_menu);
	/* add_audio_options_menu(options_menu); */

	menu_add_space_item(options_menu);
	menu_add_previous_menu_item(options_menu);
}
