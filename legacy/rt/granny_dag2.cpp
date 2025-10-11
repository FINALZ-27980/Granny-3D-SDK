// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_dag2.h"

#include "granny_animation.h"
#include "granny_control.h"
#include "granny_controlled_animation.h"
#include "granny_dag2_subgraph.h"
#include "granny_dag2_utilities.h"
#include "granny_file_info.h"
#include "granny_local_pose.h"
#include "granny_memory.h"
#include "granny_memory_arena.h"
#include "granny_memory_ops.h"
#include "granny_model_instance.h"
#include "granny_parameter_checking.h"
#include "granny_skeleton.h"
#include "granny_string.h"
#include "granny_string_formatting.h"

#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

#define SubsystemCode BlendDagLogMessage
USING_GRANNY_NAMESPACE;

data_type_definition GRANNY Dag2InputEdgeType[] =
{
    { Int32Member,  "ParameterType" },
    { Int32Member,  "InputNodeIndex" },
    { Int32Member,  "ConnectedOutput" },
    { StringMember, "EdgeName" },
    { Bool32Member, "Internal" },
    { EndMember },
};

data_type_definition GRANNY Dag2OutputEdgeType[] =
{
    { Int32Member,  "ParameterType" },
    { StringMember, "EdgeName" },
    { Bool32Member, "Internal" },
    { Int32Member,  "Forwarded" },
    { Int32Member,  "ForwardIndex" },
    { EndMember },
};

data_type_definition GRANNY Dag2NodeType[] =
{
    { StringMember,           "Name" },
    { Int32Member,            "ParentIndex" },
    { Int32Member,            "NodeXPos" },
    { Int32Member,            "NodeYPos" },
    { Int32Member,            "ExternalInputEdgeCount" },
    { ReferenceToArrayMember, "InputEdges", Dag2InputEdgeType },
    { Int32Member,            "ExternalOutputEdgeCount" },
    { ReferenceToArrayMember, "OutputEdges", Dag2OutputEdgeType },

    { Int32Member,            "InstanceOffset" },
    { VariantReferenceMember, "NodeData" },
    { EndMember },
};

data_type_definition GRANNY Dag2SourceType[] =
{
    { StringMember,    "SourceName" },
    { ReferenceMember, "CachedSource", FileInfoType },
    { EndMember }
};

data_type_definition GRANNY Dag2Type[] =
{
    { Int32Member,             "RootNodeIndex" },
    { Int32Member,             "InstanceDataSize" },
    { ArrayOfReferencesMember, "Nodes",   Dag2NodeType },
    { ArrayOfReferencesMember, "Sources", Dag2SourceType },
    { EndMember }
};


dag2* GRANNY
NewDag()
{
    return NewDagInPlace(Allocate(dag2, AllocationBuilder));
}

dag2* GRANNY
NewDagInPlace(void* Memory)
{
    CheckPointerNotNull(Memory, return 0);

    dag2* DAG = (dag2*)Memory;

    ZeroStructure(*DAG);
    DAG->RootNodeIndex = NoNodeIndex;

    return DAG;
}

int32x GRANNY
FindDag2SourceIndex(dag2& TheDag, char const* Name)
{
    CheckPointerNotNull(Name, return 0);

    {for(int32x Idx = 0; Idx < TheDag.SourceCount; ++Idx)
    {
        dag2_source* Source = TheDag.Sources[Idx];

        if (Source && StringsAreEqualLowercase(Source->SourceName, Name))
            return Idx;
    }}

    return -1;
}


bool GRANNY
IsDag2SourceIndexValid(int32x SourceIndex)
{
    return SourceIndex >= 0;
}


void GRANNY
PermuteNodeIndices(dag2& TheDag,
                   dag2_node* Node,
                   int32x const* PermutationArray)
{
    if (IsValidNodeIndex(Node->ParentIndex))
    {
        Node->ParentIndex = PermutationArray[Node->ParentIndex];
        Assert(IsValidNodeIndex(Node->ParentIndex));
    }

    {for(int32x Idx = 0; Idx < Node->TotalInputEdgeCount; ++Idx)
    {
        if (IsValidNodeIndex(Node->InputEdges[Idx].InputNodeIndex))
        {
            Assert(IsValidNodeIndex(PermutationArray[Node->InputEdges[Idx].InputNodeIndex]));

            Node->InputEdges[Idx].InputNodeIndex =
                PermutationArray[Node->InputEdges[Idx].InputNodeIndex];
        }
    }}

    dag2_nodedata_subgraph* Subgraph = GetNodeDataSubgraph(Node);
    if (Subgraph)
    {
        {for(int32x Idx = 0; Idx < Subgraph->SubnodeIndexCount; ++Idx)
        {
            Assert(IsValidNodeIndex(PermutationArray[Subgraph->SubnodeIndices[Idx]]));
            Subgraph->SubnodeIndices[Idx] =
                PermutationArray[Subgraph->SubnodeIndices[Idx]];

            PermuteNodeIndices(TheDag,
                               TheDag.Nodes[Subgraph->SubnodeIndices[Idx]],
                               PermutationArray);
        }}
    }
}

// The idea here is that one of the edges has been reordered, or deleted.  We need to step
// up to the subgraph that owns the specified node, and iterate though all the nodes,
// looking for external input connections to it.  Permute those edges with the given array
void GRANNY
PermuteInputEdgeConnections(dag2& TheDag,
                            dag2_node* Node,
                            int32x const* PermutationArray)
{
    CheckPointerNotNull(Node, return);
    CheckPointerNotNull(PermutationArray, return);
    CheckCondition(IsValidNodeIndex(Node->ParentIndex), return);

    int32x const NodeIndex = GetDag2NodeIndex(TheDag, Node);
    Assert(IsValidNodeIndex(NodeIndex));

    dag2_node* Parent = GetDag2Node(TheDag, Node->ParentIndex);
    Assert(Parent);

    dag2_nodedata_subgraph* Subgraph = GetNodeDataSubgraph(Parent);
    CheckPointerNotNull(Subgraph, return);

    // Look at all the sibling nodes in the subgraph.
    {for(int32x Idx = 0; Idx < Subgraph->SubnodeIndexCount; ++Idx)
    {
        Assert(IsValidNodeIndex(Subgraph->SubnodeIndices[Idx]));
        dag2_node* OtherNode = GetDag2Node(TheDag, Subgraph->SubnodeIndices[Idx]);
        Assert(OtherNode);

        {for(int32x EdgeIdx = 0; EdgeIdx < OtherNode->ExternalInputEdgeCount; ++EdgeIdx)
        {
            dag2_input_edge& Edge = OtherNode->InputEdges[EdgeIdx];
            if (Edge.InputNodeIndex != NodeIndex)
                continue;

            CheckInt32Index(PermutationArray[Edge.ConnectedOutput],
                              Node->ExternalOutputEdgeCount, return);
            Edge.ConnectedOutput = PermutationArray[Edge.ConnectedOutput];
        }}
    }}

    // Now look at the /internal/ inputs of the subgraph itself
    {for(int32x InputIdx = Parent->ExternalInputEdgeCount;
         InputIdx < Parent->TotalInputEdgeCount;
         ++InputIdx)
    {
        dag2_input_edge& Edge = Parent->InputEdges[InputIdx];
        Assert(Edge.Internal);

        if (Edge.InputNodeIndex != NodeIndex)
            continue;

        CheckInt32Index(PermutationArray[Edge.ConnectedOutput],
                          Node->ExternalOutputEdgeCount, return);
        Edge.ConnectedOutput = PermutationArray[Edge.ConnectedOutput];
    }}
}


bool GRANNY
BindDag2Sources(dag2& TheDag, dag2_source_callback* Callback, void* UserData)
{
    CheckPointerNotNull(Callback, return false);

    {for(int32x Idx = 0; Idx < TheDag.SourceCount; ++Idx)
    {
        CheckPointerNotNull(TheDag.Sources[Idx], return false);

        // Prevent double-binding
        if (TheDag.Sources[Idx]->CachedSource != 0)
            continue;

        TheDag.Sources[Idx]->CachedSource =
            (*Callback)(TheDag.Sources[Idx]->SourceName, UserData);
        CheckPointerNotNull(TheDag.Sources[Idx]->CachedSource, return false);
    }}

    return true;
}


dag2_node* GRANNY
GetDag2Root(dag2& TheDag)
{
    return GetDag2Node(TheDag, TheDag.RootNodeIndex);
}

dag2_node* GRANNY
GetDag2Node(dag2& TheDag, int32x Index)
{
    if (!IsValidNodeIndex(Index))
        return 0;

    CheckInt32Index(Index, TheDag.NodeCount, return 0);
    return TheDag.Nodes[Index];
}

int32x GRANNY
GetDag2NodeIndex(dag2& TheDag, dag2_node* Node)
{
    if (Node == 0)
        return NoNodeIndex;

    {for(int32x Idx = 0; Idx < TheDag.NodeCount; ++Idx)
    {
        if (TheDag.Nodes[Idx] == Node)
            return Idx;
    }}

    Log0(WarningLogMessage, BlendDagLogMessage, "Unable to match node in dag\n");
    return NoNodeIndex;
}


bool GRANNY
GetDag2NodeInputConnectionInfo(dag2_node& Node,
                               int32x InputIndex,
                               int32x* ConnectedNodeIndex,
                               int32x* ConnectedOutputIndex)
{
    CheckInt32Index(InputIndex, Node.TotalInputEdgeCount, return false);

    dag2_input_edge const& Edge = Node.InputEdges[InputIndex];
    if (Edge.InputNodeIndex == NoNodeIndex)
        return false;

    if (ConnectedNodeIndex)
        *ConnectedNodeIndex = Edge.InputNodeIndex;

    if (ConnectedOutputIndex)
        *ConnectedOutputIndex = Edge.ConnectedOutput;

    return true;
}



int GRANNY
NumDag2Sources(dag2& TheDag)
{
    return TheDag.SourceCount;
}


dag2_source* GRANNY
GetDag2Source(dag2& TheDag, int32x SourceIndex)
{
    CheckInt32Index(SourceIndex, NumDag2Sources(TheDag), return 0);
    CheckPointerNotNull(TheDag.Sources, return 0);
    CheckPointerNotNull(TheDag.Sources[SourceIndex], return 0);

    return TheDag.Sources[SourceIndex];
}


static dag2_node*
FindDag2Node_Subgraph(dag2_node& Node,
                      dag2_nodedata_subgraph& Subgraph,
                      char const* NodeName,
                      dag2& TheDag)
{
    CheckPointerNotNull(NodeName, return 0);
    CheckCondition(NodeName[0] != '\0', return 0);

    char const* EndChar = NodeName + 1;
    while (*EndChar != '\0' && *EndChar != ':')
        ++EndChar;
    Assert(*EndChar == '\0' || *EndChar == ':');

    {for(subgraph_node_iterator Iter = FirstSubgraphNode(Subgraph); Iter != 0;
         Iter = NextSubgraphNode(Subgraph, Iter))
    {
        dag2_node* ItrNode = NodeFromIterator(TheDag, Iter);
        if (StringsAreEqualLowercase(NodeName, EndChar, ItrNode->Name))
        {
            // Found it!  If the end char is at the end of the string, this was the node
            // we're looking for.  Otherwise, recurse.
            if (*EndChar == '\0')
                return ItrNode;
            else
                return FindDag2Node(TheDag, *ItrNode, EndChar+1);
        }
    }}

    Log2(WarningLogMessage, BlendDagLogMessage,
         "In Subgraph node \"%s\": unable to find match for remaining search string \"%s\"\n",
         Node.Name, NodeName);

    return 0;
}

dag2_node* GRANNY
FindDag2Node(dag2& TheDag, dag2_node& Node, char const* NodeName)
{
    CheckPointerNotNull(NodeName, return 0);
    CheckCondition(NodeName[0] != '\0', return 0);

    char const* EndChar = NodeName + 1;
    while (*EndChar != '\0' && *EndChar != ':')
        ++EndChar;

#define DISPATCH_ARGS NodeName, TheDag
#define DISPATCH_RESULT Result
    dag2_node* Result = 0;
    TYPE_DISPATCH(Node)
        NODE_DISPATCH(Subgraph,  subgraph,  FindDag2Node_Subgraph)
        END_TYPE_DISPATCH();
#undef DISPATCH_ARGS
#undef DISPATCH_RESULT

    if (!Result)
    {
        Log2(WarningLogMessage, BlendDagLogMessage,
            "In node \"%s\": unable to find match for remaining search string \"%s\"\n",
            Node.Name, NodeName);
    }

    return Result;
}


static char const*
GetNameFromObject(void* ObjectPtr,
                  int32x const NameOffset)
{
    if (ObjectPtr == NULL)
        return NULL;

    char const** PtrToName = (char const**)((uint8*)ObjectPtr + NameOffset);
    return *PtrToName;
}

static char*
GetNextName(char const* Name)
{
    if (StringLength(Name) == 0)
    {
        return CloneString("0");
    }

    char const* LastDigit = Name + (StringLength(Name) - 1);
    while (LastDigit > Name && IsDecimal(*LastDigit))
        --LastDigit;

    // If LastDigit doesn't equal Name, then it now points to the character /before/ the
    // digits in the name, so advance.  Note that this may make the digit string the empty
    // string.
    if (LastDigit != Name)
        ++LastDigit;

    // Extract the tail number.
    uint32x const EndNumber = ConvertToUInt32(LastDigit);
    uint32x const NextNumber = EndNumber + 1;

    // We're not going to be super tight about the space.  A uint32 always fits within 9
    // digits, so just allocate that much extra space on the end of the name string
    int32x const NameChars = (int32x)(LastDigit - Name);
    int32x RequiredSpace = NameChars + 9 + 1;
    char* NewName = AllocateArray(RequiredSpace, char, AllocationBuilder);
    // todo: handle allocation failure

    Copy(NameChars, Name, NewName);
    uint32 Used = ConvertUInt32ToString(RequiredSpace - NameChars,
                                        NewName + NameChars,
                                        NextNumber, 10);
    // Null terminate the strings
    *(NewName + NameChars + Used) = 0;

    return NewName;
}

static char* MakeAcceptableName(char const*   Proposed,
                                dag2&         TheDag,
                                int32*        NamedObjectIndices,
                                int32x const  NameOffset,
                                int32x const  NumObjects,
                                memory_arena& Arena)
{
    Assert(Proposed);
    CheckPointerNotNull(NamedObjectIndices, return MemoryArenaPushString(Arena, Proposed));

    char* CurrName = CloneString(Proposed);
    while (true)
    {
        bool NameExists = false;
        {for(int32x Idx = 0; !NameExists && Idx < NumObjects; ++Idx)
        {
            char const* OtherName = GetNameFromObject(GetDag2Node(TheDag, NamedObjectIndices[Idx]), NameOffset);
            if (OtherName && StringsAreEqualLowercase(OtherName, CurrName))
                NameExists = true;
        }}

        if (!NameExists)
        {
            char* ReturnString = MemoryArenaPushString(Arena, CurrName);
            Deallocate(CurrName);

            return ReturnString;
        }

        // Ok, that name is no good, let's generate a new one.
        char* NextName = GetNextName(CurrName);
        Deallocate(CurrName);
        CurrName = NextName;
    }
}

void GRANNY
SetNodeName(dag2&         TheDag,
            dag2_node*    Node,
            char const*   NewName,
            memory_arena& Arena)
{
    CheckPointerNotNull(Node, return);
    CheckPointerNotNull(NewName, return);

    // Note that in all cases in which it's valid to call this function, the Node name
    // comes from an arena, so we don't want to free it, just 0 it in case we come around
    // to our name in the MakeAcceptable loop
    Node->Name = 0;

    if (!IsValidNodeIndex(Node->ParentIndex))
    {
        Node->Name = MemoryArenaPushString(Arena, NewName);
    }
    else
    {
        dag2_node* ParentNode = TheDag.Nodes[Node->ParentIndex];
        Assert(ParentNode);

        dag2_nodedata_subgraph* ParentGraph = GetNodeDataSubgraph(ParentNode);
        Node->Name = MakeAcceptableName(NewName,
                                        TheDag, ParentGraph->SubnodeIndices,
                                        OffsetFromType(dag2_node, Name),
                                        ParentGraph->SubnodeIndexCount,
                                        Arena);
    }
}

bool GRANNY
BreakNodeInputConnection(dag2&      TheDag,
                         dag2_node* Node,
                         int32x     InputIndex)
{
    CheckPointerNotNull(Node, return false);
    CheckInt32Index(InputIndex, Node->TotalInputEdgeCount, return false);

    // For an input connection, we only need to detach the pointer in the connection
    // itself
    Node->InputEdges[InputIndex].InputNodeIndex  = NoNodeIndex;
    Node->InputEdges[InputIndex].ConnectedOutput = -1;

    return true;
}

bool GRANNY
BreakNodeOutputConnection(dag2&      TheDag,
                          dag2_node* Node,
                          int32x     OutputIndex)
{
    CheckPointerNotNull(Node, return false);
    CheckInt32Index(OutputIndex, Node->TotalOutputEdgeCount, return false);

    int32x const NodeIndex = GetDag2NodeIndex(TheDag, Node);

    // Output edges are a little tricker.  Since any given output can be connected to
    // multiple inputs, we need to search all of the possible locations to find who's
    // referencing us.  Because of the way we restrict node connections, this boils
    // down to the following.
    //
    //  For external outputs
    //  - Parent internal inputs: (If parent is a subgraph node)
    //  - Siblings external inputs
    //
    //  For internal outputs (node must be a subgraph)
    //  - Children external inputs
    //  - Self internal inputs
    if (Node->OutputEdges[OutputIndex].Internal == 0)
    {
        dag2_node* ParentNode = GetDag2Node(TheDag, Node->ParentIndex);
        if (ParentNode)
        {
            dag2_nodedata_subgraph* Subgraph = GetNodeDataSubgraph(ParentNode);
            CheckPointerNotNull(Subgraph, return false);

            // Check parent's internal inputs
            {for(int32x InputIdx = 0; InputIdx < ParentNode->TotalInputEdgeCount; ++InputIdx)
            {
                if (ParentNode->InputEdges[InputIdx].Internal == 0)
                    continue;

                dag2_input_edge& Edge = ParentNode->InputEdges[InputIdx];
                if (Edge.InputNodeIndex == NodeIndex && Edge.ConnectedOutput == OutputIndex)
                {
                    Edge.InputNodeIndex = NoNodeIndex;
                    Edge.ConnectedOutput = -1;
                }
            }}

            // Check sibling external inputs
            {for(subgraph_node_iterator Iter = FirstSubgraphNode(*Subgraph); Iter != 0;
                 Iter = NextSubgraphNode(*Subgraph, Iter))
            {
                dag2_node* Sibling = NodeFromIterator(TheDag, Iter);
                if (Sibling == Node)
                    continue;

                {for(int32x InputIdx = 0; InputIdx < Sibling->TotalInputEdgeCount; ++InputIdx)
                {
                    if (Sibling->InputEdges[InputIdx].Internal != 0)
                        continue;

                    dag2_input_edge& Edge = Sibling->InputEdges[InputIdx];
                    if (Edge.InputNodeIndex == NodeIndex && Edge.ConnectedOutput == OutputIndex)
                    {
                        Edge.InputNodeIndex = NoNodeIndex;
                        Edge.ConnectedOutput = -1;
                    }
                }}
            }}
        }
    }
    else
    {
        dag2_nodedata_subgraph* Subgraph = GetNodeDataSubgraph(Node);
        CheckPointerNotNull(Subgraph, return false);

        {for(subgraph_node_iterator Iter = FirstSubgraphNode(*Subgraph); Iter;
             Iter = NextSubgraphNode(*Subgraph, Iter))
        {
            dag2_node* Child = NodeFromIterator(TheDag, Iter);
            Assert(Child);

            {for(int32x InputIdx = 0; InputIdx < Child->TotalInputEdgeCount; ++InputIdx)
            {
                if (Child->InputEdges[InputIdx].Internal != 0)
                    continue;

                dag2_input_edge& Edge = Child->InputEdges[InputIdx];
                if (Edge.InputNodeIndex == NodeIndex && Edge.ConnectedOutput == OutputIndex)
                {
                    Edge.InputNodeIndex = NoNodeIndex;
                    Edge.ConnectedOutput = -1;
                }
            }}
        }}

        {for(int32x InputIdx = 0; InputIdx < Node->TotalInputEdgeCount; ++InputIdx)
        {
            if (!Node->InputEdges[InputIdx].Internal)
                continue;

            dag2_input_edge& Edge = Node->InputEdges[InputIdx];
            if (Edge.InputNodeIndex == NodeIndex && Edge.ConnectedOutput == OutputIndex)
            {
                Edge.InputNodeIndex = NoNodeIndex;
                Edge.ConnectedOutput = -1;
            }
        }}
    }

    return true;
}
