#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <thread>
#include <bits/stdc++.h>
#include "udpheader.h"


//helper function for sending UDP packet
void UDPsend(UDPpacket* out_packet, int sockfd, struct sockaddr_in addr, int bytes_to_send=524)
{
  char* sen=reinterpret_cast<char*> (out_packet);
  int  bytes_sent=0;

  while(bytes_to_send>0)
  {
    if((bytes_sent=(sendto(sockfd, sen, bytes_to_send, 0, (struct sockaddr*) &addr, (socklen_t) sizeof (addr)) < 0)))
    {
      cerr<<"ERROR in sending";
    }
    if(bytes_sent==0)
    {
      break;
    }
    bytes_to_send-=bytes_sent;
    sen+=bytes_sent;
  }
}
