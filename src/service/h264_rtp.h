#pragma once

#include "rtp.h"

class H264RTPSession : public RTPSession
{
public:
    H264RTPSession() = default;
    H264RTPSession( const RTPHeader& dtHeader );
    ~H264RTPSession();

protected:
    virtual void    ThreadFunction();
};
