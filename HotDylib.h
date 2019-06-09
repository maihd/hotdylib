/**************************************************************
 * HotDylib - Hot reload dynamic library from memory and file *
 *                                                            *
 **************************************************************/

#pragma once

#ifndef HOTDYLIB_API
#define HOTDYLIB_API
#endif

#if defined(__cplusplus)
extern "C" {
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
    int     state;
    int     errcode;
    void*   userdata;
    char    entryName[256];
} HotDylib;

/**
 * Open an hot dynamic library, path can be not exists from open moment
 */
HOTDYLIB_API HotDylib*      HotDylibOpen(const char* path, const char* entryName);

/**
 * Free usage memory and close opened files by hot dynamic library
 */
HOTDYLIB_API void           HotDylibFree(HotDylib* lib);

/**
 * Update lib, check for changed library and reload
 */
HOTDYLIB_API int            HotDylibUpdate(HotDylib* lib);

/**
 * Get an symbol address from library with symbol's name
 */
HOTDYLIB_API void*          HotDylibGetSymbol(const HotDylib* lib, const char* symbolName);

/**
 * Get error message of hot dynamic library from last update
 */
HOTDYLIB_API const char*    HotDylibGetError(const HotDylib* lib);

/* END OF EXTERN "C" */
#ifdef __cplusplus
};
#endif
