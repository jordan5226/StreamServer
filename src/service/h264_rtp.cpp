#include "h264_rtp.h"
#include "../codec/h264.h"
#include <iostream>

using namespace std;

H264RTPSession::H264RTPSession( const RTPHeader& dtHeader ) : RTPSession( dtHeader )
{
}

H264RTPSession::~H264RTPSession()
{
}

void H264RTPSession::ThreadFunction()
{
    // Set RTP header
    this->set_rtp_header( RTPHeader( RTP_PAYLOAD_TYPE_H264, 0, 0, this->m_iSSRC ) );

    // Open source file
    H264::FrameInfoEx dtFrm;
    H264 oH264;
    oH264.OpenFile( this->m_strMediaFile.c_str(), CFileMap::modeRead );

    // Streaming
    const auto tTimestampStep = uint32_t( 90000 / FPS );    // Timestamp gap between two frames
    const auto tSleepPeriod   = uint32_t( 1000 / FPS );     // Time gap between two frames

    while( !this->is_exit() )
    {
        // Read Data
        dtFrm = oH264.GetFrame();

        if( 0 == dtFrm.m_lFrmSize )
        {
            cout << "H264RTPSession::ThreadFunction - Finished" << endl;
            return;
        }
        else if( dtFrm.m_lFrmSize < 0 )
        {
            if( !oH264.is_opening() )
                cerr << "H264RTPSession::ThreadFunction - File is not opened!" << endl;
            else
                cerr << "H264RTPSession::ThreadFunction - Get frame failed!" << endl;

            return;
        }

        // Push Stream
        this->PushStream( tTimestampStep, &dtFrm );

        //
        this_thread::sleep_for( chrono::milliseconds( tSleepPeriod ) );
    }
}
