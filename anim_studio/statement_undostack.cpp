// ========================================================================
// $File: //jeffr/granny_29/statement/statement_undostack.cpp $
// $DateTime: 2012/07/06 13:27:18 $
// $Change: 38171 $
// $Revision: #7 $
//
// $Notice: $
// ========================================================================
#include "statement_undostack.h"
#include "statement.h"
#include "statement_editstate.h"
#include "ui_character_render.h"

#include "ui_core.h"

#include "granny_assert.h"
#include "granny_camera.h"
#include "granny_file_reader.h"
#include "granny_file_writer.h"
#include "granny_memory.h"
#include "granny_memory_file_reader.h"
#include "granny_memory_file_writer.h"

#include "gstate_character_info.h"
#include "gstate_state_machine.h"
#include "gstate_token_context.h"

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include "granny_system_clock.h"

#include <vector>
#include <string>
#include <algorithm>
#include <windows.h>

USING_GRANNY_NAMESPACE;
USING_GSTATE_NAMESPACE;
using namespace std;

struct UndoPos
{
    real32     TimeVal;        // Invalid for undo positions, set for Scrubbing Positions
    real32     CharacterClock; // Invalid for undo positions, set for Scrubbing Positions

    string     PositionName;
    EUndoClass PositionClass;
    uint32     PositionUID;

    int32     CollapseMarker;

    int32  CharacterStateByteCount;
    uint8* CharacterStateBytes;

    int32  SnapshotByteCount;
    uint8* SnapshotBytes;

    // These are the bits that store off the current editing state
    uint32         CurrentContainerUID;
    vector<uint32> CurrentSelection;

    int32 CurrentAnimationSet;

    real32 CharacterXForm[16];
    ECameraUp UpSetting;

    camera Camera;
    bool   CameraTracks;

    int GetSize() const
    {
        // Just get the big stuff.
        return (CharacterStateByteCount + SnapshotByteCount + sizeof(*this));
    }

    UndoPos()
    {
        TimeVal = -1;
        CharacterClock = 0;
        PositionUID = uint32(-1);
        CollapseMarker = 0;
        CharacterStateByteCount = 0;
        CharacterStateBytes = 0;
        SnapshotByteCount = 0;
        SnapshotBytes = 0;
        CurrentContainerUID = uint32(-1);
        CurrentAnimationSet = -1;
    }

    ~UndoPos()
    {
        GrannyFreeMemoryWriterBuffer(SnapshotBytes);       SnapshotBytes = 0;
        GrannyFreeMemoryWriterBuffer(CharacterStateBytes); CharacterStateBytes = 0;
    }
};

typedef vector<UndoPos*> UndoVec;

static UndoVec s_UndoPositions;
static UndoVec s_Scrub;

static const int s_MaxScrubSize  = 20 << 20;
static int       s_CurrScrubSize = 0;

static int32 s_CurrCollapseMarker = 1;

// Guards against reentering the undo system when a stack manipulation is taking place...
static bool s_UndoGuard = false;
struct undo_guard
{
    // Does this look thread safe to you?
    undo_guard() {
        Assert(s_UndoGuard == false);
        s_UndoGuard = true;
    }
    ~undo_guard() {
        Assert(s_UndoGuard == true);
        s_UndoGuard = false;
    };
};

static void
ClearScrubs()
{
    {for (size_t Idx = 0; Idx < s_Scrub.size(); ++Idx)
    {
        delete s_Scrub[Idx];
    }}
    s_Scrub.clear();
    s_CurrScrubSize = 0;
}


static UndoPos*
GenerateUndoPosition(bool SnapshotOnly)
{
    StTMZone(__FUNCTION__);

    UndoPos* NewPos = new UndoPos;
    NewPos->TimeVal = -1;
    NewPos->CharacterClock = 0;
    // NewPos->PositionName;  // set by caller
    // NewPos->PositionClass;
    // NewPos->PositionUID;
    NewPos->CharacterStateByteCount = 0;
    NewPos->CharacterStateBytes     = 0;

    // Capture the file state
    if (!SnapshotOnly)
    {
        int32x SizeGuess = 4000;  // close to one page, but less for mem overhead

        // If we have a previous undo pos, guess that this one will be maybe just a touch
        // bigger than that one...
        if (s_UndoPositions.empty() == false)
        {
            UndoPos* UndoPos = s_UndoPositions.back();
            SizeGuess = int32x(UndoPos->CharacterStateByteCount * 1.05f);
        }

        file_writer* Writer = CreateMemoryFileWriter(SizeGuess);
        if (edit::FlattenStateToWriter(Writer, SizeGuess))
        {
            StealMemoryWriterBuffer(Writer,
                                    &NewPos->CharacterStateBytes,
                                    &NewPos->CharacterStateByteCount);
        }
        else
        {
            delete NewPos;
            return 0;
        }
        CloseFileWriter(Writer);
    }
    else
    {
        NewPos->SnapshotByteCount = 0;
        NewPos->SnapshotBytes     = 0;
    }

    // And the current instance state...
    {
        file_writer* Writer = CreateMemoryFileWriter(4000);  // close to one page, but less for mem overhead

        // Any tokenized bit that shares the editing context will do, but let's just use
        // the root machine for consistency.
        tokenized* Root = edit::GetRootMachine();
        if (Root)
        {
            if (Root->GetTokenContext()->CaptureStateSnapshot((granny_file_writer*)Writer))
            {
                StealMemoryWriterBuffer(Writer,
                                        &NewPos->SnapshotBytes,
                                        &NewPos->SnapshotByteCount);
            }
            else
            {
                delete NewPos;
                return 0;
            }
        }
        CloseFileWriter(Writer);
    }

    // Editing state
    if (!SnapshotOnly)
    {
        // The current container
        NewPos->CurrentContainerUID = edit::GetCurrentContainer()->GetUID();
        NewPos->CurrentAnimationSet = edit::GetCurrentAnimationSet();

        // Selection
        vector<tokenized*> Selection;
        edit::GetSelection(Selection);
        NewPos->CurrentSelection.resize(Selection.size());

        {for (size_t Idx = 0; Idx < Selection.size(); ++Idx)
        {
            NewPos->CurrentSelection[Idx] = Selection[Idx]->GetUID();
        }}

        // Motion extracted position
        GetCharacterXForm(NewPos->CharacterXForm);
        GetSceneCamera(&NewPos->Camera, &NewPos->CameraTracks);
        NewPos->UpSetting = GetCameraUpSetting();
    }
    else
    {
        NewPos->CurrentContainerUID = 0;
    }

    return NewPos;
}

bool GRANNY
PushUndoPos(char const* PositionName,
            bool        DontMarkModified /* = false */,
            EUndoClass  Class /*= eNeverCollapse*/,
            uint32      UID   /*= 0*/)
{
    StTMZone(__FUNCTION__);

    if (DontMarkModified == false)
        edit::MarkModified();

    undo_guard Guard;

    if (Class != eNeverCollapse &&
        s_UndoPositions.empty() == false &&
        s_UndoPositions.back()->PositionClass == Class &&
        s_UndoPositions.back()->PositionUID == UID)
    {
        return true;
    }

    UndoPos* NewPos = 0;
    {
        //     system_clock Start, End;
        //     GetSystemSeconds(&Start);

        NewPos = GenerateUndoPosition(false);

        //     GetSystemSeconds(&End);

        //     char buffer[512];
        //     sprintf(buffer, "  snap bytes: %d [%f ms]\n", NewPos->SnapshotByteCount, GetSecondsElapsed(Start, End) * 1000);
        //     OutputDebugString(buffer);
    }
    if (NewPos == 0)
        return false;
    NewPos->PositionName  = PositionName ? PositionName : "<Action>";
    NewPos->PositionClass = Class;
    NewPos->PositionUID   = UID;

    // Restart scrubbing
    ClearScrubs();

    StTMPrintf("Pushing undo: %s (%d/%d bytes)",
               StTMDynString(PositionName),
               NewPos->CharacterStateByteCount,
               NewPos->SnapshotByteCount);
    s_UndoPositions.push_back(NewPos);

    return true;
}

bool GRANNY
PopUndoPos()
{
    StTMZone(__FUNCTION__);
    undo_guard Guard;

    if (s_UndoPositions.empty())
    {
        FadePrintf(gWarningColor, 2, "Nothing to undo");
        return false;
    }

    edit::MarkModified();
    
    UndoPos* ToPop = s_UndoPositions.back();
    StTMPrintf("Poping undo: %s", StTMDynString(ToPop->PositionName.c_str()));

    FadePrintf(gNotifyColor, 2, "Undoing: %s", ToPop->PositionName.c_str());

    s_UndoPositions.pop_back();

    // Restart scrubbing.  Theoretically we can fail below, but we've popped the undo
    // stack, so clear the scrub anyways.
    ClearScrubs();

    file* File = 0;
    gstate_character_info* NewInfo = 0;
    {
        StTMZone("Reading CharacterInfo");
        file_reader* Reader = CreateMemoryFileReader(ToPop->CharacterStateByteCount,
                                                     ToPop->CharacterStateBytes);

        if (!GStateReadCharacterInfoFromReader((granny_file_reader*)Reader, (granny_file*&)File, NewInfo))
        {
            Assert(false);  // this is really bad.
            CloseFileReader(Reader);
            return false;
        }
        CloseFileReader(Reader);
    }

    bool Success = edit::ResetFromFileInfoPair(File, NewInfo, NULL);

    // Todo: should go beneath the edit state reset?
    // Now we need to apply the snapshot.  Again, we'll use the current root machine, just for consistency.
    tokenized* Root = edit::GetRootMachine();
    if (Root)
    {
        file_reader* Reader = CreateMemoryFileReader(ToPop->SnapshotByteCount, ToPop->SnapshotBytes);

        if (!Root->GetTokenContext()->ResetFromStateSnapshot((granny_file_reader*)Reader))
        {
            // that's *really* bad.  Uh, we've replaced the whole thing at this point
            Assert(false);
            // todo: log warning that undo has become corrupted
        }

        CloseFileReader(Reader);
    }
    else
    {
        Assert(ToPop->SnapshotByteCount == 0);
    }


    // Editing state
    {
        token_context* Context = Root->GetTokenContext();

        // Set the current container
        tokenized* ContAsToken = Context->GetProductForUID(ToPop->CurrentContainerUID);
        container* Container = GSTATE_DYNCAST(ContAsToken, container);
        Assert(Container);
        edit::SetCurrentContainer(Container);

        edit::ClearSelection();
        vector<tokenized*> Selection;
        {for (size_t Idx = 0; Idx < ToPop->CurrentSelection.size(); ++Idx)
        {
            edit::AddToSelection(Context->GetProductForUID(ToPop->CurrentSelection[Idx]));
        }}

        // Motion extracted position
        SetCharacterXForm(ToPop->CharacterXForm);
        SetSceneCamera(ToPop->Camera, ToPop->CameraTracks);
        SetCameraUpSetting(ToPop->UpSetting);
        
        edit::SetCurrentAnimationSet(ToPop->CurrentAnimationSet);

        // Set the character clock
        if (edit::GetCharacterInstance())
            edit::GetCharacterInstance()->CurrentTime = ToPop->CharacterClock;
    }

    delete ToPop; // to get the string and vectors...
    return Success;
}

int32x GRANNY
PushCollapseMarker(char const* PositionName)
{
    PushUndoPos(PositionName);
    if (s_UndoPositions.empty())
        return -1;

    UndoPos* Pos = s_UndoPositions.back();
    Pos->CollapseMarker = s_CurrCollapseMarker++;

    return Pos->CollapseMarker;
}


bool GRANNY
CollapseToMarker(int32x Marker)
{
    // Find the thing first...
    int32x Found = -1;
    for (int Idx = int(s_UndoPositions.size() - 1); Idx >= 0 && Found == -1; --Idx)
    {
        if (s_UndoPositions[Idx]->CollapseMarker == Marker)
            Found = Idx;
    }

    if (Found == -1)
        return false;

    // Otherwise, move back to that spot...
    ClearScrubs();

    while (s_UndoPositions.size() > size_t(Found + 1))
    {
        UndoPos* Pos = s_UndoPositions.back();
        s_UndoPositions.pop_back();

        delete Pos;
    }
    
    return true;
}

void GRANNY
ClearUndoStack()
{
    StTMZone(__FUNCTION__);
    ClearScrubs();

    while (!s_UndoPositions.empty())
    {
        UndoPos* ToPop = s_UndoPositions.back();
        s_UndoPositions.pop_back();
        delete ToPop;
    }
}

int GRANNY
UndoStackCount()
{
    return (int)s_UndoPositions.size();
}


int GRANNY
UndoStackSize()
{
    int Sum = 0;
    {for (size_t Idx = 0; Idx < s_UndoPositions.size(); ++Idx)
    {
        Sum += s_UndoPositions[Idx]->GetSize();
    }}

    return Sum;
}


int GRANNY
ScrubListSize()
{
    return s_CurrScrubSize;
}


bool GRANNY
PushTimeScrub(real32 CurrT)
{
    StTMZone(__FUNCTION__);
    undo_guard Guard;

    if (s_Scrub.empty() == false)
    {
        Assert(CurrT >= s_Scrub.back()->TimeVal);
    }

    UndoPos* NewPos = GenerateUndoPosition(true);
    if (NewPos == 0)
        return false;
    NewPos->TimeVal = CurrT;

    if (edit::GetCharacterInstance())
        NewPos->CharacterClock = edit::GetCharacterInstance()->CurrentTime;
    else
        NewPos->CharacterClock = -1;

    NewPos->PositionClass = eNeverCollapse;
    NewPos->PositionUID   = 0;

    GetCharacterXForm(NewPos->CharacterXForm);
    GetSceneCamera(&NewPos->Camera, &NewPos->CameraTracks);

    s_Scrub.push_back(NewPos);
    s_CurrScrubSize += NewPos->GetSize();
    while (s_CurrScrubSize > s_MaxScrubSize)
    {
        s_CurrScrubSize -= s_Scrub.front()->GetSize();

        UndoPos* Pos = *(s_Scrub.begin());
        s_Scrub.erase(s_Scrub.begin());
        delete Pos;
    }

    return true;
}

bool GRANNY
GetScrubWindow(real32& Minimum,
               real32& Maximum)
{
    if (s_Scrub.empty())
        return false;

    Minimum = s_Scrub.front()->TimeVal;
    Maximum = s_Scrub.back()->TimeVal;
    Assert(Minimum <= Maximum);

    return true;
}


struct TimeValLess
{
    bool operator()(UndoPos* One, real32 Val) { return One->TimeVal < Val; }
    bool operator()(real32 Val, UndoPos* One) { return Val < One->TimeVal; }
    bool operator()(UndoPos* One, UndoPos* Two) { return One->TimeVal < Two->TimeVal; }
};

bool GRANNY
ScrubToTime(real32 ScrubTime)
{
    StTMZone(__FUNCTION__);
    real32 Minimum, Maximum;
    GetScrubWindow(Minimum, Maximum);
    if (ScrubTime < Minimum || ScrubTime > Maximum)
        return false;

    UndoVec::iterator itr =
        std::lower_bound(s_Scrub.begin(), s_Scrub.end(), ScrubTime, TimeValLess());

    Assert(itr != s_Scrub.end());

    UndoPos* ScrubShot = *itr;

    // Apply the snapshot *without* altering the underlying machine or selection sets...

    // Again, we'll use the current root machine, just for consistency.
    tokenized* Root = edit::GetRootMachine();
    if (Root)
    {
        file_reader* Reader = CreateMemoryFileReader(ScrubShot->SnapshotByteCount, ScrubShot->SnapshotBytes);

        if (!Root->GetTokenContext()->ResetFromStateSnapshot((granny_file_reader*)Reader))
        {
            // that's *really* bad.  Uh, we've replaced the whole thing at this point
            Assert(false);
            // todo: log warning that undo has become corrupted
        }

        CloseFileReader(Reader);
    }
    else
    {
        Assert(ScrubShot->SnapshotByteCount == 0);
    }

    // But *do* alter the CharacterXForm
    SetCharacterXForm(ScrubShot->CharacterXForm);
    SetSceneCamera(ScrubShot->Camera, ScrubShot->CameraTracks);

    // Set the character clock
    if (edit::GetCharacterInstance())
        edit::GetCharacterInstance()->CurrentTime = ScrubShot->CharacterClock;

    return true;
}
