#include "exporters/export_dot.h"

#include <string.h>

static void print_quoted(FILE *out, const char *name)
{
    fputc('"', out);
    for (const char *c = name; *c; c++) {
        if (*c == '"')
            fputc('\\', out);
        fputc(*c, out);
    }
    fputc('"', out);
}

/*
 * Category fill/stroke pairs — soft pastel fills with a darker
 * matching stroke, instead of harsh saturated X11 color names.
 */
static int node_palette(const DtNode *node, const char **fill,
                         const char **stroke, const char **style)
{
    for (const DtProp *p = node->props; p; p = p->next) {
        if (strcmp(p->name, "#clock-cells") == 0) {
            *fill = "#E3F2FD"; *stroke = "#1565C0"; *style = "filled,rounded";
            return 1;
        }
        if (strcmp(p->name, "interrupt-controller") == 0) {
            *fill = "#FDECEA"; *stroke = "#C62828"; *style = "filled,rounded";
            return 1;
        }
        if (strcmp(p->name, "#dma-cells") == 0) {
            *fill = "#E8F5E9"; *stroke = "#2E7D32"; *style = "filled,rounded";
            return 1;
        }
        if (p->kind == PROP_STRING &&
            strcmp(p->name, "compatible") == 0 &&
            strstr(p->str, "simple-bus") != NULL) {
            *fill = "#F2F2F2"; *stroke = "#757575"; *style = "filled,dashed";
            return 1;
        }
    }
    *fill = "#FDF6E3"; *stroke = "#B58900"; *style = "filled,rounded";
    return 0;
}

static const char *edge_color(DepType type)
{
    switch (type) {
    case DEP_CLOCK:     return "#1565C0";
    case DEP_INTERRUPT: return "#C62828";
    case DEP_DMA:       return "#2E7D32";
    case DEP_BUS:       return "#9E9E9E";
    case DEP_POWER:     return "#EF6C00";
    case DEP_GPIO:      return "#6A1B9A";
    case DEP_RESET:     return "#5D4037";
    case DEP_PHY:       return "#00695C";
    default:            return "#424242";
    }
}

static const char *edge_label(DepType type)
{
    switch (type) {
    case DEP_CLOCK:     return "clock";
    case DEP_INTERRUPT: return "interrupt";
    case DEP_DMA:       return "dma";
    case DEP_BUS:       return "bus";
    case DEP_POWER:     return "power";
    case DEP_GPIO:      return "gpio";
    case DEP_RESET:     return "reset";
    case DEP_PHY:       return "phy";
    default:            return "dep";
    }
}

void dot_export(const DepGraph *g, FILE *out)
{
    fprintf(out, "digraph dtdep {\n");
    fprintf(out, "  rankdir=LR;\n");
    fprintf(out, "  bgcolor=\"white\";\n");
    fprintf(out, "  pad=\"0.3\";\n");
    fprintf(out, "  nodesep=\"0.45\";\n");
    fprintf(out, "  ranksep=\"0.9 equally\";\n");
    fprintf(out, "  splines=spline;\n\n");
    fprintf(out, "  graph [fontname=\"Helvetica\", fontsize=11, fontcolor=\"#444444\"];\n");
    fprintf(out, "  node  [shape=box, fontname=\"Helvetica\", fontsize=11, "
                 "penwidth=1, margin=\"0.18,0.10\"];\n");
    fprintf(out, "  edge  [fontname=\"Helvetica\", fontsize=9, fontcolor=\"#555555\", "
                 "penwidth=1.3, arrowsize=0.8];\n\n");

    /* skip the bare root '/' when it has no edges — pure clutter */
    for (int i = 0; i < g->nnodes; i++) {
        const GraphNode *gn = &g->nodes[i];
        if (strcmp(gn->dt_node->name, "/") == 0 && gn->nedges == 0)
            continue;

        const char *fill, *stroke, *style;
        node_palette(gn->dt_node, &fill, &stroke, &style);

        print_quoted(out, gn->dt_node->name);
        fprintf(out, " [fillcolor=\"%s\", color=\"%s\", style=\"%s\"];\n",
                fill, stroke, style);
    }

    fprintf(out, "\n");

    for (int i = 0; i < g->nnodes; i++) {
        const GraphNode *gn = &g->nodes[i];
        for (int j = 0; j < gn->nedges; j++) {
            const char *color = edge_color(gn->edge_types[j]);
            int is_bus = (gn->edge_types[j] == DEP_BUS);

            print_quoted(out, gn->dt_node->name);
            fprintf(out, " -> ");
            print_quoted(out, gn->edges[j]->name);
            fprintf(out, " [label=\"%s\", color=\"%s\"%s];\n",
                    edge_label(gn->edge_types[j]), color,
                    is_bus ? ", style=dashed" : "");
        }
    }

    fprintf(out, "}\n");
}
