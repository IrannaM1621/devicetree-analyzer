#include <stdio.h>
#include <stdlib.h>

#include "parser/dts_lexer.h"
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

    DtsLexer lex;
    dts_lexer_init(&lex, fp);

    DtsToken tok;
    do {
        tok = dts_lexer_next(&lex);
        printf("%-12s '%s'",
               dts_token_type_name(tok.type),
               tok.text);
        if (tok.type == TOK_NUMBER)
            printf("  (0x%llx)", (unsigned long long)tok.numval);
        printf("\n");
    } while (tok.type != TOK_EOF && tok.type != TOK_ERROR);

    fclose(fp);
    return 0;
}
