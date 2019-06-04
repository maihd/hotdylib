#include <stdio.h>
#include "../HotDylib.h"

void* HotDylibMain(void* userdata, int oldState, int newState)
{
    printf("HotDylibMain Changed\n");
    return 0;
}