#ifndef RESOLVER_H
#define RESOLVER_H

#include "model/dt_tree.h"
#include "resolver/hash_map.h"

/*
 * DtResolver — holds the two index maps built from a parsed tree.
 *
 * @label_map   : "gcc"  → DtNode*   (from  gcc: clk@100 { ... })
 * @phandle_map : "1"    → DtNode*   (from  phandle = <1>;)
 *
 * After dt_resolver_resolve() runs, every phandle reference in the
 * tree's properties has been cross-checked and the resolver can
 * answer "which node does &gcc refer to?" in O(1).
 */
typedef struct {
    HashMap label_map;
    HashMap phandle_map;
    int     unresolved;   /* count of references that could not be resolved */
} DtResolver;

/*
 * dt_resolver_build - walk @tree and populate both index maps.
 * Must be called before dt_resolver_resolve().
 */
void dt_resolver_build(DtResolver *res, const DtTree *tree);

/*
 * dt_resolver_resolve - walk @tree properties and resolve phandle
 * sentinel values (0xffffffff) to real phandle numbers where possible.
 * Prints a warning for every unresolved reference.
 */
void dt_resolver_resolve(DtResolver *res, DtTree *tree);

/*
 * dt_resolver_lookup_label - return the node with the given label,
 * or NULL if not found.
 */
DtNode *dt_resolver_lookup_label(const DtResolver *res, const char *label);

/*
 * dt_resolver_free - release memory held by the resolver's hash maps.
 * Does not free the DtTree.
 */
void dt_resolver_free(DtResolver *res);

#endif /* RESOLVER_H */
