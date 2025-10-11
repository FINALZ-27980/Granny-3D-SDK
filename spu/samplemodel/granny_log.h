#if !defined(GRANNY_LOG_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_log.h $
// $DateTime: 2012/04/27 12:43:03 $
// $Change: 37156 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================

/* ========================================================================
   Explicit Dependencies
   ======================================================================== */
#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(LogGroup);

EXPTYPE enum log_message_type
{
    // An ignored log entry is just that - ignored.
    IgnoredLogMessage,

    // A note is simply information about something that occurred.  It
    // does not indicate that something bad is happening.
    NoteLogMessage,

    // A warning is an alert that something potentially dangerous may
    // be occurring, although it is not clear from the current context
    // whether or not it will result in undesirable behavior.
    WarningLogMessage,

    // An error is an alert that undesirable behavior will occur, or
    // more to the point, _would be_ occuring if the error checking
    // code did not catch it.
    ErrorLogMessage,

    OnePastLastMessageType,

    log_message_type_forceint = 0x7fffffff
};

EXPTYPE enum log_message_origin
{
    // Debugging
    NotImplementedLogMessage,

    // Application codes
    ApplicationLogMessage,

    // Platform dependent code
    Win32SubsystemLogMessage,
    Win64SubsystemLogMessage,
    WinXXSubsystemLogMessage,
    MacOSSubsystemLogMessage,
    ANSISubsystemLogMessage,
    NACLSubsystemLogMessage,
    GamecubeSubsystemLogMessage,
    PS2SubsystemLogMessage,
    PSPSubsystemLogMessage,
    PS3SubsystemLogMessage,
    XboxSubsystemLogMessage,
    XenonSubsystemLogMessage,
    N3DSSubsystemLogMessage,
    WiiSubsystemLogMessage,
    WiiUSubsystemLogMessage,
    SekritSubsystemLogMessage,

    MAXSubsystemLogMessage,
    MayaSubsystemLogMessage,
    XSISubsystemLogMessage,
    ModoSubsystemLogMessage,
    LightwaveSubsystemLogMessage,

    // Platform independent code
    FileWritingLogMessage,
    FileReadingLogMessage,
    ExporterLogMessage,
    CompressorLogMessage,
    StringLogMessage,
    StringTableLogMessage,
    VertexLayoutLogMessage,
    MeshLogMessage,
    PropertyLogMessage,
    SkeletonLogMessage,
    AnimationLogMessage,
    SetupGraphLogMessage,
    TextureLogMessage,
    BSplineLogMessage,
    HashLogMessage,
    LinkerLogMessage,
    InstantiatorLogMessage,
    DataTypeLogMessage,
    NameMapLogMessage,
    MaterialLogMessage,
    ModelLogMessage,
    StackAllocatorLogMessage,
    FixedAllocatorLogMessage,
    SceneLogMessage,
    TrackMaskLogMessage,
    LocalPoseLogMessage,
    WorldPoseLogMessage,
    NameLibraryLogMessage,
    ControlLogMessage,
    MeshBindingLogMessage,
    MathLogMessage,
    VersionLogMessage,
    MemoryLogMessage,
    DeformerLogMessage,
    VoxelLogMessage,
    BitmapLogMessage,
    IKLogMessage,
    CurveLogMessage,
    TrackGroupLogMessage,
    ThreadSafetyLogMessage,
    QuantizeLogMessage,
    BlendDagLogMessage,
    AssertionLogMessage,
    ArenaAllocatorLogMessage,
    FileOperationLogMessage,
    CameraLogMessage,
    TelemetryLogMessage,
    GStateLogMessage,

    // ...add new message sources here.  Make sure to update GetMessageOriginString...

    // Terminator
    OnePastLastMessageOrigin,

    log_message_origin_forceint = 0x7fffffff
};

// Logs will not begin until SetLogFileName is called with a non-0 pointer.
// Logging will terminate once SetLogFileName is called with a 0 pointer.
// The optional "clear" parameter, if set to true, will clear out
// the log file of any current contents.  If false, log entries are
// appended to the file.
// If SetLogFileName discovers that a specified log file cannot be opened
// for writing, it will return false.  Otherwise, it will return true.
EXPAPI GS_MODIFY bool SetLogFileName(char const *FileName, bool Clear);

// Alternatively (or additionally), you can get called back on log
// messages to dump them to your own logfile or display them in a console.
EXPAPI typedef CALLBACK_FN(void) log_function(log_message_type Type,
                                              log_message_origin Origin,
                                              char const* File, int32x Line,
                                              char const *Message,
                                              void *UserData);
EXPTYPE_EPHEMERAL struct log_callback
{
    log_function *Function;
    void *UserData;
};

EXPAPI GS_SPECIAL void GetLogCallback(log_callback* Result);
EXPAPI GS_SPECIAL void SetLogCallback(log_callback const &LogCallback);

EXPAPI GS_SAFE char const* GetLogMessageTypeString(log_message_type Type);
EXPAPI GS_SAFE char const* GetLogMessageOriginString(log_message_origin Origin);

// Logging() returns true if logging is active, false if it is not.
// This can be used to ignore parts of the code that are for logging
// purposes only, to avoid speed hits in the case where logging is
// not needed.
EXPAPI GS_READ bool Logging(void);

#if !GRANNY_NO_LOGGING

#define Log0(T, O, F) _Log(T, O, __FILE__, __LINE__, F)
#define Log1(T, O, F, a) _Log(T, O, __FILE__, __LINE__, F, a)
#define Log2(T, O, F, a, b) _Log(T, O, __FILE__, __LINE__, F, a, b)
#define Log3(T, O, F, a, b, c) _Log(T, O, __FILE__, __LINE__, F, a, b, c)
#define Log4(T, O, F, a, b, c, d) _Log(T, O, __FILE__, __LINE__, F, a, b, c, d)
#define Log5(T, O, F, a, b, c, d, e) _Log(T, O, __FILE__, __LINE__, F, a, b, c, d, e)
#define Log6(T, O, F, a, b, c, d, e, f) _Log(T, O, __FILE__, __LINE__, F, a, b, c, d, e, f)
#define Log7(T, O, F, a, b, c, d, e, f, g) _Log(T, O, __FILE__, __LINE__, F, a, b, c, d, e, f, g)
#define Log8(T, O, F, a, b, c, d, e, f, g, h) _Log(T, O, __FILE__, __LINE__, F, a, b, c, d, e, f, g, h)
void _Log(log_message_type Type, log_message_origin Origin,
          char const* File, int32x Line,
          char const *FormatString, ...);

#define LogNotImplemented(Message) \
    _Log(WarningLogMessage, NotImplementedLogMessage, __FILE__, __LINE__, Message)

#else

#define Log0(T, O, F)
#define Log1(T, O, F, a)
#define Log2(T, O, F, a, b)
#define Log3(T, O, F, a, b, c)
#define Log4(T, O, F, a, b, c, d)
#define Log5(T, O, F, a, b, c, d, e)
#define Log6(T, O, F, a, b, c, d, e, f)
#define Log7(T, O, F, a, b, c, d, e, f, g)
#define Log8(T, O, F, a, b, c, d, e, f, g, h)

#define LogNotImplemented(Message)

#endif

// Filtering is straightforward.
EXPAPI GS_MODIFY void FilterMessage(log_message_origin Origin, bool Enabled);
EXPAPI GS_MODIFY void FilterAllMessages(bool Enabled);


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_LOG_H
#endif
