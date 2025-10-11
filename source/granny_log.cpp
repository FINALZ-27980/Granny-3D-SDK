// ========================================================================
// $File: //jeffr/granny_29/rt/granny_log.cpp $
// $DateTime: 2012/04/27 12:43:03 $
// $Change: 37156 $
// $Revision: #3 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_log.h"

#include "granny_file_operations.h"
#include "granny_file_writer.h"
#include "granny_limits.h"
#include "granny_memory.h"
#include "granny_string.h"
#include "granny_string_formatting.h"
#include "granny_telemetry.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

USING_GRANNY_NAMESPACE;

BEGIN_GRANNY_NAMESPACE;

log_callback LogCallback;

END_GRANNY_NAMESPACE;


#if GRANNY_NO_LOGGING

bool GRANNY
SetLogFileName(char const *FileName, bool Clear)
{
    return false;
}

void GRANNY
GetLogCallback(log_callback* Result)
{
    Result->Function = 0;
    Result->UserData = 0;
}

void GRANNY
SetLogCallback(log_callback const &LogCallbackInit)
{
}

bool GRANNY
Logging(void)
{
    return false;
}

void GRANNY
FilterMessage(log_message_origin Origin, bool Enabled)
{
}

void GRANNY
FilterAllMessages(bool Enabled)
{
}

char const* GRANNY
GetLogMessageTypeString(log_message_type Type)
{
    return "UnknownMessageType";
}

char const* GRANNY
GetLogMessageOriginString(log_message_origin Origin)
{
    return "UnknownMessageOrigin";
}


#else //#if GRANNY_NO_LOGGING



static bool LogIsDisabled[OnePastLastMessageOrigin];
static char LogFileName[MaximumLogFileNameSize] = { 0 };

bool GRANNY
SetLogFileName(char const *FileName, bool Clear)
{
    bool Result = true;

    if (FileName)
    {
        StringEquals(LogFileName, SizeOf(LogFileName), FileName);
    }
    else
    {
        LogFileName[0] = '\0';
    }

    if (*LogFileName)
    {
        file_writer *Writer = NewFileWriter(FileName, Clear);
        if (Writer)
        {
            CloseFileWriter(Writer);
        }
        else
        {
            // Unable to open the file.  Don't try later...
            LogFileName[0] = '\0';
            Result = false;
        }
    }

    return Result;
}


void GRANNY
GetLogCallback(log_callback* Result)
{
    Assert(Result);

    *Result = LogCallback;
}

void GRANNY
SetLogCallback(log_callback const &LogCallbackInit)
{
    LogCallback = LogCallbackInit;
}

bool GRANNY
Logging(void)
{
    return (*LogFileName || LogCallback.Function);
}

static void
WriteMessage(log_message_type Type,
             log_message_origin Origin,
             char const* File, int32x Line,
             int32x LengthToEndOfFile,
             int32x Length,
             char const *Message)
{
    // @@ thread safety

    // Dump out to file if necessary...
    if (*LogFileName)
    {
        static bool AvoidInfiniteRecursion = true;
        if (AvoidInfiniteRecursion)
        {
            AvoidInfiniteRecursion = false;
            AppendStringToFile(LogFileName, Message);
            AvoidInfiniteRecursion = true;
        }
    }

    if(LogCallback.Function)
    {
        LogCallback.Function(Type, Origin, File, Line,
                             Message + LengthToEndOfFile,
                             LogCallback.UserData);
    }
}

void GRANNY
_Log(log_message_type Type, log_message_origin Origin,
     char const* File, int32x Line,
     char const *FormatString, ...)
{
    // @@ thread safety
    Assert(Type < OnePastLastMessageType);
    Assert(Origin < OnePastLastMessageOrigin);

    char TempMessageBuffer[MaximumMessageBufferSize];

    if(Type != IgnoredLogMessage)
    {
        // This message is a regular log message, so we write it
        // directly to the log file.
        if(!LogIsDisabled[Origin] && Logging())
        {
            c_argument_list Args;
            OpenArgumentList(Args, FormatString);

            int32x const LengthToFileMarker =
                ConvertToStringVar(SizeOf(TempMessageBuffer),
                                   TempMessageBuffer,
                                   "%s(%d): ", File, Line);
            int32x const LengthOfMessage =
                ConvertToStringList(SizeOf(TempMessageBuffer) - LengthToFileMarker,
                                    TempMessageBuffer + LengthToFileMarker,
                                    FormatString, Args);
            CloseArgumentList(Args);

            WriteMessage(Type, Origin, File, Line,
                         LengthToFileMarker,
                         LengthToFileMarker + LengthOfMessage,
                         TempMessageBuffer);

#if defined(GRANNY_TELEMETRY) && GRANNY_TELEMETRY
            GRANNY_TM_MESSAGE((Type == ErrorLogMessage ? TMMF_SEVERITY_ERROR :
                               (Type == WarningLogMessage ? TMMF_SEVERITY_WARNING :
                                TMMF_SEVERITY_LOG)),
                              TempMessageBuffer);
#endif
        }
    }
}

void GRANNY
FilterMessage(log_message_origin Origin, bool Enabled)
{
    Assert(Origin < OnePastLastMessageOrigin);
    LogIsDisabled[Origin] = !Enabled;
}

void GRANNY
FilterAllMessages(bool Enabled)
{
    {for(int32x OriginIndex = 0;
         OriginIndex < OnePastLastMessageOrigin;
         ++OriginIndex)
    {
        LogIsDisabled[OriginIndex] = !Enabled;
    }}
}

#define ENUM_STRING_CASE(x) case x ## LogMessage: return #x

char const* GRANNY
GetLogMessageTypeString(log_message_type Type)
{
    switch (Type)
    {
        ENUM_STRING_CASE(Ignored);
        ENUM_STRING_CASE(Note);
        ENUM_STRING_CASE(Warning);
        ENUM_STRING_CASE(Error);

        default:
            return "UnknownMessageType";
    }
}

char const* GRANNY
GetLogMessageOriginString(log_message_origin Origin)
{
    switch (Origin)
    {
        ENUM_STRING_CASE(NotImplemented);
        ENUM_STRING_CASE(Application);
        ENUM_STRING_CASE(Win32Subsystem);
        ENUM_STRING_CASE(Win64Subsystem);
        ENUM_STRING_CASE(WinXXSubsystem);
        ENUM_STRING_CASE(MacOSSubsystem);
        ENUM_STRING_CASE(ANSISubsystem);
        ENUM_STRING_CASE(NACLSubsystem);
        ENUM_STRING_CASE(GamecubeSubsystem);
        ENUM_STRING_CASE(PS2Subsystem);
        ENUM_STRING_CASE(PSPSubsystem);
        ENUM_STRING_CASE(PS3Subsystem);
        ENUM_STRING_CASE(XboxSubsystem);
        ENUM_STRING_CASE(XenonSubsystem);
        ENUM_STRING_CASE(N3DSSubsystem);
        ENUM_STRING_CASE(WiiSubsystem);
        ENUM_STRING_CASE(WiiUSubsystem);
        ENUM_STRING_CASE(SekritSubsystem);
        ENUM_STRING_CASE(MAXSubsystem);
        ENUM_STRING_CASE(MayaSubsystem);
        ENUM_STRING_CASE(XSISubsystem);
        ENUM_STRING_CASE(ModoSubsystem);
        ENUM_STRING_CASE(LightwaveSubsystem);
        ENUM_STRING_CASE(FileWriting);
        ENUM_STRING_CASE(FileReading);
        ENUM_STRING_CASE(Exporter);
        ENUM_STRING_CASE(Compressor);
        ENUM_STRING_CASE(String);
        ENUM_STRING_CASE(StringTable);
        ENUM_STRING_CASE(VertexLayout);
        ENUM_STRING_CASE(Mesh);
        ENUM_STRING_CASE(Property);
        ENUM_STRING_CASE(Skeleton);
        ENUM_STRING_CASE(Animation);
        ENUM_STRING_CASE(SetupGraph);
        ENUM_STRING_CASE(Texture);
        ENUM_STRING_CASE(BSpline);
        ENUM_STRING_CASE(Hash);
        ENUM_STRING_CASE(Linker);
        ENUM_STRING_CASE(Instantiator);
        ENUM_STRING_CASE(DataType);
        ENUM_STRING_CASE(NameMap);
        ENUM_STRING_CASE(Material);
        ENUM_STRING_CASE(Model);
        ENUM_STRING_CASE(StackAllocator);
        ENUM_STRING_CASE(FixedAllocator);
        ENUM_STRING_CASE(Scene);
        ENUM_STRING_CASE(TrackMask);
        ENUM_STRING_CASE(LocalPose);
        ENUM_STRING_CASE(WorldPose);
        ENUM_STRING_CASE(NameLibrary);
        ENUM_STRING_CASE(Control);
        ENUM_STRING_CASE(MeshBinding);
        ENUM_STRING_CASE(Math);
        ENUM_STRING_CASE(Version);
        ENUM_STRING_CASE(Memory);
        ENUM_STRING_CASE(Deformer);
        ENUM_STRING_CASE(Voxel);
        ENUM_STRING_CASE(Bitmap);
        ENUM_STRING_CASE(IK);
        ENUM_STRING_CASE(Curve);
        ENUM_STRING_CASE(TrackGroup);
        ENUM_STRING_CASE(ThreadSafety);
        ENUM_STRING_CASE(Quantize);
        ENUM_STRING_CASE(BlendDag);
        ENUM_STRING_CASE(Assertion);
        ENUM_STRING_CASE(ArenaAllocator);
        ENUM_STRING_CASE(FileOperation);
        ENUM_STRING_CASE(Camera);
        ENUM_STRING_CASE(Telemetry);
        ENUM_STRING_CASE(GState);

        default:
            return "UnknownMessageOrigin";
    }

}

#endif //#else //#if GRANNY_NO_LOGGING

