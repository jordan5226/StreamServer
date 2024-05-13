#include "cmutex.h"
#include <chrono>

CMutex::CMutex()
{
}

CMutex::~CMutex()
{
}

bool CMutex::Lock( int iTimeout/* = INFINITY*/ )
{
    if( this->is_init() )
    {
        uint32_t nTID = pthread_self();

        if( this->m_nTID != 0 && nTID == this->m_nTID )
        {
            this->m_nCount++;
            return true;
        }

        //
        if( iTimeout <= 0 )
        {
            this->m_mtx.lock();
            this->m_nCount = 1;
            this->m_nTID = nTID;

            return true;
        }

        //
        if( this->m_mtx.try_lock_until( std::chrono::steady_clock::now() + std::chrono::milliseconds( iTimeout ) ) )
        {
            this->m_nCount = 1;
            this->m_nTID = nTID;
            
            return true;
        }
    }

    return false;
}

bool CMutex::Unlock()
{
    if( this->is_init() )
    {
        uint32_t nTID = pthread_self();

        if( this->m_nTID != 0 && nTID == this->m_nTID )
        {
            if( this->m_nCount == 1 )
            {
                this->m_nCount = 0;
                this->m_nTID   = 0;
                this->m_mtx.unlock();
            }
            else if( this->m_nCount > 1 )
            {
                this->m_nCount--;
            }

            return true;
        }
    }

    return false;
}
