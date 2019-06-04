#include "../HotDylib.h"

#include <Windows.h>
#include <fileapi.h>

int main(void)
{
    HotDylib lib;
    HotDylibInit(&lib, "bin/x32/Debug/HotDylib.LibTest.dll");

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