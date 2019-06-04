local ROOT_DIR = path.getabsolute(".")
local BUILD_DIR = path.join(ROOT_DIR, "Build")

workspace "GdiLearn.Bin"
do
    language "C"
    location (BUILD_DIR)

    platforms { "x32", "x64" }
    configurations { "Debug", "Release" }

    filter {}
end

project "GdiLearn.Lib"
do
    kind "SharedLib"

    includedirs {
        path.join(ROOT_DIR, "../../")
    }

    defines {
        "APP_EXPORT"
    }

    files {
        path.join(ROOT_DIR, "app.h"),
        path.join(ROOT_DIR, "app.c"),
    }
end

project "GdiLearn.Bin"
do
    kind "WindowedApp"

    links {
        "GdiLearn.Lib"
    }

    includedirs {
        path.join(ROOT_DIR, "../../")
    }

    files {
        path.join(ROOT_DIR, "main.c"),
    }

    filter { "platforms:Release" }
    do
        defines {
            "NDEBUG"
        }
    end

    filter {}
end

workspace "GdiLearn.Script"
do
    language "C"
    location (BUILD_DIR)

    platforms { "x32", "x64" }
    configurations { "Debug", "Release" }

    filter {}
end

project "GdiLearn.Script"
do
    kind "SharedLib"

    includedirs {
        path.join(ROOT_DIR, "../../")
    }

    files {
        path.join(ROOT_DIR, "script.c"),
    }
end