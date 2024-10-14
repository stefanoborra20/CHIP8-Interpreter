CC = gcc

CLFAGS=-std=c17 -Wall -Wextra -Werror

LDFLAGS = `sdl2-config --cflags --libs`

SRCS = main.c emu.c

all: chip8

chip8: $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o chip8 $(LDFLAGS) 

clean:
	rm -f chip8
