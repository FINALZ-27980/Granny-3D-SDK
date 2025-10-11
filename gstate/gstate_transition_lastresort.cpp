// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_transition_lastresort.cpp $
// $DateTime: 2012/08/28 11:13:50 $
// $Change: 39051 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================
#include "gstate_transition_lastresort.h"
#include "gstate_token_context.h"
#include "gstate_node.h"
#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE tr_lastresort::tr_lastresortImpl
{
    granny_real32 Probability;
};

granny_data_type_definition GSTATE
tr_lastresort::tr_lastresortImplType[] =
{
    { GrannyReal32Member, "Probability" },
    { GrannyEndMember },
};


DEFAULT_TAKE_TOKENOWNERSHIP(tr_lastresort);

// transition is a concrete class, so we must create a slotted container
struct tr_lastresort_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(transition);
    DECL_TOKEN_SLOT(tr_lastresort);
};

granny_data_type_definition tr_lastresort::tr_lastresortTokenType[] =
{
    DECL_UID_MEMBER(tr_lastresort),
    DECL_TOKEN_MEMBER(transition),
    DECL_TOKEN_MEMBER(tr_lastresort),

    { GrannyEndMember }
};

IMPL_CREATE_DEFAULT(tr_lastresort);

GSTATE
tr_lastresort::tr_lastresort(token_context*               Context,
                             granny_data_type_definition* TokenType,
                             void*                        TokenObject,
                             token_ownership              TokenIsOwned)
  : parent(Context, TokenType, TokenObject, TokenIsOwned),
    m_tr_lastresortToken(0)
{
    IMPL_INIT_FROM_TOKEN(tr_lastresort);
}


GSTATE
tr_lastresort::~tr_lastresort()
{
    DTOR_RELEASE_TOKEN(tr_lastresort);
}


transition_type GSTATE
tr_lastresort::GetTransitionType() const
{
    return Transition_LastResort;
}

bool GSTATE
tr_lastresort::FillDefaultToken(granny_data_type_definition* TokenType,
                                void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares tr_lastresortImpl*& Slot = // member
    GET_TOKEN_SLOT(tr_lastresort);

    // Our slot in this token should be empty.
    // Create a new tr_lastresort token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->Probability = 1.0f;

    return true;
}

granny_real32 GSTATE
tr_lastresort::GetProbability() const
{
    return m_tr_lastresortToken->Probability;
}

void GSTATE
tr_lastresort::SetProbability(granny_real32 Probability)
{
    GStateAssert(Probability >= 0 && Probability <= 1.0f);

    TakeTokenOwnership();
    m_tr_lastresortToken->Probability = Probability;
}


bool GSTATE
tr_lastresort::ShouldActivate(int              PassNumber,
                              activate_trigger Trigger,
                              granny_real32    AtT,
                              granny_real32    DeltaFromLastCheck)
{
    if (PassNumber == 0 || Trigger == Trigger_Requested)
        return false;

    if (!CheckConditionals(AtT))
        return false;
    
    // We need to query the start node to see if a loop occurred in the window of
    // interest
    granny_int32x StartExt = m_StartNode->GetNthExternalOutput(0);
    bool LoopOccurred = GetStartNode()->DidLoopOccur(StartExt, AtT, DeltaFromLastCheck);

    if (LoopOccurred)
    {
        granny_real32 Prob = m_tr_lastresortToken->Probability;
        if (Prob == 1.0f)
            return true;
        else if (Prob == 0.0f)
            return false;

        else
        {
            return RandomUnit() <= Prob;
        }
    }

    return false;
}

