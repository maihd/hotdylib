#include <stdio.h>
#include "../HotDylib.h"

void* HotDylibMain(void* userdata, int oldState, int newState)
{
    printf("HotDylibMain Changed\n");

    int* ptr = 0;
    *ptr = 0;
    return 0;
}