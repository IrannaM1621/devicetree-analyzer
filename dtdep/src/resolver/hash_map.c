#define _POSIX_C_SOURCE 200809L
#include "resolver/hash_map.h"

#include <stdlib.h>
#include <string.h>

/* djb2 hash — fast and good enough for DTS label strings */
static uint32_t djb2(const char *s)
{
    uint32_t h = 5381;
    unsigned char c;
    while ((c = (unsigned char)*s++) != 0)
        h = ((h << 5) + h) + c;
    return h;
}

void hash_map_init(HashMap *map)
{
    memset(map, 0, sizeof(*map));
}

int hash_map_put(HashMap *map, const char *key, DtNode *value)
{
    if (map->count >= HASH_MAP_CAP * 7 / 10)
        return -1;   /* load factor exceeded */

    uint32_t idx = djb2(key) & (HASH_MAP_CAP - 1);

    /* linear probe */
    for (int i = 0; i < HASH_MAP_CAP; i++) {
        HashEntry *e = &map->entries[idx];

        if (!e->key) {
            /* empty slot — insert here */
            e->key   = strdup(key);
            e->value = value;
            map->count++;
            return 0;
        }

        if (strcmp(e->key, key) == 0) {
            /* update existing */
            e->value = value;
            return 0;
        }

        idx = (idx + 1) & (HASH_MAP_CAP - 1);
    }
    return -1;  /* full */
}

DtNode *hash_map_get(const HashMap *map, const char *key)
{
    uint32_t idx = djb2(key) & (HASH_MAP_CAP - 1);

    for (int i = 0; i < HASH_MAP_CAP; i++) {
        const HashEntry *e = &map->entries[idx];

        if (!e->key)
            return NULL;   /* empty slot — key not present */

        if (strcmp(e->key, key) == 0)
            return e->value;

        idx = (idx + 1) & (HASH_MAP_CAP - 1);
    }
    return NULL;
}

void hash_map_free(HashMap *map)
{
    for (int i = 0; i < HASH_MAP_CAP; i++) {
        if (map->entries[i].key) {
            free(map->entries[i].key);
            map->entries[i].key = NULL;
        }
    }
    map->count = 0;
}
