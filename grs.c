#include <errno.h>
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
    if (bind(pGrs->sd, (struct sockaddr *) &pArgs->sockAddr, ssLen(&pArgs->sockAddr)) != 0) {
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

    return 0;
}

int grsMain(Grs *pGrs, const CmdArgs *pArgs)
{
    // Open the listening TCP socket
    if (configGrsSock(pGrs, pArgs) != 0) {
        // Error message already printed
        return -1;
    }

    return 0;
}

