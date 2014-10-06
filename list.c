#include <stdlib.h>
#include "list.h"

struct list *
list_make(void)
{
	struct list *l = malloc(sizeof *l);

	l->first = NULL;
	l->last = &l->first;
	l->length = 0;

	return l;
}

static struct list_node *
list_node_make(void *data)
{
	struct list_node *ln = malloc(sizeof *ln);

	ln->data = data;
	ln->next = NULL;

	return ln;
}

struct list *
list_append(struct list *l, void *data)
{
	struct list_node *ln = list_node_make(data);

	*l->last = ln;
	l->last = &ln->next;
	++l->length;

	return l;
}

struct list *
list_prepend(struct list *l, void *data)
{
	struct list_node *ln = list_node_make(data);

	if ((ln->next = l->first) == NULL)
		l->last = &ln->next;

	l->first = ln;
	++l->length;

	return l;
}

void
list_free(struct list *l, void (*free_data)(void *))
{
	struct list_node *ln, *next;

	ln = l->first;

	while (ln) {
		next = ln->next;

		if (free_data)
			free_data(ln->data);

		free(ln);

		ln = next;
	}

	free(l);
}

void *
list_element_at(const struct list *l, int n)
{
	struct list_node *ln;

	ln = l->first;

	while (n-- && ln)
		ln = ln->next;

	return ln == NULL ? NULL : ln->data;
}
