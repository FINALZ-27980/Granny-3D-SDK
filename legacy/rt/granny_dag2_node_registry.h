#if !defined(GRANNY_DAG2_NODE_REGISTRY_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_node_registry.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE;

struct data_type_definition;

data_type_definition* GetDag2NodeDataTypeFromTypeMarker(char const*);
int32x GetDag2NodeDataTypeIndexFromTypeMarker(char const*);
bool RegisterCustomNodeType(data_type_definition*);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_DAG2_NODE_REGISTRY_H
#endif
