local THIRDPARTY_DIR = path.getabsolute("ThirdParty")

function thirdpartyfiles()
    files {
        path.join(MOJO_DIR, "ThirdParty/.Sources/glew-2.1.0/src/glew.c")
    }
end

function thirdpartylinks()
    links {
        "SDL2",
        "SDL2main",
        "OpenAL32",
        "OpenGL32",
    }

    linkoptions {
        --"/wholearchive:OpenAL32"
    }

    filter {}
end

function thirdpartydefines()
    defines {
        "GLEW_STATIC",
        "AL_LIBTYPE_STATIC",
    }
end

function thirdpartylibdirs()
    filter { "platforms:x32" }
    do
        libdirs {
            path.join(THIRDPARTY_DIR, "Libs/Win32"),
        }

        postbuildcommands {
            "{COPY} " .. path.join(THIRDPARTY_DIR, "Libs/Win32/SDL2.dll") .. " %{cfg.buildtarget.directory}"
        }
    end

    filter { "platforms:x64" }
    do
        libdirs {
            path.join(THIRDPARTY_DIR, "Libs/Win64"),
        }

        postbuildcommands {
            "{COPY} " .. path.join(THIRDPARTY_DIR, "Libs/Win64/SDL2.dll") .. " %{cfg.buildtarget.directory}"
        }
    end

    filter {}
end

function thirdpartyincludedirs()
    includedirs {
        path.join(THIRDPARTY_DIR, "Include/SDL2"),
        path.join(THIRDPARTY_DIR, ".Sources/stb"),
        path.join(THIRDPARTY_DIR, ".Sources/glew-2.1.0/include"),
    }
end