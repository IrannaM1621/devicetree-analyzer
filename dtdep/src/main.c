#include <stdio.h>
#include <stdlib.h>

#include "model/dt_tree.h"
#include "resolver/resolver.h"
#include "dtdep_err.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <file.dts>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        perror(argv[1]);
        return 1;
    }

    /* Phase 2 — parse */
    DtTree tree;
    int rc = dt_tree_parse(&tree, fp, argv[1]);
    fclose(fp);
    if (rc != 0) {
        fprintf(stderr, "parse failed\n");
        return 1;
    }

    /* Phase 3 — resolve */
    DtResolver res;
    dt_resolver_build(&res, &tree);
    dt_resolver_resolve(&res, &tree);

    if (res.unresolved)
        fprintf(stderr, "warning: %d unresolved phandle reference(s)\n",
                res.unresolved);

    /* print resolved tree */
    dt_tree_print(&tree);

    dt_resolver_free(&res);
    dt_tree_free(&tree);
    return 0;
}
