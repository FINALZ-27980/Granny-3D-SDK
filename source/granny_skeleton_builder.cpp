// ========================================================================
// $File: //jeffr/granny_29/rt/granny_skeleton_builder.cpp $
// $DateTime: 2012/06/04 11:43:59 $
// $Change: 37681 $
// $Revision: #3 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_skeleton_builder.h"

#include "granny_aggr_alloc.h"
#include "granny_math.h"
#include "granny_matrix_operations.h"
#include "granny_memory_arena.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"
#include "granny_skeleton.h"
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
#define SubsystemCode SkeletonLogMessage

USING_GRANNY_NAMESPACE;

BEGIN_GRANNY_NAMESPACE;

struct skeleton_builder_bone
{
    int32x      Parent;
    char const* Name;
    transform   LocalTransform;

    matrix_4x4  InverseWorld4x4;

    real32 LODError;
};

struct skeleton_builder
{
    int32x MaxBoneCount;
    int32x BoneCount;
    skeleton_builder_bone *Bones;
    skeleton_lod_type LODType;

    int32x* BoneWritten;
};

END_GRANNY_NAMESPACE;

static void
EstimateSkeletonLOD(skeleton_builder& Builder)
{
    // Alloc our temp space
    memory_arena* LODArena = NewMemoryArena();
    real32* ComputedLOD = (real32*)MemoryArenaPush(*LODArena, Builder.BoneCount * sizeof(real32));
    {for(int32x BoneIndex = 0; BoneIndex < Builder.BoneCount; ++BoneIndex)
    {
        ComputedLOD[BoneIndex] = -1.0f;
    }}

    // Our braindead estimate of this parameter is as follows.  Find
    //  the furthest child bone, and set our error to twice that
    //  distance.  If we wind up with no children, then set our
    //  parameter to half that of our parent.
    // Note that we really need to look at the bounding information to
    //  do this properly, but at this stage, we don't have that info.
    //  Note that if the user has set a lod manually, then we always
    //  use that.  This allows the creation of proper post-processing
    //  tools that take the bounding/vertex data into account.  (See the
    //  preprocessor for an example.)
    // Note also we enforce the parent.error >= child.error condition.

    // These loops probably do unecessary work, but we can't count on
    //  the topo sort property yet.

    // initial computation
    {for(int32x BoneIndex = 0; BoneIndex < Builder.BoneCount; ++BoneIndex)
    {
        triple CurrOffset;
        Copy32(3, Builder.Bones[BoneIndex].LocalTransform.Position, CurrOffset);

        // Walk up the skeleton, and set the parent errors...
        int Walk = Builder.Bones[BoneIndex].Parent;
        while (Walk != -1)
        {
            Assert(Walk >= 0 && Walk < Builder.BoneCount);

            // First, see if this offset causes us to exceed the
            //  currently computed LOD parameter
            const real32 EstError = VectorLength3(CurrOffset);
            if (EstError > ComputedLOD[Walk])
            {
                ComputedLOD[Walk] = EstError;
            }

            // Transform CurrOffset and step to parent
            TransformVectorInPlace(CurrOffset, Builder.Bones[Walk].LocalTransform);
            Walk = Builder.Bones[Walk].Parent;
        }
    }}

    // Copy to the bones, but only if no explicitly set parameter is
    //  in place...
    {for(int32x BoneIndex = 0; BoneIndex < Builder.BoneCount; ++BoneIndex)
    {
        if (Builder.Bones[BoneIndex].LODError < 0.0f)
        {
            Builder.Bones[BoneIndex].LODError = ComputedLOD[BoneIndex];
        }
        else
        {
            // If the user sets a parameter, we assume that this is measured
            // lod.
            Builder.LODType = MeasuredLOD;
        }
    }}

    // Ensure heap property, and catch the leaf bones.
    {for(int32x BoneIndex = 0; BoneIndex < Builder.BoneCount; ++BoneIndex)
    {
        if (Builder.Bones[BoneIndex].LODError < 0.0f)
        {
            if (Builder.Bones[BoneIndex].Parent == -1)
            {
                // Singleton bone, no parent, no children.  There's nothing good about
                //  this situation, turn off the LOD for the skeleton entirely.
                //  Otherwise, you can have "insta-lod" portions of the skeleton.  For
                //  these situations, Anim LOD is the only solution.
                //
                // Note that we'll continue to process the skeleton to ensure somewhat
                // sensible values, however.
                Builder.LODType = NoSkeletonLOD;
            }
            else
            {
                int ParentIndex = Builder.Bones[BoneIndex].Parent;
                Assert(ParentIndex >= 0 && ParentIndex < Builder.BoneCount);

                // Half the parent
                Builder.Bones[BoneIndex].LODError = Builder.Bones[ParentIndex].LODError / 2;
            }
        }

        // And traverse up for the heap-property
        int Walk = BoneIndex;
        while (Builder.Bones[Walk].Parent != -1)
        {
            int ParentIndex = Builder.Bones[Walk].Parent;
            Assert(ParentIndex >= 0 && ParentIndex < Builder.BoneCount);


            if (Builder.Bones[Walk].LODError > Builder.Bones[ParentIndex].LODError)
                Builder.Bones[ParentIndex].LODError = Builder.Bones[Walk].LODError;

            Walk = ParentIndex;
        }
    }}

    // Clear our temp mem
    FreeMemoryArena(LODArena);
}

// TODO: swap out for insertion sort...
// Yeah, that's right, a bubble sort.  It's not quite as crazy as you
// think, this allows us to avoid the CRT or STL, and the bone array
// is mostly sorted anways, since we enforce the topo sort constraint
// that parents come before children, and a parent's lod must exceed
// that of all of its child nodes.  Note that we do a lot of
// superflous loops here that could be combined, but I want this to be
// super clear about what's going on.
static void
BubbleSortOnLOD(skeleton_builder& Builder,
                bone*             Bones,
                int32x*           RemappingTable)
{
#define BUBBLE_SWAP(type, a, b) do { type Temp = a; a = b; b = Temp; } while (false)

    // Allocate our temp buffers
    memory_arena* SortArena = NewMemoryArena();
    int32x* OldToNewMap = (int32x*)MemoryArenaPush(*SortArena, Builder.BoneCount * sizeof(int32x));
    int32x* NewToOldMap = (int32x*)MemoryArenaPush(*SortArena, Builder.BoneCount * sizeof(int32x));

    {for(int BoneIndex = 0; BoneIndex < Builder.BoneCount; ++BoneIndex)
    {
        OldToNewMap[BoneIndex] = -1;

        // Note that NTO[i] = i to begin
        NewToOldMap[BoneIndex] = BoneIndex;
    }}

    bone* OldBones = (bone*)MemoryArenaPush(*SortArena, Builder.BoneCount * sizeof(bone));
    Copy(sizeof(bone) * Builder.BoneCount, Bones, OldBones);

    {for(int i = Builder.BoneCount - 1; i >= 0; --i)
    {
        {for(int j = 0; j <= i - 1; ++j)
        {
            if (Bones[j].LODError < Bones[j + 1].LODError)
            {
                // Swap the elements that we care about
                BUBBLE_SWAP(int32x, NewToOldMap[j], NewToOldMap[j + 1]);
                BUBBLE_SWAP(bone,   Bones[j], Bones[j+1]);
            }
        }}
    }}

    // Invert the new-to-old map to get our proper ordering...
    {for(int i = 0; i < Builder.BoneCount; ++i)
    {
        OldToNewMap[NewToOldMap[i]] = i;
    }}

    if (RemappingTable)
    {
        // Remap the Remapping table.  Note that we're going to use the
        // NewToOldMap as temp space, since we're done with it.
        {for(int i = 0; i < Builder.BoneCount; ++i)
        {
            RemappingTable[i] = OldToNewMap[RemappingTable[i]];
        }}
    }

    // And clean up the parent indices...
    {for(int BoneIndex = 0; BoneIndex < Builder.BoneCount; ++BoneIndex)
    {
        if (Bones[BoneIndex].ParentIndex != NoParentBone)
        {
            int32x OldParentIndex = Bones[BoneIndex].ParentIndex;
            int32x NewParentIndex = OldToNewMap[OldParentIndex];

            Bones[BoneIndex].ParentIndex = NewParentIndex;
        }
    }}

#ifdef DEBUG
    // Do some checking...
    {for(int BoneIndex = 0; BoneIndex < Builder.BoneCount; ++BoneIndex)
    {
        if (BoneIndex != 0)
        {
            Assert(Bones[BoneIndex-1].LODError >= Bones[BoneIndex].LODError);
        }

        if (Bones[BoneIndex].ParentIndex != NoParentBone)
        {
            Assert(Bones[BoneIndex].ParentIndex >= 0 &&
                   Bones[BoneIndex].ParentIndex < Builder.BoneCount);
            Assert(Bones[BoneIndex].ParentIndex < BoneIndex);
        }
    }}
#endif // DEBUG

    // Clear our temp mem
    FreeMemoryArena(SortArena);

#undef BUBBLE_SWAP
}


skeleton_builder *GRANNY
BeginSkeleton(int32x BoneCount)
{
    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    skeleton_builder *Builder;
    AggrAllocPtr(Allocator, Builder);
    AggrAllocOffsetArrayPtr(Allocator, Builder, BoneCount, MaxBoneCount, Bones);
    AggrAllocOffsetArrayPtr(Allocator, Builder, BoneCount, MaxBoneCount, BoneWritten);
    if(EndAggrAlloc(Allocator, AllocationBuilder))
    {
        Builder->BoneCount = 0;
        Builder->LODType = EstimatedLOD;

        {for(int32x BoneIndex = 0;
             BoneIndex < BoneCount;
             ++BoneIndex)
        {
            Builder->BoneWritten[BoneIndex] = -1;
        }}
    }

    return(Builder);
}

skeleton *GRANNY
EndSkeleton(skeleton_builder *Builder, int32x *RemappingTable)
{
    skeleton *Skeleton = 0;

    if(Builder)
    {
        int32x TotalSize = GetResultingSkeletonSize(*Builder);
        Skeleton = EndSkeletonInPlace(Builder, AllocateSize(TotalSize, AllocationBuilder),
                                      RemappingTable);
    }

    return(Skeleton);
}

int32x GRANNY
GetResultingSkeletonSize(skeleton_builder const& Builder)
{
    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    skeleton *Ignored;
    AggrSkeleton(Allocator, Builder.BoneCount, Ignored);

    // Count up the size of the bone names.  Here, we don't collapse similar strings, let
    // the file io handle that...
    char* StringBufferDummy;
    {
        int StringSize = 0;
        for (int Idx = 0; Idx < Builder.BoneCount; ++Idx)
        {
            if (Builder.Bones[Idx].Name)
                StringSize += StringLength(Builder.Bones[Idx].Name) + 1;
        }
        AggrAllocSizePtr(Allocator, StringSize, StringBufferDummy);
    }

    int32x ResultingSize;
    CheckConvertToInt32(ResultingSize, EndAggrSize(Allocator), return 0);
    return ResultingSize;
}

static void
WriteBone(skeleton_builder& Builder,
          int32x            InBoneIndex,
          int32x&           OutBoneIndex,
          bone*             BoneData,
          char*&            StringBuffer,
          int32x*           RemappingTable)
{
    if(Builder.BoneWritten[InBoneIndex] == -2)
    {
        Log1(ErrorLogMessage, SkeletonLogMessage,
             "Circular parenting chain located at bone %d",
             InBoneIndex);
        return;
    }

    if(Builder.BoneWritten[InBoneIndex] == -1)
    {
        Builder.BoneWritten[InBoneIndex] = -2;

        int32x ParentIndex = NoParentBone;
        skeleton_builder_bone const& Bone = Builder.Bones[InBoneIndex];
        if(Bone.Parent != NoParentBone)
        {
            WriteBone(Builder,
                      Bone.Parent,
                      OutBoneIndex,
                      BoneData,
                      StringBuffer,
                      RemappingTable);
            Assert(Builder.BoneWritten[Bone.Parent] != -1);
            ParentIndex = Builder.BoneWritten[Bone.Parent];
        }

        MakeEmptyDataTypeObject(BoneType, &BoneData[OutBoneIndex]);

        // Copy the bone name into the string buffer, if it's non-null.  Advance the
        // buffer pointer for the calling function
        if (Bone.Name != 0)
        {
            int32x const CopyLength = StringLength(Bone.Name) + 1;

            BoneData[OutBoneIndex].Name  = StringBuffer;
            StringBuffer                += CopyLength;

            Copy(CopyLength, Bone.Name, (char*)BoneData[OutBoneIndex].Name);
        }

        BoneData[OutBoneIndex].ParentIndex    = ParentIndex;
        BoneData[OutBoneIndex].LODError       = Bone.LODError;
        BoneData[OutBoneIndex].LocalTransform = Bone.LocalTransform;

        Copy(sizeof(Bone.InverseWorld4x4),
             Bone.InverseWorld4x4,
             BoneData[OutBoneIndex].InverseWorld4x4);

        if(RemappingTable)
        {
            RemappingTable[InBoneIndex] = OutBoneIndex;
        }

        Builder.BoneWritten[InBoneIndex] = OutBoneIndex;
        ++OutBoneIndex;
    }
}

skeleton *GRANNY
EndSkeletonInPlace(skeleton_builder *Builder,
                   void *InMemory,
                   int32x *RemappingTable)
{
    skeleton *Skeleton = 0;

    if(Builder)
    {
        // Estimate the skeleton lod parameters.  Note that we'll use
        // that parameter to sort the skeleton later
        EstimateSkeletonLOD(*Builder);

        aggr_allocator Allocator;
        InitializeAggrAlloc(Allocator);

        AggrSkeleton(Allocator, Builder->BoneCount, Skeleton);

        int32x StringSize  = 0;
        char* StringBuffer = 0;
        {
            // See GetResultingSkeletonSize
            for (int Idx = 0; Idx < Builder->BoneCount; ++Idx)
            {
                if (Builder->Bones[Idx].Name)
                    StringSize += StringLength(Builder->Bones[Idx].Name) + 1;
            }
            AggrAllocSizePtr(Allocator, StringSize, StringBuffer);
        }

        if(EndAggrPlacement(Allocator, InMemory))
        {
            char* StringBufferWrite = StringBuffer;

            // Techinically, we could allow the loop below to write these bones out in the
            // course of the recursive WriteBone calls, but this gets all the parents to
            // the top of the tree by default, which is where they belong, for now.
            // (We'll possibly spoil that ordering by sorting on skeleton lod)
            int32x OutBoneIndex = 0;
            {for(int32x BoneIndex = 0;
                 BoneIndex < Skeleton->BoneCount;
                 ++BoneIndex)
            {
                skeleton_builder_bone &Bone = Builder->Bones[BoneIndex];

                if(Bone.Parent == -1)
                {
                    WriteBone(*Builder,
                              BoneIndex,
                              OutBoneIndex,
                              Skeleton->Bones,
                              StringBufferWrite,
                              RemappingTable);
                }
            }}

            {for(int32x BoneIndex = 0;
                 BoneIndex < Skeleton->BoneCount;
                 ++BoneIndex)
            {
                skeleton_builder_bone &Bone = Builder->Bones[BoneIndex];

                if(Bone.Parent != -1)
                {
                    WriteBone(*Builder,
                              BoneIndex,
                              OutBoneIndex,
                              Skeleton->Bones,
                              StringBufferWrite,
                              RemappingTable);
                }
            }}
            Assert(OutBoneIndex == Skeleton->BoneCount);
            Assert(StringBufferWrite == StringBuffer + StringSize);

            BubbleSortOnLOD(*Builder, Skeleton->Bones, RemappingTable);
            Skeleton->LODType = Builder->LODType;
            Skeleton->ExtendedData.Type = 0;
            Skeleton->ExtendedData.Object = 0;
        }

        Deallocate(Builder);
    }

    return(Skeleton);
}

void GRANNY
AddBoneWithInverse(skeleton_builder& Builder,
                   char const*       Name,
                   real32 const*     LocalPosition,
                   real32 const*     LocalOrientation,
                   real32 const*     LocalScaleShear,
                   matrix_4x4 const* InverseWorld4x4)
{
    if(Builder.BoneCount < Builder.MaxBoneCount)
    {
        int32x BoneIndex = Builder.BoneCount++;
        skeleton_builder_bone &Bone = Builder.Bones[BoneIndex];
        Bone.Name = Name;
        Bone.Parent = -1;
        Bone.LODError = -1.0f;
        SetTransformWithIdentityCheck(Bone.LocalTransform,
                                      LocalPosition, LocalOrientation,
                                      LocalScaleShear);

        // Copy the inverse transform.  Note that we can't check the
        //  inverse precision here, since we don't have the forward
        //  transform
        Copy(sizeof(Bone.InverseWorld4x4),
             InverseWorld4x4,
             Bone.InverseWorld4x4);
    }
    else
    {
        Log0(ErrorLogMessage, SkeletonLogMessage,
             "AddBone called after all allotted bones were already used");
    }
}

bool GRANNY
AddBone(skeleton_builder& Builder,
        char const*       Name,
        real32 const*     LocalPosition,
        real32 const*     LocalOrientation,
        real32 const*     LocalScaleShear,
        real32 const*     WorldPosition,
        real32 const*     WorldOrientation,
        real32 const*     WorldScaleShear)
{
    real32 World[4][4];
    transform WorldTransform;
    matrix_4x4 InverseWorld4x4;
    SetTransformWithIdentityCheck(WorldTransform,
                                  WorldPosition, WorldOrientation,
                                  WorldScaleShear);
    BuildCompositeTransform4x4(WorldTransform, (real32 *)World);
    MatrixInvert4x3((real32 *)InverseWorld4x4,
                    (real32 const *)World);

    AddBoneWithInverse(Builder, Name,
                       LocalPosition, LocalOrientation, LocalScaleShear,
                       (matrix_4x4 const*)&InverseWorld4x4);

    // Check the inverse precision...
#if DEBUG
    real32 Test1[4][4], Test2[4][4];
    BuildCompositeTransform4x4(WorldTransform,
                               (real32 *)Test1);
    ColumnMatrixMultiply4x3Impl(
        (real32 *)Test2,
        (real32 const *)Test1,
        (real32 const *)InverseWorld4x4);

    real32 TotalError = 0.0f;
    {for(int32x Row = 0;
         Row < 4;
         ++Row)
    {
        {for(int32x Column = 0;
             Column < 4;
             ++Column)
        {
            TotalError += AbsoluteValue(GlobalIdentity4x4[Row][Column] -
                                        Test2[Row][Column]);
        }}
    }}

    if(TotalError > 0.0001f)
    {
        Log0(WarningLogMessage, SkeletonLogMessage,
             "Matrix inverse is not sufficiently precise");
        return false;
    }
#endif

    return true;
}

void GRANNY
SetBoneParent(skeleton_builder &Builder, int32x BoneIndex, int32x ParentIndex)
{
    if(BoneIndex < Builder.MaxBoneCount)
    {
        if(ParentIndex < Builder.MaxBoneCount)
        {
            Builder.Bones[BoneIndex].Parent = ParentIndex;
        }
        else
        {
            Log2(ErrorLogMessage, SkeletonLogMessage,
                 "SetBoneParent parent index (%d) is out of specified bone "
                 "domain (%d)", BoneIndex, Builder.MaxBoneCount);
        }
    }
    else
    {
        Log2(ErrorLogMessage, SkeletonLogMessage,
             "SetBoneParent bone index (%d) is out of specified bone "
             "domain (%d)", BoneIndex, Builder.MaxBoneCount);
    }
}


void GRANNY
SetBoneLODError(skeleton_builder &Builder,
                int32x BoneIndex,
                real32 LODError)
{
    if(BoneIndex < Builder.MaxBoneCount)
    {
        if(BoneIndex < Builder.MaxBoneCount)
        {
            Builder.Bones[BoneIndex].LODError = LODError;
        }
        else
        {
            Log2(ErrorLogMessage, SkeletonLogMessage,
                 "SetBoneParent parent index (%d) is out of specified bone "
                 "domain (%d)", BoneIndex, Builder.MaxBoneCount);
        }
    }
    else
    {
        Log2(ErrorLogMessage, SkeletonLogMessage,
             "SetBoneParent bone index (%d) is out of specified bone "
             "domain (%d)", BoneIndex, Builder.MaxBoneCount);
    }
}
