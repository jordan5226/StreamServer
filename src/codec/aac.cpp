#include "aac.h"

int AAC::gm_arrSampFrqIdxValue[ 16 ] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, -1, -1, 0 };

AAC::AAC()
{
}

AAC::~AAC()
{
}

AAC::FrameInfoEx AAC::GetFrame( bool bNext )
{
    FrameInfoEx dtFrm;

    // Is file opening?
    if( !this->is_opening() )
    {
        dtFrm.m_lFrmSize = -1;
        return dtFrm;
    }

    // Is end?
    if( this->is_end() )
        return dtFrm;

    // Is valid ADTS frame?
    uint8_t* pSyncword = NULL;
    if( !this->FindSyncword( this->get_cur_pos(), &pSyncword ) )
    {   // Syncword not found
        dtFrm.m_lFrmSize = -1;
        return dtFrm;
    }

    // Get frame buffer
    if( this->get_cur_pos() != pSyncword )
        this->Seek( pSyncword - this->get_cur_pos(), CFileMap::SeekPosition::current );

    dtFrm.m_pBuf = this->get_cur_pos();

    // Get AAC frame length ( ADTS Header size + AAC Data size )
    dtFrm.m_lFrmSize = ( ( ( (uint16_t)pSyncword[ 3 ] & 0x03 ) << 11 ) |
                         ( ( (uint16_t)pSyncword[ 4 ] & 0xFF ) <<  3 ) |
                         ( ( (uint16_t)pSyncword[ 5 ] & 0xE0 ) >>  5 ) );

    // Get protection absent
    dtFrm.m_nPtAbs = ( pSyncword[ 1 ] & 0x01 );

    // Get profile
    dtFrm.m_nProfile = ( pSyncword[ 2 ] & 0xC0 ) >> 6;

    // Get Sampling Frequency Index
    dtFrm.m_nSampFrqIdx = ( pSyncword[ 2 ] & 0x3C ) >> 2;

    // Get channel numbers
    dtFrm.m_nChannels = ( ( pSyncword[ 2 ] & 0x01 ) << 2 ) | ( ( pSyncword[ 3 ] & 0xC0 ) >> 6 );

    // Get AAC data size
    dtFrm.m_lAACDataSize = dtFrm.m_lFrmSize - dtFrm.get_adts_header_size();

    // Seek to next frame
    if( bNext ) this->Seek( dtFrm.m_lFrmSize, CFileMap::SeekPosition::current );

    return dtFrm;
}

/// @brief Find syncword from the given position
/// @param lpFrom Search from this file position
/// @param lppPos syncword position in file
/// @return Found syncword or not
bool AAC::FindSyncword( const uint8_t* lpFrom, uint8_t** lppPos )
{
    int64_t lRemainBytes = this->get_end_pos() - lpFrom;

    while( lRemainBytes >= ADTS_HEADER_SIZE )
    {
        if( *lpFrom == 0xFF && ( *( lpFrom + 1 ) & 0xF0 ) == 0xF0 ) // Find 0xFFF
        {
            *lppPos = const_cast< uint8_t* >( lpFrom );
            return true;
        }

        lRemainBytes--;
        lpFrom++;
    }

    // Not Found
    *lppPos = NULL;

    return false;
}
