#pragma once

#include <sys/socket.h>

typedef struct CmdArgs {
    struct sockaddr_storage sockAddr;   // IP address and TCP port used by GRS to listen for client connections
} CmdArgs;
