#include <iostream>
#include <stdlib.h>
#include <string.h>
#include "network.h"

using namespace std;

/// @brief Initialize a socket service
/// @param eProtocal TCP/UDP protocal defined in PROTO
/// @param iPort Port number
/// @param iConnNum Maximum connection numbers
/// @return Socket FD
int CSocket::InitService( PROTO eProtocal, int iPort, int iConnNum )
{
    int s = 0, iType = 0;
    struct sockaddr_in sin = { 0 };
    struct protoent* pPE = NULL;

    //
    sin.sin_family      = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port        = htons( iPort );

    //
    if( PROTO_UDP == eProtocal )
    {
        iType = SOCK_DGRAM;
        pPE = getprotobyname( "udp" );
    }
    else if( PROTO_TCP == eProtocal )
    {
        iType = SOCK_STREAM;
        pPE = getprotobyname( "tcp" );
    }

    if( !pPE )
    {
        cerr << "Get protocal entry failed!" << endl;
        exit( 1 );
    }

    //
    s = socket( PF_INET, iType, pPE->p_proto );
    if( s < 0 )
    {
        fprintf( stderr, "Create socket failed! ( Err: '%s' )\n", strerror(errno) );
        exit( 1 );
    }

    int opt = 1;
    if( setsockopt( s, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof( opt ) ) < 0 )
    {
        fprintf( stderr, "setsockopt SO_REUSEADDR | SO_REUSEPORT failed! ( Err: '%s' )\n", strerror(errno) );
        exit( 1 );
    }

    opt = PACKET_MAX_SIZE;
    if( setsockopt( s, SOL_SOCKET, SO_SNDBUF, &opt, sizeof( opt ) ) < 0 )
    {
        fprintf( stderr, "setsockopt SO_SNDBUF failed! ( Err: '%s' )\n", strerror(errno) );
        exit( 1 );
    }

    if( bind( s, reinterpret_cast< sockaddr* >( &sin ), sizeof( sin ) ) < 0 )
    {
        fprintf( stderr, "Bind on port:%d failed! ( Err: '%s' )\n", iPort, strerror(errno) );
        exit( 1 );
    }

    if( ( iType == SOCK_STREAM ) && ( listen( s, iConnNum ) < 0 ) )
    {
        fprintf( stderr, "Listen on port:%d failed! ( Err: '%s' )\n", iPort, strerror(errno) );
        exit( 1 );
    }

    if( PROTO_UDP == eProtocal )
        printf( "Init UDP channel on port: %d\n", iPort );
    else if( PROTO_TCP == eProtocal )
        printf( "Init TCP server on port: %d\n", iPort );

    return s;
}

int CSocket::InitTCPService( int iPort, int iConnNum )
{
    return InitService( PROTO_TCP, iPort, iConnNum );
}

int CSocket::InitUDPService( int iPort, int iConnNum )
{
    return InitService( PROTO_UDP, iPort, iConnNum );
}

/// @brief Parse message from 1 line until '\n'
/// @param iSrcBufSize Source buffer size
/// @param lpszSrc Source buffer
/// @param iDstBufSize Destination buffer size
/// @param lpszDst Destination buffer
/// @return Next line position in source buffer
int CSocket::ParseLine( int iSrcBufSize, const char* lpszSrc, int iDstBufSize, char* lpszDst )
{
    if( !lpszSrc || !lpszDst )
    {
        fprintf( stderr, "CSocket::ParseLine - Invalid data! ( lpszSrc: 0x%p, lpszDst: 0x%p )\n", lpszSrc, lpszDst );
    }

    int iSrcPos = 0, iDstPos = 0;

    while( ( iSrcPos < iSrcBufSize ) && ( iDstPos < iDstBufSize ) 
        && ( *( lpszSrc + iSrcPos ) != '\n' && *( lpszSrc + iSrcPos ) != '\0' ) )
    {
        *( lpszDst + iDstPos ) = *( lpszSrc + iSrcPos );
        ++iSrcPos;
        ++iDstPos;
    }

    if( iDstPos + 1 < iDstBufSize )
    {
        *( lpszDst + iDstPos ) = '\n';
        ++iDstPos;
    }

    if( iDstPos < iDstBufSize )
        *( lpszDst + iDstPos ) = 0;

    return ( iSrcPos + 1 );
}
