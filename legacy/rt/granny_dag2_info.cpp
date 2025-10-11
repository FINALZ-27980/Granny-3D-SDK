// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_info.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_dag2_info.h"

#include "granny_assert.h"
#include "granny_dag2.h"
#include "granny_dag2_node_registry.h"
#include "granny_data_type_conversion.h"
#include "granny_file.h"
#include "granny_file_format.h"
#include "granny_memory.h"
#include "granny_parameter_checking.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

#define SubsystemCode BlendDagLogMessage
USING_GRANNY_NAMESPACE;

BEGIN_GRANNY_NAMESPACE;

data_type_definition Dag2InfoType[] =
{
    {ArrayOfReferencesMember, "DAGs", Dag2Type},
    {VariantReferenceMember,  "ExtendedData"},

    {EndMember},
};

END_GRANNY_NAMESPACE;

#define ExtractCountAndPtr(Count, Pointer, Type, Memory)        \
    do {                                                        \
        Count   = *((int32*)(Memory));                          \
        Pointer = *((Type*)((uint8*)Memory + SizeOf(int32)));   \
    } while (false)


static int32x
MakeNodeDataCurrent(variant& Variant, uint8* Memory)
{
    CheckPointerNotNull(Variant.Type, return 0);

    data_type_definition* RealType =
        GetDag2NodeDataTypeFromTypeMarker(Variant.Type[0].Name);
    CheckPointerNotNull(RealType, return 0);

    if (DataTypesAreEqual(Variant.Type, RealType))
        return 0;

    int32x ConvertedTreeSize =
        GetConvertedTreeSize(Variant.Type, Variant.Object, RealType, 0);
    if (Memory != 0)
    {
        Variant.Object =
            ConvertTreeInPlace(Variant.Type, Variant.Object, RealType, Memory, 0);
        Variant.Type = RealType;
    }

    return ConvertedTreeSize;
}


dag2_info* GRANNY
GetDag2Info(file& File)
{
    variant Root;
    GetDataTreeFromFile(File, &Root);

    uint32 TypeTag = File.Header->TypeTag;
    if ((TypeTag & 0xC0000000) != 0xC0000000)
    {
        Log1(WarningLogMessage, FileReadingLogMessage,
             "File has run-time type tag of 0x%x, which doesn't look like a DAG.",
             TypeTag);
        return 0;
    }

    // We have to go through and make sure that the type markers in the dags are correct,
    // given the current state of the node data type dispatch table, so no immediate
    // return below...
    dag2_info* ReturnValue = 0;

    if(TypeTag == CurrentDAGStandardTag)
    {
        ReturnValue = (dag2_info*)Root.Object;
    }
    else
    {
        if (!File.ConversionBuffer)
        {
            Log2(WarningLogMessage, FileReadingLogMessage,
                 "File has run-time type tag of 0x%x, which doesn't match this version "
                 "of Granny (0x%x).  Automatic conversion will be attempted.",
                 TypeTag, CurrentDAGStandardTag);

            // We have to dance a bit here, since we need to convert the variants in the
            // dag into the same memory buffer.  Assessing the size of the buffer needed
            // is a bit tricky.
            int32x const RootTreeSize =
                GetConvertedTreeSize(Root.Type, Root.Object, Dag2InfoType, 0);
            int32x ConversionBufferSize = RootTreeSize;

            // Find the dags first...
            variant dags_member;
            variant dagnodes_member;
            if (!FindMatchingMember(Root.Type, Root.Object, "DAGs", &dags_member) ||
                !FindMatchingMember(dags_member.Type->ReferenceType, 0, "Nodes", &dagnodes_member))
            {
                InvalidCodePath("GetDag2Info: Unable to find touchstone members for conversion.");
                return 0;
            }

            // Do a throwaway conversion to figure out the exact size of the buffer once the variants are converted
            {
                dag2_info ThrowAwayInfo;
                ConvertSingleObject(Root.Type, Root.Object, Dag2InfoType, &ThrowAwayInfo, 0);

                // Handle this carefully for 64bit...
                int32 Count;
                void const **DAGArray;
                ExtractCountAndPtr(Count, DAGArray, void const**, dags_member.Object);
                UNUSED_VARIABLE(Count);

                {for(int32x DAGIdx = 0; DAGIdx < ThrowAwayInfo.DAGCount; ++DAGIdx)
                {
                    if (!DAGArray[DAGIdx])
                        continue;

                    dag2 ThrowAwayDag;
                    ConvertSingleObject(dags_member.Type->ReferenceType,
                                        DAGArray[DAGIdx],
                                        Dag2Type, &ThrowAwayDag, 0);

                    {for(int32x NodeIdx = 0; NodeIdx < ThrowAwayDag.NodeCount; ++NodeIdx)
                    {
                        if (!ThrowAwayDag.Nodes[NodeIdx])
                            continue;

                        dag2_node ThrowAwayNode;
                        ConvertSingleObject(dagnodes_member.Type->ReferenceType,
                                            ThrowAwayDag.Nodes[NodeIdx],
                                            Dag2NodeType, &ThrowAwayNode, 0);

                        ConversionBufferSize += MakeNodeDataCurrent(ThrowAwayNode.NodeData, 0);
                    }}
                }}
            }

            // Ok, now the real deal.  Significantly easier, since we can use the
            // converted tree to access the nodes.
            {
                uint8 *DestTree = AllocateArray(ConversionBufferSize, uint8, AllocationBuilder);
                if (!DestTree)
                {
                    Log1(ErrorLogMessage, FileReadingLogMessage,
                         "GetDag2Info: Unable to allocate a buffer of %d bytes for conversion",
                         ConversionBufferSize);
                    return 0;
                }

                File.ConversionBuffer = ConvertTreeInPlace(Root.Type, Root.Object, Dag2InfoType, DestTree, 0);
                uint8* CurrentVariantPos = DestTree + RootTreeSize;

                dag2_info* Info = (dag2_info*)File.ConversionBuffer;

                {for(int32x DagIdx = 0; DagIdx < Info->DAGCount; ++DagIdx)
                {
                    dag2* Dag = Info->DAGs[DagIdx];
                    if (Dag == 0)
                        continue;

                    {for(int32x NodeIdx = 0; NodeIdx < Dag->NodeCount; ++NodeIdx)
                    {
                        if (Dag->Nodes[NodeIdx] == 0)
                            continue;

                        CurrentVariantPos +=
                            MakeNodeDataCurrent(Dag->Nodes[NodeIdx]->NodeData, CurrentVariantPos);
                    }}
                }}
            }
        }

        ReturnValue = (dag2_info*)File.ConversionBuffer;
    }

    if (ReturnValue)
    {
        {for(int32x DagIdx = 0; DagIdx < ReturnValue->DAGCount; ++DagIdx)
        {
            dag2* DAG = ReturnValue->DAGs[DagIdx];
            if (!DAG)
                continue;

            {for(int32x NodeIdx = 0; NodeIdx < DAG->NodeCount; ++NodeIdx)
            {
                dag2_node* Node = DAG->Nodes[NodeIdx];
                if (!Node)
                    continue;

                if (!Node->NodeData.Type || !Node->NodeData.Object)
                    continue;

                *((int32*)Node->NodeData.Object) =
                    GetDag2NodeDataTypeIndexFromTypeMarker(Node->NodeData.Type[0].Name);
            }}
        }}
    }

    return ReturnValue;
}

