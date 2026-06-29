#ifndef DT_NODE_H
#define DT_NODE_H

#include <stdint.h>

typedef enum {
    PROP_EMPTY = 0,
    PROP_STRING,
    PROP_CELLS,
    PROP_BYTES,
} DtPropKind;

#define DT_NAME_MAX    128
#define DT_STR_MAX     256
#define DT_CELLS_MAX   64
#define DT_BYTES_MAX   128

typedef struct DtProp {
    char            name[DT_NAME_MAX];
    DtPropKind      kind;
    struct DtProp  *next;

    union {
        char        str[DT_STR_MAX];

        struct {
            uint32_t cells[DT_CELLS_MAX];
            int      ncells;
        };

        struct {
            uint8_t  bytes[DT_BYTES_MAX];
            int      nbytes;
        };
    };
} DtProp;

typedef struct DtNode {
    char            name[DT_NAME_MAX];
    char            label[DT_NAME_MAX];
    uint32_t        phandle;

    DtProp         *props;
    struct DtNode  *children;
    struct DtNode  *next;
    struct DtNode  *parent;
} DtNode;

DtNode *dt_node_alloc(const char *name);
void    dt_node_free(DtNode *node);
void    dt_node_add_prop(DtNode *node, DtProp *prop);
void    dt_node_add_child(DtNode *node, DtNode *child);
DtProp *dt_prop_alloc(const char *name);
void    dt_node_print(const DtNode *node, int depth);

#endif /* DT_NODE_H */
