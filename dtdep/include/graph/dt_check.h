#ifndef DT_CHECK_H
#define DT_CHECK_H

#include "model/dt_tree.h"
#include "resolver/resolver.h"
#include "graph/dep_engine.h"
#include "graph/dep_graph.h"

/*
 * Severity of a diagnostic finding.
 */
typedef enum {
    SEV_ERROR = 0,   /* will cause a driver probe failure        */
    SEV_WARNING,     /* may cause incorrect or degraded behavior */
} Severity;

/*
 * A single diagnostic finding.
 */
typedef struct DtFinding {
    Severity  sev;
    char      node[128];
    char      message[256];
    struct DtFinding *next;
} DtFinding;

typedef struct {
    DtFinding *head;
    int        n_errors;
    int        n_warnings;
} DtFindingList;

/*
 * dt_check_run - run all structural checks against the parsed tree,
 * resolver, dependency list, and graph. Populates @out with findings.
 */
void dt_check_run(DtFindingList *out,
                   DtTree *tree,
                   DtResolver *res,
                   DtDepList *deps,
                   DepGraph *graph);

/* Print all findings to stdout, errors first then warnings. */
void dt_check_print(const DtFindingList *list);

/* Free the finding list. */
void dt_check_free(DtFindingList *list);

#endif /* DT_CHECK_H */
