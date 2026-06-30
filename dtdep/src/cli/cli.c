#define _POSIX_C_SOURCE 200809L

#include "cli/cli.h"

#include <stdio.h>
#include <string.h>
#include <getopt.h>

void cli_print_usage(const char *prog)
{
    printf("usage: %s <command> <file.dts> [options]\n\n", prog);
    printf("commands:\n");
    printf("  parse <file>      parse and validate a device tree\n\n");
    printf("options:\n");
    printf("  --tree              print the full parsed node tree\n");
    printf("  --deps              print the dependency list\n");
    printf("  --graph             print the adjacency-list graph\n");
    printf("  --check-cycles      run dependency cycle detection\n");
    printf("  --depends-on LABEL  list nodes that depend on LABEL\n");
    printf("  --format FORMAT     output format: text (default), dot, or html\n");
    printf("  --check             run structural analysis for driver-breaking issues\n");
    printf("  --quiet             suppress the summary banner\n");
    printf("  --help              show this message\n");
}

int cli_parse(int argc, char *argv[], CliOptions *opts)
{
    memset(opts, 0, sizeof(*opts));
    opts->format = FORMAT_TEXT;

    if (argc < 3) {
        cli_print_usage(argv[0]);
        return -1;
    }

    if (strcmp(argv[1], "parse") != 0) {
        fprintf(stderr, "error: unknown command '%s'\n", argv[1]);
        cli_print_usage(argv[0]);
        return -1;
    }

    strncpy(opts->file, argv[2], sizeof(opts->file) - 1);

    static struct option long_opts[] = {
        {"tree",          no_argument,       0, 't'},
        {"deps",          no_argument,       0, 'd'},
        {"graph",         no_argument,       0, 'g'},
        {"check-cycles",  no_argument,       0, 'c'},
        {"depends-on",    required_argument, 0, 'r'},
        {"format",        required_argument, 0, 'f'},
        {"check",         no_argument,       0, 'k'},
        {"quiet",         no_argument,       0, 'q'},
        {"help",          no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int shifted_argc = argc - 2;
    char **shifted_argv = argv + 2;

    int c;
    optind = 1;
    while ((c = getopt_long(shifted_argc, shifted_argv, "",
                             long_opts, NULL)) != -1) {
        switch (c) {
        case 't': opts->show_tree    = 1; break;
        case 'd': opts->show_deps    = 1; break;
        case 'g': opts->show_graph   = 1; break;
        case 'c': opts->check_cycles = 1; break;
        case 'r':
            strncpy(opts->depends_on, optarg, sizeof(opts->depends_on) - 1);
            break;
        case 'f':
            if (strcmp(optarg, "dot") == 0) {
                opts->format = FORMAT_DOT;
            } else if (strcmp(optarg, "html") == 0) {
                opts->format = FORMAT_HTML;
            } else if (strcmp(optarg, "text") == 0) {
                opts->format = FORMAT_TEXT;
            } else {
                fprintf(stderr, "error: unknown format '%s'\n", optarg);
                return -1;
            }
            break;
        case 'k': opts->check = 1; break;
        case 'q': opts->quiet = 1; break;
        case 'h':
            cli_print_usage(argv[0]);
            return -1;
        default:
            cli_print_usage(argv[0]);
            return -1;
        }
    }

    return 0;
}
