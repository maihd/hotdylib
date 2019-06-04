#include "HotDylibEx.h"

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
#else
static long HotDylib_GetLastModifyTime(const char* path)
{
    struct stat st;
    if (stat(path, &st) != 0)
    {
	    return 0;
    }

    return (long)st.st_mtime;
}
#endif

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