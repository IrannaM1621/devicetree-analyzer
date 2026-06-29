#ifndef DEP_ENGINE_H
#define DEP_ENGINE_H

#include "model/dt_node.h"
#include "resolver/resolver.h"

/*
 * Dependency types — what kind of hardware relationship this edge represents.
 * Each maps to a well-known DTS property name.
 */
typedef enum {
    DEP_UNKNOWN   = 0,
    DEP_CLOCK,        /* clocks = <&gcc 5>;              */
    DEP_INTERRUPT,    /* interrupts, interrupt-parent     */
    DEP_DMA,          /* dmas = <&dma 1>;                */
    DEP_POWER,        /* vdd-supply, power-domains        */
    DEP_GPIO,         /* gpios, reset-gpios, cs-gpios     */
    DEP_RESET,        /* resets = <&rst 3>;               */
    DEP_PINCTRL,      /* pinctrl-0, pinctrl-1             */
    DEP_BUS,          /* parent is a simple-bus           */
    DEP_PHY,          /* phys = <&usb_phy>;               */
} DepType;

/*
 * A single directed dependency edge.
 *
 * @source   : the node that has the dependency
 * @target   : the node being depended on
 * @type     : what kind of dependency this is
 * @prop     : property name that caused this dependency
 */
typedef struct DtDep {
    DtNode     *source;
    DtNode     *target;
    DepType     type;
    char        prop[64];
    struct DtDep *next;   /* intrusive linked list */
} DtDep;

/*
 * DtDepList — the complete set of dependencies extracted from a tree.
 *
 * @head   : first dependency in the list
 * @count  : total number of dependencies
 */
typedef struct {
    DtDep *head;
    int    count;
} DtDepList;

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * dt_dep_analyze - walk @tree, resolve references via @res, and
 * populate @list with all found dependencies.
 */
void dt_dep_analyze(DtDepList *list, DtTree *tree, DtResolver *res);

/* Free all DtDep objects in the list. */
void dt_dep_list_free(DtDepList *list);

/* Print all dependencies to stdout. */
void dt_dep_list_print(const DtDepList *list);

/* Return a human-readable name for a DepType. */
const char *dep_type_name(DepType type);

#endif /* DEP_ENGINE_H */
