/**************************************************************
 * HotDylib - Hot reload dynamic library from memory and file *
 *                                                            *
 **************************************************************/

#pragma once

#ifndef HOTDYLIB_API
#define HOTDYLIB_API
#endif

#if defined(__cplusplus)
#   extern "C" {
#elif !defined(__bool_true_false_are_defined)
typedef unsigned char bool;
enum { true = 1, false = 0 };
#endif

/**
 * CSFX lib state
 */
enum
{
    HOTDYLIB_NONE,
    HOTDYLIB_INIT,
    HOTDYLIB_QUIT,
    HOTDYLIB_UNLOAD,
    HOTDYLIB_RELOAD,
    HOTDYLIB_FAILED,
};

/**
 * CSFX lib error code
 */
enum
{
    HOTDYLIB_ERROR_NONE,
    HOTDYLIB_ERROR_ABORT,
    HOTDYLIB_ERROR_ILLCODE,
    HOTDYLIB_ERROR_SYSCALL,
    HOTDYLIB_ERROR_MISALIGN,
    HOTDYLIB_ERROR_SEGFAULT,
    HOTDYLIB_ERROR_OUTBOUNDS,
    HOTDYLIB_ERROR_STACKOVERFLOW,
};

/** 
 * Script data structure
 */
typedef struct HotDylib
{
    int   state;
    int   errcode;
    void* userdata;
    char  internal[sizeof(void*)];
} HotDylib;

/**
 * File data structure
 */
typedef struct
{
    long        time;
    const char* path;
} HotDylibFileTime;

/** Hot reload library API **/

HOTDYLIB_API bool  HotDylib_Begin(void);
HOTDYLIB_API void  HotDylib_End(void);

/**
 * Initialize lib
 */
HOTDYLIB_API bool  HotDylibInit(HotDylib* lib, const char* path);

/**
 * Free memory usage by lib, unload library and raise quit event
 */
HOTDYLIB_API void  HotDylibFree(HotDylib* lib);

/**
 * Update lib, check for changed library and reload
 */
HOTDYLIB_API int   HotDylibUpdate(HotDylib* lib);

/**
 * Get an symbol address from lib
 */
HOTDYLIB_API void* HotDylibGetSymbol(HotDylib* lib, const char* name);

/**
 * Get error message of lib
 */
HOTDYLIB_API const char* HotDylibGetError(const HotDylib* lib);

#if defined(_WIN32)
/* Undocumented, should not call by hand */
HOTDYLIB_API int HotDylib_SehFilter(HotDylib* lib, unsigned long code);
#endif

#if defined(_MSC_VER)
# define HOTDYLIB_TRY(s)    HotDylib_Begin(); __try
# define HOTDYLIB_EXCEPT(s) __except(HotDylib_SehFilter(s, GetExceptionCode()))
# define HOTDYLIB_FINALLY   __finally HotDylib_End(); if (true)
#elif (__unix__)
# include <signal.h>
# include <setjmp.h>
# define HOTDYLIB_TRY(s)							\
    (s)->errcode = sigsetjmp(csfx__jmpenv, 0);				\
    if ((s)->errcode == 0) (s)->errcode = HOTDYLIB_ERROR_NONE;		\
    if ((s)->errcode == HOTDYLIB_ERROR_NONE)

# define HOTDYLIB_EXCEPT(s) else if (csfx__errcode_filter(s))
# define HOTDYLIB_FINALLY   
# define csfx__errcode_filter(s)					\
    (s)->errcode > HOTDYLIB_ERROR_NONE					\
    && (s)->errcode <= HOTDYLIB_ERROR_STACKOVERFLOW

extern __thread sigjmp_buf csfx__jmpenv;
#else
# include <signal.h>
# include <setjmp.h>
# define HOTDYLIB_TRY(s)						\
    (s)->errcode = setjmp(csfx__jmpenv);			\
    if ((s)->errcode == 0) (s)->errcode = HOTDYLIB_ERROR_NONE;	\
    if ((s)->errcode == HOTDYLIB_ERROR_NONE)

# define HOTDYLIB_EXCEPT(s) else if (csfx__errcode_filter(s))
# define HOTDYLIB_FINALLY   
# define csfx__errcode_filter(s)					\
    (s)->errcode > HOTDYLIB_ERROR_NONE					\
    && (s)->errcode <= HOTDYLIB_ERROR_STACKOVERFLOW

extern
# if defined(__MINGW32__)
__thread
# endif
jmp_buf csfx__jmpenv;
#endif

/**
 * Watch for files or directories is changed
 * @note: if change the directory after received a changed event
 *        ensure call this again to update time to ignore change
 * @example: 
 *        HotDylibFileTime dir = { 0, "<dirpath>" };
 *        HotDylibWatchFiles(&dir, 1); // Initialize
 *        ...
 *        if (HotDylibWatchFiles(&dir, 1))
 *        {
 *            ... some operations on <dirpath>
 *            HotDylibWatchFiles(&dir, 1); // Ignore change
 *        }
 */
HOTDYLIB_API bool HotDylibWatchFiles(HotDylibFileTime* files, int count);

/**
 * Script main function
 */
#if defined(_WIN32) || defined(__CYGWIN__)
__declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
__attribute((visible("default")))
#else
extern
#endif
void* HotDylibMain(void* userdata, int old_state, int new_state);

/* END OF EXTERN "C" */
#ifdef __cplusplus
};
#endif
