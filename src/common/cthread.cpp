#include "cthread.h"
#include "counter.h"
#include <unistd.h>

CThread::CThread( thread&& t ) : m_thread( move( t ) )
{
    if( m_thread.joinable() )
    {
        m_hThread  = m_thread.native_handle();
        m_bRunning = m_bCreate = true;
    }
}

CThread::~CThread()
{
    this->ReleaseThread();
}

void* CThread::ThreadProcess()
{
    this->set_running( true );
    this->ThreadFunction();
    this->set_running( false );

    return nullptr;
}

bool CThread::InitThread( thread&& t, const int iEventCnt )
{
    if( m_mtx.Lock() )
    {
        m_bExit = false;

        // Release Thread
        if( this->is_running() && m_bCreate )
        {
            //pthread_cancel( m_hThread );
            //pthread_exit( &m_hThread );
            this->set_running( false );

            m_thread.join();

            m_bCreate = false;
        }

        //
        m_thread = move( t );

        if( m_thread.joinable() )
        {
            m_hThread  = m_thread.native_handle();
            m_bRunning = m_bCreate = true;
        }

        m_mtx.Unlock();
        return true;
    }

    return false;
}

bool CThread::InitThread( LPTHREADFUNCTION lpFunction, void* lpUserData, const int iEventCnt )
{
    return this->InitThread( move( thread( lpFunction, lpUserData ) ), iEventCnt );
}

void CThread::ReleaseThread()
{
    if( m_mtx.Lock() && m_bCreate )
    {
        // Release Thread
        if( this->is_running() )
        {
            //pthread_cancel( m_hThread );
            //pthread_exit( &m_hThread );
            this->set_running( false );
        }

        m_thread.join();

        m_bCreate = false;

        m_mtx.Unlock();
    }
}

bool CThread::SetExit( const int iWaittingQuitMSec, const bool bRelease )
{
    // Set exit flag
    if( m_mtx.Lock() )
    {
        if( !m_bCreate )
        {
            m_mtx.Unlock();
            return false;
        }

        m_bExit = true;

        m_mtx.Unlock();
    }

    // Set exit event
    ;

    //
    bool bThreadRunning = true;

    if( iWaittingQuitMSec >= 1 )
    {
        Counter counter( true );

        while( bThreadRunning )
        {
            if( m_mtx.Lock( 10 ) )
            {
                bThreadRunning = this->is_running();
                m_mtx.Unlock();
            }

            // Is thread stop?
            if( !bThreadRunning )
                break;

            // Timeout
            if( iWaittingQuitMSec > 1 )
            {
                if( counter.Stop() >= iWaittingQuitMSec )
                    break;
            }

            this_thread::sleep_for( chrono::milliseconds( 10 ) );
        }
    }

    //
    if( bRelease )
    {
        if( !bThreadRunning )
            this_thread::sleep_for( chrono::milliseconds( 100 ) );

        this->ReleaseThread();
    }

    return true;
}
