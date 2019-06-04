#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <Windows.h>

#include "app.h"

#include <HotDylibEx.h>
#include <HotDylibEx.c>

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static void execute_and_wait(const char* cmd)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    
    si.cb = sizeof(si);
    
    if (CreateProcessA(NULL,
		       (char*)cmd,
		       NULL, NULL, FALSE, 0,
		       NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
		     LPSTR cmdLine, int cmdShow)
{   
    printf("= Windows GDI experiment application =\n");
    printf("======================================\n\n");
    
    app_create_window(hInstance);

#ifndef _WIN64
#ifndef NDEBUG
    const char* path = "bin/x32/Debug/GdiLearn.Script.dll";
#else
    const char* path = "bin/x32/Release/GdiLearn.Script.dll";
#endif
#else
#ifndef NDEBUG
    const char* path = "bin/x64/Debug/GdiLearn.Script.dll";
#else
    const char* path = "bin/x64/Release/GdiLearn.Script.dll";
#endif
#endif

    HotDylib* lib = HotDylibOpen(path, "script_main");

    //HotDylibFileTime files[] = {
	//    { 0, "../script.c" },
    //};
    //HotDylibWatchFiles(files, countof(files));
    
    while (!app_isquit())
    {
        app_update(lib);
        
        //if (HotDylibWatchFiles(files, countof(files)))
        //{
        //    printf("Script has changed. Rebuilding...\n");
        //    execute_and_wait("build_script.bat");
        //}
	
        app_usleep(1000);
    }

    printf("\n");
    printf("======================================\n");
    
    HotDylibFree(lib);
    return 0;
}
