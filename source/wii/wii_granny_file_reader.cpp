// ========================================================================
// $File: //jeffr/granny_29/rt/wii/wii_granny_file_reader.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "wii_granny_revolution.h"

#include "granny_assert.h"
#include "granny_file_reader.h"
#include "granny_log.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode FileReadingLogMessage

USING_GRANNY_NAMESPACE;
BEGIN_GRANNY_NAMESPACE;

struct wii_file_reader
{
    file_reader Base;
    DVDFileInfo FileInfo;
};

END_GRANNY_NAMESPACE;

static CALLBACK_FN(void)
WiiCloseFileReader(file_reader *ReaderInit)
{
    wii_file_reader *Reader = (wii_file_reader *)ReaderInit;

    if(Reader)
    {
        if(!DVDClose(&Reader->FileInfo))
        {
            WiiLogErrorAsWarning(DVDClose);
        }

        Deallocate(Reader);
    }
}

static CALLBACK_FN(int32x)
WiiReadAtMost(file_reader* ReaderInit, int32x FilePosition,
                   int32x UInt8Count, void *Buffer)
{
    CheckPointerNotNull(ReaderInit, return 0);
    wii_file_reader &Reader = *((wii_file_reader*)ReaderInit);

    uint8 iobuf[ 1024 + 32 ];
    uint8 *pbuf = (uint8*) OSRoundUp32B( iobuf );
    uint32 psize = ( iobuf + 1024 + 31 - pbuf ) & ~31;
    uint32 aread = 0;

    uint32 bytes = UInt8Count;
    uint32 offset = FilePosition;
    void *dest = Buffer;

    // do anything at 1K and above
    while ( bytes )
    {
        int32 amt, bytesread, status;

        // how much to read?
        if ( bytes >= psize )
            amt = psize;
        else
            amt = ( bytes + 31 ) & ~31; // round up

        DVDReadAsyncPrio( &Reader.FileInfo, pbuf, amt,
                          offset, 0, 0 );

        // wait until the async read is done
        do
        {
            status = DVDGetFileInfoStatus( &Reader.FileInfo );

            switch ( status )
            {
                case DVD_STATE_FATAL_ERROR:     // A fatal error occurred.
                case DVD_STATE_COVER_CLOSED:    // internal state. never returns.
                case DVD_STATE_NO_DISK:         // There's no disk in the drive.
                case DVD_STATE_COVER_OPEN:      // Cover is open.
                case DVD_STATE_WRONG_DISK:      // Wrong disk is in the drive.
                case DVD_STATE_MOTOR_STOPPED:   // Reserved
                case DVD_STATE_IGNORED:         // DVDPrepareStream only. The request was
                    // ignored because the drive is finishing a track.
                case DVD_STATE_PAUSING:
                case DVD_STATE_RETRY:           // Retry error occurred.
                case DVD_STATE_CANCELED:
                    return( 0 );

                case DVD_STATE_END:
                    // worked!
                    bytesread = amt;
                    goto success;
            }
        } while ( 1 );

 success:

        // clamp
        if ( (uint32)bytesread > bytes )
            bytesread = bytes;

        Copy(bytesread, pbuf, dest);
        dest = ( ( uint8 * ) dest ) + bytesread;
        aread += bytesread;
        bytes -= bytesread;
        offset += bytesread;
    }

    return(aread);
}

static CALLBACK_FN(bool)
WiiGetReaderSize(file_reader* ReaderInit, int32x* ResultSize)
{
    CheckPointerNotNull(ReaderInit, return false);
    wii_file_reader &Reader = *((wii_file_reader*)ReaderInit);

    CheckPointerNotNull(ResultSize, return false);
    *ResultSize = Reader.FileInfo.length;
    return true;
}


file_reader* GRANNY
CreatePlatformFileReaderInternal(char const *FileNameToOpen)
{
    wii_file_reader *Reader = Allocate(wii_file_reader, AllocationUnknown);

    // TODO: What's the deal with this non-constness here?
    if(DVDOpen((char *)FileNameToOpen, &Reader->FileInfo))
    {
        InitializeFileReader(WiiCloseFileReader,
                             WiiReadAtMost,
                             WiiGetReaderSize,
                             Reader->Base);
    }
    else
    {
        Deallocate(Reader);
        WiiLogErrorAsWarning(DVDOpen);
    }

    return((file_reader *)Reader);
}

open_file_reader_callback *GRANNY OpenFileReaderCallback = CreatePlatformFileReaderInternal;
