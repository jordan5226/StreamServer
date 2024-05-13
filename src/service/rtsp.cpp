#include "rtsp.h"
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <algorithm>
#include "../common/util.h"

using namespace std;

void RTSP::ClientInfo::ReleaseThread()
{
    if( this->m_pThread.get() )
    {
        this->m_pThread->SetExit( 500, true );
        this->m_pThread.reset();
    }
}

void RTSP::ClientInfo::Disconnect()
{
    this->ReleaseThread();
    this->Close();
}

RTSP::RTSP( const char* lpszFile ) : CSocket()
    , m_strFile( lpszFile )
{
    srand( time( NULL) );
}

RTSP::~RTSP()
{
    this->set_running( false );

    if( this->m_mtx.Lock() )
    {
        //
        for( auto client : this->m_vClient )
        {
            client.Disconnect();
        }

        //
        this->m_vClient.clear();
        this->m_vSSRC.clear();

        this->m_mtx.Unlock();
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
        if( m_mtx.Lock() )
        {
            for( auto client : m_vClient )
            {
                if( client.m_fdSock > 0 )
                    FD_SET( client.m_fdSock, &rfds );

                if( client.m_fdSock > nfds )
                    nfds = client.m_fdSock;
            }

            m_mtx.Unlock();
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
                ClientInfo dtClient;
                if( m_mtx.Lock() )
                {
                    dtClient.m_eProtocal = PROTO::PROTO_TCP;
                    dtClient.m_fdSock    = cs;
                    dtClient.m_sin       = sinClient;
                    m_vClient.push_back( dtClient );
                    m_mtx.Unlock();
                }
            }
        }

        // Case: Handle packet
        if( m_mtx.Lock() )
        {
            for( int i = m_vClient.size() - 1; i >= 0; --i )
            {
                if( FD_ISSET( m_vClient[ i ].m_fdSock, &rfds ) )
                {
                    if( !ProcessClient( m_vClient[ i ] ) )
                    {   // Client disconnected
                        m_vClient[ i ].Disconnect();

                        this->RemoveSSRC( m_vClient[ i ].m_dtRTSP.m_iSSRC );

                        m_vClient.erase( m_vClient.begin() + i );
                    }
                }
            }

            m_mtx.Unlock();
        }
    }

    this->set_running( false );

    return 0;
}

void RTSP::PlayThread( ClientInfo& dtClient, RTSP* lpThis )
{
    if( !lpThis )
        return;

    weak_ptr< CThread > wpThread;

    //
    for( int i = 0; ; ++i )
    {
        if( i == 5 )
        {
            cerr << "RTSP::PlayThread - Thread is not ready!" << endl;
            return;
        }

        if( lpThis->Lock() )
        {
            if( dtClient.m_pThread.get() )
            {
                wpThread = dtClient.m_pThread;
                lpThis->Unlock();
                break;
            }

            lpThis->Unlock();
        }

        this_thread::sleep_for( chrono::milliseconds( 20 ) );
    }

    //
    lpThis->Play( dtClient );

    //
    if( shared_ptr< CThread > pThread = wpThread.lock() )
        pThread->set_running( false );
}

bool RTSP::ProcessClient( ClientInfo& dtClient )
{
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
        dtClient.m_pThread = make_shared< CThread >( thread( RTSP::PlayThread, std::ref( dtClient ), this ) );
    else if( !strcmp( dtClient.m_dtRTSP.m_szMethod, "TEARDOWN" ) )
        return false;

    return true;
}

bool RTSP::CreateSSRC( int& iSSRC )
{
    for( int i = 0; i < 5; ++i )
    {
        if( !this->is_running() )
            return false;

        if( this->m_mtx.Lock( 5000 ) )
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

            this->m_mtx.Unlock();
            return true;
        }
    }

    return false;
}

void RTSP::RemoveSSRC( int iSSRC )
{
    if( this->m_mtx.Lock() )
    {
        for( int i = 0, iSize = this->m_vSSRC.size(); i < iSize; ++i )
        {
            if( this->m_vSSRC[ i ] == iSSRC )
            {
                this->m_vSSRC.erase( this->m_vSSRC.begin() + i );
                break;
            }
        }
        
        this->m_mtx.Unlock();
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
        char szIP[ 132 ]  = { 0 };
        char szSDP[ 1024 ] = { 0 };

        sscanf( dtRTSP.m_szUrl, "rtsp://%[^:]:", szIP );

        snprintf( szSDP, sizeof( szSDP ), 
            "v=0\r\n"                       // Session Description Protocol Version
            "o=- 9%ld 1 IN IP4 %s\r\n"      // Username:-, SessionID:9+time(NULL), SessionVersion:1, NetworkType:IN(means Internet), AddressType:IP4(means IPv4), Address:szIP
            "t=0 0\r\n"                     // Session active starttime endtime (from 1900/1/1 in seconds)
            "a=control:*\r\n"               // The control path of the stream can be any value
            "m=video 0 RTP/AVP 96\r\n"      // MediaType:video, Port:0(response in SETUP), Protocal:RTP/AVP(RTP over UDP/Audio-Video Profile), RTPPayloadType:96(H.264)
            "a=rtpmap:96 H264/90000\r\n"    // RTPPayloadType:96(H.264), Codec:H264, ClockFrequency:90000Hz
            "a=control:track0\r\n",         // The control path of the stream is 'track0'
            time( NULL ), szIP );

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
        // Create UUID
        char szUUID[ 37 ] = { 0 };
        CreateUUID( szUUID );
        dtRTSP.m_strSessionID = szUUID;

        // Create SSRC
        if( !this->CreateSSRC( dtRTSP.m_iSSRC ) )
            return false;

        // Make SETUP response
        snprintf( lpszBuf, iBufSize, 
            "RTSP/1.0 200 OK\r\n"
            "Cseq: %d\r\n"
            "Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d;ssrc=%d;mode=play\r\n"
            "Session: %s; timeout=%d\r\n\r\n",
            dtRTSP.m_iCSeq, 
            dtRTSP.m_iClientPort_RTP, dtRTSP.m_iClientPort_RTCP, PORT_RTP, PORT_RTCP, dtRTSP.m_iSSRC, 
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

void RTSP::Play( ClientInfo& dtClient )
{
    // Set Dst addr info
    char szIP[ 16 ] = { 0 };
    inet_ntop( AF_INET, &dtClient.m_sin.sin_addr, szIP, sizeof( szIP ) );

    struct sockaddr_in sinDst = { 0 };
    sinDst.sin_family      = AF_INET;
    sinDst.sin_port        = htons( dtClient.m_dtRTSP.m_iClientPort_RTP );
    sinDst.sin_addr.s_addr = inet_addr( szIP );

    //
    const auto tTimeStampStep = uint32_t( 90000 / FPS );    // Timestamp gap between two frames
    const auto tSleepPeriod   = uint32_t( 1000 / FPS );     // Time gap between two frames

    H264::FrameInfoEx dtFrm;
    H264 oH264;
    oH264.OpenFile( this->m_strFile.c_str(), CFileMap::modeRead );

    RTPHeader dtRTPHeader( 0, 0, dtClient.m_dtRTSP.m_iSSRC );
    RTPMan oRTPMan( dtRTPHeader );

    while( dtClient.m_pThread.get() && !dtClient.m_pThread->is_exit() )
    {
        // Read Data
        dtFrm = oH264.GetFrame();

        if( 0 == dtFrm.m_lFrmSize )
        {
            printf( "RTSP::Play - Finished\n" );
            return;
        }
        else if( dtFrm.m_lFrmSize < 0 )
        {
            if( !oH264.is_opening() )
                cerr << "RTSP::Play - File is not opened!" << endl;
            else
                cerr << "RTSP::Play - Get frame failed!" << endl;

            return;
        }

        // Push Stream
        this->PushStream( tTimeStampStep, oRTPMan, &dtFrm, (sockaddr*)&sinDst );

        //
        this_thread::sleep_for( chrono::milliseconds( tSleepPeriod ) );
    }
}

bool RTSP::PushStream( const uint32_t nTimestampStep, RTPMan& oRTPMan, FrameInfo* lpFrmInfo, const sockaddr* lpClientSockAddr )
{
    uint8_t* pData     = NULL;
    int64_t  lDataSize = 0;
    int64_t  lPktCnt   = 0;
    int64_t  lMaxData  = RTP_DATA_MAX_SIZE;

    switch( lpFrmInfo->m_eCodecType )
    {
    case CODEC_H264:
        {
            H264::FrameInfoEx* pFrmEx = dynamic_cast< H264::FrameInfoEx* >( lpFrmInfo );
            if( pFrmEx )
            {
                pData     = pFrmEx->m_pBuf + pFrmEx->m_iStartCodeBytes;     // NALU data
                lDataSize = pFrmEx->m_lFrmSize - pFrmEx->m_iStartCodeBytes; // NALU size
                lPktCnt   = lDataSize / lMaxData;
            }
            else
            {
                cerr << "RTSP::PushStream - dynamic_cast H264::FrameInfoEx* failed!" << endl;
                return false;
            }
        }
        break;

    case CODEC_H265:
        {
            return false;   // Not Support
        }
        break;

    default:
        return false;
    }

    //
    int64_t lPos = 0;

    if( lDataSize % lMaxData > 0 ) ++lPktCnt;  // Add a cycle for the recent data

    for( int64_t i = 0, lPending = 0; i < lPktCnt; ++i )
    {
        //
        lPending = oRTPMan.SetPayloadData( lpFrmInfo->m_eCodecType, pData, lPos, lDataSize, i );

        //
        if( 1 == lPktCnt )
            lPending += RTP_HEADER_SIZE;
        else
            lPending += ( FU_SIZE + RTP_HEADER_SIZE );

        oRTPMan.SendPacket( this->m_fdRTP, lPending, 0, lpClientSockAddr );

        oRTPMan.Forward( nTimestampStep );

        //
        lPos += RTP_DATA_MAX_SIZE;
    }

    return true;
}

bool RTSP::ParseRequest( int iBufSize, const char* lpszBuf, ConnInfo* lpData )
{
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
                    &lpClientInfo->m_dtRTSP.m_iClientPort_RTP, &lpClientInfo->m_dtRTSP.m_iClientPort_RTCP ) != 2 )
                {
                    cerr << "RTSP::ParseRequest - Parse RTSP SETUP content error!" << endl;
                    return false;
                }

                break;
            }
        }
    }

    return true;
}

bool RTSP::SendResponse( ConnInfo* lpData )
{
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
