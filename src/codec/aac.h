/* ADTS Header
0              |1              |2              |3              |4              |5              |6              |
0 1 2 3 4 5 6 7|8 9 0 1 2 3 4 5|6 7 8 9 0 1 2 3|4 5 6 7 8 9 0 1|2 3 4 5 6 7 8 9|0 1 2 3 4 5 6 7|8 9 0 1 2 3 4 5|
----------------------- - --- - --- ------- - ----- - - - - ------------------------- --------------------- --- 
           1           |2| 3 |4| 5 |   6   |7|  8  |9|A|B|C|            D            |          E          | F |
----------------------- - --- - --- ------- - ----- - - - - ------------------------- --------------------- --- 
1:syncword: 0xFFF
2:ID: 0:MPEG-4, 1:MPEG-2
3:layer: 0x00
4:protection_absent: 0:has CRC, 1:no CRC
5:profile: AAC codec level ( 0:Main Profile, 1:LC, 2:SSR, 3:reserved )
6:sampling_frequency_index: Sample Rate
7:private_bit: 0
8:channel_configuration: Channel numbers
9:original_copy: 0
A:home: 0
B:copyright_identification_bit: 0
C:copyright_identification_start: 0
D:aac_frame_length: ADTS frame size = ADTS header size(1 == protection_absent ? 7 : 9) + AAC data size
E:adts_buffer_fullness: 0x7FF: Variable bitrate
F:number_of_raw_data_blocks_in_frame: ADTS frame numbers ( 0 Base )
*/

#pragma once

#include "../common/filemap.h"
#include "codec.h"

#define AAC_FILE_EXT        ".aac"

#define ADTS_HEADER_SIZE    7

#define AAC_SAMPLE_RATE_PER_FRAME   1024

#define AAC_DATASIZE_MASK_H 0x1FE0
#define AAC_DATASIZE_MASK_L 0x1F

class AAC : public CFileMap
{
// Attributes
public:
    // ADTS Frame
    struct FrameInfoEx : public FrameInfo
    {
        //uint8_t* m_pBuf     = NULL;  // ADTS Header + AAC Data
        //int64_t  m_lFrmSize = 0;     // 0: End, -1: Failed
        uint8_t m_nPtAbs       = 0;     // Protection Absent
        uint8_t m_nProfile     = 0;     // AAC codec level ( 0:Main Profile, 1:LC, 2:SSR, 3:reserved )
        uint8_t m_nSampFrqIdx  = 0x4;   // Sampling Frequency Index ( Sample Rate )
        uint8_t m_nChannels    = 0;     // Channel numbers
        int64_t m_lAACDataSize = 0;     // AAC Data Size

        FrameInfoEx()
        {
            m_eCodecType = CODEC_TYPE::CODEC_AAC;
        }

        virtual ~FrameInfoEx() {}

        /*FrameInfoEx& operator = ( const FrameInfoEx& dtFrmEx )
        {
            if( this != &dtFrmEx )
            {
                FrameInfo::operator=( dtFrmEx );

                this->m_nPtAbs       = dtFrmEx.m_nPtAbs;
                this->m_nProfile     = dtFrmEx.m_nProfile;
                this->m_nSampFrqIdx  = dtFrmEx.m_nSampFrqIdx;
                this->m_nChannels    = dtFrmEx.m_nChannels;
                this->m_lAACDataSize = dtFrmEx.m_lAACDataSize;
            }

            return *this;
        }*/

        inline uint8_t* get_aac_data() { return ( this->m_pBuf + this->get_adts_header_size() ); }
        inline int64_t  get_aac_data_size() { return this->m_lAACDataSize; }
        inline int64_t  get_adts_header_size() { return ( 1 == this->m_nPtAbs ? 7 : 9 ); }
    };

    static int gm_arrSampFrqIdxValue[ 16 ]; // Sampling Frequency Index Value ( -1: reserved, 0: escape value )

// Methods
public:
    AAC();
    ~AAC();

    FrameInfoEx GetFrame( bool bNext = true );
    bool FindSyncword( const uint8_t* lpFrom, uint8_t** lppPos );
};
