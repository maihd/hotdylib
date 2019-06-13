local ROOT_DIR = path.getabsolute(".")
local BUILD_DIR = path.join(ROOT_DIR, "Build")

dofile (path.join(ROOT_DIR, "premake5.thirdparty.lua"))

workspace "HotImGui.Host"
do
    language "C"
    location (BUILD_DIR)

    platforms { "x32", "x64" }
    configurations { "Debug", "Release" }

    rtti "off"
    exceptionhandling "off"

    filter {}
end

project "ImGui"
do
    kind "SharedLib"

    includedirs {
        path.join(ROOT_DIR, "imgui"),
        path.join(ROOT_DIR, "../../"),
    }

    defines {
        "IMGUI_IMPL_OPENGL_LOADER_GLEW",
        "IMGUI_API=__declspec(dllexport)",
    }

    files {
        path.join(ROOT_DIR, "ImguiImpl/*.h"),
        path.join(ROOT_DIR, "ImguiImpl/*.cc"),
        path.join(ROOT_DIR, "ImguiImpl/**/*.h"),
        path.join(ROOT_DIR, "ImguiImpl/**/*.cc"),

        path.join(ROOT_DIR, "imgui/*.h"),
        path.join(ROOT_DIR, "imgui/*.cpp"),
    }

    filter {}
    
    -- third party
    thirdpartyfiles()
    thirdpartylinks()
    thirdpartydefines()
    thirdpartylibdirs()
    thirdpartyincludedirs()

    filter { "platforms:Release" }
    do
        defines {
            "NDEBUG"
        }
    end

    filter {}
end

project "HotImGui.Host"
do
    kind "WindowedApp"

    links {
        "ImGui",
        "OpenGL32"
    }
    
    defines {
        "IMGUI_IMPL_OPENGL_LOADER_GLEW",
        "IMGUI_API=__declspec(dllimport)"
    }

    includedirs {
        path.join(ROOT_DIR, ""),
        path.join(ROOT_DIR, "../../"),
    }

    files {
        path.join(ROOT_DIR, "Host/*.h"),
        path.join(ROOT_DIR, "Host/*.cc"),
        path.join(ROOT_DIR, "Host/**/*.h"),
        path.join(ROOT_DIR, "Host/**/*.cc"),
    }

    thirdpartylinks()
    thirdpartylibdirs()
    thirdpartyincludedirs()

    filter { "platforms:Release" }
    do
        defines {
            "NDEBUG"
        }
    end

    filter {}
end

workspace "HotImGui.Guest"
do
    language "C"
    location (BUILD_DIR)

    platforms { "x32", "x64" }
    configurations { "Debug", "Release" }

    filter {}
end

project "HotImGui.Guest"
do
    kind "SharedLib"
    
    defines {
        "IMGUI_API=__declspec(dllimport)"
    }

    links {
        "ImGui"
    }

    libdirs {
        "%{cfg.buildtarget.directory}"
    }

    includedirs {
        path.join(ROOT_DIR, ""),
        path.join(ROOT_DIR, "../../"),
    }

    files {
        path.join(ROOT_DIR, "../../HotDylibApi.h"),
        
        path.join(ROOT_DIR, "Guest/*.h"),
        path.join(ROOT_DIR, "Guest/*.cc"),
        path.join(ROOT_DIR, "Guest/**/*.h"),
        path.join(ROOT_DIR, "Guest/**/*.cc"),
    }

    filter {}
end