#include "rtp.h"
#include <netinet/in.h>
#include <string.h>
#include <algorithm>
#include "../codec/h264.h"

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
    m_ssrc        = htonl( m_ssrc );
}

RTPHeader::RTPHeader( const uint16_t seq, const uint32_t timestamp, const uint32_t ssrc )
{
    m_version     = RTP_VER;
    m_padding     = 0;
    m_extension   = 0;
    m_csrcCount   = 0;
    m_marker      = 0;
    m_payloadType = RTP_PAYLOAD_TYPE_H264;
    m_seq         = htons( seq );
    m_timestamp   = htonl( timestamp );
    m_ssrc        = htonl( m_ssrc );
}

RTPHeader::~RTPHeader()
{
}

RTPMan::RTPMan( const RTPHeader& dtHeader ) : RTPPacket( dtHeader )
{
}

RTPMan::~RTPMan()
{
}

/// @brief Set RTP payload data
/// @param eCodec Stream Codec type
/// @param lpData Stream actual data
/// @param iOffset Data offset
/// @param lDataSize Stream actual data size
/// @param iIndex Fragmentation unit index
/// @return Set data size
int64_t RTPMan::SetPayloadData( const CODEC_TYPE eCodec, const uint8_t* lpData, const int64_t iOffset, int64_t lDataSize, const int iIndex )
{
    if( ( 0 == iIndex ) && ( lDataSize < RTP_DATA_MAX_SIZE ) )
    {
        memcpy( m_dtPayload, lpData, std::min( lDataSize, static_cast< int64_t >( sizeof( m_dtPayload ) ) ) );
        return lDataSize;
    }

    // Fragmentation
    switch( eCodec )
    {
    case CODEC_H264:
        {
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
        }
        return lDataSize;

    case CODEC_H265:
        {
            ;   // Not Support
        }
        break;

    default:
        break;
    }

    return 0;
}

/// @brief Send RTP packet
/// @param fdSock send by this socket IO
/// @param iBufSize Send buffer size
/// @param iFlags 
/// @param lpDstSockAddr Destination socket address
/// @return Sent bytes of packet
int64_t RTPMan::SendPacket( int fdSock, int64_t iBufSize, int iFlags, const sockaddr* lpDstSockAddr )
{
    auto sentBytes = sendto( fdSock, static_cast< RTPPacket* >( this ), iBufSize, iFlags, lpDstSockAddr, sizeof( *lpDstSockAddr ) );
    return sentBytes;
}

void RTPMan::Forward( const uint32_t nTimestampStep )
{
    this->m_dtHeader.set_seq( this->m_dtHeader.get_seq() + 1 );
    this->m_dtHeader.set_timestamp( this->m_dtHeader.get_timestamp() + nTimestampStep );
}
