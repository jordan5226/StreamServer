#include "csemaphore.h"
#include <fcntl.h>
#include <errno.h>
#include <string.h>

CSemaphore::CSemaphore( const char* lpszName/* = NULL*/, bool bGlobal/* = false*/ ) : m_pSem( NULL ), m_strName( "" )
{
    this->Create( lpszName, bGlobal );
}

CSemaphore::~CSemaphore()
{
    this->Release();
}

bool CSemaphore::Create( const char* lpszName/* = NULL*/, bool bGlobal/* = false*/ )
{
    if( m_pSem )
        return true;

    if( lpszName && ( ::strlen( lpszName ) != 0 ) && bGlobal )
    {
        this->m_pSem = sem_open( lpszName, O_RDWR | O_CREAT, 0644, 0 );

        if( SEM_FAILED == m_pSem )
        {
            cerr << "CSemaphore::Create - sem_open Failed! ( Err: " << errno << " )" << endl;
            return false;
        }

        this->m_strName = lpszName;
    }
    else
    {
        m_pSem = new sem_t();

        if( 0 != sem_init( m_pSem, ( bGlobal ? 1 : 0 ), 0 ) )
        {
            cerr << "CSemaphore::Create - sem_init Failed! ( Err: " << errno << " )" << endl;

            delete m_pSem;
            m_pSem = NULL;

            return false;
        }
    }

    return true;
}

void CSemaphore::Release()
{
	if( m_pSem )
    {
        if( m_strName.empty() )
        {
            if( 0 != sem_destroy( m_pSem ) )
                cerr << "CSemaphore::Release - sem_destroy Failed! ( Err: " << errno << " )" << endl;

            delete m_pSem;
            m_pSem = NULL;
        }
        else
        {
            if( 0 != sem_close( m_pSem ) )
                cerr << "CSemaphore::Release - sem_close Failed! ( Err: " << errno << " )" << endl;

            if( 0 != sem_unlink( m_strName.c_str() ) )
                cerr << "CSemaphore::Release - sem_unlink Failed! ( Err: " << errno << " )" << endl;
        }
    }

	this->m_pSem = NULL;
	this->m_strName.clear();
}

bool CSemaphore::Wait()
{
    if ( !m_pSem )
	{
		cerr << "CSemaphore::Wait - Invalid Mutex Handle!" << endl;
		return false;
	}

    int ret = sem_wait( m_pSem );

    if( ret != 0 )
    {
        cerr << "CSemaphore::Wait - Failed! ( Err: " << errno << " )" << endl;
        return false;
    }

    return true;
}

bool CSemaphore::Post()
{
    if ( !m_pSem )
	{
		cerr << "CSemaphore::Post - Invalid Mutex Handle!" << endl;
		return false;
	}

    int ret = sem_post( m_pSem );

    if( ret != 0 )
    {
        cerr << "CSemaphore::Post - Failed! ( Err: " << errno << " )" << endl;
        return false;
    }

    return true;
}
