# CS118 Project 2

## Contributors
* Nathan Tsai (304575323) - Responsible for the client features such as the handshake, fragmenting the file, sending the file, retransmitting file fragments, closing the connection, and maintaining congestion control state variables to limit the network activity
* Arghya Mukherjee (905225938) - Responsible for server features & setting up initial UDP header structure, and 3-way handshake. Implemented the server-side features like starting a connection, responding to different types of packets from multiple clients, and finally reconstructing the file once a client has completed sending

## Makefile

This provides a couple make targets for things.
By default (all target), it makes the `server` and `client` executables.

It provides a `clean` target, and `tarball` target to create the submission file as well.

## Provided Files

`server.cpp` and `client.cpp` are the entry points for the server and client part of the project. `udpheader.h` contains useful definitions for UDP packet creation and header elements, and `udpfunctions.h` contains a helper function for packet sending.

## Wireshark dissector

For debugging purposes, you can use the wireshark dissector from `tcp.lua`. The dissector requires
at least version 1.12.6 of Wireshark with LUA support enabled.

To enable the dissector for Wireshark session, use `-X` command line option, specifying the full
path to the `tcp.lua` script:

    wireshark -X lua_script:./confundo.lua

To dissect tcpdump-recorded file, you can use `-r <pcapfile>` option. For example:

    wireshark -X lua_script:./confundo.lua -r confundo.pcap

## High Level Design

### Client
* Verifies user-provided parameters
* Opens a connection to the server
* Initially reads the entire file into a char vector
* Initializes congestion control variables `cwnd` and `ssthresh`
* UDP Packet creation is done in `udpheader.h`, so the client simply calls this interface when data needs to be sent 
* Initializes handshake by sending a SYN packet to the server
* Sets `recvfrom` to be a non-blocking operation to keep track of timeouts
* If handshake SYN-ACK packet received from server, begin sending file
* Maintains two pointers (indices) into the char file vector
	* `first_unsent_byte` is the location of the most recent byte that hasn't been transmitted to the server
	* `first_unacked_byte` is the location of the most recent byte that hasn't been acknowledged by the server
* Calculates the size of the packet to send and update the pointers into the char vector
	* Usually, packets are of size 512 bytes if we are examining a block of data within the file
	* However, The last chunk of the file is usually less than 512 bytes and must be sent and packaged accordingly
* Uses a set to keep track of which packets have been sent so we know which packets are duplicates
* Sends up to cwnd bytes since the `first_unsent_byte`
* Creates and sends the UDP packet
* Once we've sent out our packets, we wait to receive the corresponding acknowledgements
	* If 0.5s has passed since sending the packets, re-transmit the file data
* Waits for an expected ACK from the server, if expected then update congestion control variables
* Uses the `chrono` time library to keep track of the server's responsiveness
* Uses `pollfd` to detect timeouts for when to re-transmit packets and when to reset the congestion control variables
* Once the file is completely sent to the server, close the connection with a FIN packet
* ACK all FIN responses from the server for two seconds and drop all other non-FIN packets

### Server
* Verifies user-provided parameters
* Starts server on a user-specified port
* Creates user-specified directory if the directory doesn't already exist
* Creates a socket and waits on `recvfrom()` to receive from clients
* UDP Packet creation is done in `udpheader.h`, so the server simply calls this interface packet needs to be created
* UDP Packet sending is done in `udpfunctions.h`, so the server simply calls this interface when data needs to be sent 
* If incoming packet is a SYN packet- it's the start of a new connection
	* Assign a new `connId` to the client, and send the SYN-ACK packet.
	* For every data acket from this client after this point,the packets will be stored in `v[connID]`
* If incoming packet is a data packet, check if that packet has been previously received from that client's `connID`. 
	* If no, it means it is the next expected packet. Push the packet in `v[connID]`, and send corresponding ACK
	* If yes, this has been previously received- drop the packet, and send ACK for expected `seqnum`
* If incoming packet is a FIN packet, the client has finished sending
	* Lookup the client's corresponding vector of packets
	* Write to `connId.file` all the payloads of the packets in client's vector
* If incoming packet is an ACK packet, there are two cases
	* It's the ACK after SYN sent by client - client's packet vector is empty, do nothing
	* It's the ACK after the FIN and maybe the FIN packet got lost- reconstruct the client's file, as in the FIN case
* The server uses CUMULATIVE acknowledgements. That is, if it sends acknum# x, every seqnum# upto (x-1) has been received properly
* Server calls `print_log` everytime it receives, sends or drops a packet, according to the format specified


## Problems and Solutions
* How to send the UDP header in exactly the specified format was initially a problem
	* This was solved by defining a struct for the UDP packet, and using the `reinterpret_cast` to cast it to a char pointer and send
* Since the server needs to support multiple clients, reconstructing the files after all the packets is an issue, since there maybe reordering among different clients sending the packets, and also reordering of packets for a single client
	* This problem was solved by maintaining a separate vector of packets for each client,that is, a vector of vector of packets, pushed in sequential order for each client, and dropping any packet that has been previously received
* The multiple timeout features (detecting unresponsive server or re-transmission timeouts) was difficult to solve
	* The complexity arises in simultaneously keeping track of two different timers
	* The problem was solved by using the `chrono` library to keep track of the unresponsive server and `pollfd` to detect if there exists data to receive from within the time frame of 0.5s

## Additional Libraries Used

### Client
```
#include <algorithm>
#include <chrono>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <bits/stdc++.h>
#include <poll.h>
```

### Server
```
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <bits/stdc++.h>
```

## Online References
* http://www.cplusplus.com/reference/chrono/
* https://www.geeksforgeeks.org/socket-programming-cc/
* https://pubs.opengroup.org/onlinepubs/7908799/xns/syssocket.h.html
