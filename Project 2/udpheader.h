#define MAXSEQACKNUM 102400
#define MAXBUF 1024
#define MAXCWND 51200
#define INITSSTHRESH 10000
#define DATABUF 512
#define HEADER 20
#define SRVR_DEFAULT_SEQ 4321
#define CLNT_DEFAULT_SEQ 12345
using namespace std;

struct UDPheader
{
  unsigned int seqnum;
  unsigned int acknum;
  short int connId;
  uint16_t flags;
};

class UDPpacket
{
  public:
    UDPpacket(unsigned int seqnum, unsigned int acknum, short int connId,
                int ack, int syn, int fin, char* _payload, size_t _payload_size=0)
    {
      head.seqnum=seqnum;
      head.acknum=acknum;
      head.connId=connId;

      head.flags=0;
      uint16_t a=ack<<2;
      uint16_t s=syn<<1;
      uint16_t f=fin;

      head.flags= a|s|f;
      head.flags=htons(head.flags);
      memset(payload, 0, sizeof(payload));
      if (_payload)
      {
        memcpy(payload, _payload, _payload_size);
      }
    }
    
    unsigned int getSeq()
    {
      return ntohl(head.seqnum);
    }
    unsigned int getAck()
    {
      return ntohl(head.acknum);
    }
     short int getconnID()
    {
      return ntohs(head.connId);
    }
    bool isFin()
    {
      uint16_t i=1;
      return ntohs(head.flags)&i;
    }
    bool isSyn()
    {
      uint16_t i=1<<1;
      return ntohs(head.flags)&i;
    }
    bool isAck()
    {
      uint16_t i=1<<2;
      return ntohs(head.flags)&i;
    }
    char* getpayload()
    {
      return payload;
    }
    int getsize()
    {
      return sizeof(head) + sizeof(payload);
    }
    int getheadersize()
    {
      return sizeof(head);
    }
  private:
    UDPheader head;
    char payload[DATABUF];
};
