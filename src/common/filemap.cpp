#include "filemap.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

CFileMap::CFileMap()
{
}

CFileMap::~CFileMap()
{
    this->Close();
}

bool CFileMap::OpenFile( const char* lpszFullFilePath, const CFileMap::OpenFlags eFlag )
{
    if( !lpszFullFilePath )
    {
        fprintf( stderr, "CFileMap::OpenFile - Invalid argument!\n" );
        return false;
    }

    // Close opening file
    if( m_fdFile > 0 )
        this->Close();

    // Open file
    if( eFlag & modeReadWrite )
    {
        if( eFlag & modeCreate )
            m_fdFile = open( lpszFullFilePath, ( O_CREAT | O_RDWR | O_TRUNC ) );
        else
            m_fdFile = open( lpszFullFilePath, O_RDWR );
    }
    else if( eFlag & modeWrite )
        m_fdFile = open( lpszFullFilePath, O_WRONLY );
    else
        m_fdFile = open( lpszFullFilePath, O_RDONLY );

    // Map file to memory
    if( m_fdFile > 0 )
    {
        m_strFile = lpszFullFilePath;

        //
        struct stat dtFileStat{ 0 };
        fstat( m_fdFile, &dtFileStat );
        m_lFileSize = dtFileStat.st_size;

        //
        int iProt = PROT_NONE;
        if( eFlag & modeReadWrite )
            iProt = PROT_READ | PROT_WRITE;
        else if( eFlag & modeWrite )
            iProt = PROT_WRITE;
        else
            iProt = PROT_READ;

        m_pStart = reinterpret_cast< uint8_t* >( mmap( NULL, m_lFileSize, iProt, MAP_PRIVATE, m_fdFile, 0 ) );

        if( MAP_FAILED == m_pStart )
        {
            fprintf( stderr, "CFileMap::OpenFile - mmap failed! ( File: '%s' )\n", lpszFullFilePath );
            this->Close();
            return false;
        }

        m_pCurrent = m_pStart;
        m_pEnd     = m_pStart + m_lFileSize;

        return true;
    }
    else
    {
        fprintf( stderr, "CFileMap::OpenFile - Open file failed! ( File: '%s' )\n", lpszFullFilePath );
    }

    return false;
}

void CFileMap::Close()
{
    if( m_fdFile > 0 )
        close( m_fdFile );

    if( m_pStart )
        munmap( reinterpret_cast< void* >( m_pStart ), m_lFileSize );

    m_pStart = m_pEnd = m_pCurrent = NULL;
}

bool CFileMap::Seek( int iOffset, CFileMap::SeekPosition eFrom )
{
    if( m_fdFile <= 0 || !m_pStart )
        return false;

    switch( eFrom )
    {
    case CFileMap::begin:
        m_pCurrent = m_pStart + iOffset;
        break;

    case CFileMap::current:
        m_pCurrent += iOffset;
        break;
    }

    if( m_pCurrent > m_pEnd )
        m_pCurrent = m_pEnd;

    return true;
}
