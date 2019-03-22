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
#include "udpfunctions.h"

void signalHandler( int signum )
{
   cerr << "INTERRUPT: Interrupt signal (" << signum << ") received.\n";
   exit(signum);
}

void print_log(bool isrecv, unsigned int seqnum, unsigned int acknum,
  short int connId, bool isAck, bool isSyn, bool isFin, bool isdrop=false)
{
  if(isdrop)
    cout << "DROP ";
  else {
    if (isrecv) {
      cout << "RECV ";
    } else {
      cout << "SEND ";
    }
  }
  cout << seqnum << " " << acknum << " " << connId;
  if (isAck)
    cout << " ACK";
  if (isSyn)
    cout << " SYN";
  if (isFin)
    cout << " FIN";
  cout << endl;
}

int main(int argc, char const *argv[])
{
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);
  signal(SIGQUIT, signalHandler);

  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  //fcntl(sockfd, F_SETFL, O_NONBLOCK);

  if (argc!=3)
  {
    cerr<<"ERROR: Invalid number of arguments"<<endl;
    exit(1);
  }

  if(sockfd < 0)
  {
    cerr<<"ERROR: Socket creation failed"<<endl;
    exit(1);
  }

  // allow others to reuse the address
  int yes=1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
    cerr<<"ERROR: setsockopt failed"<<endl;
    close(sockfd);
    exit(1);
  }

  stringstream geek(argv[1]);
  short port = 0;
  geek >> port;

  if(port<1023 || port>65535)
  {
    cerr<<"ERROR: Incorrect port"<<endl;
    close(sockfd);
    exit(1);
  }

  const char* directory_arg = argv[2];

  // Construct the directory string from the user input
  string directory(directory_arg);
  if (directory.size() > 0) {
    if (directory[0] != '.') {
      directory = "." + directory;
    }
    if (directory[1] != '/') {
      directory = directory[0] + "/" + directory.substr(1);
    }
    if (directory[directory.size() - 1] != '/') {
      directory += "/";
    }
  }

  // If the directory does not exist, create it
  struct stat buffer;
  if (stat (directory.c_str(), &buffer) == -1) {
    if (mkdir(directory.c_str(), 0777) == -1) {
      cerr<<"ERROR: Could not create directory"<<endl;
      close(sockfd);
      exit(1);
    }
  }

  struct sockaddr_in addr,cliaddr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));
  memset(&cliaddr, 0, sizeof(cliaddr));

  socklen_t cliaddr_len=sizeof(cliaddr);
  // bind address to socket
  if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    cerr<<"ERROR: Binding error"<<endl;
    exit(1);
  }

  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(sockfd, &readfds);


  bool end=false;
  int clientcount=0;
  vector< vector<UDPpacket> > v (50);
  vector<vector <int> > sizes (50);
  vector<int> expected (50);
  //FILE *f=fopen("1.file","w+b");
  while(!end)
  {
    UDPpacket* pkt_in= NULL;
    char rec[MAXBUF];
    bzero(rec,MAXBUF);

    int block_size = recvfrom(sockfd, (char*)rec, MAXBUF, 0, ( struct sockaddr *) &cliaddr, &cliaddr_len);
    if(block_size < 0)
    {
      cerr<<"ERROR in receive "<<strerror(errno);
    }
    pkt_in=reinterpret_cast<UDPpacket*> (rec);



    if(pkt_in->isSyn()) // SYN packet, send SYN-ACK
    {
      print_log(true, pkt_in->getSeq(), pkt_in->getAck(), pkt_in->getconnID(),
        pkt_in->isAck(), pkt_in->isSyn(), pkt_in->isFin());

      clientcount+=1;
      UDPpacket* pkt_out= new UDPpacket(htonl(SRVR_DEFAULT_SEQ), htonl(pkt_in->getSeq()+1), htons(clientcount), 1, 1, 0, NULL);
      UDPsend(pkt_out, sockfd, cliaddr);
      print_log(false, pkt_out->getSeq(), pkt_out->getAck(), pkt_out->getconnID(),
        pkt_out->isAck(), pkt_out->isSyn(), pkt_out->isFin());
      expected[clientcount]=pkt_in->getSeq()+1;
    }
    else if(pkt_in->isAck())
    {
      print_log(true, pkt_in->getSeq(), pkt_in->getAck(), pkt_in->getconnID(),
        pkt_in->isAck(), pkt_in->isSyn(), pkt_in->isFin());
     //client only sends ACK twice: SYN-ACK & FIN-ACK
     //maybe FIN pkt got lost, check if this is teardown-> reconstruct file
     if (v[pkt_in->getconnID()].size()!=0)
     {
       string s= to_string(pkt_in->getconnID())+".file";
       char fname[s.size()+1];
       strcpy(fname, s.c_str());

       // Create an empty file in the directory and save its file descriptor
       string file_path = directory + to_string(pkt_in->getconnID()) + ".file";
       FILE* f = fopen(file_path.c_str(), "w+b");
       if (!f) {
         cerr<<"ERROR: Could not open file"<<endl;
         close(sockfd);
         exit(1);
       }
       for (size_t i=0; i< v[pkt_in->getconnID()].size() ; i++)
       {
          //cout<<v[pkt_in->getconnID()][i]->getSeq())<<endl;
         fwrite(v[pkt_in->getconnID()][i].getpayload(), sizeof(char), sizes[pkt_in->getconnID()][i], f);
       }
       fclose(f);
     }
    }
    else if(pkt_in->isFin())
    {
      print_log(true, pkt_in->getSeq(), pkt_in->getAck(), pkt_in->getconnID(),
        pkt_in->isAck(), pkt_in->isSyn(), pkt_in->isFin());
      //send ACK for the FIN
      UDPpacket* pkt_out= new UDPpacket(htonl(SRVR_DEFAULT_SEQ+1), htonl((pkt_in->getSeq() + block_size - pkt_in->getheadersize()+1)%(MAXSEQACKNUM + 1)),
        htons(pkt_in->getconnID()), 1, 0, 1, NULL);
      UDPsend(pkt_out, sockfd, cliaddr);
      print_log(false, pkt_out->getSeq(), pkt_out->getAck(), pkt_out->getconnID(),
        pkt_out->isAck(), pkt_out->isSyn(), pkt_out->isFin());
     
      //reconstruct the file sent by client
      //cout<<v[pkt_in->getconnID()].size()<<endl;

      string s= to_string(pkt_in->getconnID())+".file";
      char fname[s.size()+1];
      strcpy(fname, s.c_str());

      // Create an empty file in the directory and save its file descriptor
      string file_path = directory + to_string(pkt_in->getconnID()) + ".file";
      FILE* f = fopen(file_path.c_str(), "w+b");
      if (!f) {
        cerr<<"ERROR: Could not open file"<<endl;
        close(sockfd);
        exit(1);
      }
      for (size_t i=0; i< v[pkt_in->getconnID()].size() ; i++)
      {
        fwrite(v[pkt_in->getconnID()][i].getpayload(), sizeof(char), sizes[pkt_in->getconnID()][i], f);
      }
      fclose(f);
    }
    else  // received data packet, store it accordingly
    {
      if(pkt_in->getSeq()==(size_t)expected[pkt_in->getconnID()])
      {

        print_log(true, pkt_in->getSeq(), pkt_in->getAck(), pkt_in->getconnID(),
            pkt_in->isAck(), pkt_in->isSyn(), pkt_in->isFin());

        v[pkt_in->getconnID()].push_back(*pkt_in);
        sizes[pkt_in->getconnID()].push_back(block_size - pkt_in->getheadersize());

        // send ack for received packet
        UDPpacket* pkt_out= new UDPpacket(htonl(SRVR_DEFAULT_SEQ+1), htonl((pkt_in->getSeq() + block_size - pkt_in->getheadersize())%(MAXSEQACKNUM + 1)),
          htons(pkt_in->getconnID()), 1, 0, 0, NULL);
        UDPsend(pkt_out, sockfd, cliaddr);
        print_log(false, pkt_out->getSeq(), pkt_out->getAck(), pkt_out->getconnID(),
          pkt_out->isAck(), pkt_out->isSyn(), pkt_out->isFin());

        //update next expected seqnum from this client
        expected[pkt_in->getconnID()] = (pkt_in->getSeq() + block_size - pkt_in->getheadersize())%(MAXSEQACKNUM + 1);
      }

      else //server's Ack got dropped, send dup Ack
      {
        UDPpacket* pkt_out= new UDPpacket((htonl(SRVR_DEFAULT_SEQ+1)), htonl(expected[pkt_in->getconnID()]),
          htons(pkt_in->getconnID()), 1, 0, 0, NULL);
        UDPsend(pkt_out, sockfd, cliaddr);
        //log of dropped received packet
        print_log(false, pkt_in->getSeq(), pkt_in->getAck(), pkt_in->getconnID(),
          pkt_in->isAck(), pkt_in->isSyn(), pkt_in->isFin(),true);
        //log of sent ack packet
        print_log(false, pkt_out->getSeq(), pkt_out->getAck(), pkt_out->getconnID(),
            pkt_out->isAck(), pkt_out->isSyn(), pkt_out->isFin());
      }
    }
  }
}
