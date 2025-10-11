#if !defined(GRANNY_MODEL_CONTROL_BINDING_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_model_control_binding.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

#if !defined(GRANNY_ASSERT_H)
#include "granny_assert.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(ControlGroup);

struct transform;
struct control;
struct model_instance;
struct local_pose;
struct world_pose;
struct controlled_animation;

EXPTYPE struct model_control_binding;
EXPTYPE struct spu_controlled_animation;
EXPTYPE struct controlled_pose;

typedef bool initialize_binding_state(model_control_binding &Binding,
                                      void *InitData);
typedef void accumulate_binding_state(model_control_binding &Binding,
                                      int32x const FirstBone, int32x const BoneCount,
                                      local_pose &Result, real32 AllowedError,
                                      int32x const *SparseBoneArray);
typedef void build_binding_direct(model_control_binding &Binding,
                                  int32x const BoneCount,
                                  real32 const *Offset4x4,
                                  world_pose &Result,
                                  real32 AllowedError);
typedef void accumulate_loop_transform(model_control_binding &Binding,
                                       real32 SecondsElapsed,
                                       real32 &TotalWeight,
                                       real32 *ResultTranslation,
                                       real32 *ResultRotation,
                                       bool Inverse);
typedef void cleanup_binding_state(model_control_binding &Binding);
typedef controlled_animation *get_controlled_animation ( model_control_binding &Binding );
typedef controlled_pose *get_controlled_pose ( model_control_binding &Binding );
typedef spu_controlled_animation* get_spu_controlled_animation ( model_control_binding &Binding );
typedef bool control_is_rebased ( model_control_binding &Binding );
typedef bool control_is_mirrored ( model_control_binding &Binding );

struct model_control_callbacks
{
    get_controlled_animation *GetControlledAnimation;
    get_controlled_pose *GetControlledPose;
    get_spu_controlled_animation* GetSPUControlledAnimation;  // note: going with the callback for this once
                                                              // more, but another control type means switching to
                                                              // an enum/cast sceme to avoid silliness.
    initialize_binding_state *InitializeBindingState;
    accumulate_binding_state *AccumulateBindingState;
    build_binding_direct *BuildBindingDirect;
    accumulate_loop_transform *AccumulateLoopTransform;
    cleanup_binding_state *CleanupBindingState;
    control_is_rebased*  ControlIsRebased;
    control_is_mirrored* ControlIsMirrored;
};

// This enumeration value is stored in the ReservedPointer member of the
// model_control_binding to indicate what sort of binding it is.  This is slightly
// suspect, since technically, the Callbacks member servse the same purpose, but we need a
// method that doesn't depend on structure addresses to work across the PPU/SPU boundary
// on the PS3.
enum model_control_binding_type
{
    ControlledPose    = 0,
    ControlledAnim    = 1,
    SPUControlledAnim = 2,

    model_control_binding_type_forceint = 0x7fffffff
};

struct model_control_binding
{
    // This is the ring of bindings that share the same control
    control *Control;
    model_control_binding *ControlPrev;
    model_control_binding *ControlNext;

    // This is the ring of bindings that share the same model
    model_instance *ModelInstance;
    model_control_binding *ModelPrev;
    model_control_binding *ModelNext;

    model_control_callbacks const *Callbacks;

    // This is where either a controlled_pose or a controlled_animation structure live.
    // Note that this field must be larger on 64 bit platforms to accommodate the larger
    //  pointer sizes
    uint32 Derived[8 * (SizeOf(void*) / SizeOf(uint32))];

    // See note on binding_type above...
    intaddrx BindingType;
};
CompileAssert(IS_ALIGNED_16(sizeof(model_control_binding)));

void UnlinkModelControlBinding(model_control_binding &Binding);
void LinkModelControlBinding(model_control_binding &Binding,
                             control &Control,
                             model_instance &ModelInstance,
                             bool Active);
void RelinkModelSideOfControlBinding(model_control_binding &Binding,
                                     bool Active);

void InitializeSentinel(model_control_binding &Binding);

model_control_binding *CreateModelControlBinding(
    model_control_callbacks const &Callbacks,
    control &Control, model_instance &ModelInstance,
    bool Active, void *InitData);
void FreeModelControlBinding(model_control_binding *Binding);

void SampleModelControlBindingLODSparse(model_control_binding &Binding,
                                        int32x FirstBone, int32x BoneCount,
                                        local_pose &Result, real32 AllowedError,
                                        int32x const *SparseBoneArray);
void BuildBindingDirect(model_control_binding &Binding,
                        int32x BoneCount,
                        real32 const *Offset4x4,
                        world_pose &Result,
                        real32 AllowedError);
void AccumulateModelControlBindingLoopTransform(
    model_control_binding &Binding, real32 SecondsElapsed,
    real32 &TotalWeight, real32 *ResultTranslation, real32 *ResultRotation, bool Inverse);

void FreeControlRing(model_control_binding &Sentinel);
void FreeModelRing(model_control_binding &Sentinel);

EXPAPI GS_READ model_control_binding &ModelControlsBegin(model_instance &Model);
EXPAPI GS_READ model_control_binding &ModelControlsNext(model_control_binding &Binding);
EXPAPI GS_READ model_control_binding &ModelControlsEnd(model_instance &Model);

EXPAPI GS_READ model_control_binding &ControlModelsBegin(control &Control);
EXPAPI GS_READ model_control_binding &ControlModelsNext(model_control_binding &Binding);
EXPAPI GS_READ model_control_binding &ControlModelsEnd(control &Control);

EXPAPI GS_READ model_instance *GetModelInstanceFromBinding(model_control_binding &Binding);
EXPAPI GS_READ control *GetControlFromBinding(model_control_binding &Binding);


#define VAR_FROM_DERIVED(type, VarName, Binding)        \
    Assert(SizeOf(type) <= SizeOf((Binding).Derived));  \
    type* VarName = (type*)(void*)&(Binding).Derived

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_MODEL_CONTROL_BINDING_H
#endif
