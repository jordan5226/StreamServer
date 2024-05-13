#pragma once

#include <stdint.h>
#include <cstddef>

enum CODEC_TYPE
{
    CODEC_UNKNOWN = 0,
    CODEC_H264,
    CODEC_H265,
};

struct FrameInfo
{
    CODEC_TYPE  m_eCodecType = CODEC_TYPE::CODEC_UNKNOWN;
    uint8_t*    m_pBuf       = NULL;
    int64_t     m_lFrmSize   = 0;

    virtual ~FrameInfo() {}
};
