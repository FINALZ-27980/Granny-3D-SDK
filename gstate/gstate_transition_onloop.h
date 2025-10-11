// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_transition_onloop.h $
// $DateTime: 2012/05/11 15:03:34 $
// $Change: 37384 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_TRANSITION_ONLOOP_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_TRANSITION_H
#include "gstate_transition.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class tr_onloop : public transition
{
    typedef transition parent;

public:
    virtual transition_type GetTransitionType() const;

    bool ShouldActivate(int PassNumber,
                        activate_trigger Trigger,
                        granny_real32 AtT,
                        granny_real32 DeltaT);

    DECL_CONCRETE_CLASS_TOKEN(tr_onloop);

    granny_real32 GetProbability() const;
    void          SetProbability(granny_real32 Probability);
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_TRANSITION_ONLOOP_H
#endif /* GSTATE_TRANSITION_ONLOOP_H */
