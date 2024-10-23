#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL.h>

// Emulator configuration
typedef struct {
    uint32_t window_width;
    uint32_t window_height;
    uint32_t bg_color;      // Background color RGBA
    uint32_t fg_color;      // Foreground color RGBA
    uint32_t scale_factor;  // Amount to scale a CHIP8 pixel
    bool pixel_outlines;    // Sets pixel outlines
} config_t;

// Emulator states
typedef enum {
    QUIT,
    RUNNING,
    PAUSED,
} emulator_state_t;

// CHIP8 instruction format
typedef struct {
    uint16_t opcode;
    uint16_t NNN;       // 12 bit address/constant
    uint8_t NN;         // 8 bit constant
    uint8_t N;          // 4 bit constant
    uint8_t X;          // 4 bit register identifier
    uint8_t Y;          // 4 bit register identifier

} instruction_t;

// CHIP8 machine 
typedef struct {
    emulator_state_t state;
    uint8_t ram[4096];      // Random access memory
    bool display[64*32];    // Original CHIP8 resolution 
    uint16_t stack[12];     // Subrutine stacks
    uint16_t *stack_ptr;    // Stack pointer
    uint8_t V[16];          // Registers V0 - VF
    uint16_t I;             // Index memory register
    uint16_t PC;            // Program counter
    uint8_t delay_timer;    // Decrements at 60Hz
    uint8_t sound_timer;    // Decrements at 60Hz and plays a tone when > 0
    bool keypad[16];        // Hexadecimal keypad 0x0 - 0xF
    const char *rom_name;   // Currently running ROM
    instruction_t inst;     // Currently executing instruction
} chip8_t;

// SDL container object
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
} sdl_t;

bool init_chip8(chip8_t *chip8, const char *rom_name) {
    // CHIP8 roms will be loaded at 0x200
    const uint16_t entry_point = 0x200; 

    // Font specified for CHIP8 display
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

    // Load font
    memcpy(&chip8->ram[0], font, sizeof(font));

    // Load ROM
    FILE *rom = fopen(rom_name, "rb");

    if(!rom) {
        SDL_Log("Rom file %s is invalid or does not exist\n", rom_name);
        return false;
    }

    // Get ROM size
    fseek(rom, 0, SEEK_END);
    const size_t rom_size = ftell(rom); 
    const size_t max_size = sizeof chip8->ram - entry_point;
    rewind(rom);

    if(rom_size > max_size) {
        SDL_Log("Rom file %s is too big, Rom size: %zu, Max size allowed: %zu\n", rom_name, rom_size, max_size);
        return false;
    }

    if(fread(&chip8->ram[entry_point], rom_size, 1, rom) != 1) {
        SDL_Log("Could not read ROM file %s into CHIP8 memory", rom_name);
        return false;
    };

    fclose(rom);

    // Set CHIP8 machine defaults
    chip8->state = RUNNING;
    chip8->PC = entry_point; 
    chip8->rom_name = rom_name;
    chip8->stack_ptr = &chip8->stack[0];

    return true;
}

bool init_config(config_t *config, int argc, char **argv) {

    // Set defaults
    *config = (config_t) {
        .window_width = 64,
        .window_height = 32,
        .bg_color = 0x000000FF,
        .fg_color = 0xFFFFFFFF,
        .scale_factor = 20,
        .pixel_outlines = false,
    };

    //Override defaults from arguments
    for (int i = 1; i < argc; i++) {
        (void)argv[i]; // Prevent complier error
    }

    return true;
}

bool init_sdl(sdl_t *sdl, config_t *config) {

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        SDL_Log("Could not initalize SDL subsystems! %s\n", SDL_GetError());
        return false;
    }

    sdl->window = SDL_CreateWindow("CHIP8 Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                    config->window_width * config->scale_factor,
                                    config->window_height * config->scale_factor,
                                    0); 
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

void final_cleanup(sdl_t sdl) {
    SDL_DestroyRenderer(sdl.renderer);
    SDL_DestroyWindow(sdl.window);
    SDL_Quit();
}

void clear_screen(const sdl_t sdl, const config_t config) {
    const uint8_t r = (config.bg_color >> 24) & 0xFF;
    const uint8_t g = (config.bg_color >> 16) & 0xFF;
    const uint8_t b = (config.bg_color >>  8) & 0xFF;
    const uint8_t a = (config.bg_color >>  0) & 0xFF;

    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
    SDL_RenderClear(sdl.renderer);
}

void update_screen(const sdl_t sdl, const config_t config, const chip8_t chip8) {
    
    // Rectangle to be drawn
    SDL_Rect rect = {.x = 0, .y = 0, .w = config.scale_factor, .h = config.scale_factor};
    
    // Grab color values to draw
    const uint8_t bg_r = (config.bg_color >> 24) & 0xFF;
    const uint8_t bg_g = (config.bg_color >> 16) & 0xFF;
    const uint8_t bg_b = (config.bg_color >>  8) & 0xFF;
    const uint8_t bg_a = (config.bg_color >>  0) & 0xFF;

    const uint8_t fg_r = (config.fg_color >> 24) & 0xFF;
    const uint8_t fg_g = (config.fg_color >> 16) & 0xFF;
    const uint8_t fg_b = (config.fg_color >>  8) & 0xFF;
    const uint8_t fg_a = (config.fg_color >>  0) & 0xFF;

    for(uint32_t i = 0; i < sizeof chip8.display; i++) {
        // Translate 1D index i value to 2D X/Y coordinates
        // X = i % window width
        // Y = i / window width
        rect.x = (i % config.window_width) * config.scale_factor;
        rect.y = (i / config.window_width) * config.scale_factor;

        if(chip8.display[i]) {
            // Pixel is on, draw foreground color
            SDL_SetRenderDrawColor(sdl.renderer, fg_r, fg_g, fg_b, fg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);

            // If user requested drawing pixel outlines, draw those here 
            if(config.pixel_outlines){
                SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
                SDL_RenderDrawRect(sdl.renderer, &rect);
            }
        } else {
            // Pixel is off, draw background color
            SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        }
    }

    SDL_RenderPresent(sdl.renderer);
}

void handle_input(chip8_t *chip8){
    SDL_Event event;

    while(SDL_PollEvent(&event)) {

        switch(event.type) {
        
            case SDL_QUIT:
                //Exit window. End program
                chip8->state = QUIT;
                return;

            case SDL_KEYDOWN:

                switch(event.key.keysym.sym) {

                    case SDLK_ESCAPE:
                        chip8->state = QUIT;
                        return;

                    case SDLK_SPACE:
                        if(chip8->state == RUNNING) {
                            chip8->state = PAUSED;
                            puts("====PAUSED=====");
                        } else {
                            chip8->state = RUNNING;
                            puts("====RESUME=====");
                        } 
                        return;
                    
                }

            default:
                break;
        }
    }
}

#ifdef DEBUG
void print_debug_info(chip8_t *chip8) {

    printf("Address: 0x%04X Opcode: 0x%04X Desc: ", chip8->PC-2, chip8->inst.opcode);

    switch ((chip8->inst.opcode >> 12) & 0x0F) {
        case 0x0:
            if(chip8->inst.NN == 0xE0) {
                // 0x0E0 Clear the screen
                printf("Clear screen\n");

            } else if (chip8->inst.NN == 0xEE) {
                // 0x00EE Return from subrutine
                //  Set program counter to last address on subrutine stack ("pop" it off the stack)
                //  so that next opcode will be gotten from that address
                printf("Return from subrutine address 0x%04X\n", *(chip8->stack_ptr-1));
            } else {
                printf("Uninmplemented Opcode\n");
            }
            break;

        case 0x01:
            //0x1NNN: Jump to the adress NNN
            printf("Jump to addres NNN (0x%04X)\n", chip8->inst.NNN);
            break;

        case 0x02:
            // 0x02NNN: Call subrutine at NNN
            // Store current address to return to on subrutine stack ("push" it on the stack)
            // and set program counter to subrutine address so that the next opcode is gotten from there
            printf("Call subroutine at NNN (0x%04X)\n", chip8->inst.NNN);
            break;

        case 0x03:
            // If V[X] == NN, skip the next instruction
            printf("If V%X (0x%02X) == NN (0x%02X), skip the next instruction\n", 
                chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN);
            break;

        case 0x04:
            // If V[X] != NN, skip the next instruction
            printf("If V%X (0x%02X) != NN (0x%02X), skip the next instruction\n", 
                chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN);
            break;
            
        case 0x05:
            // If V[X] != NN, skip the next instruction
            printf("If V%X (0x%02X) == V%X (0x%02X), skip the next instruction\n", 
                chip8->inst.X, chip8->V[chip8->inst.X], 
                chip8->inst.Y, chip8->V[chip8->inst.Y]);
            break;

        case 0x06:
            // 0x6XNN: Set V[X] to NN
            printf("Set register V%X = NN (0x%02X)\n", chip8->inst.X, chip8->inst.NN);
            break;

        case 0x07:
            // 0x7XNN: Adds NN to V[X]
            printf("Set register V%X (0x%02X) += NN (0x%02X). Result: 0x%02X\n", 
            chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN, chip8->V[chip8->inst.X] +  chip8->inst.NN);
            break;

        case 0x08:
            switch(chip8->inst.N) {
                case 0x0:
                    // 0x8XY0: Set register V[X] = V[Y]
                    printf("Set register V%X = V%X (0x%02X)\n",
                    chip8->inst.X, chip8->inst.Y, chip8->V[chip8->inst.Y]);
                    break;

                case 0x1: 
                    // 0x8XY1: Set register V[X] |= V[Y]
                    printf("Set register V%X (0x%02X) |= V%X (0x%02X); Result: 0x%02X\n",
                    chip8->inst.X, chip8->V[chip8->inst.X], 
                    chip8->inst.Y, chip8->V[chip8->inst.Y],
                    chip8->V[chip8->inst.X] | chip8->V[chip8->inst.Y]);
                    break;
                
                case 0x2: 
                    // 0x8XY2: Set register V[X] &= V[Y]
                    printf("Set register V%X (0x%02X) &= V%X (0x%02X); Result: 0x%02X\n",
                    chip8->inst.X, chip8->V[chip8->inst.X], 
                    chip8->inst.Y, chip8->V[chip8->inst.Y],
                    chip8->V[chip8->inst.X] & chip8->V[chip8->inst.Y]);
                    break;

                case 0x3: 
                    // 0x8XY3: Set register V[X] ^= V[Y]
                    printf("Set register V%X (0x%02X) ^= V%X (0x%02X); Result: 0x%02X\n",
                    chip8->inst.X, chip8->V[chip8->inst.X], 
                    chip8->inst.Y, chip8->V[chip8->inst.Y],
                    chip8->V[chip8->inst.X] ^ chip8->V[chip8->inst.Y]);
                    break;

                case 0x4: 
                    // 0x8XY4: Set register V[X] += V[Y]; Set V[F] to 1 if carry
                    printf("Set register V%X (0x%02X) += V%X (0x%02X), V[0xF] = 1 if carry; Result: 0x%02X, V[0xF] = %X\n",
                    chip8->inst.X, chip8->V[chip8->inst.X], 
                    chip8->inst.Y, chip8->V[chip8->inst.Y],
                    chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y],
                    ((uint16_t) (chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]) > 255)); 
                    break;

                case 0x5:
                    // 0x8XY5: Set register V[X] -= V[Y]; Set V[F] to 1 if there is not a borrow
                    printf("Set register V%X (0x%02X) -= V%X (0x%02X), V[0xF] = 1 if there is not a borrow; Result: 0x%02X, V[0xF] = %X\n",
                    chip8->inst.X, chip8->V[chip8->inst.X], 
                    chip8->inst.Y, chip8->V[chip8->inst.Y],
                    chip8->V[chip8->inst.X] - chip8->V[chip8->inst.Y], 
                    (chip8->V[chip8->inst.Y] <= chip8->V[chip8->inst.X])); 
                    break;

                case 0x06:
                    //0x8XY6: Shifts V[X] to the right by 1, then stores the least significant bit of V[X] prior to the shift into V[F].
                    printf("Set register V%X (0x%02X) >>= 1  V[0xF] = shifted off bit (%X); Result: 0x%02X\n",
                    chip8->inst.X, chip8->V[chip8->inst.X], 
                    chip8->V[chip8->inst.X] & 0x1,
                    chip8->V[chip8->inst.X] >> 1); 
                    break;

                case 0x7:
                    // 0x8XY7: Set register V[X] = V[Y] - V[X]; Set V[F] to 1 if there is not a borrow
                    printf("Set register V%X (0x%02X) = V%X (0x%02X) - V%X (0x%02X), V[0xF] = 1 if there is not a borrow; Result: 0x%02X, V[0xF] = %X\n",
                    chip8->inst.X, chip8->V[chip8->inst.X], 
                    chip8->inst.Y, chip8->V[chip8->inst.Y],
                    chip8->inst.X, chip8->V[chip8->inst.X],
                    chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X], 
                    (chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y])); 
                    break;

                case 0xE:
                    //0x8XYE: Shifts V[X] to the left by 1, then stores the most significant bit of V[X] prior to the shift into V[F].
                    printf("Set register V%X (0x%02X) <<= 1  V[0xF] = shifted off bit (%X); Result: 0x%02X\n",
                    chip8->inst.X, chip8->V[chip8->inst.X], 
                    ((chip8->V[chip8->inst.X] & 0x80) >> 7),
                    (chip8->V[chip8->inst.X] << 1)); 
                    break;

                default:
                    break; // Uinmplemented/Wrong opcode 
            }
        break;

        case 0x0A:
            //  0x0ANN: Set index register I to NNN
            printf("Set I to NNN (0x%04X)\n", chip8->inst.NNN);
            break;

        case 0x0D:
            // 0XDXYN: Draw N-height sprite at coords X,Y; Read from memory location I
            // Screen pixels are XOR'd with sprite bits
            // VF (Carry flag) is set if any screen pixels are set off (This is useful for collision detection)
            printf("Draw N (%u) height sprite at coords V%X (0x%02X), V%X (0x%02X) "
                    "from memory location I (0x%04X). Set VF = 1 if any pixels are turned off.\n"
                    , chip8->inst.N, chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, 
                    chip8->V[chip8->inst.Y], chip8->I);
            break;

        default:
            printf("Uninmplemented Opcode\n");
            break; // Unimplemented or invalid opcode
    }
}
#endif

// Emulate CHIP8 instructions
void emulate_instruction(chip8_t *chip8, const config_t config) {
    bool carry; // Save carry flag/VF value for some instructions

    chip8->inst.opcode = (chip8->ram[chip8->PC] << 8) | chip8->ram[chip8->PC+1]; // Get next opcode form RAM
    chip8->PC += 2; // Increment program counter for next opcode

    // Fill out instruction format
    chip8->inst.NNN = chip8->inst.opcode & 0x0FFF;
    chip8->inst.NN = chip8->inst.opcode & 0x0FF;
    chip8->inst.N = chip8->inst.opcode & 0x0F;
    chip8->inst.X = (chip8->inst.opcode >> 8) & 0x0F;
    chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x0F;

#ifdef DEBUG
    print_debug_info(chip8);
#endif

    // Emulate opcode
    switch ((chip8->inst.opcode >> 12) & 0x0F) {

        case 0x0:
            if(chip8->inst.NN == 0xE0) {
                // 0x00E0: Clear the screen
                memset(&chip8->display[0], false, sizeof chip8->display);

            } else if (chip8->inst.NN == 0xEE) {
                // 0x00EE Return from subrutine
                //  Set program counter to last address on subrutine stack ("pop" it off the stack)
                //  so that next opcode will be gotten from that address
                chip8->PC = *--chip8->stack_ptr;

            } else {
                // Uninmplemented/invalid opcode, may be 0xNNN for calling machine code routines
            }

            break;

        case 0x01:
            //0x1NNN: Jump to the adress NNN
            chip8->PC = chip8->inst.NNN; //Set program counter so that next opcode is NNN.
            break;

        case 0x02:
            // 0x2NNN: Call subrutine at NNN
            // Store current address to return to on subrutine stack ("push" it on the stack)
            // and set program counter to subrutine address so that the next opcode is gotten from there
            *chip8->stack_ptr++ = chip8->PC;
            chip8->PC = chip8->inst.NNN;
            break;

        case 0x03:
            // If V[X] == NN, skip the next instruction
            if(chip8->V[chip8->inst.X] == chip8->inst.NN)
                chip8->PC += 2;

            break;
        
        case 0x04:
            // If V[X] != NN, skip the next instruction
            if(chip8->V[chip8->inst.X] != chip8->inst.NN)
                chip8->PC += 2;

            break;

        case 0x05:
            // If V[X] == V[Y], skip the next instruction
            if(chip8->inst.N != 0) break; // Wrong opocode

            if(chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y])
                chip8->PC += 2;
            
            break;

        case 0x06:
            // 0x6XNN: Set V[X] to NN
            chip8->V[chip8->inst.X] = chip8->inst.NN;
            break;

        case 0x07:
            // 0x7XNN: Adds NN to V[X]
            chip8->V[chip8->inst.X] += chip8->inst.NN;
            break;

        case 0x08:
            switch(chip8->inst.N) {
                case 0x0:
                    // 0x8XY0: Set register V[X] = V[Y]
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];
                    break;

                case 0x1: 
                    // 0x8XY1: Set register V[X] |= V[Y]
                    chip8->V[chip8->inst.X] |= chip8->V[chip8->inst.Y];
                    break;
                
                case 0x2: 
                    // 0x8XY2: Set register V[X] &= V[Y]
                    chip8->V[chip8->inst.X] &= chip8->V[chip8->inst.Y];
                    break;

                case 0x3: 
                    // 0x8XY3: Set register V[X] ^= V[Y]
                    chip8->V[chip8->inst.X] ^= chip8->V[chip8->inst.Y];
                    break;

                case 0x4: 
                    // 0x8XY4: Set register V[X] += V[Y]; Set V[F] to 1 if carry
                    carry = ((uint16_t) (chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]) > 255);

                    chip8->V[chip8->inst.X] += chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = carry; 
                    break;

                case 0x5:
                    // 0x8XY5: Set register V[X] -= V[Y]; Set V[F] to 1 if there is not a borrow
                    carry = (chip8->V[chip8->inst.Y] <= chip8->V[chip8->inst.X]);

                    chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = carry;
                    break;

                case 0x06:
                    //0x8XY6: Shifts V[X] to the right by 1, then stores the least significant bit of V[X] prior to the shift into V[F].
                    chip8->V[0xF] = chip8->V[chip8->inst.X] & 0x1;
                    chip8->V[chip8->inst.X] >>= 1;
                    break;

                case 0x7:
                    // 0x8XY7: Set register V[X] = V[Y] - V[X]; Set V[F] to 1 if there is not a borrow
                    carry = (chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y]);

                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X];
                    chip8->V[0xF] = carry;
                    break;

                case 0xE:
                    //0x8XYE: Shifts V[X] to the left by 1, then stores the most significant bit of V[X] prior to the shift into V[F].
                    chip8->V[0xF] = (chip8->V[chip8->inst.X] & 0x80) >> 7;
                    chip8->V[chip8->inst.X] <<= 1;
                    break;

                default:
                    break; // Uinmplemented/Wrong opcode 

            }
            break;

        case 0x0A:
            //  0xANNN: Set index register I to NNN
            chip8->I = chip8->inst.NNN;
            break;

        case 0x0D:
            // 0XDXYN: Draw N-height sprite at coords X,Y; Read from memory location I
            // Screen pixels are XOR'd with sprite bits
            // VF (Carry flag) is set if any screen pixels are set off (This is useful for collision detection)
            uint8_t X_coord = chip8->V[chip8->inst.X] % config.window_width;
            uint8_t Y_coord = chip8->V[chip8->inst.Y] % config.window_height;
            const uint8_t orig_X = X_coord;

            chip8->V[0xF] = 0; // Initialize carry flag to 0

            for(uint8_t i = 0; i < chip8->inst.N; i++) {
                // Get next byte/row of sprite data
                const uint8_t sprite_data = chip8->ram[chip8->I + i];
                X_coord = orig_X; // Reset X for next row to draw

                for(int8_t j = 7; j >= 0; j--) {
                    // If sprite pixel/bit is on and display pixel is on, set carry flag
                    bool *pixel = &chip8->display[Y_coord * config.window_width + X_coord];

                    const bool sprite_bit = (sprite_data & (1 << j));

                    if(sprite_bit && *pixel) {
                        chip8->V[0xF] = 1;
                    }

                    //XOR display pixel with sprite pixel/bit to set it off or  on
                    *pixel ^= sprite_bit;

                    // Stop drawing if hit right edge of the screen
                    if(++X_coord >= config.window_width) break;

                }

                // Stop drawing if hit bottom edge of the screen
                if(++Y_coord >= config.window_height) break;

            }

            break;

        default:
            break; // Unimplemented or invalid opcode
    }
}

int main(int argc, char **argv) {

    if(argc < 2) {
        fprintf(stderr, "Usage %s <rom_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    chip8_t chip8 = {0};
    const char *rom_name = argv[1];
    if(!init_chip8(&chip8, rom_name)) exit(EXIT_FAILURE);

    config_t config = {0}; 
    if(!init_config(&config, argc, argv)) exit(EXIT_FAILURE);

    sdl_t sdl = {0};
    if(!init_sdl(&sdl, &config)) exit(EXIT_FAILURE);

    clear_screen(sdl, config);

    // Main emulator loop
    while(chip8.state != QUIT) {
        handle_input(&chip8);

        if(chip8.state == PAUSED) continue;

        emulate_instruction(&chip8, config);
        
        // Delay for approximately 60Hz (16.67ms)
        SDL_Delay(16);

        update_screen(sdl, config, chip8);
    }

    final_cleanup(sdl);
    exit(EXIT_SUCCESS);
}