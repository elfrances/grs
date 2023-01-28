#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "defs.h"

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
        "        connections. If no address is specified, the server will use\n"
        "        any of the available network interfaces.\n"
        "    --max-riders <num>\n"
        "        Specifies the maximum number of riders allowed to join the\n"
        "        group ride.\n"
        "    --report-period <secs>\n"
        "        Specifies the period (in seconds) the client app's need to send\n"
        "        their update messages to the server.\n"
        "    --start-time <time>\n"
        "        Specifies the start date and time (in ISO 8601 UTC format) of\n"
        "        the group ride.\n"
        "    --tcp-port <port>\n"
        "        Specifies the TCP port used by the GRS app. The default is TCP\n"
        "        port 54321."
        "    --version\n"
        "        Show program's version info and exit.\n"
        "\n";

static int invArg(const char *arg)
{
    fprintf(stderr, "Invalid argument: '%s'\n", arg);
    return -1;
}

static int missArg(const char *arg, const char *val)
{
    fprintf(stderr, "Missing argument. Syntax: '%s %s'\n", arg, val);
    return -1;
}

static int parseCmdArgs(int argc, char *argv[], CmdArgs *pArgs)
{
    int numArgs = argc - 1;

    for (int n = 1; n <= numArgs; n++) {
        const char *arg;
        const char *val;

        arg = argv[n];

        if (strcmp(arg, "--help") == 0) {
            fprintf(stdout, "%s\n", help);
            exit(0);
        } else if (strcmp(arg, "--ip-addr") == 0) {
            val = argv[++n];
            if (val == NULL) {
                return missArg(arg, "<addr>");
            } else {
                struct in_addr addrV4;
                struct in6_addr addrV6;
                if (inet_pton(AF_INET, val, &addrV4) == 1) {
                    struct sockaddr_in *sockAddr = (struct sockaddr_in *) &pArgs->sockAddr;
                    sockAddr->sin_family = AF_INET;
                    sockAddr->sin_addr = addrV4;
                } else if (inet_pton(AF_INET6, val, &addrV6) == 1) {
                    struct sockaddr_in6 *sockAddr = (struct sockaddr_in6 *) &pArgs->sockAddr;
                    sockAddr->sin6_family = AF_INET6;
                    sockAddr->sin6_addr = addrV6;
                } else {
                    return invArg(val);
                }
            }
        } else if (strcmp(arg, "--max-riders") == 0) {
            val = argv[++n];
            if (val == NULL) {
                return missArg(arg, "<num>");
            } else if (sscanf(val, "%d", &pArgs->maxRiders) != 1) {
                return invArg(val);
            }
        } else if (strcmp(arg, "--report-period") == 0) {
            val = argv[++n];
            if (val == NULL) {
                return missArg(arg, "<num>");
            } else if (sscanf(val, "%d", &pArgs->reportPeriod) != 1) {
                return invArg(val);
            }
        } else if (strcmp(arg, "--tcp-port") == 0) {
            val = argv[++n];
            if (val == NULL) {
                return missArg(arg, "<num>");
            } else if (sscanf(val, "%d", &pArgs->tcpPort) != 1) {
                return invArg(val);
            }
        } else if (strcmp(arg, "--start-time") == 0) {
            val = argv[++n];
            if (val == NULL) {
                return missArg(arg, "<date+time>");
            } else {
                struct tm brkDwnTime = {0};
                if (strptime(val, "%Y-%m-%dT%H:%M:%S", &brkDwnTime) != NULL) {
                    pArgs->startTime = mktime(&brkDwnTime);
                } else {
                    return invArg(val);
                }
            }
        } else if (strcmp(arg, "--version") == 0) {
            fprintf(stdout, "Program version %s built on %s %s\n", PROGRAM_VERSION, __DATE__, __TIME__);
            exit(0);
        } else {
            fprintf(stderr, "Invalid option: %s\n", arg);
            return -1;
        }
    }

    // Sanity check the args..

    if ((pArgs->tcpPort < 49152) || (pArgs->tcpPort > 65535)) {
        return invArg("TCP port must be in the range 49152-65535");
    }

    // If no address was specified, use the IPv4 wildcard
    if (pArgs->sockAddr.ss_family == 0) {
        pArgs->sockAddr.ss_family = AF_INET;
    }

    // Set the TCP port in the socket address object
    if (pArgs->sockAddr.ss_family == AF_INET) {
        ((struct sockaddr_in *) &pArgs->sockAddr)->sin_port = htons(pArgs->tcpPort);
    } else {
        ((struct sockaddr_in6 *) &pArgs->sockAddr)->sin6_port = htons(pArgs->tcpPort);
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



