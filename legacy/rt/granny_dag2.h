#if !defined(GRANNY_DAG2_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_DATA_TYPE_DEFINITION_H)
#include "granny_data_type_definition.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(DagGroup);

#define CurrentDAGStandardTag (0xC0000000 + 8) EXPMACRO

struct dag2_instance;
struct dag2_node;
struct file_info;
struct local_pose;
struct memory_arena;
struct model;
struct model_instance;
struct skeleton;

#define NoNodeIndex -1 EXPMACRO

EXPTYPE enum dag2_parameter_type
{
    FloatParameterType,
    PoseParameterType,
    TrackmaskParameterType,

    dag2_parameter_type_forceint = 0x7fffffff
};

EXPTYPE struct dag2_source;
struct dag2_source
{
    char const* SourceName;
    file_info*  CachedSource;
};
EXPCONST EXPGROUP(dag2_source) extern data_type_definition Dag2SourceType[];

EXPTYPE struct dag2
{
    int32      RootNodeIndex;
    int32      InstanceDataSize;

    int32       NodeCount;
    dag2_node** Nodes;

    int32         SourceCount;
    dag2_source** Sources;
};
EXPCONST EXPGROUP(dag2) extern data_type_definition Dag2Type[];

EXPTYPE struct dag2_input_edge;
struct dag2_input_edge
{
    int32       ParameterType;
    int32       InputNodeIndex;
    int32       ConnectedOutput;
    char const* EdgeName;

    bool32      Internal;
};
EXPCONST EXPGROUP(dag2_input_edge) extern data_type_definition Dag2InputEdgeType[];

EXPTYPE struct dag2_output_edge;
struct dag2_output_edge
{
    int32       ParameterType;
    char const* EdgeName;

    bool32      Internal;
    int32       Forwarded;
    int32       ForwardIndex;
};
EXPCONST EXPGROUP(dag2_output_edge) extern data_type_definition Dag2OutputEdgeType[];

EXPTYPE struct dag2_node;
struct dag2_node
{
    char*       Name;
    int32       ParentIndex;        // The root has no parent...

    // You may be asking yourself: X/YPos?  Isn't that a 2d thing?  What's it doing in my
    // dag node?  Good question!
    int32 NodeXPos;
    int32 NodeYPos;

    int32             ExternalInputEdgeCount;
    int32             TotalInputEdgeCount;
    dag2_input_edge*  InputEdges;

    int32             ExternalOutputEdgeCount;
    int32             TotalOutputEdgeCount;
    dag2_output_edge* OutputEdges;

    int32   InstanceOffset;
    variant NodeData;
};
EXPCONST EXPGROUP(dag2_node) extern data_type_definition Dag2NodeType[];

EXPAPI GS_READ dag2* NewDag();
EXPAPI GS_READ dag2* NewDagInPlace(void* Memory);

// Support for the callback node...
EXPAPI GS_READ bool GetDag2NodeInputConnectionInfo(dag2_node& Node,
                                                   int32x InputIndex,
                                                   int32x* ConnectedNodeIndex,
                                                   int32x* ConnectedOutputIndex);

EXPAPI GS_READ dag2_node* FindDag2Node(dag2& TheDag, dag2_node& Node, char const* NodeName);
void SetNodeName(dag2& TheDag, dag2_node* Node, char const* NewName, memory_arena& Arena);

bool BreakNodeInputConnection(dag2& TheDag, dag2_node* Node, int32x InputIndex);
bool BreakNodeOutputConnection(dag2& TheDag, dag2_node* Node, int32x OutputIndex);

EXPAPI GS_READ dag2_node* GetDag2Root(dag2& TheDag);
dag2_node* GetDag2Node(dag2& TheDag, int32x Index);
int32x GetDag2NodeIndex(dag2& TheDag, dag2_node* Node);

int NumDag2Sources(dag2& TheDag);
dag2_source* GetDag2Source(dag2& TheDag, int32x SourceIndex);
int32x FindDag2SourceIndex(dag2& TheDag, char const* Name);

bool IsDag2SourceIndexValid(int32x SourceIndex);
void PermuteNodeIndices(dag2& TheDag, dag2_node* Node, int32x const* PermutationArray);
void PermuteInputEdgeConnections(dag2& TheDag, dag2_node* Node, int32x const* PermutationArray);

EXPAPI typedef CALLBACK_FN(file_info *) dag2_source_callback(char const* SourceName, void* UserData);
EXPAPI GS_MODIFY bool BindDag2Sources(dag2& TheDag, dag2_source_callback* Callback, void* UserData);

#define IsValidNodeIndex(Idx) ((Idx) > NoNodeIndex)

#define DECLARE_GET_NODEDATA(CapType, LowerType)                        \
    struct dag2_nodedata_ ## LowerType;                                 \
    dag2_nodedata_ ## LowerType * GetNodeData ## CapType(dag2_node*)

#define IMPL_GET_NODEDATA(CapType, LowerType)                                                   \
    dag2_nodedata_ ## LowerType * GRANNY                                                        \
    GetNodeData ## CapType (dag2_node* Node)                                                    \
    {                                                                                           \
        CheckPointerNotNull(Node, return 0);                                                    \
                                                                                                \
        /* If there is no sub data, obviously null */                                           \
        if (Node->NodeData.Type == 0 || Node->NodeData.Object == 0)                             \
            return 0;                                                                           \
                                                                                                \
        /* Otherwise they should both be present... */                                          \
        Assert(Node->NodeData.Type && Node->NodeData.Object);                                   \
                                                                                                \
        static int32x TypeIndex =                                                               \
            GetDag2NodeDataTypeIndexFromTypeMarker(Dag2NodeData ## CapType ## Type [0].Name);   \
                                                                                                \
        /* The first entry in a node data is the type marker.  See GetDag2Info() */             \
        if (TypeIndex == *(int32*)Node->NodeData.Object)                                        \
        {                                                                                       \
            return (dag2_nodedata_ ## LowerType *)Node->NodeData.Object;                        \
        }                                                                                       \
                                                                                                \
        /* Not a "type" node */                                                                 \
        return 0;                                                                               \
    } typedef int Require_SemiColon

DECLARE_GET_NODEDATA(AdditiveBlend, additiveblend);
DECLARE_GET_NODEDATA(Blend, blend);
DECLARE_GET_NODEDATA(Callback, callback);
DECLARE_GET_NODEDATA(LockedBlend, locked_blend);
DECLARE_GET_NODEDATA(NWayBlend, nwayblend);
DECLARE_GET_NODEDATA(PoseAnimSource, poseanimsource);
DECLARE_GET_NODEDATA(ScalarSource, scalarsource);
DECLARE_GET_NODEDATA(ScaleOffset, scaleoffset);
DECLARE_GET_NODEDATA(Selection, selection);
DECLARE_GET_NODEDATA(Sequence, sequence);
DECLARE_GET_NODEDATA(Subgraph, subgraph);
DECLARE_GET_NODEDATA(TimeShift, timeshift);
DECLARE_GET_NODEDATA(Trackmask, trackmask);


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_DAG2_H
#endif
