#ifndef DEP_GRAPH_H
#define DEP_GRAPH_H

#include "model/dt_node.h"
#include "graph/dep_engine.h"

/*
 * Adjacency-list graph built from a DtDepList.
 *
 * Each GraphNode wraps one DtNode and owns a dynamic array of edges
 * pointing at the GraphNodes it depends on.
 */
#define GRAPH_MAX_NODES   512
#define GRAPH_MAX_EDGES   16   /* edges per node */

typedef struct {
    DtNode *dt_node;                      /* the underlying device tree node */
    DtNode *edges[GRAPH_MAX_EDGES];        /* nodes this one depends on       */
    DepType edge_types[GRAPH_MAX_EDGES];   /* dependency type for each edge   */
    int     nedges;

    /* DFS bookkeeping for cycle detection — 0=white 1=gray 2=black */
    int     visit_state;
} GraphNode;

typedef struct {
    GraphNode nodes[GRAPH_MAX_NODES];
    int       nnodes;
} DepGraph;

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/* Build a graph from the tree (collects every node) and the dep list. */
void dep_graph_build(DepGraph *g, DtTree *tree, const DtDepList *deps);

/* Find the GraphNode wrapping a given DtNode. Returns NULL if absent. */
GraphNode *dep_graph_find(DepGraph *g, const DtNode *dt_node);

/*
 * dep_graph_detect_cycles - run DFS cycle detection over the whole graph.
 * Returns 1 if a cycle was found (and prints the cycle path), 0 if acyclic.
 */
int dep_graph_detect_cycles(DepGraph *g);

/*
 * dep_graph_reverse_deps - print every node that (transitively or
 * directly) depends on @target.
 */
void dep_graph_reverse_deps(DepGraph *g, const DtNode *target, int direct_only);

/* Print the full adjacency list. */
void dep_graph_print(const DepGraph *g);

#endif /* DEP_GRAPH_H */
