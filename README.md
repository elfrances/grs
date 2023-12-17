# Group Ride Server

**GRS** is a server app for coordinating virtual cycling group rides that can have 100's of participants. The client side of the app is embedded in the virtual cycling app used by the user/rider. The client and the server communicate using simple JSON messages exchanged over a TCP connection.

The typical workflow is as follows:

A user who wants to join a group ride registers with the GRS, using the hostname and port provided by the GRS administartor. The registration data includes the name of the group ride they want to join, their name or alias, and optionally their gender and age, which are used to place the user in his/her correct category; e.g. 'Men 35-39", "Women 19-34", etc. If gender is specified, but age is not, the rider is registered in the "Men General" or "Women General" category. If neither gender nor age are specified, the rider is registered in the catch-all "General" category.

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
    --control-file <url>
        Specifies the URL of the ride's control file.
    --help
        Show this help and exit.
    --ip-addr <addr>
        Specifies the IP address where the GRS app will listen for
        connections. If no address is specified, the server will use
        any of the available network interfaces.
    --leaderboard-period <secs>
        Specifies the period (in seconds) the GRS app needs to send
        its "leaderboard" messages to the client apps.
    --max-riders <num>
        Specifies the maximum number of riders allowed to join the
        group ride.
    --prog-update-period <secs>
        Specifies the period (in seconds) the client app's need to send
        their "progress update" messages to the server.
    --ride-name <name>
        Specifies the name of the group ride.
    --start-time <time>
        Specifies the start date and time (in ISO 8601 UTC format) of
        the group ride; e.g. 2023-04-01T17:00:00Z
    --tcp-port <port>
        Specifies the TCP port used by the GRS app. The default is TCP
        port 50000.
    --version
        Show program's version info and exit.
    --video-file <url>
        Specifies the URL of the ride's video file.
```

Notice that the specified TCP port must be allowed by the server's firewall.  For example, if **GRS** is running on a Linux server, and the selected TCP port is 54321, the port can be open as follows:

```
$ sudo firewall-cmd --permanent --add-port=54321/tcp
$ sudo firewall-cmd --reload
```

# Example

In the following example we schedule the group ride "RPI-TCR" to start at 09:07:00 on 2023-04-05, and instruct the GRS to listen for connections on its IP address 192.168.0.249 and port 5000, and to use the default message report periods and rider limit: 

```
$ grs --ip-addr 192.168.0.249 --tcp-port 50000 --ride-name RPI-TCR --control-file http://grs.net/RPI-TCR.shiz --video-file  http://grs.net/RPI-TCR.mp4 --start-time 2023-04-05T09:07:00Z
2023-04-05T10:05:55:INFO:parseCmdArgs: rideName=RPI-TCR controlFile=http://grs.net/RPI-TCR.shiz videoFile=http://grs.net/RPI-TCR.mp4 startTime=2023-04-05T10:07:00 maxRiders=100 progUpdPeriod=1 leaderboardPeriod=2
2023-04-05T10:06:01:INFO:procConnect: New connection: sd=4 addr=208.184.70.254[ 8042]
2023-04-05T10:06:01:INFO:procRegReqMsg: Received "regReq" message: fd=4 name="John" gender=male age=30
2023-04-05T10:06:01:INFO:sendRegRespMsg: Sent "regResp" message: fd=4 name="John" bibNum=1
2023-04-05T10:06:02:INFO:procConnect: New connection: sd=5 addr=208.184.70.254[23386]
2023-04-05T10:06:02:INFO:procRegReqMsg: Received "regReq" message: fd=5 name="Peter" gender=male age=34
2023-04-05T10:06:02:INFO:sendRegRespMsg: Sent "regResp" message: fd=5 name="Peter" bibNum=2
2023-04-05T10:06:05:INFO:procConnect: New connection: sd=6 addr=208.184.70.254[26557]
2023-04-05T10:06:05:INFO:procRegReqMsg: Received "regReq" message: fd=6 name="Mary" gender=female age=21
2023-04-05T10:06:05:INFO:sendRegRespMsg: Sent "regResp" message: fd=6 name="Mary" bibNum=3
2023-04-05T10:06:07:INFO:procConnect: New connection: sd=7 addr=208.184.70.254[51802]
2023-04-05T10:06:07:INFO:procRegReqMsg: Received "regReq" message: fd=7 name="Suzie" gender=female age=23
2023-04-05T10:06:07:INFO:sendRegRespMsg: Sent "regResp" message: fd=7 name="Suzie" bibNum=4
2023-04-05T10:07:01:INFO:grsMain: Ready... Set... Go!
2023-04-05T10:07:01:INFO:sendRideStartedMsg: Sent "rideStarted" message: fd=7 name="Suzie" bibNum=4
2023-04-05T10:07:01:INFO:sendRideStartedMsg: Sent "rideStarted" message: fd=6 name="Mary" bibNum=3
2023-04-05T10:07:01:INFO:sendRideStartedMsg: Sent "rideStarted" message: fd=5 name="Peter" bibNum=2
2023-04-05T10:07:01:INFO:sendRideStartedMsg: Sent "rideStarted" message: fd=4 name="John" bibNum=1
2023-04-05T10:07:01:INFO:procProgUpdMsg: Received "progUpd" message: fd=7 name="Suzie" distance=0 power=0
2023-04-05T10:07:01:INFO:sendLeaderboardMsg: Sending "leaderboard" message: {"msgType": "leaderboard", "category": "WU35", "riderList": [{"name": "Suzie", "bibNum": "4", "distance": "0", "power": "0"}, {"name": "Mary", "bibNum": "3", "distance": "0", "power": "0"}]}
2023-04-05T10:07:01:INFO:sendLeaderboardMsg: Sending "leaderboard" message: {"msgType": "leaderboard", "category": "MU35", "riderList": [{"name": "Peter", "bibNum": "2", "distance": "0", "power": "0"}, {"name": "John", "bibNum": "1", "distance": "0", "power": "0"}]}
2023-04-05T10:07:01:INFO:procProgUpdMsg: Received "progUpd" message: fd=5 name="Peter" distance=0 power=0
2023-04-05T10:07:01:INFO:procProgUpdMsg: Received "progUpd" message: fd=4 name="John" distance=0 power=0
2023-04-05T10:07:01:INFO:procProgUpdMsg: Received "progUpd" message: fd=6 name="Mary" distance=0 power=0
2023-04-05T10:07:02:INFO:procProgUpdMsg: Received "progUpd" message: fd=5 name="Peter" distance=1 power=1
2023-04-05T10:07:02:INFO:procProgUpdMsg: Received "progUpd" message: fd=6 name="Mary" distance=1 power=1
2023-04-05T10:07:02:INFO:procProgUpdMsg: Received "progUpd" message: fd=7 name="Suzie" distance=1 power=1
2023-04-05T10:07:02:INFO:procProgUpdMsg: Received "progUpd" message: fd=4 name="John" distance=1 power=1
2023-04-05T10:07:03:INFO:procProgUpdMsg: Received "progUpd" message: fd=5 name="Peter" distance=2 power=2
2023-04-05T10:07:03:INFO:sendLeaderboardMsg: Sending "leaderboard" message: {"msgType": "leaderboard", "category": "WU35", "riderList": [{"name": "Suzie", "bibNum": "4", "distance": "1", "power": "1"}, {"name": "Mary", "bibNum": "3", "distance": "1", "power": "1"}]}
2023-04-05T10:07:03:INFO:sendLeaderboardMsg: Sending "leaderboard" message: {"msgType": "leaderboard", "category": "MU35", "riderList": [{"name": "Peter", "bibNum": "2", "distance": "2", "power": "2"}, {"name": "John", "bibNum": "1", "distance": "1", "power": "1"}]}
    .
    .
    .
2023-04-05T10:07:11:INFO:procDisconnect: Disconnected: sd=4 addr=208.184.70.254[ 8042] state=registered name="John"
2023-04-05T10:07:11:INFO:procDisconnect: Disconnected: sd=5 addr=208.184.70.254[23386] state=registered name="Peter"
2023-04-05T10:07:11:INFO:procDisconnect: Disconnected: sd=7 addr=208.184.70.254[51802] state=registered name="Suzie"
2023-04-05T10:07:11:INFO:sendLeaderboardMsg: Sending "leaderboard" message: {"msgType": "leaderboard", "category": "WU35", "riderList": [{"name": "Mary", "bibNum": "3", "distance": "9", "power": "9"}]}
2023-04-05T10:07:11:INFO:procDisconnect: Disconnected: sd=6 addr=208.184.70.254[26557] state=registered name="Mary"
```

# JSON Messages

The virtual cycling app (VCA) and the GRS communicate using simple JSON messages.

When a user wants to join a group ride, they configure their VCA with the required information, and the app sends a "Registration Request" message to the GRS to initiate the registration process.  The message has the following format:

```
   {
     "msgType": "regReq",
     "name": "<RidersName>",
     "gender": "{female|male|unspec}",
     "age": "<RidersAge>",
     "ride": "<RideName>"
   }
```

"name" is the name or nickname of the rider, used to identify him/her in the leaderboard. "gender" and "age" are the gender and age of the rider, which are used to place the rider in his/her correct category; e.g. 'Men U35", "Women U30", etc. "ride" is the name of the group ride the user wants to join.

If everything is OK, the GRS sends back a "Registration Response" message to the VCA. The message has the following format:

```
   {
     "msgType": "regResp",
     "status": "{error|success}",
     "bibNum": "<BibNumber>",
     "startTime": "<StartTimeInUTC>",
     "controlFile": "<URL>",
     "videoFile": "<URL>",
     "progUpdPeriod"="<ProgUpdPeriodInSec>"
   }
```

"status" indicates whether or not the registration was accepted. "bibNum" is the bib number assigned to the rider. "startTime" is the UTC time at which the ride is scheduled to start. "controlFile" and "videoFile" are the URL's to the control and video files of the ride. "progUpdPeriod" is the time (in seconds) the VCA should send its Progress Update messages to the GRS.  The VCA can use the URL of the control and video files to download the files, or to validate that they match the local copy they may already have in their cache.

If the registration is successful, the VCA just sits idle until the GRS sends the "Ride Started" message to all the registered riders, indicating that it is time to start pedalling.  The message has the following format:

```
   {
     "msgType": "rideStarted"
   }
```

At that pont, the VCA starts sending periodic "Progress Update" message to the GRS, to indicate the rider's position in the course, and its current speed and power values. The message has the following format:

```
   {
     "msgType": "progUpd",
     "distance": "<DistanceInMeters>",
     "power": "<PowerInWatts>",
     "speed": "<SpeedInMetersPerSec>"
   }
```

The GRS collects the data from the "Progress Update" messages sent by each of the VCA's, and periodically sends a "Leaderboard" message to all registered riders in the same category; i.e. same gender and age group.  Assuming there are N riders in the given gender and age group, the "Leaderboard" message would have the following format:

```
   {
     "msgType": "leaderboard",
     "category": "<Category>",
     "riderList": [
       {"name": "<RidersName1>", "bibNum": <BibNum1>", "distance": "<DistanceInMeters1>", "power": "<PowerInWatts1>", "speed": "<SpeedInMetersPerSec1>"},
       {"name": "<RidersName2>", "bibNum": <BibNum2>", "distance": "<DistanceInMeters2>", "power": "<PowerInWatts2>", "speed": "<SpeedInMetersPerSec2>"},
           .
           .
           .
       {"name": "<RidersNameN>", "bibNum": <BibNumN>", "distance": "<DistanceInMetersN>", "power": "<PowerInWattsN>", "speed": "<SpeedInMetersPerSecN>"},
     ],
   }
```

The VCA can then use this information to position each of the riders on a course overlay shown on the screen, allowing the rider to get a visual idea of his/her own position with respect to the other riders.


 

