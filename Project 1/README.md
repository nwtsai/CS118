# CS118 Project 1
Nathan Tsai, 304575323

## High Level Design
For the client, I parse the arguments, connect to the correct server IP address and port, create the socket, and attempt to connect to the server with a 15 second timeout. I used the select() function to determine if connect() is successful or timed out. Once connected, I sent the file 1024 bytes at a time. As long as there is more data to read from the file, the client continues to send the data to the server through the send() socket function. I used select() again to determine whether or not the send() function is timed out. Finally, I close the connection and the file normally once the file is completely sent to the server, exiting with a zero code to indicate the expected program behavior.

For the server, I parse the arguments, install signal handlers, validate the IP address or convert the hostname to a valid IP address, and then validate the port. I create a directory string to save all of the received files in. I use setsockopt() to allow the address for reuse. I bind the address to the socket and then set the server's status to listening. After that, I detach a thread each time a new connection is accepted. Each detached thread that handles the connection is assigned a connection id, which is used to construct the final file's name. I use the recv() socket function to receive data from the client in 1024 byte chunks. If a timeout is detected using the select() function, the file is cleared and ERROR is written to the file. After the entire file is received, the socket is closed and the program exits normally. If the client's connection closes normally, the recv() function will detect that and the server will terminate the connection. If the recv() call times out past 15 seconds, that is when the timeout error is printed and the connection is terminated.

## Problems I Ran Into
Notably, the biggest problem I had with this project was figuring out the timeout functionality. It took reading the man pages for how to use select() in harmony with recv(), send(), and connect(). Multithreading was also a big challenge. Once I figured out that I just needed to detach a thread once a connection is accepted, the code became concise and straightforward. Overall, the problems I encountered were overcome with reading up on relevant documentation.

## Additional Libraries Used
server.cpp:
```
#include <arpa/inet.h>
#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <string.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
```

client.cpp:
```
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <regex>
#include <unistd.h>
```

## Online Tutorials
* https://linux.die.net/man/
* https://stackoverflow.com/

## Makefile

This provides a couple make targets for things.
By default (all target), it makes the `server` and `client` executables.

It provides a `clean` target, and `tarball` target to create the submission file as well.

You will need to modify the `Makefile` to add your userid for the `.tar.gz` turn-in at the top of the file.

## Academic Integrity Note

You are encouraged to host your code in private repositories on [GitHub](https://github.com/), [GitLab](https://gitlab.com), or other places.  At the same time, you are PROHIBITED to make your code for the class project public during the class or any time after the class.  If you do so, you will be violating academic honestly policy that you have signed, as well as the student code of conduct and be subject to serious sanctions.

## Provided Files

`server.cpp` and `client.cpp` are the entry points for the server and client part of the project.
