#include "app.h"
#include <stdio.h>
#include <Windows.h>
#include <HotDylib.h>

void* HotDylibMain(void* userdata, int old_state, int state)
{
    switch (state)
    {
    case HOTDYLIB_INIT:
	    break;

    case HOTDYLIB_QUIT:
	    break;

    case HOTDYLIB_RELOAD:
	    break;

    case HOTDYLIB_UNLOAD:
	    break;
    }

    return userdata;
}

__declspec(dllexport)
void on_paint(HDC hdc, int width, int height)
{
    static int posX     = 0;
    static int posY     = 100;
    static int posW     = 200;
    static int posH     = 200;

    SelectObject(hdc, GetStockObject(GRAY_BRUSH));
    Ellipse(hdc, posX++, posY, posX + posW, posY + posH);
}
