#include "../HotDylib.h"

#include <Windows.h>
#include <fileapi.h>

int main(void)
{
    HotDylib* lib = HotDylibOpen("bin/x32/Debug/HotDylib.LibTest.dll", "HotDylibMain");
    while (1)
    {
        int code = HotDylibUpdate(lib);
        switch (code)
        {

        }

        Sleep(1000);
    }

    HotDylibFree(lib);
    return 0;
}