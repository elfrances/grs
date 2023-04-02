# Group Ride Server

**GRS** is a server app for coordinating virtual cycling group rides that can have 100's of participants. The client side of the app is embedded in the virtual cycling app used by the user. The client and the server communicate using simple JSON messages exchanged over a TCP connection.

A user who wants to join a group ride registers with the GRS, using the hostname and port provided by the GRS's administartor. The registration data includes the name of the group ride they want to join, their name or alias, and optionally their gender and age, which are used to place the user in his/her correct category; e.g. 'Men 35-39", "Women 19-34", etc. If gender is specified, but age is not, the rider is registered in the "Men General" or "Women General" category. If neither gender nor age are specified, the rider is registered in the catch-all "General" category.

The server is configured to start the group ride at a certain date and time, at which time it sends a "Go!" message to all the registered users, so that the individual riders all start at the same time.

During the ride, each user periodically reports to the GRS his/her current location, speed, and power values. The GRS collects all these data, and periodically sends a report to each registered rider that includes the location, speed, and power values of each of the other active riders in their category.  This information is used by the virtual cycling app to keep a leaderboard, and position each of the riders in the given category on a route map overlay, allowing the rider to get a visual idea of his/her own position with respect to the other riders.

# Building the tool

**GRS** is written entirely in C. The tool is known to build under Windows/Cygwin, macOS Ventura, and Ubuntu 22.04.
 
To build the **GRS** tool all you need to do is run 'make' at the top-level directory:

```
$ make
cc -m64 -D_GNU_SOURCE -I. -ggdb -Wall -Werror -O0 -o grs.o -c grs.c
cc -m64 -D_GNU_SOURCE -I. -ggdb -Wall -Werror -O0 -o json.o -c json.c
cc -m64 -D_GNU_SOURCE -I. -ggdb -Wall -Werror -O0 -o main.o -c main.c
cc -ggdb  -o ./grs ./grs.o ./json.o ./main.o
```

# Usage

Running the tool with the --help argument will print the list of available options:

```
SYNTAX:
    grs [OPTIONS]

    GRS is a server app for coordinating virtual cycling group rides that
    can have 100's of participants.

OPTIONS:
    --help
        Show this help and exit.
    --ip-addr <addr>
        Specifies the IP address where the GRS app will listen for
        connections. If no address is specified, the server will use
        any of the available network interfaces.
    --max-riders <num>
        Specifies the maximum number of riders allowed to join the
        group ride.
    --report-period <secs>
        Specifies the period (in seconds) the client app's need to send
        their update messages to the server.
    --ride-name <name>
        Specifies the name of the group ride.
    --start-time <time>
        Specifies the start date and time (in ISO 8601 UTC format) of
        the group ride.
    --tcp-port <port>
        Specifies the TCP port used by the GRS app. The default is TCP
        port 50000.
    --version
        Show program's version info and exit.
```

Notice that the specified TCP port must be allowed by the server's firewall.  For example, if **GRS** is running on a Linux server, and the selected TCP port is 54321, the port can be open as follows:

```
$ sudo firewall-cmd --permanent --add-port=54321/tcp
$ sudo firewall-cmd --reload
```
