// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_character_info.cpp $
// $DateTime: 2012/05/15 14:36:44 $
// $Change: 37415 $
// $Revision: #5 $
//
// $Notice: $
// ========================================================================
#include "gstate_character_info.h"
#include "gstate_token_context.h"

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

// =============================================================================
// Type arrays
// =============================================================================

static granny_data_type_definition SourceFileRefType[] =
{
    { GrannyStringMember, "SourceFilename" },
    { GrannyInt32Member,  "ExpectedAnimCount" },  // 0 == old file
    { GrannyUInt32Member, "AnimCRC" },            // 0 == no crc computed

    { GrannyEmptyReferenceMember, "SourceInfo" },

    { GrannyEndMember }
};

static granny_data_type_definition AnimationSpecType[] =
{
    { GrannyReferenceMember, "SourceReference", SourceFileRefType },
    { GrannyInt32Member,     "AnimationIndex" },
    { GrannyStringMember,    "ExpectedName" },

    { GrannyEndMember }
};

granny_data_type_definition GSTATE AnimationSlotType[] =
{
    { GrannyStringMember,    "Name" },
    { GrannyInt32Member,     "SlotIndex" },

    { GrannyEndMember }
};

granny_data_type_definition GSTATE AnimationSetType[] =
{
    { GrannyStringMember,            "Name" },
    { GrannyArrayOfReferencesMember, "SourceFileReferences", SourceFileRefType },
    { GrannyReferenceToArrayMember,  "AnimationSpecs",       AnimationSpecType },
      
    { GrannyEndMember }
};

granny_data_type_definition GStateCharacterInfoType[] =
{
    // Note that the TouchVersion command looks for this member, make sure to update that
    // preprocessor command if this moves.
    { GrannyInt32Member,             "NumUniqueTokenized" },

    { GrannyVariantReferenceMember,  "StateMachine" },
    { GrannyArrayOfReferencesMember, "AnimationSlots",         AnimationSlotType },
    { GrannyArrayOfReferencesMember, "AnimationSets",          AnimationSetType },

    { GrannyStringMember, "ModelNameHint" },
    { GrannyInt32Member,  "ModelIndexHint" },

    { GrannyVariantReferenceMember,  "EditorData" },

    { GrannyEndMember }
};

// =============================================================================
gstate_character_info*
GStateGetCharacterInfo(granny_file* File)
{
    if (File == 0)
        return 0;

    gstate_character_info* InfoPtr = 0;

    granny_variant Root;
    GrannyGetDataTreeFromFile(File, &Root);

    granny_uint32 TypeTag = File->Header->TypeTag;
    if (TypeTag == GStateCharacterInfoTag)
    {
        InfoPtr = (gstate_character_info*)Root.Object;
    }
    else
    {
        // Ok, we need to do the basic upgrade, then scan through and look for
        // variants that we need to update separately.  So let's get the size of the
        // conversion buffer first, then look for our custom bits.
        if (!File->ConversionBuffer)
        {
            GStateWarning("File has run-time type tag of 0x%x, which doesn't match this "
                          "version of GState (0x%x).  Automatic conversion will "
                          "be attempted.", TypeTag, GStateCharacterInfoTag);

            File->ConversionBuffer =
                GrannyConvertTree(Root.Type, Root.Object,
                                  GStateCharacterInfoType,
                                  &token_context::UpdateVariantHandler);
        }

        InfoPtr = (gstate_character_info *)File->ConversionBuffer;
    }

    return InfoPtr;
}

bool
GStateReadCharacterInfoFromReader(granny_file_reader*     Reader,
                                  granny_file*&           FilePtr,
                                  gstate_character_info*& InfoPtr)
{
    // Clear out the result variables
    FilePtr = GrannyReadEntireFileFromReader(Reader);
    InfoPtr = GStateGetCharacterInfo(FilePtr);

    return (InfoPtr != 0);
}

bool
GStateReadCharacterInfo(char const*             Filename,
                        granny_file*&           FilePtr,
                        gstate_character_info*& InfoPtr)
{
    // Clear out the result variables
    InfoPtr = 0;
    FilePtr = 0;

    granny_file_reader* Reader = GrannyCreatePlatformFileReader(Filename);
    if (Reader == 0)
        return false;

    bool Success = GStateReadCharacterInfoFromReader(Reader, FilePtr, InfoPtr);
    GrannyCloseFileReader(Reader);

    return Success;
}

// =============================================================================
granny_int32x
GStateGetNumAnimationSets(gstate_character_info* Info)
{
    return Info->AnimationSetCount;
}

char const*
GStateGetAnimationSetName(gstate_character_info* Info, granny_int32x SetIndex)
{
    GStateCheckIndex(SetIndex, Info->AnimationSetCount, return 0);

    return Info->AnimationSets[SetIndex]->Name;
}



granny_int32x
GStateSetIndexFromSetName(gstate_character_info*   Info,
                          char const*              SetName)
{
    GStateCheckPtrNotNULL(Info, return false);
    GStateCheckPtrNotNULL(SetName, return false);
    
    // Find SetName in the AnimationSets....
    {for (int Idx = 0; Idx < Info->AnimationSetCount; ++Idx)
    {
        GStateCheckPtrNotNULL(Info->AnimationSets[Idx], continue);
        
        if (_stricmp(Info->AnimationSets[Idx]->Name, SetName) == 0)
        {
            return Idx;
        }
    }}

    GStateWarning("%s: Set '%s' not found\n", __FUNCTION__, SetName);
    return -1;
}

bool
GStateBindCharacterFileReferencesForSetIndex(gstate_character_info*   Info,
                                             granny_int32x            SetIndex,
                                             gstate_file_ref_callback RefCallback,
                                             void*                    UserData)
{
    GStateCheckPtrNotNULL(Info, return false);
    GStateCheckPtrNotNULL(RefCallback, return false);
    GStateCheckIndex(SetIndex, Info->AnimationSetCount, return false);

    animation_set* ThisSet = Info->AnimationSets[SetIndex];
    
    bool AllSucceeded = true;
    {for (int Idx = 0; Idx < ThisSet->SourceFileReferenceCount; ++Idx)
    {
        source_file_ref* Ref = ThisSet->SourceFileReferences[Idx];
        GStateAssert(Ref);

        if (Ref->SourceInfo == 0)
        {
            granny_file_info* NewInfo = RefCallback(Info, Ref->SourceFilename, UserData);
            if (NewInfo == 0)
            {
                AllSucceeded = false;
                GStateWarning("Expected to find source '%s'", Ref->SourceFilename);
                continue;
            }

            granny_uint32 NewCRC = ComputeAnimCRC(NewInfo);
            if (Ref->ExpectedAnimCount != 0 && Ref->AnimCRC != 0)
            {
                if (Ref->ExpectedAnimCount != NewInfo->AnimationCount)
                {
                    GStateError("Expected '%s' to have %d animations, it has %d.  Ignoring this file",
                                Ref->SourceFilename,
                                Ref->ExpectedAnimCount,
                                NewInfo->AnimationCount);
                    AllSucceeded = false;
                }
                else if (Ref->AnimCRC != NewCRC)
                {
                    GStateError("Expected '%s' to crc as %x, not %x.  Ignoring this file",
                                Ref->SourceFilename, Ref->AnimCRC, NewCRC);
                    AllSucceeded = false;
                }
                else
                {
                    // All fine.
                    Ref->SourceInfo = NewInfo;
                }
            }
            else
            {
                // Just take the info, and update the reference, nothing we can do right
                // here.
                Ref->ExpectedAnimCount = NewInfo->AnimationCount;
                Ref->AnimCRC           = NewCRC;
                Ref->SourceInfo        = NewInfo;
            }
        }
    }}

    return AllSucceeded;
}

bool
GStateBindCharacterFileReferences(gstate_character_info*   Info,
                                  gstate_file_ref_callback RefCallback,
                                  void*                    UserData)
{
    GStateCheckPtrNotNULL(Info, return false);
    GStateCheckPtrNotNULL(RefCallback, return false);

    bool AllSucceeded = true;

    {for (int Idx = 0; Idx < Info->AnimationSetCount; ++Idx)
    {
        AllSucceeded =
            GStateBindCharacterFileReferencesForSetIndex(
                Info, Idx,
                RefCallback, UserData)
            && AllSucceeded;
    }}

    return AllSucceeded;
    
}

granny_uint32 GSTATE
ComputeAnimCRC(granny_file_info* FileInfo)
{
    // Handy little function to do a quick check that the animation names/count are what
    // we expect from the file.  This allows us to hopefully detect when a source file has
    // changed underneath us
    granny_uint32 CRC = 0x1;
    GrannyBeginCRC32(&CRC);
    {for (granny_int32 Idx = 0; Idx < FileInfo->AnimationCount; ++Idx)
    {
        GrannyAddToCRC32(&CRC, sizeof(Idx), &Idx);
        if (FileInfo->Animations[Idx]->Name)
            GrannyAddToCRC32(&CRC, strlen(FileInfo->Animations[Idx]->Name), FileInfo->Animations[Idx]->Name);
    }}

    GrannyEndCRC32(&CRC);
    return CRC;
}
