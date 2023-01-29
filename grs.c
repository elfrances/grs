#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "grs.h"

static int configGrsSock(Grs *pGrs, const CmdArgs *pArgs)
{
    // Open the listening TCP socket
    if ((pGrs->sd = socket(pArgs->sockAddr.ss_family, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "ERROR: failed to open TCP socket! (%s)\n", strerror(errno));
        return -1;
    }

    // Bind it to the specified address and port
    if (bind(pGrs->sd, (SockAddr *) &pArgs->sockAddr, ssLen(&pArgs->sockAddr)) != 0) {
        fprintf(stderr, "ERROR: failed to bind TCP socket! (%s)\n", strerror(errno));
        close(pGrs->sd);
        return -1;
    }

    // Start listening for client connections
    if (listen(pGrs->sd, 10) != 0) {
        fprintf(stderr, "ERROR: failed to listen on TCP socket! (%s)\n", strerror(errno));
        close(pGrs->sd);
        return -1;
    }

    // Add the listening socket as the first entry in
    // the pollFds array
    pGrs->pollFds[0].fd = pGrs->sd;
    pGrs->pollFds[0].events = POLLIN;

    return 0;
}

int procFdEvents(Grs *pGrs, const CmdArgs *pArgs, int nFds)
{
    // Check for new connections
    if (pGrs->pollFds[0].revents & POLLIN) {
        int cliSd;
        SockAddrStore cliAddr;
        socklen_t addrLen = sizeof (cliAddr);

        if ((cliSd = accept(pGrs->pollFds[0].fd, (SockAddr *) &cliAddr, &addrLen)) < 0) {
            fprintf(stderr, "ERROR: failed to accept new connection! (%s)\n", strerror(errno));
            return -1;
        }
        printf("New connection: cliSd=%d\n", cliSd);
    }

    return 0;
}

int sendReportMsg(Grs *pGrs, const CmdArgs *pArgs)
{
    printf("Sending report messages...\n");
    clock_gettime(CLOCK_REALTIME, &pGrs->lastReport);

    return 0;
}

int grsMain(Grs *pGrs, const CmdArgs *pArgs)
{
    Timespec reportPeriod = { .tv_sec = pArgs->reportPeriod, .tv_nsec = 0};

    // Allocate space for the list of file descriptors
    // to be monitored by poll()
    if ((pGrs->pollFds = calloc(pArgs->maxRiders, sizeof (PollFd))) == NULL) {
        fprintf(stderr, "ERROR: failed to alloc pollFds array! (%s)\n", strerror(errno));
        return -1;
    }

    // Open the listening TCP socket
    if (configGrsSock(pGrs, pArgs) != 0) {
        // Error message already printed
        return -1;
    }

    while (true) {
        int nFds;
        Timespec now, deltaT;

        // Wait for an event on any of the file descriptors or
        // until the report period expires...
        if ((nFds = ppoll(pGrs->pollFds, (pGrs->numRegRiders+1), &reportPeriod, NULL)) < 0) {
            fprintf(stderr, "ERROR: failed to wait for file descriptor events! (%s)\n", strerror(errno));
            return -1;
        }

        if (nFds > 0) {
            // Process the file descriptor events
            if (procFdEvents(pGrs, pArgs, nFds) != 0) {
                // Error message already printed
                return -1;
            }
        }

        clock_gettime(CLOCK_REALTIME, &now);
        tvSub(&deltaT, &now, &pGrs->lastReport);
        if (tvCmp(&deltaT, &reportPeriod) >+ 0) {
            // Send the report message to the clients
            if (sendReportMsg(pGrs, pArgs) != 0) {
                // Error message already printed
                return -1;
            }
        }
    }

    return 0;
}

