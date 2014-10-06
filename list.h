#ifndef LIST_H_
#define LIST_H_

struct list_node {
	struct list_node *next;
	void *data;
};

struct list {
	struct list_node *first;
	struct list_node **last;
	int length;
};

struct list *
list_make(void);

struct list *
list_append(struct list *l, void *data);

struct list *
list_prepend(struct list *l, void *data);

void
list_free(struct list *l, void(*free_data)(void *));

void *
list_element_at(const struct list *l, int n);

#endif /* LIST_H_ */
