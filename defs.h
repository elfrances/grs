#pragma once

#include <sys/queue.h>
#include <sys/socket.h>
#include <time.h>

typedef struct CmdArgs {
    int maxRiders;                      // Max number of riders that can join the group ride
    int reportPeriod;                   // Period (in seconds) the client app needs to send its progress update messages
    struct sockaddr_storage sockAddr;   // IP address and TCP port (in network byte order) used by GRS to listen for client connections
    time_t startTime;                   // Start date/time (in UTC) for the group ride
    int tcpPort;                        // TCP port used by the listening socket
} CmdArgs;

typedef enum Gender {
    unspec = 0,
    female = 1,
    male = 2,
    GenderMax = 3
} Gender;

typedef enum AgeGrp {
    undef = 0,      // undefined
    u19 = 1,        // 0-18
    u35 = 2,        // 19-34
    u40 = 3,        // 35-39
    u45 = 4,        // 40-44
    u50 = 5,        // 45-49
    u55 = 6,        // 50-54
    u60 = 7,        // 55-59
    u65 = 8,        // 60-64
    u70 = 9,        // 65-69
    u75 = 10,       // 70-74
    u80 = 11,       // 75-79
    u85 = 12,       // 80-84
    u90 = 13,       // 85-89
    u95 = 14,       // 90-94
    u100 = 15,      // 95-99
    AgeGrpMax = 16
} AgeGrp;

// Rider
typedef struct Rider {
    int age;                            // rider's age (as of Dec 31 of the current year)
    AgeGrp ageGrp;                      // rider's age group
    Gender gender;                      // rider's gender
    char *name;                         // rider's name or alias
    time_t regTime;                     // time (UTC) the rider registered with the GRS
    int sd;                             // file descriptor of the connected socket
    struct sockaddr_storage sockAddr;   // remote IP address and TCP port
} Rider;

// Group Ride Server
typedef struct Grs {
    // List of registered riders
    TAILQ_HEAD(RiderList, Rider) riderList[GenderMax][AgeGrpMax];
    int sd;                             // file descriptor of the listening socket
} Grs;

#ifdef __cplusplus
extern "C" {
#endif

static __inline__ socklen_t ssLen(const struct sockaddr_storage *sockAddr) { return (sockAddr->ss_family == AF_INET) ? sizeof (struct sockaddr_in) : sizeof (struct sockaddr_in6); }

#ifdef __cplusplus
}
#endif

