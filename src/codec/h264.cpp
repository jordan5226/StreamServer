#include "h264.h"

H264::H264() : CFileMap()
{
}

H264::~H264()
{
}

H264::FrameInfoEx H264::GetFrame( bool bNext/* = true*/ )
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

    // Is valid frame?
    uint8_t* pStartCode = NULL;
    int iStartCodeBytes = this->FindStartCode( this->get_cur_pos(), &pStartCode );
    if( 0 == iStartCodeBytes )
    {   // StartCode not found
        dtFrm.m_lFrmSize = -1;
        return dtFrm;
    }

    // Get StartCode bytes
    dtFrm.m_iStartCodeBytes = iStartCodeBytes;

    // Get frame buffer
    if( this->get_cur_pos() != pStartCode )
        this->Seek( pStartCode - this->get_cur_pos(), CFileMap::SeekPosition::current );

    dtFrm.m_pBuf = this->get_cur_pos();

    // Find next frame start position, and get current frame info
    iStartCodeBytes = this->FindStartCode( this->get_cur_pos() + iStartCodeBytes, &pStartCode );
    if( 0 == iStartCodeBytes ) // Next startcode not found, is last frame
        dtFrm.m_lFrmSize = this->get_end_pos() - this->get_cur_pos();
    else
        dtFrm.m_lFrmSize = pStartCode - this->get_cur_pos();

    // Seek to next frame
    if( bNext ) this->Seek( dtFrm.m_lFrmSize, CFileMap::SeekPosition::current );
    
    return dtFrm;
}

/// @brief Find startcode from the given position, and return startcode bytes.
/// @param lpFrom Search from this file position
/// @param lppPos StartCode position in file
/// @return StartCode bytes; 0: Not found
int H264::FindStartCode( const uint8_t* lpFrom, uint8_t** lppPos )
{
    int64_t lRemainBytes = this->get_end_pos() - lpFrom;

    while( lRemainBytes >= 3 )
    {
        if( *lpFrom == 0 && *( lpFrom + 1 ) == 0 && *( lpFrom + 2 ) == 1 ) // 0x000001
        {
            *lppPos = const_cast< uint8_t* >( lpFrom );
            return 3;
        }
        
        if( lRemainBytes >= 4 )
        {
            if( *lpFrom == 0 && *( lpFrom + 1 ) == 0 && *( lpFrom + 2 ) == 0 && *( lpFrom + 3 ) == 0x01 )  // 0x00000001
            {
                *lppPos = const_cast< uint8_t* >( lpFrom );
                return 4;
            }
        }

        lRemainBytes--;
        lpFrom++;
    }

    // Not Found
    *lppPos = NULL;

    return 0;
}
