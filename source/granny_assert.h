#if !defined(GRANNY_ASSERT_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_assert.h $
// $DateTime: 2012/04/24 15:33:15 $
// $Change: 37118 $
// $Revision: #2 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

/* ========================================================================
   Explicit Dependencies
   ======================================================================== */
#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

#include "rrCore.h"

// Handy place to do some checks.

// _DEBUG is defined if the compiler is working in debug mode,
// i.e. with no optimisations. But even in an optimised build,
// we might want asserts and suchlike.
#if defined(_DEBUG) && (_DEBUG==0)
#error Either define _DEBUG, or dont define it. Never set it to 0.
#endif

// DEBUG is the one that says "I want asserts". You might still
// want asserts and checks in an optimised build for example.
#if defined(DEBUG) && (DEBUG!=1)
#error Either define DEBUG to 1, or dont define it. Never set it to 0.
#endif

#if PLATFORM_WINXX
   #define IGNORE_STATIC static
#else
   #define IGNORE_STATIC
#endif


BEGIN_GRANNY_NAMESPACE;

typedef bool assertion_display_function(char const * const Expression,
                                        char const * const File,
                                        int32x const LineNumber,
                                        char const * const Function,
                                        bool* IgnoreAssertion,
                                        void* UserData);
struct assertion_callback
{
    assertion_display_function *Function;
    void *UserData;
};

bool DisplayAssertion(char const * const Expression,
                      char const * const File,
                      int32x const LineNumber,
                      char const * const Function,
                      bool* IgnoreAssertion,
                      void* UserData);

#ifdef GrannyAssertForward  // overidden assert

#undef Assert
#define Assert(exp) GrannyAssertForward(exp)

#else // use default granny assert

#if DEBUG
    #undef Assert
    #undef WhenAssert

#define Assert(Expression)                                                                          \
    {                                                                                               \
        USING_GRANNY_NAMESPACE;                                                                     \
        IGNORE_STATIC bool IgnoreAssert = false;                                                    \
        if (!IgnoreAssert && !(Expression))                                                         \
        {                                                                                           \
            assertion_callback AssertionCallback;                                                   \
            GetAssertionCallback(&AssertionCallback);                                               \
            if (AssertionCallback.Function)                                                         \
            {                                                                                       \
                bool ShouldBreak = AssertionCallback.Function(#Expression, __FILE__, __LINE__,      \
                                                              "Unknown", &IgnoreAssert, AssertionCallback.UserData); \
                if (ShouldBreak)                                                                    \
                {                                                                                   \
                    RR_BREAK();                                                                     \
                }                                                                                   \
            }                                                                                       \
        }                                                                                           \
        __analysis_assume(Expression);                                                              \
    } typedef int gr_AssertionsRequireSemiColons

    #define WhenAssert(x) x

#else
   #undef Assert
   #undef WhenAssert

   // Check for release mode only override
   #ifdef GrannyReleaseAssertForward
      #define Assert(Expression) GrannyReleaseAssertForward(Expression)
      #define WhenAssert(x) x
   #else // GrannyReleaseAssertForward
      #define Assert(Expression) ((void)sizeof(Expression))
      #define WhenAssert(x)
   #endif // GrannyReleaseAssertForward

#endif // DEBUG

#endif // GrannyAssertForward

// Compile assertions are always turned on
#define CompileAssert(Expression) typedef int RR_NUMBERNAME(CompileAssert_Type_) [(Expression) ? 1 : -1]
#define DelayJoinStr(x, y) x ## y

#define InvalidCodePath(Text) Assert(!Text)
#define NotImplemented InvalidCodePath("NotImplemented")

void GetAssertionCallback(assertion_callback* Result);
void SetAssertionCallback(assertion_callback const &CallbackInit);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_ASSERT_H
#endif
