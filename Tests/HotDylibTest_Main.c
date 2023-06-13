#include "../HotDylib.h"

#include <stdio.h>
#include <Windows.h>
#include <fileapi.h>

int main(void)
{
#if NDEBUG
#if _WIN64
    HotDylib* lib = HotDylibOpen("bin/x64/Release/HotDylib.LibTest.dll", "HotDylibMain");
#else
    HotDylib* lib = HotDylibOpen("bin/x32/Release/HotDylib.LibTest.dll", "HotDylibMain");
#endif
#else
#if _WIN64
    HotDylib* lib = HotDylibOpen("bin/x64/Debug/HotDylib.LibTest.dll", "HotDylibMain");
#else
    HotDylib* lib = HotDylibOpen("bin/x32/Debug/HotDylib.LibTest.dll", "HotDylibMain");
#endif
#endif
    
    bool waitToUnlock = false;
    while (1)
    {
        HotDylibState state = HotDylibUpdate(lib);
        switch (state)
        {
            case HOTDYLIB_NONE:
                if (waitToUnlock)
                {
                    printf("[Host] PDB is unlocked! Ready to recompile.");
                    waitToUnlock = false;
                }
                break;

            case HOTDYLIB_INIT:
            case HOTDYLIB_RELOAD:
                waitToUnlock = true;
                break;
        }

        Sleep(1000);
    }

    HotDylibFree(lib);
    return 0;
}
