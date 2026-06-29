#ifndef DT_TREE_H
#define DT_TREE_H

#include "model/dt_node.h"
#include "parser/dts_lexer.h"

typedef struct {
    DtNode *root;
    char    src_file[256];
} DtTree;

int  dt_tree_parse(DtTree *tree, FILE *fp, const char *src);
void dt_tree_free(DtTree *tree);
void dt_tree_print(const DtTree *tree);

#endif /* DT_TREE_H */
