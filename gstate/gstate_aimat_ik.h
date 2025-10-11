// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_aimat_ik.h $
// $DateTime: 2012/05/11 15:03:34 $
// $Change: 37384 $
// $Revision: #4 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_AIMAT_IK_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

enum EAimAxis
{
    eAimAxis_XAxis = 0,
    eAimAxis_YAxis = 1,
    eAimAxis_ZAxis = 2,

    eAimAxis_NegXAxis = 3,
    eAimAxis_NegYAxis = 4,
    eAimAxis_NegZAxis = 5,

    eAimAxis_ForceInt = 0x7fffffff
};
granny_real32 const* FloatFromAxisEnum(granny_int32x axis);


class aimat_ik : public node
{
    typedef node parent;

public:
    virtual bool BindToCharacter(gstate_character_instance* Instance);
    virtual void UnbindFromCharacter();

    virtual void AcceptNodeVisitor(node_visitor* Visitor);
    virtual granny_real32 GetDuration(granny_int32x OnOutput);

    virtual granny_local_pose* SamplePoseOutput(granny_int32x OutputIdx,
                                                granny_real32 AtT,
                                                granny_real32 AllowedError,
                                                granny_pose_cache* PoseCache);

    virtual bool GetRootMotionVectors(granny_int32x  OutputIdx,
                                      granny_real32  AtT,
                                      granny_real32  DeltaT,
                                      granny_real32* ResultTranslation,
                                      granny_real32* ResultRotation,
                                      bool           Inverse);

    DECL_CONCRETE_CLASS_TOKEN(aimat_ik);
    DECL_SNAPPABLE();


    // This is the only per-instance piece of data...
    void SetAimPosition(granny_real32 const* Position);
    void GetAimPosition(granny_real32*);

    // All of these affect the token
    void SetDefaultAimPosition(granny_real32 const* Position);
    void GetDefaultAimPosition(granny_real32*);

    void SetAimAxis(EAimAxis Axis);
    EAimAxis GetAimAxis();
    granny_real32 const* GetAimAxisFloat();

    void SetEarAxis(EAimAxis Axis);
    EAimAxis GetEarAxis();
    granny_real32 const* GetEarAxisFloat();

    void SetGroundNormal(EAimAxis Axis);
    EAimAxis GetGroundNormal();
    granny_real32 const* GetGroundNormalFloat();

    void SetHeadName(char const* HeadName);
    void SetFirstActiveName(char const* FirstActive);
    void SetLastActiveName(char const* LastActive);

    char const* GetHeadName();
    char const* GetFirstActiveName();
    char const* GetLastActiveName();

    bool IsActive() const;

    // These will be valid when IsActive is true
    granny_int32x GetBoundHeadIndex();
    granny_int32x GetBoundLinkCount();
    granny_int32x GetBoundInactiveLinkCount();

public:
    virtual void Activate(granny_int32x OnOutput, granny_real32 AtT);
    virtual bool DidLoopOccur(granny_int32x OnOutput, granny_real32 AtT, granny_real32 DeltaT);
    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;

private:
    granny_world_pose* m_tempPose;

    granny_real32 m_AimPosition[3];

    // If the node is bound to an invalid character, all of these will be -1.
    // Unless all of them are > 0, and m_InactiveLinks < m_linkCount, the node
    // is inactive
    granny_int32x m_HeadIndex;
    granny_int32x m_LinkCount;
    granny_int32x m_InactiveLinks;

    void RefreshBoundValues();
};

END_GSTATE_NAMESPACE;


#include "gstate_header_postfix.h"
#define GSTATE_AIMAT_IK_H
#endif /* GSTATE_AIMAT_IK_H */
