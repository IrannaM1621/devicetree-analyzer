#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/cli.h"
#include "model/dt_tree.h"
#include "resolver/resolver.h"
#include "graph/dep_engine.h"
#include "graph/dep_graph.h"
#include "exporters/export_dot.h"
#include "exporters/export_html.h"
#include "graph/dt_check.h"
#include "exporters/export_html.h"
#include "graph/dt_check.h"
#include "dtdep_err.h"

static int count_props(const DtNode *node)
{
    int n = 0;
    for (const DtProp *p = node->props; p; p = p->next)
        n++;
    for (const DtNode *c = node->children; c; c = c->next)
        n += count_props(c);
    return n;
}

static int count_nodes(const DtNode *node)
{
    int n = 1;
    for (const DtNode *c = node->children; c; c = c->next)
        n += count_nodes(c);
    return n;
}

static int tree_depth(const DtNode *node)
{
    int max_child = 0;
    for (const DtNode *c = node->children; c; c = c->next) {
        int d = tree_depth(c);
        if (d > max_child)
            max_child = d;
    }
    return 1 + max_child;
}

int main(int argc, char *argv[])
{
    CliOptions opts;
    if (cli_parse(argc, argv, &opts) != 0)
        return 1;

    FILE *fp = fopen(opts.file, "r");
    if (!fp) {
        perror(opts.file);
        return 1;
    }

    DtTree tree;
    int rc = dt_tree_parse(&tree, fp, opts.file);
    fclose(fp);

    if (rc != 0) {
        fprintf(stderr, "\u2717 Parse failed\n");
        return 1;
    }

    DtResolver res;
    dt_resolver_build(&res, &tree);
    dt_resolver_resolve(&res, &tree);

    DtDepList deps;
    dt_dep_analyze(&deps, &tree, &res);

    DepGraph graph;
    dep_graph_build(&graph, &tree, &deps);

    /* DOT/HTML formats short-circuit everything else — pure graph output */
    if (opts.format == FORMAT_DOT) {
        dot_export(&graph, stdout);
        dt_dep_list_free(&deps);
        dt_resolver_free(&res);
        dt_tree_free(&tree);
        return 0;
    }

    if (opts.format == FORMAT_HTML) {
        html_export(&graph, &res, stdout);
        dt_dep_list_free(&deps);
        dt_resolver_free(&res);
        dt_tree_free(&tree);
        return 0;
    }

    if (!opts.quiet) {
        printf("\u2713 Parsed successfully\n");
        printf("  Nodes       : %d\n", count_nodes(tree.root));
        printf("  Properties  : %d\n", count_props(tree.root));
        printf("  Tree depth  : %d\n", tree_depth(tree.root));
        printf("  Dependencies: %d\n", deps.count);
        if (res.unresolved)
            printf("  Unresolved  : %d phandle reference(s)\n", res.unresolved);
    }

    if (opts.show_tree)
        dt_tree_print(&tree);

    if (opts.show_deps)
        dt_dep_list_print(&deps);

    if (opts.show_graph)
        dep_graph_print(&graph);

    if (opts.check_cycles) {
        int has_cycle = dep_graph_detect_cycles(&graph);
        printf("\nCycle check: %s\n", has_cycle ? "CYCLE FOUND" : "clean");
    }

    if (opts.check) {
        DtFindingList findings;
        dt_check_run(&findings, &tree, &res, &deps, &graph);
        dt_check_print(&findings);
        dt_check_free(&findings);
    }

    if (opts.depends_on[0]) {
        DtNode *target = dt_resolver_lookup_label(&res, opts.depends_on);
        if (!target) {
            fprintf(stderr, "error: no node with label '%s'\n",
                    opts.depends_on);
        } else {
            dep_graph_reverse_deps(&graph, target, 0);
        }
    }

    dt_dep_list_free(&deps);
    dt_resolver_free(&res);
    dt_tree_free(&tree);
    return 0;
}
