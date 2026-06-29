#define _POSIX_C_SOURCE 200809L

#include "resolver/resolver.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Pass 1 — walk tree and build label + phandle index maps             */
/* ------------------------------------------------------------------ */

static void index_node(DtResolver *res, DtNode *node)
{
    /* index by label */
    if (node->label[0] != '\0')
        hash_map_put(&res->label_map, node->label, node);

    /* check properties for phandle / linux,phandle declarations */
    for (DtProp *p = node->props; p; p = p->next) {
        if (p->kind != PROP_CELLS || p->ncells != 1)
            continue;
        if (strcmp(p->name, "phandle") == 0 ||
            strcmp(p->name, "linux,phandle") == 0) {
            node->phandle = p->cells[0];
            char key[32];
            snprintf(key, sizeof(key), "%u", node->phandle);
            hash_map_put(&res->phandle_map, key, node);
        }
    }

    for (DtNode *c = node->children; c; c = c->next)
        index_node(res, c);
}

void dt_resolver_build(DtResolver *res, const DtTree *tree)
{
    memset(res, 0, sizeof(*res));
    hash_map_init(&res->label_map);
    hash_map_init(&res->phandle_map);

    if (tree->root)
        index_node(res, tree->root);
}

/* ------------------------------------------------------------------ */
/* Pass 2 — resolve sentinel 0xffffffff cells to real phandle numbers  */
/* ------------------------------------------------------------------ */

static void resolve_node(DtResolver *res, DtNode *node)
{
    for (DtProp *p = node->props; p; p = p->next) {
        if (p->kind != PROP_CELLS)
            continue;

        for (int i = 0; i < p->ncells; i++) {
            if (p->cells[i] != 0xffffffff)
                continue;

            if (p->phandle_refs[i][0] != '\0') {
                DtNode *target =
                    hash_map_get(&res->label_map, p->phandle_refs[i]);
                if (target && target->phandle != 0) {
                    p->cells[i] = target->phandle;
                } else {
                    fprintf(stderr,
                        "warning: %s: property '%s': "
                        "unresolved phandle reference &%s\n",
                        node->name, p->name, p->phandle_refs[i]);
                    res->unresolved++;
                }
            } else {
                fprintf(stderr,
                    "warning: %s: property '%s': "
                    "phandle sentinel at cell[%d] has no label\n",
                    node->name, p->name, i);
                res->unresolved++;
            }
        }
    }

    for (DtNode *c = node->children; c; c = c->next)
        resolve_node(res, c);
}

void dt_resolver_resolve(DtResolver *res, DtTree *tree)
{
    if (tree->root)
        resolve_node(res, tree->root);
}

DtNode *dt_resolver_lookup_label(const DtResolver *res, const char *label)
{
    return hash_map_get(&res->label_map, label);
}

void dt_resolver_free(DtResolver *res)
{
    hash_map_free(&res->label_map);
    hash_map_free(&res->phandle_map);
}
