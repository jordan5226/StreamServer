#pragma once

#include "rtp.h"

class AACRTPSession : public RTPSession
{
public:
    AACRTPSession() = default;
    AACRTPSession( const RTPHeader& dtHeader );
    ~AACRTPSession();

protected:
    virtual void    ThreadFunction();
};
