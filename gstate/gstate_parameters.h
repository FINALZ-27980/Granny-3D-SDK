// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_parameters.h $
// $DateTime: 2012/05/11 15:03:34 $
// $Change: 37384 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_PARAMETERS_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class parameters : public node
{
    typedef node parent;

public:
    virtual void AcceptNodeVisitor(node_visitor*);

    void SetParameter(granny_int32x OutputIdx, granny_real32 NewVal);

    virtual granny_int32x AddOutput(node_edge_type EdgeType, char const* EdgeName);
    virtual bool          DeleteOutput(granny_int32x OutputIndex);

    virtual granny_real32      SampleScalarOutput(granny_int32x OutputIdx,
                                                  granny_real32 AtT);
    virtual bool               GetScalarOutputRange(granny_int32x OutputIdx,
                                                    float*        MinVal,
                                                    float*        MaxVal);
    void  SetScalarOutputRange(granny_int32x OutputIdx, float MinVal,  float MaxVal);

    
    // We track this to let any owning state machines know that we've changed a parameter
    // name...
    virtual bool SetOutputName(granny_int32x OutputIdx, char const* NewEdgeName);

    virtual void SetParameterDefault(granny_int32x OutputIdx, granny_real32 NewVal);

    // Note that this is "ClampSlider" because it only constrains the *interface*, not the code.
    void SetClampSliderToInts(bool Clamp);
    bool GetClampSliderToInts();

protected:
    DECL_CONCRETE_CLASS_TOKEN(parameters);
    DECL_SNAPPABLE();
    IMPL_CASTABLE_INTERFACE(parameters);

private:
    float* m_Parameters;
};

END_GSTATE_NAMESPACE;


#include "gstate_header_postfix.h"
#define GSTATE_PARAMETERS_H
#endif /* GSTATE_PARAMETERS_H */
