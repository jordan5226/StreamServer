#pragma once

#include <mutex>
#include <thread>

#define INFINITY    0xffffffff

class CMutex
{
// Attributes    
private:
    uint32_t m_nCount = 0;
    uint32_t m_nTID = 0;
    std::timed_mutex m_mtx;

public:
    CMutex();
    ~CMutex();

    operator std::timed_mutex&()
    {
        return this->m_mtx;
    }

    bool Lock( int iTimeout = INFINITY );
    bool Unlock();

    inline bool is_locked() { return ( this->m_nTID != 0 && this->m_nCount >= 1 ); }
    inline bool is_init() { return ( this->m_mtx.native_handle() != nullptr ); }
};
