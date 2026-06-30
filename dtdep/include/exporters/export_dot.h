#ifndef EXPORT_DOT_H
#define EXPORT_DOT_H

#include <stdio.h>
#include "graph/dep_graph.h"

/*
 * dot_export - write @g as a Graphviz DOT graph to @out.
 *
 * Nodes are colored by inferred category (clock, interrupt-controller,
 * dma, bus, generic). Edges are labeled with their dependency type.
 */
void dot_export(const DepGraph *g, FILE *out);

#endif /* EXPORT_DOT_H */
