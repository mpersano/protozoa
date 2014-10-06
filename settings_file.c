#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>

#include "panic.h"
#include "settings.h"

static struct name_to_offset {
	const char *name;
	int offset;
} name_to_offset[] = {
#define NAME_OFFSET(name, field) { name, offsetof(struct settings, field) },
	NAME_OFFSET("control-type", control_type)
	NAME_OFFSET("pad-keysyms-0", pad_keysyms[0])
	NAME_OFFSET("pad-keysyms-1", pad_keysyms[1])
	NAME_OFFSET("pad-keysyms-2", pad_keysyms[2])
	NAME_OFFSET("pad-keysyms-3", pad_keysyms[3])
	NAME_OFFSET("pad-keysyms-4", pad_keysyms[4])
	NAME_OFFSET("pad-keysyms-5", pad_keysyms[5])
	NAME_OFFSET("pad-keysyms-6", pad_keysyms[6])
	NAME_OFFSET("pad-keysyms-7", pad_keysyms[7])
	NAME_OFFSET("pad-sticks-0", pad_sticks[0])
	NAME_OFFSET("pad-sticks-1", pad_sticks[1])
	NAME_OFFSET("pad-sticks-2", pad_sticks[2])
	NAME_OFFSET("pad-sticks-3", pad_sticks[3])
	NAME_OFFSET("pad-sticks-4", pad_sticks[4])
	NAME_OFFSET("pad-sticks-5", pad_sticks[5])
	NAME_OFFSET("pad-sticks-6", pad_sticks[6])
	NAME_OFFSET("pad-sticks-7", pad_sticks[7])
	NAME_OFFSET("fullscreen-enabled", fullscreen_enabled)
	NAME_OFFSET("resolution", resolution)
	NAME_OFFSET("water-detail", water_detail)
	{ NULL, -1 }
};

int
load_settings(struct settings *settings, const char *file_name)
{
	static char line[256];
	FILE *in;

	if ((in = fopen(file_name, "r")) == NULL) {
		return 0;
	} else {
		while (fgets(line, sizeof line, in) != NULL) {
			char *name, *param_str, *end;
			struct name_to_offset *p;
			int param;

			name = strtok(line, " ");
			param_str = strtok(NULL, " ");

			if (!param_str)
				continue;

			param = strtol(param_str, &end, 10);

			if (end == param_str)
				continue;

			for (p = name_to_offset; p->name; p++) {
				if (!strcmp(p->name, name))
		 			*(int *)((char *)settings + p->offset) = param;
			}
		}

		fclose(in);

		return 1;
	}
}

void
save_settings(const struct settings *settings, const char *file_name)
{
	FILE *out;
	struct name_to_offset *p;

	if ((out = fopen(file_name, "w")) == NULL)
		panic("failed to open %s: %s\n", strerror(errno));

	for (p = name_to_offset; p->name; p++) {
		fprintf(out, "%s %d\n", p->name,
		  *(int *)((char *)settings + p->offset));
	}

	fclose(out);
}
