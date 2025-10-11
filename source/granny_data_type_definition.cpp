// ========================================================================
// $File: //jeffr/granny_29/rt/granny_data_type_definition.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_data_type_definition.h"

#include "granny_log.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"
#include "granny_string.h"
#include "granny_transform.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode DataTypeLogMessage
USING_GRANNY_NAMESPACE;
BEGIN_GRANNY_NAMESPACE;

data_type_definition EmptyType[]     = {{EndMember}};
data_type_definition StringType[]    = {{StringMember,           "String"},           {EndMember}};
data_type_definition Int16Type[]     = {{Int16Member,            "Int16"},            {EndMember}};
data_type_definition Int32Type[]     = {{Int32Member,            "Int32"},            {EndMember}};
data_type_definition UInt8Type[]     = {{UInt8Member,            "UInt8"},            {EndMember}};
data_type_definition UInt16Type[]    = {{UInt16Member,           "UInt16"},           {EndMember}};
data_type_definition UInt32Type[]    = {{UInt32Member,           "UInt32"},           {EndMember}};
data_type_definition Real32Type[]    = {{Real32Member,           "Real32"},           {EndMember}};
data_type_definition TripleType[]    = {{Real32Member,           "Real32", 0, 3},     {EndMember}};
data_type_definition QuadType[]      = {{Real32Member,           "Real32", 0, 4},     {EndMember}};
data_type_definition TransformType[] = {{TransformMember,        "Transform"},        {EndMember}};
data_type_definition Matrix4x4Type[] = {{Real32Member,           "Matrix4x4", 0, 16}, {EndMember}};
data_type_definition VariantType[]   = {{VariantReferenceMember, "Variant"},          {EndMember}};

struct member_type_info
{
    char const *TypeName;

    int32x Size32;         // Size of the member on a 32bit platform
    int32x Size64;         // Size of the member on a 64bit platform

    marshalling_type Marshalling;
    char const *CTypeName;
};

struct traversal_entry
{
    data_type_definition const* TraversalA;
    data_type_definition const* TraversalB;

    traversal_entry* Next;
};


#define Ptr32Size SizeOf(uint32)
#define Ptr64Size SizeOf(uint64)

member_type_info MemberTypeInfo[] =
{
    {"EndMember",    0, 0, AnyMarshalling, ""},

    // Note!
    // Widen your display.  Yes, it's really easier this way...
    // Note!

    // Structured members             Size32                                  Size64                                  Marshalling
    //                                -----------------------------------------------------------------------------------------
    {"InlineMember",                  0,                                      0,                                      AnyMarshalling,    ""},
    {"ReferenceMember",               Ptr32Size,                              Ptr64Size,                              AnyMarshalling,    "void *"},
    {"ReferenceToArrayMember",        SizeOf(int32) + Ptr32Size,              SizeOf(int32) + Ptr64Size,              Int32Marshalling,  "void *"},
    {"ArrayOfReferencesMember",       SizeOf(int32) + Ptr32Size,              SizeOf(int32) + Ptr64Size,              Int32Marshalling,  "void **"},
    {"VariantReferenceMember",        Ptr32Size + Ptr32Size,                  Ptr64Size + Ptr64Size,                  AnyMarshalling,    "void *"},

    /* we have to keep this around to match the enum*/
    {"__REMOVED",0, 0, AnyMarshalling,"unsupported"},
    /* we have to keep this around to match the enum*/

    {"ReferenceToVariantArrayMember", Ptr32Size + SizeOf(int32) + Ptr32Size,  Ptr64Size + SizeOf(int32) + Ptr64Size,  Int32Marshalling,  "void *"},

    // Yes, I know string is a pointer, so it really shouldn't care
    // which marshalling is done to it, but if someone registers a
    // string crc callback, it's actually an Int32.  Sigh.
    {"StringMember",                  Ptr32Size,                              Ptr64Size,                              Int32Marshalling,  "char *"},


    {"TransformMember",               SizeOf(transform),                      SizeOf(transform),                      Int32Marshalling,  "granny_transform"},

    // Floating-point members
    {"Real32Member",                  SizeOf(real32),                         SizeOf(real32),                         Int32Marshalling,  "granny_real32"},

    {"Int8Member",                    SizeOf(int8),                           SizeOf(int8),                           Int8Marshalling,   "granny_int8"},
    {"UInt8Member",                   SizeOf(uint8),                          SizeOf(uint8),                          Int8Marshalling,   "granny_uint8"},
    {"BinormalInt8Member",            SizeOf(int8),                           SizeOf(int8),                           Int8Marshalling,   "granny_int8"},
    {"NormalUInt8Member",             SizeOf(uint8),                          SizeOf(uint8),                          Int8Marshalling,   "granny_uint8"},

    {"Int16Member",                   SizeOf(int16),                          SizeOf(int16),                          Int16Marshalling,  "granny_int16"},
    {"UInt16Member",                  SizeOf(uint16),                         SizeOf(uint16),                         Int16Marshalling,  "granny_uint16"},
    {"BinormalInt16Member",           SizeOf(int16),                          SizeOf(int16),                          Int16Marshalling,  "granny_int16"},
    {"NormalUInt16Member",            SizeOf(uint16),                         SizeOf(uint16),                         Int16Marshalling,  "granny_uint16"},

    {"Int32Member",                   SizeOf(int32),                          SizeOf(int32),                          Int32Marshalling,  "granny_int32"},
    {"UInt32Member",                  SizeOf(uint32),                         SizeOf(uint32),                         Int32Marshalling,  "granny_uint32"},

    {"Real16Member",                  SizeOf(real16),                         SizeOf(real16),                         Int16Marshalling,  "granny_real16"},
    {"EmptyReferenceMember",          Ptr32Size,                              Ptr64Size,                              AnyMarshalling,    "void *"},
};

END_GRANNY_NAMESPACE;

int32x GRANNY
GetMemberUnitSize(data_type_definition const &Member)
{
    CompileAssert(ArrayLength(MemberTypeInfo) == OnePastLastMemberType);
    Assert(Member.Type >= 0 && Member.Type < OnePastLastMemberType);

    switch(Member.Type)
    {
        // Returns the size of /one/ of the objects
        case InlineMember:
        {
            CheckPointerNotNull(Member.ReferenceType, return 0);
            return GetTotalObjectSize(Member.ReferenceType);
        }

        default:
        {
        #if PROCESSOR_32BIT_POINTER
            return MemberTypeInfo[Member.Type].Size32;
        #elif PROCESSOR_64BIT_POINTER
            return MemberTypeInfo[Member.Type].Size64;
        #endif
        }
    }
}


int32x GRANNY
GetMemberUnitSizePlatform(member_type Type, int32x PointerSizeInBits)
{
    Assert(PointerSizeInBits == 32 || PointerSizeInBits == 64);
    Assert(Type >= 0 && Type < OnePastLastMemberType);
    CheckCondition(Type != InlineMember, return 0);

    switch (PointerSizeInBits)
    {
        case 32:
            return(MemberTypeInfo[Type].Size32);
        case 64:
            return(MemberTypeInfo[Type].Size64);

        default:
            InvalidCodePath("Pointer size parameter incorrect");
            return 0;
    }
}


int32x GRANNY
GetMemberTypeSize(data_type_definition const &MemberType)
{
    CompileAssert(ArrayLength(MemberTypeInfo) == OnePastLastMemberType);
    Assert(MemberType.Type < OnePastLastMemberType);

    int32x const MemberSize = GetMemberUnitSize(MemberType);
    return (MemberSize * GetMemberArrayWidth(MemberType));
}

int32x GRANNY
GetTotalObjectSize(data_type_definition const *TypeDefinition)
{
    int32x Size = 0;

    while(TypeDefinition && (TypeDefinition->Type != EndMember))
    {
        Size += GetMemberTypeSize(*TypeDefinition);
        ++TypeDefinition;
    }

    return(Size);
}

int32x GRANNY
GetTotalTypeSize(data_type_definition const *TypeDefinition)
{
    int32x Size = 0;
    if (TypeDefinition)
    {
        do
        {
            Size += SizeOf(data_type_definition);
        } while ((TypeDefinition++)->Type != EndMember);
    }

    return(Size);
}

char const *GRANNY
GetMemberTypeName(member_type Type)
{
    Assert(Type < OnePastLastMemberType);
    return(MemberTypeInfo[Type].TypeName);
}

char const *GRANNY
GetMemberCTypeName(member_type Type)
{
    Assert(Type < OnePastLastMemberType);
    return(MemberTypeInfo[Type].CTypeName);
}

void GRANNY
GetTypeDefintionCName(data_type_definition const *TypeDef, char *Result, int32x MaxLength)
{
    int32x LengthLeft = MaxLength;

    member_type Type = TypeDef->Type;

    Assert(Type < OnePastLastMemberType);
    switch ( Type )
    {
    case ReferenceMember:
    {
        Assert ( TypeDef->ReferenceType != NULL );
        // I'd like to put the name of the structure (e.g. granny_curve2),
        // but we have no way of actually knowing what that is!
        CopyStringMaxLength ( "struct *", &Result, &LengthLeft );
        return;
    }

    case InlineMember:
    {
        Assert ( TypeDef->ReferenceType != NULL );
        CopyStringMaxLength ( TypeDef->Name, &Result, &LengthLeft );
        return;
    }

    case EmptyReferenceMember:
    case ReferenceToArrayMember:
    case ArrayOfReferencesMember:
    case VariantReferenceMember:
    case ReferenceToVariantArrayMember:
        // These still need doing properly.
        CopyStringMaxLength ( MemberTypeInfo[Type].CTypeName, &Result, &LengthLeft );
        return;

    case EndMember:
    case StringMember:
    case TransformMember:
    case Real32Member:
    case Real16Member:
    case Int8Member:
    case UInt8Member:
    case BinormalInt8Member:
    case NormalUInt8Member:
    case Int16Member:
    case UInt16Member:
    case BinormalInt16Member:
    case NormalUInt16Member:
    case Int32Member:
    case UInt32Member:
        CopyStringMaxLength ( MemberTypeInfo[Type].CTypeName, &Result, &LengthLeft );
        return;
    default:
        // If you hit this, you added a new type. Does it need a special case?
        InvalidCodePath("Unknown type");
        CopyStringMaxLength ( MemberTypeInfo[Type].CTypeName, &Result, &LengthLeft );
        return;
    }
}

bool GRANNY
MemberHasPointers(data_type_definition const &MemberType)
{
    if(MemberType.Type == InlineMember)
    {
        return(TypeHasPointers(MemberType.ReferenceType));
    }
    else
    {
        // Note that we don't note the EmptyReferences...
        return((MemberType.Type == VariantReferenceMember) ||
               (MemberType.Type == ReferenceToVariantArrayMember) ||
               (MemberType.Type == StringMember) ||
               ( ((MemberType.Type == ReferenceMember) ||
                  (MemberType.Type == ReferenceToArrayMember) ||
                  (MemberType.Type == ArrayOfReferencesMember) )
                && (MemberType.ReferenceType != 0)));
    }
}

bool GRANNY
TypeHasPointers(data_type_definition const *TypeDefinition)
{
    // Subtlety - you don't have to check for recursion in this
    // routine, because if a type can (eventually) point back
    // to itself, that means it _must_ have had a pointer, so
    // we would've early-outed already.

    while(TypeDefinition && (TypeDefinition->Type != EndMember))
    {
        if(MemberHasPointers(*TypeDefinition))
        {
            return(true);
        }

        ++TypeDefinition;
    }

    return(false);
}

uint32 GRANNY
GetMemberMarshalling(data_type_definition const &MemberType)
{
    if(MemberType.Type == InlineMember)
    {
        return(GetObjectMarshalling(MemberType.ReferenceType));
    }
    else
    {
        return(MemberTypeInfo[MemberType.Type].Marshalling);
    }
}

uint32 GRANNY
GetObjectMarshalling(data_type_definition const *TypeDefinition)
{
    uint32 Marshalling = 0;
    while(TypeDefinition && (TypeDefinition->Type != EndMember))
    {
        Marshalling |= GetMemberMarshalling(*TypeDefinition);
        ++TypeDefinition;
    }

    return(Marshalling);
}

bool GRANNY
IsMixedMarshalling(uint32x Marshalling)
{
    return((Marshalling & (Marshalling - 1)) != 0);
}

uintaddrx GRANNY
MakeEmptyDataTypeMember(data_type_definition const &MemberType, void *Member)
{
    uintaddrx MemberSize = 0;

    switch(MemberType.Type)
    {
        case InlineMember:
        {
            Assert(MemberType.ReferenceType);
            int32x const Width    = GetMemberArrayWidth(MemberType);
            int32x const TypeSize = GetMemberUnitSize(MemberType);

            {for (int32x Idx = 0; Idx < Width; ++Idx)
            {
                uint8* Location = (uint8*)Member + (Idx*TypeSize);
                MemberSize += MakeEmptyDataTypeObject(MemberType.ReferenceType, Location);
            }}
        } break;

        case TransformMember:
        {
            MakeIdentity(*(transform *)Member);
            MemberSize = SizeOf(transform);
        } break;

        default:
        {
            MemberSize = GetMemberTypeSize(MemberType);
            SetUInt8(MemberSize, 0, Member);
        } break;
    }

    return(MemberSize);
}

uintaddrx GRANNY
MakeEmptyDataTypeObject(data_type_definition const *TypeDefinition,
                        void *Object)
{
    uint8 *Member = (uint8 *)Object;
    while(TypeDefinition && (TypeDefinition->Type != EndMember))
    {
        Member += MakeEmptyDataTypeMember(*TypeDefinition, Member);
        ++TypeDefinition;
    }

    return (Member - (uint8 *)Object);
}

int32x GRANNY
GetMemberArrayWidth(data_type_definition const &Member)
{
    switch (Member.Type)
    {
        case ReferenceMember:
        case ReferenceToArrayMember:
        case ArrayOfReferencesMember:
        case VariantReferenceMember:
        case ReferenceToVariantArrayMember:
        case EmptyReferenceMember:
        case StringMember:
        {
            CheckCondition(Member.ArrayWidth == 0, return 1);
            return 1;
        }

        default:
        {
            return (Member.ArrayWidth ? Member.ArrayWidth : 1);
        }
    }
}

static bool
DataTypesAreEqualUberRoutine(data_type_definition const *A,
                             data_type_definition const *B,
                             string_comparison_callback *LocalStringComparisonCallback,
                             data_type_definition const *UnlessBEquals,
                             traversal_entry* RecursionChain)
{
    if ( ( B == UnlessBEquals ) && ( UnlessBEquals != NULL ) )
    {
        return false;
    }

    if ( A == B )
    {
        return true;
    }

    if ( ( A == NULL ) || ( B == NULL ) )
    {
        // ...and they're obviously not both NULL.
        return false;
    }

    // Check for recursion
    {for(traversal_entry* Walk = RecursionChain; Walk; Walk = Walk->Next)
    {
        if (((Walk->TraversalA == A) && (Walk->TraversalB == B)) ||
            ((Walk->TraversalA == B) && (Walk->TraversalB == A)))
        {
            // We recursed! Assume if they do differ, the calling
            // routine will spot the difference.
            return true;
        }
    }}

    // Setup the chain entry for this comparison
    traversal_entry StackEntry;
    StackEntry.TraversalA = A;
    StackEntry.TraversalB = B;
    StackEntry.Next = RecursionChain;

    bool Result = true;
    while((A->Type != EndMember) && (B->Type != EndMember))
    {
        if((A->Type != B->Type) ||
           (A->ArrayWidth != B->ArrayWidth))
        {
            Result = false;
            break;
        }
        else if ( ( LocalStringComparisonCallback != NULL ) && ( LocalStringComparisonCallback ( A->Name, B->Name ) != 0 ) )
        {
            // The same types, but the names are different.
            Result = false;
            break;
        }

        Assert ( A->Type == B->Type );
        if(A->Type == UnsupportedMemberType_Remove)
        {
            LogNotImplemented("Switchable types no longer supported in Granny 2.7+");
        }
        else
        {
            if(!DataTypesAreEqualUberRoutine(A->ReferenceType, B->ReferenceType,
                                             LocalStringComparisonCallback,
                                             UnlessBEquals, &StackEntry))
            {
                Result = false;
                break;
            }
        }

        ++A;
        ++B;
    }

    if(A->Type != B->Type)
    {
        Result = false;
    }

    return(Result);
}

bool GRANNY
DataTypesAreEqualWithNameCallback(data_type_definition const *A,
                                  data_type_definition const *B,
                                  string_comparison_callback *LocalStringComparisonCallback)
{
    return DataTypesAreEqualUberRoutine ( A, B, LocalStringComparisonCallback, NULL, NULL );
}


bool GRANNY
DataTypesAreEqualWithNames(data_type_definition const *A,
                           data_type_definition const *B)
{
    // Use the built-in comparison.
    return DataTypesAreEqualUberRoutine ( A, B, StringDifferenceInternal, NULL, NULL );
}

bool GRANNY
DataTypesAreEqual(data_type_definition const *A,
                  data_type_definition const *B)
{
    return DataTypesAreEqualUberRoutine ( A, B, NULL, NULL, NULL );
}

bool GRANNY
DataTypesAreEqualAndBDoesntInclude( data_type_definition const *A,
                                    data_type_definition const *B,
                                    data_type_definition const *UnlessBEquals)
{
    return DataTypesAreEqualUberRoutine ( A, B, NULL, UnlessBEquals, NULL );
}



data_type_definition *GRANNY
DataTypeBeginsWith(data_type_definition const *A,
                   data_type_definition const *B)
{
    if(A && B)
    {
        while((A->Type != EndMember) && (B->Type != EndMember))
        {
            if((A->Type != B->Type) ||
               (A->ArrayWidth != B->ArrayWidth))
            {
                return(0);
            }

            if(A->Type == UnsupportedMemberType_Remove)
            {
                LogNotImplemented("Switchable types no longer supported in Granny 2.7+");
            }
            else
            {
                if(!DataTypesAreEqual(A->ReferenceType, B->ReferenceType))
                {
                    return(0);
                }
            }

            ++A;
            ++B;
        }

        return((B->Type == EndMember) ? (data_type_definition *)A : 0);
    }

    return(0);
}

void GRANNY
ReverseTypeArray(data_type_definition const *Type, int32x Count, void *Array)
{
    uint8 *MemberData = (uint8 *)Array;
    while(Count--)
    {
        data_type_definition const *MemberType = Type;
        while(MemberType->Type != EndMember)
        {
            int32x const MemberSize = GetMemberTypeSize(*MemberType);
            if(MemberType->Type == InlineMember)
            {
                int32x ArrayCount = GetMemberArrayWidth(*MemberType);
                ReverseTypeArray(MemberType->ReferenceType, ArrayCount, MemberData);
            }
            else
            {
                switch(GetMemberMarshalling(*MemberType))
                {
                    case Int32Marshalling:
                    {
                        Reverse32(MemberSize, MemberData);
                    } break;

                    case Int16Marshalling:
                    {
                        Reverse16(MemberSize, MemberData);
                    } break;

                    case AnyMarshalling:
                    case Int8Marshalling:
                    {
                        // Nothing special to do for this type
                    } break;

                    default:
                    {
                        // Illegal type
                        InvalidCodePath("Unrecognized type");
                    } break;
                }
            }

            MemberData += MemberSize;
            ++MemberType;
        }
    }
}


// This is rampant paranoia, but wtf.
CompileAssert(EndMember == 0);
CompileAssert(InlineMember == 1);
CompileAssert(ReferenceMember == 2);
CompileAssert(ReferenceToArrayMember == 3);
CompileAssert(ArrayOfReferencesMember == 4);
CompileAssert(VariantReferenceMember == 5);
CompileAssert(UnsupportedMemberType_Remove == 6);
CompileAssert(ReferenceToVariantArrayMember == 7);
CompileAssert(StringMember == 8);
CompileAssert(TransformMember == 9);
CompileAssert(Real32Member == 10);
CompileAssert(Int8Member == 11);
CompileAssert(UInt8Member == 12);
CompileAssert(BinormalInt8Member == 13);
CompileAssert(NormalUInt8Member == 14);
CompileAssert(Int16Member == 15);
CompileAssert(UInt16Member == 16);
CompileAssert(BinormalInt16Member == 17);
CompileAssert(NormalUInt16Member == 18);
CompileAssert(Int32Member == 19);
CompileAssert(UInt32Member == 20);
CompileAssert(Real16Member == 21);
CompileAssert(EmptyReferenceMember == 22);


