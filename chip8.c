#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "SDL.h"

// SDL container struct.
typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
} sdl_t;

// Emulator configuration struct.
typedef struct
{
    uint32_t window_width;      // SDL window width.
    uint32_t window_height;     // SDL window height.
    uint32_t foreground_colour; // RGBA8888.
    uint32_t background_colour; // RGNA8888.
    uint32_t scale_factor;      // Amount to scale each CHIP-8 pixel by. E.g. 20x will be 20x larger.
    bool pixel_outlines;        // Draw pixel "outlines" yes/no.
} config_t;

// Emulator states.
typedef enum
{
    QUIT,
    RUNNING,
    PAUSED,
} emulator_state_t;

typedef struct
{
    uint16_t opcode;
    uint16_t NNN; // 12 bit address
    uint8_t NN;   // 8 bit address
    uint8_t N;    // 4 bit address
    uint8_t X;    // 4 bit register identifier
    uint8_t Y;    // 4 bit register identifier
} instruction_t;

// CHIP-8 machine struct.
typedef struct
{
    emulator_state_t state;
    uint8_t ram[4096];
    bool display[64 * 32]; // original chip8 resolution
    uint16_t stack[12];    // subroutine stack
    uint16_t *stack_ptr;
    uint8_t V[16];        // data registers V0-VF
    uint16_t I;           // index registers
    uint16_t PC;          // program counter
    uint8_t delay_timer;  // decrements at 60hz when > 0
    uint8_t sound_timer;  // decrements at 60hz and plays tone when > 0
    bool keypad[16];      // hexadecimal keypad 0x0 - 0xF
    const char *rom_name; // currently running ROM
    instruction_t inst;   // currently executing instruction
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
    if (!sdl->renderer)
    {
        SDL_Log("Unable to create SDL renderer. %s", SDL_GetError());
    }

    return true; // If success.
}

// Initial interpreter config.
bool setConfig_Args(config_t *config, const int argc, char **argv)
{

    // Set defaults.
    *config = (config_t){
        .window_width = 64,              // CHIP-8 original X resolution.
        .window_height = 32,             // CHIP-8 original Y resolution.
        .foreground_colour = 0x18392B00, // GREEN
        .background_colour = 0x000000FF, // BLACK
        .scale_factor = 20,              // Default resolution will be 1280x640.
        .pixel_outlines = true           // Draw pixel outlines by default
    };

    // Override defaults.
    for (int i = 0; i < argc; i++)
    {
        (void)argv[i]; // Prevent compiler error due to unused argc/argv.
        //...
    };

    return true;
}

// Initialise CHIP-8 machine.
bool initCHIP(chip8_t *chip8, const char rom_name[])
{
    const uint32_t entry_point = 0x200; // roms loaded to 0x200
    const uint8_t font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };

    // load font
    memcpy(&chip8->ram[0], font, sizeof(font));

    // open rom file
    FILE *rom = fopen(rom_name, "rb");
    if (!rom)
    {
        SDL_Log("ROM file %s is invalid or does not exist.\n", rom_name);
        return false;
    }
    // check rom size
    fseek(rom, 0, SEEK_END);
    const size_t rom_size = ftell(rom);
    const size_t max_size = sizeof chip8->ram - entry_point;
    rewind(rom);

    if (rom_size > max_size)
    {
        SDL_Log("ROM file %s is too big.\n Rom size: %zu.\nMax size allowed: %zu\n",
                rom_name, rom_size, max_size);
        return false;
    }

    // load rom
    if (fread(&chip8->ram[entry_point], rom_size, 1, rom) != 1)
    {
        SDL_Log("Could not read ROM file %s into CHIP-8 memory.\n",
                rom_name);
        return false;
    };
    fclose(rom);

    // set machine defaults
    chip8->state = RUNNING;
    chip8->PC = entry_point;
    chip8->rom_name = rom_name;
    chip8->stack_ptr = &chip8->stack[0];

    return true; // Success.
}

// Final cleanup function.
void finalCleanUp(const sdl_t *sdl)
{
    SDL_DestroyRenderer(sdl->renderer);
    SDL_DestroyWindow(sdl->window);
    SDL_Quit(); // Close SDL subsystem.
}

// Clear SDL window to background configuration.
void clearWindow(const sdl_t sdl, const config_t config)
{
    const uint8_t r = (config.background_colour >> 24) & 0xFF;
    const uint8_t g = (config.background_colour >> 16) & 0xFF;
    const uint8_t b = (config.background_colour >> 8) & 0xFF;
    const uint8_t a = (config.background_colour >> 0) & 0xFF;

    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
    SDL_RenderClear(sdl.renderer);
}

// Update window with changes.
void updateScreen(const sdl_t sdl, const config_t config, const chip8_t chip8)
{
    SDL_Rect rect = {.x = 0, .y = 0, .w = config.scale_factor, .h = config.scale_factor};

    // get colour vals
    const uint8_t bg_r = (config.background_colour >> 24) & 0xFF;
    const uint8_t bg_g = (config.background_colour >> 16) & 0xFF;
    const uint8_t bg_b = (config.background_colour >> 8) & 0xFF;
    const uint8_t bg_a = (config.background_colour >> 0) & 0xFF;

    const uint8_t fg_r = (config.foreground_colour >> 24) & 0xFF;
    const uint8_t fg_g = (config.foreground_colour >> 16) & 0xFF;
    const uint8_t fg_b = (config.foreground_colour >> 8) & 0xFF;
    const uint8_t fg_a = (config.foreground_colour >> 0) & 0xFF;

    // loop through pixels, drawing rectangles for each
    for (uint32_t i = 0; i < sizeof chip8.display; i++)
    {
        // translate 1d index i to 2d x/y coords
        // x = i % window width
        // y = i / window width

        rect.x = (i % config.window_width) * config.scale_factor;
        rect.y = (i / config.window_width) * config.scale_factor;

        if (chip8.display[i])
        {
            // if on, draw foreground colour
            SDL_SetRenderDrawColor(sdl.renderer, fg_r, fg_g, fg_b, fg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);

            // if user requests pixel outlines, draw here:
            if (config.pixel_outlines)
            {
                SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
                SDL_RenderDrawRect(sdl.renderer, &rect);
            }
        }
        else
        {
            // pixel is off draw background colour
            SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        }
    }

    SDL_RenderPresent(sdl.renderer);
}

// Handle user input.
void handleInput(chip8_t *chip8)
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_QUIT:
            // Exit window or end program.
            chip8->state = QUIT; // Will break the main emulator loop.
            return;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym)
            {
            case SDLK_ESCAPE:
                // Escape key: exit window, and end program.
                chip8->state = QUIT;
                return;
            case SDLK_SPACE:
                // Space bar
                if (chip8->state == RUNNING)
                    chip8->state = PAUSED; // pause
                else
                {
                    chip8->state = RUNNING;
                    puts("==== PAUSED ====");
                }
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

#ifdef DEBUG
void print_debug_info(chip8_t *chip8)
{
    printf("Address: 0x%04X | OpCode:0x%04X\nDesc: ",
           chip8->PC - 2, chip8->inst.opcode);
    switch ((chip8->inst.opcode >> 12) & 0x0F)
    {
    case 0x00:
        // subroutine at NN
        if (chip8->inst.NN == 0xE0)
        {
            // 0x00E0: clear screen
            printf("Clear screen\n\n");
        }
        else if (chip8->inst.NN == 0xEE)
        {
            // 0x00EE: return from subroutine
            // set program counter to last address on stack ("pop from stack")
            //  so next opcode will be pulled from that address
            printf("Return from subroutine to a new address 0x%04X\n\n",
                   *(chip8->stack_ptr - 1));
        }
        else
        {
            printf("Unimplemented OpCode.\n\n");
        }
        break;
    case 0x01:
        // 0x1NNN: jumps to address NNN
        printf("Jump to address NNN (0x%04X)\n\n", chip8->inst.NNN);
        break;
    case 0x02:
        // 0x2NNN: call subroutine at NNN
        *chip8->stack_ptr++ = chip8->PC; // store current address to return to on subroutine stack ("push on stack")
        chip8->PC = chip8->inst.NNN;     // set program counter to subroutine address to pull next opcode.
        break;
    case 0x03:
        // 0x3XNN: skip next instruction if VX == NN
        printf("Check if V%X (0x%02X) == NN (0x%02X). Skip next instruction if true.\n\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN);
        break;
    case 0x04:
        // 0x4XNN: skip next instruction if VX != NN
        printf("Check if V%X (0x%02X) != NN (0x%02X). Skip next instruction if true.\n\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN);
        break;
    case 0x05:
        // 0x5XY0: skip next instruction if VX == NN
        printf("Check if V%X (0x%02X) == V%X (0x%02X). Skip next instruction if true.\n\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y]);
        break;
    case 0x06:
        // 0x6XNN: sets VX to NN
        printf("Set register V%X to NN (0x%02X)\n\n", chip8->inst.X, chip8->inst.NN);
        break;
    case 0x07:
        // 0x7XNN: sets VX += NN
        printf("Set register V%X (0x%02X) to += NN (0x%02X). Result: %02X\n\n",
               chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN, chip8->V[chip8->inst.X] + chip8->inst.NN);
        break;
    case 0x08:
        switch (chip8->inst.N)
        {
        case 0:
            // 0x8XY0: sets VX = VY
            printf("Set register V%X = V%X (0x%02X)\n\n",
                   chip8->inst.X, chip8->inst.Y, chip8->V[chip8->inst.Y]);
            break;
        case 1:
            // 0x8XY1: sets VX |= VY
            printf("Set register V%X (0x%02X) |= V%X (0x%02X). Result: 0x%02X\n\n",
                   chip8->inst.X, chip8->V[chip8->inst.X],
                   chip8->inst.Y, chip8->V[chip8->inst.Y],
                   chip8->V[chip8->inst.X] | chip8->V[chip8->inst.Y]);
            break;
        case 2:
            // 0x8XY2: sets VX &= VY
            printf("Set register V%X (0x%02X) &= V%X (0x%02X). Result: 0x%02X\n\n",
                   chip8->inst.X, chip8->V[chip8->inst.X],
                   chip8->inst.Y, chip8->V[chip8->inst.Y],
                   chip8->V[chip8->inst.X] & chip8->V[chip8->inst.Y]);
            break;
        case 3:
            // 0x8XY3: sets VX ^= VY
            printf("Set register V%X (0x%02X) ^= V%X (0x%02X). Result: 0x%02X\n\n",
                   chip8->inst.X, chip8->V[chip8->inst.X],
                   chip8->inst.Y, chip8->V[chip8->inst.Y],
                   chip8->V[chip8->inst.X] ^ chip8->V[chip8->inst.Y]);
            break;
        case 4:
            // 0x8XY4: sets VX += VY, set VF to 1 if carry, and 0 when not
            printf("Set register V%X (0x%02X) += V%X (0x%02X), VF = 1 if carry. Result: 0x%02X, VF = %X\n\n",
                   chip8->inst.X, chip8->V[chip8->inst.X],
                   chip8->inst.Y, chip8->V[chip8->inst.Y],
                   chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y],
                   ((uint16_t)(chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]) > 255));
            break;
        case 5:
            // 0x8XY5: sets VX -= VY, set VF to 0 if borrow, and 1 when not
            printf("Set register V%X (0x%02X) -= V%X (0x%02X), VF = 0 if borrow. Result: 0x%02X, VF = %X\n\n",
                   chip8->inst.X, chip8->V[chip8->inst.X],
                   chip8->inst.Y, chip8->V[chip8->inst.Y],
                   chip8->V[chip8->inst.X] - chip8->V[chip8->inst.Y],
                   (chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y]));
            break;
        case 6:
            // 0x8XY6: sets VX >>= 1, stores shifted bit into VF.
            printf("Set register V%X (0x%02X) >>= 1, VF = shifted bit. Result: 0x%02X, VF = %X\n\n",
                   chip8->inst.X, chip8->V[chip8->inst.X],
                   chip8->V[chip8->inst.X] >> 1,
                   chip8->V[chip8->inst.X] & 1);
            break;
        case 7:
            // 0x8XY7: sets VX = VY - VX, set VF to 0 if borrow, and 1 when not
            printf("Set register V%X = V%X (0x%02X) - V%X (0x%02X), VF = 0 if borrow. Result: 0x%02X, VF = %X\n\n",
                   chip8->inst.X,
                   chip8->inst.X, chip8->V[chip8->inst.X],
                   chip8->inst.Y, chip8->V[chip8->inst.Y],
                   chip8->V[chip8->inst.X] - chip8->V[chip8->inst.Y],
                   (chip8->V[chip8->inst.Y] <= chip8->V[chip8->inst.X]));
            break;
        case 0xE:
            // 0x8XYE: sets VX <<= 1, stores shifted bit into VF.
            printf("Set register V%X (0x%02X) <<= 1, VF = shifted bit. Result: 0x%02X, VF = %X\n\n",
                   chip8->inst.X, chip8->V[chip8->inst.X],
                   chip8->V[chip8->inst.X] << 1,
                   (chip8->V[chip8->inst.X] & 0x80) >> 7);
            break;
        }
        break;
    case 0x09:
        // 0x9XY0: skip next instruction if VX != NN
        printf("Check if V%X (0x%02X) != V%X (0x%02X). Skip next instruction if true.\n\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y]);
        break;
    case 0x0A:
        // 0xANNN: set index register I to NNN
        printf("Set index register I to NNN (0x%04X)\n\n", chip8->inst.NNN);
        break;
    case 0x0D:
        // 0xDXYN: Draw sprite at coordinate (VX, VY), read from memory location I
        // sprite width 8, height N
        // screen pixels are XOR'd with sprite bits
        // VF (carry flag) set if any screen pixels are set off. Important for collision detection etc.
        printf("Drawing N (%u) height sprite at coords V%X (0x%02X), V%X (0x%02X) "
               "from memory location I (0x%04X).\nSet VF = 1 if any pixels are off.\n\n",
               chip8->inst.N, chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y], chip8->I);
        break;
    default:
        printf("Unimplemented OpCode.\n\n");
        break; // invalid opcode
    }
};
#endif

// Emulate chip8 instructions:
void emulateInstructions(chip8_t *chip8, const config_t config)
{
    // get next opcode from ram
    chip8->inst.opcode = chip8->ram[chip8->PC] << 8 | chip8->ram[chip8->PC + 1];
    chip8->PC += 2; // pre-increment pc to get next op code

    // fill out current instruction format
    //  DXYN
    chip8->inst.NNN = chip8->inst.opcode & 0x0FFF;
    chip8->inst.NN = chip8->inst.opcode & 0x0FF;
    chip8->inst.N = chip8->inst.opcode & 0x0F;
    chip8->inst.X = (chip8->inst.opcode >> 8) & 0x0F;
    chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x0F;

#ifdef DEBUG
    print_debug_info(chip8);
#endif

    // emulate opcode
    switch ((chip8->inst.opcode >> 12) & 0x0F)
    {
    case 0x00:
        // subroutine at NN
        if (chip8->inst.NN == 0xE0)
        {
            // 0x00E0: clear screen
            memset(chip8->display, false, sizeof chip8->display);
        }
        else if (chip8->inst.NN == 0xEE)
        {
            // 0x00EE: return from subroutine
            // set program counter to last address on stack ("pop from stack")
            //  so next opcode will be pulled from that address
            chip8->PC = *--chip8->stack_ptr;
        }
        else
        {
            // Unimplemented/invalid opcode, may be 0xNNN for calling machine code routine for RCA1802
        }
        break;

    case 0x01:
        // 0x1NNN: jumps to address NNN
        chip8->PC = chip8->inst.NNN; // set program counter so next opcode is from NNN.
        break;

    case 0x02:
        // 0x2NNN: call subroutine at NNN
        *chip8->stack_ptr++ = chip8->PC; // store current address to return to on subroutine stack ("push on stack")
        chip8->PC = chip8->inst.NNN;     // set program counter to subroutine address to pull next opcode.
        break;

    case 0x03:
        // 0x3XNN: skip next instruction if VX == NN
        if (chip8->V[chip8->inst.X] == chip8->inst.NN)
        {
            chip8->PC += 2; // skip next op code.
        }
        break;

    case 0x04:
        // 0x4XNN: skip next instruction if VX != NN
        if (chip8->V[chip8->inst.X] != chip8->inst.NN)
        {
            chip8->PC += 2; // skip next op code.
        }
        break;

    case 0x05:
        // 0x5XY0: skip next instruction if VX == VY
        if (chip8->inst.N != 0)
        {
            break; // wrong opcode
        }
        if (chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y])
        {
            chip8->PC += 2; // skip next op code.
        }
        break;

    case 0x06:
        // 0x6XNN: sets VX to NN
        chip8->V[chip8->inst.X] = chip8->inst.NN;
        break;

    case 0x07:
        // 0x7XNN: sets register VX += NN
        chip8->V[chip8->inst.X] += chip8->inst.NN;
        break;

    case 0x08:

        switch (chip8->inst.N)
        {
        case 0:
            // 0x8XY0: sets VX = VY
            chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];
            break;

        case 1:
            // 0x8XY1: sets VX |= VY
            chip8->V[chip8->inst.X] |= chip8->V[chip8->inst.Y];
            break;

        case 2:
            // 0x8XY2: sets VX &= VY
            chip8->V[chip8->inst.X] &= chip8->V[chip8->inst.Y];
            break;

        case 3:
            // 0x8XY3: sets VX ^= VY
            chip8->V[chip8->inst.X] ^= chip8->V[chip8->inst.Y];
            break;

        case 4:
            // 0x8XY4: sets VX += VY, set VF to 1 if carry, and 0 when not
            if ((uint16_t)(chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]) > 255)
            {
                chip8->V[0xF] = 1;
                chip8->V[chip8->inst.X] += chip8->V[chip8->inst.Y];
            }
            else
            {
                chip8->V[0xF] = 0;
                chip8->V[chip8->inst.X] += chip8->V[chip8->inst.Y];
            };
            break;

        case 5:
            // 0x8XY5: sets VX -= VY, set VF to 0 if borrow, and 1 when not
            if (chip8->V[chip8->inst.Y] > chip8->V[chip8->inst.X])
            {
                chip8->V[0xF] = 0;
                chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];
            }
            else
            {
                chip8->V[0xF] = 1;
                chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];
            }
            break;

        case 6:
            // 0x8XY6: sets VX >>= 1, stores shifted bit into VF.
            chip8->V[0xF] = chip8->V[chip8->inst.X] & 1;
            chip8->V[chip8->inst.X] >>= 1;

            break;

        case 7:
            // 0x8XY7: sets VX = VY - VX, set VF to 0 if borrow, and 1 when not
            if (chip8->V[chip8->inst.X] > chip8->V[chip8->inst.Y])
            {
                chip8->V[0xF] = 0;
                chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X];
            }
            else
            {
                chip8->V[0xF] = 1;
                chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X];
            }
            break;

        case 0xE:
            // 0x8XYE: sets VX <<= 1, stores shifted bit into VF.
            chip8->V[0xF] = (chip8->V[chip8->inst.X] & 0x80) >> 7;
            chip8->V[chip8->inst.X] <<= 1;
            break;

        default:
            break; // wrong op code
        }

        break;

    case 0x09:
        // 0x9XY0: skip next instruction if VX != VY
        if (chip8->inst.N != 0)
        {
            break; // wrong opcode
        }
        if (chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y])
        {
            chip8->PC += 2; // skip next op code.
        }
        break;

    case 0x0A:
        // 0xANNN: set index register I to NNN
        chip8->I = chip8->inst.NNN;
        break;

    case 0x0D:
        // 0xDXYN: Draw sprite at coordinate (VX, VY), read from memory location I
        // sprite width 8, height N
        // screen pixels are XOR'd with sprite bits
        // VF (carry flag) set if any screen pixels are set off. Important for collision detection etc.
        uint8_t X_pos = chip8->V[chip8->inst.X] % config.window_width;
        uint8_t Y_pos = chip8->V[chip8->inst.Y] % config.window_height;
        const uint8_t X_origin = X_pos;

        chip8->V[0xF] = 0; // init carry flag to 0.

        // loop over all N rows of sprite
        for (uint8_t i = 0; i < (chip8->inst.N); i++)
        {
            // get next byte
            const uint8_t sprite_data = chip8->ram[chip8->I + i];
            X_pos = X_origin; // reset X to draw next row

            for (int8_t j = 7; j >= 0; j--)
            {
                // if sprite pixel bit is on and display pixel is on, set carry flag.
                bool *pixel = &chip8->display[Y_pos * config.window_width + X_pos];
                const bool sprite_bit = (sprite_data & (1 << j));
                if (sprite_bit && *pixel)
                {
                    chip8->V[0xF] = 1;
                }

                // XOR display pixel with sprite pixel/bit to set it on or off
                *pixel ^= sprite_bit;

                // stop drawing if hit right edge of screen
                if (++X_pos >= config.window_width)
                    break;
            }
            // stop drawing whole sprite if at bottom edge of screen
            if (++Y_pos >= config.window_height)
                break;
        }
        break;

    default:
        break; // invalid opcode
    }
}

// MAIN FUNC
int main(int argc, char **argv)
{
    // default usage message for args
    if (argc < 2)
    {
        fprintf(stderr, "\nNo ROM selected.\nCorrect Usage: %s <rom_name>\n\n", argv[0]);
        exit(EXIT_FAILURE);
    }

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

    // Initialise CHIP-8 machine.
    chip8_t chip8 = {0};
    const char *rom_name = argv[1];
    if (!initCHIP(&chip8, rom_name))
        exit(EXIT_FAILURE);

    // Initial screen clear to background configuration.
    clearWindow(sdl, config);

    // Main emulator loop
    while (chip8.state != QUIT)
    {
        // Handle user inputs.
        handleInput(&chip8);

        // if paused continue.
        if (chip8.state == PAUSED)
            continue;

        // Get time()
        // Emulate CHIP8 instructions
        emulateInstructions(&chip8, config);
        // Get time changed from last get time()

        // Delay for 60FPS!
        SDL_Delay(16.67); //-actual time elapsed!!!!!!!!!!!!!!!!!
        // Update window with changes.
        updateScreen(sdl, config, chip8);
    }

    // Final cleanup before interpreter exit.
    finalCleanUp(&sdl);

    exit(EXIT_SUCCESS);
}