// =====================================================================
// GrannyPhysX: A helper layer for integrating Granny with the Ageia
//  PhysX SDK.
//
// See header file for detailed commentary
// =====================================================================
#include "granny.h"

#include "GrannyPhysX.h"
#include "Nx.h"
#include "Nxf.h"
#include "NXU_Streaming.h"
#include "NXU_Helper.h"


#if !NOGRANNY

#include <vector>

static bool
DefaultMapNameActorToBone(char const*   ActorName,
                          char*         BoneNameBuffer,
                          granny_int32x BoneNameBufferSize)
{
    if (ActorName == 0)
        return false;

    if (BoneNameBuffer == 0)
        return false;

    if (BoneNameBuffer == 0)
        return false;

    if ((int)strlen(ActorName) >= BoneNameBufferSize)
        return false;

    strcpy_s(BoneNameBuffer, BoneNameBufferSize, ActorName);
    return true;
}



// Our mapping object.  Implementation of the non-trivial functions
// can be found below the forwarding layer at the bottom of this file.
class GrannyPhysX : public NXU_userNotify, public NXU_errorReport
{
public:
    GrannyPhysX();
    ~GrannyPhysX();
    bool IsValid() const;

    bool createActorBinding(NXU::NxuPhysicsCollection* Collection,
                            NxPhysicsSDK*              SDK,
                            NxScene*                   Scene,
                            granny_model_instance*     ModelInstance,
                            grp_mapNameActorToBone*    NameMap);

    bool zeroVelocity();

    bool setWorldPose(granny_world_pose const* WorldPose);

    bool setWorldPoseForBone(granny_world_pose const* WorldPose,
                             int const                BoneIndex);
    bool moveWorldPoseForBone(granny_world_pose const* WorldPose,
                              int const                BoneIndex);
    bool getActorMatrixForBone(granny_world_pose const* WorldPose,
                               int const                BoneIndex,
                               NxMat34*                 Matrix);

    bool getWorldPose(granny_world_pose*       WorldPose,
                      granny_local_pose const* LocalPose,
                      float const*             Offset4x4);

    int getNumBones() const;

    int getNumActors() const;
    NxActor* getActor(int const ActorIndex);
    NxActor* getActorByName(char const* ActorName);
    NxActor* getActorForBone(int const BoneIndex);

    int getBoneForActor(int const ActorIndex);

    int getActorIndex(NxActor* Actor);

    int getNumJoints() const;
    NxJoint* getJoint(int const JointIndex);

    // Implements the NxuUserNotify interface so we can capture
    // pointers to the instantiated objects.
public:
    virtual void NXU_errorMessage(bool severity,const char *str)
    {
    }

    virtual void NXU_notifyScene(NxScene *scene,const char *userProperties) { m_scene = scene; }
    virtual void NXU_notifyFluid(NxFluid *fluid,const char *userProperties) {}
    virtual void NXU_notifyJoint(NxJoint *joint,const char *userProperties)
    {
        m_joints.push_back(joint);
    }
    virtual void NXU_notifyActor(NxActor *actor,const char *userProperties)
    {
        const char* ActorName = actor->getName();
        m_actors.push_back(actor);
    }

    virtual void NXU_notifyCloth(NxCloth *c,const char *userProperteis) { };


    NxScene * NXU_preNotifyScene(unsigned int sno,NxSceneDesc &scene,const char *userProperties)
    {
        return 0;
    }

    bool      NXU_preNotifyJoint(NxJointDesc &joint,const char *userProperties)
    {
        return true;
    }

    bool      NXU_preNotifyActor(NxActorDesc &actor,const char *userProperties)
    {
        return true;
    }

    bool      NXU_preNotifyCloth(NxClothDesc &cloth,const char *userProperties)
    {
        return false; // not implemented yet
    }

    bool      NXU_preNotifyFluid(NxFluidDesc &fluid,const char *userProperties)
    {
        return false;
    }


private:
    NxScene*               m_scene;
    granny_model_instance* m_modelInstance;
    granny_model*          m_sourceModel;
    granny_skeleton*       m_sourceSkeleton;

    granny_local_pose*     m_scratchPose;
    granny_transform*      m_deltaPoseBuffer;
    granny_transform*      m_poseBuffer;

    std::vector<NxActor*> m_actors;
    std::vector<NxJoint*> m_joints;

    std::vector<int> m_boneToActorMap;
    std::vector<int> m_actorToBoneMap;
    std::vector<NxMat34> m_actorToBoneMatrices;
    std::vector<NxMat34> m_boneToActorMatrices;

private:
    // bool CreateJointMappings();
};


GrannyPhysX* grp_createMapping(NXU::NxuPhysicsCollection* Collection,
                               NxPhysicsSDK*              SDK,
                               NxScene*                   Scene,
                               granny_model_instance*     ModelInstance,
                               grp_mapNameActorToBone*    NameMap)
{
    assert(Collection);
    assert(SDK);
    assert(ModelInstance);

    // Use the default name map if none is set
    if (NameMap == 0)
        NameMap = DefaultMapNameActorToBone;

    GrannyPhysX *NewMapping = new GrannyPhysX;
    if (!NewMapping->createActorBinding(Collection, SDK, Scene, ModelInstance, NameMap))
    {
        delete NewMapping;
        NewMapping = NULL;
    }
    else
    {
        assert(NewMapping->IsValid());
    }

    return NewMapping;
}

void grp_releaseMapping(GrannyPhysX* Mapping)
{
    if (Mapping)
    {
        assert(Mapping->IsValid());
        delete Mapping;
    }
}


bool grp_zeroVelocity(GrannyPhysX* Mapping)
{
    assert(Mapping);
    return Mapping->zeroVelocity();
}


bool grp_setWorldPose(GrannyPhysX*             Mapping,
                      granny_world_pose const* WorldPose)
{
    assert(Mapping);
    return Mapping->setWorldPose(WorldPose);
}

bool grp_setWorldPoseForBone(GrannyPhysX*             Mapping,
                             granny_world_pose const* WorldPose,
                             const int                BoneIndex)
{
    assert(Mapping);

    return Mapping->setWorldPoseForBone(WorldPose, BoneIndex);
}

bool grp_getActorMatrixForBone(GrannyPhysX*             Mapping,
                               granny_world_pose const* WorldPose,
                               int const                BoneIndex,
                               NxMat34*                 Matrix)
{
    assert(Mapping);

    return Mapping->getActorMatrixForBone(WorldPose, BoneIndex, Matrix);
}

bool grp_getActorMatrixForActor(GrannyPhysX*             Mapping,
                                granny_world_pose const* WorldPose,
                                int const                ActorIndex,
                                NxMat34*                 Matrix)
{
    assert(Mapping);

    int BoneIndex = Mapping->getBoneForActor(ActorIndex);
    if (BoneIndex == GRP_NO_MAPPING)
        return false;

    return Mapping->getActorMatrixForBone(WorldPose, BoneIndex, Matrix);
}

bool grp_moveWorldPoseForBone(GrannyPhysX*             Mapping,
                              granny_world_pose const* WorldPose,
                              const int                BoneIndex)
{
    assert(Mapping);

    return Mapping->moveWorldPoseForBone(WorldPose, BoneIndex);
}

bool grp_getWorldPose(GrannyPhysX*             Mapping,
                      granny_world_pose*       WorldPose,
                      granny_local_pose const* LocalPose,
                      float const*             Offset4x4)
{
    assert(Mapping);
    assert(WorldPose);
    assert(LocalPose);

    return Mapping->getWorldPose(WorldPose, LocalPose, Offset4x4);
}

int grp_getNumActors(GrannyPhysX* Mapping)
{
    assert(Mapping);

    return Mapping->getNumActors();
}

NxActor* grp_getActor(GrannyPhysX* Mapping,
                      int const    ActorIndex)
{
    assert(Mapping);
    assert(ActorIndex >= 0 && ActorIndex < Mapping->getNumActors());

    return Mapping->getActor(ActorIndex);
}

int grp_getActorIndex(GrannyPhysX* Mapping,
                      NxActor*     Actor)
{
    assert(Mapping);

    return Mapping->getActorIndex(Actor);
}

NxActor* grp_findActorByName(GrannyPhysX* Mapping,
                             const char*  ActorName)
{
    assert(Mapping);
    assert(ActorName);

    return Mapping->getActorByName(ActorName);
}

NxActor* grp_findActorForBone(GrannyPhysX* Mapping,
                              int const    BoneIndex)
{
    assert(Mapping);
    assert(BoneIndex >= 0 && BoneIndex < Mapping->getNumBones());

    return Mapping->getActorForBone(BoneIndex);
}

int grp_getNumJoints(GrannyPhysX* Mapping)
{
    assert(Mapping);

    return Mapping->getNumJoints();
}

NxJoint* grp_getJoint(GrannyPhysX* Mapping,
                      int const    JointIndex)
{
    assert(Mapping);
    assert(JointIndex >= 0 && JointIndex < Mapping->getNumJoints());

    return Mapping->getJoint(JointIndex);
}


// =====================================================================
// Retrieves the actor that corresponds to the given bone.  Returns
// NULL if no such corresponce exists.
NxActor* grp_findActorForBone(GrannyPhysX* Mapping,
                              int const BoneIndex);



// =====================================================================
// =====================================================================
GrannyPhysX::GrannyPhysX()
  : m_sourceSkeleton(NULL),
    m_sourceModel(NULL),
    m_scene(NULL),
    m_modelInstance(NULL)
{
    //
    m_scratchPose = 0;
    m_deltaPoseBuffer = 0;
    m_poseBuffer = 0;
}

GrannyPhysX::~GrannyPhysX()
{
    for (size_t i = 0; i < m_joints.size(); i++)
    {
        NxScene& scene = m_joints[i]->getScene();
        scene.releaseJoint(*m_joints[i]);
    }

    for (size_t i = 0; i < m_actors.size(); i++)
    {
        NxScene& scene = m_actors[i]->getScene();
        scene.releaseActor(*m_actors[i]);
    }

    m_sourceSkeleton  = NULL;
    m_sourceModel     = NULL;
    m_modelInstance   = NULL;

    GrannyFreeLocalPose(m_scratchPose);
    m_scratchPose = 0;

    delete [] m_deltaPoseBuffer;
    delete [] m_poseBuffer;
    m_deltaPoseBuffer = 0;
    m_poseBuffer = 0;
}

bool GrannyPhysX::IsValid() const
{
    if (m_sourceSkeleton == NULL || m_sourceModel == NULL || m_modelInstance == NULL)
        return false;

    if (m_actors.size() != m_actorToBoneMap.size())
        return false;

    if (granny_int32x(m_boneToActorMap.size()) != m_sourceSkeleton->BoneCount)
        return false;

    {for(size_t Actor = 0; Actor < m_actors.size(); ++Actor)
    {
        if (m_actors[Actor] == NULL)
            return false;
    }}

    {for(size_t Joint = 0; Joint < m_joints.size(); ++Joint)
    {
        if (m_joints[Joint] == NULL)
            return false;
    }}

    {for(size_t A2B = 0; A2B < m_actorToBoneMap.size(); ++A2B)
    {
        if (m_actorToBoneMap[A2B] == GRP_NO_MAPPING)
            continue;

        if (m_actorToBoneMap[A2B] < 0 || m_actorToBoneMap[A2B] >= m_sourceSkeleton->BoneCount)
            return false;
    }}
    {for(size_t B2A = 0; B2A < m_boneToActorMap.size(); ++B2A)
    {
        if (m_boneToActorMap[B2A] == GRP_NO_MAPPING)
            continue;

        if (m_boneToActorMap[B2A] < 0 || m_boneToActorMap[B2A] >= int(m_actors.size()))
            return false;
    }}

    return true;
}

bool GrannyPhysX::createActorBinding(NXU::NxuPhysicsCollection* Collection,
                                     NxPhysicsSDK*              SDK,
                                     NxScene*                   Scene,
                                     granny_model_instance*     ModelInstance,
                                     grp_mapNameActorToBone*    NameMap)
{
    // We shouldn't already be bound...
    assert(!IsValid());
    assert(Collection);
    assert(SDK);
    assert(ModelInstance);
    assert(NameMap);

    // Capture the granny state
    m_modelInstance  = ModelInstance;
    m_sourceModel    = GrannyGetSourceModel(m_modelInstance);
    m_sourceSkeleton = GrannyGetSourceSkeleton(m_modelInstance);

    m_scratchPose     = GrannyNewLocalPose(m_sourceSkeleton->BoneCount);
    m_deltaPoseBuffer = new granny_transform[m_sourceSkeleton->BoneCount];
    m_poseBuffer      = new granny_transform[m_sourceSkeleton->BoneCount];

    bool ok = NXU::instantiateCollection(Collection, *SDK, Scene, 0, this );
    if ( Scene == NULL )
        Scene = m_scene;

    // // Relate the joints to the actors they refer too after the scene is instantiated
    // CreateJointMappings();

    // Compute the mapping arrays.
    assert(m_actorToBoneMap.size() == 0 && m_boneToActorMap.size() == 0);
    m_actorToBoneMap.resize(m_actors.size(), GRP_NO_MAPPING);
    m_boneToActorMap.resize(m_sourceSkeleton->BoneCount, GRP_NO_MAPPING);

    m_actorToBoneMatrices.resize(m_actors.size());
    m_boneToActorMatrices.resize(m_actors.size());

    {for(size_t ActorIdx = 0; ActorIdx < m_actors.size(); ++ActorIdx)
    {
        assert(m_actors[ActorIdx]);
        const char* ActorName = m_actors[ActorIdx]->getName();

        if (ActorName == 0)
        {
            // Log error...
            continue;
            //return false;
        }

        char MappedName[GRP_MAX_BONENAME_LEN];
        if (NameMap(ActorName, MappedName, sizeof(MappedName)) == false)
        {
            return false;
        }

        granny_int32x BoneIdx;
        if (GrannyFindBoneByName(m_sourceSkeleton, MappedName, &BoneIdx))
        {
            m_boneToActorMap[BoneIdx]  = int(ActorIdx);
            m_actorToBoneMap[ActorIdx] = BoneIdx;

            NxMat34& BoneToActor = m_boneToActorMatrices[ActorIdx];
            NxMat34& ActorToBone = m_actorToBoneMatrices[ActorIdx];

            NxMat34 globalPose = m_actors[ActorIdx]->getGlobalPose();

            NxMat34 InvBoneWorld;
            InvBoneWorld.setColumnMajor44(m_sourceSkeleton->Bones[BoneIdx].InverseWorld4x4);

            BoneToActor.multiply(InvBoneWorld, globalPose);
            BoneToActor.getInverse(ActorToBone);
        }
    }}

    return IsValid();
}

// bool
// GrannyPhysX::CreateJointMappings()
// {
//     {for (size_t Idx = 0; Idx < m_joints.size(); ++Idx)
//     {
//         NxJoint* Joint = m_joints[Idx];
//         if (!Joint)
//             return false;

//         NxActor* Parent = 0;
//         NxActor* Child = 0;
//         Joint->getActors(&Parent, &Child);

//         int ParentIdx = -1;
//         int ChildIdx = -1;
//         {for (size_t ActorIdx = 0; ActorIdx < m_actors.size(); ++ActorIdx)
//         {
//             if (m_actors[ActorIdx]      == Parent) ParentIdx = (int)ActorIdx;
//             else if (m_actors[ActorIdx] == Child)  ChildIdx  = (int)ActorIdx;
//         }}
//         assert(ParentIdx >= 0 && ChildIdx >= 0);
//         assert(ParentIdx != ChildIdx);

// 		m_jointMap.insert(std::pair<int,ActorJointPair>((int)ParentIdx, ActorJointPair(ChildIdx, (int)Idx)));
//     }}

// 	return true;
// }



bool GrannyPhysX::zeroVelocity()
{
    assert(IsValid());

    {for (size_t Idx = 0; Idx < m_actors.size(); ++Idx)
    {
        m_actors[Idx]->setLinearVelocity(NxVec3(0,0,0));
        m_actors[Idx]->setAngularVelocity(NxVec3(0,0,0));
    }}

    return true;
}


bool GrannyPhysX::getActorMatrixForBone(granny_world_pose const* WorldPose,
                                        int const                BoneIndex,
                                        NxMat34*                 Matrix)
{
    assert(WorldPose);
    assert(BoneIndex >= 0 && BoneIndex < m_sourceSkeleton->BoneCount);
    if (!Matrix)
        return false;

    int ActorIdx = m_boneToActorMap[BoneIndex];
    if (ActorIdx == GRP_NO_MAPPING)
        return false;
    assert(ActorIdx < (int)m_actors.size());

    NxMat34 bonePose;
    bonePose.setColumnMajor44(GrannyGetWorldPose4x4(WorldPose, BoneIndex));

    NxMat34& BoneToActor = m_boneToActorMatrices[ActorIdx];
    Matrix->multiply(bonePose, BoneToActor);

    return true;
}

bool GrannyPhysX::setWorldPose(granny_world_pose const* WorldPose)
{
    assert(IsValid());
    assert(WorldPose);
    assert(GrannyGetWorldPoseBoneCount(WorldPose) >= m_sourceSkeleton->BoneCount);

    for (granny_int32x i = 0; i < m_sourceSkeleton->BoneCount; i++)
        setWorldPoseForBone(WorldPose, i);

    return true;
}

bool GrannyPhysX::setWorldPoseForBone(granny_world_pose const* WorldPose,
                                      int const                BoneIndex)
{
    assert(IsValid());

    NxMat34 globalPose;
    if (getActorMatrixForBone(WorldPose, BoneIndex, &globalPose) == false)
        return false;

    int ActorIdx = m_boneToActorMap[BoneIndex];
    if (ActorIdx == GRP_NO_MAPPING)
        return false;
    assert(ActorIdx < (int)m_actors.size());

    NxActor* Actor = m_actors[ActorIdx];
    assert(Actor);
    Actor->setGlobalPose(globalPose);

    return true;
}

bool GrannyPhysX::moveWorldPoseForBone(granny_world_pose const* WorldPose,
                                       int const                BoneIndex)
{
    assert(IsValid());

    NxMat34 globalPose;
    if (getActorMatrixForBone(WorldPose, BoneIndex, &globalPose) == false)
        return false;

    int ActorIdx = m_boneToActorMap[BoneIndex];
    if (ActorIdx == GRP_NO_MAPPING)
        return false;
    assert(ActorIdx < (int)m_actors.size());

    NxActor* pActor = m_actors[ActorIdx];
    assert(pActor);
    pActor->moveGlobalPose(globalPose);

    return true;
}

bool GrannyPhysX::getWorldPose(granny_world_pose*       WorldPose,
                               granny_local_pose const* LocalPose,
                               float const*             Offset4x4)
{
    assert(IsValid());
    assert(WorldPose);
    assert(LocalPose);
    assert(GrannyGetWorldPoseBoneCount(WorldPose) >= m_sourceSkeleton->BoneCount);
    assert(GrannyGetLocalPoseBoneCount(LocalPose) >= m_sourceSkeleton->BoneCount);

    // If NULL was set as the world offset, use an identity matrix...
    if (Offset4x4 == NULL)
    {
        static const float StaticIdentity[16] = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
        Offset4x4 = StaticIdentity;
    }

    // We do this in two passes.  First, we extract the actor matrices
    // for bones that have a direct correspondence in the physics
    // model.  Then, using the local pose, we fill in the world
    // matrices for bones that don't have a representation in the
    // model.

    granny_matrix_4x4* WorldMatrices = GrannyGetWorldPose4x4Array(WorldPose);
    {for(size_t ActorIdx = 0; ActorIdx < m_actors.size(); ++ActorIdx)
    {
        if (m_actorToBoneMap[ActorIdx] == GRP_NO_MAPPING)
            continue;

        NxActor* Actor = m_actors[ActorIdx];
        assert(Actor);

        NxMat34 actorPose = Actor->getGlobalPose();

        NxMat34 globalPose;
        globalPose.multiply(actorPose, m_actorToBoneMatrices[ActorIdx]);

        globalPose.getColumnMajor44((float*)WorldMatrices[m_actorToBoneMap[ActorIdx]]);
    }}

    {for(granny_int32x BoneIdx = 0; BoneIdx < m_sourceSkeleton->BoneCount; ++BoneIdx)
    {
        // If we already have set this bone's matrix through the
        // mapping, ignore it
        if (m_boneToActorMap[BoneIdx] != GRP_NO_MAPPING)
            continue;

        granny_matrix_4x4* Bone4x4 = &WorldMatrices[BoneIdx];

        granny_transform* pLocalXForm = GrannyGetLocalPoseTransform(LocalPose, BoneIdx);
        granny_real32 LocalComposite[16];
        GrannyBuildCompositeTransform4x4(pLocalXForm, LocalComposite);

        granny_bone& Bone = m_sourceSkeleton->Bones[BoneIdx];
        float const* Parent4x4;
        if (Bone.ParentIndex != GrannyNoParentBone)
        {
            Parent4x4 = (granny_real32*)&WorldMatrices[Bone.ParentIndex];
        }
        else
        {
            Parent4x4 = Offset4x4;
        }

        GrannyColumnMatrixMultiply4x4((granny_real32*)Bone4x4,
                                      LocalComposite, Parent4x4);
    }}

    // Finally, since we've only computed the bones' World4x4, we use
    // BuildCompositeFromWorldPose to fill in the skinning matrices.
    // We might want to make this computation optional, since we only
    // need the composite matrices if the world_pose will be directly
    // used for skinning.
    if (GrannyGetWorldPoseComposite4x4Array(WorldPose) != NULL)
    {
        GrannyBuildWorldPoseComposites(m_sourceSkeleton,
                                       0, m_sourceSkeleton->BoneCount,
                                       WorldPose);
    }

    return true;
}

int GrannyPhysX::getNumBones() const
{
    assert(IsValid());

    return m_sourceSkeleton->BoneCount;
}

int GrannyPhysX::getNumActors() const
{
    assert(IsValid());

    return int(m_actors.size());
}

NxActor* GrannyPhysX::getActor(int const ActorIndex)
{
    assert(IsValid());
    assert(ActorIndex >= 0 && ActorIndex < int(m_actors.size()));

    return m_actors[ActorIndex];
}

int GrannyPhysX::getActorIndex(NxActor* Actor)
{
    assert(IsValid());

    if (!Actor)
        return GRP_NO_MAPPING;

    {for (size_t Idx = 0; Idx < m_actors.size(); ++Idx)
    {
        if (m_actors[Idx] == Actor)
            return int(Idx);
    }}

    return GRP_NO_MAPPING;
}


NxActor* GrannyPhysX::getActorByName(char const* ActorName)
{
    assert(IsValid());
    assert(ActorName);

    {for(size_t ActorIdx = 0; ActorIdx < m_actors.size(); ActorIdx++)
    {
        NxActor* Actor = m_actors[ActorIdx];
        assert(Actor);

        if (strcmp(Actor->getName(), ActorName) == 0)
            return Actor;
    }}

    return NULL;
}

NxActor* GrannyPhysX::getActorForBone(int const BoneIndex)
{
    assert(IsValid());
    assert(BoneIndex >= 0 && BoneIndex < m_sourceSkeleton->BoneCount);

    if (m_boneToActorMap[BoneIndex] != GRP_NO_MAPPING)
        return m_actors[m_boneToActorMap[BoneIndex]];

    return NULL;
}

int GrannyPhysX::getBoneForActor(int const ActorIndex)
{
    assert(IsValid());
    assert(ActorIndex >= 0 && ActorIndex < (int)m_actors.size());

    return m_actorToBoneMap[ActorIndex];
}

int GrannyPhysX::getNumJoints() const
{
    assert(IsValid());

    return int(m_joints.size());
}

NxJoint* GrannyPhysX::getJoint(int const JointIndex)
{
    assert(IsValid());
    assert(JointIndex >= 0 && JointIndex < int(m_joints.size()));

    return m_joints[JointIndex];
}

#endif


NXU::NxuPhysicsCollection * grp_loadCollection(const char *fname) // loads an .XML file and returns the 'NxuPhysicsCollection'
{

    NXU::NxuPhysicsCollection *ret = 0;

    ret = NXU::loadCollection(fname,NXU::FT_XML,0);


    return ret;
}


void  grp_releaseCollection(NXU::NxuPhysicsCollection *collection)
{
    NXU::releaseCollection(collection);
}

