#include "rtsp.h"
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <algorithm>
#include <sys/time.h>
#include "../common/util.h"
#include "../common/counter.h"
#include "../codec/h264.h"
#include "../codec/aac.h"

using namespace std;

/*************************************************
 * RTSPInfo
 *************************************************/
/// @brief Get file name from the URL of RTSP info
/// @param lpszFileName File name buffer
inline void RTSP::RTSPInfo::get_file_name( char* lpszFileName )
{
    sscanf( this->m_szUrl, "%*[^:]:%*2[/]%*[^/]/%[^/]", lpszFileName );
}

inline int RTSP::RTSPInfo::get_track_idx()
{
    int iTrackIdx = -1;
    sscanf( this->m_szUrl, "%*[^:]:%*2[/]%*[^/]/%*[^/]/track%d", &iTrackIdx );
    return iTrackIdx;
}

/*************************************************
 * ClientInfo
 *************************************************/
void RTSP::ClientInfo::ReleaseThread()
{
    if( this->m_pVidThread.get() )
    {
        this->m_pVidThread->SetExit( 500, true );
        this->m_pVidThread.reset();
    }

    if( this->m_pAudThread.get() )
    {
        this->m_pAudThread->SetExit( 500, true );
        this->m_pAudThread.reset();
    }
}

void RTSP::ClientInfo::Disconnect()
{
    this->ReleaseThread();
    this->Close();
}

/*************************************************
 * RTSP
 *************************************************/
/// @brief RTSP handler
/// @param lpszFilePath Media data file path
RTSP::RTSP( const char* lpszFilePath ) : CSocket()
    , m_strFilePath( lpszFilePath )
{
    srand( time( NULL) );
}

RTSP::~RTSP()
{
    this->set_running( false );

    if( this->Lock() )
    {
        //
        for( auto pClient : this->m_vClient )
        {
            pClient->Disconnect();
            delete pClient;
        }

        //
        this->m_vClient.clear();
        this->m_vSSRC.clear();

        this->Unlock();
    }
}

int RTSP::StartService()
{
    //
    int i = 0, nfds = 0, cs = NOSOCK, iAddrLen = 0;
    fd_set rfds;
    struct sockaddr_in sinClient = { 0 };

    iAddrLen = sizeof( sinClient );

    // Init Services
    //this->m_arrRTSPFD[ 0 ] = InitTCPService( PORT_RTSP1, 5 );
    this->m_arrRTSPFD[ 1 ] = InitTCPService( PORT_RTSP2, 5 );
    this->m_fdRTP          = InitUDPService( PORT_RTP,   5 );
    this->m_fdRTCP         = InitUDPService( PORT_RTCP,  5 );

    this->set_running( true );

    while( true )
    {
        // Reset
        FD_ZERO( &rfds );
        nfds = NOSOCK;

        // Add RTSP socket to read fd set
        //FD_SET( this->m_arrRTSPFD[ 0 ], &rfds );
        //if( this->m_arrRTSPFD[ 0 ] > nfds ) nfds = this->m_arrRTSPFD[ 0 ];
        FD_SET( this->m_arrRTSPFD[ 1 ], &rfds );
        if( this->m_arrRTSPFD[ 1 ] > nfds ) nfds = this->m_arrRTSPFD[ 1 ];

        // Add child sockets to read fd set
        if( this->Lock() )
        {
            for( auto pClient : m_vClient )
            {
                if( !pClient )
                    continue;

                if( pClient->m_fdSock > 0 )
                    FD_SET( pClient->m_fdSock, &rfds );

                if( pClient->m_fdSock > nfds )
                    nfds = pClient->m_fdSock;
            }

            this->Unlock();
        }

        // Select fd
        if( select( nfds + 1, &rfds, NULL, NULL, NULL ) < 0 )
        {
            fprintf( stderr, "RTSP::StartService - select error! ( Err: '%s' )\n", strerror(errno) );
            exit( 1 );
        }

        // Case: Accept RTSP client
        for( auto fd : m_arrRTSPFD )
        {
            if( FD_ISSET( fd, &rfds ) )
            {
                bzero( &sinClient, sizeof( sinClient ) );

                if( ( cs = accept( fd, reinterpret_cast< sockaddr* >( &sinClient ), reinterpret_cast< socklen_t*> ( &iAddrLen ) ) ) < 0 )
                {
                    fprintf( stderr, "RTSP::StartService - accept failed! ( Err: '%s' )\n", strerror(errno) );
                    continue;
                }

                printf("New connection, fd: %d, IP:%s, Port:%d\n", cs, inet_ntoa( sinClient.sin_addr ), ntohs( sinClient.sin_port ) );

                //
                ClientInfo* pClient = new ClientInfo();
                if( pClient )
                {
                    if( this->Lock() )
                    {
                        pClient->m_eProtocal = PROTO::PROTO_TCP;
                        pClient->m_fdSock    = cs;
                        pClient->m_sin       = sinClient;
                        m_vClient.push_back( pClient );
                        this->Unlock();
                    }
                }
            }
        }

        // Case: Handle packet
        if( this->Lock() )
        {
            for( int i = m_vClient.size() - 1; i >= 0; --i )
            {
                if( !m_vClient[ i ] )
                    continue;

                if( FD_ISSET( m_vClient[ i ]->m_fdSock, &rfds ) )
                {
                    if( !ProcessClient( *m_vClient[ i ] ) )
                    {   // Client disconnected
                        m_vClient[ i ]->Disconnect();

                        for( auto ssrc : m_vClient[ i ]->m_dtRTSP.m_arrSSRC )
                            this->RemoveSSRC( ssrc );

                        delete m_vClient[ i ];
                        m_vClient.erase( m_vClient.begin() + i );
                    }
                }
            }

            this->Unlock();
        }
    }

    this->set_running( false );

    return 0;
}

bool RTSP::ProcessClient( ClientInfo& dtClient )
{   // Note: Lock by caller
    // Recv Data
    int iRead = 0, iLeft = LEN_BUF, iPos = 0;
    char szBuf[ LEN_BUF ] = { 0 };

    while( iLeft > 0 )
    {
        iRead = recv( dtClient.m_fdSock, szBuf + iPos, iLeft, 0 );
        if( iRead <= 0 )
        {   // Disconnected
            if( iRead < 0 )
                fprintf( stderr, "RTSP::ProcessClient - recv failed! ( Err: '%s' )\n", strerror( errno ) );

            fprintf( stderr, "Client disconnect, IP:%s, Port:%d\n", inet_ntoa( dtClient.m_sin.sin_addr ), ntohs( dtClient.m_sin.sin_port ) );
            return false;
        }

        iLeft -= iRead;
        iPos  += iRead;

        //
        if( iPos >= 4 &&
            *( szBuf + iPos - 4 ) == '\r' && *( szBuf + iPos - 3 ) == '\n' &&
            *( szBuf + iPos - 2 ) == '\r' && *( szBuf + iPos - 1 ) == '\n' )
            break;
    }

    if( szBuf[ LEN_BUF - 1 ] != '\0' ) szBuf[ LEN_BUF - 1 ] = '\0';

    // Parse RTSP request
    if( !this->ParseRequest( sizeof( szBuf ), szBuf, &dtClient ) )
        return false;

    // Send RTSP response
    if( !this->SendResponse( &dtClient ) )
        return false;

    // Process method
    if( !strcmp( dtClient.m_dtRTSP.m_szMethod, "PLAY" ) )
    {
#if SUPPORT_VIDEO
        this->PlayVideo( dtClient );
#endif
#if SUPPORT_AUDIO
        this->PlayAudio( dtClient );
#endif
    }
    else if( !strcmp( dtClient.m_dtRTSP.m_szMethod, "TEARDOWN" ) )
    {
        return false;
    }

    return true;
}

bool RTSP::CreateSSRC( int& iSSRC )
{
    for( int i = 0; i < 5; ++i )
    {
        if( !this->is_running() )
            return false;

        if( this->Lock( 5000 ) )
        {
            iSSRC = rand();

            for( int i = 0, iSize = this->m_vSSRC.size(); i < iSize; ++i )
            {
                if( this->m_vSSRC[ i ] == iSSRC )
                {   // SSRC duplicate
                    iSSRC = rand();
                    i = -1;
                }
            }

            this->m_vSSRC.push_back( iSSRC );

            this->Unlock();
            return true;
        }
    }

    return false;
}

void RTSP::RemoveSSRC( int iSSRC )
{
    if( this->Lock() )
    {
        for( int i = 0, iSize = this->m_vSSRC.size(); i < iSize; ++i )
        {
            if( this->m_vSSRC[ i ] == iSSRC )
            {
                this->m_vSSRC.erase( this->m_vSSRC.begin() + i );
                break;
            }
        }
        
        this->Unlock();
    }
}

bool RTSP::MakeResponse( int iBufSize, char* lpszBuf, RTSPInfo& dtRTSP )
{
    if( !strcmp( dtRTSP.m_szMethod, "OPTIONS" ) )
    {
        snprintf( lpszBuf, iBufSize, 
            "RTSP/1.0 200 OK\r\n"
            "Cseq: %d\r\n"
            "Public: OPTIONS, DESCRIBE, SETUP, PLAY\r\n\r\n", 
            dtRTSP.m_iCSeq );
    }
    else if( !strcmp( dtRTSP.m_szMethod, "DESCRIBE" ) )
    {
        char szFileName[ LEN_FILENAME ] = { 0 };
        char szIP[ 132 ] = { 0 };
        char szSDP[ 1024 ] = { 0 };
        char szSDP_Aud[ 512 ] = { 0 };
        string strFullFilePath = "";

        // Get IP Address
        sscanf( dtRTSP.m_szUrl, "rtsp://%[^:]:", szIP );

#if SUPPORT_AUDIO
        // Generate SDP ( Audio Part )
        dtRTSP.get_file_name( szFileName );
        strFullFilePath  = this->GetFullFilePath( szFileName );
        strFullFilePath += AAC_FILE_EXT;

        AAC aac;
        if( aac.OpenFile( strFullFilePath.c_str(), CFileMap::modeRead ) )
        {
            AAC::FrameInfoEx dtFrm;
            dtFrm = aac.GetFrame();

            char szConfig[ 12 ] = { 0 };
            snprintf( szConfig, sizeof( szConfig ), "%02x%02x", 
		            (uint8_t)( ( dtFrm.m_nProfile + 1 ) << 3 ) | ( dtFrm.m_nSampFrqIdx >> 1 ), 
		            (uint8_t)( ( dtFrm.m_nSampFrqIdx << 7 ) | ( dtFrm.m_nChannels << 3 ) ) );

            snprintf( szSDP_Aud, sizeof( szSDP_Aud ), 
                "m=audio 0 RTP/AVP %d\r\n"                                      // MediaType:audio, Port:0(response in SETUP), Protocal:RTP/AVP(RTP over UDP/Audio-Video Profile), RTPPayloadType:97(AAC)
                "a=rtpmap:%d mpeg4-generic/%d/%d\r\n"                           // RTPPayloadType:97(AAC), Codec:MPEG-4, ClockFrequency:44100Hz, Channel:2
                "a=fmtp:%d streamtype=5; profile-level-id=1; mode=AAC-hbr;"     // Mode:High Bitrate AAC
                "sizelength=13;indexlength=3;indexdeltalength=3;"               // AAC Frame Length(AU-size):13 bits, AU-Index:3 bits, AU-Index-delta:3 bits
                "config=%s;\r\n"                                                // AAC config
                "a=control:track%d\r\n",                                        // The control path of the stream is 'track1'
                RTP_PAYLOAD_TYPE_AAC, 
                RTP_PAYLOAD_TYPE_AAC, AAC::gm_arrSampFrqIdxValue[ dtFrm.m_nSampFrqIdx ], dtFrm.m_nChannels, 
                RTP_PAYLOAD_TYPE_AAC, 
                szConfig, 
                RTSPInfo::TRACK_AUDIO );
        }
#endif

        // Generate SDP
        snprintf( szSDP, sizeof( szSDP ), 
            "v=0\r\n"                       // Session Description Protocol Version
            "o=- 9%ld 1 IN IP4 %s\r\n"      // Username:-, SessionID:9+time(NULL), SessionVersion:1, NetworkType:IN(means Internet), AddressType:IP4(means IPv4), Address:szIP
            "t=0 0\r\n"                     // Session active starttime endtime (from 1900/1/1 in seconds)
            "a=control:*\r\n"               // The control path of the stream can be any value
#if SUPPORT_VIDEO
            "m=video 0 RTP/AVP %d\r\n"      // MediaType:video, Port:0(response in SETUP), Protocal:RTP/AVP(RTP over UDP/Audio-Video Profile), RTPPayloadType:96(H.264)
            "a=rtpmap:%d H264/90000\r\n"    // RTPPayloadType:96(H.264), Codec:H264, ClockFrequency:90000Hz
            "a=control:track%d\r\n"         // The control path of the stream is 'track0'
#endif
            , time( NULL ), szIP
#if SUPPORT_VIDEO
            , RTP_PAYLOAD_TYPE_H264
            , RTP_PAYLOAD_TYPE_H264
            , RTSPInfo::TRACK_VIDEO
#endif
            );

        strcat( szSDP, szSDP_Aud );

        //
        snprintf( lpszBuf, iBufSize, 
            "RTSP/1.0 200 OK\r\n"
            "Cseq: %d\r\n"
            "Content-Base: %s\r\n"
            "Content-type: application/sdp\r\n"
            "Content-length: %ld\r\n"
            "\r\n"
            "%s", 
            dtRTSP.m_iCSeq, 
            dtRTSP.m_szUrl, 
            strlen( szSDP ), 
            szSDP );
    }
    else if( !strcmp( dtRTSP.m_szMethod, "SETUP" ) )
    {
        // Get track index from URL
        int iTrackIdx = dtRTSP.get_track_idx();
        if( -1 == iTrackIdx )
            return false;

        // Create UUID
        if( dtRTSP.m_strSessionID.empty() )
        {
            char szUUID[ 37 ] = { 0 };
            CreateUUID( szUUID );
            dtRTSP.m_strSessionID = szUUID;
        }

        // Create SSRC
        if( !this->CreateSSRC( dtRTSP.m_arrSSRC[ iTrackIdx ] ) )
            return false;

        // Make SETUP response
        snprintf( lpszBuf, iBufSize, 
            "RTSP/1.0 200 OK\r\n"
            "Cseq: %d\r\n"
            "Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d;ssrc=%d;mode=play\r\n"
            "Session: %s; timeout=%d\r\n\r\n",
            dtRTSP.m_iCSeq, 
            dtRTSP.m_arrClientPort_RTP[ iTrackIdx ], dtRTSP.m_arrClientPort_RTCP[ iTrackIdx ], PORT_RTP, PORT_RTCP, dtRTSP.m_arrSSRC[ iTrackIdx ], 
            dtRTSP.m_strSessionID.c_str(), RTSP_TIMEOUT );
    }
    else if( !strcmp( dtRTSP.m_szMethod, "PLAY" ) )
    {
        snprintf( lpszBuf, iBufSize, 
            "RTSP/1.0 200 OK\r\n"
            "Cseq: %d\r\n"
            "Range: npt=0.000-\r\n"
            "Session: %s; timeout=%d\r\n\r\n", 
            dtRTSP.m_iCSeq, 
            dtRTSP.m_strSessionID.c_str(), RTSP_TIMEOUT );
    }
    else if( !strcmp( dtRTSP.m_szMethod, "TEARDOWN" ) )
    {
        snprintf( lpszBuf, iBufSize, 
            "RTSP/1.0 200 OK\r\n"
            "Cseq: %d\r\n"
            "\r\n", 
            dtRTSP.m_iCSeq );
    }
    else
    {
        fprintf( stderr, "RTSP::MakeResponse - Unsupport RTSP method! ( Method: '%s' )\n", dtRTSP.m_szMethod );
        return false;
    }

    return true;
}

void RTSP::PlayVideo( ClientInfo& dtClient )
{   // Note: Lock by caller
    char szIP[ 16 ] = { 0 };
    inet_ntop( AF_INET, &dtClient.m_sin.sin_addr, szIP, sizeof( szIP ) );

    dtClient.m_pVidThread = make_shared< H264RTPSession >();
    if( dtClient.m_pVidThread.get() )
    {
        char szFileName[ LEN_FILENAME ] = { 0 };
        string strFullFilePath = "";

        dtClient.m_dtRTSP.get_file_name( szFileName );

        strFullFilePath  = this->GetFullFilePath( szFileName );
        strFullFilePath += H264_FILE_EXT;

        dtClient.m_pVidThread->Start( 
            this->m_fdRTP, 
            dtClient.m_dtRTSP.m_arrSSRC[ RTSPInfo::TRACK_VIDEO ],
            strFullFilePath, 
            szIP, 
            dtClient.m_dtRTSP.m_arrClientPort_RTP[ RTSPInfo::TRACK_VIDEO ] );
    }
}

void RTSP::PlayAudio( ClientInfo& dtClient )
{   // Note: Lock by caller
    char szIP[ 16 ] = { 0 };
    inet_ntop( AF_INET, &dtClient.m_sin.sin_addr, szIP, sizeof( szIP ) );

    dtClient.m_pAudThread = make_shared< AACRTPSession >();
    if( dtClient.m_pAudThread.get() )
    {
        char szFileName[ LEN_FILENAME ] = { 0 };
        string strFullFilePath = "";

        dtClient.m_dtRTSP.get_file_name( szFileName );

        strFullFilePath  = this->GetFullFilePath( szFileName );
        strFullFilePath += AAC_FILE_EXT;

        dtClient.m_pAudThread->Start( 
            this->m_fdRTP, 
            dtClient.m_dtRTSP.m_arrSSRC[ RTSPInfo::TRACK_AUDIO ],
            strFullFilePath, 
            szIP, 
            dtClient.m_dtRTSP.m_arrClientPort_RTP[ RTSPInfo::TRACK_AUDIO ] );
    }
}

bool RTSP::ParseRequest( int iBufSize, const char* lpszBuf, ConnInfo* lpData )
{   // Note: Lock by caller
    if( !lpData )
    {
        fprintf( stderr, "RTSP::ParseRequest - Invalid data! ( lpData: 0x%p )\n", lpData );
        return false;
    }

    ClientInfo* lpClientInfo = dynamic_cast< ClientInfo* >( lpData );
    if( !lpClientInfo )
    {
        cerr << "RTSP::ParseRequest - dynamic_cast ClientInfo* failed!" << endl;
        return false;
    }

    // Parse head
    char szLine[ LEN_BUF ] = { 0 };

    int iPos = CSocket::ParseLine( iBufSize, lpszBuf, sizeof( szLine ), szLine );
    if( sscanf( szLine, "%s %s %s\r\n", lpClientInfo->m_dtRTSP.m_szMethod, lpClientInfo->m_dtRTSP.m_szUrl, lpClientInfo->m_dtRTSP.m_szVer ) != 3 )
    {
        cerr << "RTSP::ParseRequest - Parse RTSP request head error!" << endl;
        return false;
    }

    cout << "recv " << lpClientInfo->m_dtRTSP.m_szMethod << endl;

    // Parse CSeq
    iPos += CSocket::ParseLine( ( iBufSize - iPos ), ( lpszBuf + iPos ), sizeof( szLine ), szLine );
    if( sscanf( szLine, "CSeq: %d\r\n", &lpClientInfo->m_dtRTSP.m_iCSeq ) != 1 )
    {
        cerr << "RTSP::ParseRequest - Parse RTSP CSeq error!" << endl;
        return false;
    }

    // Parse method:SETUP
    if( !strcmp( lpClientInfo->m_dtRTSP.m_szMethod, "SETUP" ) )
    {
        // Get track index from URL
        int iTrackIdx = lpClientInfo->m_dtRTSP.get_track_idx();
        if( -1 == iTrackIdx )
            return false;
            
        // Parse method:SETUP content
        int iBufLen = strlen( lpszBuf );
        while( iPos < iBufLen )
        {
            iPos += CSocket::ParseLine( ( iBufSize - iPos ), ( lpszBuf + iPos ), sizeof( szLine ), szLine );

            string strLine = szLine;
            int idx = strLine.find( "Transport:" );
            if( idx >= 0 )
            {
                if( sscanf( szLine, "Transport: RTP/AVP;unicast;client_port=%d-%d\r\n", 
                    &lpClientInfo->m_dtRTSP.m_arrClientPort_RTP[ iTrackIdx ], &lpClientInfo->m_dtRTSP.m_arrClientPort_RTCP[ iTrackIdx ] ) != 2 )
                {
                    cerr << "RTSP::ParseRequest - Parse RTSP SETUP content error!" << endl;
                    cerr << szLine << endl;
                    return false;
                }

                break;
            }
        }
    }

    return true;
}

bool RTSP::SendResponse( ConnInfo* lpData )
{   // Note: Lock by caller
    if( !lpData )
    {
        fprintf( stderr, "RTSP::SendResponse - Invalid data! ( lpData: 0x%p )\n", lpData );
        return false;
    }

    ClientInfo* lpClientInfo = dynamic_cast< ClientInfo* >( lpData );
    if( !lpClientInfo )
    {
        cerr << "RTSP::SendResponse - dynamic_cast ClientInfo* failed!" << endl;
        return false;
    }

    int iLen = 0;
    char szBuf[ LEN_BUF ] = { 0 };

    // Make RTSP response
    if( !this->MakeResponse( sizeof( szBuf ), szBuf, lpClientInfo->m_dtRTSP ) )
        return false;

    // Send RTSP response
    if( send( lpClientInfo->m_fdSock, szBuf, strlen( szBuf ), 0 ) < 0 )
    {
        cerr << "RTSP::SendResponse - Send RTSP response failed!" << endl;
        return false;
    }

    return true;
}

bool RTSP::Lock( int iTimeout )
{
    return this->m_mtx.Lock( iTimeout );
}

bool RTSP::Unlock()
{
    return this->m_mtx.Unlock();
}

string RTSP::GetFullFilePath( const char* lpszFileName )
{
    string strFullFilePath = "";

    strFullFilePath = this->m_strFilePath;
    if( strFullFilePath.at( strFullFilePath.length() - 1 ) != '/' )
        strFullFilePath += '/';
    
    strFullFilePath += lpszFileName;

    return strFullFilePath;
}
