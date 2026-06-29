#include "model/dt_node.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Allocation                                                           */
/* ------------------------------------------------------------------ */

DtNode *dt_node_alloc(const char *name)
{
    DtNode *n = calloc(1, sizeof(DtNode));
    if (!n)
        return NULL;
    strncpy(n->name, name, DT_NAME_MAX - 1);
    return n;
}

DtProp *dt_prop_alloc(const char *name)
{
    DtProp *p = calloc(1, sizeof(DtProp));
    if (!p)
        return NULL;
    strncpy(p->name, name, DT_NAME_MAX - 1);
    return p;
}

/* ------------------------------------------------------------------ */
/* List append helpers                                                  */
/* ------------------------------------------------------------------ */

void dt_node_add_prop(DtNode *node, DtProp *prop)
{
    if (!node->props) {
        node->props = prop;
        return;
    }
    DtProp *cur = node->props;
    while (cur->next)
        cur = cur->next;
    cur->next = prop;
}

void dt_node_add_child(DtNode *node, DtNode *child)
{
    child->parent = node;
    if (!node->children) {
        node->children = child;
        return;
    }
    DtNode *cur = node->children;
    while (cur->next)
        cur = cur->next;
    cur->next = child;
}

/* ------------------------------------------------------------------ */
/* Free                                                                 */
/* ------------------------------------------------------------------ */

void dt_node_free(DtNode *node)
{
    if (!node)
        return;

    /* free properties */
    DtProp *p = node->props;
    while (p) {
        DtProp *next = p->next;
        free(p);
        p = next;
    }

    /* free children recursively */
    DtNode *c = node->children;
    while (c) {
        DtNode *next = c->next;
        dt_node_free(c);
        c = next;
    }

    free(node);
}

/* ------------------------------------------------------------------ */
/* Print                                                                */
/* ------------------------------------------------------------------ */

static void print_indent(int depth)
{
    for (int i = 0; i < depth * 4; i++)
        putchar(' ');
}

static void dt_prop_print(const DtProp *p, int depth)
{
    print_indent(depth);
    printf("%s", p->name);

    switch (p->kind) {
    case PROP_EMPTY:
        printf(";");
        break;

    case PROP_STRING:
        printf(" = \"%s\";", p->str);
        break;

    case PROP_CELLS:
        printf(" = <");
        for (int i = 0; i < p->ncells; i++) {
            if (i) printf(" ");
            printf("0x%x", p->cells[i]);
        }
        printf(">;");
        break;

    case PROP_BYTES:
        printf(" = [");
        for (int i = 0; i < p->nbytes; i++) {
            if (i) printf(" ");
            printf("%02x", p->bytes[i]);
        }
        printf("];");
        break;
    }
    printf("\n");
}

void dt_node_print(const DtNode *node, int depth)
{
    if (!node)
        return;

    print_indent(depth);

    if (node->label[0])
        printf("%s: ", node->label);

    printf("%s {\n", node->name);

    /* properties */
    for (const DtProp *p = node->props; p; p = p->next)
        dt_prop_print(p, depth + 1);

    /* children */
    for (const DtNode *c = node->children; c; c = c->next)
        dt_node_print(c, depth + 1);

    print_indent(depth);
    printf("};\n");
}
