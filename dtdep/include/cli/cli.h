#ifndef CLI_H
#define CLI_H

typedef enum {
    FORMAT_TEXT = 0,
    FORMAT_DOT,
    FORMAT_HTML,
} OutputFormat;

typedef struct {
    char file[256];
    int  show_tree;
    int  show_deps;
    int  show_graph;
    int  check_cycles;
    char depends_on[128];
    int  quiet;
    int  check;
    OutputFormat format;
} CliOptions;

int  cli_parse(int argc, char *argv[], CliOptions *opts);
void cli_print_usage(const char *prog);

#endif /* CLI_H */
