// ========================================================================
// $File: //jeffr/granny_29/rt/granny_data_type_conversion.cpp $
// $DateTime: 2012/09/21 10:44:59 $
// $Change: 39482 $
// $Revision: #5 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_data_type_conversion.h"

#include "granny_conversions.h"
#include "granny_curve.h"
#include "granny_float16.h"
#include "granny_limits.h"
#include "granny_member_iterator.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"
#include "granny_pointer_hash.h"
#include "granny_stack_allocator.h"
#include "granny_string.h"
#include "granny_string_formatting.h"
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
#define SubsystemCode DataTypeLogMessage

// 64bit note: Handy macro for pulling cnt + pointer out of a memory
// location in the correct 64/32-bit fashion.
#define ExtractCountAndPtr(Count, Pointer, Type, Memory)        \
    do {                                                        \
        Count   = *((int32*)(Memory));                          \
        Pointer = *((Type*)((uint8*)Memory + SizeOf(int32)));   \
    } while (false)

// 64bit note: Handy macro for setting cnt + pointer into a memory
// location in the correct 64/32-bit fashion.
#define SetCountAndPtr(Count, Pointer, Type, Memory)            \
    do {                                                        \
        *((int32*)(Memory)) = Count;                            \
        *((Type*)((uint8*)Memory + SizeOf(int32))) = Pointer;   \
    } while (false)


USING_GRANNY_NAMESPACE;
BEGIN_GRANNY_NAMESPACE;

struct conversion_context
{
    pointer_hash* RemapHash;
    pointer_hash* PointerConvertHash;
    pointer_hash* AlreadyDone;

    bool32 ClearFields;
    bool32 Writing;
    int32x Size;
    uint8 *Base;

    conversion_handler* Handler;
    variant_replace_fn* VariantReplacer;
};

END_GRANNY_NAMESPACE;

static void
InitializeContext(conversion_context &Context, void *Base,
                  conversion_handler* Handler,
                  variant_replace_fn* VariantReplacer)
{
    Context.RemapHash = 0;
    Context.PointerConvertHash = 0;
    Context.AlreadyDone = 0;
    Context.ClearFields = true;
    Context.Writing = (Base != 0);
    Context.Size = 0;
    Context.Base = (uint8 *)Base;
    Context.Handler = Handler;
    Context.VariantReplacer = VariantReplacer;
}

static void *
GetBack(conversion_context &Context)
{
    if(Context.Writing)
    {
        return ( Context.Base + Context.Size );
    }
    else
    {
        // This must be non-zero, because NULL has the meaning
        // "I have not allocated space, but you should"
        // If we're not writing, we're still allocating space,
        // so the notion of "I have already been allocated" still exists.
        return ( (void*) (1) );
    }
}

static void
Advance(conversion_context &Context, int32x By)
{
    Context.Size += By;
}

static void *
GetSpace(conversion_context &Context, int32x Size)
{
    void *Space = GetBack(Context);
    Advance(Context, Size);
    return(Space);
}

static void **
GetPointer(conversion_context &Context, uint8 *Pointer)
{
    // This is scratch space that the first phase uses
    // so the code can just write things like array size counts
    // and pointers without worrying about whether it's valid memory or not.
    static uint32 Ignored[4];

    return(Context.Writing ? (void **)Pointer : (void **)&Ignored);
}

bool GRANNY
FindMatchingMember(data_type_definition const *SourceType,
                   void const *SourceObject,
                   char const *DestMemberName,
                   variant* Result)
{
    uint8 *SourceMember = (uint8 *)SourceObject;
    while(SourceType && (SourceType->Type != EndMember))
    {
        if (StringsAreEqualLowercaseOrCallback(SourceType->Name, DestMemberName))
        {
            if (Result)
            {
                Result->Type = (data_type_definition *)SourceType;
                Result->Object = SourceMember;
            }
            return true;
        }

        SourceMember += GetMemberTypeSize(*SourceType);
        ++SourceType;
    }

    if (Result)
    {
        Result->Type = 0;
        Result->Object = 0;
    }
    return false;
}

static int32x
ClampInt01(int32x Int, int32x Range)
{
    int32x HalfRange = Range / 2;

    if(Int > HalfRange)
    {
        return(1);
    }
    else if(Int < -HalfRange)
    {
        return(-1);
    }
    else
    {
        return(0);
    }
}

static int32x
ConvertMemberToInt32(member_type Type, void const *Member)
{
    int32x Result = 0;

    switch(Type)
    {
        case Int8Member:
        {
            Result = *(int8 const *)Member;
        } break;

        case UInt8Member:
        {
            Result = *(uint8 const *)Member;
        } break;

        case BinormalInt8Member:
        {
            Result = ClampInt01(*(int8 const *)Member, Int8Maximum);
        } break;

        case NormalUInt8Member:
        {
            Result = ClampInt01(*(uint8 const *)Member, UInt8Maximum);
        } break;

        case Int16Member:
        {
            Result = *(int16 const *)Member;
        } break;

        case UInt16Member:
        {
            Result = *(uint16 const *)Member;
        } break;

        case BinormalInt16Member:
        {
            Result = ClampInt01(*(int16 const *)Member, Int16Maximum);
        } break;

        case NormalUInt16Member:
        {
            Result = ClampInt01(*(uint16 const *)Member, UInt16Maximum);
        } break;

        case Int32Member:
        {
            Result = *(int32 const *)Member;
        } break;

        case UInt32Member:
        {
            Result = (int32x)*(uint32 const *)Member;
        } break;

        case Real32Member:
        {
            Result = RoundReal32ToInt32(*(real32 const *)Member);
        } break;

        case Real16Member:
        {
            real32 FloatVersion;
            Real16ToReal32(*(real16 const *)Member, &FloatVersion);
            Result = RoundReal32ToInt32(FloatVersion);
        } break;

        case StringMember:
        {
            char const *String = *(char const **)Member;
            if(String)
            {
                Result = ConvertToInt32(String);
            }
        } break;

        default:
        {
            Result = 0;
        } break;
    }

    return(Result);
}

static real32
ConvertMemberToReal32(member_type Type, void const *Member)
{
    real32 Result = 0.0f;

    switch(Type)
    {
        case Int8Member:
        {
            Result = (real32) *(uint8 const *)Member;
        } break;

        case UInt8Member:
        {
            Result = (real32) *(uint8 const *)Member;
        } break;

        case BinormalInt8Member:
        {
            Result = ((real32)(*(int8 const *)Member) /
                      (real32)Int8Maximum);
        } break;

        case NormalUInt8Member:
        {
            Result = ((real32)(*(uint8 const *)Member) /
                      (real32)UInt8Maximum);
        } break;

        case Int16Member:
        {
            Result = (real32) *(int16 const *)Member;
        } break;

        case UInt16Member:
        {
            Result = (real32) *(uint16 const *)Member;
        } break;

        case BinormalInt16Member:
        {
            Result = ((real32)(*(int16 const *)Member) /
                      (real32)Int16Maximum);
        } break;

        case NormalUInt16Member:
        {
            Result = ((real32)(*(uint16 const *)Member) /
                      (real32)UInt16Maximum);
        } break;

        case Int32Member:
        {
            Result = (real32) *(int32 const *)Member;
        } break;

        case Real32Member:
        {
            Result = *(real32 const *)Member;
        } break;

        case Real16Member:
        {
            Real16ToReal32(*(real16 const *)Member, &Result);
        } break;

        case StringMember:
        {
            char const *String = *(char const **)Member;
            if(String)
            {
                Result = ConvertToReal32(String, true);
            }
        } break;

        default:
        {
            Result = 0.0f;
        } break;
    }

    return(Result);
}

static real16
ConvertMemberToReal16(member_type Type, void const *Member)
{
    if (Type == Real16Member)
        return *(real16 const *)Member;

    real32 Result32 = ConvertMemberToReal32(Type, Member);
    real16 Result16 = Real32ToReal16(Result32);

    return Result16;
}

static char *
ConvertMemberToString(conversion_context &Context,
                      member_type Type, void const *Member)
{
    int32x Length = 0;
    char ConversionBuffer[MaximumNumberToStringBuffer];

    char *Result = (char *)GetBack(Context);
    char *WriteTo = Result;
    if(!Context.Writing)
    {
        WriteTo = ConversionBuffer;
    }

    switch(Type)
    {
        case Int8Member:
        case UInt8Member:
        case Int16Member:
        case UInt16Member:
        case Int32Member:
        case UInt32Member:
        {
            Length = ConvertInt32ToString(
                SizeOf(ConversionBuffer),
                WriteTo,
                ConvertMemberToInt32(Type, Member), 10);
        } break;

        case BinormalInt8Member:
        case NormalUInt8Member:
        case BinormalInt16Member:
        case NormalUInt16Member:
        case Real32Member:
        case Real16Member:
        {
            Length = ConvertReal64ToString(
                SizeOf(ConversionBuffer),
                WriteTo,
                ConvertMemberToReal32(Type, Member), 10, 6);
        } break;

        case StringMember:
        {
            char const *String = *(char const **)Member;
            if(Context.Writing)
            {
                Length = CopyString(String, WriteTo);
            }
            else
            {
                Length = StringLength(String) + 1;
            }
        } break;

        default:
            Assert ( false );
            break;
    }

    int32x AlignedLength;
    CheckConvertToInt32(AlignedLength, Align32(Length), AlignedLength = (int32x)Align32(Length); Assert(false));
    Advance(Context, AlignedLength);
    return(Result);
}

static int32x
Normalize(real32 Value, int32x Maximum)
{
    return(RoundReal32ToInt32(Value * (real32)Maximum));
}

static void
CopyMember(conversion_context &Context,
           data_type_definition const &SourceType, void const *SourceMember,
           data_type_definition const &DestType,   void *DestMember)
{
    Assert ( DestMember != NULL );
    Assert ( Context.Writing );
    int32x const DestMemberSize = GetMemberTypeSize(DestType);

    if (Context.Handler)
    {
        // Give the override handler first crack at this member...
        if ((*Context.Handler)(&SourceType, SourceMember, &DestType, DestMember))
        {
            // Copied, skip the rest...
            return;
        }
    }

    if (DestType.Type == SourceType.Type)
    {
        int32x Size = GetMemberTypeSize(SourceType);
        if(Size > DestMemberSize)
        {
            Size = DestMemberSize;
        }

        Copy(Size, SourceMember, DestMember);
    }
    else
    {
        switch(DestType.Type)
        {
            case ReferenceMember:
            case ReferenceToArrayMember:
            case ArrayOfReferencesMember:
            case VariantReferenceMember:
            case ReferenceToVariantArrayMember:
            case TransformMember:
            case EmptyReferenceMember:
            {
                if(Context.ClearFields)
                {
                    MakeEmptyDataTypeMember(DestType, DestMember);
                }
            } break;

            case StringMember:
            {
                *(char **)DestMember = ConvertMemberToString(
                    Context, SourceType.Type, SourceMember);
            } break;

            default:
            {
                if(Context.ClearFields)
                {
                    MakeEmptyDataTypeMember(DestType, DestMember);
                }

                int32x ArrayWidth = GetMemberArrayWidth(SourceType);
                if(ArrayWidth > GetMemberArrayWidth(DestType))
                {
                    ArrayWidth = GetMemberArrayWidth(DestType);
                }

                int32x SourceStride = GetMemberUnitSize(SourceType);
                int32x DestStride  = GetMemberUnitSize(DestType);

                uint8 const *SourceMemberPtr = (uint8 const *)SourceMember;
                uint8 *DestMemberPtr = (uint8 *)DestMember;
                while(ArrayWidth--)
                {
                    switch(DestType.Type)
                    {
                        case Int8Member:
                        {
                            *(int8 *)DestMemberPtr = (int8)
                                ConvertMemberToInt32(SourceType.Type, SourceMemberPtr);
                        } break;

                        case UInt8Member:
                        {
                            *(uint8 *)DestMemberPtr = (uint8)
                                ConvertMemberToInt32(SourceType.Type, SourceMemberPtr);
                        } break;

                        case Int16Member:
                        {
                            *(int16 *)DestMemberPtr = (int16)
                                ConvertMemberToInt32(SourceType.Type, SourceMemberPtr);
                        } break;

                        case UInt16Member:
                        {
                            *(uint16 *)DestMemberPtr = (uint16)
                                ConvertMemberToInt32(SourceType.Type, SourceMemberPtr);
                        } break;

                        case Int32Member:
                        {
                            *(int32 *)DestMemberPtr =
                                ConvertMemberToInt32(SourceType.Type, SourceMemberPtr);
                        } break;

                        case UInt32Member:
                        {
                            *(uint32 *)DestMemberPtr = (uint32)
                                ConvertMemberToInt32(SourceType.Type, SourceMemberPtr);
                        } break;

                        case BinormalInt8Member:
                        {
                            *(int8 *)DestMemberPtr = (int8)
                                Normalize(
                                    ConvertMemberToReal32(SourceType.Type, SourceMemberPtr),
                                    Int8Maximum);
                        } break;

                        case NormalUInt8Member:
                        {
                            *(uint8 *)DestMemberPtr = (uint8)
                                Normalize(
                                    ConvertMemberToReal32(SourceType.Type, SourceMemberPtr),
                                    UInt8Maximum);
                        } break;

                        case BinormalInt16Member:
                        {
                            *(int16 *)DestMemberPtr = (int16)
                                Normalize(
                                    ConvertMemberToReal32(SourceType.Type, SourceMemberPtr),
                                    Int16Maximum);
                        } break;

                        case NormalUInt16Member:
                        {
                            *(uint16 *)DestMemberPtr = (uint16)
                                Normalize(
                                    ConvertMemberToReal32(SourceType.Type, SourceMemberPtr),
                                    UInt16Maximum);
                        } break;

                        case Real32Member:
                        {
                            *(real32 *)DestMemberPtr =
                                ConvertMemberToReal32(SourceType.Type, SourceMemberPtr);
                        } break;

                        case Real16Member:
                        {
                            *(real16 *)DestMemberPtr =
                                ConvertMemberToReal16(SourceType.Type, SourceMemberPtr);
                        } break;

                        default:
                        {
                            InvalidCodePath("Unrecognized data type member");
                        } break;
                    }

                    SourceMemberPtr += SourceStride;
                    DestMemberPtr += DestStride;
                }
            } break;
        }
    }
}

static bool
ConvertObjectInTree(conversion_context &Context,
                    data_type_definition const *SourceType,
                    void const *SourceObject,
                    data_type_definition const *DestType,
                    void*  WriteTo,
                    void*& Written);



// This converts the actual inline data in a structure.
// Inlined members (e.g. structs inside structs) are done
// by recursion.
static void
ConvertData(conversion_context &Context,
            data_type_definition const *SourceType,
            void const *SourceObject,
            data_type_definition const *DestType,
            void *DestObject)
{
    Assert ( DestObject != NULL );
    Assert ( Context.Writing );
    uint8 *DestMember = (uint8 *)DestObject;
    while(DestType && (DestType->Type != EndMember))
    {
        if(Context.ClearFields)
        {
            MakeEmptyDataTypeMember(*DestType, DestMember);
        }
        int32x const DestMemberSize = GetMemberTypeSize(*DestType);

        variant Source;
        if(FindMatchingMember(SourceType, SourceObject, DestType->Name, &Source))
        {
            Assert(Source.Object && Source.Type);

            switch(Source.Type->Type)
            {
                case InlineMember:
                {
                    int32x const SourceWidth = GetMemberArrayWidth(*Source.Type);
                    int32x const DestWidth   = GetMemberArrayWidth(*DestType);
                    int32x const UseWidth    = SourceWidth < DestWidth ? SourceWidth : DestWidth;

                    int32x const SourceTypeSize = GetTotalObjectSize(Source.Type->ReferenceType);
                    int32x const DestTypeSize   = GetTotalObjectSize(DestType->ReferenceType);
                    {for (int32x Idx = 0; Idx < UseWidth; ++Idx)
                    {
                        uint8 const* ArraySource = (uint8*)Source.Object + (Idx*SourceTypeSize);
                        uint8*       ArrayDest   = DestMember + (Idx*DestTypeSize);

                        ConvertData(Context,
                                    Source.Type->ReferenceType, ArraySource,
                                    DestType->ReferenceType, ArrayDest);
                    }}
                } break;

                default:
                {
                    CopyMember(Context, *Source.Type, Source.Object, *DestType, DestMember);
                } break;
            }
        }
        else
        {
            // Didn't find a matching source member. Maybe you want to convert something else to this type?
            if ( DataTypesAreEqualWithNames ( /*deconstify*/(data_type_definition *)DestType, Curve2Type ) )
            {
                // We're looking for a curve2. Maybe there's an old_curve around we can convert?
                // But actually, this routine does nothing.
                // All the hard work is done by ConvertPointers.
                // So we just skip it for now.
                return;
            }
        }

        DestMember += DestMemberSize;
        ++DestType;
    }

    return;
}


// This takes a converted structure and converts the things that hang off
// it with pointers, recursing where needed using ConvertObjectInTree
// (or just ConvertPointers for inline objects, since they will have
// already been handled by ConvertData).
static bool
ConvertPointers(conversion_context &Context,
                data_type_definition const *SourceType,
                void const *SourceObject,
                data_type_definition const *DestType,
                void *WriteTo)
{
    void *Cached = 0;
    if(SourceObject == 0 || GetHashedPointerData(*Context.PointerConvertHash, SourceObject, Cached))
    {
        return true;
    }
    SetHashedPointerData(*Context.PointerConvertHash, SourceObject, 0);

    data_type_definition const *OriginalDestType = DestType;
    void *OriginalWriteTo = WriteTo;

    uint8 *DestMember = (uint8 *)WriteTo;
    while(DestType && (DestType->Type != EndMember))
    {
        int32x const DestMemberSize = GetMemberTypeSize(*DestType);

        variant Source;
        FindMatchingMember(SourceType, SourceObject, DestType->Name, &Source);
        if(Source.Object && (Source.Type->Type == DestType->Type))
        {
            switch(DestType->Type)
            {
                case ReferenceMember:
                {
                    void *SourcePointer = *(void **)Source.Object;
                    void **DestPointer = GetPointer(Context, DestMember);

                    if(!DataTypesAreEqualAndBDoesntInclude(
                           Source.Type->ReferenceType,
                           DestType->ReferenceType,
                           Curve2Type))
                    {
                        if (ConvertObjectInTree(Context,
                                                Source.Type->ReferenceType,
                                                SourcePointer,
                                                DestType->ReferenceType, NULL,
                                                *DestPointer) == false)
                        {
                            return false;
                        }
                    }
                    else
                    {
                        if (Context.VariantReplacer)
                        {
                            // Even though the types are equal, we have to fully walk the
                            // tree, since there could be variants
                            // hiding out that we need to touch.

                            // Can't use DestType here, since the walking routine needs to access it...
                            if (ConvertPointers(Context,
                                                Source.Type->ReferenceType, SourcePointer,
                                                Source.Type->ReferenceType, *DestPointer) == false)
                            {
                                return false;
                            }

                        }
                    }
                } break;

                case VariantReferenceMember:
                {
                    if (Context.VariantReplacer)
                    {
                        data_type_definition*& TypePointer = *((data_type_definition**)(Source.Object));
                        void*&                 ObjPointer  = *((void**)(((uint8*)(Source.Object)) + SizeOf(void*)));
                        if (TypePointer && ObjPointer)
                        {
                            data_type_definition const** NewDestType =
                                (data_type_definition const**)GetPointer(Context, DestMember);

                            void** DestPointer = GetPointer(Context, DestMember + SizeOf(void*));

                            data_type_definition const* NewType = Context.VariantReplacer(TypePointer, ObjPointer);
                            if (NewType != 0 && !DataTypesAreEqualWithNames(TypePointer, NewType))
                            {
                                // Yup, we're going to replace it.

                                if (ConvertObjectInTree(Context,
                                                        TypePointer, ObjPointer,
                                                        NewType, NULL,
                                                        *DestPointer) == false)
                                {
                                    return false;
                                }

                                // Replace the type in the destination...
                                *NewDestType = NewType;
                            }
                            else
                            {
                                // Can't use DestType here, since the walking routine needs to access it...
                                if (ConvertPointers(Context, TypePointer, ObjPointer, TypePointer, *DestPointer) == false)
                                {
                                    return false;
                                }
                            }
                        }
                    }
                } break;

                case ReferenceToVariantArrayMember:
                {
                    // We don't copy variants anymore, we just point
                    // into the original data.
                } break;

                case EmptyReferenceMember:
                {
                    // Same deal for empty references
                } break;

                case ReferenceToArrayMember:
                {
                    if(!DataTypesAreEqualAndBDoesntInclude(
                           Source.Type->ReferenceType,
                           DestType->ReferenceType,
                           Curve2Type) || Context.VariantReplacer)
                    {
                        int32x SourceArrayWidth = GetMemberArrayWidth(*Source.Type);
                        int32x DestArrayWidth = GetMemberArrayWidth(*DestType);

                        // Handle this carefully for 64bit...
                        int32x Count;
                        uint8 const *Array;
                        ExtractCountAndPtr(Count, Array, uint8 const*, Source.Object);

                        if(Count && Array)
                        {
                            uint32x const SourceTypeSize =
                                GetTotalObjectSize(Source.Type->ReferenceType);
                            uint32x const DestTypeSize =
                                GetTotalObjectSize(DestType->ReferenceType);

                            // Even when Context.Writing==false, we need to do this so that
                            // GetSpace allocates the right amount of space.
                            uint8 *WriteArray = (uint8 *)
                                GetSpace(Context,
                                         Count * DestTypeSize * DestArrayWidth);

                            // Handle this carefully for 64bit...
                            void* Destination = GetPointer(Context, DestMember);
                            SetCountAndPtr(Count, WriteArray, void*, Destination);

                            while(Count--)
                            {
                                {for(int32x SourceIndex = 0;
                                     SourceIndex < SourceArrayWidth;
                                     ++SourceIndex)
                                {
                                    if(SourceIndex < DestArrayWidth)
                                    {
                                        void* Written = 0;
                                        if (ConvertObjectInTree(Context,
                                                                Source.Type->ReferenceType,
                                                                Array,
                                                                DestType->ReferenceType,
                                                                WriteArray, Written) == false)
                                        {
                                            return false;
                                        }

                                        // Should not have moved that...
                                        Assert(Written == WriteArray);

                                        WriteArray += DestTypeSize;
                                    }

                                    Array += SourceTypeSize;
                                }}
                            }
                        }
                    }
                } break;

                case ArrayOfReferencesMember:
                {
                    if(!DataTypesAreEqualAndBDoesntInclude(
                           Source.Type->ReferenceType,
                           DestType->ReferenceType,
                           Curve2Type) || Context.VariantReplacer)
                    {
                        int32x SourceArrayWidth = GetMemberArrayWidth(*Source.Type);
                        int32x DestArrayWidth = GetMemberArrayWidth(*DestType);

                        // Handle this carefully for 64bit...
                        int32x Count;
                        void const **Array;
                        ExtractCountAndPtr(Count, Array, void const**, Source.Object);

                        if(Count && Array)
                        {
                            // Even when Context.Writing==false, we need to do this so that
                            // GetSpace allocates the right amount of space.
                            void **WriteArray = (void **)
                                GetSpace(Context, SizeOf(void *) * Count * DestArrayWidth);

                            // Handle this carefully for 64bit...
                            void* Destination = GetPointer(Context, DestMember);
                            SetCountAndPtr(Count, WriteArray, void**, Destination);

                            while(Count--)
                            {
                                {for(int32x SourceIndex = 0;
                                     SourceIndex < SourceArrayWidth;
                                     ++SourceIndex)
                                {
                                    if(SourceIndex < DestArrayWidth)
                                    {
                                        void *NewPointer = 0;

                                        if(*Array)
                                        {
                                            if (ConvertObjectInTree(Context,
                                                                    Source.Type->ReferenceType,
                                                                    *Array, DestType->ReferenceType, 0,
                                                                    NewPointer) == false)
                                            {
                                                return false;
                                            }
                                        }

                                        if (Context.Writing)
                                        {
                                            *WriteArray = NewPointer;
                                        }

                                        ++WriteArray;
                                    }

                                    ++Array;
                                }}
                            }
                        }
                    }
                } break;

                case InlineMember:
                {
                    if(!DataTypesAreEqualAndBDoesntInclude(Source.Type->ReferenceType,
                                                           DestType->ReferenceType,
                                                           Curve2Type) ||
                       Context.VariantReplacer)
                    {
                        int32x const SourceWidth = GetMemberArrayWidth(*Source.Type);
                        int32x const SourceSize  = GetMemberUnitSize(*Source.Type);
                        int32x const DestWidth   = GetMemberArrayWidth(*DestType);
                        int32x const DestSize    = GetMemberUnitSize(*DestType);

                        int32x const UseWidth = SourceWidth < DestWidth ? SourceWidth : DestWidth;
                        {for (int32x Idx = 0; Idx < UseWidth; ++Idx)
                        {
                            uint8* SourceLoc = (uint8*)Source.Object + (Idx*SourceSize);
                            uint8* DestLoc   = (uint8*)DestMember    + (Idx*DestSize);
                            if(ConvertPointers(Context,
                                               Source.Type->ReferenceType, SourceLoc,
                                               DestType->ReferenceType, DestLoc) == false)
                            {
                                return false;
                            }
                        }}
                    }
                } break;

                case UnsupportedMemberType_Remove:
                {
                    InvalidCodePath("Switchable types no longer supported in Granny 2.7+");
                } break;

                default:
                {
                    // Nothing to do for most types
                } break;
            }
        }
        else
        {
            // Didn't find a matching source member. Maybe you want to convert something else to this type?
            if ( DataTypesAreEqualWithNames ( /*deconstify*/(data_type_definition *)DestType, Curve2Type ) )
            {
                // We're looking for a curve2. Maybe there's an old_curve around we can convert?
                // SUBTLETY: We're converting an old_curve into a
                // curve2 followed immediately by a curve_data_da_k32f_c32f, and the
                // converted pointer data needs to go into the latter.
                // So we actually need to do a full tree convert because we need the
                // pointers fixing up AND the curve data itself.
                // QUERY - can this be "unified" - not have two separate paths for writing/not writing?
                if ( !Context.Writing )
                {
                    // Not writing, just asking the size of data we need.
                    // The caller takes care of the size of the curve2 chunk of data, but
                    // it doesn't know what size the CurveData variant is,
                    // so we need to tell it there's going to be a curve_data_da_k32f_c32f
                    // after it.
                    GetSpace(Context, SizeOf(curve_data_da_k32f_c32f));
                    // And then do a pretend convert _to_ the old curve type, which will
                    // correctly allocate space for the Knots and Controls arrays.
                    if (ConvertPointers(Context, SourceType, SourceObject, OldCurveType, NULL) == false)
                    {
                        // coverage has a hard time getting here, but can fail
                        return false;
                    }

                    // And we're done here - we already know this whole thing was a curve2.
                    return true;
                }
                else
                {
                    // We are writing, so allocate space for the curve_data_da_k32f_c32f,
                    // then convert the data itself to a temp. place.
                    // The Knots and Controls arrays should be converted by this call too,
                    // but they will allocate their space using GetSpace, which is as it should be.
                    curve_data_da_k32f_c32f *CurveDataSpace = (curve_data_da_k32f_c32f *)GetSpace(Context, SizeOf(curve_data_da_k32f_c32f));
                    old_curve OldCurve;
                    void* Written = 0;
                    // QUERY - does this fall over because of the GetHashedPointerData check?
                    if (!ConvertObjectInTree ( Context, SourceType, SourceObject, OldCurveType, &OldCurve, Written ))
                    {
                        // coverage has a hard time getting here, but can fail
                        return false;
                    }
                    Assert(Written == &OldCurve);

#if DEBUG
                    // Some basic sanity checking. These are not strictly true, but
                    // should catch the crazier bugs.
                    Assert ( OldCurve.ControlCount >= 0 );
                    Assert ( OldCurve.KnotCount >= 0 );
                    Assert ( OldCurve.ControlCount < 0x10000 );
                    Assert ( OldCurve.KnotCount < 0x10000 );
                    if ( ( OldCurve.ControlCount > 0 ) && ( OldCurve.KnotCount > 0 ) )
                    {
                        int32x Dimension = OldCurve.ControlCount / OldCurve.KnotCount;
                        Assert ( Dimension <= MaximumBSplineDimension );
                        Assert ( OldCurve.KnotCount * Dimension == OldCurve.ControlCount );
                    }
#endif



                    // Now copy the data over and initialise the curve.
                    int32x Dimension = 0;
                    if ( OldCurve.KnotCount > 0 )
                    {
                        Dimension = OldCurve.ControlCount / OldCurve.KnotCount;
                    }

                    CurveMakeStaticDaK32fC32f ( (curve2*)DestMember, CurveDataSpace,
                                                OldCurve.KnotCount, OldCurve.Degree, Dimension,
                                                OldCurve.Knots, OldCurve.Controls );

                    // And we're done here - we already know this whole thing was a curve2.
                    return true;
                }
            }
        }

        DestMember += DestMemberSize;
        ++DestType;
    }


    // If this was a granny_curve2 type, then we need to fix up the format field.
    // The format enum may have changed in between versions.
    if ( Context.Writing )
    {
        if ( DataTypesAreEqualWithNames ( /*deconstify*/(data_type_definition *)OriginalDestType, Curve2Type ) )
        {
            CurveInitializeFormat ( (curve2 *)OriginalWriteTo );
        }
    }

    return true;
}

static void
RemapPointer(pointer_hash &Hash, void *&Pointer)
{
    if(Pointer != 0)
    {
        void *NewPointer = 0;
        if(GetHashedPointerData(Hash, Pointer, NewPointer))
        {
            Pointer = NewPointer;
        }
    }
}

static bool
RemapPointers(pointer_hash& AlreadyDone, pointer_hash &Hash, member_iterator &MemberIterator)
{
    if(TypeHasPointers(MemberIterator.AtType))
    {
        {for(;
             MemberIteratorIsValid(MemberIterator);
             AdvanceMemberIterator(MemberIterator))
        {
            if (HashedPointerKeyExists(AlreadyDone, MemberIterator.At))
                continue;

            if (AddPointerToHash(AlreadyDone, MemberIterator.At, 0) == false)
            {
                Log0(ErrorLogMessage, DataTypeLogMessage, "RemapPointers: unable to add pointer to hash");
                return false;
            }


            switch(MemberIterator.Type)
            {
                case ReferenceMember:
                case VariantReferenceMember:
                {
                    RemapPointer(Hash, *MemberIterator.Pointer);
                    member_iterator SubIterator;
                    IterateOverSubMembers(MemberIterator, SubIterator);
                    if (RemapPointers(AlreadyDone, Hash, SubIterator) == false)
                        return false;
                } break;

                case ReferenceToVariantArrayMember:
                case ReferenceToArrayMember:
                {
                    member_iterator ArrayIterator;
                    IterateOverSubArray(MemberIterator, ArrayIterator);
                    if(TypeHasPointers(ArrayIterator.AtType))
                    {
                        {for(;
                             MemberIteratorIsValid(ArrayIterator);
                             AdvanceMemberIterator(ArrayIterator))
                        {
                            member_iterator SubIterator;
                            IterateOverElement(ArrayIterator, SubIterator);

                            if (RemapPointers(AlreadyDone, Hash, SubIterator) == false)
                                return false;
                        }}
                    }
                } break;

                case ArrayOfReferencesMember:
                {
                    member_iterator ArrayIterator;
                    IterateOverSubArray(MemberIterator, ArrayIterator);
                    {for(;
                         MemberIteratorIsValid(ArrayIterator);
                         AdvanceMemberIterator(ArrayIterator))
                    {
                        member_iterator SubIterator;
                        IterateOverElement(ArrayIterator, SubIterator);
                        RemapPointer(Hash, *ArrayIterator.Pointer);

                        if (RemapPointers(AlreadyDone, Hash, SubIterator) == false)
                            return false;
                    }}
                } break;

                case InlineMember:
                {
                    member_iterator SubIterator;
                    IterateOverSubMembers(MemberIterator, SubIterator);

                    if (SubIterator.At == MemberIterator.At)
                    {
                        // We need to remove the at pointer from the already done bit.  This
                        // prevents stalling on the first element of the inline member, which
                        // is bad.
                        RemovePointerFromHash(AlreadyDone, MemberIterator.At);
                    }

                    if (RemapPointers(AlreadyDone, Hash, SubIterator) == false)
                        return false;

                    // Make sure it's there on the back end.  This might need to be an
                    // error
                    if (!HashedPointerKeyExists(AlreadyDone, MemberIterator.At))
                    {
                        if (AddPointerToHash(AlreadyDone, MemberIterator.At, 0) == false)
                            return false;
                    }
                } break;

                case UnsupportedMemberType_Remove:
                    InvalidCodePath("Switchable types no longer supported in Granny 2.7+");
                default:
                    // Nothing to do.
                    break;
            }
        }}
    }

    return true;
}



// Converts a single object and all sub-objects.
// This is often called twice. The first time, with
// Context.Writing = false, to find out how much space
// to allocate. The conversion is "thought of" in full,
// with the required space being stored in Context.Size.
// Then a second call is done with Context.Writing = true
// to actually perform the conversion.
// If WriteTo is NULL, this means memory has not yet
// been allocated for this object, and so this should do
// so. If it is not NULL, memory has already been
// allocated (e.g. members of an array), so it should
// use that memory.
// Note that the NULLness of WriteTo should be the same
// through both passes (Context.Writing=false and =true),
// so that GetSpace gets called the same way both times.

static bool
ConvertObjectInTree(conversion_context &Context,
                    data_type_definition const *SourceType,
                    void const *SourceObject,
                    data_type_definition const *DestType,
                    void*  WriteTo,
                    void*& Written)
{
    void *Cached = 0;
    if(SourceObject && !GetHashedPointerData(*Context.RemapHash, SourceObject, Cached))
    {
        if(!WriteTo)
        {
            // Even when Context.Writing==false, we need to do this so that
            // GetSpace allocates the right amount of space.
            WriteTo = GetSpace(Context, GetTotalObjectSize(DestType));
            Assert ( WriteTo != NULL );
            if (SetHashedPointerData(*Context.RemapHash, SourceObject, WriteTo) == false)
            {
                return false;
            }
        }

        if(Context.Writing)
        {
            ConvertData(Context, SourceType, SourceObject, DestType, WriteTo);
        }

        if (ConvertPointers(Context, SourceType, SourceObject,
                            DestType, WriteTo) == false)
        {
            return false;
        }

        Written = WriteTo;
    }
    else
    {
        Written = Cached;
    }


    return true;
}

void GRANNY
ConvertSingleObject(data_type_definition const *SourceType,
                    void const *SourceObject,
                    data_type_definition const *DestType,
                    void *DestObject,
                    conversion_handler* Handler)
{
    conversion_context Context;
    // QUERY: shouldn't this do InitializeContext(Context, DestObject)?
    //InitializeContext(Context, 0);
    InitializeContext(Context, DestObject, Handler, 0);

    ConvertData(Context, SourceType, SourceObject, DestType, DestObject);
}

void GRANNY
MergeSingleObject(data_type_definition const *SourceType,
                  void const *SourceObject,
                  data_type_definition const *DestType,
                  void *DestObject,
                  conversion_handler* Handler)
{
    conversion_context Context;
    // QUERY: shouldn't this do InitializeContext(Context, DestObject)?
    //InitializeContext(Context, 0);
    InitializeContext(Context, DestObject, Handler, 0);
    Context.ClearFields = false;

    ConvertData(Context, SourceType, SourceObject, DestType, DestObject);
}

bool GRANNY
RemoveMember(data_type_definition* Type,
             void*                 Object,
             char const*           MemberName)
{
    CheckPointerNotNull(Type, return false);
    CheckPointerNotNull(Object, return false);
    CheckPointerNotNull(MemberName, return false);

    variant Result;
    if (FindMatchingMember(Type, Object, MemberName, &Result) == false)
        return false;

    uint8* MoveTo   = (uint8*)Result.Object;
    uint8* MoveFrom = MoveTo + GetMemberTypeSize(*Result.Type);
    uint8* EndPtr   = (uint8*)Result.Object + GetTotalObjectSize(Type);

    int32x MoveSize;
    CheckConvertToInt32(MoveSize, EndPtr - MoveFrom, return false);
    Move(MoveSize, MoveFrom, MoveTo);

    while (Result.Type->Type != EndMember)
    {
        *Result.Type = *(Result.Type + 1);
        ++Result.Type;
    }

    return true;
}


void *GRANNY
ConvertTree(data_type_definition const *SourceType,
            void const *SourceTree,
            data_type_definition const *DestType,
            variant_replace_fn* VariantReplacer)
{
    GRANNY_AUTO_ZONE_FN();

    int32x const TreeSize = GetConvertedTreeSize(SourceType, SourceTree, DestType, VariantReplacer);
    if (TreeSize < 0)
    {
        Log0(ErrorLogMessage, DataTypeLogMessage, "ConvertTree: unable to get tree size");
        return 0;
    }

    void *DestTree = AllocateSize(TreeSize, AllocationFileData);
    if (DestTree)
    {
        if (ConvertTreeInPlace(SourceType, SourceTree,
                               DestType, DestTree,
                               VariantReplacer) == NULL)
        {
            // Failed in the conversion, the tree is garbage.  Nuke it and return 0
            Deallocate(DestTree);
            DestTree = 0;
        }
    }

    return DestTree;
}

int32x GRANNY
GetConvertedTreeSize(data_type_definition const *SourceType,
                     void const *SourceTree,
                     data_type_definition const *DestType,
                     variant_replace_fn* VariantReplacer)
{
    conversion_context Context;
    InitializeContext(Context, 0, 0, VariantReplacer);

    Context.RemapHash = NewPointerHash();
    Context.PointerConvertHash = NewPointerHash();
    if (Context.RemapHash == 0 || Context.PointerConvertHash == 0)
    {
        DeletePointerHash(Context.RemapHash);
        DeletePointerHash(Context.PointerConvertHash);
        Log0(ErrorLogMessage, DataTypeLogMessage, "GetConvertedTreeSize: unable to allocate pointer hash");
        return -1;
    }

    // false return from ConvertObjectInTree indicates internal failure.  Clean up the
    // hash and return -1 regardless of the computed value

    void* IgnoreWritten = 0;
    bool Success = ConvertObjectInTree(Context, SourceType, SourceTree, DestType, 0, IgnoreWritten);

    DeletePointerHash(Context.RemapHash);
    DeletePointerHash(Context.PointerConvertHash);
    return Success ? Context.Size : -1;
}

void *GRANNY
ConvertTreeInPlace(data_type_definition const *SourceType,
                   void const *SourceTree,
                   data_type_definition const *DestType,
                   void *Memory,
                   variant_replace_fn* VariantReplacer)
{
    conversion_context Context;
    InitializeContext(Context, Memory, 0, VariantReplacer);

    Context.RemapHash = NewPointerHash();
    Context.PointerConvertHash = NewPointerHash();
    Context.AlreadyDone = NewPointerHash();
    if (Context.RemapHash == 0 || Context.AlreadyDone == 0)
    {
        Log0(ErrorLogMessage, DataTypeLogMessage, "ConvertTreeInPlace: unable to allocate pointer hashes");
        DeletePointerHash(Context.RemapHash);
        DeletePointerHash(Context.AlreadyDone);
        return 0;
    }

    void* DestTree = 0;
    if (ConvertObjectInTree(Context, SourceType, SourceTree, DestType, 0, DestTree) == false)
    {
        DeletePointerHash(Context.RemapHash);
        DeletePointerHash(Context.PointerConvertHash);
        DeletePointerHash(Context.AlreadyDone);
        return 0;
    }

    Assert(Context.RemapHash && Context.AlreadyDone);
    {
        member_iterator Tree;
        IterateOverMembers(DestType, DestTree, Tree);
        if (RemapPointers(*Context.AlreadyDone, *Context.RemapHash, Tree) == false)
        {
            // Remap failed, which means that we probably ran out of memory adding
            // pointers to our hashes.  This is a garbage convert.
            Log0(ErrorLogMessage, DataTypeLogMessage, "ConvertTreeInPlace: pointer remap failed, probably out of memory");
            DestTree = 0;
        }
    }
    DeletePointerHash(Context.AlreadyDone);
    DeletePointerHash(Context.PointerConvertHash);
    DeletePointerHash(Context.RemapHash);

    return DestTree;
}

struct rebasing_node_entry
{
    void* BasePointer;
    int32 StoredData;
};

struct rebasing_context
{
    bool32 RebaseStrings;

    stack_allocator NodeEntries;
    rebase_pointers_string_callback *Callback;
    void *CallbackData;
};

static void
RebasePointer(void *&Pointer, intaddrx Offset)
{
    if(Pointer != 0)
    {
        *(uint8 **)&Pointer += Offset;
    }
}

static bool
AddressHasBeenTouched(rebasing_context& Context, void* Address, intaddrx StoreOffset)
{
    Assert(IS_ALIGNED_N(Address, 4));
    if (!Address)
        return true;

    int32* DataPointer = (int32*)((uint8*)Address + StoreOffset);
    int32  CurrentData = *DataPointer;

    if (CurrentData >= 0 && CurrentData < GetStackUnitCount(Context.NodeEntries))
    {
        // Check to see if the entry points back to this address
        rebasing_node_entry& Entry =
            STACK_UNIT_AS(Context.NodeEntries, CurrentData, rebasing_node_entry);

        return (Entry.BasePointer == DataPointer);
    }
    else
    {
        return false;
    }
}

static void
MarkAddressAsTouched(rebasing_context& Context, void* Address, intaddrx StoreOffset)
{
    Assert(AddressHasBeenTouched(Context, Address, StoreOffset) == false);
    Assert(IS_ALIGNED_N(Address, 4));
    Assert(IS_ALIGNED_N(StoreOffset, 4));

    if (!Address)
        return;

    int32* DataPointer = (int32*)((uint8*)Address + StoreOffset);
    int32  CurrentData = *DataPointer;

    int32x NewIndex;
    rebasing_node_entry* NewEntry =
        (rebasing_node_entry*)NewStackUnit(Context.NodeEntries, &NewIndex);
    Assert(NewEntry);

    NewEntry->BasePointer = DataPointer;
    NewEntry->StoredData  = CurrentData;
    *DataPointer          = NewIndex;
}


static void
RebaseType(rebasing_context &Context, data_type_definition *&Pointer, intaddrx Offset);

static void
RebaseTypeArrayPointers(rebasing_context &Context, data_type_definition *Array,
                        intaddrx Offset)
{
    if (AddressHasBeenTouched(Context, Array, OffsetFromType(data_type_definition, Extra[0])))
        return;

    // Use one of the Extra fields, since they're not used in the traversal...
    MarkAddressAsTouched(Context, Array, OffsetFromType(data_type_definition, Extra[0]));

    while(Array->Type != EndMember)
    {
        if(Context.RebaseStrings)
        {
            Assert(Array->Name);
            *(uint8 **)&Array->Name += Offset;
        }
        else if ( Context.Callback != NULL )
        {
            uintaddrx const Identifer = (uintaddrx)Array->Name;
            uint32 Ident32 = 0;
            CheckConvertToUInt32(Ident32, Identifer, break);

            Array->Name = Context.Callback ( Context.CallbackData, Ident32 );
        }

        if(Array->ReferenceType)
        {
            RebaseType(Context, Array->ReferenceType, Offset);
        }

        ++Array;
    }
}

static void
RebaseType(rebasing_context& Context, data_type_definition*& Pointer, intaddrx Offset)
{
    if(Pointer != 0)
    {
        *(uint8 **)&Pointer += Offset;
        RebaseTypeArrayPointers(Context, Pointer, Offset);
    }
}

// UnswapData/ReswapData
// These macros facilitate the process of temporarily restoring the first DWORD in the
// object so we can ensure that recursion happens as expected.
#define UnswapData(CurrPtr)                                                     \
    int32x SwapIndex = -1;                                                      \
    if (BaseAddressTouched && CurrPtr == BaseAddress)                           \
    {                                                                           \
        int32* DataPtr = (int32*)BaseAddress;                                   \
        SwapIndex = *DataPtr;                                                   \
        rebasing_node_entry& Entry =                                            \
            STACK_UNIT_AS(Context.NodeEntries, SwapIndex, rebasing_node_entry); \
        Assert(Entry.BasePointer == BaseAddress);                               \
        *DataPtr = Entry.StoredData;                                            \
    } typedef int __require_semicolon

#define ReswapData(CurrPtr)                     \
    if (SwapIndex != -1)                        \
    {                                           \
        int32* DataPtr = (int32*)BaseAddress;   \
        rebasing_node_entry& Entry =                                            \
            STACK_UNIT_AS(Context.NodeEntries, SwapIndex, rebasing_node_entry); \
        Entry.StoredData = *DataPtr;            \
        *DataPtr = SwapIndex;                   \
    } typedef int __require_semicolon

static void
RebasePointers(rebasing_context& Context,
               member_iterator& MemberIterator,
               intaddrx Offset,
               bool IgnoreNodeTouches,
               bool BaseAddressTouched)
{
    // NB: This is a really touchy bit of code.  Because of the way we're keeping track of
    // which objects have already been rebased, we have to shuffle pieces of memory back
    // and forth in a coordinated fashion.
    //
    // Briefly: when an address is visited for the first time, it's first DWORD is
    // examined to see if it is a valid index for the table of already rebased objects.
    // If so, and if that table entry points back to the same location, we know that it
    // has already been touched.
    //
    // The complication arises when we start to rebase the object for the first time.
    // Since their may be cyclic paths back to the object, we need to ensure that the
    // "touched" marker is valid whenever we recurse, but we may need the data that has
    // been swapped out to the NodeEntry table in order to recurse properly.  This is the
    // reason for the Unswap/Reswap dance.
    //
    // Further, inline members are a royal PITA, since they may start at Offset==0 in the
    // containing object.  This means that we have to /ignore/ the marker for "touched",
    // but still swap that first DWORD back and forth.  If the inline member starts at
    // Offset!=0, then we /still/ ignore the hashing, but we /don't/ Unswap/Reswap.  Fun!
    //
    // The complication here is justified by the fact that we don't need to keep track of
    //  a) a PointerHash tracking the touched objects, which is slower, and more memory
    //     intensive
    //  b) Side data transmitting the number or location of the fixed up edges.
    //
    if(TypeHasPointers(MemberIterator.AtType))
    {
        // Subtlety: There was a bug in the variant_builder that would
        // cause it to output a type/object pointer pair that pointed
        // to the _same_ object.  This is very bad, because then the
        // hash mapping would fuck up.  So, I now do the hash check
        // _after_ the TypeHasPointers check, because those types
        // had no members in them, and thus it wouldn't cause the problem.
        // So, that's why the hash check is inside rather than outside.
        uint8* BaseAddress = MemberIterator.At;
        if (!IgnoreNodeTouches)
        {
            if (AddressHasBeenTouched(Context, BaseAddress, 0))
                return;
            MarkAddressAsTouched(Context, BaseAddress, 0);
        }

        {for(;
             MemberIteratorIsValid(MemberIterator);
             AdvanceMemberIterator(MemberIterator))
        {
            switch(MemberIterator.Type)
            {
                case ReferenceMember:
                case VariantReferenceMember:
                {
                    member_iterator SubIterator;
                    UnswapData(MemberIterator.At);
                    {
                        if(MemberIterator.Type == VariantReferenceMember)
                        {
                            RebaseType(Context, *MemberIterator.PointerType, Offset);
                        }

                        RebasePointer(*MemberIterator.Pointer, Offset);
                        IterateOverSubMembers(MemberIterator, SubIterator);
                    }
                    ReswapData(MemberIterator.At);

                    RebasePointers(Context, SubIterator, Offset, false, true);
                } break;

                case ReferenceToVariantArrayMember:
                case ReferenceToArrayMember:
                {
                    member_iterator ArrayIterator;
                    UnswapData(MemberIterator.At);
                    {
                        if(MemberIterator.Type == ReferenceToVariantArrayMember)
                        {
                            RebaseType(Context, *MemberIterator.PointerType, Offset);
                        }

                        RebasePointer(*MemberIterator.ArrayPointer, Offset);
                        IterateOverSubArray(MemberIterator, ArrayIterator);
                    }
                    ReswapData(MemberIterator.At);

                    if(TypeHasPointers(ArrayIterator.AtType))
                    {
                        {for(;
                             MemberIteratorIsValid(ArrayIterator);
                             AdvanceMemberIterator(ArrayIterator))
                        {
                            member_iterator SubIterator;
                            IterateOverElement(ArrayIterator, SubIterator);
                            RebasePointers(Context, SubIterator, Offset, false, true);
                        }}
                    }
                } break;

                case ArrayOfReferencesMember:
                {
                    member_iterator ArrayIterator;
                    UnswapData(MemberIterator.At);
                    {
                        RebasePointer(*MemberIterator.ArrayPointer, Offset);
                        IterateOverSubArray(MemberIterator, ArrayIterator);
                    }
                    ReswapData(MemberIterator.At);

                    {for(;
                         MemberIteratorIsValid(ArrayIterator);
                         AdvanceMemberIterator(ArrayIterator))
                    {
                        RebasePointer(*ArrayIterator.Pointer, Offset);

                        member_iterator SubIterator;
                        IterateOverElement(ArrayIterator, SubIterator);
                        RebasePointers(Context, SubIterator, Offset, false, true);
                    }}
                } break;

                case InlineMember:
                {
                    member_iterator SubIterator;
                    IterateOverSubMembers(MemberIterator, SubIterator);
                    if (TypeHasPointers(SubIterator.AtType))
                    {
                        // Inline members are such a PITA.  On recursion, we only have to
                        // unswap data for this if the inline member is the /first/
                        // structure in the containing member.  We still ignore the
                        // hashing on entry.
                        RebasePointers(Context, SubIterator, Offset,
                                       true, MemberIterator.At == BaseAddress);
                    }
                } break;

                case StringMember:
                {
                    UnswapData(MemberIterator.At);
                    if(Context.RebaseStrings)
                    {
                        RebasePointer(*MemberIterator.Pointer, Offset);
                    }
                    else if ( Context.Callback != NULL )
                    {
                        uintaddrx const Identifer = *(uintaddrx *)MemberIterator.Pointer;
                        uint32 Ident32 = 0;
                        CheckConvertToUInt32(Ident32, Identifer, break);

                        *MemberIterator.Pointer = Context.Callback ( Context.CallbackData, Ident32 );
                    }
                    ReswapData(MemberIterator.At);

                } break;

                case UnsupportedMemberType_Remove:
                    InvalidCodePath("Switchable types no longer supported in Granny 2.7+");
                default:
                    // Nothing to do.
                    break;
            }
        }}
    }
}


static bool
RebasePointersInternal(data_type_definition const *Type,
                       void *Data, intaddrx Offset,
                       bool RebaseStrings,
                       rebase_pointers_string_callback *Callback,
                       void *CallbackData)
{
    GRANNY_AUTO_ZONE_FN();
    bool Result = false;

    rebasing_context Context;
    Context.RebaseStrings = RebaseStrings;
    Context.Callback = Callback;
    Context.CallbackData = CallbackData;
    StackInitialize(Context.NodeEntries, sizeof(rebasing_node_entry), 1024);
    {
        member_iterator Tree;
        IterateOverMembers(Type, Data, Tree);

        ::RebasePointers(Context, Tree, Offset, false, true);

        // Loop over the NodeEntries and put back the data
        int32x NumEntries = GetStackUnitCount(Context.NodeEntries);
        while (NumEntries-- > 0)
        {
            rebasing_node_entry& Entry =
                STACK_UNIT_AS(Context.NodeEntries, NumEntries, rebasing_node_entry);

            Assert(*((int32*)Entry.BasePointer) == NumEntries);
            *((int32*)Entry.BasePointer) = Entry.StoredData;
            PopStackUnits(Context.NodeEntries, 1);
        }

        Result = true;
    }
    StackCleanUp(Context.NodeEntries);

    return(Result);
}

bool GRANNY
RebasePointers(data_type_definition const *Type, void *Data,
               intaddrx Offset, bool RebaseStrings)
{
    return RebasePointersInternal ( Type, Data, Offset, RebaseStrings, NULL, NULL );
}

bool GRANNY
RebasePointersStringCallback(data_type_definition const *Type, void *Data,
                             intaddrx Offset, rebase_pointers_string_callback *Callback, void *CallbackData)
{
    return RebasePointersInternal ( Type, Data, Offset, false, Callback, CallbackData );
}


