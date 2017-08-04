#ifndef KEYB_H
#define KEYB_H

void init_keyboard(void);

void deinit_keyboard(void);

int update_keystates(int keystates, int* key_map, int key_map_size);

#endif