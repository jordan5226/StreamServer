#pragma once

#include <thread>
#include <atomic>
#include "cmutex.h"

using namespace std;

typedef void* ( *LPTHREADFUNCTION )( void* lpUserData );

class CThread
{
public:
    enum
    {
        DEFAULT_EVENT_COUNT = 1,
    };

private:
    atomic< bool >  m_bExit{ false };    // Do exit flag
    atomic< bool >  m_bCreate{ false };  // Creating state
    atomic< bool >  m_bRunning{ false }; // Running state
    pthread_t       m_hThread = NULL;
    thread          m_thread;
    CMutex          m_mtx;

public:
    CThread() = default;
    CThread( const thread& t ) = delete;
    CThread( thread&& t );
    ~CThread();

    virtual void*   ThreadProcess();
    virtual void    ThreadFunction() {}
    virtual bool    Init( thread&& t, const int iEventCnt );
    virtual bool    Init( LPTHREADFUNCTION lpFunction, void* lpUserData, const int iEventCnt );

    void ReleaseThread();
    bool SetExit( const int iWaittingQuitMSec, const bool bRelease );

    // Inline
    inline bool is_exit() { return this->m_bExit; }
    inline bool is_running() { return this->m_bRunning; }
    inline void set_running( bool bRunning ) { this->m_bRunning = bRunning; }

    inline void InitialThread( void* lpUserData, const int iEventCnt = CThread::DEFAULT_EVENT_COUNT )
    {
        this->InitialThread( CThread::InitialThreadProc, lpUserData, iEventCnt );
    }

    inline void InitialThread( LPTHREADFUNCTION lpFunction, void* lpUserData )
    {
        this->InitialThread( lpFunction, lpUserData, CThread::DEFAULT_EVENT_COUNT );
    }

    inline void InitialThread( LPTHREADFUNCTION lpFunction, void* lpUserData, const int iEventCnt )
    {
        this->InitialThread( lpFunction, lpUserData, iEventCnt );
    }

private:
    static void* InitialThreadProc( void* lpUserData ) { return ( lpUserData ? reinterpret_cast< CThread* >( lpUserData )->ThreadProcess() : NULL ); }
};
