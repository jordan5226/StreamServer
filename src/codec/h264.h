#pragma once

#include "../common/filemap.h"
#include "codec.h"

#define H264_FILE_EXT   ".h264"

#define FPS             30

#define NALU_FNRI_MASK  0xE0
#define NALU_TYPE_MASK  0x1F

#define FU_FUA_MASK     0x1C    // 28: Use FU-A Fragmentation 
#define FU_S_MASK       0x80
#define FU_E_MASK       0x40

class H264 : public CFileMap
{
// Attributes
public:
    struct FrameInfoEx : public FrameInfo
    {
        int m_iStartCodeBytes = 0;
        //uint8_t* m_pBuf     = NULL;  // Startcode + NALU( NAL Header + RBSP )
        //int64_t  m_lFrmSize = 0;     // 0: End, -1: Failed

        FrameInfoEx()
        {
            m_eCodecType = CODEC_TYPE::CODEC_H264;
        }

        virtual ~FrameInfoEx() {}

        inline uint8_t* get_nalu_data() { return ( this->m_pBuf + this->m_iStartCodeBytes ); }
        inline int64_t  get_nalu_size() { return ( this->m_lFrmSize - this->m_iStartCodeBytes );}
    };

// Methods
public:
    H264();
    ~H264();

    FrameInfoEx GetFrame( bool bNext = true );
    int FindStartCode( const uint8_t* lpFrom, uint8_t** lppPos );
};
