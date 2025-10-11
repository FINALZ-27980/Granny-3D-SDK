// ========================================================================
// $File: //jeffr/granny_29/rt/granny_string_database.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_string_database.h"

#include "granny_assert.h"
#include "granny_crc.h"
#include "granny_data_type_conversion.h"
#include "granny_file.h"
#include "granny_file_format.h"
#include "granny_parameter_checking.h"
#include "granny_pointer_hash.h"
#include "granny_string.h"

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
#define SubsystemCode StringTableLogMessage

USING_GRANNY_NAMESPACE;

BEGIN_GRANNY_NAMESPACE;

data_type_definition StringDatabaseType[] =
{
    {ReferenceToArrayMember, "Strings", StringType},
    {UInt32Member, "DatabaseCRC"},
    {VariantReferenceMember, "ExtendedData"},

    {EndMember}
};

struct remapping_context
{
    string_database* StringDB;
    pointer_hash* Hash;
};

END_GRANNY_NAMESPACE;

string_database *GRANNY
GetStringDatabase(file &File)
{
    variant Root;
    GetDataTreeFromFile(File, &Root);

    uint32 TypeTag = File.Header->TypeTag;
    if(TypeTag == CurrentGRNStandardTag)
    {
        return((string_database *)Root.Object);
    }
    else
    {
        if(!File.ConversionBuffer)
        {
            Log2(WarningLogMessage, FileReadingLogMessage,
                 "File has run-time type tag of 0x%x, which doesn't match this "
                 "version of Granny (0x%x).  Automatic conversion will "
                 "be attempted.", TypeTag, CurrentGRNStandardTag);

            File.ConversionBuffer =
                ConvertTree(Root.Type, Root.Object, StringDatabaseType, 0);
        }

        return (string_database*)File.ConversionBuffer;
    }
}

static bool
RemapTypeStringPointers(remapping_context& Context, data_type_definition* TypePtr)
{
    if (!HashedPointerKeyExists(*Context.Hash, TypePtr))
    {
        AddPointerToHash(*Context.Hash, TypePtr, 0);

        data_type_definition* Walk = TypePtr;
        while (Walk->Type != EndMember)
        {
            intaddrx Idx = (intaddrx)Walk->Name;
            if (Idx < 0 || Idx >= Context.StringDB->StringCount)
            {
                Log0(ErrorLogMessage, StringLogMessage,
                     "Out of range string index found in type walk, has this file been remapped already?");
                return false;
            }

            Walk->Name = Context.StringDB->Strings[Idx];
            ++Walk;
        }

        // Rewalk, but now descend into the referenced types...
        Walk = TypePtr;
        while (Walk->Type != EndMember)
        {
            if (Walk->ReferenceType != NULL)
                RemapTypeStringPointers(Context, Walk->ReferenceType);

            ++Walk;
        }
    }

    return true;
}


static bool
RemapStringPointers(remapping_context& Context,
                    data_type_definition* TypePtr,
                    uint8* ObjectPtr)
{
    Assert(Context.StringDB);
    Assert(TypePtr && ObjectPtr);

    // The first thing to do is to walk the type, and remap the "Name" strings contained
    // therein.  This has to be recursive on the type, because we don't recurse into
    // sub-objects if the Object pointer of a ReferenceMember, say, is NULL.  Variant
    // members will be handled in the course of the tree traversal.
    if (!RemapTypeStringPointers(Context, TypePtr))
        return false;

    // Have we already walked this object?
    if (HashedPointerKeyExists(*Context.Hash, ObjectPtr))
        return true;
    AddPointerToHash(*Context.Hash, ObjectPtr, 0);

    // Ok, so we have the names remapped properly, let's start walking the object data.
    bool Result = true;

    uint8* WalkObj = ObjectPtr;
    data_type_definition* WalkType = TypePtr;
    while (Result && WalkType->Type != EndMember)
    {
        switch (WalkType->Type)
        {
            case InlineMember:
            {
                // There is a special case to handle here.  It's possible that the inline
                // member referred to could be at offset 0 in the object, which would mean
                // that the pointer hash check on recursion would test the same pointer as
                // above, incorrectly skipping the datatype.  If we detect that
                // (incredibly rare) situation, remove the parent object's pointer from
                // the hash so that the inline member will correctly fall through.  We
                // know that if the remap call returns true, that the pointer has been
                // correctly readded to the hash.  If false is returned, add it back
                // ourselves, even though we'll be bailing out of the loop immediately

                // Note that this only applies to the /first/ element, so if this is an
                // array inline, handle the other entries after

                if (WalkObj == ObjectPtr)
                {
                    RemovePointerFromHash(*Context.Hash, ObjectPtr);
                }

                Result = RemapStringPointers(Context,
                                             WalkType->ReferenceType,
                                             WalkObj);

                if (WalkObj == ObjectPtr && !Result)
                {
                    AddPointerToHash(*Context.Hash, ObjectPtr, 0);
                }
                else
                {
                    Assert(HashedPointerKeyExists(*Context.Hash, ObjectPtr));
                }

                int32x ArrayWidth = GetMemberArrayWidth(*WalkType);
                int32x UnitSize   = GetMemberUnitSize(*WalkType);
                {for (int32x Idx = 1; Idx < ArrayWidth && Result; ++Idx)
                {
                    Result = Result &&
                        RemapStringPointers(Context,
                                            WalkType->ReferenceType,
                                            WalkObj + (Idx*UnitSize));
                }}
            } break;

            case ReferenceMember:
            {
                uint8* ReferencedObject = *((uint8**)WalkObj);

                if (ReferencedObject != NULL)
                {
                    Result = RemapStringPointers(Context,
                                                 WalkType->ReferenceType,
                                                 ReferencedObject);
                }
            } break;

            case ReferenceToArrayMember:
            {
                int32 ArrayCount       = *((int32*)WalkObj);
                uint8* ReferencedArray = *((uint8**)(WalkObj + sizeof(int32)));

                if (ArrayCount != 0 && ReferencedArray != NULL)
                {
                    Assert(WalkType->ReferenceType);
                    if (TypeHasPointers(WalkType->ReferenceType))
                    {
                        int32x const ObjectSize = GetTotalObjectSize(WalkType->ReferenceType);

                        {for(int32 Idx = 0; Idx < ArrayCount && Result; ++Idx)
                        {
                            Result = RemapStringPointers(Context,
                                                         WalkType->ReferenceType,
                                                         ReferencedArray);
                            ReferencedArray += ObjectSize;
                        }}
                    }
                }
            } break;

            case ArrayOfReferencesMember:
            {
                int32 ArrayCount        = *((int32*)WalkObj);
                uint8** ReferencedArray = *((uint8***)(WalkObj + sizeof(int32)));

                if (ArrayCount != 0 && ReferencedArray != NULL)
                {
                    {for(int32 Idx = 0; Idx < ArrayCount && Result; ++Idx)
                    {
                        if (ReferencedArray[Idx])
                        {
                            Result = RemapStringPointers(Context,
                                                         WalkType->ReferenceType,
                                                         ReferencedArray[Idx]);
                        }
                    }}
                }
            } break;

            case VariantReferenceMember:
            {
                data_type_definition* VariantType = *((data_type_definition**)WalkObj);
                uint8*                VariantObj  = *((uint8**)(WalkObj + sizeof(void*)));

                // Ensure that this always happens
                if (VariantType)
                {
                    if (!RemapTypeStringPointers(Context, VariantType))
                        return false;
                }

                if (VariantType && VariantObj)
                {
                    Result = RemapStringPointers(Context, VariantType,
                                                 VariantObj);
                }
            } break;

            case ReferenceToVariantArrayMember:
            {
                data_type_definition* VariantType = *((data_type_definition**)WalkObj);
                int32 ArrayCount = *((int32*)(WalkObj + sizeof(void*)));
                uint8* ReferencedArray = *((uint8**)(WalkObj + sizeof(int32) + sizeof(void*)));

                // Ensure that this always happens
                if (VariantType)
                {
                    if (!RemapTypeStringPointers(Context, VariantType))
                        return false;
                }

                if (ArrayCount != 0 && ReferencedArray && VariantType)
                {
                    if (TypeHasPointers(VariantType))
                    {
                        int32x const ObjectSize = GetTotalObjectSize(VariantType);

                        {for(int32 Idx = 0; Idx < ArrayCount && Result; ++Idx)
                        {
                            Result = RemapStringPointers(Context,
                                                         VariantType,
                                                         ReferencedArray);
                            ReferencedArray += ObjectSize;
                        }}
                    }
                }
            } break;

            case StringMember:
            {
                // This is the easy one.
                char** StringPtr = (char**)WalkObj;

                intaddrx Idx = (intaddrx)*StringPtr;
                if (Idx < 0 || Idx >= Context.StringDB->StringCount)
                {
                    Log0(ErrorLogMessage, StringLogMessage,
                         "Out of range string index found in type walk, has this file been remapped already?");
                    Result = false;
                }
                *StringPtr = Context.StringDB->Strings[Idx];
            } break;

            default:
                // nothing to do
                break;
        }

        WalkObj += GetMemberTypeSize(*WalkType);
        ++WalkType;
    }

    return Result;
}


bool GRANNY
RemapFileStrings(file& File, string_database& StringDatabase)
{
    grn_file_header* Header = File.Header;
    CheckCondition(Header->StringDatabaseCRC != 0, return false);
    CheckCondition(Header->StringDatabaseCRC == StringDatabase.DatabaseCRC, return false);

    remapping_context Context;
    Context.StringDB = &StringDatabase;
    Context.Hash = NewPointerHash();

    bool Result = false;
    if (Context.Hash)
    {
        variant Root;
        GetDataTreeFromFile(File, &Root);

        Result = RemapStringPointers(Context, Root.Type, (uint8*)Root.Object);
        DeletePointerHash(Context.Hash);

        // Clear the database CRC member, to ensure that this isn't accidentally done
        // twice
        Header->StringDatabaseCRC = 0;
    }

    return Result;
}

void GRANNY
CreateStringDatabaseCRC(string_database& StringDB)
{
    BeginCRC32(StringDB.DatabaseCRC);

    {for(int32x Idx = 0; Idx < StringDB.StringCount; ++Idx)
    {
        if (StringDB.Strings[Idx])
        {
            AddToCRC32(StringDB.DatabaseCRC,
                       StringLength(StringDB.Strings[Idx]),
                       StringDB.Strings[Idx]);
        }
    }}

    // End the crc, and prevent "0" from slipping in.  We collapse that CRC to "1"
    EndCRC32(StringDB.DatabaseCRC);
    if (StringDB.DatabaseCRC == 0)
        StringDB.DatabaseCRC = 1;
}


char* GRANNY
RebaseToStringDatabase(void *Data, uint32 Identifier)
{
    CheckPointerNotNull(Data, return NULL);
    string_database* StringDB = (string_database*)Data;

    CheckUInt32Index(Identifier, (uint32)StringDB->StringCount, return NULL);

    return StringDB->Strings[Identifier];
}
