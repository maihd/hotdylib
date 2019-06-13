#include <imgui/imgui.h>
#include <HotDylibApi.h>

struct Calculator
{
    void DrawGUI()
    {
        const ImVec2 SIZE = ImVec2(80, 30);

        ImGui::Begin("Calculator");

        ImGui::Columns(3, "Calcutalor", false);

        if (ImGui::Button("1", SIZE))
        {
        }

        ImGui::NextColumn();
        if (ImGui::Button("2", SIZE))
        {
        }
        
        ImGui::NextColumn();
        if (ImGui::Button("3", SIZE))
        {
        }

        ImGui::End();
    }
};

HOTDYLIB_EXPORT void* HotDylibMain(void* userdata, int state, int oldState)
{
    switch (state)
    {
    case HOTDYLIB_INIT:
        break;

    case HOTDYLIB_RELOAD:
        break;

    case HOTDYLIB_UNLOAD:
        break;

    case HOTDYLIB_QUIT:
        break;

    default:
        break;
    }

    return NULL;
}

HOTDYLIB_EXPORT void OnGui(void)
{
    ImGui::ShowDemoWindow();
    ImGui::ShowAboutWindow();
    
    Calculator calculator;
    calculator.DrawGUI();
}