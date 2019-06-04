/**************************************************************
 * HotDylib - Hot reload dynamic library from memory and file *
 *                                                            *
 **************************************************************/

#pragma once

#include <setjmp.h>

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
    HOTDYLIB_ERROR_FLOAT,
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
    int         state;
    int         errcode;
    void*       userdata;

#ifdef __unix__
    sigjmp_buf  jumpPoint;
#else
    jmp_buf     jumpPoint;
#endif

    char        internal[sizeof(void*)];
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

/* Undocumented, should not call by hand */
HOTDYLIB_API bool   HotDylib_SEHBegin(HotDylib* lib);

/* Undocumented, should not call by hand */
HOTDYLIB_API void   HotDylib_SEHEnd(HotDylib* lib);

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(_WIN32)
#   define HOTDYLIB_TRY(lib)      if (HotDylib_SEHBegin(lib) && _setjmp((lib)->jumpPoint) == 0)
#   define HOTDYLIB_EXCEPT(lib)   else
#   define HOTDYLIB_FINALLY(lib)  HotDylib_SEHEnd(lib); if (1)
#elif (__unix__)
#   include <signal.h>
#   define HOTDYLIB_TRY(lib)      if (HotDylib_SEHBegin(lib) && sigsetjmp((lib)->jumpPoint) == 0)
#   define HOTDYLIB_EXCEPT(lib)   else
#   define HOTDYLIB_FINALLY(lib)  HotDylib_SEHEnd(lib); if (1)
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
