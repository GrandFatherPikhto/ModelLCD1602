#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <termios.h>

#include "menu.h"
#include "console.h"

#define MENU_ITEMS_SIZE 0x10
#define MENU_TITLE_SIZE 0x10

#define MENU_FLAG_EDIT 0x01


static void s_rotary_encoder_callback (uint32_t current);
static void s_push_button_callback    (void);
static void s_display_menu            (void);

/** @typedef Функция обратного вызова элемента меню
 *  @brief 
 */
typedef void (*menu_item_callback_t) (void);

/** @brief структура для хранения предыдущего, текущего и следующего значения rotary encoder 
 * 
 */
typedef struct {
    uint32_t current;
    uint32_t prev;
    int16_t delta;
} rotenc_data_t;

static rotenc_current_t rotenc_current = {0};

typedef struct _menu_item_t {
    char *title;
    struct _menu_item_t *prev;
    struct _menu_item_t *next;
    struct _menu_item_t *parent;
    struct _menu_item_t *child;
    menu_item_callback_t callback;
} menu_item_t;

static menu_item_t s_menu_start;
static menu_item_t s_menu_test;
static menu_item_t s_menu_options;

static menu_item_t s_menu_main_back;
static menu_item_t s_menu_options_back;

static menu_item_t s_menu_pwm;
static menu_item_t s_menu_pwm_back;
static menu_item_t s_menu_pwm_enable;
static menu_item_t s_menu_pwm_freq;

static menu_item_t s_menu_lo;
static menu_item_t s_menu_lo_back;
static menu_item_t s_menu_lo_enable;
static menu_item_t s_menu_lo_delay;
static menu_item_t s_menu_lo_duration;

static menu_item_t s_menu_hi;
static menu_item_t s_menu_hi_back;
static menu_item_t s_menu_hi_enable;
static menu_item_t s_menu_hi_delay;
static menu_item_t s_menu_hi_duration;


static menu_item_t s_menu_start        = {"Start",    &s_menu_options,      &s_menu_test,    NULL,    NULL, NULL};
static menu_item_t s_menu_test         = {"Test",     &s_menu_start,        &s_menu_options, NULL,    NULL, NULL};
static menu_item_t s_menu_options      = {"Options",  &s_menu_test,         &s_menu_start,   NULL,    &s_menu_options_back, NULL};

static menu_item_t s_menu_options_back = {"Back",     &s_menu_hi,           &s_menu_pwm,          &s_menu_options, NULL, NULL};
static menu_item_t s_menu_pwm          = {"PWM",      &s_menu_options_back, &s_menu_lo,           NULL, &s_menu_pwm_back, NULL};
static menu_item_t s_menu_lo           = {"Lo Arm",   &s_menu_pwm,          &s_menu_hi,           NULL, &s_menu_lo_back, NULL};
static menu_item_t s_menu_hi           = {"Hi Arm",   &s_menu_lo,           &s_menu_options_back, NULL, &s_menu_hi_back, NULL};

static menu_item_t s_menu_pwm_back     = {"Back",     &s_menu_pwm_enable,   &s_menu_pwm_freq,     &s_menu_options_back, NULL, NULL};
static menu_item_t s_menu_pwm_enable   = {"Enable",   &s_menu_pwm_freq,     &s_menu_pwm_back,     NULL, NULL, NULL};
static menu_item_t s_menu_pwm_freq     = {"Freq",     &s_menu_pwm_back,     &s_menu_pwm_enable,   NULL, NULL, NULL};

static menu_item_t s_menu_lo_back      = {"Back",     &s_menu_lo_duration,  &s_menu_lo_enable,   &s_menu_options_back, NULL, NULL};
static menu_item_t s_menu_lo_enable    = {"Enable",   &s_menu_lo_back,      &s_menu_lo_delay,    NULL, NULL, NULL};
static menu_item_t s_menu_lo_delay     = {"Delay",    &s_menu_lo_enable,    &s_menu_lo_duration, NULL, NULL, NULL};
static menu_item_t s_menu_lo_duration  = {"Duration", &s_menu_lo_duration,  &s_menu_lo_back,     NULL, NULL, NULL};

static menu_item_t s_menu_hi_back      = {"Back",     &s_menu_hi_duration,  &s_menu_hi_enable,   &s_menu_options_back, NULL, NULL};
static menu_item_t s_menu_hi_enable    = {"Enable",   &s_menu_hi_back,      &s_menu_hi_delay,    NULL, NULL, NULL};
static menu_item_t s_menu_hi_delay     = {"Delay",    &s_menu_hi_enable,    &s_menu_hi_duration, NULL, NULL, NULL};
static menu_item_t s_menu_hi_duration  = {"Duration", &s_menu_hi_duration,  &s_menu_hi_back,     NULL, NULL, NULL};

static menu_item_t *s_menu_current_item = &s_menu_start;

static void s_menu_set_current (int16_t delta);

void Menu_Init(void)
{
    // printf("Size: %lu, %0.2f\r\n", sizeof(s_menu_start), sizeof(s_menu_start) * 12.0 / 1024.0);
    s_display_menu();
    taskReadKey(s_rotary_encoder_callback, s_push_button_callback);
}

/**
 * @brief Сохраняет текущее и предыдущие значения позиций rotary encoder
 * @param current -- текущее, не обработанное значение rotary encoder
 * @note
 *      Так же, записывает и delta -- (разница между текущим и предыдущим значениями)
 *      current -- текущее значение rotary encoder. Если включён фильтр /2, /4 -- проверить, что
 *      кратно 2 или 4. И сдвига делать /2, /4... e.t.c.
 *      prev -- предыдущее значение rotary encoder.
 *      Когда находим разность, привести к типу int, чтобы не было проблем с uint
 */
static void s_rotary_encoder_callback (uint32_t current)
{
    if (current % 2 == 0) // Фильтр отработал правильно
    {
        rotenc_current.delta = (int)(current / 2) - (int)rotenc_current.current;
        rotenc_current.prev  = rotenc_current.current;
        rotenc_current.current += rotenc_current.delta;
        //printf("[0] %d, [1] %d, Delta: %d\r\n", s_renc_current.current, s_renc_current.prev, s_renc_current.delta);
        s_menu_set_current(rotenc_current.delta);
    }
}

static void s_menu_set_current (int16_t delta)
{
    if (delta > 0)
    {
        s_menu_current_item = s_menu_current_item->next;
    } else if (delta < 0)
    {
        s_menu_current_item = s_menu_current_item->prev;
    }

    s_display_menu();
}

static void s_push_button_callback (void)
{
    if (s_menu_current_item->child)
    {
        s_menu_current_item = s_menu_current_item->child;
    } else if (s_menu_current_item->parent)
    {
        s_menu_current_item = s_menu_current_item->parent;
    }
    s_display_menu();
}

static void s_display_menu(void)
{
    printf("\033[H\033[J");
    printf("> %s\r\n", s_menu_current_item->title);
    printf("%s\r\n", s_menu_current_item->next->title);
}