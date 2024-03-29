#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "defs.h"
#include "grs.h"
#include "log.h"

#define PROGRAM_VERSION     "0.0"

static const char *help =
        "SYNTAX:\n"
        "    grs [OPTIONS]\n"
        "\n"
        "    GRS is a server app for coordinating virtual cycling group rides that\n"
        "    can have 100's of participants.\n"
        "\n"
        "OPTIONS:\n"
        "    --control-file <url>\n"
        "        Specifies the URL of the ride's control file.\n"
        "    --help\n"
        "        Show this help and exit.\n"
        "    --ip-addr <addr>\n"
        "        Specifies the IP address where the GRS app will listen for\n"
        "        connections. If no address is specified, the server will use\n"
        "        any of the available network interfaces.\n"
        "    --leaderboard-period <secs>\n"
        "        Specifies the period (in seconds) the GRS app needs to send\n"
        "        its \"leaderboard\" messages to the client apps.\n"
        "    --max-riders <num>\n"
        "        Specifies the maximum number of riders allowed to join the\n"
        "        group ride.\n"
        "    --prog-update-period <secs>\n"
        "        Specifies the period (in seconds) the client app's need to send\n"
        "        their \"progress update\" messages to the server.\n"
        "    --ride-name <name>\n"
        "        Specifies the name of the group ride.\n"
        "    --start-time <time>\n"
        "        Specifies the start date and time (in ISO 8601 UTC format) of\n"
        "        the group ride; e.g. 2023-04-01T17:00:00Z\n"
        "    --tcp-port <port>\n"
        "        Specifies the TCP port used by the GRS app. The default is TCP\n"
        "        port 50000.\n"
        "    --version\n"
        "        Show program's version info and exit.\n"
        "    --video-file <url>\n"
        "        Specifies the URL of the ride's video file.\n"
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

static int missOpt(const char *opt)
{
    fprintf(stderr, "Missing option: %s\n", opt);
    return -1;
}

static int parseCmdArgs(int argc, char *argv[], CmdArgs *pArgs)
{
    int numArgs = argc - 1;

    // Set the default values
    pArgs->maxRiders = 100;
    pArgs->progUpdPeriod = 1;
    pArgs->leaderboardPeriod = 2;
    pArgs->tcpPort = DEF_TCP_PORT;

    for (int n = 1; n <= numArgs; n++) {
        const char *arg;
        const char *val;

        arg = argv[n];

        if (strcmp(arg, "--control-file") == 0) {
            val = argv[++n];
            if (val == NULL) {
                return missArg(arg, "<url>");
            } else {
                pArgs->controlFile = strdup(val);
            }
        } else if (strcmp(arg, "--help") == 0) {
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
                    struct sockaddr_in *sockAddr = (struct sockaddr_in*) &pArgs->sockAddr;
                    sockAddr->sin_family = AF_INET;
                    sockAddr->sin_addr = addrV4;
                } else if (inet_pton(AF_INET6, val, &addrV6) == 1) {
                    struct sockaddr_in6 *sockAddr = (struct sockaddr_in6*) &pArgs->sockAddr;
                    sockAddr->sin6_family = AF_INET6;
                    sockAddr->sin6_addr = addrV6;
                } else {
                    return invArg(val);
                }
            }
        } else if (strcmp(arg, "--leaderboard-period") == 0) {
            val = argv[++n];
            if (val == NULL) {
                return missArg(arg, "<num>");
            } else if (sscanf(val, "%d", &pArgs->leaderboardPeriod) != 1) {
                return invArg(val);
            }
        } else if (strcmp(arg, "--max-riders") == 0) {
            val = argv[++n];
            if (val == NULL) {
                return missArg(arg, "<num>");
            } else if (sscanf(val, "%d", &pArgs->maxRiders) != 1) {
                return invArg(val);
            }
        } else if (strcmp(arg, "--prog-update-period") == 0) {
            val = argv[++n];
            if (val == NULL) {
                return missArg(arg, "<num>");
            } else if (sscanf(val, "%d", &pArgs->progUpdPeriod) != 1) {
                return invArg(val);
            }
        } else if (strcmp(arg, "--ride-name") == 0) {
            val = argv[++n];
            if (val == NULL) {
                return missArg(arg, "<name>");
            } else {
                pArgs->rideName = strdup(val);
            }
        } else if (strcmp(arg, "--start-time") == 0) {
            val = argv[++n];
            if (val == NULL) {
                return missArg(arg, "<date+time>");
            } else {
                struct tm brkDwnTime = { 0 };
                if (strptime(val, "%Y-%m-%dT%H:%M:%S", &brkDwnTime) != NULL) {
                    pArgs->startTime = mktime(&brkDwnTime);
                } else {
                    return invArg(val);
                }
            }
        } else if (strcmp(arg, "--tcp-port") == 0) {
            val = argv[++n];
            if (val == NULL) {
                return missArg(arg, "<num>");
            } else if (sscanf(val, "%d", &pArgs->tcpPort) != 1) {
                return invArg(val);
            }
        } else if (strcmp(arg, "--version") == 0) {
            fprintf(stdout, "Program version %s built on %s %s\n", PROGRAM_VERSION, __DATE__, __TIME__);
            exit(0);
        } else if (strcmp(arg, "--video-file") == 0) {
            val = argv[++n];
            if (val == NULL) {
                return missArg(arg, "<url>");
            } else {
                pArgs->videoFile = strdup(val);
            }
        } else {
            fprintf(stderr, "Invalid option: %s\n", arg);
            return -1;
        }
    }

    // Sanity check the args...

    if (pArgs->controlFile == NULL) {
        return missOpt("--control-file <url>");
    }

    if (pArgs->rideName == NULL) {
        return missOpt("--ride-name <name>");
    }

    if (pArgs->videoFile == NULL) {
        return missOpt("--video-file <url>");
    }

    if (pArgs->startTime != 0) {
        time_t now = time(NULL);
        if (pArgs->startTime < now) {
            time_t diff = now - pArgs->startTime;
            char msg[128];
            struct tm tm;
            strftime(msg, sizeof (msg), "Ride start time is %H:%M:%S in the past!", localtime_r(&diff, &tm));
            return invArg(msg);
        }
    }

    if ((pArgs->tcpPort < 49152) || (pArgs->tcpPort > 65535)) {
        return invArg("TCP port must be in the range 49152-65535");
    }

    // If no address was specified, use the IPv4 wildcard
    if (pArgs->sockAddr.ss_family == 0) {
        pArgs->sockAddr.ss_family = AF_INET;
    }

    // Set the TCP port in the socket address object
    if (pArgs->sockAddr.ss_family == AF_INET) {
        ((struct sockaddr_in*) &pArgs->sockAddr)->sin_port = htons(pArgs->tcpPort);
    } else {
        ((struct sockaddr_in6*) &pArgs->sockAddr)->sin6_port = htons(pArgs->tcpPort);
    }

    {
        char startTime[128];
        struct tm tm;
        strftime(startTime, sizeof (startTime), "%Y-%m-%dT%H:%M:%S", localtime_r(&pArgs->startTime, &tm));
        MSGLOG(INFO, "rideName=%s controlFile=%s videoFile=%s startTime=%s maxRiders=%d progUpdPeriod=%d leaderboardPeriod=%d",
                pArgs->rideName, pArgs->controlFile, pArgs->videoFile,
                startTime, pArgs->maxRiders, pArgs->progUpdPeriod, pArgs->leaderboardPeriod);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    CmdArgs cmdArgs = { 0 };
    Grs grs = { 0 };

    // Parse the command-line arguments
    if (parseCmdArgs(argc, argv, &cmdArgs) != 0) {
        fprintf(stderr, "Use --help for the list of supported options.\n\n");
        return -1;
    }

    // Start the main work loop...
    if (grsMain(&grs, &cmdArgs) != 0) {
        MSGLOG(FATAL, "Something went wrong. BYE!");
        return -1;
    }

    return 0;
}

