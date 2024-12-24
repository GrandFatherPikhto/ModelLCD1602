#include <stdint.h>

#ifndef __CONSOLE_H
#define __CONSOLE_H

typedef void (* rotary_encoder_callback_t) (uint32_t current);
typedef void (* push_button_callback_t) (void);

int getKeyPress(void);
void printMenu(const char *str1, const char *str2);
void taskReadKey(rotary_encoder_callback_t, push_button_callback_t);

#endif //__CONSOLE_H