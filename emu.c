#include "emu.h"

#define DEBUG

config_t config = {0};
sdl_t sdl = {0};
chip8_t chip8 = {0};

// SDL functions
void audio_callback(void *userdata, uint8_t *stream, int len) {
    config_t *config = (config_t *)userdata;
    uint16_t *audio_data = (uint16_t *) stream;

    uint32_t running_sample_index = 0;

    const int32_t square_wave_period = sdl.have.freq / config->square_wave_freq;
    const int32_t half_square_wave_period = square_wave_period / 2;

    for (int i = 0; i < len/2; i++) {
        // writing 2 byte at the time
        audio_data[i] = ((running_sample_index++ / half_square_wave_period) % 2) ?
                          config->volume : -config->volume;
    }
}

bool sdl_init() {
    srand(time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        SDL_Log("Can't initialize SDL: %s", SDL_GetError());
        return false;
    }

    // init window
    sdl.window = SDL_CreateWindow("Chip8 Emu", SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED,
                                    config.window_w * config.scale,
                                    config.window_h * config.scale,
                                    0);
    if (!sdl.window) {
        SDL_Log("Could not create SDL window: %s", SDL_GetError());
        return false;
    }
    
    // init renderer
    sdl.renderer = SDL_CreateRenderer(sdl.window, -1, SDL_RENDERER_ACCELERATED);
    if (!sdl.renderer) {
        SDL_Log("Could not create SDL renderer: %s", SDL_GetError());
        return false;
    }
    
    // init audio device
    sdl.want = (SDL_AudioSpec) {
        .freq = 44100, 
        .format = AUDIO_U8,
        .channels = 1,
        .samples = 4096,
        .callback = audio_callback,
        .userdata = &config,
    };

    sdl.dev = SDL_OpenAudioDevice(NULL, 0, &sdl.want, &sdl.have, 0);

    if (sdl.dev == 0) {
        SDL_Log("Could not get an Audio Device\n");
        return false;
    }

    return true;
}

bool sdl_quit() {
    SDL_DestroyRenderer(sdl.renderer);
    SDL_DestroyWindow(sdl.window);
    SDL_CloseAudioDevice(sdl.dev);
    SDL_Quit();
}

bool clear_screen() {
    const uint8_t r = (config.bk_color >> 24) & 0xFF;
    const uint8_t g = (config.bk_color >> 16) & 0xFF;
    const uint8_t b = (config.bk_color >> 8) & 0xFF;
    const uint8_t a = (config.bk_color >> 0) & 0xFF;

    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
    SDL_RenderClear(sdl.renderer);
}

bool update_screen() {
    SDL_Rect r = {
        .x = 0,
        .y = 0,
        .w = config.scale,
        .h = config.scale,
    };

    // get background color
    const uint8_t bk_r = (config.bk_color >> 24) & 0xFF;
    const uint8_t bk_g = (config.bk_color >> 16) & 0xFF;
    const uint8_t bk_b = (config.bk_color >> 8) & 0xFF;
    const uint8_t bk_a = (config.bk_color >> 0) & 0xFF;

    // get foreground color
    const uint8_t fg_r = (config.fg_color >> 24) & 0xFF;
    const uint8_t fg_g = (config.fg_color >> 16) & 0xFF;
    const uint8_t fg_b = (config.fg_color >> 8) & 0xFF;
    const uint8_t fg_a = (config.fg_color >> 0) & 0xFF;
    
    // draw 
    for (uint32_t i = 0; i < sizeof chip8.display; i++) {
        // translate 1D index i value to 2D x, y coords
        // x = i % window_w
        // y = i / window_h

        r.x = (i % config.window_w) * config.scale;
        r.y = (i / config.window_w) * config.scale;
        
        if (chip8.display[i]) {
            // if pixel is on, draw foreground color
            SDL_SetRenderDrawColor(sdl.renderer, fg_r, fg_g, fg_b, fg_a);
            SDL_RenderFillRect(sdl.renderer, &r);    

            // outlines (configurable)
            if (config.pixel_outlines) {
                SDL_SetRenderDrawColor(sdl.renderer, bk_r, bk_g, bk_b, bk_a);
                SDL_RenderDrawRect(sdl.renderer, &r);
            }

        } else {
            SDL_SetRenderDrawColor(sdl.renderer, bk_r, bk_g, bk_b, bk_a);
            SDL_RenderFillRect(sdl.renderer, &r);    
        }

    }

    SDL_RenderPresent(sdl.renderer);
}

void user_input() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                chip8.state = QUIT;
                break;
            
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_1: chip8.keypad[0x1] = true; break; 
                    case SDLK_2: chip8.keypad[0x2] = true; break; 
                    case SDLK_3: chip8.keypad[0x3] = true; break; 
                    case SDLK_4: chip8.keypad[0xC] = true; break; 

                    case SDLK_q: chip8.keypad[0x4] = true; break; 
                    case SDLK_w: chip8.keypad[0x5] = true; break; 
                    case SDLK_e: chip8.keypad[0x6] = true; break; 
                    case SDLK_r: chip8.keypad[0xD] = true; break; 

                    case SDLK_a: chip8.keypad[0x7] = true; break; 
                    case SDLK_s: chip8.keypad[0x8] = true; break; 
                    case SDLK_d: chip8.keypad[0x9] = true; break; 
                    case SDLK_f: chip8.keypad[0xE] = true; break; 

                    case SDLK_z: chip8.keypad[0xA] = true; break; 
                    case SDLK_x: chip8.keypad[0x0] = true; break; 
                    case SDLK_c: chip8.keypad[0xB] = true; break; 
                    case SDLK_v: chip8.keypad[0xF] = true; break;
                }
                break;

            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                    case SDLK_1: chip8.keypad[0x1] = false; break; 
                    case SDLK_2: chip8.keypad[0x2] = false; break; 
                    case SDLK_3: chip8.keypad[0x3] = false; break; 
                    case SDLK_4: chip8.keypad[0xC] = false; break; 

                    case SDLK_q: chip8.keypad[0x4] = false; break; 
                    case SDLK_w: chip8.keypad[0x5] = false; break; 
                    case SDLK_e: chip8.keypad[0x6] = false; break; 
                    case SDLK_r: chip8.keypad[0xD] = false; break; 

                    case SDLK_a: chip8.keypad[0x7] = false; break; 
                    case SDLK_s: chip8.keypad[0x8] = false; break; 
                    case SDLK_d: chip8.keypad[0x9] = false; break; 
                    case SDLK_f: chip8.keypad[0xE] = false; break; 

                    case SDLK_z: chip8.keypad[0xA] = false; break; 
                    case SDLK_x: chip8.keypad[0x0] = false; break; 
                    case SDLK_c: chip8.keypad[0xB] = false; break; 
                    case SDLK_v: chip8.keypad[0xF] = false; break;
                }
                break;

            default:
                break;
        }
    }
}

// configuration functions
bool config_init(int argc, char **argv) {

     // set default values
    config = (config_t){
        .window_w = 64,
        .window_h = 32,
        .fg_color = 0xFFFFFFFF,
        .bk_color = 0x00000000,
        .scale = 20, // default resolution is 1280x640
        .pixel_outlines = true,
        .instr_per_frame = 20,
        .square_wave_freq = 440,
        .volume = 3000,
    };

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-po", strlen("-po"))      == 0) config.pixel_outlines = (bool) strtoul(argv[++i], NULL, 10);
        if (strncmp(argv[i], "-s", strlen("-s"))        == 0) config.scale = (uint32_t) strtoul(argv[++i], NULL, 10);
        if (strncmp(argv[i], "-ipf", strlen("-ipf"))    == 0) config.instr_per_frame = (uint32_t) strtoul(argv[++i], NULL, 10);
        if (strncmp(argv[i], "-v", strlen("-v"))        == 0) config.volume = (int16_t) strtoul(argv[++i], NULL, 10);
    }

    return true;
}

uint32_t get_instr_per_frame() {
    return config.instr_per_frame;
}

// chip8 functions
bool chip8_init(char *rom_path) {
    const uint8_t font_set[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0,		// 0
	    0x20, 0x60, 0x20, 0x20, 0x70,		// 1
	    0xF0, 0x10, 0xF0, 0x80, 0xF0,		// 2
	    0xF0, 0x10, 0xF0, 0x10, 0xF0,		// 3
	    0x90, 0x90, 0xF0, 0x10, 0x10,		// 4
	    0xF0, 0x80, 0xF0, 0x10, 0xF0,		// 5
	    0xF0, 0x80, 0xF0, 0x90, 0xF0,		// 6
	    0xF0, 0x10, 0x20, 0x40, 0x40,		// 7
	    0xF0, 0x90, 0xF0, 0x90, 0xF0,		// 8
	    0xF0, 0x90, 0xF0, 0x10, 0xF0,		// 9
	    0xF0, 0x90, 0xF0, 0x90, 0x90,		// A
	    0xE0, 0x90, 0xE0, 0x90, 0xE0,		// B
	    0xF0, 0x80, 0x80, 0x80, 0xF0,		// C
	    0xE0, 0x90, 0x90, 0x90, 0xE0,		// D
	    0xF0, 0x80, 0xF0, 0x80, 0xF0,		// E
	    0xF0, 0x80, 0xF0, 0x80, 0x80		// F
    };
    
    // load font
    memcpy(&chip8.ram[0], &font_set, sizeof(font_set));

    // load rom
    FILE *rom_ptr = fopen(rom_path, "rb");
    if (!rom_ptr) {
        SDL_Log("Unable to open rom file: %s", rom_path);
        return false;
    }

#ifdef DEBUG
    printf("Loading ROM: %s\n", rom_path);
#endif

    // add some more error handling later
    fseek(rom_ptr, 0, SEEK_END);
    const size_t rom_size = ftell(rom_ptr);
    rewind(rom_ptr);

#ifdef DEBUG
    printf("ROM size: %zu bytes\n", rom_size);
#endif

    // read rom
    if (fread(&chip8.ram[0x200], rom_size, 1, rom_ptr) != 1) {
        SDL_Log("Could not load Rom into Ram");
        return false;
    }

    fclose(rom_ptr);

    chip8.state = RUNNING;
    chip8.stack_ptr = &chip8.stack[0];
    chip8.PC = 0x200;
    chip8.rom_path = rom_path;

    return true;
}


#ifdef DEBUG
void print_debug(instruction_t inst) {
    printf("Address: 0x%04X, Opcode: 0x%04X Desc: ", chip8.PC-2, inst.opcode);

    switch ((inst.opcode >> 12) & 0x0F) {
        case 0x0:
            if (inst.NN == 0xE0) {
                // clear display
                printf("Clear screen\n");                
            }
            else if (inst.NN == 0xEE) {
                // returns from a subroutine
                printf("Return from subroutine 0x%04X\n", *(chip8.stack_ptr - 1));
            }
            break;

        case 0x01:
            printf("Jump to NNN: 0x%04X\n", inst.NNN); 
            break;

        case 0x02:
            printf("Call subroutine at NNN: 0x%04X\n", inst.NNN);            
            break;

        case 0x03:
            printf("Check if V%X (0x%02X) == NN (0x%04X)\n", inst.X, chip8.V[inst.X], inst.NN);
            break;

        case 0x04:
            printf("Check if V%X (0x%02X) != NN (0x%04X)\n", inst.X, chip8.V[inst.X], inst.NN);
            break;

        case 0x05:
            printf("Check if V%X (0x%02X) == V%X (0x%02X)\n", inst.X, chip8.V[inst.X], inst.Y, chip8.V[inst.Y]);
            break;

        case 0x06:
            // sets VX to NN
            printf("Set V%X = NN (0x%02X)\n", inst.X, inst.NN);           
            break;
        
        case 0x07:
            printf("Set V%X += NN (0x%02X)\n", inst.X, inst.NN);
            break;

        case 0x08:
            printf("Bit operations\n");
            break;

        case 0x09:
            printf("Check if V%X (0x%02X) != V%X (0x%02X), is it is skip next line\n",
                    inst.X, chip8.V[inst.X], inst.Y, chip8.V[inst.Y]);
            break;

        case 0x0A:
            // sets I to NNN
            printf("Set I to NN (0x%04X)\n", inst.NN);
            break;

        case 0x0B:
            // Jump to PC = V0 + NNN
            printf("PC (0x%02X) = V0 (0x%2X) + NNN (0x%4X)\n", chip8.PC, chip8.V[0], inst.NNN);
            break;

        case 0x0C:
            printf("Set V%X to rand() & NN (0x%02X)\n", inst.X, inst.NN);
            break;

        case 0x0D:
            /* draw n-byte sprite starting at I at Vx, Vy
             * Xor sprite pixels and screen pixels
             * if any are erased set Vf = 1 otherwise Vf = 0
            */
            printf("Draw N (%u) height sprite at V%X (0x%02X), V%X (0x%02X)\n"
                    "from memory location I (0x%04X)\n",
                    inst.N, inst.X, chip8.V[inst.X], inst.Y, chip8.V[inst.Y], chip8.I);
            break;

        case 0x0E:
            if (inst.NN == 0x9E) {
                printf("Check if key stored in V%X is pressed (%d)\n", inst.X, chip8.keypad[chip8.V[inst.X]]);
            }
            else if (inst.NN == 0xA1){
                printf("Check if key stored in V%X is not pressed (%d)\n", inst.X, chip8.keypad[chip8.V[inst.X]]);
            }
            break;

        case 0x0F:
            switch(inst.NN) {
                case 0x07:
                    // set Vx = delay timer
                    printf("Set V%X = delay timer (0x%04X)\n", inst.X, chip8.delay_timer);
                    break;

                case 0x0A:
                    // store pressed key in Vx and system halted until a key is pressed
                    printf("Waiting for a key to be pressed\n");
                    break;

                case 0x15:
                    // sets delay timer to Vx
                    printf("Set delay timer = V%X (0x%02X)\n", inst.X, chip8.V[inst.X]);
                    break; 

                case 0x18:
                    // sets sound timer to Vx
                    printf("Set sound timer = V%X (0x%02X)\n", inst.X, chip8.V[inst.X]);
                    break; 

                case 0x1E:
                    // adds Vx to I
                    printf("Adding to V%X (0x%02X) += I (0x%02X)\n", inst.X, chip8.V[inst.X], chip8.I);
                    break;

                case 0x29:
                    // sets I to the location of the sprite stored at Vx
                    printf("Set I = V%X (0x%02X)\n", inst.X, chip8.V[inst.X]);
                    break;

                case 0x33:
                    // store BCD for Vx starting at I
                    printf("Store V%X BCD at I\n", inst.X);
                    break;

                case 0x55:
                    // dumps V0-Vx included to memory from I
                    printf("Dumping V0-V%X into memory at I (0x%02X)\n", inst.X, chip8.I);
                    break;

                case 0x65:
                    // load register V0-Vx included from memory starting at I
                    printf("Loading from I (0x%02X) into V0-V%X\n", chip8.I, inst.X);
                    break;

                default:
                    printf("Unimplemented 0xF Opcode\n");
                    break;
            }
            break;

        default:
            printf("Unimplemented\n");
            break; // undefined 
    }
}
#endif

void execute_instruction() {
    instruction_t inst;
    inst.opcode = chip8.ram[chip8.PC] << 8 | chip8.ram[chip8.PC + 1];
    chip8.PC += 2;

    inst.NNN = inst.opcode & 0x0FFF;
    inst.NN = inst.opcode & 0x0FF;
    inst.N = inst.opcode & 0x0F;
    inst.X = (inst.opcode >> 8) & 0x0F;
    inst.Y = (inst.opcode >> 4) & 0x0F;


#ifdef DEBUG
    print_debug(inst);
#endif

    switch ((inst.opcode >> 12) & 0x0F) {
        case 0x0:
            if (inst.NN == 0xE0) {
                // clear display
                memset(&chip8.display, false, sizeof(chip8.display));
            }
            else if (inst.NN == 0xEE) {
                // returns from a subroutine
                chip8.PC = *--chip8.stack_ptr;  
            }
            break;

        case 0x01:
            chip8.PC = inst.NNN;
            break;

        case 0x02:
            *chip8.stack_ptr++ = chip8.PC; // stores current PC on the stack
            chip8.PC = inst.NNN;
            break;

        case 0x03:
            if (chip8.V[inst.X] == inst.NN) chip8.PC += 2;
            break;

        case 0x04:
            if (chip8.V[inst.X] != inst.NN) chip8.PC += 2;
            break;

        case 0x05:
            if (chip8.V[inst.X] == chip8.V[inst.Y]) chip8.PC += 2;
            break;

        case 0x06:
            // sets VX to NN
            chip8.V[inst.X] = inst.NN;
            break;
        
        case 0x07:
            chip8.V[inst.X] += inst.NN;
            break;

        case 0x08:
           switch(inst.N) {
               case 0:
                   chip8.V[inst.X] = chip8.V[inst.Y];
                   break;

               case 1:
                   chip8.V[inst.X] |= chip8.V[inst.Y];
                   break;

               case 2:
                   chip8.V[inst.X] &= chip8.V[inst.Y];
                   break;

               case 3:
                   chip8.V[inst.X] ^= chip8.V[inst.Y];
                   break;

               case 4:
                   if ((uint16_t)chip8.V[inst.X] + (uint16_t)chip8.V[inst.Y] > 255) chip8.V[0xF] = 1;
                   chip8.V[inst.X] += chip8.V[inst.Y];
                   break;

               case 5:
                   if ((uint16_t)chip8.V[inst.X] >= (uint16_t)chip8.V[inst.Y]) {
                       chip8.V[0xF] = 1;
                   }
                   else {
                        chip8.V[0xF] = 0;
                   }

                   chip8.V[inst.X] -= chip8.V[inst.Y];
                   break;
               
               case 6:
                   // shift Vx by 1 and store shifted bit in Vf
                   chip8.V[0xF] = chip8.V[inst.X] & 1;
                   chip8.V[inst.X] >>= 1;
                   break;

               case 7:
                   if ((uint16_t)chip8.V[inst.Y] >= (uint16_t)chip8.V[inst.X]) {
                       chip8.V[0xF] = 1;
                   }
                   else {
                        chip8.V[0xF] = 0;
                   }

                   chip8.V[inst.X] = chip8.V[inst.Y] - chip8.V[inst.X];
                   break;

               case 0xE:
                   chip8.V[0xF] = (chip8.V[inst.X] & 0x80) >> 7;
                   chip8.V[inst.X] <<= 1;
                   break;

               default:
                   break;
           }
           break;

        case 0x09:
           if (chip8.V[inst.X] != chip8.V[inst.Y]) chip8.PC += 2; 
           break;

        case 0x0A:
            // sets I to NNN
            chip8.I = inst.NNN;
            break;

        case 0x0B:
            // Jump to PC = V0 + NNN
            chip8.PC = chip8.V[0x0] + inst.NNN;
            break;

        case 0x0C:
            // Vx = rand() & NN
            chip8.V[inst.X] = (rand() % 256) & inst.NN;
            break;
            
        case 0x0D:
            /* draw n-byte sprite starting at I at Vx, Vy
             * Xor sprite pixels and screen pixels
             * if any are erased set Vf = 1 otherwise Vf = 0
            */
            
            uint8_t x = chip8.V[inst.X] % config.window_w;
            uint8_t y = chip8.V[inst.Y] % config.window_h;
            const uint8_t original_x = x;
            
            chip8.V[0xF] = 0;

            // loop N rows of the sprite
            for (uint8_t i = 0; i < inst.N; i++) {
                // get next byte
                uint8_t sprite_data = chip8.ram[chip8.I + i];
                x = original_x;

                for (int8_t j = 7; j >= 0; j--) {
                    // if sprite pixel and display pixel are on, set carry flag
                    bool *screen_px = &chip8.display[y * config.window_w + x];
                    const bool sprite_bit = (sprite_data & (1 << j));

                    if (sprite_bit && *screen_px) {
                        chip8.V[0xF] = 1;
                    }

                    // xor display pixel with sprite pixel to set it on or off
                    *screen_px ^= sprite_bit;

                    // stop drawing if it hits right edge of screen
                    if (++x >= config.window_w) break;
                }

                // stop drawing entire sprite if it hits bottom edge
                if (++y >= config.window_h) break;
            }
            break;


        case 0x0E:
            if (inst.NN == 0x9E) {
                if (chip8.keypad[chip8.V[inst.X]]) chip8.PC += 2;
            }
            else if (inst.NN == 0xA1) {
                if (!chip8.keypad[chip8.V[inst.X]]) chip8.PC += 2;
            }
            break;

        case 0x0F:
            switch(inst.NN) {
                case 0x07:
                    // set Vx = delay timer
                    chip8.V[inst.X] = chip8.delay_timer;
                    break;

                case 0x0A:
                    // store pressed key in Vx and system halted until a key is pressed
                    bool any_key_pressed = false;
                    int8_t key = -1;
                    for (uint8_t i = 0; i < sizeof(chip8.keypad); i++) {

                        if (chip8.keypad[i]) {
                            chip8.V[inst.X] = i;
                            key = i;
                            any_key_pressed = true;
                            break;
                        }
                    }
                    
                    if (!any_key_pressed) chip8.PC -= 2;
                    else if (chip8.keypad[key]) chip8.PC -=2;
                    else {
                        chip8.V[inst.X] = key;
                        key = -1;
                    }
                    break;

                case 0x15:
                    // sets delay timer to Vx
                    chip8.delay_timer = chip8.V[inst.X];
                    break; 

                case 0x18:
                    // sets sound timer to Vx
                    chip8.sound_timer = chip8.V[inst.X];
                    break;

                case 0x1E:
                    // adds Vx to I
                    chip8.I += chip8.V[inst.X];
                    break;

                case 0x29:
                    // sets I to the location of the sprite stored at Vx
                    chip8.I = chip8.V[inst.X] * 5;
                    break;

                case 0x33:
                    // store BCD for Vx starting at I
                    chip8.ram[chip8.I]      = (chip8.V[inst.X] % 1000) / 100;   // hundreds digit
                    chip8.ram[chip8.I + 1]  = (chip8.V[inst.X] % 100) / 10;     // tens digit
                    chip8.ram[chip8.I + 2]  = (chip8.V[inst.X] % 10);           // ones digit                    
                    break;

                case 0x55:
                    // dumps V0-Vx included to memory from I
                    for (uint8_t i = 0; i < inst.X; i++)
                        chip8.ram[chip8.I + i] = chip8.V[i];
                    break;

                case 0x65:
                    // load register V0-Vx included from memory starting at I
                    for (uint8_t i = 0; i < inst.X; i++)
                        chip8.V[i] = chip8.ram[chip8.I + i];
                    break;

                default:
                    break;
            }
            break;

        default:
            break; // unimplemented
    }

}


void update_timers() {
    if (chip8.delay_timer > 0) chip8.delay_timer--;

    if (chip8.sound_timer > 0) {
        chip8.sound_timer--;
        SDL_PauseAudioDevice(sdl.dev, 0); // play sound
    }
    else {
        SDL_PauseAudioDevice(sdl.dev, 1); // pause sound
    }
}

emu_state_t get_chip8_state() {
    return chip8.state;
}

