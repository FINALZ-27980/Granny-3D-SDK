// ========================================================================
// $File: //jeffr/granny_29/statement/statement_editstate.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#if !defined(STATEMENT_EDITSTATE_H)

#ifndef GRANNY_TYPES_H
#include "granny_types.h"
#endif

#ifndef GSTATE_BASE_H
#include "gstate_base.h"
#endif

#include <vector>
#include <string>
#include <windows.h>

BEGIN_GSTATE_NAMESPACE;
class container;
class node;
class state_machine;
class tokenized;
class transition;
struct animation_slot;
struct animation_spec;
struct source_file_ref;
END_GSTATE_NAMESPACE;

struct lua_State;

BEGIN_GRANNY_NAMESPACE;

struct animation;
struct file;
struct file_writer;

namespace edit
{
    void StoreWindowPos();
    bool RecallWindowPos(RECT*, uint32* Style);

    bool IsModified();
    void MarkModified();

    bool Save(bool QueryAlways);
    bool CheckForSaveModified();
    bool Load();
    bool LoadFromFilename(char const* Filename);
    bool ResetMachines();
    bool ResetAll(bool Force);
    bool CreateDefaultFile();

    bool ResetFromFileInfoPair(file* File, gstate_character_info* NewInfo,
                               char const* FileNameIfDisk);
    bool FlattenStateToWriter(file_writer* Writer, int32x SizeGuess);

    bool SetCharacterBase(char const* Filename, int ModelIdx);
    GSTATE source_file_ref* AddAnimationFile(char const* Filename);

    // Turns relative paths in source_file_refs into absolute paths.
    std::string GetSourcePath(char const* SourceFilename);

    void LookForChangedSources();
    
    // Utility
    animation* GetAnimationForCurrSet(int32x SourceIndex, int32x AnimIndex);

    GSTATE state_machine* GetRootMachine();

    gstate_character_instance* GetCharacterInstance();
    real32 GetCharacterOrGlobalTime();

    int32 GetCurrentAnimationSet();
    bool  SetCurrentAnimationSet(int32 SetIdx);

    void GetAnimationSlots(std::vector<GSTATE animation_slot*>* Slots);
    void GetAnimationSpecs(std::vector<GSTATE animation_spec>* Specs);

    bool CanEditSlot(GSTATE animation_slot*);
    void EditSlot(GSTATE animation_slot*);
    
    // Note that this is not necessarily the same as the root machine, since we may have
    // descended into a blend_graph, or sub-state machine
    GSTATE container* GetCurrentContainer();
    bool              SetCurrentContainer(GSTATE container* NewContainer);

    // Selection maintainance
    bool SetSelection(GSTATE tokenized* SelectedNode);
    bool RemoveFromSelection(GSTATE tokenized* SelectedNode);
    bool AddToSelection(GSTATE tokenized* SelectedNode);
    void ClearSelection();
    bool IsSelected(GSTATE tokenized*);
    bool GetSelection(std::vector<GSTATE tokenized*>& Selection);

    // Centralize these so we can change them if necessary...
    int32x TokenizedToID(GSTATE tokenized*);
    GSTATE tokenized*  IDToTokenized(int32x ID);
    GSTATE node*       IDToNode(int32x ID);
    GSTATE transition* IDToTransition(int32x ID);

    void GetNodeIcons(GSTATE node*, int32x& Large, int32x& Small);
}

bool Edit_Register(lua_State* State);

END_GRANNY_NAMESPACE;

#define STATEMENT_EDITSTATE_H
#endif /* STATEMENT_EDITSTATE_H */
