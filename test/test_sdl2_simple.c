#include <SDL2/SDL.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    /* Just test version info without initializing subsystems */
    SDL_version compiled;
    SDL_version linked;
    
    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);
    
    printf("SDL2 compiled version: %d.%d.%d\n", 
           compiled.major, compiled.minor, compiled.patch);
    printf("SDL2 linked version: %d.%d.%d\n", 
           linked.major, linked.minor, linked.patch);

    printf("SDL2 library test completed successfully!\n");
    return 0;
}