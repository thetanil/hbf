#include <SDL2/SDL.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    /* Initialize SDL */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    /* Print SDL version */
    SDL_version compiled;
    SDL_version linked;
    
    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);
    
    printf("SDL2 compiled version: %d.%d.%d\n", 
           compiled.major, compiled.minor, compiled.patch);
    printf("SDL2 linked version: %d.%d.%d\n", 
           linked.major, linked.minor, linked.patch);

    /* Test video subsystem */
    int num_video_drivers = SDL_GetNumVideoDrivers();
    printf("Available video drivers: %d\n", num_video_drivers);
    
    for (int i = 0; i < num_video_drivers; i++) {
        printf("  - %s\n", SDL_GetVideoDriver(i));
    }

    /* Test audio subsystem */
    int num_audio_drivers = SDL_GetNumAudioDrivers();
    printf("Available audio drivers: %d\n", num_audio_drivers);
    
    for (int i = 0; i < num_audio_drivers; i++) {
        printf("  - %s\n", SDL_GetAudioDriver(i));
    }

    /* Clean up */
    SDL_Quit();
    
    printf("SDL2 test completed successfully!\n");
    return 0;
}