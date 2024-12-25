#include <stdint.h>

#ifndef __MENU_H__
#define __MENU_H__

#define MENU_ITEM_TITLE_LEN 0x10 ///< Максимальная длина строки элемента меню (16 символов)
#define MENU_SIZE           0x20 ///< Максимальное значение для размера меню (использутеся для статического массива)
#define ENCODER_INPUT_FILTER   2 ///< Значение фильтра Rotary Encode

#define MENU_STATIC_MEMORY  0 ///< Использовать статический массив
#define MENU_DYNAMIC_MEMORY 1 ///< Использовать динамический массив

#define MENU_USAGE_STATIC_MEMORY 1
#define MENU_USAGE_DYNAMIC_MEMORY 2

#if (MENU_STATIC_MEMORY != 0)
#undef  MENU_USAGE_MEMORY 
#define MENU_USAGE_MEMORY MENU_USAGE_STATIC_MEMORY
#elif (MENU_DYNAMIC_MEMORY != 0)
#undef  MENU_USAGE_MEMORY 
#define MENU_USAGE_MEMORY MENU_USAGE_DYNAMIC_MEMORY
#endif

#define MENU_FLAG_GOTO_PARENT 0x80
#define MENU_FLAG_EDIT_DATA   0x40
#define MENU_FLAG_GOTO_CHILD  0x20
#define MNUE_FLAG_GOTO_CBFUNC 0x10

void Menu_Init(void);

#endif // __MENU_H__