#include "console.h"

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <termios.h>

// Глобальная структура для предыдущих настроек терминала
struct termios orig_termios;


/**
 * @brief Считывает одиночное нажатие клавиши без ожидания Enter.
 * 
 * Эта функция изменяет режим работы терминала, чтобы получить возможность считывать нажатия
 * клавиш по одной, без буферизации и эхо-печати. После считывания символа, настройки
 * терминала возвращаются к изначальному состоянию.
 *
 * @return Возвращает символ, считанный с клавиатуры.
 */
int getKeyPress(void)
{
    struct termios oldt, newt;  // Структуры для хранения настроек терминала
    int ch;  // Переменная для хранения считанного символа

    tcgetattr(STDIN_FILENO, &oldt);  // Получаем текущие настройки терминала и сохраняем их в oldt
    newt = oldt;  // Копируем старые настройки в newt для дальнейших изменений
    newt.c_lflag &= ~(ICANON | ECHO);  // Выключаем канонический режим и эхо-вывод
                                       // ICANON - отключает канонический ввод, когда данные отправляются сразу
                                       // ECHO - отключает отображение введённых символов на экране

    tcsetattr(STDIN_FILENO, TCSANOW, &newt);  // Устанавливаем новые настройки терминала немедленно (TCSANOW)
    
    ch = getchar();  // Считываем одиночный символ с ввода без ожидания Enter

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);  // Восстанавливаем старые настройки терминала, чтобы вернуть
                                              // его в стандартный режим работы

    return ch;  // Возвращаем считанный символ
}

// Включение raw режима в терминале
void enableRawMode() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &orig_termios);
    raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Выключение raw режима и восстановление настроек терминала
void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

/**
 * @brief Обрабатывает нажатия клавиш для управления функциями колбэков энкодера и кнопки.
 * 
 * Эта функция непрерывно считывает символы с клавиатуры, вызывая соответствующие
 * колбэки в зависимости от полученного символа. Она завершает выполнение при
 * нажатии клавиши 'Esc'.
 *
 * @param rotary_encoder_callback_func Функция колбэка, вызываемая при изменении состояния энкодера.
 * @param push_button_callback_func Функция колбэка, вызываемая при нажатии кнопки.
 * 
 * @details
 * - На каждом шаге функция считывает символ с помощью `getKeyPress()`.
 * - Если введённый символ соответствует клавише 'Esc' ('\033'), цикла завершает выполнение.
 * - Если символ — это 'Enter' (значения 13 или 10), вызывается `push_button_callback_func`.
 * - Если символ — это начало управляющей последовательности ('\033'), далее анализируется,
 *   какой именно стрелкой закончилась последовательность:
 *   - 'A' — стрелка вверх: текущая переменная уменьшает значение на 2.
 *   - 'B' — стрелка вниз: текущая переменная увеличивается на 2.
 * - После определения изменения вызывается `rotary_encoder_callback_func` с 
 *   текущим значением переменной.
 */
void taskReadKey(rotary_encoder_callback_t rotary_encoder_callback_func, push_button_callback_t push_button_callback_func, long_push_buttont_callback_t long_push_button_callback_func) {
    uint32_t current = 0;
    char buf[3];

    while (1) {
        enableRawMode(); // Включаем raw режим

        ssize_t n = read(STDIN_FILENO, buf, 3);

        if (n == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        if (n == 1 && buf[0] == '\033') {  // Выход при нажатии клавиши Esc
            break;
        } else if (n == 1) { 
            switch (buf[0])
            {
                case 'd':
                case 'D':
                    long_push_button_callback_func ();
                    break;
                case 10:
                case 13:
                    push_button_callback_func      ();
                default:
                    break;
            }            
        } else if (n > 1 && buf[0] == '\033' && buf[1] == '[') { // Начало ESC последовательности
            switch (buf[2]) {
                case 'A': // Стрелка вверх
                    current -= 2;
                    rotary_encoder_callback_func(current);
                    break;
                case 'B': // Стрелка вниз
                    current += 2;
                    rotary_encoder_callback_func(current);
                    break;
                case 'C': // Стрелка вправо
                    // Обработка стрелки вправо (если требуется)
                    break;
                case 'D': // Стрелка влево
                    // Обработка стрелки влево (если требуется)
                    break;
                default:
                    break;
            }
        }

        disableRawMode(); // Выключаем raw режим перед выходом
    }
}

/**
 * @brief Выводит текстовое меню на экран, обновляя содержимое консоли.
 *
 * Функция очищает экран, затем выводит два переданных строковых параметра в качестве
 * пунктов меню, где первый пункт выделяется символом ">".
 *
 * @param str1 Строка, представляющая первый пункт меню, который будет выделен в интерфейсе.
 * @param str2 Строка, представляющая второй пункт меню.
 */
void printMenu(const char *str1, const char *str2)
{
    printf("\033[H\033[J"); // Экранированные последовательности ANSI для очистки экрана.
                            // \033[H - перемещает курсор в верхний левый угол экрана (1,1).
                            // \033[J - очищает экран от курсора до конца. Вместе это стирает весь экран.
    printf("Для выхода нажмите Esc\r\n");

    printf("> %s\r\n", str1); // Выводит первый пункт меню с символом ">", обозначающим его выбор или акцент.
                              // \r\n используется для перевода строки и возвращения каретки.
    printf("%s\r\n", str2);   // Выводит второй пункт меню без какого-либо выделения.
}
