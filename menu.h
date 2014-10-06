#ifndef MENU_H_
#define MENU_H_
struct list;

enum menu_item_type {
	MI_SUBMENU,
	MI_ACTION,
	MI_TOGGLE,
	MI_SPACE,
	MI_TITLE,
	MI_DEFINE_KEY,
};

enum menu_align {
	MA_LEFT, MA_CENTER
};

struct menu {
	char *name;
	const struct font_render_info *fri;
	int max_text_width;
	float item_height;
	struct list *items;
	struct menu_item *hilite;
	struct menu *parent;
	struct menu *cur_submenu;
	float x, y;
	float scale;
	int align;
	int defining_key;
	void (*on_activate)(struct menu *);
	float spread; /* don't ask. */
};

struct menu_item {
	enum menu_item_type type;
	char *text;

	int visible;

	struct menu *parent;

	float hilite_scale;

	/* if toggle or action */
	void (*on_action)(void *extra_on_action);
	void *extra_on_action;

	/* if toggle/define key */
	const char *(*get_status)(void *extra_get_status);
	void *extra_get_status;

	/* if define_key */
	int (*set_key)(int key, void *extra_set_key);
	void *extra_set_key;

	int (*set_stick)(int stick, void *extra_set_stick);
	void *extra_set_stick;

	/* if submenu */
	struct menu *submenu;
};

struct font_render_info;

struct menu *
menu_make(const char *name, const struct font_render_info *fri, float scale,
  int align, float x, float y);

void
menu_free(struct menu *menu);

struct menu *
menu_add_submenu_item(struct menu *menu, const char *name, float scale,
  int align);

void
menu_add_action_item(struct menu *menu, const char *text,
  void(*on_action)(void *extra), void *extra);

void
menu_add_previous_menu_item(struct menu *menu);

void
menu_add_space_item(struct menu *menu);

void
menu_add_title_item(struct menu *menu, const char *name);

void
menu_add_toggle_item(struct menu *menu, const char *text,
  void(*on_action)(void *extra), void *extra_on_action,
  const char *(*get_toggle_status)(void *extra_on_get_toggle),
  void *extra_on_get_toggle);

void
menu_add_define_key_item(struct menu *menu, const char *text,
  const char *(*get_key_status)(void *), void *extra_get_key_status,
  int(*set_key)(int, void *), void *extra_set_key,
  int(*set_stick)(int, void *), void *extra_set_stick);

void
menu_render(const struct menu *menu);

void
menu_update(struct menu *menu);

int
menu_on_mouse_motion(struct menu *menu, int x, int y);

int
menu_on_mouse_click(struct menu *menu, int x, int y);

void
menu_on_hide(struct menu *menu);

void
menu_on_show(struct menu *menu);

void
menu_reset(struct menu *menu);

void
menu_on_key_pressed(struct menu *menu, int key);

void
menu_on_stick_pressed(struct menu *menu, int stick);

int
menu_is_on_top_level(struct menu *menu);

#endif /* MENU_H_ */
