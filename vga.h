#ifndef VGA_H
#define VGA_H

#define VIDEO_INT 0x10
#define SET_MODE 0x00
#define VGA_256_COLOR_MODE 0x13
#define VGA_TEXT_MODE 0x03

#define INPUT_STATUS_1      0x03da
#define VRETRACE            0x08

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200
#define NUM_COLORS 256

#define pixel(x,y) graphic_buffer[(y<<8)+(y<<6)+x]

typedef unsigned char byte;

extern byte graphic_buffer[64000L];

void wait_retrace();

void show_buffer();

void set_mode(byte mode);

void setup_palette(byte *unshaded_colors);

#endif