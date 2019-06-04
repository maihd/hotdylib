#include "HotDylib.h"

#ifdef HOTDYLIB_PDB_DELETE
#define HOTDYLIB_PDB_UNLOCK 1
#endif

#ifndef HOTDYLIB_PDB_UNLOCK
#define HOTDYLIB_PDB_UNLOCK 1
#endif

#define HOTDYLIB_MAX_PATH 256

typedef struct
{
    void* library;

    long  libtime;
    char  libRealPath[HOTDYLIB_MAX_PATH];
    char  libTempPath[HOTDYLIB_MAX_PATH];

#if defined(_MSC_VER) && HOTDYLIB_PDB_UNLOCK
    int   delpdb;
    long  pdbtime;

    char  pdbRealPath[HOTDYLIB_MAX_PATH];
    char  pdbTempPath[HOTDYLIB_MAX_PATH];
#endif
} HotDylibData;

#include <stdio.h>
#include <stdlib.h>

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

static long HotDylib_GetLastModifyTime(const char* path)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad))
    {
        return 0;
    }

    LARGE_INTEGER time;
    time.LowPart  = fad.ftLastWriteTime.dwLowDateTime;
    time.HighPart = fad.ftLastWriteTime.dwHighDateTime;

    return (long)(time.QuadPart / 10000000L - 11644473600L);
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

int HotDylib_SehFilter(HotDylib* lib, unsigned long code)
{
    int errcode = HOTDYLIB_ERROR_NONE;
    
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:
        errcode = HOTDYLIB_ERROR_SEGFAULT;
        break;

    case EXCEPTION_ILLEGAL_INSTRUCTION:
        errcode = HOTDYLIB_ERROR_ILLCODE;
        break;
	
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        errcode = HOTDYLIB_ERROR_MISALIGN;
        break;

    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        errcode = HOTDYLIB_ERROR_OUTBOUNDS;
        break;

    case EXCEPTION_STACK_OVERFLOW:
        errcode = HOTDYLIB_ERROR_STACKOVERFLOW;
        break;

    default:
        break;
    }

    if (lib) lib->errcode = errcode;
    if (errcode == HOTDYLIB_ERROR_NONE)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    else
    {
        return EXCEPTION_EXECUTE_HANDLER;
    }
}

static int HotDylib_RemoveFile(const char* path);

#if defined(_MSC_VER)

#  if defined(HOTDYLIB_PDB_UNLOCK) || defined(HOTDYLIB_PDB_DELETE)
#    include <winternl.h>
#    include <RestartManager.h> 
#    pragma comment(lib, "ntdll.lib")
#    pragma comment(lib, "rstrtmgr.lib")

#    define NTSTATUS_SUCCESS              ((NTSTATUS)0x00000000L)
#    define NTSTATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xc0000004L)

// Undocumented SYSTEM_INFORMATION_CLASS: SystemHandleInformation
#    define SystemHandleInformation ((SYSTEM_INFORMATION_CLASS)16)

typedef struct
{
    ULONG       ProcessId;
    BYTE        ObjectTypeNumber;
    BYTE        Flags;
    USHORT      Value;
    PVOID       Address;
    ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE;

typedef struct
{
    ULONG         HandleCount;
    SYSTEM_HANDLE Handles[1];
} SYSTEM_HANDLE_INFORMATION;

typedef struct
{
    UNICODE_STRING Name;
} OBJECT_INFORMATION;

static BOOL HotDylib_PathCompare(LPCWSTR path0, LPCWSTR path1)
{
    //Get file handles
    HANDLE handle1 = CreateFileW(path0, 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE handle2 = CreateFileW(path1, 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    BOOL result = FALSE;

    //if we could open both paths...
    if (handle1 != INVALID_HANDLE_VALUE && handle2 != INVALID_HANDLE_VALUE)
    {
        BY_HANDLE_FILE_INFORMATION fileInfo1;
        BY_HANDLE_FILE_INFORMATION fileInfo2;
        if (GetFileInformationByHandle(handle1, &fileInfo1) && GetFileInformationByHandle(handle2, &fileInfo2))
        {
            //the paths are the same if they refer to the same file (fileindex) on the same volume (volume serial number)
            result = 
                fileInfo1.dwVolumeSerialNumber == fileInfo2.dwVolumeSerialNumber &&
                fileInfo1.nFileIndexHigh == fileInfo2.nFileIndexHigh &&
                fileInfo1.nFileIndexLow == fileInfo2.nFileIndexLow;
        }
    }

    // free the handles
    CloseHandle(handle1);
    CloseHandle(handle2);

    return result;
}

static void HotDylib_UnlockFileFromProcess(HANDLE heap, SYSTEM_HANDLE_INFORMATION* sys_info, ULONG pid, const WCHAR* file)
{ 
    // Make sure the process is valid
    HANDLE hCurProcess = GetCurrentProcess();
    HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProcess)
    {
        return;
    }

    int i;
    for (i = 0; i < sys_info->HandleCount; i++)
    {
        SYSTEM_HANDLE* handle_info = &sys_info->Handles[i];
        HANDLE         handle      = (HANDLE)handle_info->Value;      

        if (handle_info->ProcessId == pid)
        {
            HANDLE hCopy; // Duplicate the handle in the current process
            if (!DuplicateHandle(hProcess, handle, hCurProcess, &hCopy, MAXIMUM_ALLOWED, FALSE, 0))
            {
                continue;
            }

            ULONG ObjectInformationLength = sizeof(OBJECT_INFORMATION) + 512;
            OBJECT_INFORMATION* pobj = (OBJECT_INFORMATION*)HeapAlloc(heap, 0, ObjectInformationLength);

            if (pobj == NULL)
            {
                continue;
            }

            ULONG ReturnLength;
            if (NtQueryObject(hCopy, (OBJECT_INFORMATION_CLASS)1, pobj, ObjectInformationLength, &ReturnLength) != NTSTATUS_SUCCESS)
            {
                HeapFree(heap, 0, pobj); 
                continue;
            }                  

        #if 0
            const WCHAR prefix[] = L"\\Device\\HarddiskVolume";
            const ULONG prefix_length = sizeof(prefix) / sizeof(prefix[0]) - 1;
            if (pobj->Name.Buffer && wcsncmp(pobj->Name.Buffer, prefix, prefix_length) == 0)
            {                          
                int volume;                   
                WCHAR path0[HOTDYLIB_MAX_PATH];
                WCHAR path1[HOTDYLIB_MAX_PATH];

            #if _MSC_VER >= 1200
                swscanf_s(pobj->Name.Buffer + prefix_length, L"%d\\%s", &volume, path0, _countof(path0));
            #else
                swscanf(pobj->Name.Buffer + prefix_length, L"%d\\%s", &volume, path0);
            #endif

                WCHAR fullpath[HOTDYLIB_MAX_PATH];
                GetVolumePathNameW(file, fullpath, HOTDYLIB_MAX_PATH);
                wsprintfW(path1, L"%c:\\%s", 'A' + volume - 1, path0);

                if (wcscmp(pobj->Name.Buffer, file) == 0)
                {
                    HANDLE hForClose;
                    DuplicateHandle(hProcess, handle, hCurProcess, &hForClose, MAXIMUM_ALLOWED, FALSE, DUPLICATE_CLOSE_SOURCE);
                    CloseHandle(hForClose);
                }
            }
        #endif


            const WCHAR prefix[] = L"\\Device\\HarddiskVolume";
            const ULONG prefix_length = sizeof(prefix) / sizeof(prefix[0]) - 1;
            if (pobj->Name.Buffer && wcsncmp(pobj->Name.Buffer, prefix, prefix_length) == 0)
            {
                WCHAR temp0[HOTDYLIB_MAX_PATH];
                WCHAR temp1[HOTDYLIB_MAX_PATH];

                int volume;
                swscanf_s(pobj->Name.Buffer + prefix_length, L"%d", &volume);
                wsprintfW(temp0, L"\\Device\\HarddiskVolume%d", volume);
                if (QueryDosDevice(temp0, temp1, HOTDYLIB_MAX_PATH) == 0)
                {
                    volume = 0;
                    DWORD error = GetLastError();
                    if (error == ERROR_INSUFFICIENT_BUFFER)
                    {
                        error = 0;
                    }
                }

                WCHAR path0[HOTDYLIB_MAX_PATH];
                WCHAR path1[HOTDYLIB_MAX_PATH];

            #if _MSC_VER >= 1200
                swscanf_s(pobj->Name.Buffer + prefix_length, L"%s", path0, _countof(path0));
            #else
                swscanf(pobj->Name.Buffer + prefix_length, L"%s", path0);
            #endif                                            

                wsprintfW(path1, L"\\?\\HarddiskVolume%s", path0);

                if (HotDylib_PathCompare(path1, file))
                {
                    HANDLE hForClose;
                    DuplicateHandle(hProcess, handle, hCurProcess, &hForClose, MAXIMUM_ALLOWED, FALSE, DUPLICATE_CLOSE_SOURCE);
                    CloseHandle(hForClose);
                }
            }

            CloseHandle(hCopy);
            HeapFree(heap, 0, pobj);
        }
    }

    CloseHandle(hProcess);
}

static SYSTEM_HANDLE_INFORMATION* HotDylib_CreateSystemInfo(void)
{                          
    size_t sys_info_size = 0;
    HANDLE heap_handle   = GetProcessHeap();
    SYSTEM_HANDLE_INFORMATION* sys_info = NULL;
    {
        ULONG res;
        NTSTATUS status;
        do
        {
            HeapFree(heap_handle, 0, sys_info);
            sys_info_size += 4096;
            sys_info = (SYSTEM_HANDLE_INFORMATION*)HeapAlloc(heap_handle, 0, sys_info_size);
            status = NtQuerySystemInformation(SystemHandleInformation, sys_info, sys_info_size, &res);
        } while (status == NTSTATUS_INFO_LENGTH_MISMATCH);
    }

    return sys_info;
}

static DWORD WINAPI HotDylib_UnlockPdbFileThread(void* userdata)
{
    HANDLE heap_handle = GetProcessHeap();
    const struct ThreadInput
    {
        HotDylibData*   lib;
        WCHAR           szFile[1];
    }* data = (const struct ThreadInput*)userdata;

    int   i;
    UINT  nProcInfoNeeded;
    UINT  nProcInfo = 10;    
    DWORD dwError;                                  
    DWORD dwReason;
    DWORD dwSession;     
    RM_PROCESS_INFO rgpi[10];
    WCHAR szSessionKey[CCH_RM_SESSION_KEY + 1] = { 0 };

    /* Create system info */
    SYSTEM_HANDLE_INFORMATION* sys_info = NULL;

    dwError = RmStartSession(&dwSession, 0, szSessionKey);
    if (dwError == ERROR_SUCCESS)
    {
        LPCWSTR szFile = data->szFile;
        dwError = RmRegisterResources(dwSession, 1, &szFile, 0, NULL, 0, NULL);
        if (dwError == ERROR_SUCCESS)
        {
            dwError = RmGetList(dwSession, &nProcInfoNeeded, &nProcInfo, rgpi, &dwReason);
            if (dwError == ERROR_SUCCESS)
            {
                for (i = 0; i < nProcInfo; i++)
                {                                            
                    if (!sys_info) sys_info = HotDylib_CreateSystemInfo();
                    HotDylib_UnlockFileFromProcess(heap_handle, sys_info, rgpi[i].Process.dwProcessId, szFile);
                }
            }
        }
        RmEndSession(dwSession);
    }            

    /* Remove the .pdb file if required */
#if defined(HOTDYLIB_PDB_DELETE)
    //data->lib->delpdb = TRUE;
    if (HotDylib_RemoveFile(data->lib->pdbRealPath))
    {
        data->lib->delpdb = TRUE;
    }
    else
    {
        data->lib->delpdb = TRUE;
    }

    HANDLE handle = CreateFileA(data->lib->pdbRealPath,
                                0,
                                FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
                                NULL);
    if (handle)
    {
        CloseHandle(handle);
    }
#endif

    /* Clean up */
    HeapFree(heap_handle, 0, sys_info);
    HeapFree(heap_handle, 0, (void*)data);

    return 0;
}

static void HotDylib_UnlockPdbFile(HotDylibData* lib, const char* file)
{
    HANDLE heap_handle = GetProcessHeap();
    struct ThreadInput
    {
        HotDylibData*   lib;
        WCHAR           szFile[1];
    }* data = (struct ThreadInput*)HeapAlloc(heap_handle, 0, sizeof(struct ThreadInput) + HOTDYLIB_MAX_PATH);

    data->lib = lib;
    MultiByteToWideChar(CP_UTF8, 0, file, -1, data->szFile, HOTDYLIB_MAX_PATH);

#if !defined(CSFX_SINGLE_THREAD)
    HANDLE threadHandle = CreateThread(NULL, 0, HotDylib_UnlockPdbFileThread, (void*)data, 0, NULL);
    (void)threadHandle;
#else
    HotDylib_UnlockPdbFileThread((void*)data);
#endif

    /* Clean up szFile must be done in HotDylib_UnlockPdbFileThread */
}   

static int HotDylib_GetPdbPath(const char* libpath, char* buf, int len)
{
    int i, chr;
    char drv[8];
    char dir[HOTDYLIB_MAX_PATH];
    char name[HOTDYLIB_MAX_PATH];
    char ext[32];

    GetFullPathNameA(libpath, len, buf, NULL);

#if _MSC_VER >= 1200
    _splitpath_s(buf, drv, sizeof(drv), dir, sizeof(dir), name, sizeof(name), NULL, 0);
#else
    _splitpath(buf, drv, dir, name, NULL);
#endif

    return snprintf(buf, len, "%s%s%s.pdb", drv, dir, name);
}
#  endif /* HOTDYLIB_PDB_UNLOCK */

bool HotDylib_Begin(void)
{
    return false;
}

void HotDylib_End(void)
{
    /* NULL */
}
# else

#  if defined(__MINGW32__)
__thread
#  else
__declspec(thread)
#  endif
jmp_buf csfx__jmpenv;

static LONG WINAPI csfx__sighandler(EXCEPTION_POINTERS* info)
{
    int excode  = info->ExceptionRecord->ExceptionCode;
    int errcode = HotDylib_SehFilter(NULL, excode);
    longjmp(csfx__jmpenv, errcode);
    return EXCEPTION_CONTINUE_EXECUTION;
}

int  HotDylib_Begin(void)
{
    SetUnhandledExceptionFilter(csfx__sighandler);
    return 0;
}

void HotDylib_End(void)
{
    SetUnhandledExceptionFilter(NULL);
}
# endif /* _MSC_VER */

#elif defined(__unix__)
# include <string.h>
# include <sys/stat.h>
# include <sys/types.h>
# define CSFX__PATH_LENGTH PATH_MAX
# define csfx__countof(x) (sizeof(x) / sizeof((x)[0]))

__thread sigjmp_buf csfx__jmpenv;
const int csfx__signals[] = { SIGBUS, SIGSYS, SIGILL, SIGSEGV, SIGABRT };

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
    sprintf(scmd, "\\cp -fR %s %s", from_path, to_path);
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

static void csfx__sighandler(int code, siginfo_t* info, void* context)
{
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
    siglongjmp(csfx__jmpenv, errcode);
}


int  HotDylib_Begin(void)
{
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags     = SA_SIGINFO | SA_RESTART | SA_NODEFER;
    sa.sa_handler   = NULL;
    sa.sa_sigaction = csfx__sighandler;

    int idx;
    for (idx = 0; idx < csfx__countof(csfx__signals); idx++)
    {
	if (sigaction(csfx__signals[idx], &sa, NULL) != 0)
	{
	    return -1;
	}
    }

    return 0;
}

void HotDylib_End(void)
{
    int idx;
    for (idx = 0; idx < csfx__countof(csfx__signals); idx++)
    {
	if (signal(csfx__signals[idx], SIG_DFL) != 0)
	{
	    break;
	}
    }
}
#else
#error "Unsupported platform"
#endif

static int HotDylib_RemoveFile(const char* path)
{
    return remove(path) == 0;
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

        #if defined(_MSC_VER) && _MSC_VER >= 1200
            FILE* file;
            if (fopen_s(&file, buffer, "r") != 0)
            {
                file = NULL;
            }
        #else
            FILE* file = fopen(buffer, "r");
        #endif

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
    HotDylibData** dptr = (HotDylibData**)(&lib->internal);
    HotDylibData*  data = *dptr;
    
    long src = data->libtime;
    long cur = HotDylib_GetLastModifyTime(data->libRealPath);
    bool res = cur > src;
#if defined(_MSC_VER) && defined(HOTDYLIB_PDB_UNLOCK)
    if (res)
    {  
        src = data->pdbtime;
        cur = HotDylib_GetLastModifyTime(data->pdbRealPath);
        res = (cur == src && cur == 0) || cur > src;
    }
#endif
    return res;
}

static int HotDylib_CheckFileChanged(HotDylibFileTime* ft)
{
    long src = ft->time;
    long cur = HotDylib_GetLastModifyTime(ft->path);
    
    if (cur > src)
    {
        ft->time = cur;
        return 1;
    }
    else
    {
        return 0;
    }
}

static int HotDylib_CallMain(HotDylib* lib, void* library, int state)
{
    typedef void* (*HotDylibMainFn)(void*, int, int);

    const char*     name = "HotDylibMain";
    HotDylibMainFn  func = (HotDylibMainFn)Dylib_GetSymbol(library, name);

    if (func)
    {
        HOTDYLIB_TRY (lib)
        {
            lib->userdata = func(lib->userdata, lib->state, state);
        }
        HOTDYLIB_EXCEPT (lib)
        {
            return -1;
        }
    }

    return 0;
}

/* @impl: HotDylibInit */
void HotDylibInit(HotDylib* lib, const char* libpath)
{
    lib->state    = HOTDYLIB_NONE;
    lib->errcode  = HOTDYLIB_ERROR_NONE;
    lib->userdata = NULL;

    HotDylibData** dptr = (HotDylibData**)(&lib->internal);
    HotDylibData*  data = (HotDylibData*)(malloc(sizeof(HotDylibData)));

    if (data)
    {
        *dptr = data;

        data->libtime = 0;
        data->library = NULL;
        HotDylib_GetTempPath(libpath, data->libTempPath, HOTDYLIB_MAX_PATH);
    
    #if defined(_MSC_VER) && _MSC_VER >= 1200
        strncpy_s(data->libRealPath, HOTDYLIB_MAX_PATH, libpath, HOTDYLIB_MAX_PATH);
    #else
        strncpy(data->libRealPath, libpath, HOTDYLIB_MAX_PATH);
    #endif
    
    #if defined(_MSC_VER) && defined(HOTDYLIB_PDB_UNLOCK)
        data->delpdb  = FALSE;
        data->pdbtime = 0;
        HotDylib_GetPdbPath(libpath, data->pdbRealPath, HOTDYLIB_MAX_PATH);
        HotDylib_GetTempPath(data->pdbRealPath, data->pdbTempPath, HOTDYLIB_MAX_PATH);
    #endif
    }
}

void HotDylibFree(HotDylib* lib)
{
    if (!lib) return;

    HotDylibData** dptr = (HotDylibData**)(&lib->internal);
    HotDylibData*  data = *dptr;

    /* Raise quit event */
    if (data->library)
    {
        lib->state = HOTDYLIB_QUIT;
        HotDylib_CallMain(lib, data->library, lib->state);
        Dylib_Free(data->library);

        /* Remove temp library */
        HotDylib_RemoveFile(data->libTempPath); /* Ignore error code */
	
#if defined(_MSC_VER) && defined(HOTDYLIB_PDB_DELETE)
        HotDylib_CopyFile(data->pdbTempPath, data->pdbRealPath);
        HotDylib_RemoveFile(data->pdbTempPath);
#endif
    }

    /* Clean up */
    free(data);
    *dptr = NULL;
}

int HotDylibUpdate(HotDylib* lib)
{
    HotDylibData** dptr = (HotDylibData**)(&lib->internal);
    HotDylibData*  data = *dptr;

#if 0
    if (data->delpdb)
    {
        HANDLE fileHandle = CreateFileA(
            data->pdbRealPath,
            GENERIC_READ,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_DELETE_ON_CLOSE,
            NULL);
        if (fileHandle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(fileHandle);
        }

        if (HotDylib_RemoveFile(data->pdbRealPath))
        {
            int a = 0;
            int b = 1;

            a = a + b;
        }
        else
        {
            int a = 0;
            int b = 1;

            a = a + b;
        }
    }
#endif
    
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

            if (lib->errcode != HOTDYLIB_ERROR_NONE)
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
                int state = lib->state; /* new state */
                state = state == HOTDYLIB_NONE ? HOTDYLIB_INIT : HOTDYLIB_RELOAD;
                HotDylib_CallMain(lib, library, state);

                data->library = library;
                data->libtime = HotDylib_GetLastModifyTime(data->libRealPath);

                if (lib->errcode != HOTDYLIB_ERROR_NONE)
                {
                    Dylib_Free(data->library);

                    data->library = NULL;
                    lib->state = HOTDYLIB_FAILED;
                }
                else
                {
                    lib->state = state;

                #if defined(_MSC_VER) && defined(HOTDYLIB_PDB_UNLOCK)
                #   if defined(HOTDYLIB_PDB_DELETE)
                    HotDylib_RemoveFile(data->pdbTempPath);
                    HotDylib_CopyFile(data->pdbRealPath, data->pdbTempPath);
                #   endif

                    HotDylib_UnlockPdbFile(data, data->pdbRealPath);
                    data->pdbtime = HotDylib_GetLastModifyTime(data->pdbRealPath);
                #endif
                }
            }
        }

        return lib->state;
    }
    else
    {
        if (lib->state != HOTDYLIB_FAILED)
        {
            lib->state = HOTDYLIB_NONE;
        }

        return HOTDYLIB_NONE;
    }
}

void* HotDylibGetSymbol(HotDylib* lib, const char* name)
{
    HotDylibData** dptr = (HotDylibData**)(&lib->internal);
    HotDylibData*  data = *dptr;
    return Dylib_GetSymbol(data->library, name);
}

const char* HotDylibGetError(const HotDylib* lib)
{
    (void)lib;
    return Dylib_GetError();
}

/* @impl: HotDylibWatchFiles */
bool HotDylibWatchFiles(HotDylibFileTime* files, int count)
{
    bool changed = 0;
    for (int i = 0; i < count; i++)
    {
        if (HotDylib_CheckFileChanged(&files[i]))
        {
            changed = true;
        }
    }
    return changed;
}
