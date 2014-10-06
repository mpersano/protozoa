#ifndef MAP_H_
#define MAP_H_

struct hash_table;

struct hash_table *
hash_table_make(int nbuckets, unsigned (*key_hash)(const void *),
  int (*key_equals)(const void *, const void *));

void
hash_table_put(struct hash_table *hash_table, void *key, void *value);

void *
hash_table_get(struct hash_table *hash_table, const void *key);

void
hash_table_free(struct hash_table *hash_table, void (*free_key)(void *), void (*free_data)(void *));

void
hash_table_map(struct hash_table *hash_table, void (*mapper)(const void *key, void *value, void *extra),
  void *extra);

int
hash_table_num_entries(struct hash_table *hash_table);

#endif /* MAP_H_ */
