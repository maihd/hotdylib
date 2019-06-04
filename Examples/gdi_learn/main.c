#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <Windows.h>

#include "app.h"

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

    HotDylib* lib = HotDylibOpen("script.dll", "script_main");

    HotDylibFileTime files[] = {
	    { 0, "script.c" },
    };
    HotDylibWatchFiles(files, countof(files));
    
    while (!app_isquit())
    {
        app_update(lib);
        
        if (HotDylibWatchFiles(files, countof(files)))
        {
            printf("Script has changed. Rebuilding...\n");
            execute_and_wait("build_script.bat");
        }
	
        app_usleep(1000);
    }

    printf("\n");
    printf("======================================\n");
    
    HotDylibFree(lib);
    return 0;
}
