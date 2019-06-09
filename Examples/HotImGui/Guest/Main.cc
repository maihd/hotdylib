#include <imgui/imgui.h>

extern "C"
__declspec(dllexport) void OnGui(void)
{
    ImGui::ShowDemoWindow();

    if (ImGui::Button("Click me!"))
    {

    }
}