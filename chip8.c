#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "SDL.h"

//SDL container struct.
typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
} sdl_t;

//Emulator configuration struct.
typedef struct
{
    uint32_t window_width;  // SDL window width.
    uint32_t window_height; // SDL window height.
    uint32_t foreground_colour; // RGBA8888.
    uint32_t background_colour; // RGNA8888.
    uint32_t scale_factor; // Amount to scale each CHIP-8 pixel by. E.g. 20x will be 20x larger.
} config_t;

//Emulator states.
typedef enum {
    QUIT,
    RUNNING,
    PAUSED,
} emulator_state_t;

//CHIP-8 machine struct.
typedef struct {
    emulator_state_t state;
} chip8_t;

// Initialise SDL function.
bool initSDL(sdl_t *sdl, const config_t config)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0)
    {
        SDL_Log("Unable to initialise SDL subsystems. %s\n", SDL_GetError());
        return false;
    }

    sdl->window = SDL_CreateWindow(
        "CHIP-8 Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        config.window_width * config.scale_factor,
        config.window_height * config.scale_factor,
        0);

    if (!sdl->window)
    {
        SDL_Log("Unable to create SDL window. %s", SDL_GetError());
        return false;
    }

    sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);
    if (!sdl->renderer) {
        SDL_Log("Unable to create SDL renderer. %s", SDL_GetError());
    }

    return true; // If success.
}

// Initial interpreter config.
bool setConfig_Args(config_t *config, const int argc, char **argv)
{

    // Set defaults.
    *config = (config_t){
        .window_width = 64,  // CHIP-8 original X resolution.
        .window_height = 32, // CHIP-8 original Y resolution.
        .foreground_colour = 0xFF00FF00, //GREEN
        .background_colour = 0x00000000, //BLACK
        .scale_factor = 20, // Default resolution will be 1280x640.
    };

    // Override defaults.
    for (int i = 0; i < argc; i++)
    {
        (void)argv[i]; // Prevent compiler error due to unused argc/argv.
        //...
    };

    return true;
}

//Initialise CHIP-8 machine.
bool initCHIP(chip8_t *chip8){
    chip8->state = RUNNING;
    return true; //Success.
}

// Final cleanup function.
void finalCleanUp(const sdl_t *sdl)
{
    SDL_DestroyRenderer(sdl->renderer);
    SDL_DestroyWindow(sdl->window);
    SDL_Quit(); // Close SDL subsystem.
}

//Clear SDL window to background configuration.
void clearWindow(const sdl_t sdl, const config_t config) {
    const uint8_t r = (config.background_colour >> 24) & 0xFF;
    const uint8_t g = (config.background_colour >> 16) & 0xFF;
    const uint8_t b = (config.background_colour >> 8) & 0xFF;
    const uint8_t a = (config.background_colour >> 0) & 0xFF;

    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
    SDL_RenderClear(sdl.renderer);
}

//Update window with changes.
void updateScreen(const sdl_t sdl) {
    SDL_RenderPresent(sdl.renderer);
}

//Handle user input.
void handleInput(chip8_t *chip8){
    SDL_Event event;
    
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                //Exit window or end program.
                chip8->state = QUIT; //Will break the main emulator loop.
                return;
            
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        //Escape key: exit window, and end program.
                        chip8->state = QUIT;
                        return;
                    
                    default:
                        break;
                }
                break;

            case SDL_KEYUP:
                break;
            
            default:
                break;
        }
    }
}

//MAIN FUNC
int main(int argc, char **argv)
{

    // Initialise emulator config.
    config_t config = {0};
    if (!setConfig_Args(&config, argc, argv))
    {
        exit(EXIT_FAILURE);
    };

    // Initialise SDL.
    sdl_t sdl = {0};
    if (!initSDL(&sdl, config))
    {
        exit(EXIT_FAILURE);
    };

    //Initialise CHIP-8 machine.
    chip8_t chip8 = {0};
    if (!initCHIP(&chip8))
    {
        exit(EXIT_FAILURE);
    };

    //Initial screen clear to background configuration.
    clearWindow(sdl, config);

    //Main emulator loop
    while (chip8.state != QUIT) {
        //Handle user inputs.
        handleInput(&chip8);


        //if paused continue.

        //Get time()
        //Emulate CHIP8 instructions
        //Get time changed from last get time()

        //Delay for 60FPS!
        SDL_Delay(16.67); //-actual time elapsed!!!!!!!!!!!!!!!!!
        //Update window with changes.
        updateScreen(sdl);
    }

    // Final cleanup before interpreter exit.
    finalCleanUp(&sdl);

    exit(EXIT_SUCCESS);
}