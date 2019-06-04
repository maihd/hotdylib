local ROOT_DIR = path.getabsolute(".")
local BUILD_DIR = path.join(ROOT_DIR, "Build")

workspace "HotDylib"
do
    language "C"
    location (BUILD_DIR)

    platforms { "x32", "x64" }
    configurations { "Debug", "Release" }
    
    startproject "HotDylib.Test"

    rtti "off"
    exceptionhandling "SEH"

    filter {}
end

project "HotDylib"
do
    kind "StaticLib"

    files {
        path.join(ROOT_DIR, "HotDylib.h"),
        path.join(ROOT_DIR, "HotDylib.c"),
    }

    filter {}
end

project "HotDylib.Test"
do
    kind "ConsoleApp"

    links {
        "HotDylib"
    }

    
    files {
        path.join(ROOT_DIR, "Tests/HotDylibTest_Main.c"),
    }

    filter {}
end

workspace "HotDylib.Lib"
do
    language "C"
    location (BUILD_DIR)

    platforms { "x32", "x64" }
    configurations { "Debug", "Release" }
    
    startproject "HotDylib.Test"

    filter {}
end

project "HotDylib.LibTest"
do
    kind "SharedLib"

    files {
        path.join(ROOT_DIR, "Tests/HotDylibTest_Lib.c"),
    }

    filter {}
end