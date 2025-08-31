load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
)

cmake(
    name = "sdl2",
    cache_entries = {
        "SDL_STATIC": "ON",
        "SDL_SHARED": "OFF",
        "SDL_AUDIO": "ON",
        "SDL_VIDEO": "ON",
        "SDL_RENDER": "ON",
        "SDL_OPENGL": "ON",
        "SDL_OPENGLES": "ON",
        "SDL_VULKAN": "ON",
        "SDL_METAL": "ON",
        "SDL_EVENTS": "ON",
        "SDL_JOYSTICK": "ON",
        "SDL_HAPTIC": "ON",
        "SDL_HIDAPI": "ON",
        "SDL_POWER": "ON",
        "SDL_FILESYSTEM": "ON",
        "SDL_THREADS": "ON",
        "SDL_TIMERS": "ON",
        "SDL_LOADSO": "ON",
        "SDL_CPUINFO": "ON",
        "SDL_ASSEMBLY": "ON",
        "SDL_ATOMIC": "ON",
        "SDL_SENSOR": "ON",
        "SDL_LOCALE": "ON",
        "SDL_MISC": "ON",
        "CMAKE_BUILD_TYPE": "Release",
    },
    lib_source = ":all_srcs",
    out_include_dir = "include",
    out_static_libs = ["libSDL2.a", "libSDL2main.a"],
    visibility = ["//visibility:public"],
)