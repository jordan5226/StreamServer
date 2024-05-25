#pragma once

#include <stdint.h>
#include <cstddef>

enum CODEC_TYPE
{
    CODEC_UNKNOWN = 0,
    CODEC_H264,
    CODEC_H265,
    CODEC_AAC,
};

struct FrameInfo
{
    CODEC_TYPE  m_eCodecType = CODEC_TYPE::CODEC_UNKNOWN;
    uint8_t*    m_pBuf       = NULL;
    int64_t     m_lFrmSize   = 0;

    virtual ~FrameInfo() {}

    FrameInfo& operator = ( const FrameInfo& dtFrm )
    {
        if( this != &dtFrm )
        {
            this->m_eCodecType = dtFrm.m_eCodecType;
            this->m_pBuf       = dtFrm.m_pBuf;
            this->m_lFrmSize   = dtFrm.m_lFrmSize;
        }

        return *this;
    }
};
