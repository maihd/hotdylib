/**************************************************************
 * HotDylib - Hot reload dynamic library from memory and file *
 *                                                            *
 **************************************************************/

#pragma once

/**
 * HotDylibState
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

#if defined(_MSC_VER) || (defined(__clang__) && defined(_WIN32))
#   define HOTDYLIB_EXPORT_SPEC __declspec(dllexport)
#elif defined(__GNU__)
#   define HOTDYLIB_EXPORT_SPEC __attribute__((visible("default")))
#else
#   define HOTDYLIB_EXPORT_SPEC
#endif

#if defined(__cplusplus)
#   define HOTDYLIB_EXPORT extern "C" HOTDYLIB_EXPORT_SPEC
#else
#   define HOTDYLIB_EXPORT HOTDYLIB_EXPORT_SPEC
#endif