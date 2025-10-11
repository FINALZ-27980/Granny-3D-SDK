// ========================================================================
// $File: //jeffr/granny_29/rt/granny_deformers.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_deformers.h"

#include "granny_accelerated_deformers.h"
#include "granny_data_type_definition.h"
#include "granny_generic_deformers.h"
#include "granny_log.h"
#include "granny_memory.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

USING_GRANNY_NAMESPACE;

int32x const MaxBoneDeformerTableCount = 64;
int32x BoneDeformerTableCount = 0;
bone_deformer BoneDeformerTable[MaxBoneDeformerTableCount];

bool GRANNY
FindBoneDeformerFor(data_type_definition const *SourceLayout,
                    data_type_definition const *DestLayout,
                    deformation_type Type,
                    bool IgnoreMismatchedTail,
                    bone_deformer &Result,
                    bone_deformer_parameters &Parameters)
{
    if(BoneDeformerTableCount == 0)
    {
        AddAcceleratedDeformers();
        AddGenericDeformers();
    }

    {for(int32x DeformerIndex = 0;
         DeformerIndex < BoneDeformerTableCount;
         ++DeformerIndex)
    {
        bone_deformer &Deformer = BoneDeformerTable[DeformerIndex];

        if(Deformer.Type == Type)
        {
            // These are common to all of the success paths.  We'll
            // just do this here, if the compare fails, we return
            // failure with the bool return value.
            Result = Deformer;
            Parameters.SourceVertexSize = GetTotalObjectSize(SourceLayout);
            Parameters.DestVertexSize = GetTotalObjectSize(DestLayout);

            if(Deformer.CanDoTailCopies)
            {
                // This deformer can handle tail copies
                data_type_definition *SourceCopy =
                    DataTypeBeginsWith(SourceLayout, Deformer.FromLayout);
                data_type_definition *DestCopy =
                    DataTypeBeginsWith(DestLayout, Deformer.ToLayout);

                if(SourceCopy && DestCopy)
                {
                    if(GetTotalObjectSize(SourceCopy) ==
                       GetTotalObjectSize(DestCopy))
                    {
                        Parameters.TailCopy32Count = GetTotalObjectSize(SourceCopy);

                        if((Parameters.TailCopy32Count % SizeOf(uint32)) == 0)
                        {
                            Parameters.TailCopy32Count /= SizeOf(uint32);

                            if(!DataTypesAreEqual(SourceCopy, DestCopy))
                            {
                                Log0(WarningLogMessage, DeformerLogMessage,
                                     "Using deformer whose tail copy size matches, "
                                     "but whose layout may not be correct.");
                            }

                            return(true);
                        }
                    }
                    else if (Deformer.CanIgnoreTailItems && IgnoreMismatchedTail)
                    {
                        data_type_definition *DestUncopiedTail =
                            DataTypeBeginsWith(SourceCopy, DestCopy);
                        if (DestUncopiedTail)
                        {
                            Parameters.TailCopy32Count =
                                GetTotalObjectSize(DestCopy) - GetTotalObjectSize(DestUncopiedTail);
                            if (Parameters.TailCopy32Count < 0)
                            {
                                Parameters.TailCopy32Count = 0;
                                return true;
                            }
                            else if((Parameters.TailCopy32Count % SizeOf(uint32)) == 0)
                            {
                                Parameters.TailCopy32Count /= SizeOf(uint32);
                                return true;
                            }
                            else
                            {
                                // Failure, tail copy must be a factor of 4
                            }
                        }
                        else
                        {
                            // Pretty convincingly mismatched.  This was explicitly
                            // requested, though, so there should be no need to log.
                            Parameters.TailCopy32Count = 0;
                            return true;
                        }
                    }
                }
            }
            else
            {
                // This deformer can't handle tail copies
                if(DataTypesAreEqual(SourceLayout, Deformer.FromLayout) &&
                   DataTypesAreEqual(DestLayout, Deformer.ToLayout))
                {
                    Parameters.TailCopy32Count = 0;
                    return(true);
                }
            }
        }
    }}

    return(false);
}

void GRANNY
AddBoneDeformer(bone_deformer &Deformer)
{
    Assert(BoneDeformerTableCount < MaxBoneDeformerTableCount);
    BoneDeformerTable[BoneDeformerTableCount++] = Deformer;
}

void GRANNY
AddBoneDeformerTable(int32x Count, bone_deformer *Deformer)
{
    Assert(BoneDeformerTableCount < MaxBoneDeformerTableCount);
    while(Count--)
    {
        AddBoneDeformer(*Deformer++);
    }
}
