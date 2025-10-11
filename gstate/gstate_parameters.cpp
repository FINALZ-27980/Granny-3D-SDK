// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_parameters.cpp $
// $DateTime: 2012/05/11 15:03:34 $
// $Change: 37384 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================
#include "gstate_parameters.h"

#include "gstate_node_visitor.h"
#include "gstate_state_machine.h"
#include "gstate_quick_vecs.h"
#include "gstate_snapshotutils.h"
#include "gstate_token_context.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct parameter_spec
{
    granny_real32 MinValue;
    granny_real32 MaxValue;
    granny_real32 DefaultValue;
};

struct GSTATE parameters::parametersImpl
{
    granny_int32    ParameterCount;
    parameter_spec* Parameters;

    granny_bool32   ClampToInts;
};

static granny_data_type_definition ParameterSpecType[] =
{
    { GrannyReal32Member, "MinValue" },
    { GrannyReal32Member, "MaxValue" },
    { GrannyReal32Member, "DefaultValue" },
    { GrannyEndMember }
};

granny_data_type_definition GSTATE
parameters::parametersImplType[] =
{
    { GrannyReferenceToArrayMember, "Parameters", ParameterSpecType },
    { GrannyBool32Member,           "ClampToInts" },
    { GrannyEndMember },
};

// parameters is a concrete class, so we must create a slotted container
struct parameters_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(parameters);
};

granny_data_type_definition parameters::parametersTokenType[] =
{
    DECL_UID_MEMBER(parameters),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(parameters),

    { GrannyEndMember }
};

void GSTATE
parameters::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(parameters);

    // When we take token ownership, future code will assume that it needs to free our
    // owned pointers when they change.  Don't disappoint them.
    GStateCloneArray(m_parametersToken->Parameters,
                     OldToken->Parameters,
                     m_parametersToken->ParameterCount);
}

void GSTATE
parameters::ReleaseOwnedToken_parameters()
{
    GStateDeallocate(m_parametersToken->Parameters);
}

IMPL_CREATE_DEFAULT(parameters);

GSTATE
parameters::parameters(token_context*               Context,
                       granny_data_type_definition* TokenType,
                       void*                        TokenObject,
                       token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_parametersToken(0),
    m_Parameters(0)
{
    IMPL_INIT_FROM_TOKEN(parameters);
    {
        if (m_parametersToken->ParameterCount)
        {
            m_Parameters = GStateAllocArray(float, m_parametersToken->ParameterCount);
            {for (int Idx = 0; Idx < m_parametersToken->ParameterCount; ++Idx)
            {
                m_Parameters[Idx] = m_parametersToken->Parameters[Idx].DefaultValue;
            }}
        }
    }

    if (EditorCreated())
    {
        // Default is one pose output and one input
        AddOutput(ScalarEdge,  "Parameter");
    }
}


GSTATE
parameters::~parameters()
{
    GStateDeallocate(m_Parameters);
    DTOR_RELEASE_TOKEN(parameters);
}


bool GSTATE
parameters::FillDefaultToken(granny_data_type_definition* TokenType,
                             void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares parametersImpl*& Slot = // member
    GET_TOKEN_SLOT(parameters);

    // Our slot in this token should be empty.
    // Create a new parameters Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    // These will come through in "addoutput"
    Slot->ParameterCount = 0;
    Slot->Parameters = 0;

    return true;
}

void GSTATE
parameters::AcceptNodeVisitor(node_visitor* Visitor)
{
    Visitor->VisitNode(this);
}


void GSTATE
parameters::SetParameter(granny_int32x OutputIdx, granny_real32 NewVal)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    // SetParameter bypasses min/max.  todo: Yes?
    m_Parameters[OutputIdx] = NewVal;
}

void GSTATE
parameters::SetParameterDefault(granny_int32x OutputIdx, granny_real32 NewVal)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    TakeTokenOwnership();

    m_parametersToken->Parameters[OutputIdx].DefaultValue = NewVal;
}

void GSTATE
parameters::SetClampSliderToInts(bool Clamp)
{
    TakeTokenOwnership();

    m_parametersToken->ClampToInts = Clamp ? 1 : 0;
}


bool GSTATE
parameters::GetClampSliderToInts()
{
    return (m_parametersToken->ClampToInts != 0);
}

bool GSTATE
parameters::GetScalarOutputRange(granny_int32x OutputIdx, float* MinVal, float* MaxVal)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(MinVal && MaxVal);

    *MinVal = m_parametersToken->Parameters[OutputIdx].MinValue;
    *MaxVal = m_parametersToken->Parameters[OutputIdx].MaxValue;

    return true;
}


void GSTATE
parameters::SetScalarOutputRange(granny_int32x OutputIdx,
                                 float         MinVal,
                                 float         MaxVal)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    TakeTokenOwnership();

    m_parametersToken->Parameters[OutputIdx].MinValue = MinVal;
    m_parametersToken->Parameters[OutputIdx].MaxValue = MaxVal;
}

granny_int32x GSTATE
parameters::AddOutput(node_edge_type EdgeType,
                      char const* EdgeName)
{
    TakeTokenOwnership();

    int NewIndex = parent::AddOutput(EdgeType, EdgeName);
    GStateAssert(NewIndex == m_parametersToken->ParameterCount);
    GStateAssert(NewIndex+1 == GetNumOutputs());

    // Increase these vecs.  They are at GetNumOutputs() - 1, we want them at GetNumOutputs()
    parameter_spec& NewSpec = QVecPushNewElement(m_parametersToken->ParameterCount,
                                                 m_parametersToken->Parameters);
    float& NewParam = QVecPushNewElementNoCount(GetNumOutputs() - 1, m_Parameters);

    GStateAssert(m_parametersToken->ParameterCount == GetNumOutputs());

    NewSpec.MinValue     = 0;
    NewSpec.MaxValue     = 1;
    NewSpec.DefaultValue = 0;
    NewParam             = NewSpec.DefaultValue;

    return NewIndex;
}

bool GSTATE
parameters::DeleteOutput(granny_int32x OutputIndex)
{
    GStateAssert(m_parametersToken->ParameterCount == GetNumOutputs());

    if (!parent::DeleteOutput(OutputIndex))
        return false;

    TakeTokenOwnership();

    QVecRemoveElementNoCount(OutputIndex, m_parametersToken->ParameterCount, m_Parameters);
    QVecRemoveElement(OutputIndex, m_parametersToken->ParameterCount, m_parametersToken->Parameters);

    return true;
}

granny_real32 GSTATE
parameters::SampleScalarOutput(granny_int32x OutputIdx,
                               granny_real32 AtT)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    float MinVal, MaxVal;
    GetScalarOutputRange(OutputIdx, &MinVal, &MaxVal);

    float CurrVal = m_Parameters[OutputIdx];
    if (CurrVal < MinVal)
        return MinVal;
    else if (CurrVal > MaxVal)
        return MaxVal;

    return CurrVal;
}

bool GSTATE
parameters::SetOutputName(granny_int32x OutputIdx,
                          char const* NewEdgeName)
{
    if (!parent::SetOutputName(OutputIdx, NewEdgeName))
        return false;

    state_machine* ParentSM = GSTATE_DYNCAST(GetParent(), state_machine);
    if (ParentSM)
        ParentSM->NoteParameterNameChange(this, OutputIdx);

    return true;
}


// Snapshot creation.  Let's have a talk about this in the parameter context.  We need to
// dump the m_Parameters, and then read it back in, pushing it into the token iff we own
// it.  That's a little weird, obv.
CREATE_SNAPSHOT(parameters)
{
    granny_int32 NumParameters = m_parametersToken->ParameterCount;
    CREATE_WRITE_INT32(NumParameters);

    if (NumParameters != 0)
    {
        GStateAssert(m_Parameters != 0);
        CREATE_WRITE_INT32_ARRAY(m_Parameters, NumParameters);
    }

    CREATE_PASS_TO_PARENT();
}

RESET_FROMSNAPSHOT(parameters)
{
    RESET_OFFSET_TRACKING();

    granny_int32 NumParameters = 0;
    RESET_READ_INT32(NumParameters);

    if (NumParameters != m_parametersToken->ParameterCount)
    {
        GStateError("Invalid paramater count found in Reset");
        return false;
    }

    if (NumParameters != 0)
    {
        GStateAssert(m_Parameters != 0);
        RESET_READ_INT32_ARRAY(m_Parameters, NumParameters);
    }

    RESET_PASS_TO_PARENT();
}


