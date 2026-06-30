#ifndef CLI_H
#define CLI_H

/*
 * Parsed command-line options.
 *
 * @file          : path to the .dts/.dtb file (required, positional)
 * @show_tree     : --tree            print the parsed node tree
 * @show_deps     : --deps            print the dependency list
 * @show_graph    : --graph           print the adjacency-list graph
 * @check_cycles  : --check-cycles    run cycle detection
 * @depends_on    : --depends-on LBL  reverse-lookup consumers of label LBL
 * @quiet         : --quiet           suppress the summary banner
 */
typedef struct {
    char file[256];
    int  show_tree;
    int  show_deps;
    int  show_graph;
    int  check_cycles;
    char depends_on[128];
    int  quiet;
} CliOptions;

/*
 * cli_parse - parse argv into @opts.
 * Returns 0 on success, -1 on usage error (prints usage to stderr).
 */
int cli_parse(int argc, char *argv[], CliOptions *opts);

/* Print usage/help text to stdout. */
void cli_print_usage(const char *prog);

#endif /* CLI_H */
