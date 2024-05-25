#pragma once

#include <semaphore.h>
#include <iostream>

using namespace std;

class CSemaphore
{
private:
    sem_t*      m_pSem;
    std::string m_strName;

public:
    CSemaphore( const char* lpszName = NULL, bool bGlobal = false );
    ~CSemaphore();

	virtual bool Create( const char* lpszName = NULL, bool bGlobal = false );
	virtual void Release();
    virtual bool Wait();
    virtual bool Post();

    //  ==========================================================================
	//  Inline Function
	//  ==========================================================================
    inline operator sem_t* () const
	{
		return this->m_pSem;
	}

    inline sem_t* get_semaphore()
    {
		return this->m_pSem;
	}

	inline bool is_init() const
	{
		return ( this->m_pSem != NULL );
	}

	inline const char* get_name()
	{
		return this->m_strName.c_str();
	}
};
