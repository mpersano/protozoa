#ifndef DICT_H_
#define DICT_H_

struct dict;

struct dict *
dict_make(void);

void
dict_put(struct dict *dict, const char *name, void *value);

void *
dict_get(struct dict *dict, const char *name);

void
dict_free(struct dict *dict, void (*free_data)(void *));

#endif /* DICT_H_ */
