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
#include "udpfunctions.h"

using namespace std;

void signalHandler(int signum) {
   cerr << "INTERRUPT: Interrupt signal (" << signum << ") received.\n";
   exit(signum);
}

int fd_set_blocking(int sockfd, bool blocking) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        return 0;
    } else if (blocking) {
      flags &= ~O_NONBLOCK;
    } else {
      flags |= O_NONBLOCK;
    }
    return fcntl(sockfd, F_SETFL, flags) != -1;
}

void print_log(int type, unsigned int seqnum, unsigned int acknum,
  short int connId, long cwnd, long ssthresh, bool isAck, bool isSyn,
  bool isFin, bool isDup = false) {
  if (type == 0) {
    cout << "RECV ";
  } else if (type == 1) {
    cout << "SEND ";
  } else if (type == 2) {
    cout << "DROP ";
  }
  cout << seqnum << " " << acknum << " " << connId << " ";
  cout << cwnd << " " << ssthresh;
  if (isAck) {
    cout << " ACK";
  }
  if (isSyn) {
    cout << " SYN";
  }
  if (isFin) {
    cout << " FIN";
  }
  if (isDup) {
    cout << " DUP";
  }
  cout << endl;
}

int main(int argc, char const *argv[]) {
  if (argc != 4) {
    cerr << "ERROR: Invalid number of arguments" << endl;
    exit(1);
  }

  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);
  signal(SIGQUIT, signalHandler);

  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    cerr << "ERROR: Socket creation failed" << endl;
    exit(1);
  }

  struct hostent *host;
  stringstream geek(argv[2]);
  short port = 0;
  geek >> port;

  // Verify port number
  if (port < 1023 || port > 65535) {
    cerr << "ERROR: Incorrect port" << endl;
    close(sockfd);
    exit(1);
  }

  // Verify host name
  host = gethostbyname(argv[1]);
  if (host->h_name == NULL) {
    cerr << "ERROR: Invalid hostname" << endl;
    close(sockfd);
    exit(1);
  }

  in_addr* address = (in_addr*)host->h_addr;
  const char* ip_address = inet_ntoa(*address);

  // Create the server address struct to connect to
  struct sockaddr_in serverAddr;
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(port);
  serverAddr.sin_addr.s_addr =  inet_addr(ip_address);
  memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));
  socklen_t serverAddr_len = sizeof(serverAddr);

  // Attempt to open the file
  const char* fname = argv[3];
  FILE* f = fopen(fname, "r");
  if (!f) {
    cerr << "ERROR: Cannot open file" << endl;
    exit(1);
  }

  // Read the entire file into a char vector
  vector<char> file;
  char buf[1];
  bzero(buf, 1);
  int block_size = 0;
  while ((block_size = fread(buf, sizeof(char), 1, f) > 0)) {
    file.push_back(buf[0]);
    bzero(buf, 1);
  }
  if (block_size < 0) {
    cerr << "ERROR: Problem with reading file" << endl;
    close(sockfd);
    exit(1);
  }

  // Congestion control variables
  long cwnd = DATABUF;
  long ssthresh = INITSSTHRESH;

  // Send SYN packet to initiate the connection
  UDPpacket* pkt_syn = new UDPpacket(htonl(CLNT_DEFAULT_SEQ), htonl(0), 0, 0, 1,
    0, NULL);
  UDPsend(pkt_syn, sockfd, serverAddr, pkt_syn->getheadersize());
  print_log(1, pkt_syn->getSeq(), pkt_syn->getAck(), pkt_syn->getconnID(),
    cwnd, ssthresh, pkt_syn->isAck(), pkt_syn->isSyn(), pkt_syn->isFin());

  // Make recv non-blocking to track the 10s server timeout
  fd_set_blocking(sockfd, false);

  // Saving the connection ID
  short int connectionID = 0;

  // Save the current time and begin waiting for a response
  chrono::steady_clock::time_point syn_start = chrono::steady_clock::now();
  char rec[MAXBUF];
  while (1) {
    if (chrono::steady_clock::now() - syn_start > chrono::seconds(10)) {

      // If there is no response from the server after 10 seconds
      cerr << "ERROR: No response from server" << endl;
      close(sockfd);
      exit(1);
    }

    // Poll the socket file descriptor for half of a second
    struct pollfd pfd;
    pfd.fd = sockfd;
    pfd.events = POLLIN;
    int poll_res = poll(&pfd, 1, 500);
    if (poll_res == -1) {
      cerr << "ERROR: Could not poll socket" << endl;
      close(sockfd);
      exit(1);
    }

    // If timeout, retransmit
    if (poll_res == 0) {
      UDPpacket* pkt_syn = new UDPpacket(htonl(CLNT_DEFAULT_SEQ), htonl(0),
        htons(connectionID), 0, 1, 0, NULL);
      UDPsend(pkt_syn, sockfd, serverAddr, pkt_syn->getheadersize());
      print_log(1, pkt_syn->getSeq(), pkt_syn->getAck(),
        pkt_syn->getconnID(), cwnd, ssthresh, pkt_syn->isAck(),
        pkt_syn->isSyn(), pkt_syn->isFin(), true);
      continue;
    }

    // Here, we have data to read
    bzero(rec, MAXBUF);
    int recv_res = recvfrom(sockfd, (char*)rec, MAXBUF, 0,
      (struct sockaddr*) &serverAddr, &serverAddr_len);
    if (recv_res == -1) {
      continue;
    }
    UDPpacket* pkt_in = reinterpret_cast<UDPpacket*> (rec);
    print_log(0, pkt_in->getSeq(), pkt_in->getAck(), pkt_in->getconnID(),
      cwnd, ssthresh, pkt_in->isAck(), pkt_in->isSyn(), pkt_in->isFin());

    // If we receive a SYN-ACK, finish the 3-way handshake and break
    if (pkt_in->isSyn() && pkt_in->isAck()) {
      connectionID = pkt_in->getconnID();

      // Send the handshake ACK
      UDPpacket* pkt_syn_ack = new UDPpacket(
        htonl(pkt_in->getAck() % (MAXSEQACKNUM + 1)),
        htonl((pkt_in->getSeq() + 1) % (MAXSEQACKNUM + 1)),
        htons(pkt_in->getconnID()), 1, 0, 0, NULL);
      UDPsend(pkt_syn_ack, sockfd, serverAddr, pkt_syn_ack->getheadersize());
      print_log(1, pkt_syn_ack->getSeq(), pkt_syn_ack->getAck(),
        pkt_syn_ack->getconnID(), cwnd, ssthresh, pkt_syn_ack->isAck(),
        pkt_syn_ack->isSyn(), pkt_syn_ack->isFin());
      break;
    }
  }

  // State variables for sending the file
  long first_unsent_byte = 0;
  long first_unacked_byte = 0;
  bool finished_sending = false;
  bool finished_receiving = false;

  // Unordered set to check if we've already transmitted this file
  unordered_set<long> sent_bytes;

  // Continue looping if we are still sending or receiving UDP packets
  while (!finished_sending || !finished_receiving) {

    long unsent_bytes = file.size() - first_unsent_byte;
    int bytes_to_send = min(unsent_bytes, (long) DATABUF);
    if (bytes_to_send <= 0) {
      finished_sending = true;
    }

    // Send up to cwnd bytes
    while (!finished_sending && first_unsent_byte - first_unacked_byte +
      bytes_to_send <= cwnd) {

      // Create the UDP payload
      char payload[bytes_to_send];
      bzero(payload, bytes_to_send);
      copy(file.begin() + first_unsent_byte, file.begin() + first_unsent_byte +
        bytes_to_send, payload);

      // Create and send the UDP packet
      UDPpacket* pkt_file = new UDPpacket(
        htonl((first_unsent_byte + CLNT_DEFAULT_SEQ + 1) % (MAXSEQACKNUM + 1)),
        htonl(0), htons(connectionID), 0, 0, 0, payload, bytes_to_send);
      UDPsend(pkt_file, sockfd, serverAddr, bytes_to_send +
        pkt_file->getheadersize());

      // It is a DUP only if we've seen it in the set already
      bool isDUP = false;
      if (sent_bytes.find(first_unsent_byte) == sent_bytes.end()) {
        sent_bytes.insert(first_unsent_byte);
      } else {
        isDUP = true;
      }
      print_log(1, pkt_file->getSeq(), pkt_file->getAck(),
        pkt_file->getconnID(), cwnd, ssthresh, pkt_file->isAck(),
        pkt_file->isSyn(), pkt_file->isFin(), isDUP);

      // Update state variables
      first_unsent_byte += bytes_to_send;
      unsent_bytes = file.size() - first_unsent_byte;
      bytes_to_send = min(unsent_bytes, (long) DATABUF);
      if (bytes_to_send <= 0) {
        finished_sending = true;
      }
    }

    long unreceived_bytes = file.size() - first_unacked_byte;
    int bytes_to_receive = min(unreceived_bytes, (long) DATABUF);
    if (bytes_to_receive <= 0) {
      finished_receiving = true;
    }

    // Initialize server timeout clock reference
    chrono::steady_clock::time_point ack_start = chrono::steady_clock::now();
    while (!finished_receiving && first_unacked_byte < first_unsent_byte) {
      if (chrono::steady_clock::now() - ack_start > chrono::seconds(10)) {

        // If there is no response from the server after 10 seconds
        cerr << "ERROR: No response from server" << endl;
        close(sockfd);
        exit(1);
      } else {

        // Poll the socket file descriptor for half of a second
        struct pollfd pfd;
        pfd.fd = sockfd;
        pfd.events = POLLIN;
        int poll_res = poll(&pfd, 1, 500);
        if (poll_res == -1) {
          cerr << "ERROR: Could not poll socket" << endl;
          close(sockfd);
          exit(1);
        }

        // If timeout, retransmit
        if (poll_res == 0) {

          // Update state variables
          finished_sending = false;
          first_unsent_byte = first_unacked_byte;

          // Update congestion control variables
          ssthresh = cwnd / 2;
          cwnd = DATABUF;
          break;
        }

        // Expect an ACK from the packet just sent out
        bzero(rec, MAXBUF);
        int recv_res = recvfrom(sockfd, (char*)rec, MAXBUF, 0,
          (struct sockaddr*) &serverAddr, &serverAddr_len);
        if (recv_res == -1) {
          continue;
        }
        UDPpacket* pkt_in = reinterpret_cast<UDPpacket*> (rec);
        print_log(0, pkt_in->getSeq(), pkt_in->getAck(), pkt_in->getconnID(),
          cwnd, ssthresh, pkt_in->isAck(), pkt_in->isSyn(), pkt_in->isFin());
        if (pkt_in->isAck()) {

          // If we receive the next expected ACK
          long next_expected_ack = (CLNT_DEFAULT_SEQ + 1 + first_unacked_byte +
            bytes_to_receive) % (MAXSEQACKNUM + 1);
          if (pkt_in->getAck() >= next_expected_ack || abs(next_expected_ack -
            pkt_in->getAck()) > 80000) {

            // Update congestion control variables
            if (cwnd < ssthresh) {
              cwnd += DATABUF;
            } else {
              cwnd += (DATABUF * DATABUF) / cwnd;
            }

            // Keep CWND within its allowed bounds
            cwnd = min(cwnd, (long) MAXCWND);
            cwnd = max(cwnd, (long) DATABUF);

            // Update state variables
            first_unacked_byte += bytes_to_receive;
            unreceived_bytes = file.size() - first_unacked_byte;
            bytes_to_receive = min(unreceived_bytes, (long) DATABUF);
            if (bytes_to_receive <= 0) {
              finished_receiving = true;
            }

            // If done sending and receiving, close connection with a FIN
            if (finished_sending && finished_receiving) {
              UDPpacket* pkt_fin = new UDPpacket(
                htonl(pkt_in->getAck() % (MAXSEQACKNUM + 1)), htonl(0),
                htons(connectionID), 0, 0, 1, NULL);
              UDPsend(pkt_fin, sockfd, serverAddr, pkt_fin->getheadersize());
              print_log(1, pkt_fin->getSeq(), pkt_fin->getAck(),
                pkt_fin->getconnID(), cwnd, ssthresh, pkt_fin->isAck(),
                pkt_fin->isSyn(), pkt_fin->isFin());
              break;
            }
          }
        }
      }
    }
  }

  // If 2 seconds have passed, exit normally without sending anymore ACKs
  chrono::steady_clock::time_point fin_start = chrono::steady_clock::now();
  while (1) {
    if(chrono::steady_clock::now() - fin_start > chrono::seconds(2)) {
      break;
    }

    // Poll the socket file descriptor for 2 seconds
    struct pollfd pfd;
    pfd.fd = sockfd;
    pfd.events = POLLIN;
    int poll_res = poll(&pfd, 1, 2000);
    if (poll_res == -1) {
      cerr << "ERROR: Could not poll socket" << endl;
      close(sockfd);
      exit(1);
    }

    // If timeout, gracefully close the connection normally
    if (poll_res == 0) {
      break;
    }

    // We successfully received an ACK within the two second window
    bzero(rec, MAXBUF);
    int recv_res = recvfrom(sockfd, (char*)rec, MAXBUF, 0,
      (struct sockaddr*) &serverAddr, &serverAddr_len);
    if (recv_res == -1) {
      continue;
    }

    // Only send an ACK if the received packet is a FIN
    UDPpacket* pkt_in = reinterpret_cast<UDPpacket*> (rec);
    if (pkt_in->isFin()) {

      // Receive the FIN packet
      print_log(0, pkt_in->getSeq(), pkt_in->getAck(), pkt_in->getconnID(),
        cwnd, ssthresh, pkt_in->isAck(), pkt_in->isSyn(), pkt_in->isFin());

      // Send the ACK packet
      UDPpacket* pkt_ack = new UDPpacket(
        htonl(pkt_in->getAck() % (MAXSEQACKNUM + 1)),
        htonl((pkt_in->getSeq() + 1) % (MAXSEQACKNUM + 1)),
        htons(pkt_in->getconnID()), 1, 0, 0, NULL);
      UDPsend(pkt_ack, sockfd, serverAddr, pkt_ack->getheadersize());
      print_log(1, pkt_ack->getSeq(), pkt_ack->getAck(), pkt_ack->getconnID(),
        cwnd, ssthresh, pkt_ack->isAck(), pkt_ack->isSyn(), pkt_ack->isFin());
    } else {

      // Drop any non-FIN packet
      print_log(2, pkt_in->getSeq(), pkt_in->getAck(), pkt_in->getconnID(),
        cwnd, ssthresh, pkt_in->isAck(), pkt_in->isSyn(), pkt_in->isFin());
    }
  }

  // Normal program exit
  close(sockfd);
  exit(0);
}
