#include <stdio.h>
#include <dos.h>

#include "mouse.h"

int mouse_on = 0;
int num_buttons = 0;

void init_mouse() {
	union REGS regs;
	regs.x.ax = 0;
	int86(0x33, &regs, &regs);
	mouse_on = regs.x.ax;
	num_buttons = regs.x.bx;
}

void cursor_pos(int* x, int* y, int* buttons) {
	union REGS regs;
	regs.x.ax = 3;
	int86(0x33, &regs, &regs);
	*x = regs.x.cx >> 1;
	*y = regs.x.dx;
	*buttons = regs.x.bx;
}