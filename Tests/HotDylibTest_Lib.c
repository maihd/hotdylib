#include <stdio.h>
#include "../HotDylib.h"

__declspec(dllexport)
void* HotDylibMain(void* userdata, int newState, int oldState)
{
    printf("HotDylibMain Changed\n");
    printf("Hello world from HotDylibTest_Lib\n");

    //int* ptr = 0;
    //*ptr = 0;
    return 0;
}