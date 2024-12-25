#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>

#include "menu.h"
#include "console.h"

/** @typedef Функция обратного вызова элемента меню
 *  @brief 
 */
typedef void (*menu_item_callback_t) (void);

/** 
 * @typedef rotenc_data_t
 * @brief структура для хранения предыдущего, текущего и следующего значения rotary encoder 
 */
typedef struct {
    uint32_t current; ///< Текущее значение, считанное с энкодера
    uint32_t prev;    ///< Предыдущее значение, считанное с энкодера
    int32_t  delta;   ///< Разница между текущим и предыдущим значениями энкодера. В отличие от беззнакового значения энкодера, хранится в знаковом виде
} rotenc_data_t;

/**
 * @typedef menu_item_t
 * @brief Структура, представляющая элемент меню.
 * 
 * Эта структура описывает элемент меню, являющийся частью связанной цепочки с поддержкой
 * навигации между элементами на одном уровне, возврата на родительский уровень, и перехода на
 * дочерний уровень. Также поддерживается выполнение функции обратного вызова.
 * 
 * @details
 * Особенности структуры:
 * - **Связанность элементов**: Меню представляет собой кольцевую двусвязную цепочку, где последний элемент
 *   связан с первым, обеспечивая цикличную навигацию. Аналогично, первый элемент связан с последним.
 * - **Уровни меню**: Каждый элемент может ссылаться на родительский или дочерний уровень меню:
 *   - **Родительский элемент**: Указывает на элемент верхнего уровня, позволяя возврат на предыдущий уровень, 
 *     если элемент такой указан.
 *   - **Дочерний элемент**: Указывает на элемент нижнего уровня, куда можно переместиться при поступлении
 *     соответствующего сигнала, например нажатия кнопки энкодера.
 * - **Функция обратного вызова**: Каждый элемент может иметь функцию, которая вызывается при взаимодействии
 *   с ним (например, при нажатии кнопки энкодера). Если функция определена, она имеет приоритетное право
 *   на выполнение перед навигацией.
 *
 * Применение структуры:
 * Используется в проектах, где требуется иерархическая организация меню, например, в системах
 * с пользовательским интерфейсом, основанным на ротационных энкодерах.
 * 
 * Пример:
 * ```c
 * menu_item_t item = {
 *     .title = "Settings",
 *     .prev = &amp;other_item,
 *     .next = &amp;another_item,
 *     .parent = &amp;main_menu,
 *     .child = &amp;settings_submenu,
 *     .callback = settings_callback
 * };
 * ```
 */
typedef struct _menu_item_t {
    char title[MENU_ITEM_TITLE_LEN]; ///< Заголовок пункта меню.
    struct _menu_item_t *prev;       ///< Указатель на предыдущий пункт меню (для навигации назад).
    struct _menu_item_t *next;       ///< Указатель на следующий пункт меню (для навигации вперёд).
    struct _menu_item_t *folowing;   ///< Указатель на следующий элемент для односвязного списка.
    struct _menu_item_t *parent;     ///< Указатель на родительский пункт меню. Определяет возврат на верхний уровень
    struct _menu_item_t *child;      ///< Указатель на дочерний пункт меню. Определяет переход на подменю (Дочерняя цепочка меню)
    menu_item_callback_t callback;   ///< Функция обратного вызова, выполняемая при взаимодействии с элементом
    uint32_t data;                   ///< Данные текущего пункта меню
    uint8_t flags;                   ///< Флаги для обработки при нажатии кнопки и т.д.
} menu_item_t;

/**
 * @typedef menu_handle_t
 * @brief Структура для управления и навигации по меню.
 *
 * Это внутренняя структура, которая используется для представления состояния и управления меню, 
 * состоящего из связанных элементов (цепочки). Она содержит указатели на первый, последний, 
 * родительский, текущий и стартовый элементы меню, что позволяет легко добавлять, удалять
 * и перемещаться по элементам меню. Также хранит состояние энкодера для взаимодействия с пользователем.
 */
typedef struct {
    rotenc_data_t rotenc;  ///< Структура, содержащая текущее состояние энкодера.
                           ///< Обеспечивает взаимодействие с меню путем считывания данных от поворотного энкодера.
    menu_item_t  *current; ///< Указатель на текущий активный элемент меню.
                           ///< Используется для отображения текущего состояния меню на дисплее и навигации пользователя.
    menu_item_t  *start;   ///< Указатель на стартовый элемент меню.
                           ///< Полезен для управления памятью и удаления всей цепочки меню при необходимости.
#if (MENU_USAGE_MEMORY == MENU_USAGE_STATIC_MEMORY)
    uint8_t       static_array_pos; ///< Используется в условиях статического распределения памяти.
                                    ///< Предоставляет индекс для работы с внутренним статическим массивом элементов меню.
#endif    
} menu_handle_t;

static menu_handle_t s_menu_handle; ///< Все текущие состояния, связанные с меню, состоянием энкодера и т.д.

#if (MENU_USAGE_MEMORY == MENU_USAGE_STATIC_MEMORY)
static menu_item_t s_menu_items[MENU_SIZE] = {0}; ///< Статический массив из которого берутся новые значения для элементов меню. Задействован, чтобы не использовать malloc
#endif

static void s_rotary_encoder_callback   (uint32_t current);
static void s_push_button_callback      (void);
static void s_display_menu              (void);
static void s_menu_position_handling    (void);
static void s_menu_init                 (void);
static menu_item_t * s_create_new_item  (void);

static menu_item_t * s_menu_add_item    (char *title, menu_item_t *parent, menu_item_callback_t callback, uint8_t flags);
static void s_menu_set_child            (menu_item_t *item, menu_item_t *child);
static void s_menu_rechain              (menu_item_t *parent);

static void s_long_push_button_callback (void);

#if (MENU_USAGE_MEMORY == MENU_USAGE_DYNAMIC_MEMORY)
static void s_menu_free_items           (void);
#endif

static void s_print_chain (menu_item_t *item)
{
    menu_item_t *first = item;
    while(item->next != first)
    {
        printf("%s\r\n", item->title);
        item = item->next;
    }
}

/**
 * @brief Пользовательская функция инициализации меню
 * @note 
 *      Здесь задаются пользовательские пункты меню
 *      Функция s_create_submenu запускает создание новой цепочки субменю с указанным parent
 *      s_create_submenu создаёт пункт с указанным title, нажатие на который возвращает в
 *      родительскую цепочку меню.
 *      Все пункты меню, созданные после вызова этой функции будут принадлежать этой цепочке
 *      Функция s_menu_add_item добавляет пункты субменю к цепочке созданной в s_create_submenu
 *      Если в самом начале s_create_submenu не была вызвана, создаётся корневой список пунктов
 *      меню.
 */
void Menu_Init(void)
{
    menu_item_t *menu_start    = s_menu_add_item ("Start",   NULL, NULL, 0);
    menu_item_t *menu_test     = s_menu_add_item ("Test",    NULL, NULL, 0);
    menu_item_t *menu_options  = s_menu_add_item ("Options", NULL, NULL, 0);

    menu_item_t *menu_opt_bck  = s_menu_add_item ("Back",   menu_options, NULL, MENU_FLAG_GOTO_PARENT);
    menu_item_t *menu_pwm      = s_menu_add_item ("PWM",    menu_options, NULL, 0);
    menu_item_t *menu_lo_arm   = s_menu_add_item ("Lo Arm", menu_options, NULL, 0);
    menu_item_t *menu_hi_arm   = s_menu_add_item ("Hi Arm", menu_options, NULL, 0);
    
    s_menu_set_child(menu_options, menu_opt_bck);

    menu_item_t *menu_pwm_back   = s_menu_add_item ("Back",      menu_pwm, NULL, MENU_FLAG_GOTO_PARENT);
    menu_item_t *menu_pwm_enable = s_menu_add_item ("Enable",    menu_pwm, NULL, 0);
    menu_item_t *menu_pwm_freq   = s_menu_add_item ("Frequency", menu_pwm, NULL, 0);
    
    s_menu_set_child(menu_pwm, menu_pwm_back);

    menu_item_t *menu_lo_arm_back     = s_menu_add_item ("Back",     menu_lo_arm, NULL, MENU_FLAG_GOTO_PARENT);
    menu_item_t *menu_lo_arm_enable   = s_menu_add_item ("Enable",   menu_lo_arm, NULL, 0);
    menu_item_t *menu_lo_arm_delay    = s_menu_add_item ("Delay",    menu_lo_arm, NULL, 0);
    menu_item_t *menu_lo_arm_duration = s_menu_add_item ("Duration", menu_lo_arm, NULL, 0);

    s_menu_set_child(menu_lo_arm, menu_lo_arm_back);

    menu_item_t *menu_hi_arm_back     = s_menu_add_item ("Back",     menu_hi_arm, NULL, MENU_FLAG_GOTO_PARENT);
    menu_item_t *menu_hi_arm_enable   = s_menu_add_item ("Enable",   menu_hi_arm, NULL, 0);
    menu_item_t *menu_hi_arm_delay    = s_menu_add_item ("Delay",    menu_hi_arm, NULL, 0);
    menu_item_t *menu_hi_arm_duration = s_menu_add_item ("Duration", menu_hi_arm, NULL, 0);

    s_menu_set_child(menu_hi_arm, menu_hi_arm_back);

    s_menu_init();
}

/**
 * @brief Инициализация системы меню.
 *
 * Эта функция подготавливает систему меню к работе, вызывая необходимые функции
 * для отображения, обработки событий ввода и управления памятью. После вызова
 * функции меню готово к взаимодействию с пользователем через заданный интерфейс.
 *
 * Функция выполняет следующие действия:
 * 
 * 1. `s_display_menu()` - актуализирует текущее отображение меню. Это может
 *    включать в себя перерисовку экранных элементов, отображение начального состояния
 *    меню или обновление данных о текущем выбранном элементе.
 * 
 * 2. `taskReadKey()` - регистрирует функции обратного вызова для обработки
 *    событий ввода от поворотного энкодера и кнопки. Это позволяет системе
 *    реагировать на взаимодействие пользователя, обеспечивая динамическую
 *    навигацию по меню.
 * 
 * 3. (Условно) `s_menu_free_items()` - освобождает динамически выделенную память
 *    для пунктов меню, если используется динамическое выделение памяти. Это важно
 *    для управления ресурсами и предотвращения утечек памяти в системах, где
 *    пункты меню могут варьироваться по структуре и количеству во время работы.
 *
 * @note Макрос `MENU_USAGE_MEMORY` контролирует использование динамической
 *       памяти. При `MENU_USAGE_DYNAMIC_MEMORY` память освобождается в
 *       процессе инициализации.
 *
 * @todo Рассмотреть возможность расширения функции дополнительными параметрами
 *       для настройки начального состояния меню или добавления логирования
 *       для отладки.
 */
static void s_menu_init (void)
{
    s_menu_handle.current = s_menu_handle.start;
    s_display_menu();
    taskReadKey(s_rotary_encoder_callback, s_push_button_callback, s_long_push_button_callback);
#if (MENU_USAGE_MEMORY == MENU_USAGE_DYNAMIC_MEMORY)
    s_menu_free_items ();
#endif    
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
    if ((current / ENCODER_INPUT_FILTER) * ENCODER_INPUT_FILTER != current) { // Дебаунсинг и Фильтрация
      return; // Неправильное значение, игнорировать
    }

    s_menu_handle.rotenc.delta = (int)(current / 2) - (int)s_menu_handle.rotenc.current;
    s_menu_handle.rotenc.prev  = s_menu_handle.rotenc.current;
    s_menu_handle.rotenc.current += s_menu_handle.rotenc.delta;
    
    if (s_menu_handle.current->callback != NULL)
    {
        s_menu_handle.current->callback();
    } else {
        s_menu_position_handling();
    }
}

/**
 * @brief Обрабатывает изменение позиции текущего элемента меню на основе изменения ротари энкодера.
 *
 * Эта функция отвечает за обновление текущей позиции в меню, в зависимости от значения
 * `delta`, предоставленного энкодером, который хранится в `s_menu_handle.rotenc`.
 * 
 * - Если `delta` положительное, функция перемещает текущий указатель на следующий элемент в меню.
 * - Если `delta` отрицательное, текущий указатель перемещается на предыдущий элемент в меню.
 * 
 * После обновления позиции вызывается функция `s_display_menu()`, чтобы обновить отображение меню
 * с учетом новой позиции. Эта функция должна вызываться всякий раз, когда нужно обработать
 * изменение позиции от ротари энкодера.
 *
 * @note Предполагается, что элементы меню связаны в циклический список, где у первого элемента
 * предшествующий указывает на последний, и наоборот, у последнего — следующий на первый.
 */
static void s_menu_position_handling (void)
{
    if (s_menu_handle.rotenc.delta > 0)
    {
        s_menu_handle.current = s_menu_handle.current->next;
    } 
    else if (s_menu_handle.rotenc.delta < 0)
    {
        s_menu_handle.current = s_menu_handle.current->prev;
    }

    s_display_menu();
}

/**
 * @brief Обрабатывает нажатие кнопки и смену текущего меню.
 *
 * Эта функция отвечает за обработку нажатия кнопки, изменяя текущее активное меню
 * в зависимости от того, доступен ли дочерний или родительский пункт меню. Функция
 * может использоваться в графическом интерфейсе пользователя или в системе
 * навигации по меню, которая управляется кнопками.
 * 
 * Поведение функции:
 * - Если у текущего элемента меню имеется дочерний элемент, функция переходит в
 *   этот дочерний элемент.
 * - Если дочерний элемент отсутствует, но имеется родительский элемент, происходит
 *   возврат к родительскому меню.
 * - Если ни дочерний, ни родительский элементы не заданы, текущий элемент меню
 *   остаётся без изменений.
 *
 * После изменения текущего элемента меню вызывается `s_display_menu()`, обновляющая
 * отображение для пользователя.
 *
 * @todo Добавить обработку callback для специфичной логики или действий при смене меню.
 */
static void s_push_button_callback (void)
{
    if (s_menu_handle.current->child && (s_menu_handle.current->flags & MENU_FLAG_GOTO_CHILD) == MENU_FLAG_GOTO_CHILD)
    {
        // Переход к дочернему элементу меню
        s_menu_handle.current = s_menu_handle.current->child;
    } 
    else if (s_menu_handle.current->parent && (s_menu_handle.current->flags & MENU_FLAG_GOTO_PARENT) == MENU_FLAG_GOTO_PARENT)
    {
        // Переход к родительскому элементу меню
        s_menu_handle.current = s_menu_handle.current->parent;
    }

    // Обновление отображения меню
    s_display_menu();
}

/**
 * @brief Отображение текущего элемента меню
 */
static void s_display_menu(void)
{
    printMenu(s_menu_handle.current->title, s_menu_handle.current->next->title);
}

/**
 * @brief Создаёт или получает новый элемент меню в зависимости от выбранного режима управления памятью.
 *
 * Эта функция отвечает за выделение памяти для нового пункта меню, используя
 * либо статический массив, либо динамическое выделение памяти, в зависимости от
 * текущих настроек программы. Она возвращает указатель на новый элемент меню,
 * готовый к дальнейшему заполнению данными.
 *
 * В зависимости от используемого режима памяти, функция работает следующим образом:
 * 
 * 1. **Статическая память** (`MENU_USAGE_STATIC_MEMORY`):
 *    - Используется заранее выделенный массив фиксированного размера для хранения элементов меню.
 *    - Если достигнуто максимальное количество элементов (`MENU_SIZE`), функция
 *      возвращает `NULL`, указывая на невозможность добавить новый элемент.
 *    - В противном случае возвращается указатель на следующий доступный элемент в массиве.
 * 
 * 2. **Динамическая память** (`MENU_USAGE_DYNAMIC_MEMORY`):
 *    - Выделяет память для нового элемента с использованием `malloc`.
 *    - Возвращает указатель на новосозданный элемент меню, импортируя работу с динамической памятью.
 *
 * @note Используйте вместе с функцией освобождения памяти, если работаете в режиме
 * динамической памяти, чтобы предотвратить утечки памяти.
 * 
 * @return Указатель на новый элемент меню или `NULL`, если выделение не удалось.
 */
static menu_item_t * s_create_new_item(void)
{
    menu_item_t *item = NULL;
#if (MENU_USAGE_MEMORY == MENU_USAGE_STATIC_MEMORY)
    // Проверка на исчерпание статического массива
    if (s_menu_handle.static_array_pos >= MENU_SIZE) {
        return NULL; // Нет больше места
    }
    item = &s_menu_items[s_menu_handle.static_array_pos++]; // Берём следующий доступный элемент
#elif (MENU_USAGE_MEMORY == MENU_USAGE_DYNAMIC_MEMORY)
    item = (menu_item_t *)malloc(sizeof(menu_item_t));
#endif
    return item;
}

/**
 * @brief Переинициализация цепочки подменю по родителю
 *
 * Эта функция перебирает список элементов меню и создаёт
 * циклический двусвязный список из элементов, имеющих одного родителя.
 * 
 * @param parent Указатель на родительский элемент меню, для которого
 * требуется переинициализировать подменю.
 */
static void s_menu_rechain (menu_item_t *parent)
{
    // Указатель на первый элемент в новом двусвязном списке
    menu_item_t *first = NULL;

    // Указатель на предыдущий элемент в новом двусвязном списке
    menu_item_t *prev  = NULL;

    // Указатель, с которого начинается обход текущего списка меню 
    menu_item_t *item  = s_menu_handle.start;

    // Перебираем все элементы в исходном списке
    while (item)
    {
        // Проверяем, принадлежит ли текущий элемент указанному родителю
        if (item->parent == parent)
        {
            // Если это первый элемент в подменю, запоминаем его
            if (first == NULL)
            {
                first = item;
            }

            // Если текущий элемент не NULL, обновляем указатель на предыдущий элемент
            if (item != NULL)
            {
                item->prev = prev;
            }

            // Устанавливаем указатель на следующий элемент предыдущего
            if (prev != NULL)
            {
                prev->next = item;
            }

            // Обновляем указатель предыдущего элемента для следующей итерации
            prev = item;
        }
        
        // Переходим к следующему элементу в списке
        item = item->folowing;
    }

    // После завершения цикла, соединяем первый и последний элементы, чтобы сделать список циклическим
    if (first)
    {
        first->prev = prev; // Замыкаем кольцо: первый элемент ссылается на последний
    }
    
    if (prev)
    {
        prev->next = first; // Замыкаем кольцо: последний элемент ссылается на первый
    }
}

/**
 * @brief Функция для добавления нового элемента меню
 *
 * Создаёт и инициализирует новый элемент меню, добавляет его в связный список 
 * элементов меню и переинициализирует цепочку подменю для указанного родителя.
 *
 * @param title Заголовок нового элемента меню.
 * @param parent Указатель на родительский элемент меню. Может быть NULL, если элемент без родителя.
 * @param callback Указатель на функцию обратного вызова, ассоциированную с этим элементом меню.
 * @param flags Флаги, определяющие параметры элемента меню.
 * @return Указатель на созданный элемент меню, или NULL, если создание не удалось.
 */
static menu_item_t* s_menu_add_item(char *title, menu_item_t *parent, menu_item_callback_t callback, uint8_t flags)
{
    // Создаём новый элемент меню с помощью вспомогательной функции s_create_new_item.
    menu_item_t *item = s_create_new_item();
    
    // Если создать элемент не удалось, возвращаем NULL.
    if (item == NULL)
        return NULL; // Ошибка создания нового элемента

    // Инициализация нового элемента меню.
    // Копируем заголовок в поле title. Количество копируемых символов ограничено MENU_ITEM_TITLE_LEN.
    strncpy(item->title, title, MENU_ITEM_TITLE_LEN);
    item->parent   = parent;   // Устанавливаем родительский элемент.
    item->child    = NULL;     // Пока у нового элемента нет дочерних элементов.
    item->flags    = flags;    // Устанавливаем флаги элемента.
    item->callback = callback; // Устанавливаем callback-функцию, если она есть.
    item->folowing = NULL;     // Следующий элемент в цепочке пока не определён.

    // Если в текущем контексте меню установлен текущий элемент...
    if (s_menu_handle.current) 
    {
        // Указываем, что после текущего элемента идёт только что созданный элемент.
        s_menu_handle.current->folowing = item;
        // Необходимо для отладки, чтобы увидеть, как связаны элементы (закомментировано).
        // printf("%s => %s\r\n", s_menu_handle.current->title, item->title);
    }

    // Устанавливаем созданный элемент как текущий элемент меню.
    s_menu_handle.current = item;

    // Если начальный элемент (стартовый) цепочки ещё не определён, устанавливаем созданный элемент.
    if (s_menu_handle.start == NULL)
    {
        s_menu_handle.start = item;
    }

    // Переинициализируем цепочку подменю для указанного родителя.
    s_menu_rechain(parent);

    // Возвращаем указатель на созданный элемент меню.
    return item;
}


/**
 * @brief Настройка элемента меню для перехода в дочернюю цепочку 
 * и перехода из родительского элемента по клику в родительскую цепочку.
 *
 * @param item Указатель на элемент меню, который будет настроен для перехода.
 * @param child Указатель на дочерний элемент меню, к которому будет осуществлён переход.
 */
static void s_menu_set_child(menu_item_t *item, menu_item_t *child)
{
    // Проверяем, что переданный элемент item не является NULL.
    if (item)
    {
        // Устанавливаем указатель на дочерний элемент для текущего элемента меню.
        item->child = child;
        
        // Устанавливаем флаг MENU_FLAG_GOTO_CHILD для текущего элемента меню,
        // чтобы указать, что у него есть возможность перейти к дочернему элементу.
        item->flags |= MENU_FLAG_GOTO_CHILD;
    }
}

#if (MENU_USAGE_MEMORY == MENU_USAGE_DYNAMIC_MEMORY)
/**
 * @brief Рекурсивное освобождение памяти всех элементов меню.
 * 
 * Эта функция рекурсивно освобождает память, занятую элементами меню,
 * начиная с указанного элемента и продолжая перерабатывать связанный
 * список всех подчинённых элементов. 
 *
 * @param item Указатель на элемент меню, начиная с которого будет
 * осуществляться освобождение памяти. Если указанный элемент или его дочерний
 * элемент равен `NULL`, функция завершает выполнение сразу.
 *
 * @details
 * Функция сначала проверяет, не является ли текущий элемент или его дочерний
 * элемент пустыми. В случае, если дочерний элемент не пуст, функция вызывает
 * сама себя рекурсивно для освобождения памяти всех дочерних элементов.
 * 
 * Затем функция проходит через все элементы в цепочке, начиная с текущего
 * элемента, и освобождает память для каждого из них. Если элемент имеет своих 
 * дочерних элементов, то они также освобождаются рекурсивно.
 *
 * Наконец, сам исходный элемент также освобождается.
 *
 * Это прекращает использование и утечку памяти, гарантируя, что вся
 * память, выделенная для меню, корректно освобождается.
 */
static void s_menu_free_items (void)
{
    menu_item_t *item = s_menu_handle.start;
    menu_item_t *next = NULL;
    while(item->folowing)
    {
        next = item->folowing;
        free(item);
        item = next;
    }
}

#endif


/**
 * @brief Обратный вызов для обработки длительного нажатия кнопки, 
 * переходящий к родительскому элементу меню или к стартовому элементу меню.
 */
static void s_long_push_button_callback (void)
{
    // Проверяем, есть ли у текущего элемента меню родительский элемент.
    if (s_menu_handle.current->parent)
    {
        // Устанавливаем текущий элемент меню как его родительский элемент.
        s_menu_handle.current = s_menu_handle.current->parent;

        // Вызываем функцию для обновления и отображения меню.
        s_display_menu();
    } 
    else 
    {
        // Если у текущего элемента нет родителя, устанавливаем текущий элемент
        // меню как стартовый элемент меню (корневой элемент).
        s_menu_handle.current = s_menu_handle.start;

        // Вызываем функцию для обновления и отображения меню.
        s_display_menu();
    }
}