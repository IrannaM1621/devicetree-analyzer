#include "graph/dt_check.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static void add_finding(DtFindingList *list, Severity sev,
                        const char *node, const char *fmt, ...)
{
    DtFinding *f = calloc(1, sizeof(DtFinding));
    if (!f)
        return;

    f->sev = sev;
    strncpy(f->node, node, sizeof(f->node) - 1);

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(f->message, sizeof(f->message), fmt, ap);
    va_end(ap);

    f->next   = list->head;
    list->head = f;

    if (sev == SEV_ERROR)
        list->n_errors++;
    else
        list->n_warnings++;
}

static int has_prop(const DtNode *node, const char *name)
{
    for (const DtProp *p = node->props; p; p = p->next)
        if (strcmp(p->name, name) == 0)
            return 1;
    return 0;
}

static const DtProp *get_prop(const DtNode *node, const char *name)
{
    for (const DtProp *p = node->props; p; p = p->next)
        if (strcmp(p->name, name) == 0)
            return p;
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Check 1 — interrupts without interrupt-parent in ancestry            */
/* ------------------------------------------------------------------ */

static int has_interrupt_parent_in_ancestry(const DtNode *node)
{
    for (const DtNode *n = node; n; n = n->parent) {
        if (has_prop(n, "interrupt-parent"))
            return 1;
    }
    return 0;
}

static void check_interrupt_parent(DtFindingList *out, DtNode *node)
{
    if (has_prop(node, "interrupts") &&
        !has_interrupt_parent_in_ancestry(node)) {
        add_finding(out, SEV_ERROR, node->name,
            "has 'interrupts' but no 'interrupt-parent' in its ancestry "
            "— IRQ routing is undefined, irq_of_parse_and_map() will fail");
    }
    for (DtNode *c = node->children; c; c = c->next)
        check_interrupt_parent(out, c);
}

/* ------------------------------------------------------------------ */
/* Check 2 — cell-count mismatch against #xxx-cells on referenced node */
/* ------------------------------------------------------------------ */

static const char *cells_prop_for_dep(DepType type)
{
    switch (type) {
    case DEP_CLOCK:     return "#clock-cells";
    case DEP_DMA:        return "#dma-cells";
    case DEP_GPIO:       return "#gpio-cells";
    case DEP_RESET:      return "#reset-cells";
    case DEP_PHY:         return "#phy-cells";
    default:              return NULL;
    }
}

static void check_cell_counts(DtFindingList *out, const DtDepList *deps)
{
    for (const DtDep *d = deps->head; d; d = d->next) {
        const char *cprop = cells_prop_for_dep(d->type);
        if (!cprop)
            continue;

        if (!has_prop(d->target, cprop)) {
            add_finding(out, SEV_WARNING, d->source->name,
                "references '%s' (%s) but target '%s' has no '%s' — "
                "argument count cannot be validated",
                d->prop, dep_type_name(d->type), d->target->name, cprop);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Check 3 — unresolved phandle references                              */
/* ------------------------------------------------------------------ */

static void check_unresolved_phandles(DtFindingList *out, DtNode *node)
{
    for (const DtProp *p = node->props; p; p = p->next) {
        if (p->kind != PROP_CELLS)
            continue;
        for (int i = 0; i < p->ncells; i++) {
            if (p->cells[i] == 0xffffffff) {
                add_finding(out, SEV_ERROR, node->name,
                    "property '%s' has an unresolved phandle reference "
                    "(label '%s' not found) — driver will read a bogus value",
                    p->name,
                    p->phandle_refs[i][0] ? p->phandle_refs[i] : "?");
            }
        }
    }
    for (DtNode *c = node->children; c; c = c->next)
        check_unresolved_phandles(out, c);
}

/* ------------------------------------------------------------------ */
/* Check 4 — disabled nodes that are still referenced                   */
/* ------------------------------------------------------------------ */

static int is_disabled(const DtNode *node)
{
    const DtProp *p = get_prop(node, "status");
    return p && p->kind == PROP_STRING && strcmp(p->str, "disabled") == 0;
}

static void check_disabled_but_referenced(DtFindingList *out,
                                          const DtDepList *deps)
{
    for (const DtDep *d = deps->head; d; d = d->next) {
        if (is_disabled(d->target)) {
            add_finding(out, SEV_WARNING, d->source->name,
                "depends on '%s' via '%s' but that node has "
                "status = \"disabled\" — dependency will not be satisfied",
                d->target->name, d->prop);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Check 5 — orphaned phandle nodes (declared but never referenced)     */
/* ------------------------------------------------------------------ */

static int phandle_is_used(const DtNode *target, const DtDepList *deps)
{
    for (const DtDep *d = deps->head; d; d = d->next)
        if (d->target == target)
            return 1;
    return 0;
}

static void check_orphan_phandles(DtFindingList *out, DtNode *node,
                                  const DtDepList *deps)
{
    if (node->phandle != 0 && !phandle_is_used(node, deps)) {
        add_finding(out, SEV_WARNING, node->name,
            "declares phandle <0x%x> but no other node references it "
            "— possibly dead or unused", node->phandle);
    }
    for (DtNode *c = node->children; c; c = c->next)
        check_orphan_phandles(out, c, deps);
}

/* ------------------------------------------------------------------ */
/* Check 6 — dependency cycles (reuses Phase 5 detector)                */
/* ------------------------------------------------------------------ */

static void check_cycles(DtFindingList *out, DepGraph *graph)
{
    if (dep_graph_detect_cycles(graph)) {
        add_finding(out, SEV_ERROR, "(graph)",
            "dependency cycle detected — see stderr for the full cycle path. "
            "This will cause infinite recursion or deadlock during probe");
    }
}

/* ------------------------------------------------------------------ */
/* Check 7 — reg without #address-cells/#size-cells on parent           */
/* ------------------------------------------------------------------ */

static void check_reg_without_cells(DtFindingList *out, DtNode *node)
{
    if (has_prop(node, "reg") && node->parent) {
        if (!has_prop(node->parent, "#address-cells") ||
            !has_prop(node->parent, "#size-cells")) {
            add_finding(out, SEV_WARNING, node->name,
                "has 'reg' but parent '%s' is missing #address-cells or "
                "#size-cells — reg parsing may use wrong cell widths",
                node->parent->name);
        }
    }
    for (DtNode *c = node->children; c; c = c->next)
        check_reg_without_cells(out, c);
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void dt_check_run(DtFindingList *out,
                   DtTree *tree,
                   DtResolver *res,
                   DtDepList *deps,
                   DepGraph *graph)
{
    memset(out, 0, sizeof(*out));
    (void)res;

    if (!tree->root)
        return;

    check_interrupt_parent(out, tree->root);
    check_cell_counts(out, deps);
    check_unresolved_phandles(out, tree->root);
    check_disabled_but_referenced(out, deps);
    check_orphan_phandles(out, tree->root, deps);
    check_cycles(out, graph);
    check_reg_without_cells(out, tree->root);
}

void dt_check_print(const DtFindingList *list)
{
    printf("\nAnalysis (%d error%s, %d warning%s):\n",
           list->n_errors, list->n_errors == 1 ? "" : "s",
           list->n_warnings, list->n_warnings == 1 ? "" : "s");

    for (const DtFinding *f = list->head; f; f = f->next) {
        if (f->sev == SEV_ERROR)
            printf("  \u2717 %-20s %s\n", f->node, f->message);
    }
    for (const DtFinding *f = list->head; f; f = f->next) {
        if (f->sev == SEV_WARNING)
            printf("  \u26a0 %-20s %s\n", f->node, f->message);
    }

    if (list->n_errors == 0 && list->n_warnings == 0)
        printf("  \u2713 no issues found\n");
}

void dt_check_free(DtFindingList *list)
{
    DtFinding *f = list->head;
    while (f) {
        DtFinding *next = f->next;
        free(f);
        f = next;
    }
    list->head = NULL;
    list->n_errors = list->n_warnings = 0;
}
