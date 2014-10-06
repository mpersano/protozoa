#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "hash_table.h"
#include "dict.h"

struct dict;

enum {
	NBUCKETS = 331
};

static unsigned
str_hash(const char *str)
{
	unsigned h = 0;

	while (*str)
		h = (h << 4) + *(const unsigned char *)str++;

	return h;
}

static int
str_equals(const char *p, const char *q)
{
	return strcmp(p, q) == 0;
}

struct dict *
dict_make(void)
{
	return (struct dict *)hash_table_make(NBUCKETS,
	  (unsigned(*)(const void *))str_hash,
	  (int(*)(const void *, const void *))str_equals);
}

void
dict_put(struct dict *pt, const char *key, void *value)
{
	hash_table_put((struct hash_table *)pt, strdup(key), value);
}

void *
dict_get(struct dict *pt, const char *key)
{
	return hash_table_get((struct hash_table *)pt, key);
}

void
dict_free(struct dict *pt, void(*free_data)(void *))
{
	hash_table_free((struct hash_table *)pt, free, free_data);
}
