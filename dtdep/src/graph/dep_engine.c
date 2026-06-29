#include "graph/dep_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static DepType prop_to_dep_type(const char *name)
{
    if (strcmp(name, "clocks") == 0)              return DEP_CLOCK;
    if (strcmp(name, "interrupt-parent") == 0)    return DEP_INTERRUPT;
    if (strcmp(name, "interrupts-extended") == 0) return DEP_INTERRUPT;
    if (strcmp(name, "dmas") == 0)                return DEP_DMA;
    if (strstr(name, "-supply") != NULL)          return DEP_POWER;
    if (strcmp(name, "power-domains") == 0)       return DEP_POWER;
    if (strstr(name, "gpios") != NULL)            return DEP_GPIO;
    if (strstr(name, "-gpio") != NULL)            return DEP_GPIO;
    if (strcmp(name, "resets") == 0)              return DEP_RESET;
    if (strncmp(name, "pinctrl-", 8) == 0 &&
        name[8] >= '0' && name[8] <= '9')         return DEP_PINCTRL;
    if (strcmp(name, "phys") == 0)                return DEP_PHY;
    if (strcmp(name, "phy-handle") == 0)          return DEP_PHY;
    return DEP_UNKNOWN;
}

/*
 * Map a DepType to the #xxx-cells property name on the target node.
 * This tells us how many argument cells follow each phandle.
 */
static const char *cells_prop_for_type(DepType type)
{
    switch (type) {
    case DEP_CLOCK:     return "#clock-cells";
    case DEP_INTERRUPT: return "#interrupt-cells";
    case DEP_DMA:       return "#dma-cells";
    case DEP_POWER:     return "#power-domain-cells";
    case DEP_GPIO:      return "#gpio-cells";
    case DEP_RESET:     return "#reset-cells";
    case DEP_PHY:       return "#phy-cells";
    default:            return NULL;
    }
}

/*
 * Read the #xxx-cells value from a target node.
 * Returns 0 if not found (treat as 0 args after phandle).
 */
static int read_cells_prop(const DtNode *node, const char *prop_name)
{
    if (!prop_name)
        return 0;
    for (const DtProp *p = node->props; p; p = p->next) {
        if (strcmp(p->name, prop_name) == 0 &&
            p->kind == PROP_CELLS && p->ncells == 1)
            return (int)p->cells[0];
    }
    return 0;
}

static void dep_add(DtDepList *list,
                    DtNode *source, DtNode *target,
                    DepType type, const char *prop)
{
    DtDep *dep = calloc(1, sizeof(DtDep));
    if (!dep)
        return;
    dep->source = source;
    dep->target = target;
    dep->type   = type;
    strncpy(dep->prop, prop, sizeof(dep->prop) - 1);
    dep->next   = list->head;
    list->head  = dep;
    list->count++;
}

/* ------------------------------------------------------------------ */
/* Per-node analysis                                                    */
/* ------------------------------------------------------------------ */

static void analyze_node(DtDepList *list, DtNode *node, DtResolver *res)
{
    for (DtProp *p = node->props; p; p = p->next) {
        DepType type = prop_to_dep_type(p->name);
        if (type == DEP_UNKNOWN)
            continue;

        if (p->kind != PROP_CELLS)
            continue;

        /*
         * Walk the cell array treating it as:
         *   [ phandle  arg0 arg1 ... argN  phandle  arg0 ... ]
         *
         * For each phandle cell:
         *   1. resolve it to a target node
         *   2. read #xxx-cells from target to know how many args follow
         *   3. skip those arg cells
         *   4. emit one dependency edge
         *
         * interrupt-parent is a single bare phandle with no args.
         */
        int i = 0;
        while (i < p->ncells) {
            uint32_t val = p->cells[i];
            i++;

            if (val == 0xffffffff) {
                /* unresolved — skip, no way to know arg count */
                continue;
            }

            char key[32];
            snprintf(key, sizeof(key), "%u", val);
            DtNode *target = hash_map_get(&res->phandle_map, key);

            if (target && target != node) {
                dep_add(list, node, target, type, p->name);

                /* skip argument cells for this phandle */
                const char *cprop = cells_prop_for_type(type);
                int nargs = read_cells_prop(target, cprop);
                i += nargs;
            }
        }
    }

    /* bus parent dependency */
    if (node->parent) {
        for (DtProp *p = node->parent->props; p; p = p->next) {
            if (p->kind == PROP_STRING &&
                strcmp(p->name, "compatible") == 0 &&
                strstr(p->str, "simple-bus") != NULL) {
                dep_add(list, node, node->parent, DEP_BUS, "parent");
                break;
            }
        }
    }

    for (DtNode *c = node->children; c; c = c->next)
        analyze_node(list, c, res);
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void dt_dep_analyze(DtDepList *list, DtTree *tree, DtResolver *res)
{
    memset(list, 0, sizeof(*list));
    if (tree->root)
        analyze_node(list, tree->root, res);
}

void dt_dep_list_free(DtDepList *list)
{
    DtDep *d = list->head;
    while (d) {
        DtDep *next = d->next;
        free(d);
        d = next;
    }
    list->head  = NULL;
    list->count = 0;
}

void dt_dep_list_print(const DtDepList *list)
{
    printf("\nDependencies (%d):\n", list->count);
    printf("%-20s  %-10s  %-20s  %s\n",
           "source", "type", "target", "property");
    printf("%-20s  %-10s  %-20s  %s\n",
           "------", "----", "------", "--------");
    for (const DtDep *d = list->head; d; d = d->next) {
        printf("%-20s  %-10s  %-20s  %s\n",
               d->source->name,
               dep_type_name(d->type),
               d->target->name,
               d->prop);
    }
}

const char *dep_type_name(DepType type)
{
    switch (type) {
    case DEP_CLOCK:     return "CLOCK";
    case DEP_INTERRUPT: return "INTERRUPT";
    case DEP_DMA:       return "DMA";
    case DEP_POWER:     return "POWER";
    case DEP_GPIO:      return "GPIO";
    case DEP_RESET:     return "RESET";
    case DEP_PINCTRL:   return "PINCTRL";
    case DEP_BUS:       return "BUS";
    case DEP_PHY:       return "PHY";
    default:            return "UNKNOWN";
    }
}
