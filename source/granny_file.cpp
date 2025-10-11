// ========================================================================
// $File: //jeffr/granny_29/rt/granny_file.cpp $
// $DateTime: 2012/09/11 11:05:01 $
// $Change: 39276 $
// $Revision: #2 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_file.h"

#include "granny_aggr_alloc.h"
#include "granny_catenating_reader.h"
#include "granny_crc.h"
#include "granny_data_type_io.h"
#include "granny_file_builder.h"
#include "granny_file_info.h"
#include "granny_file_operations.h"
#include "granny_file_reader.h"
#include "granny_file_writer.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_memory_file_reader.h"
#include "granny_memory_file_writer.h"
#include "granny_parameter_checking.h"
#include "granny_platform_convert.h"
#include "granny_telemetry.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary


#undef SubsystemCode
#define SubsystemCode FileReadingLogMessage

USING_GRANNY_NAMESPACE;

static bool
ReadAt(int32x Offset, file_reader* Reader,
       bool Reverse, int32x Bytes, void *Buffer)
{
    bool Result = false;
    Assert(Buffer);

    if(ReadExactly(Reader, Offset, Bytes, Buffer))
    {
        if(Reverse)
        {
            uintaddrx RBytes = Align32(Bytes);
            Reverse32(RBytes, Buffer);
        }

        Result = true;
    }

    return Result;
}

static void *
LoadFileSection(file_reader* Reader, grn_section const &Section,
                bool FileIsByteReversed, void* DestinationMemory)
{
    GRANNY_AUTO_ZONE_FN();
    void* Result = 0;

    if(Section.ExpandedDataSize > 0)
    {
        uintaddrx BlockSize = Align32(Section.ExpandedDataSize);
        if ( DestinationMemory == NULL )
        {
            // No destination specified - allocate your own.
            aggr_allocator Allocator;
            InitializeAggrAlloc(Allocator);

            SetAggrAlignment(Allocator, Section.InternalAlignment);
            AggrAllocSizePtr(Allocator, BlockSize, Result);
            EndAggrAlloc(Allocator, AllocationFileData);
        }
        else
        {
            Result = DestinationMemory;
            // Better hope that was aligned right.
            Assert ( ( uintaddrx(Result) & ( (uintaddrx)Section.InternalAlignment - 1 ) ) == 0 );
        }

        if ( Result != NULL )
        {
            if(Section.Format == NoCompression)
            {
                Assert(Section.DataSize <= Section.ExpandedDataSize);
                ReadAt(Section.DataOffset, Reader,
                       false, Section.DataSize, Result);
            }
            else
            {
                compression_type const Format = (compression_type)Section.Format;
                void *TempBuffer = AllocateSize(Section.DataSize +
                                                GetCompressedBytesPaddingSize(Format),
                                                AllocationTemporary);
                if (TempBuffer)
                {
                    ReadAt(Section.DataOffset, Reader, false, Section.DataSize, TempBuffer);

                    if (!DecompressData(Format, FileIsByteReversed,
                                        Section.DataSize, TempBuffer,
                                        Section.First16Bit, Section.First8Bit,
                                        Section.ExpandedDataSize, Result))
                    {
                        // Only free result if we allocated it!
                        if (DestinationMemory == 0)
                            Deallocate(Result);

                        Result = 0;
                    }

                    Deallocate(TempBuffer);
                }
                else
                {
                    // Only free result if we allocated it!
                    if (DestinationMemory == 0)
                        Deallocate(Result);

                    Result = 0;
                }
            }
        }
    }

    return Result;
}

bool GRANNY
ReadFileSectionInPlace(file_reader* Reader, file &File, int32x SectionIndex, void *DestinationMemory)
{
    CheckPointerNotNull(File.Header, return false);
    CheckInt32Index(SectionIndex, File.SectionCount, return false);

    if(!File.Sections[SectionIndex])
    {
        grn_section const *SectionArray = GetGRNSectionArray(*File.Header);
        Assert(SectionArray);

        grn_section const &Section = SectionArray[SectionIndex];
        File.Sections[SectionIndex] =
            LoadFileSection(Reader, Section, Bool32(File.IsByteReversed), DestinationMemory);
        File.IsUserMemory[SectionIndex] = ( DestinationMemory != NULL ) ? true : false;

        if(File.Sections[SectionIndex] && File.IsByteReversed)
        {
            ReverseSection(Section.First16Bit, Section.First8Bit,
                           Section.ExpandedDataSize,
                           File.Sections[SectionIndex]);
        }

        File.Marshalled[SectionIndex] = !File.IsByteReversed;

        return (Section.ExpandedDataSize == 0 ||
                File.Sections[SectionIndex] != NULL);
    }
    else
    {
        Log1(WarningLogMessage, FileReadingLogMessage,
             "Ignored request to read already populated file section %d",
             SectionIndex);

        // Note: Kind of a toss up what to do here, but we'll return true,
        // since the section is loaded.
        return true;
    }
}

bool GRANNY
ReadFileSection(file_reader* Reader, file &File, int32x SectionIndex)
{
    CheckPointerNotNull(Reader, return false);

    return ReadFileSectionInPlace(Reader, File, SectionIndex, NULL);
}


void GRANNY
FreeFixupArray(void* FixupOrMarshalling)
{
    // Handles NULL ok...
    Deallocate(FixupOrMarshalling);
}

bool GRANNY
LoadMarshallingArray(file_reader* Reader,
                     grn_section const &Section,
                     bool FileIsByteReversed,
                     grn_mixed_marshalling_fixup** Array)
{
    CheckPointerNotNull(Reader, return false);
    CheckPointerNotNull(Array, return false);
    if (Section.MixedMarshallingFixupArrayCount == 0)
    {
        *Array = 0;
        return true;
    }

    grn_mixed_marshalling_fixup*& Fixups = *Array;

    if (Fixups == 0)
    {
        Fixups = AllocateArray(Section.MixedMarshallingFixupArrayCount,
                               grn_mixed_marshalling_fixup, AllocationTemporary);
        *Array = Fixups;
    }

    if (Fixups)
    {
        ReadAt(Section.MixedMarshallingFixupArrayOffset, Reader,
               FileIsByteReversed,
               Section.MixedMarshallingFixupArrayCount *
               SizeOf(grn_mixed_marshalling_fixup),
               Fixups);
    }

    return Fixups != 0;
}

bool GRANNY
LoadFixupArray(file_reader* Reader,
               grn_section const &Section,
               bool FileIsByteReversed,
               grn_pointer_fixup** Array)
{
    CheckPointerNotNull(Reader, return false);
    CheckPointerNotNull(Array, return false);

    if (Section.PointerFixupArrayCount == 0)
    {
        *Array = 0;
        return true;
    }

    grn_pointer_fixup*& Fixups = *Array;
    if (Fixups == 0)
    {
        Fixups = AllocateArray(Section.PointerFixupArrayCount, grn_pointer_fixup, AllocationTemporary);
        *Array = Fixups;
    }

    if (Fixups)
    {
        ReadAt(Section.PointerFixupArrayOffset, Reader,
               FileIsByteReversed,
               Section.PointerFixupArrayCount * SizeOf(grn_pointer_fixup),
               Fixups);
    }

    return Fixups != 0;
}



// A word of explanation here.
// Why do we do fixup in two phases, with pointer fixup
// happening twice? Well, I'll tell you.
// If you have a data_type_definition that refers to another
// data_type_definition (such as an array or an inline member),
// and the structure it lives in is a mixed-marshalling type
// (because it contains int16s and int8s for example),
// then you need to do mixed marshalling on it. This needs to
// walk the type trees of those structures. But when walking
// the type trees, the pointers between the trees need to have
// been fixed up (or the pointers are bogus). So we first
// fix the pointers up, then we do mixed marshalling.
// However, mixed marshalling can then mangle pointers
// that actually live in the data, e.g. if you have a
// structure that is an int and then a pointer, that will
// be marked as int32 marshalling, and the whole 8 bytes
// will be marshalled, destroying the pointer that we already
// fixed up. So the solution is to fix up the pointers
// twice. Ideally, we wouldn't do this, instead we'd
// actually walk the structures and marshall the int32 but not
// the pointer. But that's not the way the code currently works.
// The other problem is that 90% of Granny's structures would
// then be mixed marshalling, which is far far slower than
// just zapping through them all with Reverse32 and
// then fixing the pointers up. So the brute-force
// approach may actually be the quickest one.
//
// Also, we need to do phase 1 (pointer fixup) of all the
// sections before we can do phase 2 (mixed marshalling, then
// pointer fixup), because we might have a granny_variant with a
// type descriptor stored in section 3 that refers to a data
// chunk in section 1.
//
// TODO: fix the marshalling so that it doesn't re-marshall
// pointers, meaning we can remove the second pointer fixup.
// Unless the alternative is slower (see above).

void GRANNY
FixupFileSectionPhase1(file &File, int32x SectionIndex,
                       grn_pointer_fixup const* PointerFixupArray)
{
    CheckPointerNotNull(File.Header, return);
    CheckInt32Index(SectionIndex, File.SectionCount, return);

    grn_section const *SectionArray = GetGRNSectionArray(*File.Header);
    Assert(SectionArray);

    grn_section const &Section = SectionArray[SectionIndex];

    if(Section.PointerFixupArrayCount)
    {
        CheckPointerNotNull(PointerFixupArray, return);

        GRNFixUp(Section.PointerFixupArrayCount,
                 PointerFixupArray, (void const **)File.Sections,
                 File.Sections[SectionIndex]);
    }
}


void GRANNY
FixupFileSectionPhase2(file &File, int32x SectionIndex,
                       grn_pointer_fixup const* PointerFixupArray,
                       grn_mixed_marshalling_fixup const* MarshallFixupArray)
{
    CheckPointerNotNull(File.Header, return);
    CheckInt32Index(SectionIndex, File.SectionCount, return);

    grn_section const *SectionArray = GetGRNSectionArray(*File.Header);
    Assert(SectionArray);

    grn_section const &Section = SectionArray[SectionIndex];

    if(File.IsByteReversed &&
       Section.MixedMarshallingFixupArrayCount &&
       !File.Marshalled[SectionIndex])
    {
        CheckPointerNotNull(MarshallFixupArray, return);

        GRNMarshall(Section.MixedMarshallingFixupArrayCount,
                    MarshallFixupArray, (void const **)File.Sections,
                    File.Sections[SectionIndex]);
        File.Marshalled[SectionIndex] = true;

        // Marshalling may have destroyed the pointers. Do the fixup again.
        if(Section.PointerFixupArrayCount)
        {
            CheckPointerNotNull(PointerFixupArray, return);
            GRNFixUp(Section.PointerFixupArrayCount,
                     PointerFixupArray, (void const **)File.Sections,
                     File.Sections[SectionIndex]);
        }
    }
}

// So here's the deal.  The size of the grn_file_header changes from time to time as
// fields are added onto the end, most recently, the StringDatabaseCRC field.  When that
// happens, we're in a bit of trouble, because the only information we have about the size
// of the header comes in the form of the combined byte count of the (Header +
// SectionArray).  This function detects when an older file's header structure is causing
// the section array to overlap with the end of the /newer/ header structure, and moves it
// out of harm's way.
//
// In order to do this safely, we need to adjust both the location of the section array,
// and also the offset in the grn_file_header.  In order to ensure that we have enough
// space to do the move safely, AdjustHeaderAllocation should increase the allocation from
// HeaderSize to (HeaderSize + Padding), where Padding is basically more than the
// difference in size between version 0 of the header, and the current version.  32 bytes
// (the value as of this writing) is more than enough, but I'm being paranoid.
static bool
AdjustedHeaderRead(int32x Position, file_reader* Reader,
                   bool IsByteReversed,
                   int32x HeaderSize, void* HeaderPtr)
{
    CheckPointerNotNull(Reader, return false);
    CheckCondition(Position >= 0, return false);
    CheckCondition(HeaderSize > 0, return false);
    CheckPointerNotNull(HeaderPtr, return false);

    uint8* HeaderBytes = (uint8*)HeaderPtr;
    if (!ReadAt(SizeOf(grn_file_magic_value), Reader, IsByteReversed, HeaderSize, HeaderBytes))
    {
        Log0(ErrorLogMessage, FileReadingLogMessage, "Unable to read file header");
        return false;
    }

    grn_file_header* Header = (grn_file_header*)HeaderBytes;
    if (Header->SectionArrayOffset < SizeOf(grn_file_header))
    {
        // First, move the section array memory out of the new header hole
        int32x SectionArraySize = HeaderSize - SizeOf(grn_file_magic_value) - Header->SectionArrayOffset;
        CheckCondition((SectionArraySize % SizeOf(grn_section)) == 0, return false);

        uint8* OldPointer  = HeaderBytes + Header->SectionArrayOffset;
        uint8* SafePointer = HeaderBytes + SizeOf(grn_file_header);
        Move(SectionArraySize, OldPointer, SafePointer);

        // Next, zero the memory that overlapped
        SetUInt8(SizeOf(grn_file_header) - Header->SectionArrayOffset, 0, OldPointer);

        // And reset the SectionArrayOffset
        Header->SectionArrayOffset = SizeOf(grn_file_header);
    }

    return true;
}

static int32x
AdjustHeaderAllocation(int32x Unadjusted)
{
    // See note in the AdjustedHeaderRead function for why this is necessary, as well as
    // what to do about this magical number.
    return Unadjusted + 32;
}

static grn_file_header *
ReadFileHeader(file_reader* Reader,
               int32x FilePosition,
               bool &IsByteReversed,
               bool &IsPointerSizeDifferent,
               grn_file_magic_value* FileMagicValue)
{
    grn_file_magic_value DiscardMagic;
    if (FileMagicValue == NULL)
    {
        FileMagicValue = &DiscardMagic;
    }

    if(ReadExactly(Reader, FilePosition, SizeOf(grn_file_magic_value), FileMagicValue))
    {
        FilePosition += SizeOf(grn_file_magic_value);

        uint32 HeaderSize;
        if(IsGrannyFile(*FileMagicValue, &HeaderSize, &IsByteReversed, &IsPointerSizeDifferent))
        {
            NormalizeMagicValue(*FileMagicValue);

            // TODO: Compressed headers
            if(FileMagicValue->HeaderFormat == 0)
            {
                int32x HeaderAllocation = HeaderSize;
                if (SizeOf(grn_file_header) > HeaderSize)
                    HeaderAllocation = SizeOf(grn_file_header);
                HeaderAllocation = AdjustHeaderAllocation(HeaderAllocation);

                grn_file_header *Header = (grn_file_header *)AllocateSize(HeaderAllocation, AllocationFileData);
                if(Header)
                {
                    ZeroStructure(*Header);
                    if (AdjustedHeaderRead(FilePosition, Reader, IsByteReversed, HeaderSize, Header))
                    {
                        FilePosition += HeaderSize;
                        return Header;
                    }
                    else
                    {
                        Log0(ErrorLogMessage, FileReadingLogMessage,
                             "Unable to read file header");
                    }

                    Deallocate(Header);
                }
            }
            else
            {
                Log1(ErrorLogMessage, FileReadingLogMessage,
                     "File has an unrecognized header format (%d)",
                     FileMagicValue->HeaderFormat);
            }
        }
        else
        {
            Log0(ErrorLogMessage, FileReadingLogMessage,
                 "File is not a Granny file");
        }
    }
    else
    {
        Log0(ErrorLogMessage, FileReadingLogMessage, "Unable to read magic value");
    }

    return 0;
}

static bool
FileCRCIsValidFromReader(file_reader* Reader)
{
    bool Result = false;
    CheckPointerNotNull(Reader, return Result);

    bool IsByteReversed = false;
    bool PointerSizeDifferent = false;
    uint32x HeaderSize;

    grn_file_magic_value MagicValue;
    grn_file_header *Header =
        ReadFileHeader(Reader, 0, IsByteReversed, PointerSizeDifferent, &MagicValue);
    if (Header && IsGrannyFile(MagicValue, &HeaderSize, &IsByteReversed, &PointerSizeDifferent))
    {
        int32x FilePosition = HeaderSize;  // Don't use the one from the magic value!  Might be swapped.
        uint32 CRC;

        BeginCRC32(CRC);

        // Explicitly add the section array, since it gets a little wierd when
        // upgrading old files.  Just start with the reading crc in the real data
        if (IsByteReversed)
        {
            Reverse32(Header->SectionArrayCount * SizeOf(grn_section),
                      ((uint8*)Header) + Header->SectionArrayOffset);
        }

        AddToCRC32(CRC, Header->SectionArrayCount * SizeOf(grn_section),
                   ((uint8*)Header) + Header->SectionArrayOffset);

        if (IsByteReversed)
        {
            Reverse32(Header->SectionArrayCount * SizeOf(grn_section),
                      ((uint8*)Header) + Header->SectionArrayOffset);
        }

        uint8 *Buffer = (uint8 *)AllocateSize(CRCCheckBufferSize, AllocationTemporary);
        if (Buffer)
        {
            while (true)
            {
                int32x ReadSize = ReadAtMost(Reader, FilePosition, CRCCheckBufferSize, Buffer);
                if ( ReadSize == 0 )
                    break;

                AddToCRC32(CRC, ReadSize, Buffer);
                FilePosition += ReadSize;
            }
            EndCRC32(CRC);
            Result = (Header->CRC == CRC);
        }

        Deallocate(Buffer);
    }

    Deallocate(Header);

    return Result;
}

bool GRANNY
FileCRCIsValidFromMemory(int32x MemorySize, void* Memory)
{
    bool Result = false;
    CheckPointerNotNull(Memory, return false);
    CheckCondition(MemorySize > 0, return false);

    file_reader *Reader = CreateMemoryFileReader(MemorySize, Memory);
    if(Reader)
    {
        Result = FileCRCIsValidFromReader(Reader);
        CloseFileReader(Reader);
    }
    else
    {
        Log0(ErrorLogMessage, FileReadingLogMessage, "Unable to memory buffer reading");
    }

    return Result;
}

bool GRANNY
FileCRCIsValid(char const *FileName)
{
    bool Result = false;
    CheckPointerNotNull(FileName, return Result);

    file_reader *Reader = OpenFileReader(FileName);
    if(Reader)
    {
        Result = FileCRCIsValidFromReader(Reader);
        CloseFileReader(Reader);
    }
    else
    {
        Log1(ErrorLogMessage, FileReadingLogMessage, "Unable to open \"%s\" for reading", FileName);
    }

    return Result;
}

file *GRANNY
ReadEntireFile(char const *FileName)
{
    GRANNY_AUTO_ZONE_FN();
    GRANNY_TM_LOG("Filename: %s", GRANNY_TM_DYNSTRING(FileName));

    file *Result = 0;
    CheckPointerNotNull(FileName, return Result);

    file_reader *Reader = OpenFileReader(FileName);
    if(Reader)
    {
        Result = ReadEntireFileFromReader(Reader);
        CloseFileReader(Reader);
    }
    else
    {
        Log1(ErrorLogMessage, FileReadingLogMessage, "Unable to open \"%s\" for reading", FileName);
    }

    return Result;
}

file *GRANNY
ReadEntireFileFromMemory(int32x MemorySize, void const* Memory)
{
    GRANNY_AUTO_ZONE_FN();

    file *Result = 0;

    file_reader *Reader = CreateMemoryFileReader(MemorySize, Memory);
    if(Reader)
    {
        Result = ReadEntireFileFromReader(Reader);
        CloseFileReader(Reader);
    }

    return Result;
}

static file *
ReadRestOfFileFromReaderPointerSize(file_reader* Reader,
                                    file* Result,
                                    grn_file_magic_value &FileMV)
{
    CheckPointerNotNull(Reader, return 0);
    GRANNY_AUTO_ZONE_FN();

    grn_section const *SectionArray = GetGRNSectionArray(*Result->Header);
    Assert(SectionArray);

    // Read the sections
    {for(int32x SectionIndex = 0;
         SectionIndex < Result->SectionCount;
         ++SectionIndex)
    {
        Result->Sections[SectionIndex] = 0;
        if (!ReadFileSection(Reader, *Result, SectionIndex))
        {
            FreeFile(Result);
            return 0;
        }

        // Ok, I admit, this is f'ed up.  Since we're going to allow the
        //  pointer conversion to do all of the endian correct, we don't
        //  actually /want/ the sections to be reversed.  Since it's now
        //  in the "correct" format, reverse it again.
        // Todo: this is pretty wacky, can fix?
        if (Result->Sections[SectionIndex] && Result->IsByteReversed)
        {
            grn_section const& Section = SectionArray[SectionIndex];
            ReverseSection(Section.First16Bit, Section.First8Bit,
                           Section.ExpandedDataSize,
                           Result->Sections[SectionIndex]);
        }
    }}

    // Load the fixup arrays and the marshalling
    grn_pointer_fixup **Fixups =
        (grn_pointer_fixup**)AllocateArray(Result->SectionCount, grn_pointer_fixup*,
                                           AllocationTemporary);
    grn_mixed_marshalling_fixup **Marshalls =
        (grn_mixed_marshalling_fixup**)AllocateArray(Result->SectionCount, grn_mixed_marshalling_fixup*,
                                                     AllocationTemporary);
    if (!Fixups || !Marshalls)
    {
        Log0(ErrorLogMessage, FileReadingLogMessage,
             "ReadRestOfFileFromReaderPointerSize: unable to allocate fixup arrays");
        Deallocate(Fixups);
        Deallocate(Marshalls);
        FreeFile(Result);
        return 0;
    }
    SetPtrNULL(Result->SectionCount, Fixups);
    SetPtrNULL(Result->SectionCount, Marshalls);

    bool FixupLoadsSucceeded = true;
    {for(int32x SectionIndex = 0;
         SectionIndex < Result->SectionCount;
         ++SectionIndex)
    {
        grn_section const &Section = SectionArray[SectionIndex];

        if (!LoadFixupArray(Reader, Section,
                            Bool32(Result->IsByteReversed), &Fixups[SectionIndex]))
        {
            FixupLoadsSucceeded = false;
            break;
        }

        if (!LoadMarshallingArray(Reader, Section, Bool32(Result->IsByteReversed), &Marshalls[SectionIndex]))
        {
            FixupLoadsSucceeded = false;
            break;
        }
    }}
    if (!FixupLoadsSucceeded)
    {
        {for(int32x SectionIndex = 0;
             SectionIndex < Result->SectionCount;
             ++SectionIndex)
        {
            Deallocate(Fixups[SectionIndex]);
            Deallocate(Marshalls[SectionIndex]);
        }}

        Deallocate(Fixups);
        Deallocate(Marshalls);
        FreeFile(Result);

        return 0;
    }

    // Convert the file
    int32x SourcePointerSize = 0, DestPointerSize = 0;
    bool   SourceLittleEndian = false, DestLittleEndian = false;
    bool ValidMagic = GetMagicValueProperties(FileMV,
                                              &SourcePointerSize,
                                              &SourceLittleEndian);
    Assert(ValidMagic); // this should never, ever happen
    GetThisPlatformProperties(&DestPointerSize, &DestLittleEndian);

    if (PlatformConversion(SourcePointerSize, SourceLittleEndian,
                           DestPointerSize, DestLittleEndian, false,
                           Result, Fixups, Marshalls))
    {
        // Fix up the section pointers, phase 1
        {for(int32x SectionIndex = 0;
             SectionIndex < Result->SectionCount;
             ++SectionIndex)
        {
            FixupFileSectionPhase1(*Result, SectionIndex, Fixups[SectionIndex]);
        }}

        // Fix up the section pointers, phase 2
        FixupLoadsSucceeded = true;
        if (Result->IsByteReversed)
        {
            {for(int32x SectionIndex = 0; SectionIndex < Result->SectionCount; ++SectionIndex)
            {
                grn_mixed_marshalling_fixup *MixedMarshallingFixupArray = 0;
                if (!LoadMarshallingArray(Reader, SectionArray[SectionIndex],
                                          Bool32(Result->IsByteReversed), &MixedMarshallingFixupArray))
                {
                    Log1(ErrorLogMessage, FileReadingLogMessage,
                         "ReadEntireFile: unable to load marshalling array for section: %d",
                         SectionIndex);
                    return 0;
                }

                FixupFileSectionPhase2(*Result, SectionIndex,
                                       Fixups[SectionIndex],
                                       MixedMarshallingFixupArray);

                Deallocate(MixedMarshallingFixupArray);
            }}
        }
    }
    else
    {
        Log0(ErrorLogMessage, FileReadingLogMessage,
             "ReadRestOfFileFromReaderPointerSize: PlatformConversion failed");
        // Tested below
        FixupLoadsSucceeded = false;
    }


    // Lose the fixups and the marshalls.
    {for(int32x SectionIndex = 0;
         SectionIndex < Result->SectionCount;
         ++SectionIndex)
    {
        Deallocate(Fixups[SectionIndex]);
        Deallocate(Marshalls[SectionIndex]);
    }}
    Deallocate(Fixups);
    Deallocate(Marshalls);

    if (!FixupLoadsSucceeded)
    {
        FreeFile(Result);
        Result = 0;
    }
    return Result;
}


file *GRANNY
ReadEntireFileFromReader(file_reader* Reader)
{
    CheckPointerNotNull(Reader, return 0);

    file *Result = ReadPartialFileFromReader(Reader);
    if(Result)
    {
        // If the file is a different pointer size, then we need to do
        // some compilicated shizzle.  Shunt off to a different
        // function.  Note that since we can now write out files that
        // have the correct byte-order, regardless of source platform,
        // we now issue warnings for files that require marshalling.
        int32x SourcePointerSize = 0;
        bool   SourceLittleEndian = false;
        int32x DestPointerSize  = 0;
        bool   DestLittleEndian = false;
        bool ValidMagic = GetMagicValueProperties(*Result->SourceMagicValue,
                                                  &SourcePointerSize,
                                                  &SourceLittleEndian);
        Assert(ValidMagic);  // this should never, ever happen
        GetThisPlatformProperties(&DestPointerSize, &DestLittleEndian);

        if (SourcePointerSize != DestPointerSize)
        {
            Log2(WarningLogMessage, FileReadingLogMessage,
                 "Converting pointer size from %d to %d.  This is EXTREMELY SLOW!",
                 SourcePointerSize, DestPointerSize);

            return ReadRestOfFileFromReaderPointerSize(Reader, Result, *Result->SourceMagicValue);
        }

        if (SourceLittleEndian != DestLittleEndian)
        {
            Log0(WarningLogMessage, FileReadingLogMessage,
                 "Endian mismatch between current platform and source.  File will be endian marshalled.");
        }


        // Read the sections
        int32x const SectionCount = Result->SectionCount;
        {for(int32x SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
        {
            Result->Sections[SectionIndex] = 0;
            if (!ReadFileSection(Reader, *Result, SectionIndex))
            {
                Log1(ErrorLogMessage, FileReadingLogMessage,
                     "ReadEntireFileFromReader: Failed to load section: %d", SectionIndex);
                FreeFile(Result);
                return 0;
            }
        }}

        // Load the fixup arrays
        grn_pointer_fixup **Fixups =
            (grn_pointer_fixup**)AllocateArray(SectionCount, grn_pointer_fixup*, AllocationTemporary);
        if (Fixups == NULL)
        {
            FreeFile(Result);
            return 0;
        }
        SetPtrNULL(SectionCount, Fixups);

        grn_section const *SectionArray = GetGRNSectionArray(*Result->Header);
        Assert(SectionArray);

        bool FixupsLoaded = true;
        {for(int32x SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
        {
            grn_section const &Section = SectionArray[SectionIndex];
            if (!LoadFixupArray(Reader, Section, Bool32(Result->IsByteReversed),
                                &Fixups[SectionIndex]))
            {
                FixupsLoaded = false;
                break;
            }
        }}

        if (FixupsLoaded == true)
        {
            // Fix up the section pointers, phase 1
            {for(int32x SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
            {
                FixupFileSectionPhase1(*Result, SectionIndex, Fixups[SectionIndex]);
            }}

            // Fix up the section pointers, phase 2
            if (Result->IsByteReversed)
            {
                {for(int32x SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
                {
                    grn_mixed_marshalling_fixup *MixedMarshallingFixupArray = 0;
                    if (!LoadMarshallingArray(Reader, SectionArray[SectionIndex],
                                              Bool32(Result->IsByteReversed), &MixedMarshallingFixupArray))
                    {
                        Log1(ErrorLogMessage, FileReadingLogMessage,
                             "ReadEntireFile: unable to load marshalling array for section: %d",
                             SectionIndex);
                        return 0;
                    }

                    FixupFileSectionPhase2(*Result, SectionIndex,
                                           Fixups[SectionIndex],
                                           MixedMarshallingFixupArray);
                    Deallocate(MixedMarshallingFixupArray);
                }}
            }
        }
        else
        {
            // We failed to properly load a set of pointer fixups.
            // It's necessary to bail at this point.  Note that the fixup freeing below /must/
            // happen though, so just null the result
            FreeFile(Result);
            Result = 0;
        }

        // Lose the fixups
        {for(int32x SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
        {
            Deallocate(Fixups[SectionIndex]);
        }}
        Deallocate(Fixups);
    }

    return Result;
}

file *GRANNY
ReadPartialFileFromReader(file_reader* Reader)
{
    CheckPointerNotNull(Reader, return 0);

    file *Result = 0;

    bool IsByteReversed, IsPointerSizeDifferent;
    grn_file_magic_value FileMagicValue;
    grn_file_header *Header =
        ReadFileHeader(Reader, 0, IsByteReversed,
                       IsPointerSizeDifferent, &FileMagicValue);
    if(Header)
    {
        if(Header->Version != CurrentGRNFileVersion)
        {
            Log2(WarningLogMessage, FileReadingLogMessage,
                 "File is file format revision %d "
                 "(current version is %d)",
                 Header->Version, CurrentGRNFileVersion);
        }

        aggr_allocator Allocator;
        InitializeAggrAlloc(Allocator);

        AggrAllocPtr(Allocator, Result);
        AggrAllocOffsetPtr(Allocator, Result, SourceMagicValue);
        AggrAllocOffsetArrayPtr(Allocator, Result, Header->SectionArrayCount,
                                SectionCount, Sections);
        AggrAllocOffsetArrayPtr(Allocator, Result, Header->SectionArrayCount,
                                SectionCount, Marshalled);
        AggrAllocOffsetArrayPtr(Allocator, Result, Header->SectionArrayCount,
                                SectionCount, IsUserMemory);
        if(EndAggrAlloc(Allocator, AllocationFileData))
        {
            Result->IsByteReversed = IsByteReversed;
            Result->Header = Header;
            *Result->SourceMagicValue = FileMagicValue;
            Result->ConversionBuffer = 0;

            ZeroArray(Result->SectionCount, Result->Sections);
            ZeroArray(Result->SectionCount, Result->Marshalled);
            ZeroArray(Result->SectionCount, Result->IsUserMemory);
        }
        else
        {
            Deallocate(Header);
            Header = 0;
        }
    }

    return Result;
}

bool GRANNY
PrepReadEntireFileNoAlloc(file_reader* Reader, noalloc_loading_mem& Holder)
{
    grn_file_magic_value FileMagicValue;
    if (!ReadExactly(Reader, 0, SizeOf(FileMagicValue), &FileMagicValue))
    {
        Log0(ErrorLogMessage, FileReadingLogMessage, "Unable to read magic value");
        return false;
    }

    uint32 HeaderSize;
    bool IsByteReversed;
    bool IsPointerSizeDifferent;
    if (!IsGrannyFile(FileMagicValue, &HeaderSize, &IsByteReversed, &IsPointerSizeDifferent))
    {
        Log0(ErrorLogMessage, FileReadingLogMessage, "Magic value not recognized");
        return false;
    }

    // Make sensible
    NormalizeMagicValue(FileMagicValue);

    // TODO: Compressed headers
    if (FileMagicValue.HeaderFormat != 0)
    {
        Log0(ErrorLogMessage, FileReadingLogMessage, "Header format either too new, or corrupted.");
        return false;
    }

    uint8 HeaderBuffer[1024];
    grn_file_header* Header = (grn_file_header*)HeaderBuffer;
    if (HeaderSize > SizeOf(HeaderBuffer))
    {
        Log0(ErrorLogMessage, FileReadingLogMessage, "Header size exceeds stack allocation");
        return false;
    }

    if (!ReadAt(SizeOf(FileMagicValue), Reader, IsByteReversed, HeaderSize, Header))
    {
        Log0(ErrorLogMessage, FileReadingLogMessage, "Unable to read file header");
        return false;
    }

    grn_section* Sections = (grn_section*)&HeaderBuffer[Header->SectionArrayOffset];
    int32x SectionCount = Header->SectionArrayCount;

    if (SectionCount <= 0 || SectionCount > MaximumSectionCount)
    {
        Log0(ErrorLogMessage, FileReadingLogMessage, "Invalid section count");
        return false;
    }

    // Ok, let's compute the memory required to read this in
    ZeroStructure(Holder);

    // First, compute the fixed memory.  This includes the granny_file, magic value, and
    // the file header.
    {
        // File object
        Holder.HeaderByteCount += SizeOf(file);
        Holder.HeaderByteCount  = (int32x)Align32(Holder.HeaderByteCount);

        Holder.HeaderByteCount += SizeOf(void*) * SectionCount;
        Holder.HeaderByteCount += SizeOf(bool)  * SectionCount;
        Holder.HeaderByteCount += SizeOf(bool)  * SectionCount;
        Holder.HeaderByteCount  = (int32x)Align32(Holder.HeaderByteCount);

        // Magic number
        Holder.HeaderByteCount += SizeOf(grn_file_magic_value);
        Holder.HeaderByteCount  = (int32x)Align32(Holder.HeaderByteCount);

        // Header
        Holder.HeaderByteCount += HeaderSize;
        Holder.HeaderByteCount  = (int32x)Align32(Holder.HeaderByteCount);

        // Alignment provision
        Holder.HeaderByteCount += 4;
    }


    // Next, fill in the section requirements
    Holder.SectionCount = SectionCount;
    {for (int32x SecIdx = 0; SecIdx < SectionCount; ++SecIdx)
    {
        if (Sections[SecIdx].ExpandedDataSize != 0)
        {
            Holder.SectionSizes[SecIdx] = (Sections[SecIdx].ExpandedDataSize +
                                           Sections[SecIdx].InternalAlignment);
        }

        int32x FixupArraySize = (Sections[SecIdx].PointerFixupArrayCount *
                                 SizeOf(grn_pointer_fixup));
        if (IsByteReversed)
        {
            FixupArraySize += (Sections[SecIdx].MixedMarshallingFixupArrayCount *
                               SizeOf(grn_mixed_marshalling_fixup));
        }

        if (FixupArraySize > Holder.TempByteCount)
            Holder.TempByteCount = FixupArraySize;
    }}

    return true;
}

file* GRANNY
ReadEntireFileNoAlloc(file_reader* Reader, noalloc_loading_mem& Holder)
{
    CheckPointerNotNull(Reader, return 0);
    CheckPointerNotNull(Holder.HeaderBytes, return 0);

    file* Result = 0;
    bool IsByteReversed;
    bool IsPointerSizeDifferent;
    {
        grn_file_magic_value* FileMV;
        uint8* HeaderBytes;
        {
            uintaddrx Position = 0;
            uint8*   Pointer  = (uint8*)Align32((uintaddrx)Holder.HeaderBytes);

            FileMV = (grn_file_magic_value*)(Pointer + Position);
            Position += SizeOf(grn_file_magic_value);
            Position  = Align32(Position);

            Result = (file*)(Pointer + Position);
            Position += SizeOf(file);
            Position  = Align32(Position);

            // Setup the sub pointers
            {
                Result->SectionCount = Holder.SectionCount;
                Result->Sections = (void**)(Pointer + Position);
                Position += SizeOf(void*) * Holder.SectionCount;

                Result->Marshalled = (bool*)(Pointer + Position);
                Position += SizeOf(bool) * Holder.SectionCount;
                SetUInt8(SizeOf(bool) * Holder.SectionCount, 0, Result->Marshalled);

                Result->IsUserMemory = (bool*)(Pointer + Position);
                Position += SizeOf(bool) * Holder.SectionCount;
                SetUInt8(SizeOf(bool) * Holder.SectionCount, 0, Result->IsUserMemory);
            }
            Position  = Align32(Position);

            Result->ConversionBuffer = 0;

            HeaderBytes = (Pointer + Position);
        }

        if (!ReadExactly(Reader, 0, SizeOf(grn_file_magic_value), FileMV))
        {
            Log0(ErrorLogMessage, FileReadingLogMessage, "Unable to read magic value");
            return 0;
        }

        uint32 HeaderSize;
        if (!IsGrannyFile(*FileMV, &HeaderSize, &IsByteReversed, &IsPointerSizeDifferent))
        {
            Log0(ErrorLogMessage, FileReadingLogMessage, "Magic value not recognized");
            return 0;
        }

        // Make sensible
        NormalizeMagicValue(*FileMV);

        if (FileMV->HeaderFormat != 0)
        {
            Log0(ErrorLogMessage, FileReadingLogMessage, "Header format either too new, or corrupted.");
            return 0;
        }

        if (!AdjustedHeaderRead(SizeOf(grn_file_magic_value), Reader, IsByteReversed, HeaderSize, HeaderBytes))
        {
            Log0(ErrorLogMessage, FileReadingLogMessage, "Unable to read file header");
            return 0;
        }

        // Fill in the rest of the structure
        Result->IsByteReversed   = IsByteReversed;
        Result->Header           = (grn_file_header*)HeaderBytes;
        Result->SourceMagicValue = FileMV;
    }
    Assert(Result);

    if (IsPointerSizeDifferent)
    {
        Log0(ErrorLogMessage, FileReadingLogMessage,
             "Converting pointer size.  This not supported in the NoAlloc read path");
        return 0;
    }

    if (IsByteReversed)
    {
        Log0(WarningLogMessage, FileReadingLogMessage,
             "Endian mismatch between current platform and source.  File will be endian marshalled.");
    }

    grn_section const *SectionArray = GetGRNSectionArray(*Result->Header);
    Assert(SectionArray);

    // Read the sections
    int32x const SectionCount = Result->SectionCount;
    {for(int32x SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
    {
        Result->Sections[SectionIndex] = 0;

        void* AlignedPointer = AlignPtrN(Holder.SectionPointers[SectionIndex],
                                         SectionArray[SectionIndex].InternalAlignment);

        if (!ReadFileSectionInPlace(Reader, *Result, SectionIndex, AlignedPointer))
        {
            Log1(ErrorLogMessage, FileReadingLogMessage,
                 "ReadEntireFileFromReaderNoAlloc: Failed to load section: %d", SectionIndex);
            return 0;
        }
    }}

    // Do the fixups
    {for(int32x SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
    {
        grn_section const &Section = SectionArray[SectionIndex];

        grn_pointer_fixup* Fixups = (grn_pointer_fixup*)Holder.TempBytes;

        if (!LoadFixupArray(Reader, Section, Bool32(Result->IsByteReversed), &Fixups))
        {
            Log1(ErrorLogMessage, FileReadingLogMessage,
                 "Unable to load fixup array for section: %d", SectionIndex);
            return 0;
        }

        FixupFileSectionPhase1(*Result, SectionIndex, Fixups);
    }}

    if (Result->IsByteReversed)
    {
        {for(int32x SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
        {
            grn_section const &Section = SectionArray[SectionIndex];

            grn_pointer_fixup* Fixups = (grn_pointer_fixup*)Holder.TempBytes;
            grn_mixed_marshalling_fixup* Marshals =
                (grn_mixed_marshalling_fixup*)(Fixups + Section.PointerFixupArrayCount);

            if (!LoadFixupArray(Reader, Section, Bool32(Result->IsByteReversed), &Fixups))
            {
                Log1(ErrorLogMessage, FileReadingLogMessage,
                     "Unable to load fixup array for section: %d", SectionIndex);
                return 0;
            }

            if (!LoadMarshallingArray(Reader, Section, Bool32(Result->IsByteReversed), &Marshals))
            {
                Log1(ErrorLogMessage, FileReadingLogMessage,
                     "Unable to load marshalling array for section: %d", SectionIndex);
                return 0;
            }

            FixupFileSectionPhase2(*Result, SectionIndex, Fixups, Marshals);
        }}
    }

    return Result;
}


void GRANNY
FreeFileSection(file &File, int32x SectionIndex)
{
    CheckInt32Index(SectionIndex, File.SectionCount, return);

    if(File.Sections[SectionIndex])
    {
        if ( !File.IsUserMemory[SectionIndex] )
        {
            Deallocate(File.Sections[SectionIndex]);
        }
        File.IsUserMemory[SectionIndex] = false;
        File.Marshalled[SectionIndex] = false;
        File.Sections[SectionIndex] = 0;
    }
}



void GRANNY
FreeAllFileSections(file &File)
{
    {for(int32x SectionIndex = 0;
         SectionIndex < File.SectionCount;
         ++SectionIndex)
    {
        FreeFileSection(File, SectionIndex);
    }}
}

void GRANNY
FreeFile(file *File)
{
    if(File)
    {
        FreeAllFileSections(*File);
        Deallocate(File->Header);
        Deallocate(File->ConversionBuffer);
        Deallocate(File);
    }
}


int32x GRANNY
GetFileSectionOfLoadedObject(file const &File,
                             void const *Object)
{
    grn_section *SectionArray = GetGRNSectionArray(*File.Header);
    {for ( uint32x SectionNum = 0; SectionNum < File.Header->SectionArrayCount; SectionNum++ )
    {
        char *SectionStart = (char*)File.Sections[SectionNum];
        char *SectionEnd = SectionStart + SectionArray[SectionNum].ExpandedDataSize;
        if ( ( (char*)Object >= SectionStart ) && ( (char*)Object < SectionEnd ) )
        {
            return SectionNum;
        }
    }}
    // Unknown!
    return -1;
}


void GRANNY
GetDataTreeFromFile(file const &File,
                    variant* Result)
{
    CheckPointerNotNull(Result, return);

    Result->Type = (data_type_definition *)DecodeGRNReference(
        (void const **)File.Sections, File.Header->RootObjectTypeDefinition);
    Result->Object = DecodeGRNReference(
        (void const **)File.Sections, File.Header->RootObject);
}

uint32 GRANNY
GetFileTypeTag(file const &File)
{
    CheckPointerNotNull(File.Header, return 0);
    return File.Header->TypeTag;
}

static file_location
FileLocationFrom(grn_section const *SectionArray, int32x SectionIndex,
                 uint32 Offset)
{
    grn_section const &Section = SectionArray[SectionIndex];

    marshalling_type Marshalling;
    if(Offset < Section.First16Bit)
    {
        Marshalling = Int32Marshalling;
    }
    else if(Offset < Section.First8Bit)
    {
        Marshalling = Int16Marshalling;
        Offset -= Section.First16Bit;
    }
    else
    {
        Marshalling = Int8Marshalling;
        Offset -= Section.First8Bit;
    }

    file_location Location;
    Location.SectionIndex = SectionIndex;
    Location.BufferIndex = GetBufferIndexFor(Marshalling);
    Location.Offset = Offset;

    return Location;
}

bool GRANNY
RecompressFile(char const *SourceFile, char const *DestFile,
               int32x CompressorMappingCount, int32x const *CompressorMapping)
{
    bool Result = false;

    file_reader *Reader = OpenFileReader(SourceFile);
    if(Reader)
    {
        bool IsByteReversed;
        bool PointerSizeDifferent;
        grn_file_magic_value MagicValue;
        grn_file_header *Header =
            ReadFileHeader(Reader, 0, IsByteReversed, PointerSizeDifferent, &MagicValue);
        if(Header)
        {
            if(Header->Version != CurrentGRNFileVersion)
            {
                Log2(WarningLogMessage, FileReadingLogMessage,
                     "File is file format revision %d (current version is %d)",
                     Header->Version, CurrentGRNFileVersion);
            }

            if (!IsByteReversed && !PointerSizeDifferent)
            {
                int32x SectionCount = Header->SectionArrayCount;

                file_builder *Builder = BeginFile(SectionCount, Header->TypeTag,
                                                  MagicValue,
                                                  GetTemporaryDirectory(),
                                                  "granny_recompress");
                Assert(Builder);

                {for(int32x TagIndex = 0; TagIndex < GRNExtraTagCount; ++TagIndex)
                {
                    SetFileExtraTag(*Builder, TagIndex,
                                    Header->ExtraTags[TagIndex]);
                }}

                SetFileStringDatabaseCRC(*Builder, Header->StringDatabaseCRC);

                grn_section const *SectionArray = GetGRNSectionArray(*Header);
                Assert(SectionArray);

                {for(int32x SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
                {
                    grn_section const &Section = SectionArray[SectionIndex];

                    int32x CompressionType = Section.Format;
                    if(CompressionType < CompressorMappingCount)
                    {
                        CompressionType = CompressorMapping[Section.Format];
                    }
                    SetFileSectionFormat(*Builder, SectionIndex,
                                         (compression_type)CompressionType,
                                         Section.InternalAlignment);

                    uint8 *SectionData =
                        (uint8 *)LoadFileSection(Reader, Section, IsByteReversed, NULL);

                    WriteFileChunk(*Builder, SectionIndex,
                                   Int32Marshalling, Section.First16Bit,
                                   SectionData, NULL);
                    WriteFileChunk(*Builder, SectionIndex,
                                   Int16Marshalling, Section.First8Bit - Section.First16Bit,
                                   SectionData + Section.First16Bit, NULL);
                    WriteFileChunk(*Builder, SectionIndex,
                                   Int8Marshalling, Section.ExpandedDataSize - Section.First8Bit,
                                   SectionData + Section.First8Bit, NULL);

                    grn_pointer_fixup *FixupArray;
                    if (!LoadFixupArray(Reader, Section, IsByteReversed, &FixupArray))
                    {
                        // TODO: handle correctly
                        InvalidCodePath("Out of memory condition in RecompressFile?");
                        return false;
                    }

                    Assert(FixupArray || Section.PointerFixupArrayCount == 0);
                    {for(uint32x FixupIndex = 0;
                         FixupIndex < Section.PointerFixupArrayCount;
                         ++FixupIndex)
                    {
                        grn_pointer_fixup &Fixup = FixupArray[FixupIndex];
                        MarkFileFixup(*Builder,
                                      FileLocationFrom(SectionArray,
                                                       SectionIndex,
                                                       Fixup.FromOffset), 0,
                                      FileLocationFrom(SectionArray,
                                                       Fixup.To.SectionIndex,
                                                       Fixup.To.Offset));
                    }}
                    Deallocate(FixupArray);

                    grn_mixed_marshalling_fixup *MarshallingArray;
                    if (!LoadMarshallingArray(Reader, Section, IsByteReversed, &MarshallingArray))
                    {
                        // TODO: handle correctly
                        InvalidCodePath("Out of memory condition in RecompressFile?");
                        return false;
                    }

                    Assert(MarshallingArray || Section.MixedMarshallingFixupArrayCount == 0);
                    {for(uint32x FixupIndex = 0;
                         FixupIndex < Section.MixedMarshallingFixupArrayCount;
                         ++FixupIndex)
                    {
                        grn_mixed_marshalling_fixup &Fixup = MarshallingArray[FixupIndex];

                        MarkMarshallingFixup(
                            *Builder,
                            FileLocationFrom(SectionArray, Fixup.Type.SectionIndex, Fixup.Type.Offset),
                            FileLocationFrom(SectionArray, SectionIndex, Fixup.Offset),
                            Fixup.Count);
                    }}
                    Deallocate(MarshallingArray);
                    Deallocate(SectionData);
                }}

                // Fixup the root object and type definitions.  This is somewhat wierd, since
                // we have to backport the flattened reference into the separated buffers, but
                // that's ok.
                {
                    file_location RootObject = {
                        Header->RootObject.SectionIndex, 0,
                        Header->RootObject.Offset
                    };
                    file_location RootType = {
                        Header->RootObjectTypeDefinition.SectionIndex, 0,
                        Header->RootObjectTypeDefinition.Offset
                    };
                    Assert(int32x(RootObject.SectionIndex) < SectionCount);
                    Assert(int32x(RootType.SectionIndex) < SectionCount);

                    section* Sections = Builder->Sections;
                    while (int32x(RootObject.Offset) >=
                           Sections[RootObject.SectionIndex].BufferSize[RootObject.BufferIndex])
                    {
                        RootObject.Offset -=
                            Sections[RootObject.SectionIndex].BufferSize[RootObject.BufferIndex];
                        ++RootObject.BufferIndex;
                        Assert(RootObject.BufferIndex < SectionBufferCount);
                    }

                    while (int32x(RootType.Offset) >=
                           Sections[RootType.SectionIndex].BufferSize[RootType.BufferIndex])
                    {
                        RootType.Offset -=
                            Sections[RootType.SectionIndex].BufferSize[RootType.BufferIndex];
                        ++RootType.BufferIndex;
                        Assert(RootType.BufferIndex < SectionBufferCount);
                    }

                    Builder->RootObject = RootObject;
                    Builder->RootObjectTypeDefinition = RootType;
                }

                // Just in case the filenames for the input and the output are the same, close
                // the file reader before we execute the EndFile.
                CloseFileReader(Reader);
                Result = EndFile(Builder, DestFile);
            }
            else
            {
                Log0(ErrorLogMessage, FileReadingLogMessage,
                     "RecompressFile: only supports recompressing native file types.  "
                     "Must rewrite file using the normal file_builder interface.");
            }

            Deallocate(Header);
        }
        else
        {
            CloseFileReader(Reader);
        }
    }

    return Result;
}

bool GRANNY
ConvertFileInfoToRaw(file_info* SourceFileInfo,
                     char const *DestFileName)
{
    bool Result = false;

    if(SourceFileInfo)
    {
        int32x FileSectionCount = 1;
        int32x DefaultSectionIndex = 0;

        file_writer *Writer = NewFileWriter(DestFileName, true);
        if(Writer)
        {
            file_builder *Builder =
                BeginFile(FileSectionCount, 0,
                          GRNFileMV_ThisPlatform,
                          GetTemporaryDirectory(),
                          "raw_writing_buffer");

            file_data_tree_writer *DataTreeWriter =
                BeginFileDataTreeWriting(FileInfoType,
                                         SourceFileInfo,
                                         DefaultSectionIndex,
                                         DefaultSectionIndex);
            if(DataTreeWriter)
            {
                SetFileDataTreeFlags(*DataTreeWriter, ExcludeTypeTree);

                Result = true;
                if(!WriteDataTreeToFileBuilder(*DataTreeWriter, *Builder))
                {
                    Result = false;
                }

                if(!EndFileRawToWriter(Builder, Writer))
                {
                    Result = false;
                }

                EndFileDataTreeWriting(DataTreeWriter);
            }
            else
            {
                AbortFile(Builder);
            }

            CloseFileWriter(Writer);
        }
    }

    return Result;
}

bool GRANNY
ConvertFileToRaw(char const *SourceFileName, char const *DestFileName)
{
    bool Result = false;

    file *SourceFile = ReadEntireFile(SourceFileName);
    if(SourceFile)
    {
        // Ask Granny to parse the source file
        file_info *SourceFileInfo = GetFileInfo(*SourceFile);
        Result = ConvertFileInfoToRaw(SourceFileInfo, DestFileName);
        FreeFile(SourceFile);
    }
    else
    {
        Log1(WarningLogMessage, FileReadingLogMessage,
             "Unable to read \"%s\" for raw conversion.", SourceFileName);
    }

    return Result;
}


static void
FreeCompressedSections(file *File,
                       uint8 ***CompressedSections)
{
    {for(int32x SectionIndex = 0;
         SectionIndex < File->SectionCount;
         ++SectionIndex)
    {
        if ((*CompressedSections)[SectionIndex] != 0)
        {
            FreeMemoryWriterBuffer((*CompressedSections)[SectionIndex]);
            (*CompressedSections)[SectionIndex] = 0;
        }
    }}
}


static bool
CreateCompressedSections(file* File,
                         uint8*** CompressedSections,
                         int32x DiskAlignment)
{
    grn_section *SectionArray = GetGRNSectionArray(*File->Header);
    Assert(SectionArray);

    void* AlignmentArray = AllocateSize(DiskAlignment, AllocationTemporary);
    if (AlignmentArray == 0)
        return false;
    SetUInt8(DiskAlignment, 0, AlignmentArray);

    bool Result = true;
    {for(int32x SectionIndex = 0; SectionIndex < File->SectionCount; ++SectionIndex)
    {
        grn_section &Section = SectionArray[SectionIndex];
        if (Section.ExpandedDataSize == 0)
        {
            (*CompressedSections)[SectionIndex] = NULL;
            Section.DataSize = 0;
            continue;
        }

        file_writer *Writer = CreateMemoryFileWriter(FileCopyBufferSize);
        if (Writer == 0)
        {
            Result = false;
            break;
        }
        else
        {
            int32x AligningSize = (int32x)AlignN(Section.ExpandedDataSize, DiskAlignment);
            file_compressor *Compressor =
                BeginFileCompression(AligningSize, SectionBufferCount,
                                     (compression_type)Section.Format, Bool32(File->IsByteReversed),
                                     Writer);
            if(Compressor)
            {
                // Simulate having broken out 32/16/8 buffers
                uint8* SubSectionBuffer[SectionBufferCount];
                int32x SubSectionBufferSize[SectionBufferCount];
                {
                    SubSectionBuffer[BufferIndex32]     = (uint8*)File->Sections[SectionIndex];
                    SubSectionBufferSize[BufferIndex32] = Section.First16Bit;

                    SubSectionBuffer[BufferIndex16]     = ((uint8*)File->Sections[SectionIndex] + Section.First16Bit);
                    SubSectionBufferSize[BufferIndex16] = (Section.First8Bit - Section.First16Bit);

                    SubSectionBuffer[BufferIndex8]      = ((uint8*)File->Sections[SectionIndex] + Section.First8Bit);
                    SubSectionBufferSize[BufferIndex8]  = (Section.ExpandedDataSize - Section.First8Bit);
                }

                // You might be asking: "Why exactly is this so convoluted?"  I'm glad you
                // asked!  The problem here is that we need to pad the last section to
                // ensure that our ExpandedDataSize is aligned, but we can't just overread
                // the Section buffer, since it might not be big enough.  We also can't
                // just add bytes to the end with CompressContentsOfMemory, because that
                // f's with the stops in the compressor.  So we do the first two buffers
                // as normal, and then use a catenating_reader to put together the
                // contents of the unmarshalled buffer with the alignment pad.
                Result = Result && CompressContentsOfMemory(*Compressor, SubSectionBufferSize[0], SubSectionBuffer[0]);
                Result = Result && CompressContentsOfMemory(*Compressor, SubSectionBufferSize[1], SubSectionBuffer[1]);

                if (Result)
                {
                    if (Section.ExpandedDataSize == AlignN(Section.ExpandedDataSize, DiskAlignment))
                    {
                        // Let's save ourselves some hassle here, it's already aligned.
                        Result = Result && CompressContentsOfMemory(*Compressor, SubSectionBufferSize[2], SubSectionBuffer[2]);
                    }
                    else
                    {
                        // Nope.  We're going to have to write something here.  Note that
                        // it's entirely possible that SectionBufferSize[2] is 0.  Doesn't
                        // matter.
                        int32x PadSize      = AligningSize - Section.ExpandedDataSize;
                        Assert(PadSize > 0 && PadSize < DiskAlignment);  // 0 handled above

                        file_reader* SectionReader = CreateMemoryFileReader(SubSectionBufferSize[2], SubSectionBuffer[2]);
                        file_reader* AlignReader   = CreateMemoryFileReader(PadSize, AlignmentArray);
                        if (SectionReader && AlignReader)
                        {
                            // TODO: should we just have an aligning memory reader?  Would
                            // solve some of these problems...
                            file_reader* ReaderArray[2] = { SectionReader, AlignReader };
                            file_reader* Catenated = CreateCatenatingReader(2, ReaderArray, 1);

                            Result = Result && (Catenated != 0);
                            Result = Result && CompressContentsOfReader(*Compressor, SubSectionBufferSize[2] + PadSize, *Catenated);

                            CloseFileReader(Catenated);
                        }
                        else
                        {
                            // Fail!
                            Result = false;
                        }

                        CloseFileReader(SectionReader);
                        CloseFileReader(AlignReader);
                    }
                }

                // {for(uint32 BufferIndex = 0; (BufferIndex < (uint32)SectionBufferCount) && Result; ++BufferIndex)
                // {
                //     uint8 const* Buffer     = SubSectionBuffer[BufferIndex];
                //     int32x const BufferSize = SubSectionBufferSize[BufferIndex];

                //     Result = Result && CompressContentsOfMemory(*Compressor, BufferSize, Buffer);
                // }}

                // Not a typo that Result is after.  We need to
                // delete the compressor even if we've failed.
                // EndFileCompression does it...
                Result =
                    EndFileCompression(Compressor, Section.DataSize) &&
                    Result;
            }

            // Steal the compressed buffer from the writer.  Note that we snatch
            //  the size at the same time...
            int32x CheckSize = -1;
            Result = Result &&
                StealMemoryWriterBuffer(Writer,
                                        &((*CompressedSections)[SectionIndex]),
                                        &CheckSize);
            Assert((*CompressedSections)[SectionIndex] != 0);
            Assert(CheckSize == (int32x)Section.DataSize);

            CloseFileWriter(Writer);
        }
    }}

    Deallocate(AlignmentArray);

    return Result;
}


bool GRANNY
PlatformConvertReaderToWriterAligned(file_reader*                SourceReader,
                                     file_writer*                DestWriter,
                                     grn_file_magic_value const& DestMagicValue,
                                     bool                        ExcludeTypeTree,
                                     int32x                      DiskAlignment)
{
    CheckPointerNotNull(SourceReader, return false);
    CheckPointerNotNull(DestWriter,   return false);
    CheckCondition(DiskAlignment >= 4, return false);
    CheckCondition((DiskAlignment & (DiskAlignment-1)) == 0, return false);

    file *File = ReadPartialFileFromReader(SourceReader);
    if (!File)
    {
        Log0(ErrorLogMessage, FileReadingLogMessage,
             "PlatformConvertReaderToWriter: ReadPartialFileFromReader failed");
        return false;
    }

    // Log a warning if this is a useless conversion
    if (DoesMagicValueMatch(*File->SourceMagicValue, DestMagicValue, 0))
    {
        Log0(WarningLogMessage, FileReadingLogMessage,
             "PlatformConvertReaderToWriter called when Source and Dest Magic values match.");
    }

    int32x SourcePointerSize, DestPointerSize;
    bool   SourceLittleEndian, DestLittleEndian;
    if (!GetMagicValueProperties(*File->SourceMagicValue,
                                 &SourcePointerSize,
                                 &SourceLittleEndian) ||
        !GetMagicValueProperties(DestMagicValue,
                                 &DestPointerSize,
                                 &DestLittleEndian))
    {
        Log0(ErrorLogMessage, FileReadingLogMessage,
             "PlatformConvertReaderToWriter: failed to obtain good platform information");
        return false;
    }

    // Do we need to reverse values on persistence?
    bool const NeedsReversalOnPersist = (DestLittleEndian != (bool)(PROCESSOR_LITTLE_ENDIAN));

    // Read the sections
    SetPtrNULL(File->SectionCount, File->Sections);
    {for(int32x SectionIndex = 0;
         SectionIndex < File->SectionCount;
         ++SectionIndex)
    {
        if (!ReadFileSection(SourceReader, *File, SectionIndex))
        {
            FreeFile(File);
            return false;
        }
    }}

    grn_section *SectionArray = GetGRNSectionArray(*File->Header);
    Assert(SectionArray);

    // Load the fixup and marshalling arrays
    grn_pointer_fixup **Fixups =
        (grn_pointer_fixup**)AllocateArray(File->SectionCount, grn_pointer_fixup*, AllocationTemporary);
    grn_mixed_marshalling_fixup **Marshalls =
        (grn_mixed_marshalling_fixup**)AllocateArray(File->SectionCount, grn_mixed_marshalling_fixup*, AllocationTemporary);
    if (!Fixups || !Marshalls)
    {
        Deallocate(Fixups);
        Deallocate(Marshalls);
        FreeFile(File);
        return 0;
    }
    SetPtrNULL(File->SectionCount, Fixups);
    SetPtrNULL(File->SectionCount, Marshalls);

    bool FixupsLoaded = true;
    {for(int32x SectionIndex = 0;
         SectionIndex < File->SectionCount;
         ++SectionIndex)
    {
        grn_section const &Section = SectionArray[SectionIndex];

        if (!LoadFixupArray(SourceReader, Section, Bool32(File->IsByteReversed),
                            &Fixups[SectionIndex]))
        {
            FixupsLoaded = false;
            break;
        }

        if (!LoadMarshallingArray(SourceReader, Section, Bool32(File->IsByteReversed),
                                  &Marshalls[SectionIndex]))
        {
            FixupsLoaded = false;
            break;
        }
    }}


    bool Result = false;
    if (FixupsLoaded)
    {
        // Perform the conversion
        Result = PlatformConversion(SourcePointerSize, SourceLittleEndian,
                                    DestPointerSize, DestLittleEndian, ExcludeTypeTree,
                                    File, Fixups, Marshalls);
        if (Result)
        {
            // This is going to look pretty familiar.  We can't use the
            // WriteFile function of the granny_file_builder, since we
            // actually /have/ a file here, rather than a file_builder,
            // but it's the same drill.  Note that we have to do this
            // crappy thing of precompressing the sections rather than
            // compressing them in place since we need to write the bits
            // of the file from the start of the sectionarray to the end
            // of the file linearly for the CRC check.

            // Note that this also fills in the datasize member of the section
            uint8 **CompressedData = AllocateArray(File->SectionCount, uint8*, AllocationTemporary);
            Result = CreateCompressedSections(File, &CompressedData, DiskAlignment);

            // Compute some basic offsets...
            uint32 const TopOfFile = (uint32)GetWriterPosition(DestWriter);

            uint32x FixedHeaderSize  = (SizeOf(grn_file_magic_value) + SizeOf(grn_file_header));
            uint32x SectionArraySize = File->SectionCount * SizeOf(grn_section);
            uint32x FullHeaderSize   = FixedHeaderSize + SectionArraySize;

            // Set the offsets in the sections
            {
                uintaddrx CurrentOffset = AlignN(FullHeaderSize, DiskAlignment);

                // All of the section data is contiguous
                {for(int32x SectionIndex = 0; SectionIndex < File->SectionCount; SectionIndex++)
                {
                    grn_section &Section = SectionArray[SectionIndex];

                    Section.DataOffset = (uint32x)CurrentOffset;
                    CurrentOffset = AlignN(CurrentOffset + Section.DataSize, DiskAlignment);
                }}

                // Follow up with pointer and marshall fixups
                {for(int32x SectionIndex = 0; SectionIndex < File->SectionCount; SectionIndex++)
                {
                    grn_section &Section = SectionArray[SectionIndex];

                    Section.PointerFixupArrayOffset = (uint32x)CurrentOffset;
                    CurrentOffset = AlignN(CurrentOffset + (Section.PointerFixupArrayCount *
                                                            SizeOf(grn_pointer_fixup)),
                                           DiskAlignment);

                    Section.MixedMarshallingFixupArrayOffset = (uint32x)CurrentOffset;
                    CurrentOffset = AlignN(CurrentOffset + (Section.MixedMarshallingFixupArrayCount *
                                                            SizeOf(grn_mixed_marshalling_fixup)),
                                           DiskAlignment);
                }}
            }

            // Position the writer to the section header and start CRCing
            SeekWriterFromCurrentPosition(DestWriter, FixedHeaderSize);
            BeginWriterCRC(DestWriter);

            // Ok, compute the rest of the section fields and write them
            // out.  The only fields that need work are:
            //  DataOffset
            //  PointerFixupArrayOffset;
            //  MixedMarshallingFixupArrayOffset;
            // The rest are valid from the previous read
            {for(int32x SectionIndex = 0; SectionIndex < File->SectionCount; SectionIndex++)
            {
                grn_section Section = SectionArray[SectionIndex];
                Section.DataSize = (uint32x)AlignN(Section.DataSize, DiskAlignment);
                Section.ExpandedDataSize = (uint32x)AlignN(Section.ExpandedDataSize, DiskAlignment);

                if (NeedsReversalOnPersist)
                {
                    Reverse32(SizeOf(Section), &Section);
                }

                Result = Result &&
                    WriteBytes(DestWriter, SizeOf(Section), &Section);
            }}

            // Now dump the section data
            {for(int32x SectionIndex = 0; SectionIndex < File->SectionCount; SectionIndex++)
            {
                grn_section &Section = SectionArray[SectionIndex];

                // Align the writer
                AlignWriterTo(DestWriter, DiskAlignment);
                Assert(Section.DataOffset == (uint32x)GetWriterPosition(DestWriter));

                // Write section data
                if(Section.DataSize != 0)
                {
                    Assert(CompressedData[SectionIndex] != 0);

                    Result = Result &&
                        WriteBytes(DestWriter, Section.DataSize, CompressedData[SectionIndex]);
                }
            }}


            {for(int32x SectionIndex = 0; SectionIndex < File->SectionCount; SectionIndex++)
            {
                grn_section &Section = SectionArray[SectionIndex];

                // Write the pointer fixups
                if (Section.PointerFixupArrayCount != 0)
                {
                    Assert(Fixups[SectionIndex]);

                    if (NeedsReversalOnPersist)
                    {
                        Reverse32(SizeOf(grn_pointer_fixup) * Section.PointerFixupArrayCount,
                                  Fixups[SectionIndex]);
                    }

                    AlignWriterTo(DestWriter, DiskAlignment);
                    Assert(Section.PointerFixupArrayOffset == (uint32x)GetWriterPosition(DestWriter));

                    Result = Result &&
                        WriteBytes(DestWriter,
                                   SizeOf(grn_pointer_fixup) * Section.PointerFixupArrayCount,
                                   Fixups[SectionIndex]);

                    // Technically, we could ignore this, we don't use
                    // these after they're written, but let's be safe...
                    if (NeedsReversalOnPersist)
                    {
                        Reverse32(SizeOf(grn_pointer_fixup) * Section.PointerFixupArrayCount,
                                  Fixups[SectionIndex]);
                    }
                }

                // Write the marshalls
                if (Section.MixedMarshallingFixupArrayCount != 0)
                {
                    Assert(Marshalls[SectionIndex]);

                    if (NeedsReversalOnPersist)
                    {
                        Reverse32(SizeOf(grn_mixed_marshalling_fixup) * Section.MixedMarshallingFixupArrayCount,
                                  Marshalls[SectionIndex]);
                    }

                    AlignWriterTo(DestWriter, DiskAlignment);
                    Assert(Section.MixedMarshallingFixupArrayOffset == (uint32x)GetWriterPosition(DestWriter));

                    Result = Result &&
                        WriteBytes(DestWriter,
                                   (SizeOf(grn_mixed_marshalling_fixup) *
                                    Section.MixedMarshallingFixupArrayCount),
                                   Marshalls[SectionIndex]);

                    // Technically, we could ignore this, we don't use
                    // these after they're written, but let's be safe...
                    if (NeedsReversalOnPersist)
                    {
                        Reverse32(SizeOf(grn_mixed_marshalling_fixup) * Section.MixedMarshallingFixupArrayCount,
                                  Marshalls[SectionIndex]);
                    }
                }
            }}

            // Get rid of these as soon as possible
            FreeCompressedSections(File, &CompressedData);
            Deallocate(CompressedData);
            CompressedData = NULL;

            // All of the data that needs to be CRC'd is.
            uint32 CRC = EndWriterCRC(DestWriter);

            // Seek back to the start and write the magic value & header
            uint32 const TotalFileSize =
                (uint32)(GetWriterPosition(DestWriter) - TopOfFile);
            SeekWriterFromStart(DestWriter, TopOfFile);

            grn_file_magic_value MagicValue = DestMagicValue;
            MagicValue.HeaderSize = FullHeaderSize;
            MagicValue.HeaderFormat = 0;
            MagicValue.Reserved[0] = MagicValue.Reserved[1] = 0;

            grn_file_header FileHeader = *File->Header;
            FileHeader.TotalSize = TotalFileSize;
            FileHeader.CRC = CRC;
            FileHeader.SectionArrayOffset = SizeOf(FileHeader);

            // Here's a tricky bit.  If the platform that we're writing is
            // of different endianness than this platform, then we have to
            // Reverse32 the magic value and the FileHeader
            if (NeedsReversalOnPersist)
            {
                Reverse32(SizeOf(MagicValue), &MagicValue);
                Reverse32(SizeOf(FileHeader), &FileHeader);
            }

            Result = Result &&
                WriteBytes(DestWriter, SizeOf(MagicValue), &MagicValue);
            Result = Result &&
                WriteBytes(DestWriter, SizeOf(FileHeader), &FileHeader);
        }
        else
        {
            // PlatformConversion failed.  That function will log appropriately
        }
    }
    else
    {
        // Fixup loads failed: Result is off the screen, so reset here... :)
        Result = false;
    }

    // Lose the fixups and marshalls
    {for(int32x SectionIndex = 0; SectionIndex < File->SectionCount; ++SectionIndex)
    {
        Deallocate(Fixups[SectionIndex]);
        Deallocate(Marshalls[SectionIndex]);
    }}
    Deallocate(Fixups);
    Deallocate(Marshalls);
    FreeFile(File);

    return Result;
}


bool GRANNY
PlatformConvertReaderToWriter(file_reader*                SourceReader,
                              file_writer*                DestWriter,
                              grn_file_magic_value const& DestMagicValue,
                              bool                        ExcludeTypeTree)
{
    return PlatformConvertReaderToWriterAligned(SourceReader, DestWriter,
                                                DestMagicValue,
                                                ExcludeTypeTree,
                                                DefaultFileDiskAlignment);
}

uint32 GRANNY
GetInMemoryFileCRC(file *File)
{
    CheckPointerNotNull(File, return 0);
    CheckPointerNotNull(File->Header, return 0);

    grn_section *Sections = GetGRNSectionArray(*File->Header);

    uint32 CRC;
    BeginCRC32(CRC);
    {for(int32x Section = 0; Section < File->SectionCount; Section++)
    {
        AddToCRC32(CRC, Sections[Section].ExpandedDataSize, File->Sections[Section]);
    }}
    EndCRC32(CRC);

    return CRC;
}
