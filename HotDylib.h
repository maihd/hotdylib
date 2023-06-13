/**************************************************************
 * HotDylib - Hot reload dynamic library from memory and file *
 *                                                            *
 **************************************************************/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifndef HOTDYLIB_API
#define HOTDYLIB_API
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * HotDylibState
 */
typedef enum HotDylibState
{
    HOTDYLIB_NONE,
    HOTDYLIB_INIT,
    HOTDYLIB_QUIT,
    HOTDYLIB_UNLOAD,
    HOTDYLIB_RELOAD,
    HOTDYLIB_LOCKED,
    HOTDYLIB_FAILED,
} HotDylibState;

/**
 * HotDylib error code
 */
typedef enum HotDylibError
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
} HotDylibError;

/** 
 * HotDylib
 */
typedef struct HotDylib
{
    HotDylibState   state;
    HotDylibError   error;
    void*           userdata;
    char            entryName[256];
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
HOTDYLIB_API HotDylibState  HotDylibUpdate(HotDylib* lib);

// Check if guest library ready to rebuild
//HOTDYLIB_API bool           HotDylibUnlocked(HotDylib* lib);

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
