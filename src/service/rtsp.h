#pragma once

#include "network.h"
#include <string>
#include <vector>
#include <thread>
#include <memory>
#include "../common/cthread.h"
#include "../codec/h264.h"
#include "rtp.h"

using namespace std;

#define NUM_RTSP        2
#define PORT_RTSP1      554
#define PORT_RTSP2      8554
#define PORT_RTP        3456    // even number
#define PORT_RTCP       PORT_RTP + 1
#define RTSP_TIMEOUT    600
#define FPS             30

class RTSP : public CSocket
{
// Attributes
public:
    struct RTSPInfo
    {
        int     m_iCSeq            = 0;
        int     m_iSSRC            = 0;
        int     m_iClientPort_RTP  = 0;
        int     m_iClientPort_RTCP = 0;
        char    m_szMethod[ 9 ]    = { 0 };
        char    m_szUrl[ 512 ]     = { 0 };
        char    m_szVer[ 9 ]       = { 0 };
        string  m_strSessionID     = "";    // Session UUID
    };

    struct ClientInfo : public ConnInfo
    {
        RTSPInfo m_dtRTSP;
        shared_ptr< CThread > m_pThread;

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
    string m_strFile               = "";
    vector< int >        m_vSSRC;
    vector< ClientInfo > m_vClient;
    CMutex m_mtx;

// Methods
public:
    RTSP( const char* lpszFile );
    virtual ~RTSP();

    virtual int StartService() override;

    inline bool is_running() { return this->m_bRunning; }
    inline void set_running( bool bRun ) { this->m_bRunning = bRun; }

    static void PlayThread( ClientInfo& dtClient, RTSP* lpThis );

private:
    bool ProcessClient( ClientInfo& dtClient );
    bool CreateSSRC( int& iSSRC );
    void RemoveSSRC( int iSSRC );
    bool MakeResponse( int iBufSize, char* lpszBuf, RTSPInfo& dtRTSP );
    bool PushStream( const uint32_t nTimestampStep, RTPMan& oRTPMan, FrameInfo* lpFrmInfo, const sockaddr* lpClientSockAddr );
    void Play( ClientInfo& dtClient );
    bool Lock( int iTimeout = INFINITY );
    bool Unlock();

    virtual bool ParseRequest( int iBufSize, const char* lpszBuf, ConnInfo* lpData ) override;
    virtual bool SendResponse( ConnInfo* lpData ) override;
};
