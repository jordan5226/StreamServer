#include "rtp.h"
#include <netinet/in.h>
#include <string.h>
#include <algorithm>
#include <iostream>
#include "../codec/h264.h"
#include "../codec/aac.h"

using namespace std;

RTPHeader::RTPHeader( const RTPHeader& dtHeader )
{
    ::memcpy( this, &dtHeader, sizeof( RTPHeader ) );
}

RTPHeader::RTPHeader( const uint8_t version, const uint8_t padding, const uint8_t extension, const uint8_t csrcCount,
                      const uint8_t marker, const uint8_t payloadType, const uint16_t seq, const uint32_t timestamp, const uint32_t ssrc )
{
    m_version     = version;
    m_padding     = padding;
    m_extension   = extension;
    m_csrcCount   = csrcCount;
    m_marker      = marker;
    m_payloadType = payloadType;
    m_seq         = htons( seq );
    m_timestamp   = htonl( timestamp );
    m_ssrc        = htonl( ssrc );
}

RTPHeader::RTPHeader( const uint8_t payloadType, const uint16_t seq, const uint32_t timestamp, const uint32_t ssrc )
{
    m_version     = RTP_VER;
    m_padding     = 0;
    m_extension   = 0;
    m_csrcCount   = 0;
    m_marker      = 0;
    m_payloadType = payloadType;
    m_seq         = htons( seq );
    m_timestamp   = htonl( timestamp );
    m_ssrc        = htonl( ssrc );
}

RTPHeader::~RTPHeader()
{
}

/// @brief Set RTP payload data
/// @param eCodec Stream Codec type
/// @param lpData Stream actual data
/// @param iOffset Data offset
/// @param lDataSize Stream actual data size
/// @param iIndex Fragmentation unit index
/// @return The data size set to RTP payload
int64_t RTPPacket::SetPayloadData( const CODEC_TYPE eCodec, const uint8_t* lpData, const int64_t iOffset, int64_t lDataSize, const int iIndex )
{
    switch( eCodec )
    {
    case CODEC_H264:
        {
            // Single NALU packet
            if( ( 0 == iIndex ) && ( lDataSize < RTP_DATA_MAX_SIZE ) )
            {
                memcpy( m_dtPayload, lpData, std::min( lDataSize, static_cast< int64_t >( sizeof( m_dtPayload ) ) ) );
                return lDataSize;
            }

            // Fragmentation packet ( FU-A )
            // Set FU Indicator and FU Header
            m_dtPayload[ 0 ] = ( *lpData & NALU_FNRI_MASK ) | FU_FUA_MASK;
            m_dtPayload[ 1 ] = ( *lpData & NALU_TYPE_MASK );

            if( 0 == iIndex )
                m_dtPayload[ 1 ] |= FU_S_MASK;
            else if( ( lDataSize - 1 - iOffset ) < RTP_DATA_MAX_SIZE )
                m_dtPayload[ 1 ] |= FU_E_MASK;

            // Skip NALU Header ( 1 Byte ), just copy NALU Payload
            lDataSize = std::min( ( lDataSize - 1 - iOffset ), static_cast< int64_t >( RTP_DATA_MAX_SIZE ) );
            memcpy( m_dtPayload + FU_SIZE, lpData + 1 + iOffset, lDataSize );
            lDataSize += FU_SIZE;
        }
        return lDataSize;

    case CODEC_H265:
        {
            ;   // Not Support
        }
        break;

    case CODEC_AAC:
        {
            //
            m_dtPayload[ 0 ] = 0x00;
            m_dtPayload[ 1 ] = 0x10;    // 1 AU-header
            m_dtPayload[ 2 ] = ( lDataSize & AAC_DATASIZE_MASK_H ) >> 5;
            m_dtPayload[ 3 ] = ( lDataSize & AAC_DATASIZE_MASK_L ) << 3;

            //
            lDataSize = std::min( ( lDataSize - iOffset ), static_cast< int64_t >( RTP_DATA_MAX_SIZE ) );
            memcpy( m_dtPayload + RTP_PAYLOAD_AAC_EX, lpData + iOffset, lDataSize );
            lDataSize += RTP_PAYLOAD_AAC_EX;
        }
        return lDataSize;

    default:
        break;
    }

    return 0;
}

void RTPPacket::Forward( const uint32_t nTimestampStep )
{
    this->m_dtHeader.set_seq( this->m_dtHeader.get_seq() + 1 );
    this->m_dtHeader.set_timestamp( this->m_dtHeader.get_timestamp() + nTimestampStep );
}

RTPSession::RTPSession( const RTPHeader& dtHeader ) : m_dtRTPPkt( dtHeader )
{
}

RTPSession::~RTPSession()
{
}

void RTPSession::Start( int fdRTP, int iSSRC, string strMediaFile, const char* lpszDstIP, int iDstPort )
{
    // Set Dst addr info
    m_sinDst.sin_family      = AF_INET;
    m_sinDst.sin_port        = htons( iDstPort );
    m_sinDst.sin_addr.s_addr = inet_addr( lpszDstIP );

    //
    m_fdRTP        = fdRTP;
    m_iSSRC        = iSSRC;
    m_strMediaFile = strMediaFile;

    // Initial streaming thread
    this->InitialThread( this );
}

/// @brief Send RTP packet
/// @param lPayloadBufSize Payload buffer size will send
/// @param iFlags 0
/// @return Sent bytes of packet
int64_t RTPSession::SendPacket( int64_t lPayloadBufSize, int iFlags )
{
    auto sentBytes = sendto( m_fdRTP, &m_dtRTPPkt, ( lPayloadBufSize + RTP_HEADER_SIZE ), iFlags, (sockaddr*)&m_sinDst, sizeof( m_sinDst ) );
    return sentBytes;
}

/// @brief Push RTP Stream
/// @param nTimestampStep Timestamp increment after sending packet
/// @param lpFrmInfo Frame data ready to push
/// @return success or failure
bool RTPSession::PushStream( const uint32_t nTimestampStep, FrameInfo* lpFrmInfo )
{
    uint8_t* pData     = NULL;
    int64_t  lDataSize = 0;
    int64_t  lPktCnt   = 0;
    int64_t  lMaxData  = RTP_DATA_MAX_SIZE;

    switch( lpFrmInfo->m_eCodecType )
    {
    case CODEC_H264:
        {
            H264::FrameInfoEx* pFrmEx = dynamic_cast< H264::FrameInfoEx* >( lpFrmInfo );
            if( pFrmEx )
            {
                pData     = pFrmEx->get_nalu_data(); // NALU data
                lDataSize = pFrmEx->get_nalu_size(); // NALU size
                lPktCnt   = lDataSize / lMaxData;
            }
            else
            {
                cerr << "RTSP::PushStream - dynamic_cast H264::FrameInfoEx* failed!" << endl;
                return false;
            }
        }
        break;

    case CODEC_H265:
        {
            return false;   // Not Support
        }
        break;

    case CODEC_AAC:
        {
            AAC::FrameInfoEx* pFrmEx = dynamic_cast< AAC::FrameInfoEx* >( lpFrmInfo );
            if( pFrmEx )
            {
                pData     = pFrmEx->get_aac_data();         // AAC Data
                lDataSize = pFrmEx->get_aac_data_size();    // AAC Data Size
                lPktCnt   = lDataSize / lMaxData;
            }
            else
            {
                cerr << "RTSP::PushStream - dynamic_cast H264::FrameInfoEx* failed!" << endl;
                return false;
            }
        }
        break;

    default:
        return false;
    }

    //
    int64_t lPos = 0;

    if( lDataSize % lMaxData > 0 ) ++lPktCnt;  // Add a cycle for the recent data

    for( int64_t i = 0, lPending = 0; i < lPktCnt; ++i )
    {
        // Set data to RTP payload and get pending send size
        lPending = m_dtRTPPkt.SetPayloadData( lpFrmInfo->m_eCodecType, pData, lPos, lDataSize, i );

        // Send RTP packet
        if( i == lPktCnt - 1 )
            m_dtRTPPkt.m_dtHeader.set_marker( 1 ); // Mark as end of frame

        this->SendPacket( lPending, 0 );
        m_dtRTPPkt.Forward( nTimestampStep );

        //
        lPos += RTP_DATA_MAX_SIZE;
    }

    return true;
}
