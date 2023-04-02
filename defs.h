#pragma once

#include <poll.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <time.h>

// Default TCP port for the listening socket
#define DEF_TCP_PORT    50000

// Handy type aliases
typedef struct pollfd PollFd;
typedef struct sockaddr SockAddr;
typedef struct sockaddr_in SockAddrIn;
typedef struct sockaddr_in6 SockAddrIn6;
typedef struct sockaddr_storage SockAddrStore;
typedef struct timespec Timespec;

typedef enum Bool {
    false = 0,
    true = 1
} Bool;

typedef struct CmdArgs {
    int maxRiders;              // Max number of riders that can join the group ride
    int reportPeriod;           // Period (in seconds) the client app needs to send its progress update messages
    char *rideName;             // the name of the group ride
    char *shizFile;				// the URL of the ride's SHIZ file
    SockAddrStore sockAddr;     // IP address and TCP port (in network byte order) used by GRS to listen for client connections
    time_t startTime;           // Start date/time (in UTC) for the group ride
    int tcpPort;                // TCP port used by the listening socket
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

typedef enum RiderState {
    unknown = 0,    //
    connected = 1,  // Connected but not yet registered
    registered = 2, // Registered but not yet active
    active = 3      // Active
} RiderState;

// Rider
typedef struct Rider {
    AgeGrp ageGrp;              // rider's age group
    int bibNum;                 // rider's bib number
    int distance;               // rider's current distance (in meters) so far
    Gender gender;              // rider's gender
    char *name;                 // rider's name or alias
    time_t regTime;             // time (UTC) the rider registered with the GRS
    int sd;                     // file descriptor of the connected socket
    SockAddrStore sockAddr;     // remote IP address and TCP port
    RiderState state;           // rider's current state

    TAILQ_ENTRY(Rider) tqEntry; // node in the riderList
} Rider;

// Group Ride Server
typedef struct Grs {
    Timespec lastReport;        // time the last report was sent to the clients
    int numFds;                 // number of entries in the pollFds array
    int numRegRiders;           // current number of registered riders
    PollFd *pollFds;            // array of file descriptors to be monitored
    Bool rebuildPollFds;        // pollFds array needs to be rebuilt
    Bool rideActive;            // is the group ride active?

    // List of registered riders per gender and age group
    TAILQ_HEAD(RiderList, Rider) riderList[GenderMax][AgeGrpMax];

    int sd;                     // file descriptor of the listening socket
} Grs;

#ifdef __cplusplus
extern "C" {
#endif

static __inline__ socklen_t ssLen(const SockAddrStore *sockAddr) { return (sockAddr->ss_family == AF_INET) ? sizeof (SockAddrIn) : sizeof (SockAddrIn6); }

// Compare the Timespec values X and Y, returning
// -1, 0, 1 depending on whether X is less, equal, or
// larger than Y.
static __inline__ int tvCmp(const Timespec *x, const Timespec *y)
{
    if (x->tv_sec < y->tv_sec)
        return -1;
    if (x->tv_sec > y->tv_sec)
        return 1;
    if (x->tv_nsec < y->tv_nsec)
        return -1;
    if (x->tv_nsec > y->tv_nsec)
        return 1;
    return 0;
}

// Subtract the Timespec values X and Y, storing the result
// in RESULT. NOTICE: assumes X is larger than Y (i.e. the
// result is positive.)
static __inline__ void tvSub(Timespec *result, const Timespec *x, const Timespec *y)
{
    if (x->tv_nsec >= y->tv_nsec) {
        result->tv_sec = x->tv_sec - y->tv_sec;
        result->tv_nsec = x->tv_nsec - y->tv_nsec;
    } else {
        result->tv_sec = (x->tv_sec - 1) - y->tv_sec;
        result->tv_nsec = (x->tv_nsec + 1000000) - y->tv_nsec;
    }
}

#ifdef __cplusplus
}
#endif

