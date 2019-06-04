#include "../HotDylib.h"

#include <Windows.h>

int main(void)
{
    HotDylib lib;
    HotDylibInit(&lib, "bin/x32/Debug/HotDylib.LibTest.dll");

    char directory[1024];
    GetCurrentDirectoryA(sizeof(directory), directory);

    while (1)
    {
        int code = HotDylibUpdate(&lib);
        switch (code)
        {

        }

        Sleep(1000);
    }

    HotDylibFree(&lib);
    return 0;
}