#ifndef DTS_LEXER_H
#define DTS_LEXER_H

#include <stdio.h>
#include <stdint.h>

/*
 * Every distinct syntactic unit in a DTS file maps to one token type.
 *
 * Example DTS fragment and its tokens:
 *
 *   spi@1000 {                        <- IDENT, AT, NUMBER, LBRACE
 *       compatible = "vendor,spi";    <- IDENT, EQUALS, STRING, SEMICOLON
 *       reg = <0x1000 0x100>;         <- IDENT, EQUALS, LANGLE, NUMBER,
 *                                        NUMBER, RANGLE, SEMICOLON
 *   };                                <- RBRACE, SEMICOLON
 */
typedef enum {
    TOK_EOF = 0,      /* end of file                        */
    TOK_IDENT,        /* node/property name: spi, compatible */
    TOK_STRING,       /* quoted string: "vendor,spi"         */
    TOK_NUMBER,       /* integer: 0x1000, 42                 */
    TOK_LBRACE,       /* {                                   */
    TOK_RBRACE,       /* }                                   */
    TOK_LANGLE,       /* <                                   */
    TOK_RANGLE,       /* >                                   */
    TOK_LBRACKET,     /* [  (byte arrays)                    */
    TOK_RBRACKET,     /* ]                                   */
    TOK_SEMICOLON,    /* ;                                   */
    TOK_EQUALS,       /* =                                   */
    TOK_COMMA,        /* ,                                   */
    TOK_AT,           /* @  (unit address separator)         */
    TOK_SLASH,        /* /  (root node, path separator)      */
    TOK_AMPERSAND,    /* &  (phandle reference)              */
    TOK_HASH,         /* #  (e.g. #address-cells)            */
    TOK_COLON,        /* :  (label separator)                */
    TOK_ERROR,        /* unrecognised character              */
} DtsTokenType;

/*
 * Maximum length of any single token's text.
 * DTS strings and identifiers are never longer than this in practice.
 */
#define DTS_TOK_MAX 256

/*
 * A single token returned by the lexer.
 *
 * @type  : what kind of token this is
 * @text  : raw text of the token (null-terminated)
 * @numval: numeric value when type == TOK_NUMBER
 * @line  : source line number (1-based) for error messages
 * @col   : source column number (1-based) for error messages
 */
typedef struct {
    DtsTokenType type;
    char         text[DTS_TOK_MAX];
    uint64_t     numval;
    int          line;
    int          col;
} DtsToken;

/*
 * Lexer state. Treat this as opaque — only lexer functions touch it.
 *
 * @fp      : open file handle (caller opens, caller closes)
 * @line    : current line number
 * @col     : current column number
 * @current : last character read from fp
 */
typedef struct {
    FILE *fp;
    int   line;
    int   col;
    int   current;
} DtsLexer;

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

/*
 * dts_lexer_init - initialise a lexer for the given open file.
 * The caller is responsible for opening and closing @fp.
 */
void dts_lexer_init(DtsLexer *lex, FILE *fp);

/*
 * dts_lexer_next - read and return the next token.
 * Always returns TOK_EOF once the file is exhausted.
 */
DtsToken dts_lexer_next(DtsLexer *lex);

/*
 * dts_token_type_name - return a human-readable name for a token type.
 * Returns a string literal — do not free.
 */
const char *dts_token_type_name(DtsTokenType type);

#endif /* DTS_LEXER_H */
