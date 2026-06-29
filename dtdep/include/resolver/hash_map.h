#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <stdint.h>

/*
 * Forward declaration — hash map stores pointers to DtNode
 * but does not need to know its layout.
 */
typedef struct DtNode DtNode;

/*
 * Fixed-size open-addressing hash map.
 * Collisions resolved by linear probing.
 *
 * Capacity is always a power of two so we can mask instead of modulo.
 * Load factor is kept under 0.7 by the caller (no auto-resize).
 */
#define HASH_MAP_CAP 256   /* must be power of two */

typedef struct {
    char    *key;          /* heap-allocated copy of the key string */
    DtNode  *value;
} HashEntry;

typedef struct {
    HashEntry entries[HASH_MAP_CAP];
    int       count;
} HashMap;

/* Initialise all slots to empty. */
void hash_map_init(HashMap *map);

/*
 * Insert key→value. Overwrites existing entry for the same key.
 * Returns 0 on success, -1 if the map is full.
 */
int hash_map_put(HashMap *map, const char *key, DtNode *value);

/*
 * Look up key. Returns the DtNode* or NULL if not found.
 */
DtNode *hash_map_get(const HashMap *map, const char *key);

/* Free all heap-allocated keys. Does not free the DtNode values. */
void hash_map_free(HashMap *map);

#endif /* HASH_MAP_H */
