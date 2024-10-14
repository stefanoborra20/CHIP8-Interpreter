#ifndef EMU_H
#define EMU_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <SDL.h>

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;
} sdl_t;

typedef struct {
    uint32_t window_w, window_h;
    uint32_t fg_color, bk_color;
    uint32_t scale;
    bool pixel_outlines;
    uint32_t instr_per_frame;
    uint32_t square_wave_freq;
    int16_t volume;
} config_t;

typedef enum {
    RUNNING,
    PAUSE,
    QUIT,
} emu_state_t;

typedef struct {
    uint16_t opcode;
    uint16_t NNN;   // 12 bit address
    uint8_t NN;     // 8 bit const
    uint8_t N;      // 4 bit const
    uint8_t X;      // 4 bit
    uint8_t Y;      // 4 bit
} instruction_t;

typedef struct {
    emu_state_t state;
    uint8_t ram[0x1000];    // 4k
    bool display[64*32];
    uint16_t stack[12];
    uint16_t *stack_ptr;     // stack pointer
    uint8_t V[0xF];         // V0-VF
    uint16_t PC;            // 2 byte 
    uint16_t I;             // 12 bit (for mem op)
    uint8_t delay_timer;
    uint8_t sound_timer;
    bool keypad[0xF];
    char *rom_path;
} chip8_t;

// SDL functions
bool sdl_init();

void audio_callback();

bool sdl_quit();

bool clear_screen();

bool update_screen();

void user_input();

// Configuration functions
bool config_init(int argc, char **argv);

uint32_t get_instr_per_frame();

// Chip8 functions
bool chip8_init(char *rom_path);

emu_state_t get_chip8_state();

void execute_instruction();

void update_timers();

#endif
