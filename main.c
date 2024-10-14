#include <stdint.h>
#include <stdbool.h>
#include "emu.h"

int main(int argc, char **argv) {
    if (argc < 2) {
       fprintf(stderr, "To launch a game: %s rom/file/path -flags\n", argv[0]);
       exit(EXIT_FAILURE);
    }

    if (!config_init(argc, argv) != 0) exit(EXIT_FAILURE);

    if (!sdl_init()) exit(EXIT_FAILURE);

    if (!chip8_init(argv[1])) exit(EXIT_FAILURE);
    
    clear_screen();

    int64_t last_ticks = SDL_GetTicks();
    int64_t last_delta = 0;

    while (get_chip8_state() != 2) {
        user_input();
        
        last_delta = SDL_GetTicks() - last_ticks;
        last_ticks = SDL_GetTicks();
        
        for (uint32_t i = 0; i < get_instr_per_frame(); i++) {
            execute_instruction();
        }

        update_screen();
        update_timers();

        double elapsed_time = (double) (SDL_GetTicks() - last_ticks);

        if (elapsed_time < 16.67f)
            SDL_Delay(16.67f - elapsed_time);
        else
            SDL_Delay(0);
    }


    // quit SDL
    sdl_quit();     

    exit(EXIT_SUCCESS);
}
