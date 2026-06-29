#include "parser/dts_lexer.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Internal helpers                                                     */
/* ------------------------------------------------------------------ */

/* Advance one character, tracking line and column numbers. */
static int lexer_advance(DtsLexer *lex)
{
    lex->current = fgetc(lex->fp);
    if (lex->current == '\n') {
        lex->line++;
        lex->col = 0;
    } else {
        lex->col++;
    }
    return lex->current;
}

/* Skip whitespace and both comment styles:
 *   // single-line
 *   / * multi-line * /
 */
static void skip_whitespace_and_comments(DtsLexer *lex)
{
    for (;;) {
        /* skip whitespace */
        while (lex->current != EOF && isspace(lex->current))
            lexer_advance(lex);

        if (lex->current != '/')
            return;

        /* peek at next character to detect comment */
        int next = fgetc(lex->fp);

        if (next == '/') {
            /* single-line comment — consume until newline */
            lexer_advance(lex);
            while (lex->current != EOF && lex->current != '\n')
                lexer_advance(lex);
        } else if (next == '*') {
            /* multi-line comment — consume until star-slash */
            lexer_advance(lex);
            while (lex->current != EOF) {
                if (lex->current == '*') {
                    lexer_advance(lex);
                    if (lex->current == '/') {
                        lexer_advance(lex);
                        break;
                    }
                } else {
                    lexer_advance(lex);
                }
            }
        } else {
            /* not a comment — put the peeked char back */
            ungetc(next, lex->fp);
            return;
        }
    }
}

/* Fill tok->text and tok->numval for a hex or decimal integer. */
static void read_number(DtsLexer *lex, DtsToken *tok)
{
    int i = 0;

    /* collect all valid number characters */
    while (lex->current != EOF &&
           (isxdigit(lex->current) ||
            lex->current == 'x' ||
            lex->current == 'X')) {
        if (i < DTS_TOK_MAX - 1)
            tok->text[i++] = (char)lex->current;
        lexer_advance(lex);
    }
    tok->text[i] = '\0';
    tok->numval  = (uint64_t)strtoull(tok->text, NULL, 0);
    tok->type    = TOK_NUMBER;
}

/* Fill tok->text for a quoted string (strips the surrounding quotes). */
static void read_string(DtsLexer *lex, DtsToken *tok)
{
    int i = 0;
    lexer_advance(lex); /* consume opening '"' */

    while (lex->current != EOF && lex->current != '"') {
        if (lex->current == '\\') {
            /* pass through simple escape sequences */
            if (i < DTS_TOK_MAX - 1)
                tok->text[i++] = (char)lex->current;
            lexer_advance(lex);
        }
        if (i < DTS_TOK_MAX - 1)
            tok->text[i++] = (char)lex->current;
        lexer_advance(lex);
    }
    tok->text[i] = '\0';
    tok->type    = TOK_STRING;

    if (lex->current == '"')
        lexer_advance(lex); /* consume closing '"' */
}

/*
 * Fill tok->text for an identifier or keyword.
 * DTS identifiers may contain letters, digits, '-', '_', ',', '.', '#', '@'.
 * We stop before '@' and '#' so they are emitted as their own tokens.
 */
static void read_ident(DtsLexer *lex, DtsToken *tok)
{
    int i = 0;

    while (lex->current != EOF &&
           (isalnum(lex->current) ||
            lex->current == '-'   ||
            lex->current == '_'   ||
            lex->current == ','   ||
            lex->current == '.')) {
        if (i < DTS_TOK_MAX - 1)
            tok->text[i++] = (char)lex->current;
        lexer_advance(lex);
    }
    tok->text[i] = '\0';
    tok->type    = TOK_IDENT;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void dts_lexer_init(DtsLexer *lex, FILE *fp)
{
    lex->fp      = fp;
    lex->line    = 1;
    lex->col     = 0;
    lex->current = 0;
    lexer_advance(lex); /* prime the pump — read first character */
}

DtsToken dts_lexer_next(DtsLexer *lex)
{
    DtsToken tok;
    memset(&tok, 0, sizeof(tok));

    skip_whitespace_and_comments(lex);

    tok.line = lex->line;
    tok.col  = lex->col;

    if (lex->current == EOF) {
        tok.type    = TOK_EOF;
        tok.text[0] = '\0';
        return tok;
    }

    /* single-character tokens */
    switch (lex->current) {
    case '{': tok.type = TOK_LBRACE;    tok.text[0] = '{'; tok.text[1] = '\0'; lexer_advance(lex); return tok;
    case '}': tok.type = TOK_RBRACE;    tok.text[0] = '}'; tok.text[1] = '\0'; lexer_advance(lex); return tok;
    case '<': tok.type = TOK_LANGLE;    tok.text[0] = '<'; tok.text[1] = '\0'; lexer_advance(lex); return tok;
    case '>': tok.type = TOK_RANGLE;    tok.text[0] = '>'; tok.text[1] = '\0'; lexer_advance(lex); return tok;
    case '[': tok.type = TOK_LBRACKET;  tok.text[0] = '['; tok.text[1] = '\0'; lexer_advance(lex); return tok;
    case ']': tok.type = TOK_RBRACKET;  tok.text[0] = ']'; tok.text[1] = '\0'; lexer_advance(lex); return tok;
    case ';': tok.type = TOK_SEMICOLON; tok.text[0] = ';'; tok.text[1] = '\0'; lexer_advance(lex); return tok;
    case '=': tok.type = TOK_EQUALS;    tok.text[0] = '='; tok.text[1] = '\0'; lexer_advance(lex); return tok;
    case ',': tok.type = TOK_COMMA;     tok.text[0] = ','; tok.text[1] = '\0'; lexer_advance(lex); return tok;
    case '@': tok.type = TOK_AT;        tok.text[0] = '@'; tok.text[1] = '\0'; lexer_advance(lex); return tok;
    case '/': tok.type = TOK_SLASH;     tok.text[0] = '/'; tok.text[1] = '\0'; lexer_advance(lex); return tok;
    case '&': tok.type = TOK_AMPERSAND; tok.text[0] = '&'; tok.text[1] = '\0'; lexer_advance(lex); return tok;
    case '#': tok.type = TOK_HASH;      tok.text[0] = '#'; tok.text[1] = '\0'; lexer_advance(lex); return tok;
    case ':': tok.type = TOK_COLON;     tok.text[0] = ':'; tok.text[1] = '\0'; lexer_advance(lex); return tok;
    default:  break;
    }

    /* multi-character tokens */
    if (lex->current == '"') {
        read_string(lex, &tok);
        return tok;
    }

    if (isdigit(lex->current)) {
        read_number(lex, &tok);
        return tok;
    }

    if (isalpha(lex->current) || lex->current == '_') {
        read_ident(lex, &tok);
        return tok;
    }

    /* unrecognised character */
    tok.type    = TOK_ERROR;
    tok.text[0] = (char)lex->current;
    tok.text[1] = '\0';
    lexer_advance(lex);
    return tok;
}

const char *dts_token_type_name(DtsTokenType type)
{
    switch (type) {
    case TOK_EOF:       return "EOF";
    case TOK_IDENT:     return "IDENT";
    case TOK_STRING:    return "STRING";
    case TOK_NUMBER:    return "NUMBER";
    case TOK_LBRACE:    return "LBRACE";
    case TOK_RBRACE:    return "RBRACE";
    case TOK_LANGLE:    return "LANGLE";
    case TOK_RANGLE:    return "RANGLE";
    case TOK_LBRACKET:  return "LBRACKET";
    case TOK_RBRACKET:  return "RBRACKET";
    case TOK_SEMICOLON: return "SEMICOLON";
    case TOK_EQUALS:    return "EQUALS";
    case TOK_COMMA:     return "COMMA";
    case TOK_AT:        return "AT";
    case TOK_SLASH:     return "SLASH";
    case TOK_AMPERSAND: return "AMPERSAND";
    case TOK_HASH:      return "HASH";
    case TOK_COLON:     return "COLON";
    case TOK_ERROR:     return "ERROR";
    default:            return "UNKNOWN";
    }
}
