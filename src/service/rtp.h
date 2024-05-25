#pragma once

#include <cstdint>
#include "network.h"
#include "../codec/codec.h"
#include "../common/cthread.h"

#define RTP_VER                 2
#define RTP_PAYLOAD_TYPE_H264   96
#define RTP_PAYLOAD_TYPE_AAC    97
#define RTP_HEADER_SIZE         12
#define FU_SIZE                 2
#define RTP_PAYLOAD_AAC_EX      4
#define RTP_PACKET_MAX_SIZE     PACKET_MAX_SIZE - UDP_HEADER_SIZE - IPV4_HEADER_SIZE
#define RTP_PAYLOAD_MAX_SIZE    RTP_PACKET_MAX_SIZE - RTP_HEADER_SIZE
#define RTP_DATA_MAX_SIZE       RTP_PAYLOAD_MAX_SIZE - FU_SIZE

/* RTP Header
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           timestamp                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           synchronization source (SSRC) identifier            |
   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   |            contributing source (CSRC) identifiers             |
   |                             ....                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
struct RTPHeader
{
#if ( BYTE_ORDER == BIG_ENDIAN )
    // byte 0
    uint8_t m_version : 2;
    uint8_t m_padding : 1;
    uint8_t m_extension : 1;
    uint8_t m_csrcCount : 4;
    // byte 1
    uint8_t m_marker : 1;
    uint8_t m_payloadType : 7;
#elif ( BYTE_ORDER == LITTLE_ENDIAN )
    // byte 0
    uint8_t m_csrcCount : 4;
    uint8_t m_extension : 1;
    uint8_t m_padding : 1;
    uint8_t m_version : 2;
    // byte 1
    uint8_t m_payloadType : 7;
    uint8_t m_marker : 1;
#else
    #error BYTE_ORDER or BIG_ENDIAN or LITTLE_ENDIAN not defined!
#endif
    // byte 2, 3
    uint16_t m_seq;
    // byte 4-7
    uint32_t m_timestamp;
    // byte 8-11
    uint32_t m_ssrc;

    RTPHeader() = default;
    RTPHeader( const RTPHeader& dtHeader );
    RTPHeader( const uint8_t version, const uint8_t padding, const uint8_t extension, const uint8_t csrcCount, 
        const uint8_t marker, const uint8_t payloadType, const uint16_t seq, const uint32_t timestamp, const uint32_t ssrc );
    RTPHeader( const uint8_t payloadType, const uint16_t seq, const uint32_t timestamp, const uint32_t ssrc );
    ~RTPHeader();

    inline uint16_t get_seq() { return ntohs( m_seq ); }
    inline uint32_t get_timestamp() { return ntohl( m_timestamp ); }
    inline uint32_t get_ssrc() { return ntohl( m_ssrc ); }

    inline void set_seq( uint16_t seq ) { this->m_seq = htons( seq ); }
    inline void set_timestamp( uint32_t timestamp ) { this->m_timestamp = htonl( timestamp ); }
    inline void set_ssrc( uint32_t ssrc ) { this->m_ssrc = htonl( ssrc ); }
    inline void set_marker( uint8_t marker ) { this->m_marker = marker; }
};

struct RTPPacket
{
    RTPHeader   m_dtHeader;
    uint8_t     m_dtPayload[ RTP_PAYLOAD_MAX_SIZE ] = { 0 };

    RTPPacket() = default;
    RTPPacket( const RTPHeader& dtHeader ) : m_dtHeader( dtHeader ) {}

    int64_t SetPayloadData( const CODEC_TYPE eCodec, const uint8_t* lpData, const int64_t iOffset, int64_t lDataSize, const int iIndex );
    void Forward( const uint32_t nTimestampStep );

    inline void set_rtp_header( const RTPHeader& dtHeader ) { this->m_dtHeader = dtHeader; }
};

class RTPSession : public CThread
{
// Attributes
protected:
    int          m_fdRTP;               // RTP socket fd
    int          m_iSSRC = 0;
    string       m_strMediaFile = "";   // Media source full file path
    sockaddr_in  m_sinDst = { 0 };      // Destination socket address
    RTPPacket    m_dtRTPPkt;

// Methods
public:
    RTPSession() = default;
    RTPSession( const RTPHeader& dtHeader );
    ~RTPSession();

    void Start( int fdRTP, int iSSRC, string strMediaFile, const char* lpszDstIP, int iDstPort );

    inline void set_rtp_header( const RTPHeader& dtHeader ) { m_dtRTPPkt.set_rtp_header( dtHeader ); }

protected:
    int64_t SendPacket( int64_t lPayloadBufSize, int iFlags );
    bool    PushStream( const uint32_t nTimestampStep, FrameInfo* lpFrmInfo );
};
