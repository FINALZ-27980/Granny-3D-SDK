// =====================================================================
// GrannyPhysX: A helper layer for integrating Granny with the Ageia
//  PhysX SDK.
//
// Contributors
// ------------
//  Dave Moore:    davem at radgametools.com
//  John Ratcliff: jratcliff at ageia.com
//
// Major changes
// -------------
//  2010/5/19: Adding joint drive utilities
//  2010/5/10: Fixes for the joint mappings
//  2009/9/02: Updated for new PhysX toolset
//  2006/5/24: Initial Revision
//
// Summary
// -------
//  This is intended to be a razor-thin layer that interfaces a
//  collection of PhysX actors to a granny_model_instance.  The
//  intention is to provide a tool sufficient to integrate simple
//  ragdoll animation and physical feedback into your animation
//  system, and to provide enough access that more advanced effects
//  (Animation driven forces applied to models for collision feedback,
//  etc) can be either added as extensions to this interface, or
//  supported as a higher-level layer in your application.
//
//  To keep the mapping between Ageia and Granny relatively simple, we
//  make a few assumptions about the granny_model_instance and the
//  NxuPhysicsCollection.  Primarily, we're assuming that
//   - Every actor in the collection is significant (they're all added
//     to the scene unconditionally.)
//   - granny_bones are related to NxActors by name correspondence.
//   - Not more than one actor is present for any given bone.
//   - The NxActors are in the same space as the granny_model_instance.
//     I.e, if you have changed the coordinate system of the source
//     model by GrannyTransformFile or another similar API call, you
//     have to ensure that the NxActors are in the same space.
//   - The GrannyPhysX assumes that it owns all pointers to objects
//     instantiated from the PhysicsCollection.  It assumes that the
//     SDK, Scene, and model_instance objects will be valid as long as
//     the mapping exists, but it doesn't assume ownership of those
//     objects, so you must clean them up yourself.  There's no need
//     to cleanup the actor and constraint objects, they're deleted in
//     grp_releaseMapping.
//   - The error detection model is to assert in debug, and treat
//     release builds as "rails-off".  Calling getActor with a
//     nonsense index will assert in debug, and most likely simply
//     crash in release.  You can replace the assert macro with
//     something more appropriate in this file if you'd like to handle
//     release-mode errors in a different way.
//
//  For some games these assumptions may be just fine.  If not, we've
//  tried to keep the API small enough that you can modify it to
//  support whatever constraints are necessary for your application
//  without too much effort.  Please write us if you feel that there
//  are pieces missing from the API, and especially write us if you
//  find better ways to do the same things!
//
// Todo
// ----
//  - How do we attach mulitple NxActors to a bone?  Might want this
//    for complicated ragdolls.  Is it worth trying to fit that into
//    this layer?
//  - Should we provide an interface that takes a list of NxActors,
//    rather than an NxuPhysicsCollection as well?
//
// Copyright
// ---------
//
// =====================================================================
#if !defined(GRANNYPHYSX_H)

// Granny structures
struct granny_model_instance;
struct granny_local_pose;
struct granny_world_pose;

// Ageia interfaces
class NxPhysicsSDK;
class NxScene;
class NxActor;
class NxJoint;
class NxMat34;

namespace NXU
{
    class NxuPhysicsCollection;
}

// Mapping handle
class GrannyPhysX;

enum GrannyPhysX_Constants
{
    GRP_NO_MAPPING = -1,
    GRP_MAX_BONENAME_LEN = 256
};

typedef bool grp_mapNameActorToBone(char const*   ActorName,
                                    char*         BoneNameBuffer,
                                    granny_int32x BoneNameBufferSize);


// =====================================================================
// Creates a mapping object creating a correspondence between the
// actors specified by the collection and the granny_model_instance.
// All parameters are required to be non-NULL.
GrannyPhysX* grp_createMapping(NXU::NxuPhysicsCollection*  Collection,
                               NxPhysicsSDK*               SDK,
                               NxScene*                    Scene,
                               granny_model_instance*      ModelInstance,
                               grp_mapNameActorToBone*     NameMap = 0);

// =====================================================================
// Frees resources associated with the mapping.  See the notes in the
// file header for which objects the Mapping owns, and which it
// assumes you'll be cleaning up yourself.
void grp_releaseMapping(GrannyPhysX* Mapping);

// =====================================================================
// Copy the transforms from the given world_pose to the physical
// actors.  NxActor::setGlobalPose is interface used, so this amounts
// to a warp to the given position.
bool grp_setWorldPose(GrannyPhysX*             Mapping,
                      granny_world_pose const* WorldPose);

// =====================================================================
// Warp a single bone to it's position in the world_pose using
// NxActor::setGlobalPose
bool grp_setWorldPoseForBone(GrannyPhysX*             Mapping,
                             granny_world_pose const* WorldPose,
                             int const                BoneIndex);

// =====================================================================
// Get a matrix representing the position of the corresponding actor
// from the input world_pose
bool grp_getActorMatrixForBone(GrannyPhysX*             Mapping,
                               granny_world_pose const* WorldPose,
                               int const                BoneIndex,
                               NxMat34*                 Matrix);

// =====================================================================
// Get a matrix representing the position of the corresponding actor
// from the input world_pose
bool grp_getActorMatrixForActor(GrannyPhysX*             Mapping,
                                granny_world_pose const* WorldPose,
                                int const                ActorIndex,
                                NxMat34*                 Matrix);

// =====================================================================
// Kill the velocity of a mapped scene
bool grp_zeroVelocity(GrannyPhysX* Mapping);

// =====================================================================
// Move a single bone to it's position in the world_pose using
// NxActor::moveGlobalPose
bool grp_moveWorldPoseForBone(GrannyPhysX*             Mapping,
                              granny_world_pose const* WorldPose,
                              int const                BoneIndex);

// =====================================================================
// Extracts a world_pose from the current state of the physical
// actors.  Uses the LocalPose parameter to create reasonable
// transforms for bones in the model's skeleton that don't have
// physical analogues.  If you want the unspecified bones to receive
// their rest pose transform, simply copy the LocalTransform member
// from the granny_bones to the local_pose.
//
// Note that this function doesn't give you back a local_pose that
// represents the physical state of the skeleton.  Once you have a
// valid world_pose, you can call GrannyLocalPoseFromWorldPose to
// create a local_pose that you can use to blend with existing
// animations.
bool grp_getWorldPose(GrannyPhysX*             Mapping,
                      granny_world_pose*       WorldPose,
                      granny_local_pose const* LocalPose,
                      float const*             Offset4x4);

// =====================================================================
// Yields the number of physical actors controlled by the mapping
int grp_getNumActors(GrannyPhysX* Mapping);

// =====================================================================
// Retrieves a specific actor.
NxActor* grp_getActor(GrannyPhysX* Mapping, int const ActorIndex);

// =====================================================================
// Retrieves a specific actor, or GRP_NO_MAPPING if the actor isn't
// controlled by this mapping
int grp_getActorIndex(GrannyPhysX* Mapping, NxActor* Actor);

// =====================================================================
// Retrieves a actor by name in the mapping.  Case senstive string
// test used, returns NULL when the actor isn't found.
NxActor* grp_findActorByName(GrannyPhysX* Mapping, const char* ActorName);

// =====================================================================
// Retrieves the actor that corresponds to the given bone.  Returns
// NULL if no such corresponce exists.
NxActor* grp_findActorForBone(GrannyPhysX* Mapping, int const BoneIndex);


// =====================================================================
// Yields the number of physical actors controlled by the mapping
int grp_getNumJoints(GrannyPhysX* Mapping);

// =====================================================================
// Retrieves a specific joint
NxJoint* grp_getJoint(GrannyPhysX* Mapping, int const JointIndex);



NXU::NxuPhysicsCollection * grp_loadCollection(const char *fname); // loads a .SSA file and returns the 'NxuPhysicsCollection'
void                   grp_releaseCollection(NXU::NxuPhysicsCollection *collection);

#define GRANNYPHYSX_H
#endif
