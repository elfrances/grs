# Group Ride Server

GRS is a server app for coordinating virtual cycling group rides that can have 100's of participants. The client side of the app is embedded in the virtual cycling app used by the user. The client and the server communicate using JSON messages exchanged over the Internet.

A user who wants to join a group ride registers with the GRS, using the hostname and port provided by the GRS's administartor. The registration data includes the name of the group ride they want to join, their name or alias, and optionally their gender and age, which are used to place the user in his/her correct category; e.g. 'Men 35-39", "Women 19-34", etc. If gender is specified, but age is not, the rider is registered in the "Men General" or "Women General" category. If neither gender nor age are specified, the rider is registered in the catch-all "General" category.

The server is configured to start the group ride at a certain fixed time, at which time it sends a "Go!" message to all the registered users, so that the individual riders all start at the same time.

During the ride, each user reports to the GRS, once per second, their current location, speed, and power values. The GRS collects all these data, and once a second it sends a report to each registered rider that includes the location, speed, and power values of each of the other registered riders in their category.  This information is used by the virtual cycling app to position each of the riders in the given category on a route map overlay, allowing the rider to get a visual idea of his/her own position with respect to the other riders.

