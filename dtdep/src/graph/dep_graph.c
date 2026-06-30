#include "graph/dep_graph.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Build                                                                */
/* ------------------------------------------------------------------ */

/* Recursively register every DtNode as a GraphNode (no edges yet). */
static void register_nodes(DepGraph *g, DtNode *node)
{
    if (g->nnodes >= GRAPH_MAX_NODES) {
        fprintf(stderr, "warning: graph node limit (%d) exceeded\n",
                GRAPH_MAX_NODES);
        return;
    }

    GraphNode *gn = &g->nodes[g->nnodes++];
    gn->dt_node     = node;
    gn->nedges      = 0;
    gn->visit_state = 0;

    for (DtNode *c = node->children; c; c = c->next)
        register_nodes(g, c);
}

GraphNode *dep_graph_find(DepGraph *g, const DtNode *dt_node)
{
    for (int i = 0; i < g->nnodes; i++) {
        if (g->nodes[i].dt_node == dt_node)
            return &g->nodes[i];
    }
    return NULL;
}

void dep_graph_build(DepGraph *g, DtTree *tree, const DtDepList *deps)
{
    memset(g, 0, sizeof(*g));

    if (tree->root)
        register_nodes(g, tree->root);

    /* add edges from the dependency list */
    for (const DtDep *d = deps->head; d; d = d->next) {
        GraphNode *src = dep_graph_find(g, d->source);
        if (!src)
            continue;

        if (src->nedges >= GRAPH_MAX_EDGES) {
            fprintf(stderr,
                "warning: %s exceeds max edges (%d), dropping edge to %s\n",
                d->source->name, GRAPH_MAX_EDGES, d->target->name);
            continue;
        }

        src->edges[src->nedges]      = d->target;
        src->edge_types[src->nedges] = d->type;
        src->nedges++;
    }
}

/* ------------------------------------------------------------------ */
/* Cycle detection — classic DFS white/gray/black                      */
/* ------------------------------------------------------------------ */

#define COLOR_WHITE 0
#define COLOR_GRAY  1
#define COLOR_BLACK 2

static int dfs_path[GRAPH_MAX_NODES];
static int dfs_path_len;

static int dfs_visit(DepGraph *g, GraphNode *gn)
{
    gn->visit_state = COLOR_GRAY;
    dfs_path[dfs_path_len++] = (int)(gn - g->nodes);

    for (int i = 0; i < gn->nedges; i++) {
        GraphNode *target = dep_graph_find(g, gn->edges[i]);
        if (!target)
            continue;

        if (target->visit_state == COLOR_GRAY) {
            /* back edge found — cycle! print the path */
            fprintf(stderr, "cycle detected:\n");
            int start = 0;
            for (int j = 0; j < dfs_path_len; j++) {
                if (g->nodes[dfs_path[j]].dt_node == target->dt_node) {
                    start = j;
                    break;
                }
            }
            for (int j = start; j < dfs_path_len; j++)
                fprintf(stderr, "  %s ->\n",
                        g->nodes[dfs_path[j]].dt_node->name);
            fprintf(stderr, "  %s  (back to start)\n",
                    target->dt_node->name);
            return 1;
        }

        if (target->visit_state == COLOR_WHITE) {
            if (dfs_visit(g, target))
                return 1;
        }
    }

    gn->visit_state = COLOR_BLACK;
    dfs_path_len--;
    return 0;
}

int dep_graph_detect_cycles(DepGraph *g)
{
    for (int i = 0; i < g->nnodes; i++)
        g->nodes[i].visit_state = COLOR_WHITE;

    for (int i = 0; i < g->nnodes; i++) {
        if (g->nodes[i].visit_state == COLOR_WHITE) {
            dfs_path_len = 0;
            if (dfs_visit(g, &g->nodes[i]))
                return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Reverse dependency lookup                                            */
/* ------------------------------------------------------------------ */

void dep_graph_reverse_deps(DepGraph *g, const DtNode *target, int direct_only)
{
    printf("\nNodes depending on '%s':\n", target->name);

    int found = 0;
    for (int i = 0; i < g->nnodes; i++) {
        GraphNode *gn = &g->nodes[i];
        for (int j = 0; j < gn->nedges; j++) {
            if (gn->edges[j] == target) {
                printf("  %-20s  (%s)\n",
                       gn->dt_node->name, dep_type_name(gn->edge_types[j]));
                found = 1;
                break;
            }
        }
    }

    (void)direct_only; /* transitive search reserved for Phase 9 */

    if (!found)
        printf("  (none)\n");
}

/* ------------------------------------------------------------------ */
/* Print                                                                */
/* ------------------------------------------------------------------ */

void dep_graph_print(const DepGraph *g)
{
    printf("\nGraph (%d nodes):\n", g->nnodes);
    for (int i = 0; i < g->nnodes; i++) {
        const GraphNode *gn = &g->nodes[i];
        if (gn->nedges == 0)
            continue;
        printf("  %-20s -> ", gn->dt_node->name);
        for (int j = 0; j < gn->nedges; j++) {
            printf("%s", gn->edges[j]->name);
            if (j < gn->nedges - 1)
                printf(", ");
        }
        printf("\n");
    }
}
