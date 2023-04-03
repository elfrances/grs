#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "grs.h"
#include "json.h"

static const char *riderStateTble[] = {
    [unknown]       "unknown",
    [connected]     "connected",
    [registered]    "registered",
    [active]        "active"
};

// This table is used to look up a Rider record from
// its associated socket file descriptor
#define MAX_FD_VAL    (FD_SETSIZE + 1)
static Rider *fdMapTbl[MAX_FD_VAL];

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
    for (int fd = 0; fd < MAX_FD_VAL; fd++) {
        if (fdMapTbl[fd] != NULL) {
            pGrs->pollFds[n].fd = fd;
            pGrs->pollFds[n].events = (POLLIN | POLLRDHUP);
            pGrs->pollFds[n++].revents = 0;
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

    // Create the map entry
    fdMapTbl[sd] = pRider;

    // Need to rebuild the pollFds array
    pGrs->rebuildPollFds = true;

    return 0;
}

static int procDisconnect(Grs *pGrs, const CmdArgs *pArgs, int fd)
{
    Rider *pRider;

    // Use the file descriptor to locate the Rider record
    if ((pRider = fdMapTbl[fd]) != NULL) {
        char fmtBuf[SSFMT_BUF_LEN];
        printf("Disconnected: sd=%d addr=%s state=%s name=%s\n",
                fd, ssFmt(&pRider->sockAddr, fmtBuf, sizeof (fmtBuf), true),
                riderStateTble[pRider->state], pRider->name);
        if ((pRider->state == registered) || (pRider->state == active)) {
            // Remove rider from its gender/age list
            TAILQ_REMOVE(&pGrs->riderList[pRider->gender][pRider->ageGrp], pRider, tqEntry);
        }
        fdMapTbl[fd] = NULL;
        free(pRider);
        close(fd);

        // Need to rebuild the pollFds array
        pGrs->rebuildPollFds = true;

        return 0;
    }

    return -1;
}

static Gender genderFromTagVal(const char *tagVal)
{
    if (tagVal != NULL) {
        if (strcmp(tagVal, "male") == 0) {
            return male;
        } else if (strcmp(tagVal, "female") == 0) {
            return female;
        } else if (strcmp(tagVal, "nonBinary") == 0) {
            return nonBinary;
        }
    }

    return unspec;
}

static int ageFromTagVal(const char *tagVal)
{
    int age = 0;

    if (tagVal != NULL) {
        sscanf(tagVal, "%d", &age);
    }

    return age;
}

static AgeGrp ageToAgeGrp(int age)
{
    AgeGrp ageGrp = undef;

    if ((age > 0) && (age <= 18)) {
        ageGrp = u19;
    } else if ((age > 18) && (age <= 34)) {
        ageGrp = u35;
    } else if ((age > 34) && (age <= 39)) {
        ageGrp = u40;
    } else if ((age > 39) && (age <= 44)) {
        ageGrp = u45;
    } else if ((age > 44) && (age <= 49)) {
        ageGrp = u50;
    } else if ((age > 49) && (age <= 54)) {
        ageGrp = u55;
    } else if ((age > 54) && (age <= 59)) {
        ageGrp = u60;
    } else if ((age > 59) && (age <= 64)) {
        ageGrp = u65;
    } else if ((age > 64) && (age <= 69)) {
        ageGrp = u70;
    } else if ((age > 69) && (age <= 74)) {
        ageGrp = u75;
    } else if ((age > 74) && (age <= 79)) {
        ageGrp = u80;
    } else if ((age > 79) && (age <= 84)) {
        ageGrp = u85;
    } else if ((age > 84) && (age <= 89)) {
        ageGrp = u90;
    } else if ((age > 89) && (age <= 94)) {
        ageGrp = u95;
    } else if ((age > 94) && (age <= 99)) {
        ageGrp = u100;
    }

    return ageGrp;
}

// Send a Registration Response message
//
// Message format:
//
//   {
//     "type": "regResp",
//     "status": "{error|success}",
//     "bibNum": "<BibNumber>",
//     "startTime": "<StartTimeInUTC>",
//     "controlFile": "<URL>",
//     "videoFile": "<URL>",
//     "reportPeriod": "<ReportPeriodInSec>"
//   }
//
// Example:
//
//   {
//     "type": "regResp",
//     "status": "success",
//     "bibNum": "123",
//     "startTime": "1680469260",
//     "controlFile": "http://grs.net/RPI-TCR.shiz",
//     "videoFile": "http://grs.net/RPI-TCR.mp4",
//     "reportPeriod": "2"
//  }
//
static int sendRegRespMsg(Grs *pGrs, const CmdArgs *pArgs, Rider *pRider)
{
    char msg[1024];
    size_t msgLen;
    ssize_t len;

    snprintf(msg, sizeof (msg), "{\"type\": \"regResp\", \"status\": \"success\", \"bibNum\": \"%d\", \"startTime\": \"%ld\", \"controlFile\": \"%s\", \"videoFile\": \"%s\", \"reportPeriod\": \"%d\"}",
            pRider->bibNum, pArgs->startTime, pArgs->controlFile, pArgs->videoFile, pArgs->reportPeriod);
    msgLen = strlen(msg) + 1;

    if ((len = send(pRider->sd, msg, msgLen, 0)) != msgLen) {
        fprintf(stderr, "ERROR: failed to send data! fd=%d (%s)\n", pRider->sd, strerror(errno));
        return -1;
    }

    return 0;
}

// Send a GO! message to all the registered riders
//
// Message format:
//
//   {"type": "go"}
//
static int sendGoMsg(Grs *pGrs, const CmdArgs *pArgs)
{
    char msg[1024];
    size_t msgLen;

    snprintf(msg, sizeof (msg), "{\"type\": \"go\"}");
    msgLen = strlen(msg) + 1;

    for (Gender gender = unspec; gender < GenderMax; gender++) {
        for (AgeGrp ageGrp = undef; ageGrp < AgeGrpMax; ageGrp++) {
            Rider *pRider;

            TAILQ_FOREACH(pRider, &pGrs->riderList[gender][ageGrp], tqEntry) {
                if (pRider->state == registered) {
                    ssize_t len;

                    if ((len = send(pRider->sd, msg, msgLen, 0)) != msgLen) {
                        fprintf(stderr, "ERROR: failed to send data! fd=%d (%s)\n", pRider->sd, strerror(errno));
                        return -1;
                    }
                }
            }
        }
    }

    return 0;
}

// Process a Registration Request message
//
// Message format:
//
//   {
//     "type": "regReq",
//     "name": "<RidersName>",
//     "gender": "{female|male|nonBinary|unspec}",
//     "age": "<RidersAge>",
//     "ride": "<RideName>"
//   }
//
// Example:
//
//   {
//     "type": "regReq",
//     "name": "Marcelo Mourier",
//     "gender": "male",
//     "age": "61",
//     "ride": "Sarbachtal"
//   }
//
static int procRegReqMsg(Grs *pGrs, const CmdArgs *pArgs, int fd, JsonObject *pMsg)
{
    Rider *pRider;

    // Use the file descriptor to locate the Rider record
    if ((pRider = fdMapTbl[fd]) != NULL) {
        if (pRider->state == connected) {
            // Found it!

            // Get all the tag values
            char *ride = jsonGetTagValue(pMsg, "ride");
            if (ride == NULL) {
                fprintf(stderr, "ERROR: no ride name specified! fd=%d\n", fd);
                return -1;
            } else if (strcmp(ride, pArgs->rideName) != 0) {
                fprintf(stderr, "ERROR: invalid ride name! fd=%d ride=%s\n", fd, ride);
                free(ride);
                return -1;
            }
            pRider->name = jsonGetTagValue(pMsg, "name");
            pRider->gender = genderFromTagVal(jsonGetTagValue(pMsg, "gender"));
            pRider->age = ageFromTagVal(jsonGetTagValue(pMsg, "age"));
            pRider->ageGrp = ageToAgeGrp(pRider->age);

            // Assign a bib number
            pRider->bibNum = ++pGrs->numRegRiders;

            // Send back the Registration Response message
            if (sendRegRespMsg(pGrs, pArgs, pRider) != 0) {
                // Error message already printed
                return -1;
            }

            // This rider is now registered
            pRider->state = registered;

            // Move the rider to the correct gender/age
            // category.
            TAILQ_INSERT_HEAD(&pGrs->riderList[pRider->gender][pRider->ageGrp], pRider, tqEntry);

            // Don't need this anymore
            free(ride);

            printf("INFO: Received regReq message: fd=%d name=%s gender=%d age=%d\n",
                    fd, pRider->name, pRider->gender, pRider->age);

            // Done!
            return 0;
        } else {
            fprintf(stderr, "ERROR: %s: invalid state! fd=%d state=%s\n", __func__, fd, riderStateTble[pRider->state]);
            return -1;
        }
    }

    return -1;
}

// Process a Progress Update message
//
// Message format:
//
//   {
//     "type": "progUpd",
//     "distance": "<DistanceInMeters>",
//     "power": "<PowerInWatts>",
//     "speed": "<SpeedInMetersPerSec>"
//   }
//
// Example:
//
//   {
//     "type": "progUpd",
//     "distance": "1620",
//     "power": "250",
//     "speed": "9.722"
//   }
//
static int procProgUpdMsg(Grs *pGrs, const CmdArgs *pArgs, int fd, JsonObject *pMsg)
{
    Rider *pRider;

    // Make sure the group ride has started
    if (!pGrs->rideActive) {
        fprintf(stderr, "ERROR: group ride is not active! fd=%d\n", fd);
        return -1;
    }

    // Use the file descriptor to locate the Rider record
    if ((pRider = fdMapTbl[fd]) != NULL) {
        if (pRider->state == registered) {
            // Get all the tag values
            char *distance = jsonGetTagValue(pMsg, "distance");
            if (distance != NULL) {
                sscanf(distance, "%d", &pRider->distance);
                free(distance);
            } else {
                fprintf(stderr, "ERROR: no distance specified! fd=%d\n", fd);
            }

            char *power = jsonGetTagValue(pMsg, "power");
            if (power != NULL) {
                sscanf(power, "%d", &pRider->power);
                free(power);
            } else {
                fprintf(stderr, "ERROR: no power specified! fd=%d\n", fd);
            }

            printf("INFO: Received progUpd message: fd=%d name=%s distance=%d power=%d\n",
                    fd, pRider->name, pRider->distance, pRider->power);

            // Done!
            return 0;
        } else {
            fprintf(stderr, "ERROR: %s: invalid state! fd=%d state=%s\n", __func__, fd, riderStateTble[pRider->state]);
            return -1;
        }
    }

    return -1;
}

static int procData(Grs *pGrs, const CmdArgs *pArgs, int fd)
{
    char dataBuf[1000];
    ssize_t dataLen;

    //printf("Data available: fd=%d\n", fd);

    // Read in all the available data
    if ((dataLen = read(fd, dataBuf, sizeof (dataBuf))) > 0) {
        JsonObject msg = {0};
        if (jsonFindObject(dataBuf, dataLen, &msg) == 0) {
            const char *msgType = jsonFindTag(&msg, "type");
            if (msgType != NULL) {
                if (strncmp(msgType, "\"regReq\"", 8) == 0) {
                    procRegReqMsg(pGrs, pArgs, fd, &msg);
                } else if (strncmp(msgType, "\"progUpd\"", 9) == 0) {
                    procProgUpdMsg(pGrs, pArgs, fd, &msg);
                } else {
                    fprintf(stderr, "ERROR: unsupported message type! msgType=%s\n", msgType);
                    jsonDumpObject(&msg);
                    return -1;
                }
            } else {
                fprintf(stderr, "ERROR: JSON message has no type!\n");
                jsonDumpObject(&msg);
                return -1;
            }
        } else {
            fprintf(stderr, "ERROR: no JSON message found! fd=%d\n", fd);
            return -1;
        }
    } else {
        fprintf(stderr, "ERROR: failed to read data! fd=%d (%s)\n", fd, strerror(errno));
        return -1;
    }

    return 0;
}

int procFdEvents(Grs *pGrs, const CmdArgs *pArgs, int nFds)
{
    int s = 0;

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
            s = procDisconnect(pGrs, pArgs, pGrs->pollFds[n].fd);
        } else if (revents & POLLIN) {
            s = procData(pGrs, pArgs, pGrs->pollFds[n].fd);
        } else if (revents != 0) {
            fprintf(stderr, "ERROR: unknown event! fd=%d revents=%x\n",
                    pGrs->pollFds[n].fd, pGrs->pollFds[n].revents);
            return -1;
        }
    }

    if (pGrs->rebuildPollFds) {
        // Rebuild the pollFds array to add new connections
        // and remove stale connections...
        buildPollFds(pGrs);
    }

    return s;
}

// Send a Leaderboard message
//
// Message format:
//
//   {
//     "type": "leaderboard",
//     "riders": [
//       {"name": "<RidersName0>", "bibNum": <BibNum0>", "distance": "<DistanceInMeters0>", "power": "<PowerInWatts0>"},
//       {"name": "<RidersName1>", "bibNum": <BibNum1>", "distance": "<DistanceInMeters1>", "power": "<PowerInWatts1>"},
//           .
//           .
//           .
//       {"name": "<RidersNameN>", "bibNum": <BibNumN>", "distance": "<DistanceInMetersN>", "power": "<PowerInWattsN>"},
//     ],
//   }
//
// Example:
//
//   {
//     "type": "leaderboard",
//     "riders": [
//       {"name": "Marcelo Mourier", "bibNum": 123", "distance": "1620", "power": "200"},
//       {"name": "Patrick Fulghum", "bibNum": 124", "distance": "1840", "power": "250"},
//     ],
//   }
//  }
//
int sendReportMsg(Grs *pGrs, const CmdArgs *pArgs)
{
    printf("Sending report messages...\n");
    clock_gettime(CLOCK_REALTIME, &pGrs->lastReport);

    return 0;
}

int grsMain(Grs *pGrs, const CmdArgs *pArgs)
{
    Timespec reportPeriod = { .tv_sec = pArgs->reportPeriod, .tv_nsec = 0};

    // Initialize the lists of registered riders
    for (Gender gender = unspec; gender < GenderMax; gender++) {
        for (AgeGrp ageGrp = undef; ageGrp < AgeGrpMax; ageGrp++) {
            TAILQ_INIT(&pGrs->riderList[gender][ageGrp]);
        }
    }

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

    // If no start time was specified, make the group ride
    // active right away.
    if (pArgs->startTime == 0) {
        pGrs->rideActive = true;
    }

    while (true) {
        int nFds;
        Timespec start, end;
        Timespec deltaT = {0};
        Timespec timeout;

        // Wait for an event on any of the file descriptors we
        // are monitoring, or until the report period expires.
        tvSub(&timeout, &reportPeriod, &deltaT);
        if ((nFds = ppoll(pGrs->pollFds, pGrs->numFds, &timeout, NULL)) < 0) {
            fprintf(stderr, "ERROR: failed to wait for file descriptor events! (%s)\n", strerror(errno));
            return -1;
        }

        clock_gettime(CLOCK_REALTIME, &start);

        if (nFds > 0) {
            // Process the file descriptor events
            if (procFdEvents(pGrs, pArgs, nFds) != 0) {
                // Error message already printed
                return -1;
            }
        }

        if (pGrs->rideActive) {
            // Time to send the report messages?
            tvSub(&deltaT, &start, &pGrs->lastReport);
            if (tvCmp(&deltaT, &reportPeriod) >= 0) {
                // Send the report message to the clients
                if (sendReportMsg(pGrs, pArgs) != 0) {
                    // Error message already printed
                    return -1;
                }
            }
        } else {
            // Time to start the ride?
            time_t now = time(NULL);
            if (now >= pArgs->startTime) {
                // Ready-Set-Go!
                printf("INFO: Ready... Set... Go!\n");
                if (sendGoMsg(pGrs, pArgs) != 0) {
                    // Error message already printed
                    return -1;
                }

                pGrs->rideActive = true;
            }
        }

        clock_gettime(CLOCK_REALTIME, &end);
        tvSub(&deltaT, &end, &start);
    }

    return 0;
}

