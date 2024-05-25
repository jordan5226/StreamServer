#include "aac_rtp.h"
#include "../codec/aac.h"
#include <iostream>

using namespace std;

AACRTPSession::AACRTPSession( const RTPHeader& dtHeader ) : RTPSession( dtHeader )
{
}

AACRTPSession::~AACRTPSession()
{
}

void AACRTPSession::ThreadFunction()
{
    // Set RTP header
    this->set_rtp_header( RTPHeader( RTP_PAYLOAD_TYPE_AAC, 0, 0, this->m_iSSRC ) );

    // Open source file
    AAC::FrameInfoEx dtFrm;
    AAC aac;
    aac.OpenFile( this->m_strMediaFile.c_str(), CFileMap::modeRead );

    // Streaming
    uint32_t tTimestampStep = 0;    // Timestamp gap between two frames
    int32_t  tSleepPeriod   = 0;    // Time gap between two frames

    while( !this->is_exit() )
    {
        // Read Data
        dtFrm = aac.GetFrame();

        if( 0 == dtFrm.m_lFrmSize )
        {
            cout << "AACRTPSession::ThreadFunction - Finished\n" << endl;
            return;
        }
        else if( dtFrm.m_lFrmSize < 0 )
        {
            if( !aac.is_opening() )
                cerr << "AACRTPSession::ThreadFunction - File is not opened!" << endl;
            else
                cerr << "AACRTPSession::ThreadFunction - Get frame failed!" << endl;

            return;
        }

        // Calculate timestamp step
        // SampleRate per sec / SampleRate per frame = frames per sec
        // SampleRate per sec / frames per sec = frequency ( Timestamp gap between two frames )
        if( 0 == tTimestampStep )
            tTimestampStep = AAC::gm_arrSampFrqIdxValue[ dtFrm.m_nSampFrqIdx ] / ( AAC::gm_arrSampFrqIdxValue[ dtFrm.m_nSampFrqIdx ] / AAC_SAMPLE_RATE_PER_FRAME );

        // Calculate sleep period
        // SampleRate per sec / SampleRate per frame = frames per sec
        // Second / frames = Time gap between two frames
        if( 0 == tSleepPeriod )
            tSleepPeriod = 1000 / ( AAC::gm_arrSampFrqIdxValue[ dtFrm.m_nSampFrqIdx ] / AAC_SAMPLE_RATE_PER_FRAME );

        // Push Stream
        this->PushStream( tTimestampStep, &dtFrm );

        //
        this_thread::sleep_for( chrono::milliseconds( tSleepPeriod ) );
    }
}
