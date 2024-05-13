#pragma once

#include <string>

using namespace std;

class CFileMap
{
// Attributes
public:
    enum OpenFlags
    {
        modeRead       = 0x0000,
        modeWrite      = 0x0001,
        modeReadWrite  = 0x0002,
        shareCompat    = 0x0000,
        shareExclusive = 0x0010,
        shareDenyWrite = 0x0020,
        shareDenyRead  = 0x0030,
        shareDenyNone  = 0x0040,
        modeNoInherit  = 0x0080,
        modeCreate     = 0x1000,
        modeNoTruncate = 0x2000,
        typeText       = 0x4000,
        typeBinary     = (int)0x8000
    };

    enum SeekPosition
    {
        begin   = 0x0,
        current = 0x1,
        end     = 0x2,
    };

protected:
    int      m_fdFile    = -1;
    uint8_t* m_pStart    = NULL;
    uint8_t* m_pEnd      = NULL;
    uint8_t* m_pCurrent  = NULL;
    int64_t  m_lFileSize = 0;
    string   m_strFile   = "";

// Methods
public:
    CFileMap();
    virtual ~CFileMap();

    bool OpenFile( const char* lpszFullFilePath, const CFileMap::OpenFlags nFlag );
    void Close();
    bool Seek( int iOffset, CFileMap::SeekPosition eFrom );

    //  ==========================================================================
    //  Inline Function
    //  ==========================================================================
    inline bool     is_end()        { return ( m_pCurrent == m_pEnd ); }
    inline bool     is_opening()    { return ( m_fdFile > 0 ); }
    inline uint8_t* get_cur_pos()   { return m_pCurrent; }
    inline uint8_t* get_end_pos()   { return m_pEnd; }
    inline string   get_filename()  { return m_strFile; }
    inline int64_t  get_length()    { return m_lFileSize; }
};
