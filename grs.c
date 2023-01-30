#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "grs.h"

// Format a SockAddrStore object as the string: <ipAddr>[<portNum>]
#define SSFMT_BUF_LEN   (INET6_ADDRSTRLEN+1+5+1)
static char *ssFmt(const SockAddrStore *pSock, char *fmtBuf, size_t bufLen, Bool printPort)
{
    size_t len = (printPort) ? 7 : 0;
    void *pAddr;
    uint16_t port;

    if (pSock->ss_family == AF_INET) {
        len += INET_ADDRSTRLEN;
        pAddr = &((SockAddrIn *) pSock)->sin_addr;
        port = ((SockAddrIn *) pSock)->sin_port;
    } else if (pSock->ss_family == AF_INET6) {
        len += INET6_ADDRSTRLEN;
        pAddr = &((SockAddrIn6 *) pSock)->sin6_addr;
        port = ((SockAddrIn6 *) pSock)->sin6_port;
    } else {
        snprintf(fmtBuf, bufLen, "af=%d invalid!", pSock->ss_family);
        return fmtBuf;
    }

    if (bufLen < len) {
        snprintf(fmtBuf, bufLen, "bufLen=%zu too small!", bufLen);
        return fmtBuf;
    }

    inet_ntop(pSock->ss_family, pAddr, fmtBuf, bufLen);
    if (printPort) {
        size_t sLen = strlen(fmtBuf);
        char *pBuf = fmtBuf + sLen;
        snprintf(pBuf, (bufLen - sLen), "[%5u]", ntohs(port));
    }

    return fmtBuf;
}

static void buildPollFds(Grs *pGrs)
{
    int n = 0;

    // The first entry is always the file descriptor
    // of the listening socket.
    pGrs->pollFds[n].fd = pGrs->sd;
    pGrs->pollFds[n].events = POLLIN;
    pGrs->pollFds[n++].revents = 0;

    // Now add an entry for each connected socket
    for (Gender gender = unspec; gender < GenderMax; gender++) {
        for (AgeGrp ageGrp = undef; ageGrp < AgeGrpMax; ageGrp++) {
            Rider *pRider;
            TAILQ_FOREACH(pRider, &pGrs->riderList[gender][ageGrp], tqEntry) {
                if (pRider->state != unknown) {
                    pGrs->pollFds[n].fd = pRider->sd;
                    pGrs->pollFds[n].events = (POLLIN | POLLRDHUP);
                    pGrs->pollFds[n++].revents = 0;
                }
            }
        }
    }

    pGrs->numFds = n;

#if 0
    {
        printf("%s: ", __func__);
        for (n = 0; n < pGrs->numFds; n++) {
            printf("%d:{fd=%d evt=%x revt=%x} ", n, pGrs->pollFds[n].fd, pGrs->pollFds[n].events, pGrs->pollFds[n].revents);
        }
        printf("\n");
    }
#endif

    // Done!
    pGrs->rebuildPollFds = false;
}

static int configGrsSock(Grs *pGrs, const CmdArgs *pArgs)
{
    int enable = 1;

    // Open the listening TCP socket
    if ((pGrs->sd = socket(pArgs->sockAddr.ss_family, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "ERROR: failed to open TCP socket! (%s)\n", strerror(errno));
        return -1;
    }

    // Set REUSEADDR option on the listening socket to
    // allow the server to restart quickly.
    if (setsockopt(pGrs->sd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof (enable)) != 0) {
        fprintf(stderr, "ERROR: failed to set SO_REUSEADDR option! (%s)\n", strerror(errno));
        close(pGrs->sd);
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

    // Build the pollFds array
    buildPollFds(pGrs);

    return 0;
}

static int procConnect(Grs *pGrs, const CmdArgs *pArgs)
{
    int sd;
    SockAddrStore sockAddr;
    socklen_t addrLen = sizeof (sockAddr);
    int noDelay = 1;
    Rider *pRider;

    // Accept the new connection
    if ((sd = accept(pGrs->pollFds[0].fd, (SockAddr *) &sockAddr, &addrLen)) < 0) {
        fprintf(stderr, "ERROR: failed to accept new connection! (%s)\n", strerror(errno));
        return -1;
    }

    // Disable Nagel's algo
    if (setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof (noDelay)) != 0) {
        fprintf(stderr, "ERROR: failed to set TCP_NODELAY option! (%s)\n", strerror(errno));
        close(sd);
        return -1;
    }

    {
        char fmtBuf[SSFMT_BUF_LEN];
        printf("New connection: sd=%d addr=%s\n", sd, ssFmt(&sockAddr, fmtBuf, sizeof (fmtBuf), true));
    }

    if ((pRider = calloc(1, sizeof (Rider))) == NULL) {
        fprintf(stderr, "ERROR: failed to alloc Rider object! (%s)\n", strerror(errno));
        close(sd);
        return -1;
    }

    // Init what we can at this point
    pRider->sd = sd;
    pRider->sockAddr = sockAddr;
    pRider->state = connected;

    // Until the app sends the REG_REQ message, place the
    // new rider in the general category...
    TAILQ_INSERT_HEAD(&pGrs->riderList[unspec][undef], pRider, tqEntry);

    // Need to rebuild the pollFds array
    pGrs->rebuildPollFds = true;

    return 0;
}

static int procDisconnect(Grs *pGrs, const CmdArgs *pArgs, int fd)
{
    for (Gender gender = unspec; gender < GenderMax; gender++) {
        for (AgeGrp ageGrp = undef; ageGrp < AgeGrpMax; ageGrp++) {
            Rider *pRider;
            TAILQ_FOREACH(pRider, &pGrs->riderList[gender][ageGrp], tqEntry) {
                if (pRider->sd == fd) {
                    {
                        char fmtBuf[SSFMT_BUF_LEN];
                        printf("Disconnected: sd=%d addr=%s\n", fd, ssFmt(&pRider->sockAddr, fmtBuf, sizeof (fmtBuf), true));
                    }
                    close(fd);
                    pRider->state = unknown;
                }
            }
        }
    }

    // Need to rebuild the pollFds array
    pGrs->rebuildPollFds = true;

    return 0;
}

static int procData(Grs *pGrs, const CmdArgs *pArgs, int fd)
{
    printf("Data available: fd=%d\n", fd);

    return 0;
}

int procFdEvents(Grs *pGrs, const CmdArgs *pArgs, int nFds)
{
    // First check for new connections
    if (pGrs->pollFds[0].revents & POLLIN) {
        if (procConnect(pGrs, pArgs) != 0) {
            fprintf(stderr, "ERROR: failed to create new connection!\n");
            return -1;
        }
    }

    // Next check for events on any of the connected sockets
    for (int n = 1; n < pGrs->numFds; n++) {
        int revents = pGrs->pollFds[n].revents;
        if (revents & (POLLRDHUP | POLLHUP)) {
            procDisconnect(pGrs, pArgs, pGrs->pollFds[n].fd);
        } else if (revents & POLLIN) {
            procData(pGrs, pArgs, pGrs->pollFds[n].fd);
        } else if (revents != 0) {
            fprintf(stderr, "ERROR: unknown event! fd=%d revents=%x\n",
                    pGrs->pollFds[n].fd, pGrs->pollFds[n].revents);
            return -1;
        }
    }

    if (pGrs->rebuildPollFds) {
        // Rebuild the pollFds array
        buildPollFds(pGrs);
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
        if ((nFds = ppoll(pGrs->pollFds, pGrs->numFds, &reportPeriod, NULL)) < 0) {
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

        // Is it time to send the report messages?
        clock_gettime(CLOCK_REALTIME, &now);
        tvSub(&deltaT, &now, &pGrs->lastReport);
        if (tvCmp(&deltaT, &reportPeriod) >= 0) {
            // Send the report message to the clients
            if (sendReportMsg(pGrs, pArgs) != 0) {
                // Error message already printed
                return -1;
            }
        }
    }

    return 0;
}

