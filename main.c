#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "cmd_args.h"

#define PROGRAM_VERSION     "0.0"

static const char *help =
        "SYNTAX:\n"
        "    grs [OPTIONS]\n"
        "\n"
        "    Blah blah blah...\n"
        "\n"
        "OPTIONS:\n"
        "    --help\n"
        "        Show this help and exit.\n"
        "    --ip-addr <addr>\n"
        "        Specifies the IP address where the GRS app will listen for\n"
        "        connections.\n"
        "    --max-riders <num>\n"
        "        Specifies the maximum number of riders allowed to join the\n"
        "        group ride.\n"
        "    --start-time <time>\n"
        "        Specifies the start time (UTC) of the group ride.\n"
        "    --tcp-port <port>\n"
        "        Specifies the TCP port used by the GRS app.\n"
        "    --version\n"
        "        Show program's version info and exit.\n"
        "\n";

static int parseCmdArgs(int argc, char *argv[], CmdArgs *pArgs)
{
    int numArgs = argc - 1;

    for (int n = 1; n <= numArgs; n++) {
        const char *arg;
        //const char *val;

        arg = argv[n];

        if (strcmp(arg, "--help") == 0) {
            fprintf(stdout, "%s\n", help);
            exit(0);
        } else if (strcmp(arg, "--ip-addr") == 0) {

        } else if (strcmp(arg, "--tcp-port") == 0) {

        } else if (strcmp(arg, "--version") == 0) {
            fprintf(stdout, "Program version %s built on %s %s\n", PROGRAM_VERSION, __DATE__, __TIME__);
            exit(0);
        } else {
            fprintf(stderr, "Invalid option: %s\n", arg);
            return -1;
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    CmdArgs cmdArgs = {0};

    // Parse the command-line arguments
    if (parseCmdArgs(argc, argv, &cmdArgs) != 0) {
        fprintf(stderr, "Use --help for the list of supported options.\n\n");
        return -1;
    }

    return 0;
}



