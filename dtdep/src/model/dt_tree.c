#include "model/dt_tree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Parser state                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    DtsLexer  lex;
    DtsToken  cur;       /* current (already consumed) token  */
    DtsToken  peek;      /* one token of lookahead            */
    const char *src;
    int        errors;
} Parser;

static void parser_error(Parser *p, const char *msg)
{
    fprintf(stderr, "%s:%d:%d: error: %s (got '%s')\n",
            p->src, p->cur.line, p->cur.col, msg, p->cur.text);
    p->errors++;
}

/* Advance: peek becomes cur, next token becomes peek. */
static void advance(Parser *p)
{
    p->cur  = p->peek;
    p->peek = dts_lexer_next(&p->lex);
}

/* Consume cur if it matches @type, else emit error. */
static int expect(Parser *p, DtsTokenType type)
{
    if (p->cur.type != type) {
        fprintf(stderr, "%s:%d: expected %s got %s ('%s')\n",
                p->src, p->cur.line,
                dts_token_type_name(type),
                dts_token_type_name(p->cur.type),
                p->cur.text);
        p->errors++;
        return 0;
    }
    advance(p);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Property value parsers                                               */
/* ------------------------------------------------------------------ */

/*
 * Parse a cell list:  < val val &ref val >
 * Stores raw uint32 values; phandle references stored as 0xffffffff
 * (resolver will fix them up in Phase 3).
 */
static DtProp *parse_cells(Parser *p, const char *name)
{
    DtProp *prop = dt_prop_alloc(name);
    if (!prop) return NULL;

    prop->kind   = PROP_CELLS;
    prop->ncells = 0;

    advance(p); /* consume '<' */

    while (p->cur.type != TOK_RANGLE && p->cur.type != TOK_EOF) {
        if (p->cur.type == TOK_AMPERSAND) {
            /* phandle reference: &label — store sentinel, save name later */
            advance(p); /* consume '&' */
            if (prop->ncells < DT_CELLS_MAX)
                prop->cells[prop->ncells++] = 0xffffffff;
            advance(p); /* consume label ident */
        } else if (p->cur.type == TOK_NUMBER) {
            if (prop->ncells < DT_CELLS_MAX)
                prop->cells[prop->ncells++] = (uint32_t)p->cur.numval;
            advance(p);
        } else {
            parser_error(p, "unexpected token in cell list");
            advance(p);
        }
    }
    expect(p, TOK_RANGLE);
    return prop;
}

/* Parse a byte array:  [ 00 11 22 ] */
static DtProp *parse_bytes(Parser *p, const char *name)
{
    DtProp *prop = dt_prop_alloc(name);
    if (!prop) return NULL;

    prop->kind   = PROP_BYTES;
    prop->nbytes = 0;

    advance(p); /* consume '[' */

    while (p->cur.type != TOK_RBRACKET && p->cur.type != TOK_EOF) {
        if (p->cur.type == TOK_NUMBER) {
            if (prop->nbytes < DT_BYTES_MAX)
                prop->bytes[prop->nbytes++] = (uint8_t)p->cur.numval;
            advance(p);
        } else {
            parser_error(p, "unexpected token in byte array");
            advance(p);
        }
    }
    expect(p, TOK_RBRACKET);
    return prop;
}

/* ------------------------------------------------------------------ */
/* Property parser                                                      */
/* ------------------------------------------------------------------ */

/*
 * Parse one property.
 *
 * Forms:
 *   name;                        <- PROP_EMPTY
 *   name = "string";             <- PROP_STRING
 *   name = <cells>;              <- PROP_CELLS
 *   name = [bytes];              <- PROP_BYTES
 */
static DtProp *parse_property(Parser *p, char *name)
{
    /* empty property: just a name and semicolon */
    if (p->cur.type == TOK_SEMICOLON) {
        DtProp *prop = dt_prop_alloc(name);
        if (prop) prop->kind = PROP_EMPTY;
        advance(p); /* consume ';' */
        return prop;
    }

    if (!expect(p, TOK_EQUALS))
        return NULL;

    DtProp *prop = NULL;

    switch (p->cur.type) {
    case TOK_STRING:
        prop = dt_prop_alloc(name);
        if (prop) {
            prop->kind = PROP_STRING;
            strncpy(prop->str, p->cur.text, DT_STR_MAX - 1);
        }
        advance(p);
        break;

    case TOK_LANGLE:
        prop = parse_cells(p, name);
        break;

    case TOK_LBRACKET:
        prop = parse_bytes(p, name);
        break;

    default:
        parser_error(p, "expected property value");
        return NULL;
    }

    expect(p, TOK_SEMICOLON);
    return prop;
}

/* ------------------------------------------------------------------ */
/* Node parser                                                          */
/* ------------------------------------------------------------------ */

/*
 * Build a property name from the current token stream.
 * Handles:  name,  #name,  name-with-dashes
 * Cursor sits on the first token of the name on entry,
 * sits on the token AFTER the name on exit.
 */
static void collect_name(Parser *p, char *buf, int bufsz)
{
    int pos = 0;

    /* optional leading '#' */
    if (p->cur.type == TOK_HASH) {
        if (pos < bufsz - 1) buf[pos++] = '#';
        advance(p);
    }

    if (p->cur.type == TOK_IDENT) {
        int len = (int)strlen(p->cur.text);
        if (pos + len < bufsz - 1) {
            memcpy(buf + pos, p->cur.text, (size_t)len);
            pos += len;
        }
        advance(p);
    }

    buf[pos] = '\0';
}

/* Forward declaration — nodes can be nested. */
static DtNode *parse_node(Parser *p, const char *name);

/*
 * Parse the body of a node: { properties... children... }
 * Cursor is on '{' on entry, on the token after '}' on exit.
 */
static void parse_node_body(Parser *p, DtNode *node)
{
    expect(p, TOK_LBRACE);

    while (p->cur.type != TOK_RBRACE && p->cur.type != TOK_EOF) {

        /* skip /dts-v1/; and /memreserve/ directives */
        if (p->cur.type == TOK_SLASH) {
            while (p->cur.type != TOK_SEMICOLON &&
                   p->cur.type != TOK_EOF)
                advance(p);
            advance(p); /* consume ';' */
            continue;
        }

        /* collect the name (handles #address-cells etc.) */
        char name[DT_NAME_MAX] = {0};
        collect_name(p, name, DT_NAME_MAX);

        if (name[0] == '\0') {
            parser_error(p, "expected property or node name");
            advance(p);
            continue;
        }

        /* node with unit address: name@addr { */
        if (p->cur.type == TOK_AT) {
            advance(p); /* consume '@' */
            char full[DT_NAME_MAX];
            snprintf(full, sizeof(full), "%.*s@%.*s", DT_NAME_MAX/2 - 1, name, DT_NAME_MAX/2 - 1, p->cur.text);
            advance(p); /* consume address */
            DtNode *child = parse_node(p, full);
            if (child)
                dt_node_add_child(node, child);
            continue;
        }

        /* child node without unit address: name { */
        if (p->cur.type == TOK_LBRACE) {
            DtNode *child = parse_node(p, name);
            if (child)
                dt_node_add_child(node, child);
            continue;
        }

        /* otherwise it's a property */
        DtProp *prop = parse_property(p, name);
        if (prop)
            dt_node_add_prop(node, prop);
    }

    expect(p, TOK_RBRACE);
    /* consume optional trailing ';' after '}' */
    if (p->cur.type == TOK_SEMICOLON)
        advance(p);
}

static DtNode *parse_node(Parser *p, const char *name)
{
    DtNode *node = dt_node_alloc(name);
    if (!node) {
        fprintf(stderr, "OOM allocating node '%s'\n", name);
        return NULL;
    }
    parse_node_body(p, node);
    return node;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

int dt_tree_parse(DtTree *tree, FILE *fp, const char *src)
{
    memset(tree, 0, sizeof(*tree));
    strncpy(tree->src_file, src, sizeof(tree->src_file) - 1);

    Parser p;
    memset(&p, 0, sizeof(p));
    p.src = src;

    dts_lexer_init(&p.lex, fp);
    /* prime both cur and peek */
    p.peek = dts_lexer_next(&p.lex);
    advance(&p);

    /* skip /dts-v1/; header */
    while (p.cur.type == TOK_SLASH &&
           p.peek.type == TOK_IDENT) {
        while (p.cur.type != TOK_SEMICOLON &&
               p.cur.type != TOK_EOF)
            advance(&p);
        advance(&p); /* consume ';' */
    }

    /* expect root node: / { ... }; */
    if (p.cur.type != TOK_SLASH) {
        fprintf(stderr, "%s: expected root node '/'\n", src);
        return -1;
    }
    advance(&p); /* consume '/' */

    tree->root = parse_node(&p, "/");
    return (p.errors == 0) ? 0 : -1;
}

void dt_tree_free(DtTree *tree)
{
    if (tree) {
        dt_node_free(tree->root);
        tree->root = NULL;
    }
}

void dt_tree_print(const DtTree *tree)
{
    if (tree && tree->root)
        dt_node_print(tree->root, 0);
}
