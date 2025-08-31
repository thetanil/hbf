# SDL2 Bazel Build Plan

## Objective

Build SDL2 v2.32.8 with audio and OpenGL 3.3 support using Bazel and rules_foreign_cc for import into a Golang Bazel project.

## Steps

1. **Setup MODULE.bazel**

   - Add rules_foreign_cc dependency
   - Add any additional required dependencies (toolchains, etc.)

2. **Download SDL2 Source**

   - Use http_archive to fetch SDL2 v2.32.8 from GitHub release
   - Configure the archive extraction

3. **Create BUILD.bazel**

   - Use cmake() rule from rules_foreign_cc
   - Configure CMake options for:
     - Audio support enabled
     - OpenGL 3.3 support enabled
     - Static library build
   - Define cc_library target for SDL2

4. **Configure CMake Options**

   - SDL_AUDIO=ON
   - SDL_VIDEO=ON
   - SDL_RENDER=ON
   - SDL_OPENGL=ON
   - SDL_STATIC=ON
   - SDL_SHARED=OFF (for easier Golang integration)

5. **Test Build**

   - Verify the library builds successfully
   - Ensure headers are properly exposed
   - Test linking with a simple C89 program

6. **Documentation**
   - Document usage for Golang projects
   - Provide example import statements
