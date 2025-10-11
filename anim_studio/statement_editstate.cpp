// ========================================================================
// $File: //jeffr/granny_29/statement/statement_editstate.cpp $
// $DateTime: 2012/10/04 14:42:10 $
// $Change: 39675 $
// $Revision: #30 $
//
// $Notice: $
// ========================================================================
#include "statement_editstate.h"
#include "statement.h"
#include <shlwapi.h>

#include "granny_animation.h"
#include "granny_data_type_io.h"
#include "granny_file.h"
#include "granny_file_builder.h"
#include "granny_file_format.h"
#include "granny_file_info.h"
#include "granny_file_writer.h"
#include "granny_file_writer.h"
#include "granny_log.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_mesh.h"
#include "granny_mesh_binding.h"
#include "granny_model.h"
#include "granny_model_instance.h"
#include "granny_skeleton.h"
#include "granny_string.h"
#include "granny_system_clock.h"
#include "granny_track_group.h"
#include "granny_track_mask.h"

#include "luautils.h"
#include "source_monitor.h"
#include "statement_fileops.h"
#include "ui_blendnode_edit.h"
#include "ui_transition_edit.h"
#include "ui_statemachine_edit.h"
#include "ui_character_render.h"
#include "ui_controls.h"
#include "ui_core.h"
#include "ui_preferences.h"

#include "gstate_node_header_list.h"
#include "gstate_character_info.h"
#include "gstate_character_instance.h"
#include "gstate_token_context.h"
#include "gstate_transition_onrequest.h"

#include "statement_undostack.h"
#include "statement_xinput.h"

// Ok, we need to create this, so we count as internal.  :)
#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include <Shellapi.h>
#include <algorithm>
#include <vector>
#include <map>
#include <string>
#include <utility>

USING_GRANNY_NAMESPACE;
USING_GSTATE_NAMESPACE;
using namespace std;


BEGIN_GRANNY_NAMESPACE;
namespace edit
{
    bool StateModified = false;

    typedef std::pair<string, string> SpecPathPair;

    // Registry keys
    int const c_NumRecentFiles = 8;
    LPCTSTR c_RecentGSF   = TEXT("RecentFileList");
    LPCTSTR c_RecentModel = TEXT("RecentModelsList");
    LPCTSTR c_WindowL     = TEXT("WindowL");
    LPCTSTR c_WindowR     = TEXT("WindowR");
    LPCTSTR c_WindowT     = TEXT("WindowT");
    LPCTSTR c_WindowB     = TEXT("WindowB");
    LPCTSTR c_WindowStyle = TEXT("WindowStyle");

    vector<SpecPathPair> RecentFiles;
    vector<SpecPathPair> RecentModels;

    // This is the file that we loaded the current file from.  It must hang around until
    // all root machines that were instantiated/modified from that file are deleted.
    file*  LoadedFileBacking = 0;
    string FileBackingAbsolutePath;

    state_machine* RootMachine;
    container*     CurrentContainer;

    struct simulated_animation_set
    {
        animation_set* AnimationSet;

        vector<source_file_ref*> SourceFileRefs;
        vector<animation_spec>   AnimationSpecs;
    };
    vector<simulated_animation_set*> AnimationSets;
    int                              CurrentAnimationSet = -1;

    vector<animation_set*>           AnimationSetArray;
    vector<animation_slot*>          AnimationSlots;


    // Sometimes we allocate these and need to free them.  Sometimes they come from the
    // file, and it's hands off!  This is a somewhat clunky way to tell the difference.
    vector<source_file_ref*> AllocatedFileRefs;
    vector<animation_spec*>  AllocatedSpecs;
    vector<animation_slot*>  AllocatedSlots;
    vector<animation_set*>   AllocatedSets;

    // This deserves some discussion.  This is a *fake* character_info, used to
    // instantiate a character_instance.  The arrays contained herein are directed to
    // RootMachines and RootFileReferences, so they need not be deleted, but they must be
    // *created* when LoadedFileBacking is first loaded.  Whenever RootMachines is
    // touched, this must be updated, to ensure that the pointer is correct.
    gstate_character_info  CurrentInfo = { 0 };


    // Info about our exemplary character
    string                ModelFilenames;    // multiple files, potentially.  Split by ':' character
    granny_int32x         ModelIndex = -1;

    vector<model*>        Models;
    vector<file*>         ModelFiles;
    vector<file_info*>    ModelFileInfos;
    model*                Model = 0;

    gstate_character_instance* CharacterInstance = 0;

    vector<mesh_binding*> BoundMeshes;

    // Currently selected nodes in the editor.  Can be nodes, or transitions
    vector<tokenized*>          CurrentSelection;

    // Ghetto file cache...
    typedef map<string, file*> FileCache;
    FileCache s_FileCache;
} // namespace edit
END_GRANNY_NAMESPACE;
using namespace GRANNY edit;

static void
ZeroCurrentInfo()
{
    GStateDeallocate(CurrentInfo.ModelNameHint);
}



// =============================================================================
// Ghetto file cache..
// =============================================================================
static file*
FileFromCache(char const* Filename)
{
    string nameAsString = Filename;
    FileCache::iterator itr = s_FileCache.find(nameAsString);
    if (itr != s_FileCache.end())
        return itr->second;

    file* File = ReadEntireFile(Filename);
    s_FileCache[nameAsString] = File;
    return File;
}

static void
ReplaceCachedFile(string const& Path, file* File)
{
    FileCache::iterator itr = s_FileCache.find(Path);
    Assert(itr != s_FileCache.end());

    FreeFile(itr->second);
    itr->second = File;
}

static void
FlushFileCache()
{
    // todo: Note that this assumes that the references have been cleared, may want to add
    // ref counting to be sure...
    {for(FileCache::iterator itr = s_FileCache.begin(); itr != s_FileCache.end(); ++itr)
    {
        FreeFile(itr->second);
        itr->second = 0;
    }}

    s_FileCache.clear();
}
// =============================================================================

static int
GetSourceIndexFromRef(source_file_ref* Ref)
{
    if (!Ref)
        return -1;

    Assert(CurrentAnimationSet >= 0 && CurrentAnimationSet < SizeI(AnimationSets));
    simulated_animation_set* SimSet = AnimationSets[CurrentAnimationSet];

    {for (size_t Idx = 0; Idx < SimSet->SourceFileRefs.size(); ++Idx)
    {
        // found it?
        if (SimSet->SourceFileRefs[Idx] == Ref)
            return Idx;
    }}

    return -1;
}



static void
SetRootMachine(state_machine* NewRoot)
{
    // UNDONOTE: We don't undo across this boundary, since the undo system itself calls
    // this.  Caller is responsible for undo

    // Null this out first...
    CurrentInfo.StateMachine.Type = 0;
    CurrentInfo.StateMachine.Object = 0;

    if (CharacterInstance)
    {
        ReplaceStateMachine(CharacterInstance, 0);
    }

    ClearSelection();


    RootMachine = NewRoot;
    CurrentContainer = NewRoot;

    // If there's no root, nothing to do.
    if (RootMachine == 0)
        return;

    // Update the dummy character info
    RootMachine->GetTypeAndToken(&CurrentInfo.StateMachine);

    if (!CharacterInstance && Model)
    {
        // Well, why don't we create one!
        CharacterInstance = InstantiateEditCharacter(&CurrentInfo, CurrentAnimationSet, (granny_model*)Model);
    }

    if (CharacterInstance)
        ReplaceStateMachine(CharacterInstance, RootMachine);

    // Debug guard
    RootMachine->CheckConnections();
}



// A little utility to make sure the character is properly unbound during destructive
// operations, and rebound after
class CharacterBindGuard
{
public:
    CharacterBindGuard()
    {
        mContainerStore = CurrentContainer;
        mMachineStore = RootMachine;
        if (CharacterInstance)
        {
            SetRootMachine(0);
        }
    }

    ~CharacterBindGuard()
    {
        if (CharacterInstance)
        {
            SetCurrentAnimationSet(CharacterInstance, CurrentAnimationSet);
            SetRootMachine(mMachineStore);

            if (mContainerStore != 0 && mContainerStore != CurrentContainer)
            {
                container* Walk = mContainerStore;
                while (Walk)
                {
                    if (Walk == RootMachine)
                        break;
                    Walk = Walk->GetParent();
                }

                // Really weird if this isn't true anymore...
                Assert(Walk && Walk == RootMachine);
                SetCurrentContainer(mContainerStore);
            }
        }

    }

private:
    container*     mContainerStore;
    state_machine* mMachineStore;

    CharacterBindGuard(CharacterBindGuard const&);
    CharacterBindGuard& operator=(CharacterBindGuard const&);
};




void GRANNY
edit::StoreWindowPos()
{
    HWND hwnd = UIGetHWnd();
    RECT r;
    if (!GetWindowRect(hwnd, &r))
        return;

    if (IsZoomed(hwnd))
    {
        int Style = WS_OVERLAPPEDWINDOW | WS_MAXIMIZE;
        SetApplicationInt(c_WindowStyle, Style);
    }
    else
    {
        DWORD Style = WS_OVERLAPPEDWINDOW;
        SetApplicationInt(c_WindowL, r.left);
        SetApplicationInt(c_WindowR, r.right);
        SetApplicationInt(c_WindowT, r.top);
        SetApplicationInt(c_WindowB, r.bottom);
        SetApplicationInt(c_WindowStyle, Style);
    }
}

bool GRANNY
edit::RecallWindowPos(RECT* Rect, uint32* Style)
{
    bool AllSucceeded = true;
    AllSucceeded = GetApplicationInt(c_WindowL, 0,    (uint32*)&Rect->left)              && AllSucceeded;
    AllSucceeded = GetApplicationInt(c_WindowR, 1440, (uint32*)&Rect->right)             && AllSucceeded;
    AllSucceeded = GetApplicationInt(c_WindowT, 0,    (uint32*)&Rect->top)               && AllSucceeded;
    AllSucceeded = GetApplicationInt(c_WindowB, 800,  (uint32*)&Rect->bottom)            && AllSucceeded;
    AllSucceeded = GetApplicationInt(c_WindowStyle, WS_OVERLAPPEDWINDOW, Style) && AllSucceeded;

    if (AllSucceeded)
    {
        HMONITOR hMonitor;
        MONITORINFO mi;

        //
        // get the nearest monitor to the passed rect.
        //
        hMonitor = MonitorFromRect(Rect, MONITOR_DEFAULTTONEAREST);

        // get the work area or entire monitor rect.
        //
        mi.cbSize = sizeof(mi);
        GetMonitorInfo(hMonitor, &mi);

        // Adjust to make sure that the rect is contained in the work rect...
        if (Rect->left < mi.rcWork.left)
        {
            int Adjust = mi.rcWork.left - Rect->left;
            Rect->left  += Adjust;
            Rect->right += Adjust;
        }
        if (Rect->top < mi.rcWork.top)
        {
            int Adjust = mi.rcWork.top - Rect->top;
            Rect->top    += Adjust;
            Rect->bottom += Adjust;
        }

        if (Rect->right > mi.rcWork.right)
        {
            // Adjust left as much as possible
            int maxAdjust = Rect->left - mi.rcWork.left;
            int Adjust    = min(maxAdjust, Rect->right - mi.rcWork.right);

            Rect->left  -= Adjust;
            Rect->right -= Adjust;

            // Clamp anything else...
            if (Rect->right > mi.rcWork.right)
                Rect->right = mi.rcWork.right;
        }

        if (Rect->bottom > mi.rcWork.bottom)
        {
            // Adjust top as much as possible
            int maxAdjust = Rect->top - mi.rcWork.top;
            int Adjust    = min(maxAdjust, Rect->bottom - mi.rcWork.bottom);

            Rect->top  -= Adjust;
            Rect->bottom -= Adjust;

            // Clamp anything else...
            if (Rect->bottom > mi.rcWork.bottom)
                Rect->bottom = mi.rcWork.bottom;
        }
    }


    if (!AllSucceeded)
    {
        AdjustWindowRect(Rect, *Style, FALSE);
        return false;
    }

    // Convert from param array
    return true;
}


string GRANNY
edit::GetSourcePath(char const* SourceFilename)
{
    if (FileBackingAbsolutePath.empty())
        return string(SourceFilename);

    // TEMPORARY HACK TO ADDRESS OLD DATA
    {
        if (SourceFilename && !PathIsRelative(SourceFilename))
        {
            return string(SourceFilename);
        }
    }

    char FromDir[MAX_PATH];
    strcpy(FromDir, FileBackingAbsolutePath.c_str());
    PathRemoveFileSpec(FromDir);

    char AbsPath[MAX_PATH];
    PathCombine(AbsPath, FromDir, SourceFilename);

    return string(AbsPath);
}

static string
GetSourceFileSpec(char const* SourceFilename)
{
    char AbsPath[MAX_PATH];
    strcpy_s(AbsPath, SourceFilename);

    PathStripPath(AbsPath);
    return string(AbsPath);
}

static void
BackSlashes(char* Filename)
{
    while (*Filename)
    {
        if (*Filename == '/')
            *Filename = '\\';

        ++Filename;
    }
}

static char*
MakeRelativeFilePath(char const* Filename)
{
    char UsePath[MAX_PATH];
    strcpy(UsePath, Filename);
    BackSlashes(UsePath);

    char* RetVal = 0;
    if (FileBackingAbsolutePath.empty())
    {
        GStateCloneString(RetVal, UsePath);
    }
    else
    {
        char Path[MAX_PATH];
        if (!PathRelativePathTo(Path,
                                FileBackingAbsolutePath.c_str(), 0,
                                UsePath, 0))
        {
            // Error?
            GStateCloneString(RetVal, UsePath);
        }
        else
        {
            GStateCloneString(RetVal, Path);
        }
    }

    Assert(RetVal);
    BackSlashes(RetVal);
    return RetVal;
}

static void
RebaseSourceRef(source_file_ref* Ref,
                string&          FromPath,
                string&          ToPath)
{
    BackSlashes(Ref->SourceFilename);

    if (!PathIsRelative(Ref->SourceFilename))
    {
        // Relative path from the to path without rebasing
        char Path[MAX_PATH];
        if (PathRelativePathTo(Path,
                               ToPath.c_str(), 0,
                               Ref->SourceFilename, 0))
        {
            char Canonical[MAX_PATH];
            PathCanonicalize(Canonical, Path);
            GStateDeallocate(Ref->SourceFilename);
            GStateCloneString(Ref->SourceFilename, Path);
        }

        return;
    }

    if (FromPath.empty())
    {
        // Erm.  The Ref is already relative? Not sure what to do with this.
        // leave it alone for now
    }
    else
    {
        char FromDir[MAX_PATH];
        strcpy(FromDir, FromPath.c_str());
        PathRemoveFileSpec(FromDir);

        char AbsPath[MAX_PATH];
        PathCombine(AbsPath, FromDir, Ref->SourceFilename);

        char Path[MAX_PATH];
        if (PathRelativePathTo(Path,
                               ToPath.c_str(), 0,
                               AbsPath, 0))
        {
            GStateDeallocate(Ref->SourceFilename);
            GStateCloneString(Ref->SourceFilename, Path);
        }
    }
}

static void
DumpFileList(LPCTSTR value, vector<SpecPathPair>& Files)
{
    StTMPrintf("Dumping into registry key: %s (%d values)", StTMDynString(value), Files.size());
    string KeyData;
    {for (size_t Idx = 0; Idx < Files.size(); ++Idx)
    {
        StTMPrintf("  %d: \"%s\",\"%s\"", Idx, StTMDynString(Files[Idx].first.c_str()), StTMDynString(Files[Idx].second.c_str()));

        KeyData += Files[Idx].first;
        KeyData += '|';
        KeyData += Files[Idx].second;
        KeyData += '|';
    }}

    SetApplicationString(value, KeyData.c_str());
}

static void
WriteRecentFileList()
{
    DumpFileList(c_RecentGSF, RecentFiles);
    DumpFileList(c_RecentModel, RecentModels);
}

static void
ParseRecentBuffer(char const* RecentBuffer,
                  vector<SpecPathPair>& Entries)
{
    Entries.clear();

    int Curr = 0;
    while (RecentBuffer[Curr] != 0)
    {
        SpecPathPair newEntry;

        // slow slow slow but who cares.
        while(RecentBuffer[Curr] != '|' && RecentBuffer[Curr] != 0)
            newEntry.first += RecentBuffer[Curr++];
        if (RecentBuffer[Curr] != 0)
        {
            Assert(RecentBuffer[Curr] == '|');
            ++Curr;
        }

        while(RecentBuffer[Curr] != '|' && RecentBuffer[Curr] != 0)
            newEntry.second += RecentBuffer[Curr++];
        if (RecentBuffer[Curr] != 0)
        {
            Assert(RecentBuffer[Curr] == '|');
            ++Curr;
        }

        Entries.push_back(newEntry);
    }
}

static void
LoadRecentFileList()
{
    // Recent state machines
    {
        string RecentGSF;
        if (GetApplicationString(c_RecentGSF, "", &RecentGSF))
        {
            StTMPrintf("Parsing into RecentFiles");
            StTMPrintf("  \"%s\"", StTMDynString(RecentGSF.c_str()));
            ParseRecentBuffer(RecentGSF.c_str(), RecentFiles);
        }
    }

    // Recent state machines
    {
        string RecentModel;
        if (GetApplicationString(c_RecentModel, "", &RecentModel))
        {
            StTMPrintf("Parsing into RecentModels");
            StTMPrintf("  \"%s\"", StTMDynString(RecentModel.c_str()));
            ParseRecentBuffer(RecentModel.c_str(), RecentModels);
        }
    }
}

static void
ClearRecentFileList()
{
    RecentFiles.clear();
    WriteRecentFileList();
}

static void
UpdateRecentFileList(char const* Filename, vector<SpecPathPair>& List)
{
    // Slice off everything but the filename (use the first, if there are multiple)
    string Fullname = Filename;
    string Splitname = Fullname.substr(0, Fullname.find_first_of(';'));

    string ShortFilename = GetSourceFileSpec(Splitname.c_str());
    string SourcePath    = GetSourcePath(Filename);

    // Search for the item in the list
    bool Found = false;
    {for (size_t Idx = 0; Idx < List.size(); ++Idx)
    {
        if (StringsAreEqualLowercase(List[Idx].second.c_str(), SourcePath.c_str()))
        {
            // Move to front
            Found = true;
            SpecPathPair Move = List[Idx];
            List.erase(List.begin() + Idx);
            List.insert(List.begin(), Move);
        }
    }}

    if (!Found)
    {
        List.insert(List.begin(), SpecPathPair(ShortFilename, SourcePath));
        if (List.size() > c_NumRecentFiles)
            List.pop_back();
    }

    WriteRecentFileList();
}

static void
RemoveFromRecentFilelist(char const* Filename, vector<SpecPathPair>& List)
{
    // Slice off everything but the filename
    string Filespec = GetSourceFileSpec(Filename);

    // Search for the item in the list
    {for (size_t Idx = 0; Idx < List.size(); ++Idx)
    {
        if (StringsAreEqualLowercase(List[Idx].first.c_str(), Filespec.c_str()))
        {
            List.erase(List.begin() + Idx);
            return;
        }
    }}
}

static void
ClearModelAndMeshes()
{
    // UNDONOTE: We don't undo across this boundary, since the undo system itself calls
    // this.  Caller is responsible for undo

    if (CharacterInstance)
    {
        ReplaceStateMachine(CharacterInstance, 0);
        GStateFreeCharacterInstance(CharacterInstance);
        CharacterInstance = 0;
    }

    {for (size_t Idx = 0; Idx < BoundMeshes.size(); ++Idx)
    {
        FreeMeshBinding(BoundMeshes[Idx]);
    }}
    BoundMeshes.clear();

    ModelFilenames.clear();
    ModelIndex = -1;

    Models.clear();
    ModelFileInfos.clear();
    Model = 0;

    {for (size_t Idx = 0; Idx < ModelFiles.size(); ++Idx)
    {
        FreeFile(ModelFiles[Idx]);
        ModelFiles[Idx] = 0;
    }}
    ModelFiles.clear();

    // Delete the model hint
    GStateDelete(CurrentInfo.ModelNameHint);
    CurrentInfo.ModelNameHint = 0;
}

static void
ClearAnimations()
{
    // UNDONOTE: We don't undo across this boundary, since the undo system itself calls
    // this.  Caller is responsible for undo

    // Remove the animation slots, we just need to delete the name.  Cleanup for allocated
    // slots happens below...
    {for (size_t Idx = 0; Idx < AnimationSlots.size(); ++Idx)
    {
        GStateDeallocate(AnimationSlots[Idx]->Name);
    }}
    AnimationSlots.clear();

    {for (size_t AniSetIdx = 0; AniSetIdx < AnimationSets.size(); ++AniSetIdx)
    {
        simulated_animation_set* SimSet = AnimationSets[AniSetIdx];

        // Nuke the animation specs first...
        {for (size_t Idx = 0; Idx < SimSet->AnimationSpecs.size(); ++Idx)
        {
            GStateDeallocate(SimSet->AnimationSpecs[Idx].ExpectedName);
        }}
        SimSet->AnimationSpecs.clear();

        {for (size_t Idx = 0; Idx < SimSet->SourceFileRefs.size(); ++Idx)
        {
            StopMonitoring(SimSet->SourceFileRefs[Idx]);
            GStateDeallocate(SimSet->SourceFileRefs[Idx]->SourceFilename);
        }}
        SimSet->SourceFileRefs.clear();

        GStateDeallocate(SimSet->AnimationSet->Name);
        SimSet->AnimationSet = 0;
    }}
    AnimationSets.clear();
    AnimationSetArray.clear();
    CurrentAnimationSet = -1;

    // Clear out the allocated buffers.
    {for (size_t Idx = 0; Idx < AllocatedFileRefs.size(); ++Idx)
    {
        GStateDeallocate(AllocatedFileRefs[Idx]);
    }}
    AllocatedFileRefs.clear();
    {for (size_t Idx = 0; Idx < AllocatedSpecs.size(); ++Idx)
    {
        GStateDeallocate(AllocatedSpecs[Idx]);
    }}
    AllocatedSpecs.clear();
    {for (size_t Idx = 0; Idx < AllocatedSlots.size(); ++Idx)
    {
        GStateDeallocate(AllocatedSlots[Idx]);
    }}
    AllocatedSlots.clear();
    {for (size_t Idx = 0; Idx < AllocatedSets.size(); ++Idx)
    {
        GStateDeallocate(AllocatedSets[Idx]);
    }}
    AllocatedSets.clear();

    // Just to be sure...
    CurrentInfo.AnimationSetCount = 0;
    CurrentInfo.AnimationSets     = 0;

    CurrentInfo.AnimationSlotCount = 0;
    CurrentInfo.AnimationSlots     = 0;
}


bool GRANNY
edit::IsModified()
{
    return StateModified;
}

void GRANNY
edit::MarkModified()
{
    StateModified = true;
}

bool GRANNY
edit::FlattenStateToWriter(file_writer* Writer, int32x SizeGuess)
{
    if (RootMachine == 0)
        return false;
    StTMZone(__FUNCTION__);

    // We can just write out our bogos character info.  HOWEVER!  Because of the
    // EmptyReferenceMember fields in source_file_ref, we need to store off a copy of the
    // original pointers.  The file writing process will null those pointers for
    // consistency.
    vector<vector<source_file_ref>*> StoredReferences;
    {for (size_t AnimSetIdx = 0; AnimSetIdx < AnimationSets.size(); ++AnimSetIdx)
    {
        simulated_animation_set* SimSet = AnimationSets[AnimSetIdx];

        StoredReferences.push_back(new vector<source_file_ref>);
        vector<source_file_ref>& Refs = *StoredReferences.back();

        {for (size_t Idx = 0; Idx < SimSet->SourceFileRefs.size(); ++Idx)
        {
            Refs.push_back(*SimSet->SourceFileRefs[Idx]);
        }}
    }}

    // We also need to count the unique tokenized items so we can presize the instance
    // token_context at runtime.
    {
        granny_int32x Count = token_context::GetGlobalContext()->UniqueCount();
        CurrentInfo.NumUniqueTokenized = Count;
    }

    // Encode the input schemes in the EditorData field.  For now, that's all there is in
    // there, so go ahead and just send in the variant...
    {
        ZeroStructure(CurrentInfo.EditorData);
        CreateEncodedXInput((variant*)&CurrentInfo.EditorData);
    }

    // Todo: and fill in the byte count required to instantiate

    bool Success = false;
    {
        file_builder *Builder =
            BeginFileInMemory(StandardSectionCount, GStateCharacterInfoTag,
                              GRNFileMV_ThisPlatform, SizeGuess);

        file_data_tree_writer *TreeWriter =
            BeginFileDataTreeWriting((data_type_definition*)GStateCharacterInfoType, &CurrentInfo, StandardDiscardableSection, 0);
        WriteDataTreeToFileBuilder(*TreeWriter, *Builder);
        EndFileDataTreeWriting(TreeWriter);

        Success = EndFileToWriter(Builder, Writer);
    }

    // Free the encoded input schemes
    DestroyEncodedXInput((variant*)&CurrentInfo.EditorData);

    // Restore the refrences, see above
    {for (size_t AnimSetIdx = 0; AnimSetIdx < AnimationSets.size(); ++AnimSetIdx)
    {
        simulated_animation_set* SimSet = AnimationSets[AnimSetIdx];

        vector<source_file_ref>& Refs = *StoredReferences[AnimSetIdx];
        Assert(Refs.size() == SimSet->SourceFileRefs.size());

        {for (size_t Idx = 0; Idx < SimSet->SourceFileRefs.size(); ++Idx)
        {
            *SimSet->SourceFileRefs[Idx] = Refs[Idx];
        }}

        delete StoredReferences[AnimSetIdx];
        StoredReferences[AnimSetIdx] = 0;
    }}

    // Repoint the animation slots...
    Assert(CurrentAnimationSet >= 0 && CurrentAnimationSet < SizeI(AnimationSets));
    Assert(SizeI(AnimationSlots) == AnimationSets[CurrentAnimationSet]->AnimationSet->AnimationSpecCount);
    Assert(SizeI(AnimationSlots) == SizeI(AnimationSets[CurrentAnimationSet]->AnimationSpecs));
    {for (int32x Idx = 0; Idx < SizeI(AnimationSlots); ++Idx)
    {
        Assert(AnimationSlots[Idx]->Index == Idx);
    }}

    return Success;
}

bool GRANNY
edit::Save(bool QueryAlways)
{
    string SavePath = FileBackingAbsolutePath;
    if (SavePath.empty() || QueryAlways)
    {
        vector<char> HugeBuffer(1 << 16, 0);
        if (!QueryForWritableFile(HugeBuffer.size(), &HugeBuffer[0],
                                  "Save Character As",
                                  "Statement Character Files (*.gsf)",
                                  "gsf"))
        {
            return false;
        }

        SavePath = &HugeBuffer[0];
    }


    file_writer* Writer = CreatePlatformFileWriter(SavePath.c_str(), true);
    if (!Writer)
    {
        FadePrintf(gErrorColor, 2.0f, "Failed to open '%s' for writing", SavePath.c_str());
        StTMPrintfERR("Failed to open: %s!", StTMDynString(SavePath.c_str()));
        return false;
    }

    if (!StringsAreEqualLowercaseNoSlash(SavePath.c_str(), FileBackingAbsolutePath.c_str()))
    {
        {for (size_t AnimSetIdx = 0; AnimSetIdx < AnimationSets.size(); ++AnimSetIdx)
        {
            {for (size_t Idx = 0; Idx < AnimationSets[AnimSetIdx]->SourceFileRefs.size(); ++Idx)
            {
                RebaseSourceRef(AnimationSets[AnimSetIdx]->SourceFileRefs[Idx],
                                FileBackingAbsolutePath, SavePath);
            }}
        }}
    }

    bool Success = FlattenStateToWriter(Writer, 16384);  // todo: proper guess, please.
    CloseFileWriter(Writer);
    if (Success)
    {
        FadePrintf(LuaColor(0.2f, 1.f, 0.2f), 1.0f, "Saved to: %s", SavePath.c_str());
        UpdateRecentFileList(SavePath.c_str(), RecentFiles);
        StateModified = false;
    }
    else
    {
        FadePrintf(LuaColor(1.f, 0.2f, 0.2f), 2.0f, "Failed to save to: '%s'", SavePath.c_str());
        RemoveFromRecentFilelist(SavePath.c_str(), RecentFiles);
    }

    FileBackingAbsolutePath = SavePath;

    return Success;
}

bool GRANNY
edit::CheckForSaveModified()
{
    if (StateModified == false)
        return true;

    int Result = MessageBox(0, "Scene modified, save?", "Modified Scene",
                            MB_YESNOCANCEL | MB_APPLMODAL);
    if (Result == IDCANCEL)
        return false;

    if (Result == IDNO)
        return true;

    return Save(false);
}

int32 GRANNY
edit::GetCurrentAnimationSet()
{
    return CurrentAnimationSet;
}

bool GRANNY
edit::SetCurrentAnimationSet(int32 Set)
{
    Assert(Set >= 0 && Set < SizeI(AnimationSets));

    // Make sure the character is updated if necessary...
    if (CurrentAnimationSet != Set)
    {
        CharacterBindGuard bindGuard;
        CurrentAnimationSet = Set;
    }

    return true;
}

bool GRANNY
edit::ResetFromFileInfoPair(file*                  File,
                            gstate_character_info* NewInfo,
                            char const*            FileNameIfDisk)
{
    // UNDONOTE: We don't undo across this boundary, since the undo system itself calls
    // this.  Caller is responsible for undo
    Assert(NewInfo != 0);
    if (NewInfo->StateMachine.Type == 0)
    {
        // Probably an old format file that was improperly converted
        Assert(false);
        return false;
    }

    string StoredFilename = FileBackingAbsolutePath;

    // Ok, this has a reasonable chance of being the real thing, let's clear out the edit
    // state.  Note that we leave the model alone, so we can use it in the new machines...
    ClearAnimations();
    ResetMachines();
    ProtectedSimpleCall(UILuaState(), "Interactions", "Clear");
    ProtectedSimpleCall(UILuaState(), "IDItem", "FlushInactive");
    ProtectedSimpleCall(UILuaState(), "IDItem", "FlushInactive");

    // Mark this as the backing store for this file
    LoadedFileBacking = File;

    if (FileNameIfDisk != NULL)
    {
        FileBackingAbsolutePath = FileNameIfDisk;
    }
    else
    {
        // Leave alone the existing filename, this is an undo.  Note that
        // ClearAnimations/ResetMachines will zero out that string to reset
        // the state, so go back to our saved string
        FileBackingAbsolutePath = StoredFilename;
    }

    // Let's instantiate the root machine, shall we?
    {
        tokenized* NewTokenized =
            token_context::GetGlobalContext()->CreateFromToken(NewInfo->StateMachine.Type,
                                                               NewInfo->StateMachine.Object);
        RootMachine =  GSTATE_DYNCAST(NewTokenized, state_machine);
    }

    // Animation Slots
    Assert(AnimationSlots.empty());
    {
        {for (int ASIdx = 0; ASIdx < NewInfo->AnimationSlotCount; ++ASIdx)
        {
            AnimationSlots.push_back(NewInfo->AnimationSlots[ASIdx]);
            GStateCloneString(AnimationSlots[ASIdx]->Name,
                              AnimationSlots[ASIdx]->Name);
        }}
    }

    // Animation Sets
    Assert(AnimationSets.empty());
    Assert(AnimationSetArray.empty());
    {
        {for (int ASIdx = 0; ASIdx < NewInfo->AnimationSetCount; ++ASIdx)
        {
            animation_set* FileSet = NewInfo->AnimationSets[ASIdx];

            AnimationSets.push_back(new simulated_animation_set);
            simulated_animation_set* SimSet = AnimationSets.back();
            SimSet->AnimationSet = FileSet;

            // Make sure we clone the name so we can delete it unconditinally later...
            GStateCloneString(SimSet->AnimationSet->Name, SimSet->AnimationSet->Name);

            AnimationSetArray.push_back(FileSet);

            {for (int Idx = 0; Idx < FileSet->SourceFileReferenceCount; ++Idx)
            {
                SimSet->SourceFileRefs.push_back(FileSet->SourceFileReferences[Idx]);
            }}

            // Load the gr2s for the source references.  Note that we clone the string so we
            // don't have to special case ClearAnimations(), similarly for the animation nicknames
            // todo: error handling
            {for (size_t Idx = 0; Idx < SimSet->SourceFileRefs.size(); ++Idx)
            {
                string AbsolutePath = GetSourcePath(SimSet->SourceFileRefs[Idx]->SourceFilename);
                file* Source = FileFromCache(AbsolutePath.c_str());
                granny_file_info* Proposed = Source ? (granny_file_info*)GetFileInfo(*Source) : 0;

                source_file_ref* ThisRef = SimSet->SourceFileRefs[Idx];

                // Make sure we own this string
                GStateCloneString(ThisRef->SourceFilename, ThisRef->SourceFilename);

                // Cleared on error
                ThisRef->SourceInfo = Proposed;
                if (ThisRef->SourceInfo)
                {
                    if (ThisRef->ExpectedAnimCount != 0 &&
                        ThisRef->AnimCRC != 0)
                    {
                        if (ThisRef->ExpectedAnimCount != Proposed->AnimationCount ||
                            ThisRef->AnimCRC != ComputeAnimCRC(ThisRef->SourceInfo))
                        {
                            // @@ todo: warning
                            ThisRef->SourceInfo = 0;
                        }
                    }
                    else
                    {
                        // Save the anim count and crc, this is an upgrade...
                        ThisRef->ExpectedAnimCount = Proposed->AnimationCount;
                        ThisRef->AnimCRC = ComputeAnimCRC(Proposed);
                    }
                }

                // Might have been cleared in the error checker above...
                if (ThisRef->SourceInfo)
                {
                    StartMonitoring(ThisRef);
                }
            }}

            // Load the specs
            {for (int Idx = 0; Idx < FileSet->AnimationSpecCount; ++Idx)
            {
                animation_spec& Spec = FileSet->AnimationSpecs[Idx];

                // If the expected name isn't set, set it
                if (Spec.ExpectedName == 0)
                {
                    if (Spec.SourceReference &&
                        Spec.SourceReference->SourceInfo)
                    {
                        granny_file_info* Info = Spec.SourceReference->SourceInfo;
                        GStateAssert(GS_InRange(Spec.AnimationIndex, Info->AnimationCount));

                        GStateCloneString(Spec.ExpectedName, Info->Animations[Spec.AnimationIndex]->Name);
                    }
                }
                else
                {
                    GStateCloneString(Spec.ExpectedName, Spec.ExpectedName);
                }

                SimSet->AnimationSpecs.push_back(Spec);
            }}
        }}
    }

    // Create the bogus info that we'll instantiate our character from
    ZeroCurrentInfo();

    if (RootMachine)
    {
        RootMachine->CheckConnections();
        RootMachine->GetTypeAndToken(&CurrentInfo.StateMachine);
    }

    CurrentInfo.AnimationSlotCount = NewInfo->AnimationSlotCount;
    if (CurrentInfo.AnimationSlotCount != 0)
        CurrentInfo.AnimationSlots = &AnimationSlots[0];

    CurrentInfo.AnimationSetCount = NewInfo->AnimationSetCount;
    if (CurrentInfo.AnimationSetCount != 0)
        CurrentInfo.AnimationSets = &AnimationSetArray[0];

    SetCurrentAnimationSet(0);

    // If we have a name hint,
    if (NewInfo->ModelNameHint)
    {
        if (ModelFilenames == NewInfo->ModelNameHint &&
            ModelIndex == NewInfo->ModelIndexHint)
        {
            // It's fine.
            GStateCloneString(CurrentInfo.ModelNameHint, NewInfo->ModelNameHint);
            CurrentInfo.ModelIndexHint = NewInfo->ModelIndexHint;
        }
        else
        {
            // Reload the model, if we can...
            if (!SetCharacterBase(NewInfo->ModelNameHint, NewInfo->ModelIndexHint))
                ClearModelAndMeshes();
        }
    }
    else
    {
        // Clear the model
        ClearModelAndMeshes();
    }

    if (RootMachine)
    {
        SetRootMachine(RootMachine);

        // Reset the window position
        lua_State* L = UILuaState();

        LuaPoint pt(0, 0);
        RootMachine->GetEditPosition(pt.x, pt.y);
        PushPoint(L, pt);
        lua_setglobal(L, "ContainerEditingOffset");
    }

    // Input schemes
    ReadFromEncodedXInput((variant*)&NewInfo->EditorData);
    

    // We need to zero the clock out so that the first update won't reflect the loading time...
    UIZeroClock();

    MarkModified();
    return true;
}



bool GRANNY
edit::LoadFromFilename(char const* Filename)
{
    file* File = 0;
    gstate_character_info* NewInfo = 0;
    if (!GStateReadCharacterInfo(Filename, (granny_file*&)File, NewInfo))
    {
        // todo: log
        RemoveFromRecentFilelist(Filename, RecentFiles);
        return false;
    }

    bool Success = ResetFromFileInfoPair(File, NewInfo, Filename);
    if (Success)
    {
        FadePrintf(LuaColor(0.2f, 1, 0.2f), 1.0f, "Loaded from: %s", Filename);
        UpdateRecentFileList(Filename, RecentFiles);
        StateModified = false;
    }
    else
    {
        FadePrintf(LuaColor(1, 0.2f, 0.2f), 1.0f, "Failed to load from: %s!", Filename);
        FreeFile(File);
        RemoveFromRecentFilelist(Filename, RecentFiles);
    }

    return Success;
}

bool GRANNY
edit::Load()
{
    // UNDONOTE: caller responsible for clearing the stack.

    vector<char> HugeBuffer(1 << 16, 0);
    if (!QueryForReadableFile(HugeBuffer.size(), &HugeBuffer[0],
                              false,  // no multiselect
                              "Load Character",
                              "Statement Character Files (*.gsf)",
                              "gsf"))
    {
        return false;
    }
    Assert(HugeBuffer[0]);

    return LoadFromFilename(&HugeBuffer[0]);
}

bool GRANNY
edit::SetCharacterBase(char const* Filenames, int Index)
{
    if (!Filenames)
        return false;

    if (CurrentContainer)
    {
        PushUndoPos("SetCharacter");
    }

    // Why do we do this?  Just in case Filename == ModelFilename.c_str()...
    string NewFilenames = Filenames;

    // todo: UNDONOTE: Currently not undoable, does not touch the undo stack, probably wrong...
    vector<file*> NewFiles;
    vector<file_info*> NewFileInfos;
    {
        size_t Pos = 0;
        do
        {
            size_t LastPos = Pos;
            Pos = NewFilenames.find_first_of(';', Pos);

            string SubName;
            if (Pos != string::npos)
            {
                SubName = NewFilenames.substr(LastPos, Pos - LastPos);
                ++Pos;
            }
            else
            {
                SubName = NewFilenames.substr(LastPos);
            }

            file_info* NewFileInfo = 0;
            file*      NewFile = ReadEntireFile(SubName.c_str());
            if (NewFile)
                NewFileInfo = GetFileInfo(*NewFile);

            if (NewFileInfo)
            {
                NewFiles.push_back(NewFile);
                NewFileInfos.push_back(NewFileInfo);
            }
            else
            {
                // todo: log error
                FreeFile(NewFile);
                for (size_t Idx = 0; Idx < NewFiles.size(); ++Idx)
                    FreeFile(NewFiles[Idx]);

                return false;
            }
        } while (Pos != string::npos && NewFilenames[Pos] != '\0');
    }

    // Extract the models from the files
    vector<model*> NewModels;
    {for (size_t Idx = 0; Idx < NewFileInfos.size(); ++Idx)
    {
        NewModels.insert(NewModels.end(),
                         NewFileInfos[Idx]->Models,
                         NewFileInfos[Idx]->Models + NewFileInfos[Idx]->ModelCount);
    }}

    if (NewModels.empty())
    {
        // todo: Log1(ErrorLogMessage, BlendDagLogMessage, "%s: Invalid model file\n", Filename);

        for (size_t Idx = 0; Idx < NewFiles.size(); ++Idx)
            FreeFile(NewFiles[Idx]);
        return false;
    }

    if (Index < 0 || Index >= int(NewModels.size()))
        Index = 0;

    ClearModelAndMeshes();
    ResetCharacterXForm();

    ModelFilenames = NewFilenames;
    ModelIndex     = Index;

    ModelFiles     = NewFiles;
    ModelFileInfos = NewFileInfos;
    Models         = NewModels;
    Model          = Models[ModelIndex];

    // Note the hint...
    GStateReplaceString(CurrentInfo.ModelNameHint, NewFilenames.c_str());
    CurrentInfo.ModelIndexHint = Index;

    // Use the viewer heuristic.  Look for meshes in this file that have BoneBindings
    // that all match this model.
    {
        {for (size_t InfoIdx = 0; InfoIdx < ModelFileInfos.size(); ++InfoIdx)
        {
            file_info* NewInfo = ModelFileInfos[InfoIdx];

            for (int32x MeshIdx = 0; MeshIdx < NewInfo->MeshCount; ++MeshIdx)
            {
                mesh* Mesh = NewInfo->Meshes[MeshIdx];
                if (!Mesh)
                    continue;

                bool AllMatch = Mesh->BoneBindingCount != 0;
                for (int32x Idx = 0; AllMatch && Idx < Mesh->BoneBindingCount; ++Idx)
                {
                    int32x BoneIdx;
                    AllMatch = FindBoneByNameLowercase(Model->Skeleton, Mesh->BoneBindings[Idx].BoneName, BoneIdx);
                }

                if (AllMatch)
                {
                    mesh_binding* Binding = NewMeshBinding(*Mesh,
                                                           *Model->Skeleton,
                                                           *Model->Skeleton);
                    BoundMeshes.push_back(Binding);
                }
            }
        }}
    }

    // This will bind the model to the current root machine, if possible, and re-enter the
    // current container...
    {
        container* ContainerStore = CurrentContainer;
        SetRootMachine(RootMachine);

        if (ContainerStore != 0 && ContainerStore != CurrentContainer)
        {
            container* Walk = ContainerStore;
            while (Walk)
            {
                if (Walk == RootMachine)
                    break;
                Walk = Walk->GetParent();
            }

            Assert(Walk && Walk == RootMachine);
            SetCurrentContainer(ContainerStore);
        }
    }

    UpdateCameraForFile(ModelFileInfos[0]);

    // Attempt to find a bounding volume for the character to set the camera distance
    ZoomToBoundingBox();
    //ClearUndoStack();

    return true;
}

static void
DeleteRootMachine()
{
    // UNDONOTE: Called internally, caller is responsible for setting undo state.
    if (RootMachine == 0)
        return;

    state_machine* OldMachine = RootMachine;
    SetRootMachine(0);
    if (CharacterInstance)
    {
        ReplaceStateMachine(CharacterInstance, 0);
    }

    // Remove the reference and update the character info
    CurrentInfo.StateMachine.Type = 0;
    CurrentInfo.StateMachine.Object = 0;

    // Delete the old nodes
    GStateDelete<state_machine>(OldMachine);

    MarkModified();
}

bool GRANNY
edit::ResetMachines()
{
    // UNDONOTE: Called internally, caller is responsible for setting undo state.

    DeleteRootMachine();
    Assert(CurrentContainer == 0);
    Assert(RootMachine == 0);

    // Unbind, if we have an instance
    if (CharacterInstance)
    {
        ReplaceStateMachine(CharacterInstance, 0);
        GStateFreeCharacterInstance(CharacterInstance);
        CharacterInstance = 0;
    }

    // Clear out our character info struct
    ZeroCurrentInfo();

    // If we have a backing file, we can release it at this point
    if (LoadedFileBacking)
    {
        FreeFile(LoadedFileBacking);
        LoadedFileBacking = 0;
        FileBackingAbsolutePath.clear();
    }

    StateModified = false;

    return true;
}

static bool
AddAnimationSet(char const* Name)
{
    simulated_animation_set* NewSet = new simulated_animation_set;
    NewSet->AnimationSet = GStateAllocStruct(animation_set);
    ZeroStructure(*NewSet->AnimationSet);
    GStateCloneString(NewSet->AnimationSet->Name, Name);

    NewSet->AnimationSpecs.resize(AnimationSlots.size());
    {for (size_t Idx = 0; Idx < AnimationSlots.size(); ++Idx)
    {
        NewSet->AnimationSpecs[Idx].SourceReference = 0;
        NewSet->AnimationSpecs[Idx].AnimationIndex = -1;
        NewSet->AnimationSpecs[Idx].ExpectedName = 0;
    }}
    NewSet->AnimationSet->AnimationSpecCount = SizeI(AnimationSlots);
    if (NewSet->AnimationSet->AnimationSpecCount)
        NewSet->AnimationSet->AnimationSpecs = &NewSet->AnimationSpecs[0];
    else
        NewSet->AnimationSet->AnimationSpecs = 0;

    AnimationSets.push_back(NewSet);
    AnimationSetArray.push_back(NewSet->AnimationSet);
    AllocatedSets.push_back(NewSet->AnimationSet);

    // Fix up the pointers
    CurrentInfo.AnimationSetCount = SizeI(AnimationSets);
    CurrentInfo.AnimationSets     = &AnimationSetArray[0];

    return true;
}

static bool
DefaultAnimationSet()
{
    // Only called when the currentinfo has been wiped...
    Assert(CurrentInfo.AnimationSlotCount == 0);
    Assert(CurrentInfo.AnimationSlots == 0);
    Assert(CurrentInfo.AnimationSetCount == 0);
    Assert(CurrentInfo.AnimationSets == 0);
    Assert(CurrentAnimationSet == -1);

    AddAnimationSet("Base");
    CurrentAnimationSet = 0;

    return true;
}


bool GRANNY
edit::ResetAll(bool Force)
{
    if (!Force)
    {
        if (CheckForSaveModified() == false)
            return false;
    }

    ResetXInput();
    ClearUndoStack();
    ClearModelAndMeshes();
    ClearAnimations();
    ResetMachines();
    FlushFileCache();
    FileBackingAbsolutePath = "";
    
    return true;
}

static void
DefaultRootMachine()
{
    // UNDOSTATE: Called internally, caller responsible
    Assert(RootMachine == 0);

    state_machine* NewMachine = state_machine::DefaultInstance();
    NewMachine->SetName("CharacterRoot");

    MarkModified();
    Assert(CharacterInstance == 0);

    // // We are going to update the root machines vector, so we need to notify the edit
    // // character, if it exists.  At the same time, bind the new machine to the
    // // character...
    // if (CharacterInstance)
    // {
    //     NewMachine->BindToCharacter(CharacterInstance);
    //     ReplaceStateMachineArray(CharacterInstance, 0);
    // }

    // if (CharacterInstance)
    //     ReplaceStateMachineArray(CharacterInstance, &RootMachines[0]);

    SetRootMachine(NewMachine);
}

bool GRANNY
edit::CreateDefaultFile()
{
    ResetAll(true);
    DefaultAnimationSet();
    DefaultRootMachine();
    ClearUndoStack();

    // Force the state to unmodified
    StateModified = false;

    return true;
}

state_machine* GRANNY
edit::GetRootMachine()
{
    return RootMachine;
}



gstate_character_instance*
edit::GetCharacterInstance()
{
    return CharacterInstance;
}


void GRANNY
edit::GetAnimationSlots(vector<animation_slot*>* Slots)
{
    *Slots = AnimationSlots;
}

void GRANNY
edit::GetAnimationSpecs(vector<animation_spec>* Specs)
{
    Assert(CurrentAnimationSet >= 0 && CurrentAnimationSet < SizeI(AnimationSets));
    simulated_animation_set* SimSet = AnimationSets[CurrentAnimationSet];
    Assert(SimSet);

    *Specs = SimSet->AnimationSpecs;
}



static bool
FileExists(const char* fileName)
{
    DWORD fileAttr;
    fileAttr = GetFileAttributes(fileName);
    if (0xFFFFFFFF == fileAttr)
        return false;

    return true;
}

static char const*
GetSlotEditFilename(animation_slot* Slot)
{
    StTMZone(__FUNCTION__);

    Assert(CurrentAnimationSet >= 0 && CurrentAnimationSet < SizeI(AnimationSets));
    simulated_animation_set* SimSet = AnimationSets[CurrentAnimationSet];
    Assert(SimSet);

    animation_spec& Spec = SimSet->AnimationSpecs[Slot->Index];
    source_file_ref* SourceRef = Spec.SourceReference;
    if (SourceRef == 0)
        return 0;

    granny_file_info* Info = SourceRef->SourceInfo;
    if (Info == 0)
        return 0;

    // That's actually really bad.
    if (Spec.AnimationIndex < 0 || Spec.AnimationIndex > Info->AnimationCount)
    {
        InvalidCodePath("Out of bounds animation spec");
        return 0;
    }

    // First, look to see if the name of the animation is the source file...
    char const* AnimationName = Info->Animations[Spec.AnimationIndex]->Name;
    if (FileExists(AnimationName))
    {
        // Assume we can do that...
        return AnimationName;
    }

    // Next, if there is a single animation in the file, and the file_info::FromFileName
    // points at a real file, assume that is correct.
    if (Info->FromFileName && FileExists(Info->FromFileName))
    {
        return Info->FromFileName;
    }

    // Nope.
    return 0;
}

bool GRANNY
edit::CanEditSlot(animation_slot* Slot)
{
    return GetSlotEditFilename(Slot) != 0;
}

void GRANNY
edit::EditSlot(animation_slot* Slot)
{
    char const* Filename = GetSlotEditFilename(Slot);
    if (!Filename)
        return;

    ShellExecute(0, "open", Filename, 0, 0, SW_SHOWDEFAULT);
}


container* GRANNY
edit::GetCurrentContainer()
{
    return CurrentContainer;
}


bool GRANNY
edit::SetCurrentContainer(container* NewContainer)
{
    // UNDONOTE: We don't undo across this boundary, since the undo system itself calls
    // this.  Caller is responsible for undo

    Assert(NewContainer);
    if (NewContainer == CurrentContainer)
        return true;

    // First, we need to see if this container is in the current root machine...
    container* Root = NewContainer;
    while (Root->GetParent())
        Root = Root->GetParent();

    state_machine* NewRootMachine = GSTATE_DYNCAST(Root, state_machine);
    Assert(NewRootMachine == RootMachine);

    CurrentContainer = NewContainer;

    // moving containers must clear the selection
    ClearSelection();

    // And reset the window position
    {
        lua_State* L = UILuaState();

        LuaPoint pt(0, 0);
        NewContainer->GetEditPosition(pt.x, pt.y);
        PushPoint(L, pt);
        lua_setglobal(L, "ContainerEditingOffset");
    }

    return true;
}


bool GRANNY
edit::SetSelection(tokenized* Selected)
{
    // UNDONOTE: called by undo, caller is responsible
    if (Selected != 0)
    {
        CurrentSelection.resize(1);
        CurrentSelection[0] = Selected;
        return true;
    }
    else
    {
        ClearSelection();
        return false;
    }
}

bool GRANNY
edit::AddToSelection(tokenized* Selected)
{
    // UNDONOTE: called by undo, caller is responsible

    if (!Selected)
        return false;

    if (IsSelected(Selected) == false)
    {
        CurrentSelection.push_back(Selected);
    }

    return true;
}

bool GRANNY
edit::RemoveFromSelection(tokenized* Selected)
{
    // UNDONOTE: called by undo, caller is responsible

    if (!Selected)
        return false;

    vector<tokenized*>::iterator Entry = find(CurrentSelection.begin(), CurrentSelection.end(), Selected);
    if (Entry != CurrentSelection.end())
        CurrentSelection.erase(Entry);

    return true;
}

void GRANNY
edit::ClearSelection()
{
    // UNDONOTE: called by undo, caller is responsible

    CurrentSelection.clear();
}

bool GRANNY
edit::IsSelected(tokenized* Item)
{
    // linear!  This will usually be a list of 1 item, so: fine.
    return (find(CurrentSelection.begin(), CurrentSelection.end(), Item)
            != CurrentSelection.end());
}

bool GRANNY
edit::GetSelection(vector<tokenized*>& SelectionCopy)
{
    SelectionCopy = CurrentSelection;
    return true;
}


int32x GRANNY
edit::TokenizedToID(tokenized* Item)
{
    return (int32x)Item->GetUID();
}


tokenized* GRANNY
edit::IDToTokenized(int32x ID)
{
    token_context* GlobalContext = token_context::GetGlobalContext();
    return GlobalContext->GetProductForUID((uint32)ID);
}

node* GRANNY
edit::IDToNode(int32x ID)
{
    return GSTATE_DYNCAST(IDToTokenized(ID), node);
}

transition* GRANNY
edit::IDToTransition(int32x ID)
{
    return GSTATE_DYNCAST(IDToTokenized(ID), transition);
}

void GRANNY
edit::GetNodeIcons(node* Node, int32x& Large, int32x& Small)
{
    Assert(Node);

    char const* NodeTypename   = Node->GetTypename();

    char Buffer[512];
    sprintf(Buffer, "icon_%s", NodeTypename);
    Large = GetPreferenceBitmap(Buffer);

    sprintf(Buffer, "icon_%s_small", NodeTypename);
    Small = GetPreferenceBitmap(Buffer);
}

static int
Edit_GetCurrentContainerID(lua_State* L)
{
    if (CurrentContainer)
        lua_pushinteger(L, TokenizedToID(CurrentContainer));
    else
        lua_pushnil(L);

    return 1;
}


static int
Edit_CurrentContainerIsBlendgraph(lua_State* L)
{
    if (CurrentContainer)
        lua_pushboolean(L, GSTATE_DYNCAST(CurrentContainer, blend_graph) != 0);
    else
        lua_pushnil(L);

    return 1;
}

static int
Edit_ParentIsBlendgraphOrNil(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -1);
    node* Node = IDToNode(NodeID);

    if (Node->GetParent() == 0 || GSTATE_DYNCAST(Node->GetParent(), blend_graph))
        return LuaBoolean(L, true);

    return LuaBoolean(L, false);
}


static int
Edit_SetModelFromFilename(lua_State* L)
{
    char const* Filename = lua_tostring(L, -1);
    if (!Filename)
        return 0;

    if (SetCharacterBase(Filename, 0))
    {
        UpdateRecentFileList(Filename, RecentModels);
    }
    else
    {
        RemoveFromRecentFilelist(Filename, RecentModels);
    }

    return 0;
}


static int
Edit_PromptForModel(lua_State* L)
{
    vector<char> HugeBuffer(1 << 16, 0);
    if (!QueryForReadableFile(HugeBuffer.size(), &HugeBuffer[0],
                              true,
                              "Set Render Model",
                              "Granny Files (*.gr2)",
                              "gr2"))
    {
        return 0;
    }


    for (size_t Idx = 0; Idx < HugeBuffer.size(); ++Idx)
    {
        if (HugeBuffer[Idx] != '\0')
            continue;

        HugeBuffer[Idx] = ';';
        if (HugeBuffer[Idx+1] == '\0')
            break;
    }

    if (SetCharacterBase(&HugeBuffer[0], 0))
    {
        UpdateRecentFileList(&HugeBuffer[0], RecentModels);
    }
    else
    {
        RemoveFromRecentFilelist(&HugeBuffer[0], RecentModels);
    }

    return 0;
}

static int
Edit_GetModelIdx(lua_State* L)
{
    lua_pushinteger(L, ModelIndex+1);
    return 1;
}

static int
Edit_GetModelStrings(lua_State* L)
{
    vector<char const*> ModelNames;
    if (!Models.empty())
    {
        {for (size_t Idx = 0; Idx < Models.size(); ++Idx)
        {
            ModelNames.push_back(Models[Idx]->Name);
        }}
    }

    PushStringArray(L, ModelNames);
    return 1;
}

static int
Edit_SetModelIdx(lua_State* L)
{
    int NewIndex = lua_tointeger(L, -1) - 1;

    if (!ModelFilenames.empty())
    {
        SetCharacterBase(ModelFilenames.c_str(), NewIndex);
    }

    return 0;
}



static int
Edit_Reset(lua_State* L)
{
    bool Success = false;
    if (ResetAll(false))
    {
        Success = CreateDefaultFile();
    }

    return LuaBoolean(L, Success);
}


static int
Edit_SaveToFile(lua_State* L)
{
    lua_pushboolean(L, Save(!!lua_toboolean(L, -1)));

    return 1;
}


static int
Edit_PromptForLoad(lua_State* L)
{
    // todo: save check on modified
    if (CheckForSaveModified() == false)
    {
        lua_pushboolean(L, false);
        return 1;
    }

    // A file load clears the undo stack completely on success, and pushes the
    // first entry onto that stack.

    bool const Success = Load();
    if (Success)
    {
        ClearUndoStack();
        PushUndoPos("File load", true);
    }

    lua_pushboolean(L, Success);
    return 1;
}

static int
Edit_LoadFromFile(lua_State* L)
{
    // todo: save check on modified
    if (CheckForSaveModified() == false)
    {
        lua_pushboolean(L, false);
        return 1;
    }

    char const* Filename = lua_tostring(L, -1);
    if (!Filename)
    {
        lua_pushboolean(L, false);
        return 1;
    }

    bool const Success = LoadFromFilename(Filename);
    if (Success)
    {
        ClearUndoStack();
        PushUndoPos("File load", true);
    }

    lua_pushboolean(L, Success);
    return 1;
}


static node*
AddNodeImpl(lua_State* L,
            char const* NodeType,
            char const* NodeName,
            LuaPoint const& CreatePt)
{
    if (CurrentContainer == 0)
        return 0;

    // Undo.  Note that we pop this off if the creation fails, but we do this *first*,
    // since we need to snapshot the token_context before the change.
    {
        char buffer[512];
        sprintf(buffer, "Add: %s", NodeType);
        PushUndoPos(buffer);
    }

    // Check that we're in a good state
    RootMachine->CheckConnections();

    // todo: run through factory
    tokenized* NewTokenized = token_context::CreateFromClassName(NodeType);
    node* NewNode = GSTATE_DYNCAST(NewTokenized, node);
    if (!NewNode)
    {
        // todo: error
        GStateDelete<tokenized>(NewTokenized);

        PopUndoPos();
        return 0;
    }

    // Need to account for the area scrolling
    {
        lua_getglobal(L, "ContainerEditingOffset");

        LuaPoint pt(0, 0);
        ExtractPoint(L, -1, pt);
        lua_pop(L, 1);

        NewNode->SetPosition(CreatePt.x + pt.x, CreatePt.y + pt.y);
    }

    // We need to bind this node to the character instance...
    if (CharacterInstance)
    {
        NewNode->BindToCharacter(CharacterInstance);
    }

    if (NodeName)
    {
        NewNode->SetName(NodeName);
        CurrentContainer->AddChild(NewNode);
        CurrentContainer->MoveNodeToFront(NewNode);
        SetSelection(NewNode);
    }

    // Check that we're in a good state
    RootMachine->CheckConnections();
    return NewNode;
}


static int
Edit_AddNode(lua_State* L)
{
    if (CurrentContainer == 0)
        return 0;

    char const* NodeType = lua_tostring(L, -2);
    char const* NodeName = lua_tostring(L, -1);

    // todo: holy batballs, shitman!
    LuaPoint CreatePoint;
    {
        static int CreateOffset = 0;
        CreatePoint = LuaPoint(100 + CreateOffset,
                               100 + CreateOffset);
        CreateOffset = (CreateOffset + 10)%100;
    }

    return LuaNode(L, AddNodeImpl(L, NodeType, NodeName, CreatePoint));
}

static int
Edit_AddNodeAtPt(lua_State* L)
{
    if (CurrentContainer == 0)
        return 0;

    char const* NodeType = lua_tostring(L, -3);
    char const* NodeName = lua_tostring(L, -2);

    LuaPoint CreatePoint;
    if (!ExtractPoint(L, -1, CreatePoint))
    {
        return 0;
    }

    return LuaNode(L, AddNodeImpl(L, NodeType, NodeName, CreatePoint));
}

static void
DeleteNodeImpl(node* Node)
{
    RootMachine->CheckConnections();

    // We need to bind this node to the character instance...
    if (CharacterInstance)
    {
        Node->UnbindFromCharacter();
    }

    container* Parent = Node->GetParent();
    Assert(Parent);

    // Just in case the node is selected, we need to remove it, and any of its transitions (to & from) from the selection
    {for (int Idx = 0; Idx < Parent->GetNumChildren(); ++Idx)
    {
        node* ParentsChild = Parent->GetChild(Idx);

        {for (int TrIdx = 0; TrIdx < ParentsChild->GetNumTransitions(); ++TrIdx)
        {
            transition* Transition = ParentsChild->GetTransition(TrIdx);
            if (Transition->GetStartNode() == Node || Transition->GetEndNode() == Node)
                RemoveFromSelection(Transition);
        }}
    }}

    // Make sure the *node* isn't in the selection...
    RemoveFromSelection(Node);

    // Remove it from the parent, and delete
    Parent->RemoveChild(Node);
    GStateDelete<node>(Node);

    RootMachine->CheckConnections();
}

static int
Edit_ReplaceNodeWith(lua_State* L)
{
    // Check that we're in a good state
    RootMachine->CheckConnections();

    int32x NodeID = (int32x)lua_tointeger(L, -2);
    node* Node = IDToNode(NodeID);

    char const* NodeType = lua_tostring(L, -1);

    state_machine* NodeParent = GSTATE_DYNCAST(Node->GetParent(), state_machine);
    if (NodeParent == 0)
        return LuaNode(L, 0);

    bool const IsStartState = (NodeParent->GetStartStateIdx() == NodeParent->GetChildIdx(Node));

    // Undo.  Note that we pop this off if the creation fails, but we do this *first*,
    // since we need to snapshot the token_context before the change.
    {
        char buffer[512];
        sprintf(buffer, "Replace: %s with \"%s\"", Node->GetName(), NodeType);
        PushUndoPos(buffer);
    }

    LuaPoint CreatePoint(0, 0);
    Node->GetPosition(CreatePoint.x, CreatePoint.y);

    node* NewNode = AddNodeImpl(L, NodeType, Node->GetName(), LuaPoint(0, 0));
    if (NewNode == 0)
    {
        PopUndoPos();
        return LuaNode(L, 0);
    }
    NewNode->SetPosition(CreatePoint.x, CreatePoint.y);

    // We want to take possession of all of the outbound and inbound transitions...
    Node->TransferTransitionsTo(NewNode);
    Node->RetargetTransitionsTo(NewNode);

    // And delete the node...
    DeleteNodeImpl(Node);

    if (IsStartState)
        NodeParent->SetStartState(NewNode);

    RootMachine->CheckConnections();
    return LuaNode(L, NewNode);
}

static int
Edit_GetNodeType(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -1);
    node* Node = IDToNode(NodeID);

    if (Node == 0)
        return LuaNil(L);

    granny_variant TypeAndToken;
    if (!Node->GetTypeAndToken(&TypeAndToken))
        return LuaNil(L);

    // The string starts with ContextUID_<typename>
    if (strncmp(TypeAndToken.Type[0].Name, "ContextUID_", 11) != 0)
        return LuaNil(L);

    return LuaString(L, TypeAndToken.Type[0].Name + 11);
}

static int
Edit_DeleteNode(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -1);
    node* Node = IDToNode(NodeID);

    // Undo
    {
        char buffer[512];
        sprintf(buffer, "Delete: %s", Node->GetName());
        PushUndoPos(buffer);
    }

    // Do the delete...
    DeleteNodeImpl(Node);

    return 0;
}

static int
Edit_DeleteSelection(lua_State* L)
{
    // Undo.  Note that we pop this off if the creation fails, but we do this *first*,
    // since we need to snapshot the token_context before the change.
    PushUndoPos("Delete Selection");

    // Check that we're in a good state
    RootMachine->CheckConnections();

    std::vector<tokenized*> Selection;
    GetSelection(Selection);
    edit::ClearSelection();

    // Two pass.  First delete all the transitions, then all the nodes...
    {for (size_t i = 0; i < Selection.size(); ++i)
    {
        transition* AsTransition = GSTATE_DYNCAST(Selection[i], transition);
        if (AsTransition)
        {
            // Start node owns the transition
            node* StartNode = AsTransition->GetStartNode();
            StartNode->DeleteTransition(AsTransition);
            Selection[i] = 0;
        }
    }}

    {for (size_t i = 0; i < Selection.size(); ++i)
    {
        tokenized* Token = Selection[i];
        if (Token == 0)
            continue;

        transition* AsTransition = GSTATE_DYNCAST(Token, transition);
        Assert(AsTransition == 0);

        node* AsNode = GSTATE_DYNCAST(Token, node);
        Assert(AsNode);

        if (AsNode)
        {
            XInput_NoteNodeDelete(AsNode);

            // We need to bind this node to the character instance...
            if (CharacterInstance)
            {
                AsNode->UnbindFromCharacter();
            }

            container* Parent = AsNode->GetParent();
            if (!Parent)
                continue;

            // Just in case the node is selected
            Parent->RemoveChild(AsNode);
            GStateDelete<node>(AsNode);
        }
    }}

    // Check that we're in a good state
    RootMachine->CheckConnections();

    return 0;
}

static int
Edit_ClearSelection(lua_State* L)
{
    // UNDONOTE: Special case.  Selection changes often follow one another.  Let the
    // script handle this.
    ClearSelection();

    return 0;
}

static int
Edit_SelectByID(lua_State* L)
{
    // UNDONOTE: Special case.  Selection changes often follow one another.  Let the
    // script handle this.
    int32x TokenizedID = (int32x)lua_tointeger(L, -1);
    tokenized* Tokenized = IDToTokenized(TokenizedID);

    AddToSelection(Tokenized);

    return 0;
}

static int
Edit_MoveToFrontByID(lua_State* L)
{
    // UNDONOTE: Special case.  Selection changes often follow one another.  Let the
    // script handle this.
    int32x NodeID = (int32x)lua_tointeger(L, -1);
    node* Node = IDToNode(NodeID);

    container* Parent = Node->GetParent();
    Parent->MoveNodeToFront(Node);

    // Check that we're in a good state
    RootMachine->CheckConnections();

    return 0;
}

static int
Edit_ToggleSelectByID(lua_State* L)
{
    // UNDONOTE: Special case.  Selection changes often follow one another.  Let the
    // script handle this.
    int32x TokenizedID = (int32x)lua_tointeger(L, -1);
    tokenized* Tokenized = IDToTokenized(TokenizedID);

    if (IsSelected(Tokenized))
        RemoveFromSelection(Tokenized);
    else
        AddToSelection(Tokenized);

    return 0;
}

static int
Edit_IsSelectedByID(lua_State* L)
{
    int32x TokenizedID = (int32x)lua_tointeger(L, -1);
    tokenized* Tokenized = IDToTokenized(TokenizedID);

    lua_pushboolean(L, IsSelected(Tokenized));
    return 1;
}

static int
Edit_IsSolelySelectedByID(lua_State* L)
{
    int32x TokenizedID = (int32x)lua_tointeger(L, -1);
    tokenized* Tokenized = IDToTokenized(TokenizedID);

    lua_pushboolean(L, CurrentSelection.size() == 1 && IsSelected(Tokenized));
    return 1;
}

static int
Edit_IsTransition(lua_State* L)
{
    int32x TokenizedID = (int32x)lua_tointeger(L, -1);

    lua_pushboolean(L, edit::IDToTransition(TokenizedID) != 0);
    return 1;
}

static int
Edit_NodeNamesForTransition(lua_State* L)
{
    int32x TokenizedID = (int32x)lua_tointeger(L, -1);
    transition* Transition = edit::IDToTransition(TokenizedID);
    if (!Transition)
        return 0;

    Assert(Transition->GetStartNode());
    Assert(Transition->GetEndNode());

    lua_pushstring(L, Transition->GetStartNode()->GetName());
    lua_pushstring(L, Transition->GetEndNode()->GetName());
    return 2;
}

static int
Edit_CanDescendNode(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -1);
    node* Node = IDToNode(NodeID);

    container* Container = GSTATE_DYNCAST(Node, container);
    lua_pushboolean(L, Container != 0);
    return 1;
}

static int
Edit_DescendNode(lua_State* L)
{
    // Store off the current editing position to the current container
    {
        lua_getglobal(L, "ContainerEditingOffset");

        LuaPoint pt(0, 0);
        ExtractPoint(L, -1, pt);
        lua_pop(L, 1);

        if (CurrentContainer)
            CurrentContainer->SetEditPosition(pt.x, pt.y);
    }

    int32x NodeID = (int32x)lua_tointeger(L, -1);
    node* Node = IDToNode(NodeID);

    container* Container = GSTATE_DYNCAST(Node, container);
    if (!Container)
    {
        return LuaBoolean(L, false);
    }

    PushUndoPos("Change level", true);
    return LuaBoolean(L, SetCurrentContainer(Container));
}


static int
Edit_GetContainerStack(lua_State* L)
{
    vector<const char*> names;

    // push_front is slow, but there should only ever be like 5 of these...
    container* Container = CurrentContainer;
    while (Container)
    {
        names.insert(names.begin(), Container->GetName());
        Container = Container->GetParent();
    }

    PushStringArray(L, names);
    return 1;
}

static int
Edit_PopContainerLevels(lua_State* L)
{
    int32x Levels = (int32x)lua_tointeger(L, -1);
    lua_pop(L, 1);

    PushUndoPos("Change level", true);

    // Store off the current editing position to the current container
    {
        lua_getglobal(L, "ContainerEditingOffset");

        LuaPoint pt(0, 0);
        ExtractPoint(L, -1, pt);
        lua_pop(L, 1);

        if (CurrentContainer)
            CurrentContainer->SetEditPosition(pt.x, pt.y);
    }

    while (Levels && CurrentContainer->GetParent() != 0)
    {
        SetCurrentContainer(CurrentContainer->GetParent());
        --Levels;
    }
    Assert(Levels == 0);

    return 0;
}


static int
Edit_AddTransition(lua_State* L)
{
    if (CurrentContainer == 0)
        return 0;

    PushUndoPos("Add transition");

    // Current container must also be a statemachine...
    Assert(GSTATE_DYNCAST(CurrentContainer, state_machine) != 0);

    // todo: robustimicate
    int32x FromNodeID = (int32x)lua_tointeger(L, -4);
    node* FromNode = IDToNode(FromNodeID);

    LuaPoint StartPt(0, 0);
    ExtractPoint(L, -3, StartPt);

    int32x ToNodeID = (int32x)lua_tointeger(L, -2);
    node* ToNode = IDToNode(ToNodeID);

    LuaPoint EndPt(0, 0);
    ExtractPoint(L, -1, EndPt);

    // By default, all transitions start as "OnRequest"
    transition* NewTransition = tr_onrequest::DefaultInstance();
    if (NewTransition)
    {
        NewTransition->SetNodes(FromNode, StartPt.x, StartPt.y, ToNode, EndPt.x, EndPt.y);
        FromNode->AddTransition(NewTransition);
    }

    return 0;
}

extern float g_GlobalClock;

real32 GRANNY
edit::GetCharacterOrGlobalTime()
{
    real32 UseClock = g_GlobalClock;
    if (CharacterInstance)
        UseClock = CharacterInstance->CurrentTime;
    return UseClock;
}



static int
Edit_SetActiveState(lua_State* L)
{
    // UNDONOTE: handled by caller

    if (CurrentContainer == 0)
        return 0;

    state_machine* Machine = GSTATE_DYNCAST(CurrentContainer, state_machine);
    Assert(Machine != 0);

    // todo: robustimicate
    int32x DestNodeID = (int32x)lua_tointeger(L, -1);
    node* DestNode = IDToNode(DestNodeID);
    Assert(DestNode->GetParent() == Machine);

    tokenized* Active  = Machine->GetActiveElement();
    node* ActiveAsNode = GSTATE_DYNCAST(Active, node);
    if (ActiveAsNode)
    {
        // Look for a transition we can use...
        {for (int32x Idx = 0; Idx < ActiveAsNode->GetNumTransitions(); ++Idx)
        {
            transition* Transition = ActiveAsNode->GetTransition(Idx);
            if (Transition->GetEndNode() == DestNode)
            {
                Machine->StartTransition(GetCharacterOrGlobalTime(), Transition);
                return 0;
            }
        }}

        // Fall through to force...
    }
    else
    {
        // No active element, or it's in transition, just fall through and force
    }


    if (!Machine->ForceState(GetCharacterOrGlobalTime(), DestNode))
    {
        // todo: warn?
        // todo: remove undo pos?
    }

    return 0;
}

static int
Edit_SetDefaultState(lua_State* L)
{
    // UNDONOTE: handled by caller

    if (CurrentContainer == 0)
        return 0;

    MarkModified();

    state_machine* Machine = GSTATE_DYNCAST(CurrentContainer, state_machine);
    Assert(Machine != 0);

    // todo: robustimicate
    int32x DestNodeID = (int32x)lua_tointeger(L, -1);
    node* DestNode = IDToNode(DestNodeID);
    Assert(DestNode->GetParent() == Machine);

    Machine->SetStartState(DestNode);
    return 0;
}

static int
Edit_TriggerTransition(lua_State* L)
{
    if (CurrentContainer == 0)
        return 0;

    MarkModified();

    state_machine* Machine = GSTATE_DYNCAST(CurrentContainer, state_machine);
    Assert(Machine != 0);

    // todo: robustimicate
    int32x DestID = (int32x)lua_tointeger(L, -1);
    transition* Transition = IDToTransition(DestID);
    if (!Transition)
        return 0;

    if (Machine->StartTransition(GetCharacterOrGlobalTime(), Transition) == false)
        Machine->ForceStartTransition(GetCharacterOrGlobalTime(), Transition);

    return 0;
}


static int
Edit_ConnectNodes(lua_State* L)
{
    PushUndoPos("Connect nodes");
    RootMachine->CheckConnections();

    // todo: robustimicate
    int32x FromNodeID = (int32x)lua_tointeger(L, -4);
    node*  FromNode = IDToNode(FromNodeID);

    int32x FromEdge = (int32x)lua_tointeger(L, -3);

    int32x ToNodeID = (int32x)lua_tointeger(L, -2);
    node* ToNode = IDToNode(ToNodeID);

    int32x ToEdge = (int32x)lua_tointeger(L, -1);

    // Make sure we're cool, and store off the previous connection in case there are loops
    // caused by the new connection
    Assert(!RootMachine->DetectLoops());
    node*  OldNode = 0;
    int32x OldOutput = -1;
    ToNode->GetInputConnection(ToEdge, &OldNode, &OldOutput);
    ToNode->SetInputConnection(ToEdge, FromNode, FromEdge);

    if (RootMachine->DetectLoops())
    {
        ToNode->SetInputConnection(ToEdge, OldNode, OldOutput);
        Assert(!RootMachine->DetectLoops());
    }

    RootMachine->CheckConnections();
    return 0;
}

static int
Edit_DisconnectEdge(lua_State* L)
{
    PushUndoPos("Disconnect nodes");
    RootMachine->CheckConnections();

    // todo: robustimicate
    int32x ToNodeID = (int32x)lua_tointeger(L, -2);
    node*  ToNode = IDToNode(ToNodeID);

    int32x ToEdge = (int32x)lua_tointeger(L, -1);

    // Just nuke it, no checks needed
    ToNode->SetInputConnection(ToEdge, 0, 0);

    RootMachine->CheckConnections();
    return 0;
}

static int
Edit_GetSelection(lua_State* L)
{
    if (CurrentSelection.empty())
    {
        lua_pushnil(L);
        return 1;
    }

    vector<int32x> SelectionIDs;
    {for (size_t Idx = 0; Idx < CurrentSelection.size(); ++Idx)
    {
        SelectionIDs.push_back(edit::TokenizedToID(CurrentSelection[Idx]));
    }}
    PushIntArray(L, SelectionIDs);

    return 1;
}

static int
Edit_IconsForNode(lua_State* L)
{
    int NodeID = lua_tointeger(L, -1);
    node* Node = edit::IDToNode(NodeID);

    int32x Large = -1;
    int32x Small = -1;
    GetNodeIcons(Node, Large, Small);

    lua_pushinteger(L, Large);
    lua_pushinteger(L, Small);

    return 2;
}


static int
Edit_NameForNode(lua_State* L)
{
    int NodeID = lua_tointeger(L, -1);
    node* Node = edit::IDToNode(NodeID);

    lua_pushstring(L, Node->GetName());
    return 1;
}


static int
Edit_SetNameForNode(lua_State* L)
{
    PushUndoPos("Change node name");

    int NodeID = lua_tointeger(L, -2);
    node* Node = edit::IDToNode(NodeID);

    char const* NewName = lua_tostring(L, -1);

    Node->SetName(NewName);
    Node->MarkNameUserSpecified();  // protect the name

    return 0;
}

static int
Edit_NameForTokenized(lua_State* L)
{
    int TokenizedID = lua_tointeger(L, -1);
    tokenized* TokObj = edit::IDToTokenized(TokenizedID);

    lua_pushstring(L, TokObj->GetName());
    return 1;
}

static int
Edit_SetNameForTokenized(lua_State* L)
{
    PushUndoPos("Change node name");

    char const* NewName = lua_tostring(L, -1);
    int TokenizedID = lua_tointeger(L, -2);

    tokenized* TokObj = edit::IDToTokenized(TokenizedID);

    TokObj->SetName(NewName);
    return 0;
}

static int
Edit_PushUndoPos(lua_State* L)
{
    char const* PositionName = lua_tostring(L, -2);
    int PositionType = lua_tointeger(L, -1);

    PushUndoPos(PositionName, false, EUndoClass(PositionType));

    return 0;
}

static int
Edit_PopUndo(lua_State* L)
{
    PopUndoPos();

    return 0;
}

static int
Edit_PushCollapseMarker(lua_State* L)
{
    return LuaInteger(L, PushCollapseMarker(lua_tostring(L, -1)));
}

static int
Edit_CollapseToMarker(lua_State* L)
{
    return LuaBoolean(L, CollapseToMarker(lua_tointeger(L, -1)));
}


static int
Edit_UndoStackSize(lua_State* L)
{
    lua_pushinteger(L, UndoStackSize());
    return 1;
}

static int
Edit_UndoStackCount(lua_State* L)
{
    lua_pushinteger(L, UndoStackCount());
    return 1;
}

static int
Edit_ScrubListSize(lua_State* L)
{
    lua_pushinteger(L, ScrubListSize());
    return 1;
}

static int
Edit_ScrubRange(lua_State* L)
{
    real32 ScrubMin, ScrubMax;
    GetScrubWindow(ScrubMin, ScrubMax);

    lua_pushnumber(L, ScrubMin);
    lua_pushnumber(L, ScrubMax);

    return 2;
}

static int
Edit_InScrubMode(lua_State* L)
{
    extern bool g_InScrubMode;
    lua_pushboolean(L, g_InScrubMode);
    return 1;
}

static int
Edit_SetScrubMode(lua_State* L)
{
    extern bool g_InScrubMode;

    bool OldVal = g_InScrubMode;
    g_InScrubMode = !!lua_toboolean(L, -1);

    if (OldVal && !g_InScrubMode)
    {
        // We just exited, reset the position to the end of the scrub window...

        real32 ScMin, ScMax;
        if (GetScrubWindow(ScMin, ScMax))
        {
            extern real32 g_GlobalClock;
            extern real32 g_GlobalClockAdjust;
            extern system_clock g_StartClock;

            ScrubToTime(ScMax);
            g_GlobalClock = ScMax;

            // We also need to set the adjustment so the calculation with StartClock
            // yields this value
            system_clock CurrClock;
            GetSystemSeconds(&CurrClock);

            double Elapsed = GetSecondsElapsed(g_StartClock, CurrClock);

            g_GlobalClockAdjust = real32(g_GlobalClock - Elapsed);
        }
    }

    if (g_InScrubMode)
    {
        extern real32 g_GlobalDelta;
        g_GlobalDelta = 0;
    }

    return 0;
}

static int
Edit_ScrubTo(lua_State* L)
{
    extern bool g_InScrubMode;
    Assert(g_InScrubMode);

    real32 ScTo = real32(lua_tonumber(L, -1));

    real32 ScMin, ScMax;
    if (!GetScrubWindow(ScMin, ScMax) ||
        (ScTo < ScMin || ScTo > ScMax))
    {
        lua_pushboolean(L, false);
        return 1;
    }

    bool ScrubSuccess = ScrubToTime(ScTo);
    if (ScrubSuccess)
    {
        extern real32 g_GlobalClock;
        extern real32 g_GlobalDelta;
        g_GlobalClock = ScTo;
        g_GlobalDelta = 0;
    }

    lua_pushboolean(L, ScrubSuccess);
    return 1;
}

static int
Edit_GetScrubTimeMotion(lua_State* L)
{
    extern float g_ScrubMultiplier;
    lua_pushnumber(L, g_ScrubMultiplier);
    return 1;
}

static int
Edit_SetScrubTimeMotion(lua_State* L)
{
    extern bool g_InScrubMode;
    Assert(g_InScrubMode);

    extern float g_GlobalClock;
    extern float g_ScrubStartPoint;
    g_ScrubStartPoint = g_GlobalClock;

    extern float g_ScrubMultiplier;
    g_ScrubMultiplier = real32(lua_tonumber(L, -1));

    extern system_clock g_ScrubStartClock;
    GetSystemSeconds(&g_ScrubStartClock);

    return 0;
}


static int
Edit_GetAbsoluteTime(lua_State* L)
{
    system_clock CurrClock;
    GetSystemSeconds(&CurrClock);

    extern system_clock g_StartClock;
    double Elapsed = GetSecondsElapsed(g_StartClock, CurrClock);
    lua_pushnumber(L, Elapsed);
    return 1;
}

static int
Edit_IsDClick(lua_State* L)
{
    int MS = int(lua_tonumber(L, -1) * 1000.0);

    lua_pushboolean(L, MS <= (int)GetDoubleClickTime());
    return 1;
}

static int
Edit_GetGlobalClock(lua_State* L)
{
    extern real32 g_GlobalClock;
    lua_pushnumber(L, g_GlobalClock);
    return 1;
}

static int
Edit_SetPause(lua_State* L)
{
    extern bool g_GlobalPause;
    g_GlobalPause = !!lua_toboolean(L, -1);
    return 0;
}

static int
Edit_GetPause(lua_State* L)
{
    extern bool g_GlobalPause;
    lua_pushboolean(L, g_GlobalPause);
    return 1;
}


static void
PushRecentFileList(lua_State* L, vector<SpecPathPair>& Paths)
{
    lua_createtable(L, (int)Paths.size(), 0);

    {for(uint32x Idx = 0; Idx < Paths.size(); ++Idx)
    {
        lua_pushinteger(L, Idx + 1);

        lua_createtable(L, 2, 0);
        lua_pushinteger(L, 1);
        lua_pushstring(L, Paths[Idx].first.c_str());
        lua_settable(L, -3);
        lua_pushinteger(L, 2);
        lua_pushstring(L, Paths[Idx].second.c_str());
        lua_settable(L, -3);

        lua_settable(L, -3);
    }}
}

static int
Edit_GetRecentFiles(lua_State* L)
{
    PushRecentFileList(L, RecentFiles);
    return 1;
}

static int
Edit_GetRecentModels(lua_State* L)
{
    PushRecentFileList(L, RecentModels);
    return 1;
}


static int
Edit_GetAnimationSets(lua_State* L)
{
    Assert(AnimationSets.empty() == false);
    Assert(CurrentAnimationSet >= 0 && CurrentAnimationSet < int(AnimationSets.size()));

    vector<char const*> SetNames(AnimationSets.size(), 0);
    {for (size_t Idx = 0; Idx < AnimationSets.size(); ++Idx)
    {
        SetNames[Idx] = AnimationSets[Idx]->AnimationSet->Name;
    }}

    PushStringArray(L, SetNames);
    lua_pushinteger(L, CurrentAnimationSet + 1); // lua indexing

    return 2;
}

static int
Edit_SetCurrentAnimationSet(lua_State* L)
{
    Assert(AnimationSets.empty() == false);
    Assert(CurrentAnimationSet >= 0 && CurrentAnimationSet < SizeI(AnimationSets));

    int Set = lua_tointeger(L, -1) - 1; // lua indexing
    Assert(Set >= 0 && Set < SizeI(AnimationSets));

    PushUndoPos("SetCurrentAnimationSet", true);

    SetCurrentAnimationSet(Set);
    Assert(CurrentAnimationSet == Set);

    return 0;
}

static int
Edit_SetAnimationSetName(lua_State* L)
{
    Assert(AnimationSets.empty() == false);
    Assert(CurrentAnimationSet >= 0 && CurrentAnimationSet < SizeI(AnimationSets));

    int Set = lua_tointeger(L, -2) - 1; // lua indexing
    Assert(Set >= 0 && Set < SizeI(AnimationSets));

    char const* NewName = lua_tostring(L, -1);
    if (NewName == 0)
    {
        return LuaBoolean(L, false);
    }
    else
    {
        PushUndoPos("SetAnimationSetName");

        GStateDeallocate(AnimationSets[Set]->AnimationSet->Name);
        GStateCloneString(AnimationSets[Set]->AnimationSet->Name, NewName);
        return LuaBoolean(L, true);
    }
}

static int
Edit_NewAnimationSet(lua_State* L)
{
    PushUndoPos("NewAnimationSet");

    AddAnimationSet("New Anim Set");
    SetCurrentAnimationSet(SizeI(AnimationSets) - 1);

    return 0;
}

static int
Edit_DeleteCurrentAnimationSet(lua_State* L)
{
    // No, don't do that!
    if (AnimationSets.size() == 1)
        return 0;

    PushUndoPos("Delete AnimationSet");

    // Make sure the character is updated if necessary...
    {
        CharacterBindGuard bindGuard;

        simulated_animation_set* SimSet = AnimationSets[CurrentAnimationSet];

        GStateDeallocate(SimSet->AnimationSet->Name);
        {for (size_t Idx = 0; Idx < SimSet->SourceFileRefs.size(); ++Idx)
        {
            StopMonitoring(SimSet->SourceFileRefs[Idx]);

            GStateDeallocate(SimSet->SourceFileRefs[Idx]->SourceFilename);
        }}
        SimSet->SourceFileRefs.clear();

        delete SimSet;
        // Let the actual set get cleaned up by allocatesets if necessary

        AnimationSets.erase(AnimationSets.begin() + CurrentAnimationSet);
        AnimationSetArray.erase(AnimationSetArray.begin() + CurrentAnimationSet);
        if (CurrentAnimationSet >= SizeI(AnimationSets))
        {
            CurrentAnimationSet = 0;
        }

        // Fix up the pointers
        CurrentInfo.AnimationSetCount = SizeI(AnimationSets);
        CurrentInfo.AnimationSets     = &AnimationSetArray[0];
    }

    return 0;
}

static int
Edit_CopyAnimationSet(lua_State* L)
{
    PushUndoPos("CopyAnimationSet");

    string Name = AnimationSets[CurrentAnimationSet]->AnimationSet->Name;
    Name += " (copy)";
    AddAnimationSet(Name.c_str());

    // File references
    {
        AnimationSets.back()->SourceFileRefs = AnimationSets[CurrentAnimationSet]->SourceFileRefs;
        vector<source_file_ref*>& SourceFileRefs = AnimationSets.back()->SourceFileRefs;

        AnimationSets.back()->AnimationSet->SourceFileReferenceCount = SizeI(SourceFileRefs);
        if (AnimationSets.back()->AnimationSet->SourceFileReferenceCount)
            AnimationSets.back()->AnimationSet->SourceFileReferences = &SourceFileRefs[0];
        else
            AnimationSets.back()->AnimationSet->SourceFileReferences = 0;
    }

    // Copy the specs from the current set
    {for (size_t Idx = 0; Idx < AnimationSlots.size(); ++Idx)
    {
        AnimationSets.back()->AnimationSpecs[Idx] = AnimationSets[CurrentAnimationSet]->AnimationSpecs[Idx];
        GStateCloneString(AnimationSets.back()->AnimationSpecs[Idx].ExpectedName,
                          AnimationSets.back()->AnimationSpecs[Idx].ExpectedName);
    }}

    SetCurrentAnimationSet(SizeI(AnimationSets) - 1);

    return 0;
}

static int
Edit_AddSource(lua_State* L)
{
    bool Changed = false;
    vector<char> HugeBuffer(1 << 16, 0);
    if (QueryForReadableFile(HugeBuffer.size(), &HugeBuffer[0],
                             true,
                             "Add Animation(s)",
                             "Granny Files (*.gr2)",
                             "gr2"))
    {
        PushUndoPos("AddSource");

        char* Current = &HugeBuffer[0];
        while (*Current)
        {
            // undo handled in the function
            source_file_ref* NewRef = AddAnimationFile(Current);
            if (NewRef)
            {
                Changed = true;
            }

            Current += strlen(Current) + 1;
        }
    }

    return LuaBoolean(L, Changed);
}

static int
Edit_AddSourceFromFilename(lua_State* L)
{
    int SourceIndex = -1;

    char const* Filename = lua_tostring(L, -1);
    if (Filename)
    {
        PushUndoPos("AddSource");

        // undo handled in the function
        source_file_ref* NewRef = AddAnimationFile(Filename);
        if (NewRef)
        {
            SourceIndex = GetSourceIndexFromRef(NewRef) + 1;  // sources are 1 based on the lua side, dangit.
        }
    }

    return LuaInteger(L, SourceIndex);
}

static int
Edit_RemoveSource(lua_State* L)
{
    char const* Filename = lua_tostring(L, -1);
    if (!Filename)
        return 0;

    string AbsPath = GetSourcePath(Filename);

    // We are removing this only from the current animation set...
    Assert(CurrentAnimationSet >= 0 && CurrentAnimationSet < SizeI(AnimationSets));
    simulated_animation_set* SimSet = AnimationSets[CurrentAnimationSet];
    Assert(SimSet);

    // Check that we have this already...
    source_file_ref* FoundRef = 0;
    int FoundIdx = -1;
    {for (int Idx = 0; Idx < SizeI(SimSet->SourceFileRefs); ++Idx)
    {
        // todo: compare absolute paths to ensure we don't get fooled by drive letter/relative crap.
        string AbsSourcePath = GetSourcePath(SimSet->SourceFileRefs[Idx]->SourceFilename);
        if (StringsAreEqualLowercaseNoSlash(AbsSourcePath.c_str(), AbsPath.c_str()))
        {
            FoundIdx = Idx;
            FoundRef = SimSet->SourceFileRefs[Idx];
            break;
        }
    }}
    if (FoundIdx == -1)
    {
        return LuaBoolean(L, false);
    }
    Assert(FoundRef != 0);

    PushUndoPos("RemoveSource");

    // So we have to unbind and rebind if we have a character
    {
        CharacterBindGuard bindGuard;

        // Do the work of killing the reference
        {
            // Remove the ref from any open specs that refer to this source...
            {for (size_t Idx = 0; Idx < SimSet->AnimationSpecs.size(); ++Idx)
            {
                if (SimSet->AnimationSpecs[Idx].SourceReference == FoundRef)
                {
                    SimSet->AnimationSpecs[Idx].SourceReference = 0;
                    SimSet->AnimationSpecs[Idx].AnimationIndex = -1;
                    GStateDeallocate(SimSet->AnimationSpecs[Idx].ExpectedName);
                    CurrentInfo.AnimationSets[CurrentAnimationSet]->AnimationSpecs[Idx] = SimSet->AnimationSpecs[Idx];
                }
            }}

            // Remove the file from the source_refs
            StopMonitoring(SimSet->SourceFileRefs[FoundIdx]);

            SimSet->SourceFileRefs.erase(SimSet->SourceFileRefs.begin() + FoundIdx);
            FoundIdx = -1;

            SimSet->AnimationSet->SourceFileReferenceCount = SizeI(SimSet->SourceFileRefs);
            if (SimSet->AnimationSet->SourceFileReferenceCount)
                SimSet->AnimationSet->SourceFileReferences = &SimSet->SourceFileRefs[0];
            else
                SimSet->AnimationSet->SourceFileReferences = 0;

            // These are always deleted...
            GStateDeallocate(FoundRef->SourceFilename);
            FoundRef->SourceFilename = 0;

            // If the ref is in the allocated refs, delete it...
            {for (size_t Idx = 0; Idx < AllocatedFileRefs.size(); ++Idx)
            {
                if (AllocatedFileRefs[Idx] == FoundRef)
                {
                    GStateDeallocate(FoundRef);
                    AllocatedFileRefs.erase(AllocatedFileRefs.begin() + Idx);
                    break;
                }
            }}
            FoundRef = 0;
        }
    }

    return LuaBoolean(L, true);
}

static string
ShortNameForAnim(char const* RawName)
{
    Assert(RawName);

    char const* ForeSlash = strrchr(RawName, '/');
    char const* BackSlash = strrchr(RawName, '\\');

    if (ForeSlash && ForeSlash[1] != '\0' && (!BackSlash || ForeSlash > BackSlash))
    {
        return string(ForeSlash + 1);
    }
    else if (BackSlash && BackSlash[1] != '\0')
    {
        return string(BackSlash + 1);
    }
    else
    {
        return string(RawName);
    }
}


static int
Edit_GetSources(lua_State* L)
{
    // Push array of:
    //  { Path     = FullName,           -- "c:/art/model/anim.gr2"
    //    Relative = RelativePath,       -- "../model/anim.gr2"
    //    Root     = RootName,           -- "anim.gr2"
    //    ValidAnims = {
    //      { Index, Duration, ShortName, FullName },
    //      { Index, Duration, ShortName, FullName },
    //      { Index, Duration, ShortName, FullName },
    //    }
    //  }

    Assert(CurrentAnimationSet >= 0 && CurrentAnimationSet < SizeI(AnimationSets));
    simulated_animation_set* SimSet = AnimationSets[CurrentAnimationSet];
    Assert(SimSet);

    lua_createtable(L, SizeI(SimSet->SourceFileRefs), 0);  // array
    {for (size_t Idx = 0; Idx < SimSet->SourceFileRefs.size(); ++Idx)
    {
        lua_pushinteger(L, Idx + 1);  // idx, array

        lua_createtable(L, 0, 4);     // entry, idx, array
        source_file_ref* Source = SimSet->SourceFileRefs[Idx];

        // Path
        {
            lua_pushstring(L, "Path");
            string AbsPath = GetSourcePath(Source->SourceFilename);
            lua_pushstring(L, AbsPath.c_str());
            lua_settable(L, -3);
        }

        // Relative
        {
            lua_pushstring(L, "Relative");
            lua_pushstring(L, Source->SourceFilename);
            lua_settable(L, -3);
        }

        // Root
        {
            lua_pushstring(L, "Root");
            string Root = GetSourceFileSpec(Source->SourceFilename);
            lua_pushstring(L, Root.c_str());
            lua_settable(L, -3);
        }

        // Correctly loaded
        {
            lua_pushstring(L, "Loaded");
            lua_pushboolean(L, Source->SourceInfo != 0);
            lua_settable(L, -3);
        }

        if (Source->SourceInfo)
        {
            // Valid Anims
            lua_pushstring(L, "ValidAnims");
            lua_createtable(L, Source->SourceInfo->AnimationCount, 0);

            lua_pushstring(L, "AllAnims");
            lua_createtable(L, Source->SourceInfo->AnimationCount, 0);
            
            int CurrValidIdx = 1;
            {for (int Idx = 0; Idx < Source->SourceInfo->AnimationCount; ++Idx)
            {
                lua_pushinteger(L, Idx+1);
                lua_createtable(L, 0, 3);
                {
                    lua_pushstring(L, "FullName");
                    lua_pushstring(L, Source->SourceInfo->Animations[Idx]->Name);
                    lua_settable(L, -3);

                    // Short name is the name up to the last slash, so we can eliminate
                    // the filename from max or maya...
                    lua_pushstring(L, "ShortName");
                    string ShortName = ShortNameForAnim(Source->SourceInfo->Animations[Idx]->Name);
                    lua_pushstring(L, ShortName.c_str());
                    lua_settable(L, -3);

                    lua_pushstring(L, "Index");
                    lua_pushinteger(L, Idx);
                    lua_settable(L, -3);

                    lua_pushstring(L, "Duration");
                    lua_pushnumber(L, Source->SourceInfo->Animations[Idx]->Duration);
                    lua_settable(L, -3);
                }

                if (Source->SourceInfo->Animations[Idx]->TrackGroupCount != 0)
                {
                    lua_pushinteger(L, CurrValidIdx++);
                    lua_pushvalue(L, -2);
                    lua_settable(L, -7);
                }
                
                lua_settable(L, -3);
            }}

            lua_settable(L, -5);
            lua_settable(L, -3);
        }
        else
        {
            // This is a file that we *didn't* validate correctly.
            lua_pushstring(L, "ValidAnims");
            lua_createtable(L, Source->ExpectedAnimCount, 0);

            // Note <= for lua integers
            {for (int Idx = 1; Idx <= Source->ExpectedAnimCount; ++Idx)
            {
                lua_pushinteger(L, Idx);
                lua_createtable(L, 0, 1);

                // Short name is the name up to the last slash, so we can eliminate the
                // filename from max or maya...
                {
                    lua_pushstring(L, "ShortName");
                    lua_pushstring(L, "<invalid_source>");
                    lua_settable(L, -3);
                }

                lua_settable(L, -3);
            }}

            lua_settable(L, -3);
        }

        // Make the entry..
        lua_settable(L, -3);
    }}

    return 1;
}

static int
Edit_ReplaceSource(lua_State* L)
{
    // We are adding this only to the current animation set...
    Assert(CurrentAnimationSet >= 0 && CurrentAnimationSet < SizeI(AnimationSets));
    simulated_animation_set* SimSet = AnimationSets[CurrentAnimationSet];
    Assert(SimSet);

    int SourceIndex = lua_tointeger(L, -1);
    if (SourceIndex < 0 || SourceIndex >= SizeI(SimSet->SourceFileRefs))
    {
        Log0(ErrorLogMessage, ApplicationLogMessage,
             "Out of range source index in Edit_ReplaceSource\n");
        return LuaBoolean(L, false);
    }

    vector<char> HugeBuffer(1 << 16, 0);
    if (!QueryForReadableFile(HugeBuffer.size(), &HugeBuffer[0],
                              false,
                              "Replace animation source",
                              "Granny Files (*.gr2)",
                              "gr2"))
    {
        return LuaBoolean(L, false);
    }

    PushUndoPos("ReplaceSource");

    CharacterBindGuard cbg;

    char const* NewFilename = &HugeBuffer[0];

    // @@ todo: factor out
    // This is duplicated a bit because we want to ensure that we don't replicate a source
    // that we already have loaded without prompting.
    source_file_ref* OldRef = SimSet->SourceFileRefs[SourceIndex];
    source_file_ref* NewRef = SimSet->SourceFileRefs[SourceIndex];
    do
    {
        // Check that we don't have this already...
        {for (int Idx = 0; Idx < SizeI(SimSet->SourceFileRefs); ++Idx)
        {
            if (Idx == SourceIndex)
                continue;

            // todo: compare absolute paths to ensure we don't get fooled by drive letter/relative crap.
            string AbsSourcePath = GetSourcePath(SimSet->SourceFileRefs[Idx]->SourceFilename);
            if (StringsAreEqualLowercaseNoSlash(AbsSourcePath.c_str(), NewFilename))
            {
                NotImplemented;
                break;
            }
        }}
        if (OldRef != NewRef)
            break;

        file* NewFile = FileFromCache(NewFilename);
        if (NewFile == 0)
            break;

        file_info* NewInfo = GetFileInfo(*NewFile);
        if (NewInfo == 0)
        {
            Log1(ErrorLogMessage, ApplicationLogMessage, "%s: Invalid animation file (GetFileInfo failed)\n", NewFilename);
            break;
        }

        // Ok, scan the file for animation to add to our source list
        bool AnyFound = false;
        {for (int32x Idx = 0; Idx < NewInfo->AnimationCount; ++Idx)
        {
            // todo: robustimicate, if we have a character available, we should check it...
            animation* Animation = NewInfo->Animations[Idx];
            if (Animation == 0)
                continue;

            if (Animation->TrackGroupCount == 0)
                continue;

            bool Found = false;
            {for (int TGIdx = 0; !Found && TGIdx < Animation->TrackGroupCount; ++TGIdx)
            {
                track_group* TrackGroup = Animation->TrackGroups[TGIdx];
                if (!TrackGroup)
                    continue;

                // We're looking for at least one transform_track, basically
                if (TrackGroup->TransformTrackCount)
                    Found = true;
            }}

            // No transform tracks in this source file...
            if (!Found)
                continue;

            // Hey, it's valid!
            AnyFound = true;
        }}

        // Nothing in there for us...
        if (!AnyFound)
        {
            Log1(ErrorLogMessage, ApplicationLogMessage, "%s: Invalid animation file (no animations)\n", NewFilename);
            return 0;
        }

        // Ok, this is pretty likely to be an actual animation source.  First, let's add it to
        // the source list, which also means adding it to the dummy character info
        NewRef = Allocate(source_file_ref, AllocationUnknown);
        AllocatedFileRefs.push_back(NewRef);

        NewRef->SourceFilename = MakeRelativeFilePath(NewFilename);
        NewRef->SourceInfo = (granny_file_info*)NewInfo;
        NewRef->ExpectedAnimCount = NewInfo->AnimationCount;
        NewRef->AnimCRC = ComputeAnimCRC(NewRef->SourceInfo);

        // Ignore the source that's present in the list already, except to stop any
        // monitoring.  If we allocated it, we'll clean it up with the rest of the
        // AllocateFileRefs...
        StopMonitoring(OldRef);
        GStateDeallocate(OldRef->SourceFilename);

        SimSet->SourceFileRefs[SourceIndex] = NewRef;
        StartMonitoring(NewRef);

        // Replace the fake pointers in the animation set...
        SimSet->AnimationSet->SourceFileReferenceCount = SizeI(SimSet->SourceFileRefs);
        SimSet->AnimationSet->SourceFileReferences = &SimSet->SourceFileRefs[0];
    } while (false);

    if (OldRef != NewRef)
    {
        // Zip through the animation specs, and replace the pointers...
        {for (int Idx = 0; Idx < SizeI(SimSet->AnimationSpecs); ++Idx)
        {
            if (SimSet->AnimationSpecs[Idx].SourceReference == OldRef)
                SimSet->AnimationSpecs[Idx].SourceReference = NewRef;
        }}

        SimSet->AnimationSet->AnimationSpecCount = SizeI(SimSet->AnimationSpecs);
        SimSet->AnimationSet->AnimationSpecs = &SimSet->AnimationSpecs[0];
    }
    else
    {
        // @@ todo: Pop undo pos?
    }

    return LuaBoolean(L, (OldRef != NewRef));
}


static int
Edit_GetAnimationSlots(lua_State* L)
{
    // For the current animation set, the list of animation slots
    // AnimationSlotList[Idx] =
    // { Name    = SlotName,       -- "Run", "Walk", "EpicDeath" (editable)
    //   SourceIdx = nil or SourceIdx -- Indexes AnimationList
    //   AnimIdx   = nil or AnimIdx   -- Indexes AnimationList[SourceIdx].ValidAnims
    // }

    Assert(CurrentAnimationSet >= 0 && CurrentAnimationSet < SizeI(AnimationSets));
    simulated_animation_set* SimSet = AnimationSets[CurrentAnimationSet];
    Assert(SimSet);
    Assert(SimSet->AnimationSet->AnimationSpecCount == SizeI(AnimationSlots));

    lua_createtable(L, SizeI(AnimationSlots), 0);
    {for (size_t Idx = 0; Idx < AnimationSlots.size(); ++Idx)
    {
        lua_pushinteger(L, Idx+1);
        {
            lua_createtable(L, 0, 4);

            lua_pushstring(L, "Name");
            lua_pushstring(L, AnimationSlots[Idx]->Name);
            lua_settable(L, -3);

            Assert(CurrentAnimationSet >= 0 && CurrentAnimationSet < SizeI(AnimationSets));

            simulated_animation_set* SimSet = AnimationSets[CurrentAnimationSet];
            animation_spec& Spec = SimSet->AnimationSpecs[Idx];

            lua_pushstring(L, "SourceIdx");
            {
                if (Spec.SourceReference != 0)
                {
                    int FoundIdx = -1;
                    {for (size_t Idx = 0; Idx < SimSet->SourceFileRefs.size(); ++Idx)
                    {
                        if (SimSet->SourceFileRefs[Idx] == Spec.SourceReference)
                        {
                            FoundIdx = Idx;
                            break;
                        }
                    }}
                    if (FoundIdx != -1)
                    {
                        lua_pushinteger(L, FoundIdx+1);
                    }
                    else
                    {
                        Assert(FoundIdx != -1);
                        lua_pushnil(L);
                    }
                }
                else
                {
                    lua_pushnil(L);
                }
            }
            lua_settable(L, -3);

            lua_pushstring(L, "ExpectedName");
            lua_pushstring(L, Spec.ExpectedName);
            lua_settable(L, -3);

            lua_pushstring(L, "AnimIdx");
            {
                if (Spec.SourceReference != 0)
                    lua_pushinteger(L, Spec.AnimationIndex + 1);
                else
                    lua_pushnil(L);
            }
            lua_settable(L, -3);
        }
        lua_settable(L, -3);
    }}

    return 1;
}

static string
MakeUniqueSlotname(char const* NameRoot, int IgnoreIndex = -1)
{
    char SuffixBuffer[10] = { 0 };
    int SuffixInt = 1;
    do
    {
        string Test = NameRoot;
        Test += SuffixBuffer;

        bool IsUnique = true;
        {for (int Idx = 0; IsUnique && Idx < SizeI(AnimationSlots); ++Idx)
        {
            if (Idx == IgnoreIndex)
                continue;

            if (StringsAreEqualLowercase(Test.c_str(), AnimationSlots[Idx]->Name))
                IsUnique = false;
        }}
        if (IsUnique)
            return Test;

        sprintf(SuffixBuffer, " (%d)", SuffixInt++);
    } while (true);
}

static int
Edit_SetAnimationSlotName(lua_State* L)
{
    int SlotIndex = lua_tointeger(L, -2) - 1;
    char const* NewName = lua_tostring(L, -1);

    if (SlotIndex < 0 || SlotIndex >= SizeI(AnimationSlots) ||
        NewName == 0)
    {
        return LuaNil(L);
    }

    PushUndoPos("SetAnimationSlotName");

    // An animation slot may not have the same name as another
    string Unique = MakeUniqueSlotname(NewName, SlotIndex);

    GStateDeallocate(AnimationSlots[SlotIndex]->Name);
    GStateCloneString(AnimationSlots[SlotIndex]->Name, Unique.c_str());

    return LuaString(L, Unique.c_str());
}

static bool
AddAnimationSlot(char const* DefaultName)
{
    // Undo note: AddSlotsForSource calls this in a loop, handle udo at the next level...
    string Unique = MakeUniqueSlotname(DefaultName);

    // Make sure the character is updated if necessary...
    {
        CharacterBindGuard bindGuard;

        // Add to the animation slots
        animation_slot* NewSlot = GStateAllocStruct(animation_slot);
        GStateCloneString(NewSlot->Name, Unique.c_str());
        NewSlot->Index = SizeI(AnimationSlots); // see note in header about this...

        AnimationSlots.push_back(NewSlot);
        AllocatedSlots.push_back(NewSlot);

        // For each set, increase the number of specs
        {for (size_t Idx = 0; Idx < AnimationSets.size(); ++Idx)
        {
            simulated_animation_set* SimSet = AnimationSets[Idx];

            animation_spec NewSpec;
            NewSpec.SourceReference = 0;
            NewSpec.AnimationIndex = -1;
            NewSpec.ExpectedName   = 0;
            SimSet->AnimationSpecs.push_back(NewSpec);

            SimSet->AnimationSet->AnimationSpecCount = SizeI(SimSet->AnimationSpecs);
            SimSet->AnimationSet->AnimationSpecs = &SimSet->AnimationSpecs[0];
        }}

        // And the slots in the current info
        CurrentInfo.AnimationSlotCount = SizeI(AnimationSlots);
        CurrentInfo.AnimationSlots = &AnimationSlots[0];
    }

    return true;
}

static int
Edit_AddAnimationSlot(lua_State* L)
{
    char const* DefaultName = lua_tostring(L, -1);
    if (DefaultName == 0)
        DefaultName = "NewSlot";

    PushUndoPos("AddAnimationSlot");

    return LuaBoolean(L, AddAnimationSlot(DefaultName));
}


static int
Edit_AddSlotsForSource(lua_State* L)
{
    Assert(CurrentAnimationSet >= 0 && CurrentAnimationSet < SizeI(AnimationSets));
    simulated_animation_set* SimSet = AnimationSets[CurrentAnimationSet];
    Assert(SimSet);
    Assert(SimSet->AnimationSet->AnimationSpecCount == SizeI(AnimationSlots));

    int SourceIndex = lua_tointeger(L, -1) - 1;
    if (SourceIndex < 0 || SourceIndex >= SizeI(SimSet->SourceFileRefs))
    {
        return LuaBoolean(L, false);
    }

    source_file_ref* Ref = SimSet->AnimationSet->SourceFileReferences[SourceIndex];
    if (Ref == 0 || Ref->SourceInfo == 0)
    {
        return LuaBoolean(L, false);
    }

    PushUndoPos("AddSlotsForSource");

    {for (int Idx = 0; Idx < Ref->SourceInfo->AnimationCount; ++Idx)
    {
        granny_animation* Anim = Ref->SourceInfo->Animations[Idx];
        if (Anim->TrackGroupCount == 0)
            continue;

        string ShortName = ShortNameForAnim(Anim->Name);
        AddAnimationSlot(ShortName.c_str());

        // Set the slot to point to the source
        SimSet->AnimationSpecs.back().SourceReference = Ref;
        SimSet->AnimationSpecs.back().AnimationIndex  = Idx;
        GStateReplaceString(SimSet->AnimationSpecs.back().ExpectedName, Anim->Name);
    }}


    return LuaBoolean(L, true);
}

static int
Edit_AddSlotForAnimation(lua_State* L)
{
    Assert(CurrentAnimationSet >= 0 && CurrentAnimationSet < SizeI(AnimationSets));
    simulated_animation_set* SimSet = AnimationSets[CurrentAnimationSet];
    Assert(SimSet);
    Assert(SimSet->AnimationSet->AnimationSpecCount == SizeI(AnimationSlots));

    int SourceIndex = lua_tointeger(L, -2) - 1;
    int AnimIdx     = lua_tointeger(L, -1) - 1;

    if (SourceIndex < 0 || SourceIndex >= SizeI(SimSet->SourceFileRefs) || AnimIdx < 0)
        return LuaBoolean(L, false);

    source_file_ref* Ref = SimSet->AnimationSet->SourceFileReferences[SourceIndex];
    if (Ref == 0 || Ref->SourceInfo == 0 || AnimIdx >= Ref->SourceInfo->AnimationCount)
        return LuaBoolean(L, false);

    granny_animation* Anim = Ref->SourceInfo->Animations[AnimIdx];
    if (Anim->TrackGroupCount == 0)
        return LuaBoolean(L, false);

    PushUndoPos("AddSlotsForAnimation");

    string ShortName = ShortNameForAnim(Anim->Name);
    AddAnimationSlot(ShortName.c_str());

    // Set the slot to point to the source
    SimSet->AnimationSpecs.back().SourceReference = Ref;
    SimSet->AnimationSpecs.back().AnimationIndex  = AnimIdx;
    GStateReplaceString(SimSet->AnimationSpecs.back().ExpectedName, Anim->Name);

    return LuaBoolean(L, true);
}

static int
Edit_DeleteSlot(lua_State* L)
{
    int SlotIdx = lua_tointeger(L, -1) - 1;
    if (SlotIdx < 0 || SlotIdx >= SizeI(AnimationSlots))
        return LuaBoolean(L, false);

    PushUndoPos("Delete slot");

    // First, notify the machine of the deletion.
    {
        // Store this, because the bindguard will 0 it out...
        state_machine* CurrentRoot = RootMachine;

        CharacterBindGuard cbg;

        if (CurrentRoot)
            CurrentRoot->NoteAnimationSlotDeleted(AnimationSlots[SlotIdx]);

        // At this point, the animation has been safely removed from all animation nodes.
        // Remove it from the global list of slots
        {
            // We have to delete the name, but *not* the slot itself, which will be
            // cleaned up in the AllocatedSlots deletion.  The name has to go because
            // AllocatedSlots doesn't delete it in case the slot came from the file.
            animation_slot* GarbageSlot = AnimationSlots[SlotIdx];
            GStateDeallocate(GarbageSlot->Name);
            GarbageSlot->Name = 0;

            AnimationSlots.erase(AnimationSlots.begin() + SlotIdx);

            // Reindex the slots
            {for (size_t Idx = 0; Idx < AnimationSlots.size(); ++Idx)
            {
                AnimationSlots[Idx]->Index = Idx;
            }}

            CurrentInfo.AnimationSlotCount = SizeI(AnimationSlots);
            if (CurrentInfo.AnimationSlotCount != 0)
                CurrentInfo.AnimationSlots = &AnimationSlots[0];
            else
                CurrentInfo.AnimationSlots = 0;
        }

        // And from the slot buffers in each animation set...
        {for (size_t SetIdx = 0; SetIdx < AnimationSets.size(); ++SetIdx)
        {
            simulated_animation_set* SimSet = AnimationSets[SetIdx];
            Assert(SimSet);

            GStateDeallocate(SimSet->AnimationSpecs[SlotIdx].ExpectedName);
            SimSet->AnimationSpecs.erase(SimSet->AnimationSpecs.begin() + SlotIdx);
            SimSet->AnimationSet->AnimationSpecCount = SizeI(SimSet->AnimationSpecs);
            if (SimSet->AnimationSet->AnimationSpecCount != 0)
                SimSet->AnimationSet->AnimationSpecs = &SimSet->AnimationSpecs[0];
            else
                SimSet->AnimationSet->AnimationSpecs = 0;
        }}
    }

    return LuaBoolean(L, true);
}



static int
Edit_SetAnimationForSlot(lua_State* L)
{
    Assert(CurrentAnimationSet >= 0 && CurrentAnimationSet < SizeI(AnimationSets));
    simulated_animation_set* SimSet = AnimationSets[CurrentAnimationSet];
    Assert(SimSet);
    Assert(SimSet->AnimationSet->AnimationSpecCount == SizeI(AnimationSlots));

    int SlotIdx     = lua_tointeger(L, -3) - 1;
    int SourceIndex = lua_tointeger(L, -2) - 1;
    int AnimIdx     = lua_tointeger(L, -1) - 1;

    if (SlotIdx < 0 || SlotIdx >= SizeI(AnimationSlots))
        return LuaBoolean(L, false);

    if (SourceIndex < 0 || SourceIndex >= SizeI(SimSet->SourceFileRefs) || AnimIdx < 0)
        return LuaBoolean(L, false);

    source_file_ref* Ref = SimSet->AnimationSet->SourceFileReferences[SourceIndex];
    if (Ref == 0 || Ref->SourceInfo == 0 || AnimIdx >= Ref->SourceInfo->AnimationCount)
        return LuaBoolean(L, false);

    granny_animation* Anim = Ref->SourceInfo->Animations[AnimIdx];
    if (Anim->TrackGroupCount == 0)
        return LuaBoolean(L, false);

    PushUndoPos("SetAnimationForSlot");

    // Rebind the character after setting
    {
        CharacterBindGuard bindGuard;

        SimSet->AnimationSpecs[SlotIdx].SourceReference = Ref;
        SimSet->AnimationSpecs[SlotIdx].AnimationIndex  = AnimIdx;
        GStateReplaceString(SimSet->AnimationSpecs[SlotIdx].ExpectedName, Anim->Name);

        CurrentInfo.AnimationSets[CurrentAnimationSet]->AnimationSpecs[SlotIdx]
            = SimSet->AnimationSpecs[SlotIdx];
    }

    return LuaBoolean(L, true);
}

static int
Edit_SetSlotForSource(lua_State* L)
{
    int32x NodeID  = (int32x)lua_tointeger(L, -2);
    node*  Node    = IDToNode(NodeID);
    int32x SlotIdx = (int32x)lua_tointeger(L, -1);

    bool Success = false;

    anim_source* Source = GSTATE_DYNCAST(Node, anim_source);
    if (Source)
    {
        if (SlotIdx >= 0 && SlotIdx < int(AnimationSlots.size()))
        {
            animation_slot* Slot = AnimationSlots[SlotIdx];
            if (Slot != 0)
            {
                Source->SetAnimationSlot(Slot);
                Success = true;
            }
        }
    }

    return LuaBoolean(L, Success);
}

static int
MaskSource_GetBones(lua_State* L)
{
    int NodeID = lua_tointeger(L, -1);
    node* Node = edit::IDToNode(NodeID);

    mask_source* MaskSource = GSTATE_SLOW_TYPE_CHECK(Node, mask_source);
    if (MaskSource == 0)
    {
        lua_pushnil(L);
        return 1;
    }

    unbound_track_mask const* Mask = (unbound_track_mask const*)MaskSource->GetMask();

    gstate_character_instance* Instance = MaskSource->GetBoundCharacter();
    if (!Instance)
    {
        lua_pushnil(L);
        return 1;
    }

    model* Model = (model*)GetSourceModelForCharacter(Instance);
    if (!Model || !Model->Skeleton)
    {
        lua_pushnil(L);
        return 1;
    }

    skeleton* Skeleton = Model->Skeleton;
    lua_createtable(L, Skeleton->BoneCount, 0);
    {for (int32x Idx = 0; Idx < Skeleton->BoneCount; ++Idx)
    {
        lua_pushinteger(L, Idx+1);
        lua_createtable(L, 0, 2);

        lua_pushstring(L, "Name");
        lua_pushstring(L, Skeleton->Bones[Idx].Name);
        lua_settable(L, -3);

        lua_pushstring(L, "ParentIdx");
        lua_pushinteger(L, Skeleton->Bones[Idx].ParentIndex+1);
        lua_settable(L, -3);

        // n-squared aroodo!
        {
            real32 Value = 0;

            int32x Index = FindMaskIndexForName((unbound_track_mask&)*Mask, Skeleton->Bones[Idx].Name);
            if (Index != -1)
                Value = Mask->Weights[Index].Weight;

            lua_pushstring(L, "Selected");
            lua_pushboolean(L, Value != 0.0f);
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
    }}

    return 1;
}

static int
Edit_SetMaskSource(lua_State* L)
{
    int32x NodeID  = (int32x)lua_tointeger(L, -2);
    node*  Node    = IDToNode(NodeID);
    mask_source* SourceNode = GSTATE_SLOW_TYPE_CHECK(Node, mask_source);
    if (!SourceNode)
    {
        // todo: log error
        return LuaBoolean(L, false);
    }

    if (!lua_istable(L, -1))
    {
        // todo: log error
        return LuaBoolean(L, false);
    }

    gstate_character_instance* Instance = SourceNode->GetBoundCharacter();
    if (!Instance)
        return LuaBoolean(L, false);

    model* Model = (model*)GetSourceModelForCharacter(Instance);
    if (!Model)
        return LuaBoolean(L, false);

    unbound_track_mask* NewMask = NewUnboundTrackMask(0, Model->Skeleton->BoneCount);
    {for (int Idx = 0; Idx < Model->Skeleton->BoneCount; ++Idx)
    {
        lua_pushinteger(L, Idx+1);
        lua_gettable(L, -2);

        int Val = lua_toboolean(L, -1);
        lua_pop(L, 1);

        NewMask->Weights[Idx].Name   = (char*)Model->Skeleton->Bones[Idx].Name;
        NewMask->Weights[Idx].Weight = real32(Val);
    }}

    SourceNode->SetMask((granny_unbound_track_mask*)NewMask);
    FreeUnboundTrackMask(NewMask);

    return LuaBoolean(L, true);
}


source_file_ref* GRANNY
edit::AddAnimationFile(char const* Filename)
{
    if (!Filename)
        return 0;

    // We are adding this only to the current animation set...
    Assert(CurrentAnimationSet >= 0 && CurrentAnimationSet < SizeI(AnimationSets));
    simulated_animation_set* SimSet = AnimationSets[CurrentAnimationSet];
    Assert(SimSet);

    // Check that we don't have this already...
    {for (size_t Idx = 0; Idx < SimSet->SourceFileRefs.size(); ++Idx)
    {
        // todo: compare absolute paths to ensure we don't get fooled by drive letter/relative crap.
        string AbsSourcePath = GetSourcePath(SimSet->SourceFileRefs[Idx]->SourceFilename);
        if (StringsAreEqualLowercaseNoSlash(AbsSourcePath.c_str(), Filename))
        {
            // already present...
            return 0;
        }
    }}

    file* NewFile = FileFromCache(Filename);
    if (!NewFile)
    {
        // todo: log error
        return 0;;
    }

    file_info* NewInfo = GetFileInfo(*NewFile);
    if (!NewInfo)
    {
        Log1(ErrorLogMessage, ApplicationLogMessage, "%s: Invalid animation file (GetFileInfo failed)\n", Filename);
        return 0;;
    }

    if (NewInfo->AnimationCount == 0)
    {
        Log1(ErrorLogMessage, ApplicationLogMessage, "%s: Invalid animation file (no animations)\n", Filename);
        return 0;
    }

    // Ok, scan the file for animation to add to our source list
    bool AnyFound = false;
    {for (int32x Idx = 0; Idx < NewInfo->AnimationCount; ++Idx)
    {
        // todo: robustimicate, if we have a character available, we should check it...
        animation* Animation = NewInfo->Animations[Idx];
        if (Animation == 0)
            continue;

        if (Animation->TrackGroupCount == 0)
            continue;

        bool Found = false;
        {for (int TGIdx = 0; !Found && TGIdx < Animation->TrackGroupCount; ++TGIdx)
        {
            track_group* TrackGroup = Animation->TrackGroups[TGIdx];
            if (!TrackGroup)
                continue;

            // We're looking for at least one transform_track, basically
            if (TrackGroup->TransformTrackCount)
                Found = true;
        }}

        // No transform tracks in this source file...
        if (!Found)
            continue;

        // Hey, it's valid!
        AnyFound = true;
    }}

    // Nothing in there for us...
    if (!AnyFound)
        return 0;

    PushUndoPos("Add animation");

    // Ok, this is pretty likely to be an actual animation source.  First, let's add it to
    // the source list, which also means adding it to the dummy character info
    source_file_ref* NewRef = Allocate(source_file_ref, AllocationUnknown);
    AllocatedFileRefs.push_back(NewRef);

    NewRef->SourceFilename = MakeRelativeFilePath(Filename);
    NewRef->SourceInfo = (granny_file_info*)NewInfo;
    NewRef->ExpectedAnimCount = NewInfo->AnimationCount;
    NewRef->AnimCRC = ComputeAnimCRC(NewRef->SourceInfo);

    // Add it to the list, and refresh the fake pointers in the gstate::animation_set
    SimSet->SourceFileRefs.push_back(NewRef);
    SimSet->AnimationSet->SourceFileReferenceCount = SizeI(SimSet->SourceFileRefs);
    SimSet->AnimationSet->SourceFileReferences = &SimSet->SourceFileRefs[0];

    StartMonitoring(NewRef);

    return NewRef;
}

animation* GRANNY
edit::GetAnimationForCurrSet(int32x SourceIndex, int32x AnimIndex)
{
    Assert(CurrentAnimationSet >= 0 && CurrentAnimationSet < SizeI(AnimationSets));
    simulated_animation_set* SimSet = AnimationSets[CurrentAnimationSet];
    Assert(SimSet);

    if (SourceIndex >= 0 && SourceIndex < SizeI(SimSet->SourceFileRefs))
    {
        granny_file_info* Info = SimSet->SourceFileRefs[SourceIndex]->SourceInfo;
        if (Info && AnimIndex >= 0 && AnimIndex < Info->AnimationCount)
        {
            return (animation*)Info->Animations[AnimIndex];
        }
    }

    return 0;
}

void GRANNY
edit::LookForChangedSources()
{
    source_file_ref* ChangedRef = 0;
    file* ChangedFile = 0;
    file_info* ChangedFileInfo = 0;
    while (GetChangedSourceRef(ChangedRef,
                               ChangedFile,
                               ChangedFileInfo))
    {
        if (ChangedRef->ExpectedAnimCount != 0 && ChangedRef->AnimCRC != 0)
        {
            // We can check this file for consistency...
            if (ChangedFileInfo->AnimationCount != ChangedRef->ExpectedAnimCount ||
                ComputeAnimCRC((granny_file_info*)ChangedFileInfo) != ChangedRef->AnimCRC)
            {
                FadePrintf(gErrorColor, 2.0f, "'%s': incompatible change IGNORED", ChangedRef->SourceFilename);
                FreeFile(ChangedFile);
                ChangedFile = 0;
                ChangedFileInfo = 0;
                continue;
            }
        }

        // Otherwise, replace it in the file...
        {
            CharacterBindGuard cbg;

            // Zip through all the animation sets and replace this sucker...
            {for (size_t Idx = 0; Idx < AnimationSets.size(); ++Idx)
            {
                simulated_animation_set* SimSet = AnimationSets[Idx];
                {for (size_t SrcIdx = 0; SrcIdx < SimSet->SourceFileRefs.size(); ++SrcIdx)
                {
                    if (SimSet->SourceFileRefs[SrcIdx] == ChangedRef)
                    {
                        // Make sure that it's replaced in the file cache
                        string AbsolutePath = GetSourcePath(SimSet->SourceFileRefs[SrcIdx]->SourceFilename);
                        ReplaceCachedFile(AbsolutePath, ChangedFile);

                        SimSet->SourceFileRefs[SrcIdx]->SourceInfo = (granny_file_info*)ChangedFileInfo;
                        SimSet->SourceFileRefs[SrcIdx]->ExpectedAnimCount = ChangedFileInfo->AnimationCount;
                        SimSet->SourceFileRefs[SrcIdx]->AnimCRC = ComputeAnimCRC(SimSet->SourceFileRefs[SrcIdx]->SourceInfo);

                        FadePrintf(gNormalColor, 2, "Reloaded: %s", SimSet->SourceFileRefs[SrcIdx]->SourceFilename);
                    }
                }}
            }}
        }
    }
}

static int
Edit_GetNumOutputs(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -1);
    node*  Node   = IDToNode(NodeID);

    return LuaInteger(L, Node->GetNumOutputs());
}

static int
Edit_GetNumInputs(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -1);
    node*  Node   = IDToNode(NodeID);

    return LuaInteger(L, Node->GetNumInputs());
}

static int
Edit_GetNumInternalOutputs(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -1);
    node*  Node   = IDToNode(NodeID);

    int Internals = 0;
    for (int Idx = 0; Idx < Node->GetNumOutputs(); ++Idx)
    {
        if (Node->IsOutputInternal(Idx))
            ++Internals;
    }

    return LuaInteger(L, Internals);
}

static int
Edit_GetNumInternalInputs(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -1);
    node*  Node   = IDToNode(NodeID);

    int Internals = 0;
    for (int Idx = 0; Idx < Node->GetNumInputs(); ++Idx)
    {
        if (Node->IsInputInternal(Idx))
            ++Internals;
    }

    return LuaInteger(L, Internals);
}

static int
Edit_GetNumExternalOutputs(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -1);
    node*  Node   = IDToNode(NodeID);

    int Externals = 0;
    for (int Idx = 0; Idx < Node->GetNumOutputs(); ++Idx)
    {
        if (Node->IsOutputExternal(Idx))
            ++Externals;
    }

    return LuaInteger(L, Externals);
}

static int
Edit_GetNumExternalInputs(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -1);
    node*  Node   = IDToNode(NodeID);

    int Externals = 0;
    for (int Idx = 0; Idx < Node->GetNumInputs(); ++Idx)
    {
        if (Node->IsInputExternal(Idx))
            ++Externals;
    }

    return LuaInteger(L, Externals);
}

static int
Edit_IsOutputExternal(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -2);
    int32x Output  = (int32x)lua_tointeger(L, -1);
    node*  Node   = IDToNode(NodeID);

    return LuaBoolean(L, Node->IsOutputExternal(Output));
}

static int
Edit_IsInputExternal(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -2);
    int32x Input  = (int32x)lua_tointeger(L, -1);
    node*  Node   = IDToNode(NodeID);

    return LuaBoolean(L, Node->IsInputExternal(Input));
}

static int
Edit_GetInternalInput(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -2);
    int32x Input  = (int32x)lua_tointeger(L, -1);
    node*  Node   = IDToNode(NodeID);

    return LuaInteger(L, Node->GetNthInternalInput(Input));
}

static int
Edit_GetInternalOutput(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -2);
    int32x Output = (int32x)lua_tointeger(L, -1);
    node*  Node   = IDToNode(NodeID);

    return LuaInteger(L, Node->GetNthInternalOutput(Output));
}

static int
Edit_GetExternalInput(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -2);
    int32x Input  = (int32x)lua_tointeger(L, -1);
    node*  Node   = IDToNode(NodeID);

    return LuaInteger(L, Node->GetNthExternalInput(Input));
}

static int
Edit_GetExternalOutput(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -2);
    int32x Output = (int32x)lua_tointeger(L, -1);
    node*  Node   = IDToNode(NodeID);

    return LuaInteger(L, Node->GetNthExternalOutput(Output));
}

static int
Edit_GetOutputName(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -2);
    int32x Output = (int32x)lua_tointeger(L, -1);
    node*  Node   = IDToNode(NodeID);

    return LuaString(L, Node->GetOutputName(Output));
}

static int
Edit_GetInputName(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -2);
    int32x Input = (int32x)lua_tointeger(L, -1);
    node*  Node   = IDToNode(NodeID);

    return LuaString(L, Node->GetInputName(Input));
}

static int
Edit_GetOutputIndexByName(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -2);
    char const* Name = lua_tostring(L, -1);
    node*  Node   = IDToNode(NodeID);

    for (int Idx = 0; Idx < Node->GetNumOutputs(); ++Idx)
    {
        if (_stricmp(Node->GetOutputName(Idx), Name) == 0)
            return LuaInteger(L, Idx);
    }

    return LuaInteger(L, -1);
}

static int
Edit_GetInputIndexByName(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -2);
    char const* Name = lua_tostring(L, -1);
    node*  Node   = IDToNode(NodeID);

    for (int Idx = 0; Idx < Node->GetNumInputs(); ++Idx)
    {
        if (_stricmp(Node->GetInputName(Idx), Name) == 0)
            return LuaInteger(L, Idx);
    }

    return LuaInteger(L, -1);
}


static int
Edit_RenameNodeOutput(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -3);
    int32x Output = (int32x)lua_tointeger(L, -2);
    char const* NewName = lua_tostring(L, -1);

    node* Node = IDToNode(NodeID);
    if (!Node ||
        (Output < 0 || Output >= Node->GetNumOutputs()) ||
        !NewName)
    {
        return LuaBoolean(L, false);
    }

    PushUndoPos("Rename output");

    // A couple of special cases.  First off, we can always deal with parameters and event
    // sources.  Note that the event source output and it's name are always locked.
    if (GSTATE_DYNCAST(Node, parameters))
    {
        Node->SetOutputName(Output, NewName);
        GetRootMachine()->CheckConnections();
        return LuaBoolean(L, true);
    }
    else if (GSTATE_DYNCAST(Node, event_source))
    {
        Node->SetOutputName(Output, NewName);
        Node->SetName(NewName);
        GetRootMachine()->CheckConnections();
        return LuaBoolean(L, true);
    }

    // If the node's parent is a state_machine, this is now strictly disallowed, since the
    // naming of those edges is part of the consistency criterion for the graph.
    if (GSTATE_DYNCAST(Node->GetParent(), state_machine))
    {
        return LuaBoolean(L, false);
    }

    // Otherwise ok...
    Node->SetOutputName(Output, NewName);
    GetRootMachine()->CheckConnections();
    return LuaBoolean(L, true);
}

static int
Edit_SetParameterOutputProperties(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -5);
    int32x Output = (int32x)lua_tointeger(L, -4);
    granny_real32 Min = (granny_real32)lua_tonumber(L, -3);
    granny_real32 Val = (granny_real32)lua_tonumber(L, -2);
    granny_real32 Max = (granny_real32)lua_tonumber(L, -1);
    node* Node = IDToNode(NodeID);

    if (!Node || (Output < 0 || Output >= Node->GetNumOutputs()))
    {
        return LuaBoolean(L, false);
    }

    parameters* Parameters = GSTATE_DYNCAST(Node, parameters);
    if (!Parameters)
    {
        return LuaBoolean(L, false);
    }

    if (Min > Max)
    {
        Min = Max;
    }

    if (Val <= Min)
        Val = Min;
    if (Val >= Max)
        Val = Max;

    Parameters->SetScalarOutputRange(Output, Min, Max);
    Parameters->SetParameter(Output, Val);

    return LuaBoolean(L, true);
}

static int
Edit_AddPossibleEvent(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -2);
    char const* NewEvent = lua_tostring(L, -1);

    node* Node = IDToNode(NodeID);

    if (!Node || !NewEvent)
    {
        return LuaBoolean(L, false);
    }

    event_source* EventSource = GSTATE_DYNCAST(Node, event_source);
    if (!EventSource)
    {
        return LuaBoolean(L, false);
    }

    EventSource->AddPossibleEvent(NewEvent);
    return LuaBoolean(L, true);
}


static int
Edit_GetComments(lua_State* L)
{
    int NumComments = CurrentContainer->GetNumComments();

    lua_createtable(L, NumComments, 0);
    for(int32x Idx = 0; Idx < NumComments; ++Idx)
    {
        lua_pushinteger(L, Idx + 1);

        granny_int32x PosX = 0, PosY = 0;
        granny_int32x SizeX = 0, SizeY = 0;
        if (CurrentContainer->GetCommentPosition(Idx, &PosX, &PosY, &SizeX, &SizeY))
        {
            lua_createtable(L, 0, 5);

            lua_pushstring(L, "Text"); lua_pushstring(L, CurrentContainer->GetCommentText(Idx)); lua_settable(L, -3);
            lua_pushstring(L, "Pos");  PushPoint(L, LuaPoint(PosX, PosY));                       lua_settable(L, -3);
            lua_pushstring(L, "Size"); PushPoint(L, LuaPoint(SizeX, SizeY));                     lua_settable(L, -3);
        }
        else
        {
            lua_pushnil(L);
        }

        lua_settable(L, -3);
    }

    return true;
}


static int
Edit_AddComment(lua_State* L)
{
    char const* InitialComment = lua_tostring(L, -2);
    if (!InitialComment)
        return LuaNil(L);

    LuaPoint pt(0, 0);
    if (ExtractPoint(L, -1, pt) == false)
    {
        static int CreateOffset = 0;
        pt = LuaPoint(100 + CreateOffset,
                      100 + CreateOffset);
        CreateOffset = (CreateOffset + 10)%100;
    }

    lua_getglobal(L, "ContainerEditingOffset");

    LuaPoint EditingOffset(0, 0);
    ExtractPoint(L, -1, EditingOffset);
    lua_pop(L, 1);

    pt.x += EditingOffset.x;
    pt.y += EditingOffset.y;

    PushUndoPos("Add Comment");

    int CommentIdx = CurrentContainer->AddComment(InitialComment, pt.x, pt.y, 0, 0);
    if (CommentIdx == -1)
        return LuaNil(L);
    else
        return LuaInteger(L, CommentIdx + 1);
}

static int
Edit_DeleteComment(lua_State* L)
{
    int CommentIdx = lua_tointeger(L, -1) - 1;

    PushUndoPos("Delete Comment");

    CurrentContainer->DeleteComment(CommentIdx);
    return 0;
}


static int
Edit_GetCommentText(lua_State* L)
{
    int CommentIdx = lua_tointeger(L, -1) - 1;

    return LuaString(L, CurrentContainer->GetCommentText(CommentIdx));
}

static int
Edit_SetCommentText(lua_State* L)
{
    int CommentIdx = lua_tointeger(L, -2) - 1;

    PushUndoPos("set comment text");

    return LuaBoolean(L, CurrentContainer->SetCommentText(CommentIdx, lua_tostring(L, -1)));
}

static int
Edit_GetCommentPosition(lua_State* L)
{
    int CommentIdx = lua_tointeger(L, -1) - 1;

    granny_int32x PosX = 0, PosY = 0;
    granny_int32x SizeX = 0, SizeY = 0;
    if (CurrentContainer->GetCommentPosition(CommentIdx, &PosX, &PosY, &SizeX, &SizeY))
    {
        PushPoint(L, LuaPoint(PosX, PosY));
        PushPoint(L, LuaPoint(SizeX, SizeY));
        return 2;
    }
    else
    {
        return LuaNil(L);
    }
}


static int
Edit_SetCommentPosition(lua_State* L)
{
    int CommentIdx = lua_tointeger(L, -3) - 1;

    PushUndoPos("comment move", false, eCommentMove);

    LuaPoint pos, size;
    if (!ExtractPoint(L, -2, pos) ||
        !ExtractPoint(L, -1, size))
    {
        return LuaBoolean(L, false);
    }

    return LuaBoolean(L, CurrentContainer->SetCommentPosition(CommentIdx,
                                                              pos.x, pos.y,
                                                              size.x, size.y));
}


bool GRANNY
Edit_Register(lua_State* State)
{
    LoadRecentFileList();

    lua_register(State, "Edit_GetCurrentContainerID", Edit_GetCurrentContainerID);
    lua_register(State, "Edit_CurrentContainerIsBlendgraph", Edit_CurrentContainerIsBlendgraph);
    lua_register(State, "Edit_ParentIsBlendgraphOrNil", Edit_ParentIsBlendgraphOrNil);

    lua_register(State, "Edit_SetModelFromFilename", Edit_SetModelFromFilename);
    lua_register(State, "Edit_PromptForModel",       Edit_PromptForModel);
    lua_register(State, "Edit_SetModelIdx",     Edit_SetModelIdx);
    lua_register(State, "Edit_GetModelIdx",     Edit_GetModelIdx);
    lua_register(State, "Edit_GetModelStrings", Edit_GetModelStrings);
    lua_register(State, "Edit_Reset",           Edit_Reset);
    lua_register(State, "Edit_SaveToFile",      Edit_SaveToFile);
    lua_register(State, "Edit_PromptForLoad",   Edit_PromptForLoad);
    lua_register(State, "Edit_LoadFromFile",    Edit_LoadFromFile);

    lua_register(State, "Edit_GetAnimationSets",       Edit_GetAnimationSets);
    lua_register(State, "Edit_SetCurrentAnimationSet", Edit_SetCurrentAnimationSet);
    lua_register(State, "Edit_SetAnimationSetName",    Edit_SetAnimationSetName);

    lua_register(State, "Edit_NewAnimationSet",        Edit_NewAnimationSet);
    lua_register(State, "Edit_DeleteCurrentAnimationSet", Edit_DeleteCurrentAnimationSet);
    lua_register(State, "Edit_CopyAnimationSet",        Edit_CopyAnimationSet);

    lua_register(State, "Edit_AddSource",             Edit_AddSource);
    lua_register(State, "Edit_AddSourceFromFilename", Edit_AddSourceFromFilename);
    lua_register(State, "Edit_RemoveSource",          Edit_RemoveSource);
    lua_register(State, "Edit_GetSources",            Edit_GetSources);
    lua_register(State, "Edit_ReplaceSource",         Edit_ReplaceSource);

    lua_register(State, "Edit_GetAnimationSlots",    Edit_GetAnimationSlots);
    lua_register(State, "Edit_SetAnimationSlotName", Edit_SetAnimationSlotName);
    lua_register(State, "Edit_AddAnimationSlot",     Edit_AddAnimationSlot);
    lua_register(State, "Edit_AddSlotsForSource",    Edit_AddSlotsForSource);
    lua_register(State, "Edit_AddSlotForAnimation",  Edit_AddSlotForAnimation);
    lua_register(State, "Edit_DeleteSlot",           Edit_DeleteSlot);
    lua_register(State, "Edit_SetAnimationForSlot",  Edit_SetAnimationForSlot);
    lua_register(State, "Edit_SetSlotForSource",     Edit_SetSlotForSource);

    lua_register(State, "Edit_AddNode",         Edit_AddNode);
    lua_register(State, "Edit_AddNodeAtPt",     Edit_AddNodeAtPt);
    lua_register(State, "Edit_DeleteNode",      Edit_DeleteNode);
    lua_register(State, "Edit_DeleteSelection", Edit_DeleteSelection);
    lua_register(State, "Edit_ConnectNodes",    Edit_ConnectNodes);
    lua_register(State, "Edit_ReplaceNodeWith", Edit_ReplaceNodeWith);

    lua_register(State, "Edit_GetNodeType", Edit_GetNodeType);

    // Duplicate the intput/output interface for node.
    lua_register(State, "Edit_GetNumOutputs",         Edit_GetNumOutputs);
    lua_register(State, "Edit_GetNumInputs",          Edit_GetNumInputs);
    lua_register(State, "Edit_GetNumInternalOutputs", Edit_GetNumInternalOutputs);
    lua_register(State, "Edit_GetNumInternalInputs",  Edit_GetNumInternalInputs);
    lua_register(State, "Edit_GetNumExternalOutputs", Edit_GetNumExternalOutputs);
    lua_register(State, "Edit_GetNumExternalInputs",  Edit_GetNumExternalInputs);
    lua_register(State, "Edit_IsOutputExternal",      Edit_IsOutputExternal);
    lua_register(State, "Edit_IsInputExternal",       Edit_IsOutputExternal);
    lua_register(State, "Edit_GetInternalInput",      Edit_GetInternalInput);
    lua_register(State, "Edit_GetInternalOutput",     Edit_GetInternalOutput);
    lua_register(State, "Edit_GetExternalInput",      Edit_GetExternalInput);
    lua_register(State, "Edit_GetExternalOutput",     Edit_GetExternalOutput);

    lua_register(State, "Edit_GetOutputName",        Edit_GetOutputName);
    lua_register(State, "Edit_GetInputName",         Edit_GetInputName);
    lua_register(State, "Edit_GetOutputIndexByName", Edit_GetOutputIndexByName);
    lua_register(State, "Edit_GetInputIndexByName",  Edit_GetInputIndexByName);

    lua_register(State, "Edit_RenameNodeOutput", Edit_RenameNodeOutput);

    // Parameter manipulation
    lua_register(State, "Edit_SetParameterOutputProperties", Edit_SetParameterOutputProperties);
    lua_register(State, "Edit_AddPossibleEvent", Edit_AddPossibleEvent);

    // For blend graphs...
    lua_register(State, "Edit_DisconnectEdge",  Edit_DisconnectEdge);

    lua_register(State, "Edit_CanDescendNode", Edit_CanDescendNode);
    lua_register(State, "Edit_DescendNode", Edit_DescendNode);
    lua_register(State, "Edit_GetContainerStack", Edit_GetContainerStack);
    lua_register(State, "Edit_PopContainerLevels", Edit_PopContainerLevels);

    lua_register(State, "Edit_AddTransition", Edit_AddTransition);
    lua_register(State, "Edit_SetActiveState", Edit_SetActiveState);
    lua_register(State, "Edit_SetDefaultState", Edit_SetDefaultState);
    lua_register(State, "Edit_TriggerTransition", Edit_TriggerTransition);

    lua_register(State, "Edit_ClearSelection",         Edit_ClearSelection);
    lua_register(State, "Edit_SelectByID",             Edit_SelectByID);
    lua_register(State, "Edit_MoveToFrontByID",        Edit_MoveToFrontByID);
    lua_register(State, "Edit_ToggleSelectByID",       Edit_ToggleSelectByID);
    lua_register(State, "Edit_IsSelectedByID",         Edit_IsSelectedByID);
    lua_register(State, "Edit_IsSolelySelectedByID",   Edit_IsSolelySelectedByID);
    lua_register(State, "Edit_GetSelection",           Edit_GetSelection);
    lua_register(State, "Edit_IsTransition",           Edit_IsTransition);
    lua_register(State, "Edit_NodeNamesForTransition", Edit_NodeNamesForTransition);

    lua_register(State, "Edit_IconsForNode", Edit_IconsForNode);
    lua_register(State, "Edit_NameForNode", Edit_NameForNode);
    lua_register(State, "Edit_SetNameForNode", Edit_SetNameForNode);

    lua_register(State, "Edit_NameForTokenized", Edit_NameForTokenized);
    lua_register(State, "Edit_SetNameForTokenized", Edit_SetNameForTokenized);

    // Better place for these?
    lua_register(State, "MaskSource_GetBones", MaskSource_GetBones);
    lua_register(State, "MaskSource_Set", Edit_SetMaskSource);

    // actually in ui_blendnode_edit
    lua_register(State, "Edit_BlendNodeEdit", Edit_BlendNodeEdit);
    lua_register(State, "Edit_StateNodeEdit", Edit_StateNodeEdit);
    lua_register(State, "Edit_TransitionEdit", Edit_TransitionEdit);

    lua_register(State, "Edit_PushUndoPos", Edit_PushUndoPos);
    lua_register(State, "Edit_PopUndo",     Edit_PopUndo);
    lua_register(State, "Edit_PushUndoCollapseMarker", Edit_PushCollapseMarker);
    lua_register(State, "Edit_UndoCollapseToMarker", Edit_CollapseToMarker);

    lua_register(State, "Edit_UndoStackSize",  Edit_UndoStackSize);
    lua_register(State, "Edit_UndoStackCount", Edit_UndoStackCount);
    lua_register(State, "Edit_ScrubListSize",  Edit_ScrubListSize);
    lua_register(State, "Edit_ScrubRange",     Edit_ScrubRange);

    lua_register(State, "Edit_InScrubMode",    Edit_InScrubMode);
    lua_register(State, "Edit_SetScrubMode",   Edit_SetScrubMode);
    lua_register(State, "Edit_ScrubTo",        Edit_ScrubTo);
    lua_register(State, "Edit_SetScrubTimeMotion", Edit_SetScrubTimeMotion);
    lua_register(State, "Edit_GetScrubTimeMotion", Edit_GetScrubTimeMotion);

    lua_register(State, "Edit_GetGlobalClock",  Edit_GetGlobalClock);
    lua_register(State, "Edit_GetAbsoluteTime", Edit_GetAbsoluteTime);
    lua_register(State, "Edit_IsDClick",        Edit_IsDClick);
    lua_register(State, "Edit_SetPause",        Edit_SetPause);
    lua_register(State, "Edit_GetPause",        Edit_GetPause);

    lua_register(State, "Edit_GetRecentFiles",  Edit_GetRecentFiles);
    lua_register(State, "Edit_GetRecentModels",  Edit_GetRecentModels);

    lua_register(State, "Edit_GetComments", Edit_GetComments);
    lua_register(State, "Edit_AddComment", Edit_AddComment);
    lua_register(State, "Edit_DeleteComment", Edit_DeleteComment);
    lua_register(State, "Edit_GetCommentText", Edit_GetCommentText);
    lua_register(State, "Edit_SetCommentText", Edit_SetCommentText);
    lua_register(State, "Edit_GetCommentPosition", Edit_GetCommentPosition);
    lua_register(State, "Edit_SetCommentPosition", Edit_SetCommentPosition);

    return true;
}


