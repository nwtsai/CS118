#include <arpa/inet.h>
#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <string.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

#define TIMEOUT 15
#define BUF_LEN 1024

void handle_connection(int, int, std::string);
void handle_signal(int signal);

int main(int argc, char* argv[]) {

	// Initiating signal handlers
	std::signal(SIGQUIT, handle_signal);
	std::signal(SIGTERM, handle_signal);

	// Checking that the argument count is exactly 3
	if (argc != 3) {
		std::cerr << "ERROR: usage: " << argv[0] << " <PORT> <FILE-DIR>\n";
		exit(1);
	}

	// Saving command line arguments into variables
	char* port = argv[1];
	char* directory = argv[2];

	// Validating port argument
	char *end;
	uint16_t server_port = (uint16_t) strtol(port, &end, 10);
	if (*end != '\0' || server_port < 1024) {
		std::cerr << "ERROR: Invalid port number\n";
		exit(1);
	}

	// Construct the directory string from the user input
	std::string directory_string(directory);
	if (directory_string.size() > 0) {
		if (directory_string[0] != '.') {
			directory_string = "." + directory_string;
		}
		if (directory_string[directory_string.size() - 1] != '/') {
			directory_string += "/";
		}
	}

	// Creating a socket with TCP IP
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("ERROR");
		exit(1);
	}

	// Allowing address reuse
	int yes = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("ERROR");
		exit(1);
	}

	// Binding address to socket
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(server_port);
	addr.sin_addr.s_addr = INADDR_ANY;
	memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));
	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("ERROR");
		exit(1);
	}

	// Setting the socket to listen status
	if (listen(sockfd, 256) == -1) {
		perror("ERROR");
		exit(1);
	}

	// Accepting new connections, creating a thread for each accepted connection
	int connection_id = 1;
	int newsockfd;
	struct sockaddr_in clientAddr;
	socklen_t clientAddrSize = sizeof(clientAddr);
	while ((newsockfd = accept(sockfd, (struct sockaddr*)&clientAddr, &clientAddrSize))) {

		// Detach a thread that receives the file once a connection is established
		std::thread t(handle_connection, newsockfd, connection_id++, directory_string);
		t.detach();
	}
	exit(0);
}

void handle_connection(int sock, int connection_id, std::string directory) {

	// If the directory does not exist, create it
	struct stat buffer;
	if (stat (directory.c_str(), &buffer) == -1) {
		if (mkdir(directory.c_str(), 0777) == -1) {
			perror("ERROR");
			close(sock);
			exit(1);
		}
	}

	// Create an empty file and save its file descriptor
	std::string file_path = directory + std::to_string(connection_id) + ".file";
	FILE* f = fopen(file_path.c_str(), "w");
	if (!f) {
		perror("ERROR");
		close(sock);
		exit(1);
	}

	// Set socket to be non-blocking
	long arg = fcntl(sock, F_GETFL, NULL);
	if (arg == -1) { 
		perror("ERROR");
		close(sock);
		exit(1);
	} 
	arg |= O_NONBLOCK; 
	if (fcntl(sock, F_SETFL, arg) == -1) { 
		perror("ERROR");
		close(sock);
		exit(1);
	}

	// Setting the socket to timeout after TIMEOUT seconds of not receiving any data
	struct timeval tv;
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;

	// Writing to the new file by receiving the file contents from the client
	char buf[BUF_LEN];
	bzero(buf, BUF_LEN);
	int block_size = 0;
	do {
		block_size = recv(sock, buf, BUF_LEN, 0);

		// Use select to check for a timeout
		fd_set rset;
		FD_ZERO(&rset);
		FD_SET(sock, &rset);
		int select_res = select(sock + 1, &rset, NULL, NULL, &tv);

		// If select returns 0, the connection timed out
		if (select_res == 0) {
			std::cerr << "ERROR: Receive timeout\n";

			// Rewrite the file
			fclose(f);
			f = fopen(file_path.c_str(), "w");
			if (!f) {
				perror("ERROR");
				close(sock);
				exit(1);
			}

			// Write the ERROR message into the file
			char error_buf[] = {'E', 'R', 'R', 'O', 'R'};
			if (fwrite(error_buf, sizeof(char), sizeof(error_buf), f) == 0) {
				perror("ERROR");
				close(sock);
				fclose(f);
				exit(1);
			}
			break;
		}

		// If select failed
		if (select_res < 0) {
			perror("ERROR");
			close(sock);
			fclose(f);
			exit(1);
		}

		// No more blocks read from recv
		if (block_size <= 0) {
			break;
		}

		// If there is something to read
		int write_size = fwrite(buf, sizeof(char), block_size, f);
		if (write_size < block_size) {
			perror("ERROR");
			close(sock);
			fclose(f);
			exit(1);
		}
		bzero(buf, BUF_LEN);
	} while (true);

	// Set to blocking mode again
	arg = fcntl(sock, F_GETFL, NULL);
	if (arg == -1) {
		perror("ERROR");
		close(sock);
		fclose(f);
		exit(1);
	}
	arg &= (~O_NONBLOCK);
	if (fcntl(sock, F_SETFL, arg) == -1) {
		perror("ERROR");
		close(sock);
		fclose(f);
		exit(1);
	}

	close(sock);
	fclose(f);
}

void handle_signal(int signal) {
	exit(0);
}
