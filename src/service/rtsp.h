#pragma once

#include "network.h"
#include <string>
#include <vector>
#include <memory>
#include "../codec/codec.h"
#include "h264_rtp.h"
#include "aac_rtp.h"

using namespace std;

#define SUPPORT_VIDEO   1
#define SUPPORT_AUDIO   1

#define LEN_FILENAME    32

#define NUM_RTSP        2
#define PORT_RTSP1      554
#define PORT_RTSP2      8554
#define PORT_RTP        3456    // even number
#define PORT_RTCP       PORT_RTP + 1
#define RTSP_TIMEOUT    60

class RTSP : public CSocket
{
// Attributes
public:
    struct RTSPInfo
    {
        enum TRACK
        {
            TRACK_VIDEO = 0,
            TRACK_AUDIO,
            TRACK_COUNT,
        };

        int     m_iCSeq                             = 0;
        int     m_arrClientPort_RTP[ TRACK_COUNT ]  = { 0 };
        int     m_arrClientPort_RTCP[ TRACK_COUNT ] = { 0 };
        int     m_arrSSRC[ TRACK_COUNT ]            = { 0 };
        char    m_szMethod[ 9 ]                     = { 0 };
        char    m_szUrl[ 512 ]                      = { 0 };
        char    m_szVer[ 9 ]                        = { 0 };
        string  m_strSessionID = "";    // Session UUID

        inline void get_file_name( char* lppszFileName );
        inline int get_track_idx();
    };

    struct ClientInfo : public ConnInfo
    {
        RTSPInfo m_dtRTSP;
        shared_ptr< RTPSession > m_pVidThread;
        shared_ptr< RTPSession > m_pAudThread;

        ClientInfo() : ConnInfo() {}
        virtual ~ClientInfo() {}

        void ReleaseThread();
        void Disconnect();
    };

private:
    bool   m_bRunning              = false;
    int    m_arrRTSPFD[ NUM_RTSP ] = { -1, -1 };
    int    m_fdRTP                 = -1;
    int    m_fdRTCP                = -1;
    string m_strFilePath           = "";
    vector< int >         m_vSSRC;
    vector< ClientInfo* > m_vClient;
    CMutex m_mtx;

// Methods
public:
    RTSP( const char* lpszFilePath );
    virtual ~RTSP();

    virtual int StartService() override;

    inline bool is_running() { return this->m_bRunning; }
    inline void set_running( bool bRun ) { this->m_bRunning = bRun; }

private:
    bool ProcessClient( ClientInfo& dtClient );
    bool CreateSSRC( int& iSSRC );
    void RemoveSSRC( int iSSRC );
    bool MakeResponse( int iBufSize, char* lpszBuf, RTSPInfo& dtRTSP );
    void PlayVideo( ClientInfo& dtClient );
    void PlayAudio( ClientInfo& dtClient );
    bool Lock( int iTimeout = INFINITY );
    bool Unlock();
    string GetFullFilePath( const char* lpszFileName );

    virtual bool ParseRequest( int iBufSize, const char* lpszBuf, ConnInfo* lpData ) override;
    virtual bool SendResponse( ConnInfo* lpData ) override;
};
