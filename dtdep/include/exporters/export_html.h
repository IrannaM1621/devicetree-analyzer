#ifndef EXPORT_HTML_H
#define EXPORT_HTML_H

#include <stdio.h>
#include "graph/dep_graph.h"
#include "resolver/resolver.h"

/*
 * html_export - write @g as a self-contained, interactive HTML file to @out.
 *
 * The output embeds node/edge data as inline JSON and a small vanilla-JS
 * force-directed layout + click-to-inspect panel. No external assets,
 * no network requests, no Graphviz dependency at runtime.
 */
void html_export(const DepGraph *g, const DtResolver *res, FILE *out);

#endif /* EXPORT_HTML_H */
