#pragma once

#include <unistd.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/select.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define LEN_BUF             2048
#define	NOSOCK	            -1
#define PACKET_MAX_SIZE     65535
#define UDP_HEADER_SIZE     8
#define IPV4_HEADER_SIZE    20

enum PROTO
{
    PROTO_UDP = 0,
    PROTO_TCP,
};

struct ConnInfo
{
    enum PROTO          m_eProtocal = PROTO_TCP;
    int                 m_fdSock    = NOSOCK;
    struct sockaddr_in  m_sin       = { 0 };

    ConnInfo() {}
    virtual ~ConnInfo() {}

    void Close()
    {
        if( m_fdSock != NOSOCK )
        {
            close( m_fdSock );
            m_fdSock = NOSOCK;
        }

        bzero( &m_sin, sizeof( m_sin ) );
    }
};

class CSocket
{
public:
    CSocket() {};
    virtual ~CSocket() {};

    virtual int     StartService() { return 0; }
    virtual bool    ParseRequest( int iBufSize, const char* lpszBuf, ConnInfo* lpData ) { return false; }
    virtual bool    SendResponse( ConnInfo* lpData ) { return false; }

    static int      InitService( enum PROTO eProtocal, int iPort, int iConnNum );
    static int      InitTCPService( int iPort, int iConnNum );
    static int      InitUDPService( int iPort, int iConnNum );
    static int      ParseLine( int iSrcBufSize, const char* lpszSrc, int iDstBufSize, char* lpszDst );
};
