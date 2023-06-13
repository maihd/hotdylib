/**************************************************************
 * HotDylib - Hot reload dynamic library from memory and file *
 *                                                            *
 **************************************************************/

#define _CRT_SECURE_NO_WARNINGS

#ifndef HOTDYLIB_PDB_UNLOCK
#define HOTDYLIB_PDB_UNLOCK 1
#endif

#define HOTDYLIB_MAX_PATH   256
#define HotDylib_CountOf(x) (sizeof(x) / sizeof((x)[0]))

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>
#include <signal.h>

#include "HotDylib.h"

#if defined(__MINGW32__) || (defined(_WIN32) && defined(__clang__))
#   define HOTDYLIB_TRY(lib)      if (HotDylib_SEHBegin(lib))
#   define HOTDYLIB_EXCEPT(lib)   else
#   define HOTDYLIB_FINALLY(lib)  HotDylib_SEHEnd(lib); if (1)
#elif (__unix__) || defined(__linux__) || defined(__APPLE__)
#   define HOTDYLIB_TRY(lib)      if (HotDylib_SEHBegin(lib) && sigsetjmp((lib)->jumpPoint) == 0)
#   define HOTDYLIB_EXCEPT(lib)   else
#   define HOTDYLIB_FINALLY(lib)  HotDylib_SEHEnd(lib); if (1)
#endif

 /* Undocumented, should not call by hand */
HOTDYLIB_API bool   HotDylib_SEHBegin(HotDylib* lib);

/* Undocumented, should not call by hand */
HOTDYLIB_API void   HotDylib_SEHEnd(HotDylib* lib);

typedef struct
{
    void* library;

    long  libTime;
    char  libRealPath[HOTDYLIB_MAX_PATH];
    char  libTempPath[HOTDYLIB_MAX_PATH];

#if defined(_MSC_VER) && HOTDYLIB_PDB_UNLOCK
    long  pdbTime;

    char  pdbRealPath[HOTDYLIB_MAX_PATH];
    char  pdbTempPath[HOTDYLIB_MAX_PATH];
#endif
} HotDylibData;


/* Define dynamic library loading API */
#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>

#   define Dylib_Load(path)         (void*)LoadLibraryA(path)
#   define Dylib_Free(lib)          FreeLibrary((HMODULE)lib)
#   define Dylib_GetSymbol(l, n)    (void*)GetProcAddress((HMODULE)l, n)

static const char* Dylib_GetError(void)
{
    static int  error;
    static char buffer[256];

    int last_error = GetLastError();
    if (last_error != error)
    {
        error = last_error;
        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, last_error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            buffer, sizeof(buffer), NULL);
    }
    return buffer;
}
#elif (__unix__)
#  include <dlfcn.h>
#  define Dylib_Load(path)          dlopen(path, RTLD_LAZY)
#  define Dylib_Free(lib)           dlclose(lib)
#  define Dylib_GetSymbol(l, n)     dlsym(l, n)
#  define Dylib_GetError()          dlerror()
#else
#  error "Unsupported platform"
#endif


/** Custom helper functions **/
#if defined(_WIN32)

static long HotDylib_FileTimeToLong(FILETIME fileTime)
{
    LARGE_INTEGER time;
    time.LowPart = fileTime.dwLowDateTime;
    time.HighPart = fileTime.dwHighDateTime;

    return (long)(time.QuadPart / 10000000L - 11644473600L);
}

static long HotDylib_GetLastModifyTime(const char* path)
{
    FILETIME modTime;

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad))
    {
        HANDLE file = CreateFileA(path, GENERIC_READ, 0, 0, 0, FILE_ATTRIBUTE_NORMAL, INVALID_HANDLE_VALUE);
        if (file != INVALID_HANDLE_VALUE)
        {
            if (!GetFileTime(file, NULL, NULL, &modTime))
            {
                return 0;
            }

            CloseHandle(file);
        }
    }
    else
    {
        modTime = fad.ftLastWriteTime;
    }

    return HotDylib_FileTimeToLong(modTime);
}

static bool HotDylib_CopyFile(const char* from, const char* to)
{
    if (CopyFileA(from, to, false))
    {
        return true;
    }
    else
    {
        return false;
    }
}

static bool HotDylib_RemoveFile(const char* path);

#if defined(_MSC_VER)

#  if defined(HOTDYLIB_PDB_UNLOCK) || defined(HOTDYLIB_PDB_DELETE)
#    include <winternl.h>
#    include <RestartManager.h> 
#    pragma comment(lib, "ntdll.lib")
#    pragma comment(lib, "rstrtmgr.lib")

#    define NTSTATUS_SUCCESS              ((NTSTATUS)0x00000000L)
#    define NTSTATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xc0000004L)

typedef struct
{
    UNICODE_STRING Name;
} OBJECT_INFORMATION;

static void HotDylib_UnlockFileFromProcess(ULONG pid, const WCHAR* file)
{
    const OBJECT_INFORMATION_CLASS ObjectNameInformation = (OBJECT_INFORMATION_CLASS)1;
    const OBJECT_INFORMATION_CLASS ObjectTypeInformation = (OBJECT_INFORMATION_CLASS)2;

    // Make sure the process is valid
    HANDLE hCurProcess = GetCurrentProcess();
    HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProcess)
    {
        return;
    }

    WCHAR volumeName[8];
    GetVolumePathNameW(file, volumeName, sizeof(volumeName));

    volumeName[2] = 0;  /* Remove '\\' */
    WCHAR prefix[1024];
    QueryDosDeviceW(volumeName, prefix, sizeof(prefix));
    wcscat(prefix, L"\\");
    size_t prefixLength = wcslen(prefix);

    DWORD handleIter, handleCount;
    GetProcessHandleCount(hProcess, &handleCount);
    for (handleIter = 0, handleCount *= 16; handleIter <= handleCount; handleIter += 4)
    {
        HANDLE handle = (HANDLE)handleIter;

        HANDLE hCopy; // Duplicate the handle in the current process
        if (!DuplicateHandle(hProcess, handle, hCurProcess, &hCopy, 0, FALSE, DUPLICATE_SAME_ACCESS))
        {
            continue;
        }

        const char ObjectBuffer[sizeof(OBJECT_INFORMATION) + 512] = { 0 };
        OBJECT_INFORMATION* pobj = (OBJECT_INFORMATION*)ObjectBuffer;

        if (NtQueryObject(hCopy, ObjectNameInformation, pobj, sizeof(ObjectBuffer), NULL) != NTSTATUS_SUCCESS)
        {
            CloseHandle(hCopy);
            continue;
        }

        if (!pobj->Name.Buffer)
        {
            CloseHandle(hCopy);
            continue;
        }

        if (wcsncmp(pobj->Name.Buffer, prefix, prefixLength) == 0)
        {
            WCHAR path0[HOTDYLIB_MAX_PATH];
            WCHAR path1[HOTDYLIB_MAX_PATH];

            swscanf(pobj->Name.Buffer + prefixLength, L"%s", path0);

            wsprintfW(path1, L"%s\\%s", volumeName, path0);

            if (wcscmp(path1, file) == 0)
            {
                HANDLE hForClose;
                DuplicateHandle(hProcess, handle, hCurProcess, &hForClose, MAXIMUM_ALLOWED, false, DUPLICATE_CLOSE_SOURCE);
                CloseHandle(hForClose);
                CloseHandle(hCopy);
                break;
            }
        }

        CloseHandle(hCopy);
    }

    CloseHandle(hProcess);
}

static void HotDylib_UnlockPdbFile(HotDylibData* lib, const char* file)
{
    WCHAR           szFile[HOTDYLIB_MAX_PATH + 1];
    UINT            i;
    UINT            nProcInfoNeeded;
    UINT            nProcInfo = 100;
    DWORD           dwError;
    DWORD           dwReason;
    DWORD           dwSession;
    RM_PROCESS_INFO rgpi[100];
    WCHAR           szSessionKey[CCH_RM_SESSION_KEY + 1] = { 0 };

    MultiByteToWideChar(CP_UTF8, 0, file, -1, szFile, HOTDYLIB_MAX_PATH);

    dwError = RmStartSession(&dwSession, 0, szSessionKey);
    if (dwError == ERROR_SUCCESS)
    {
        const WCHAR* szFiles = szFile;

        dwError = RmRegisterResources(dwSession, 1, &szFiles, 0, NULL, 0, NULL);
        if (dwError == ERROR_SUCCESS)
        {
            dwError = RmGetList(dwSession, &nProcInfoNeeded, &nProcInfo, rgpi, &dwReason);
            if (dwError == ERROR_SUCCESS)
            {
                for (i = 0; i < nProcInfo; i++)
                {
                    HotDylib_UnlockFileFromProcess(rgpi[i].Process.dwProcessId, szFile);
                }
            }
        }
        RmEndSession(dwSession);
    }
}

static bool HotDylib_IsFileLockedFromProcess(ULONG pid, const WCHAR* file)
{
    const OBJECT_INFORMATION_CLASS ObjectNameInformation = (OBJECT_INFORMATION_CLASS)1;
    const OBJECT_INFORMATION_CLASS ObjectTypeInformation = (OBJECT_INFORMATION_CLASS)2;

    // Make sure the process is valid
    HANDLE hCurProcess = GetCurrentProcess();
    HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProcess)
    {
        return false;
    }

    WCHAR volumeName[8];
    GetVolumePathNameW(file, volumeName, sizeof(volumeName));

    volumeName[2] = 0;  /* Remove '\\' */
    WCHAR prefix[1024];
    QueryDosDeviceW(volumeName, prefix, sizeof(prefix));
    wcscat(prefix, L"\\");
    size_t prefixLength = wcslen(prefix);

    DWORD handleIter, handleCount;
    GetProcessHandleCount(hProcess, &handleCount);
    for (handleIter = 0, handleCount *= 16; handleIter <= handleCount; handleIter += 4)
    {
        HANDLE handle = (HANDLE)handleIter;

        HANDLE hCopy; // Duplicate the handle in the current process
        if (!DuplicateHandle(hProcess, handle, hCurProcess, &hCopy, 0, FALSE, DUPLICATE_SAME_ACCESS))
        {
            continue;
        }

        const char ObjectBuffer[sizeof(OBJECT_INFORMATION) + 512] = { 0 };
        OBJECT_INFORMATION* pobj = (OBJECT_INFORMATION*)ObjectBuffer;

        if (NtQueryObject(hCopy, ObjectNameInformation, pobj, sizeof(ObjectBuffer), NULL) != NTSTATUS_SUCCESS)
        {
            CloseHandle(hCopy);
            continue;
        }

        if (!pobj->Name.Buffer)
        {
            CloseHandle(hCopy);
            continue;
        }

        if (wcsncmp(pobj->Name.Buffer, prefix, prefixLength) == 0)
        {
            WCHAR path0[HOTDYLIB_MAX_PATH];
            WCHAR path1[HOTDYLIB_MAX_PATH];

            swscanf(pobj->Name.Buffer + prefixLength, L"%s", path0);

            wsprintfW(path1, L"%s\\%s", volumeName, path0);

            if (wcscmp(path1, file) == 0)
            {
                CloseHandle(hCopy);
                return true;
            }
        }

        CloseHandle(hCopy);
    }

    CloseHandle(hProcess);
    return false;
}

static bool HotDylib_IsFileLocked(const char* file)
{
    WCHAR           szFile[HOTDYLIB_MAX_PATH + 1];
    UINT            i;
    UINT            nProcInfoNeeded;
    UINT            nProcInfo = 10;
    DWORD           dwError;
    DWORD           dwReason;
    DWORD           dwSession;
    RM_PROCESS_INFO rgpi[10];
    WCHAR           szSessionKey[CCH_RM_SESSION_KEY + 1] = { 0 };

    MultiByteToWideChar(CP_UTF8, 0, file, -1, szFile, HOTDYLIB_MAX_PATH);

    dwError = RmStartSession(&dwSession, 0, szSessionKey);
    if (dwError == ERROR_SUCCESS)
    {
        const WCHAR* szFiles = szFile;

        dwError = RmRegisterResources(dwSession, 1, &szFiles, 0, NULL, 0, NULL);
        if (dwError == ERROR_SUCCESS)
        {
            dwError = RmGetList(dwSession, &nProcInfoNeeded, &nProcInfo, rgpi, &dwReason);
            if (dwError == ERROR_SUCCESS)
            {
                for (i = 0; i < nProcInfo; i++)
                {
                    if (HotDylib_IsFileLockedFromProcess(rgpi[i].Process.dwProcessId, szFile))
                    {
                        return true;
                    }
                }
            }
        }
        RmEndSession(dwSession);
    }

    return false;
}

static int HotDylib_GetPdbPath(const char* libpath, char* buf, int len)
{
    char drv[8];
    char dir[HOTDYLIB_MAX_PATH];
    char name[HOTDYLIB_MAX_PATH];

    GetFullPathNameA(libpath, len, buf, NULL);
    _splitpath(buf, drv, dir, name, NULL);

    return snprintf(buf, len, "%s%s%s.pdb", drv, dir, name);
}
#  endif /* HOTDYLIB_PDB_UNLOCK */

#if HOTDYLIB_USE_SEH
static int HotDylib_SEHFilter(HotDylib* lib, int exception)
{
    HotDylibError error = HOTDYLIB_ERROR_NONE;

    switch (exception)
    {
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_INVALID_OPERATION:
        error = HOTDYLIB_ERROR_FLOAT;
        break;

    case EXCEPTION_ACCESS_VIOLATION:
        error = HOTDYLIB_ERROR_SEGFAULT;
        break;

    case EXCEPTION_ILLEGAL_INSTRUCTION:
        error = HOTDYLIB_ERROR_ILLCODE;
        break;

    case EXCEPTION_DATATYPE_MISALIGNMENT:
        error = HOTDYLIB_ERROR_MISALIGN;
        break;

    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        error = HOTDYLIB_ERROR_OUTBOUNDS;
        break;

    case EXCEPTION_STACK_OVERFLOW:
        error = HOTDYLIB_ERROR_STACKOVERFLOW;
        break;

    default:
        break;
    }

    if (lib) lib->error = error;
    int rc = error != HOTDYLIB_ERROR_NONE;
    return rc;
}
#else
typedef struct SehFilter
{
    int                             ref;
    HotDylib*                       lib;
    LPTOP_LEVEL_EXCEPTION_FILTER    oldHandler;
} SehFilter;

static SehFilter    s_filterStack[128];
static int          s_filterStackPointer = -1;

/* Undocumented, should not call by hand */
static HotDylibError HotDylib_SEHFilter(HotDylib* lib, int exception)
{
    HotDylibError error = HOTDYLIB_ERROR_NONE;

    switch (exception)
    {
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_INVALID_OPERATION:
        error = HOTDYLIB_ERROR_FLOAT;
        break;

    case EXCEPTION_ACCESS_VIOLATION:
        error = HOTDYLIB_ERROR_SEGFAULT;
        break;

    case EXCEPTION_ILLEGAL_INSTRUCTION:
        error = HOTDYLIB_ERROR_ILLCODE;
        break;

    case EXCEPTION_DATATYPE_MISALIGNMENT:
        error = HOTDYLIB_ERROR_MISALIGN;
        break;

    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        error = HOTDYLIB_ERROR_OUTBOUNDS;
        break;

    case EXCEPTION_STACK_OVERFLOW:
        error = HOTDYLIB_ERROR_STACKOVERFLOW;
        break;

    default:
        break;
    }

    if (lib) lib->error = error;
    return error;
}

static LONG WINAPI HotDylib_SignalHandler(EXCEPTION_POINTERS* info)
{
    assert(s_filterStackPointer > -1);

    DWORD           exception = info->ExceptionRecord->ExceptionCode;
    HotDylibError   error = HotDylib_SEHFilter(NULL, exception);

    return error != HOTDYLIB_ERROR_NONE;
}

bool HotDylib_SEHBegin(HotDylib* lib)
{
    assert(lib);

    if (s_filterStack[s_filterStackPointer].lib == lib)
    {
        s_filterStack[s_filterStackPointer].ref++;
    }
    else
    {
        assert(s_filterStackPointer < (int)HotDylib_CountOf(s_filterStack));

        SehFilter* filter = &s_filterStack[++s_filterStackPointer];
        filter->ref = 0;
        filter->lib = lib;
        filter->oldHandler = SetUnhandledExceptionFilter(HotDylib_SignalHandler);
    }

    return true;
}

void HotDylib_SEHEnd(HotDylib* lib)
{
    assert(lib);
    assert(s_filterStackPointer > -1 && s_filterStack[s_filterStackPointer].lib == lib);

    SehFilter* filter = &s_filterStack[s_filterStackPointer];
    if (--filter->ref <= 0)
    {
        s_filterStackPointer--;
        SetUnhandledExceptionFilter(filter->oldHandler);
    }
}

// Check if guest library ready to rebuild
bool HotDylibUnlocked(HotDylib* lib)
{
    HotDylibData* data = (HotDylibData*)(lib - 1);
    return HotDylib_IsFileLocked(data->pdbRealPath);
}
#endif

# else
typedef struct SehFilter
{
    int                             ref;
    HotDylib*                       lib;
    LPTOP_LEVEL_EXCEPTION_FILTER    oldHandler;
} SehFilter;

static SehFilter    s_filterStack[128];
static int          s_filterStackPointer = -1;

/* Undocumented, should not call by hand */
static HotDylibError HotDylib_SEHFilter(HotDylib* lib, int exception)
{
    HotDylibError error = HOTDYLIB_ERROR_NONE;

    switch (exception)
    {
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_INVALID_OPERATION:
        error = HOTDYLIB_ERROR_FLOAT;
        break;

    case EXCEPTION_ACCESS_VIOLATION:
        error = HOTDYLIB_ERROR_SEGFAULT;
        break;

    case EXCEPTION_ILLEGAL_INSTRUCTION:
        error = HOTDYLIB_ERROR_ILLCODE;
        break;

    case EXCEPTION_DATATYPE_MISALIGNMENT:
        error = HOTDYLIB_ERROR_MISALIGN;
        break;

    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        error = HOTDYLIB_ERROR_OUTBOUNDS;
        break;

    case EXCEPTION_STACK_OVERFLOW:
        error = HOTDYLIB_ERROR_STACKOVERFLOW;
        break;

    default:
        break;
    }

    if (lib) lib->error = error;
    return error;
}

static LONG WINAPI HotDylib_SignalHandler(EXCEPTION_POINTERS* info)
{
    assert(s_filterStackPointer > -1);

    DWORD           exception = info->ExceptionRecord->ExceptionCode;
    HotDylibError   error = HotDylib_SEHFilter(NULL, exception);

    return error != HOTDYLIB_ERROR_NONE;
}

bool HotDylib_SEHBegin(HotDylib* lib)
{
    assert(lib);

    if (s_filterStack[s_filterStackPointer].lib == lib)
    {
        s_filterStack[s_filterStackPointer].ref++;
    }
    else
    {
        assert(s_filterStackPointer < (int)HotDylib_CountOf(s_filterStack));

        SehFilter* filter = &s_filterStack[++s_filterStackPointer];
        filter->ref = 0;
        filter->lib = lib;
        filter->oldHandler = SetUnhandledExceptionFilter(HotDylib_SignalHandler);
    }

    return true;
}

void HotDylib_SEHEnd(HotDylib* lib)
{
    assert(lib);
    assert(s_filterStackPointer > -1 && s_filterStack[s_filterStackPointer].lib == lib);

    SehFilter* filter = &s_filterStack[s_filterStackPointer];
    if (--filter->ref <= 0)
    {
        s_filterStackPointer--;
        SetUnhandledExceptionFilter(filter->oldHandler);
    }
}

// Check if guest library ready to rebuild
bool HotDylibUnlocked(HotDylib* lib)
{
    // HotDylibData* data = (HotDylibData*)(lib - 1);
    // return HotDylib_IsFileLocked(data->pdbRealPath);
    return false;
}
# endif /* _MSC_VER */

#elif defined(__unix__)
#   include <sys/stat.h>
#   include <sys/types.h>

const int HotDylib_Signals[] = { SIGBUS, SIGSYS, SIGILL, SIGSEGV, SIGABRT };

static long HotDylib_GetLastModifyTime(const char* path)
{
    struct stat st;
    if (stat(path, &st) != 0)
    {
        return 0;
    }

    return (long)st.st_mtime;
}

static int HotDylib_CopyFile(const char* from_path, const char* to_path)
{
    char scmd[3 * PATH_MAX]; /* 2 path and command */
    sprintf(scmd, "cp -fR %s %s", from_path, to_path);
    if (system(scmd) != 0)
    {
        /* Has an error occur */
        return 0;
    }
    else
    {
        return 1;
    }
}

static void HotDylib_SignalHandler(int code, siginfo_t* info, void* context)
{
    assert(s_filterStackPointer > -1);

    int errcode;

    (void)info;
    (void)context;

    switch (code)
    {
    case SIGILL:
        errcode = HOTDYLIB_ERROR_ILLCODE;
        break;

    case SIGBUS:
        errcode = HOTDYLIB_ERROR_MISALIGN;
        break;

    case SIGSYS:
        errcode = HOTDYLIB_ERROR_SYSCALL;
        break;

    case SIGABRT:
        errcode = HOTDYLIB_ERROR_ABORT;
        break;

    case SIGSEGV:
        errcode = HOTDYLIB_ERROR_SEGFAULT;
        break;

    default:
        errcode = HOTDYLIB_ERROR_NONE;
        break;
    }

    siglongjmp(s_filterStack[s_filterStackPointer].lib->jumpPoint, errcode);
}


bool HotDylib_SEHBegin(HotDylib* lib)
{
    if (s_filterStackPointer < 0)
    {
        struct sigaction sa;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
        sa.sa_handler = NULL;
        sa.sa_sigaction = HotDylib_SignalHandler;

        int idx;
        for (idx = 0; idx < HotDylib_CountOf(HotDylib_Signals); idx++)
        {
            if (sigaction(HotDylib_Signals[idx], &sa, NULL) != 0)
            {
                return false;
            }
        }

        SehFilter* filter = &s_filterStack[++s_filterStackPointer];
        filter->lib = lib;
    }
    return true;
}

void HotDylib_SEHEnd(HotDylib* lib)
{
    int idx;
    for (idx = 0; idx < HotDylib_CountOf(HotDylib_Signals); idx++)
    {
        if (signal(HotDylib_Signals[idx], SIG_DFL) != 0)
        {
            assert(false);
        }
    }
}

// Check if guest library ready to rebuild
bool HotDylibUnlocked(HotDylib* lib)
{
    return true;
}

#else
#error "Unsupported platform"
#endif

static bool HotDylib_RemoveFile(const char* path)
{
    return DeleteFileA(path);
}

static int HotDylib_GetTempPath(const char* path, char* buffer, int length)
{
    int res;

    if (buffer)
    {
        int version = 0;
        while (1)
        {
            res = snprintf(buffer, length, "%s.%d", path, version++);
            FILE* file = fopen(buffer, "r");
            if (file)
            {
                fclose(file);
            }
            else
            {
                break;
            }
        }
    }

    return res;
}

static bool HotDylib_CheckChanged(HotDylib* lib)
{
    HotDylibData* data = (HotDylibData*)(lib + 1);

    long src = data->libTime;
    long cur = HotDylib_GetLastModifyTime(data->libRealPath);
    bool res = cur > src;
#if defined(_MSC_VER) && HOTDYLIB_PDB_UNLOCK
    if (res)
    {
        src = data->pdbTime;
        cur = HotDylib_GetLastModifyTime(data->pdbRealPath);
        res = (cur == src && cur == 0) || cur > src;
    }
#endif
    return res;
}

static bool HotDylib_CallMain(HotDylib* lib, void* library, HotDylibState newState)
{
    typedef void* (*HotDylibMainFn)(void* userdata, HotDylibState newState, HotDylibState oldState);
    HotDylibMainFn func = (HotDylibMainFn)Dylib_GetSymbol(library, lib->entryName);

    bool res = true;
    if (func)
    {
#if defined(_MSC_VER)
        __try
        {
            lib->userdata = func(lib->userdata, newState, lib->state);
        }
        __except (HotDylib_SEHFilter(lib, GetExceptionCode()))
        {
            res = false;
        }
#else
        HOTDYLIB_TRY(lib)
        {
            lib->userdata = func(lib->userdata, lib->state, newState);
        }
        HOTDYLIB_EXCEPT(lib)
        {
            res = false;
        }
        HOTDYLIB_FINALLY(lib)
        {
            /*null statement*/
        }
#endif
    }

    return res;
}

/* @impl: HotDylibOpen */
HotDylib* HotDylibOpen(const char* path, const char* entryName)
{
    HotDylib* lib = (HotDylib*)(malloc(sizeof(HotDylib) + sizeof(HotDylibData)));
    if (!lib)
    {
        return NULL;
    }

    lib->state = HOTDYLIB_NONE;
    lib->error = HOTDYLIB_ERROR_NONE;
    lib->userdata = NULL;

    if (entryName)
    {
        strcpy(lib->entryName, entryName);
    }
    else
    {
        lib->entryName[0] = 0;
    }

    HotDylibData* data = (HotDylibData*)(lib + 1);
    data->libTime = 0;
    data->library = NULL;
    HotDylib_GetTempPath(path, data->libTempPath, HOTDYLIB_MAX_PATH);

    strncpy(data->libRealPath, path, HOTDYLIB_MAX_PATH);

#if defined(_MSC_VER) && HOTDYLIB_PDB_UNLOCK
    data->pdbTime = 0;
    HotDylib_GetPdbPath(path, data->pdbRealPath, HOTDYLIB_MAX_PATH);
    HotDylib_GetTempPath(data->pdbRealPath, data->pdbTempPath, HOTDYLIB_MAX_PATH);
#endif

    return lib;
}

void HotDylibFree(HotDylib* lib)
{
    if (lib)
    {
        HotDylibData* data = (HotDylibData*)(lib + 1);
        /* Raise quit event */
        if (data->library)
        {
            lib->state = HOTDYLIB_QUIT;
            HotDylib_CallMain(lib, data->library, lib->state);
            Dylib_Free(data->library);

            /* Remove temp library */
            HotDylib_RemoveFile(data->libTempPath); /* Ignore error code */
        }

        /* Clean up */
        free(lib);
    }
}

HotDylibState HotDylibUpdate(HotDylib* lib)
{
    HotDylibData* data = (HotDylibData*)(lib + 1);

    if (HotDylib_CheckChanged(lib))
    {
        void* library;

        /* Unload old version */
        library = data->library;
        if (library)
        {
            /* Raise unload event */
            lib->state = HOTDYLIB_UNLOAD;
            HotDylib_CallMain(lib, library, lib->state);

            /* Collect garbage */
            Dylib_Free(library);
            data->library = NULL;

            if (lib->error != HOTDYLIB_ERROR_NONE)
            {
                lib->state = HOTDYLIB_FAILED;
                return lib->state;
            }
            else
            {
                return lib->state;
            }
        }

        /* Create and load new temp version */
        HotDylib_RemoveFile(data->libTempPath); /* Remove temp library */
        if (HotDylib_CopyFile(data->libRealPath, data->libTempPath))
        {
            library = Dylib_Load(data->libTempPath);
            if (library)
            {
#if defined(_MSC_VER) && HOTDYLIB_PDB_UNLOCK
                HotDylib_UnlockPdbFile(data, data->pdbRealPath);
                data->pdbTime = HotDylib_GetLastModifyTime(data->pdbRealPath);
#endif

                HotDylibState newState = lib->state == HOTDYLIB_NONE ? HOTDYLIB_INIT : HOTDYLIB_RELOAD;
                HotDylib_CallMain(lib, library, newState);

                data->library = library;
                data->libTime = HotDylib_GetLastModifyTime(data->libRealPath);

                if (lib->error != HOTDYLIB_ERROR_NONE)
                {
                    Dylib_Free(data->library);

                    data->library = NULL;
                    lib->state = HOTDYLIB_FAILED;
                }
                else
                {
                    lib->state = newState;
                }
            }
            else
            {
                data->library = NULL;
                lib->state = HOTDYLIB_FAILED;
            }
        }

        return lib->state;
    }
    else
    {
        // Clear all active state
        if (lib->state != HOTDYLIB_FAILED)
        {
            lib->state = HOTDYLIB_NONE;
        }

        // Still return HOTDYLIB_FAILED if failed not handled
        return lib->state;
    }
}

void* HotDylibGetSymbol(const HotDylib* lib, const char* symbolName)
{
    HotDylibData* data = (HotDylibData*)(lib + 1);
    return Dylib_GetSymbol(data->library, symbolName);
}

const char* HotDylibGetError(const HotDylib* lib)
{
    (void)lib;
    return Dylib_GetError();
}

