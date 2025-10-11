#if !defined(GRANNY_DAG_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

#if !defined(GRANNY_QUATERNION_MODE_H)
#include "granny_quaternion_mode.h"
#endif

#if !defined(GRANNY_POSE_CACHE_H)
#include "granny_pose_cache.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(DagGroup);

struct local_pose;
struct model_instance;
struct skeleton;
struct track_mask;

EXPAPI typedef CALLBACK_FN(local_pose *) blend_dag_leaf_callback_sample_callback(
    void *UserData,
    int32x BoneCount,
    real32 AllowedError,
    int32x const *SparseBoneArray);

EXPAPI typedef CALLBACK_FN(void) blend_dag_leaf_callback_set_clock_callback(
    void *UserData,
    real32 NewClock);

EXPAPI typedef CALLBACK_FN(void) blend_dag_leaf_callback_motion_vectors_callback(
    void *UserData,
    real32 SecondsElapsed,
    real32 *ResultTranslationVec3,
    real32 *ResultRotationVec3,
    bool Inverse);


// A leaf that has multiple granny_animations blending onto it.
// Really just a granny_model_instance in a fancy wrapper.
struct blend_dag_leaf_animation_blend
{
    model_instance* ModelInstance;
    real32          FillThreshold;
    bool32          AutoFreeModelInstance;
};

// A leaf that is an app-supplied local pose.
// No data here - the LocalPose in the main body of blend_dag_node just has to be set up by the user manually,
// and be valid when calling any of the SampleBlendDagXXX functions.
struct blend_dag_leaf_local_pose
{
    local_pose* LocalPose;
    bool32      AutoFreeLocalPose;
};

// A user-supplied callback to fetch a local_pose from the user.
struct blend_dag_leaf_callback
{
    blend_dag_leaf_callback_sample_callback *SampleCallback;
    blend_dag_leaf_callback_set_clock_callback *SetClockCallback;
    blend_dag_leaf_callback_motion_vectors_callback *MotionVectorsCallback;
    void *UserData;
};

// A node that takes two blend_dag_node inputs and blends them
// with a single track mask and crossfade value, like using GrannyModulationCompositeLocalPose().
// The number of children must be two, and two shall be the number of children.
// One shalt thou not have, excepting thou go on to add another, neither
// shalt thou have three. Four is right out.
// The weights and track masks in the children are ignored.
struct blend_dag_node_crossfade
{
    real32 WeightNone;
    real32 WeightAll;
    // TrackMask may be NULL, in which case 1.0 is assumed (i.e. WeightAll for everything).
    track_mask* TrackMask;
    bool32 AutoFreeTrackMask;
};

// A node that takes multiple blend_dag_node inputs and blends them
// with track masks and weights, much like animations playing on a model_instance,
// except that the inputs are not animations but other nodes.
struct blend_dag_node_weighted_blend
{
    // The reference skeleton and quat mode for doing the blending.
    skeleton* Skeleton;
    quaternion_mode QuatMode;
    real32 FillThreshold;
};


EXPTYPE enum blend_dag_node_type
{
    // These are "leaf" types - the tree ends here.

    DagNodeType_Leaf_AnimationBlend,
    DagNodeType_Leaf_LocalPose,
    DagNodeType_Leaf_Callback,

    DagNodeType_OnePastLastLeafType,        // Separates leaves from nodes.

    // These are "node" types - the tree continues because they take the output of other DAG nodes as their inputs.

    DagNodeType_Node_Crossfade,
    DagNodeType_Node_WeightedBlend,


    DagNodeType_OnePastLast,         // Always just before the force int

    // TODO: Types to add:

    // * A bone remapper - basically a rebinding from one skeleton to another.
    //   So you supply two models and the node's children must spit out data in one model's bones, and it converts it to another model's bones.
    // * An IK targetter/fixup.
    // * A targetted multi-blend. Feed it a bunch of animations and a property X for each,
    //   and tell it to blend them so that you get a user-specified property X at runtime.
    //   So the simple form is that the property is the end position of a reach animation, and
    //   the value you feed it is where the object actually is, and it lerps the anims together
    //   to actually reach the object (probably). If X is more complex and represents moods,
    //   then it becomes the whole animation-by-example thing. Probably just start with something simple
    //   like a 2D value (essentially a direction within a hemisphere) - useful for aiming guns, etc.
    // * Speed blender. Feed it multiple walk/run cycles, it matches up the lengths and then
    //   plays back at a pre-determined speed, using a targetted multi-blend to pick the most appropriate one.
    // * Splitter - so that a single node can feed its output to two or more places (i.e. makes into a proper DAG, not just a tree)
    // * A child-bearing Callback type. So it's a Callback, but people can use the built-in child-minding functionality.
    // * Joint slerper. e.g. at an elbow, the artist creates a bone whose orientation should be slerped between the two main bones,
    //   to fix up the vertices on the elbow. You can do this conventionally, but it means storing another track in
    //   the animation for each one, and on consoles, memory is pretty scarce. Much better to play the anim and
    //   then do the slerp in code (might also be cheaper than looking up the motion in the spline curve!)
    // * Muscle deformer. Similar to the joint slerper - but takes the dot-product (or something) of two bones
    //   and sets its XYZ scales accordingly to make a muscle "bulge" according to movement.
    // * A "blend language" interpreter (compiler?) to run general cases of the above stuff (probably use LUA or something small and neat and known like that).

    blend_dag_node_type_forceint = 0x7fffffff
};



EXPTYPE struct blend_dag_node;

struct blend_dag_node
{
    blend_dag_node *ParentNode;

    // The array of child node pointers. This is only used for "node" types, not "leaf" types.
    int32x NumChildNodes;
    blend_dag_node **ChildNodes;


    // The weight and track_mask for use when the _parent_ node is of type WeightedBlend (and possibly others in the future).
    // TrackMask may be NULL, in which case the identity track mask is assumed.
    real32 Weight;
    track_mask* TrackMask;
    bool32 AutoFreeTrackMask;

    // TODO: Ease in/out curves?

    blend_dag_node_type Type;
    // Depending on Type, this union is one of the following.
    // Please please please use the GetBlendDagNodeXXX functions rather than looking directly,
    // because they will do Asserts and suchlike just to make sure.
    // If this were C++ I could hide them properly, but ... it isn't.
    union
    {
        blend_dag_leaf_animation_blend      AnimationBlend;
        blend_dag_leaf_local_pose           LocalPose;
        blend_dag_leaf_callback             Callback;
        blend_dag_node_crossfade            Crossfade;
        blend_dag_node_weighted_blend       WeightedBlend;
    } TypeUnion;
};



EXPAPI GS_PARAM bool IsBlendDagLeafType(blend_dag_node const *Node);
EXPAPI GS_PARAM blend_dag_node_type GetBlendDagNodeType(blend_dag_node const *Node);

// The core functions.
EXPAPI GS_MODIFY blend_dag_node *CreateBlendDagNodeAnimationBlend(model_instance* ModelInstance, bool AutoFree);
EXPAPI GS_MODIFY blend_dag_node *CreateBlendDagNodeLocalPose(local_pose* LocalPose, bool AutoFree);
EXPAPI GS_MODIFY blend_dag_node *CreateBlendDagNodeCallback(blend_dag_leaf_callback_sample_callback *SampleCallback,
                                                            blend_dag_leaf_callback_set_clock_callback *SetClockCallback,
                                                            blend_dag_leaf_callback_motion_vectors_callback *MotionVectorsCallback,
                                                            void *UserData);

EXPAPI GS_MODIFY blend_dag_node *CreateBlendDagNodeCrossfade(blend_dag_node *DagNode0,
                                                             blend_dag_node *DagNode1,
                                                             real32 WeightNone, real32 WeightAll,
                                                             track_mask* TrackMask, bool AutoFreeTrackMask,
                                                             int32x BoneCount);

EXPAPI GS_MODIFY blend_dag_node *CreateBlendDagNodeWeightedBlend(
    skeleton *ReferenceSkeleton, real32 FillThreshold,
    quaternion_mode QuaternionBlendingMode, int32x BoneCount);

EXPAPI GS_MODIFY void FreeBlendDagNode(blend_dag_node *Node);

EXPAPI GS_MODIFY void SetBlendDagNodeAnimationBlend(
    blend_dag_node *Node, model_instance* ModelInstance,
    real32 FillThreshold, bool AutoFree);

EXPAPI GS_MODIFY void SetBlendDagNodeLocalPose(blend_dag_node *Node, local_pose* LocalPose, bool AutoFree);
EXPAPI GS_MODIFY void SetBlendDagNodeCallbacks(blend_dag_node *Node,
                                               blend_dag_leaf_callback_sample_callback *SampleCallback,
                                               blend_dag_leaf_callback_set_clock_callback *SetClockCallback,
                                               blend_dag_leaf_callback_motion_vectors_callback *MotionVectorsCallback,
                                               void *UserData);
EXPAPI GS_MODIFY void SetBlendDagNodeCrossfade(blend_dag_node *Node,
                                               real32 WeightNone, real32 WeightAll,
                                               track_mask* TrackMask,
                                               bool AutoFreeTrackMask);
EXPAPI GS_MODIFY void SetBlendDagNodeCrossfadeWeights(blend_dag_node *Node,
                                                      real32 WeightNone,
                                                      real32 WeightAll);
EXPAPI GS_MODIFY void SetBlendDagNodeWeightedBlend(blend_dag_node *Node,
                                                   skeleton *ReferenceSkeleton,
                                                   real32 FillThreshold,
                                                   quaternion_mode QuaternionBlendingMode);

EXPAPI GS_READ model_instance* GetBlendDagNodeAnimationBlend(blend_dag_node *Node);
EXPAPI GS_READ local_pose* GetBlendDagNodeLocalPose(blend_dag_node *Node);
EXPAPI GS_READ blend_dag_leaf_callback_sample_callback *GetBlendDagNodeCallbackSampleCallback(blend_dag_node *Node);
EXPAPI GS_READ blend_dag_leaf_callback_set_clock_callback *GetBlendDagNodeCallbackSetClockCallback(blend_dag_node *Node);
EXPAPI GS_READ blend_dag_leaf_callback_motion_vectors_callback *GetBlendDagNodeCallbackMotionVectorsCallback(blend_dag_node *Node);
EXPAPI GS_READ void *GetBlendDagNodeCallbackUserData(blend_dag_node *Node);
EXPAPI GS_READ track_mask* GetBlendDagNodeCrossfadeTrackMask(blend_dag_node *Node);
EXPAPI GS_READ bool GetBlendDagNodeCrossfadeWeights(blend_dag_node const *Node,
                                                    real32* WeightNone,
                                                    real32* WeightAll);
EXPAPI GS_READ skeleton *GetBlendDagNodeWeightedBlendSkeleton(blend_dag_node *Node);

EXPAPI GS_MODIFY void BlendDagNodeAnimationBlendFreeCompletedControls(blend_dag_node *Node);

EXPAPI GS_MODIFY void ClearBlendDagNodeChildren(blend_dag_node *Node);
EXPAPI GS_READ int32x GetBlendDagNodeChildrenCount(blend_dag_node const *Node);
EXPAPI GS_MODIFY void SetBlendDagNodeChildren(blend_dag_node *Node, int32x NumChildren, blend_dag_node **ArrayOfChildren);
EXPAPI GS_MODIFY void SetBlendDagNodeChild(blend_dag_node *Node, int32x ChildNumber, blend_dag_node *Child);
EXPAPI GS_READ blend_dag_node *GetBlendDagNodeChild(blend_dag_node *Node, int32x ChildNumber);
EXPAPI GS_MODIFY void AddBlendDagNodeChild(blend_dag_node *Node, blend_dag_node *Child);
EXPAPI GS_MODIFY void RemoveBlendDagNodeChild(blend_dag_node *Node, blend_dag_node *Child);

EXPAPI GS_READ blend_dag_node *GetBlendDagNodeParent(blend_dag_node *Node);

EXPAPI GS_MODIFY void SetBlendDagNodeResultTrackMask(blend_dag_node *Node, track_mask* TrackMask,bool AutoFree);
EXPAPI GS_READ track_mask* GetBlendDagNodeResultTrackMask(blend_dag_node *Node);
EXPAPI GS_READ void SetBlendDagNodeResultWeight(blend_dag_node *Node, real32 Weight);
EXPAPI GS_READ real32 GetBlendDagNodeResultWeight(blend_dag_node const *Node);



EXPAPI GS_READ void GetBlendDagTreeMotionVectors(blend_dag_node const *Node,
                                                 real32 SecondsElapsed,
                                                 real32 *ResultTranslation,
                                                 real32 *ResultRotation,
                                                 bool Inverse);

EXPAPI GS_READ void UpdateBlendDagTreeMatrix(blend_dag_node const *Node,
                                             real32 SecondsElapsed,
                                             real32 const *ModelMatrix4x4,
                                             real32 *DestMatrix4x4,
                                             bool Inverse);

EXPAPI GS_PARAM local_pose* SampleBlendDagTreeLODSparseReentrant(blend_dag_node const *RootNode,
                                                                 int32x BoneCount,
                                                                 real32 AllowedError,
                                                                 int32x const *SparseBoneArray,
                                                                 pose_cache *PoseCache);

inline
EXPAPI GS_MODIFY local_pose* SampleBlendDagTreeReentrant(blend_dag_node const *RootNode,
                                                         int32x BoneCount,
                                                         pose_cache *PoseCache)
{
    return SampleBlendDagTreeLODSparseReentrant( RootNode, BoneCount, 0.0f, NULL, PoseCache);
}

inline
EXPAPI GS_MODIFY local_pose* SampleBlendDagTreeLODReentrant(blend_dag_node const *RootNode,
                                                            int32x BoneCount,
                                                            real32 AllowedError,
                                                            pose_cache *PoseCache)
{
    return SampleBlendDagTreeLODSparseReentrant( RootNode, BoneCount,
                                                 AllowedError, NULL,
                                                 PoseCache );
}


EXPAPI GS_MODIFY void SetBlendDagTreeClock(blend_dag_node *RootNode, real32 NewClock);


local_pose* SampleBlendDagTreeInternal(bool *ResultIsScratch,
                                       blend_dag_node const *RootNode,
                                       int32x BoneCount,
                                       real32 AllowedError,
                                       int32x const *SparseBoneArray,
                                       pose_cache *PoseCache);



// Various utility wrappers for the above.
EXPAPI GS_READ void GetBlendDagNodeChildren(blend_dag_node *Node,
                                            int32x MaxArraySize,
                                            blend_dag_node **ArrayOfChildren);
EXPAPI GS_MODIFY blend_dag_node *CreateBlendDagNodeWeightedBlendChildren(
    skeleton *ReferenceSkeleton,
    real32 FillThreshold,
    quaternion_mode QuaternionBlendingMode,
    int32x BoneCount,
    int32x NumChildren, blend_dag_node **ArrayOfChildren);

EXPAPI GS_MODIFY void FreeBlendDagEntireTree(blend_dag_node *RootNode);

EXPAPI GS_MODIFY void BlendDagFreeCompletedControlsEntireTree(blend_dag_node *Node);



// Duplicates an entire tree, creating new model_instance objects (from the supplied model) as necessary.
// Returns the new root node.
EXPAPI GS_MODIFY blend_dag_node *DuplicateBlendDagTree(blend_dag_node const *SourceTreeRoot,
                                                       blend_dag_node const **SourceNodeList,
                                                       blend_dag_node **DestNodeList,
                                                       int32 SizeOfNodeList,
                                                       bool AutoFreeCreatedModelInstances);



// Handy debug functions
// Checks the validity of the node.
EXPAPI GS_READ bool IsBlendDagNodeValid(blend_dag_node const *Node,
                                        char const** ReasonFailed,
                                        blend_dag_node const **NodeFailed);
// Traverses the entire tree and checks its validity. It will terminate on the first error it finds and give some sort of feedback.
EXPAPI GS_READ bool IsBlendDagTreeValid(blend_dag_node const *RootNode,
                                        char const** ReasonFailed,
                                        blend_dag_node const **NodeFailed);


EXPAPI GS_READ int32x FindBlendDagTreeDepth(blend_dag_node const *RootNode);


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_DAG_H
#endif
