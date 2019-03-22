#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <regex>
#include <unistd.h>

#define TIMEOUT 15
#define BUF_LEN 1024

int main(int argc, char* argv[]) {

	// Checking that the argument count is exactly 4
	if (argc != 4) {
		std::cerr << "ERROR: usage: " << argv[0] << " <HOSTNAME-OR-IP> <PORT> <FILENAME>\n";
		exit(1);
	}

	// Saving command line arguments into variables
	char* hostname_or_ip = argv[1];
	char* port = argv[2];
	char* filename = argv[3];

	// Validating hostname or IP address
	std::regex hostname("^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])$");
	std::regex ip("^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	if (!regex_match(hostname_or_ip, hostname) && !regex_match(hostname_or_ip, ip)) {
		std::cerr << "ERROR: Invalid hostname or IP address\n";
		exit(1);
	}

	// If the input is not an IP address, attempt to convert the input into one
	char* ip_address = hostname_or_ip;
	if (!regex_match(hostname_or_ip, ip)) {
		hostent* record = gethostbyname(hostname_or_ip);
		if (!record) {
			std::cerr << "ERROR: Invalid hostname or IP address\n";
			exit(1);
		}
		in_addr* address = (in_addr*)record->h_addr;
		ip_address = inet_ntoa(*address);
	}

	// Validating port argument
	char *end;
	uint16_t server_port = (uint16_t) strtol(port, &end, 10);
	if (*end != '\0' || server_port < 1024) {
		std::cerr << "ERROR: Invalid port number\n";
		exit(1);
	} 

	// Setting the server's IP address and port #
	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(server_port);
	serverAddr.sin_addr.s_addr = inet_addr(ip_address);
	memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));

	// Creating a socket with TCP IP
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("ERROR");
		exit(1);
	}

	// Set socket to be non-blocking
	long arg = fcntl(sockfd, F_GETFL, NULL);
	if (arg == -1) { 
		perror("ERROR");
		close(sockfd);
		exit(1);
	} 
	arg |= O_NONBLOCK; 
	if (fcntl(sockfd, F_SETFL, arg) == -1) { 
		perror("ERROR");
		close(sockfd);
		exit(1);
	}

	// Attempt to connect unless timeout exceeds TIMEOUT
	struct timeval tv;
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;
	if (connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {

		// If the connection attempt is still in progress
		if (errno == EINPROGRESS) {
			do {

				// Use select to check for a timeout
				fd_set wset;
				FD_ZERO(&wset);
				FD_SET(sockfd, &wset);
           		int select_res = select(sockfd + 1, NULL, &wset, NULL, &tv); 

           		// If select returns 0, the connection timed out
           		if (select_res == 0) {
           			std::cerr << "ERROR: Connection timeout\n";
           			close(sockfd);
					exit(1);
           		}

           		// If select failed
           		if (select_res < 0) {
           			perror("ERROR");
           			close(sockfd);
           			exit(1);
           		}

           		// If file descriptor is set
				if (FD_ISSET(sockfd, &wset)) {
					int error;
					socklen_t len = sizeof(error);
					if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
						perror("ERROR");
						close(sockfd);
						exit(1);
					}

					// If the server cannot be connected to
					if (error) {
						std::cerr << "ERROR: Connection refused\n";
						close(sockfd);
						exit(1);
					}

					// Connected successfully before the timeout
					break;
				}
			} while (true);
		} else {
			perror("ERROR");
			close(sockfd);
			exit(1); 
		}
	}

	// Reading the contents of the specified file
	FILE* f = fopen(filename, "r");
	if (!f) {
		perror("ERROR");
		close(sockfd);
		exit(1);
	}

	// Sending the specified file to the server
	char buf[BUF_LEN];
	bzero(buf, BUF_LEN);
	int block_size = 0;
	while ((block_size = fread(buf, sizeof(char), BUF_LEN, f)) > 0) {
		int send_res = send(sockfd, buf, block_size, MSG_NOSIGNAL);

		// Use select to check for a timeout
		fd_set wset;
		FD_ZERO(&wset);
		FD_SET(sockfd, &wset);
		int select_res = select(sockfd + 1, NULL, &wset, NULL, &tv);

		// If select returns 0, the connection timed out
		if (select_res == 0) {
			std::cerr << "ERROR: Send timeout\n";
			close(sockfd);
			fclose(f);
			exit(1);	
		}

		// If select failed
		if (select_res < 0) {
			perror("ERROR");
			close(sockfd);
			fclose(f);
			exit(1);
		}

		// If send fails
		if (send_res == -1) {
			perror("ERROR");
			close(sockfd);
			fclose(f);
			exit(1);
		}

		bzero(buf, BUF_LEN);
	}

	// Set to blocking mode again
	arg = fcntl(sockfd, F_GETFL, NULL);
	if (arg == -1) {
		perror("ERROR");
		close(sockfd);
		exit(1);
	}
	arg &= (~O_NONBLOCK);
	if (fcntl(sockfd, F_SETFL, arg) == -1) {
		perror("ERROR");
		close(sockfd);
		exit(1); 
	}

	// Exiting the program normally
	close(sockfd);
	fclose(f);
	exit(0);
}
