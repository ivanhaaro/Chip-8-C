#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL.h>

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
} sdl_t;

typedef struct {
    uint32_t window_width;
    uint32_t window_height;
} config_t;

bool init_sdl(sdl_t *sdl, config_t *config) {

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        SDL_Log("Could not initalize SDL subsystems! %s\n", SDL_GetError());
        return false;
    }

    sdl->window = SDL_CreateWindow("CHIP8 Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                    config->window_width, config->window_height, 0); 
    if(!sdl->window) {
        SDL_Log("Could not create window %s\n", SDL_GetError());
        return false;
    }

    sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);
    if(!sdl->renderer) {
        SDL_Log("Could not create renderer %s\n", SDL_GetError());
        return false;
    }

    return true;
}

bool set_up_config(config_t *config, int argc, char **argv) {

    // Set defaults
    *config = (config_t) {
        .window_width = 1280,
        .window_height = 640,
    };

    //Override defaults from arguments
    for (int i = 1; i < argc; i++) {
        (void)argv[i]; // Prevent complier error
    }

    return true;
}

void final_cleanup(sdl_t sdl) {
    SDL_DestroyRenderer(sdl.renderer);
    SDL_DestroyWindow(sdl.window);
    SDL_Quit();
}

int main(int argc, char **argv) {

    // Initialize the configuration of the emulator
    config_t config = {0}; 
    if(!set_up_config(&config, argc, argv)) exit(EXIT_FAILURE);

    // Initialize SDL 
    sdl_t sdl = {0};
    if(!init_sdl(&sdl, &config)) exit(EXIT_FAILURE);

    // Initial screen clear
    //SDL_SetRenderDrawColor(sdl.renderer);
    SDL_RenderClear(sdl.renderer);

    // Main emulator loop
    while(true) {

    }

    // Final cleanup
    final_cleanup(sdl);

    exit(EXIT_SUCCESS);
}