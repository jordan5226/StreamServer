#pragma once

#include <sys/time.h>
#include <string.h>
#include <errno.h>

class Counter
{
public:
    int            m_iErrno;
    struct timeval m_tvStart;
    struct timeval m_tvEnd;

    Counter( const bool bStart = false ) : m_iErrno( 0 )
    {
        ::memset( &m_tvStart, 0, sizeof( m_tvStart ) );
        ::memset( &m_tvEnd, 0, sizeof( m_tvEnd ) );

        if( bStart )
            this->Start();
    }

    virtual ~Counter()
    {
        //  this->m_iErrno
        //  this->m_tvStart
        //  this->m_tvEnd
    }

    bool Start()
    {
        if( 0 != gettimeofday( &m_tvStart, NULL ) )
        {
            m_iErrno = errno;
            return false;
        }

        this->m_tvEnd = this->m_tvStart;

        return true;
    }

    int Stop()
    {
        if( 0 != gettimeofday( &m_tvEnd, NULL ) )
        {
            m_iErrno = errno;

            return 0;
        }

        //
        return ( ( m_tvEnd.tv_sec - m_tvStart.tv_sec ) * 1000000 ) + ( ( m_tvEnd.tv_usec - m_tvStart.tv_usec ) /*/ 1000*/ );
    }

    inline bool is_started() { return ( m_tvStart.tv_sec > 0 || m_tvStart.tv_usec > 0 ); }
};

typedef const Counter  C_COUNTER;
typedef Counter*       LPCOUNTER;
typedef const Counter* LPCCOUNTER;
