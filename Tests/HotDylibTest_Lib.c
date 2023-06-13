#include <time.h>
#include <stdio.h>
#include "../HotDylib.h"

__declspec(dllexport)
void* HotDylibMain(void* userdata, int newState, int oldState)
{
    printf("HotDylibMain Changed at %d\n", (int)time(NULL));
    printf("Hello world from HotDylibTest_Lib 1\n");

    //int* ptr = 0;
    //*ptr = 0;
    return 0;
}