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
    menu_item_t  *first;   ///< Указатель на первый элемент в текущей цепочке меню или подменю.
                           ///< Используется при инициализации новых цепочек элементов меню.
                           
    menu_item_t  *last;    ///< Указатель на последний элемент в текущей цепочке меню или подменю.
                           ///< Помогает в добавлении новых элементов к концу списка.

    menu_item_t  *parent;  ///< Указатель на родительский элемент, который содержит текущее подменю.
                           ///< Используется для возврата к предыдущему уровню меню.

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

static void s_rotary_encoder_callback (uint32_t current);
static void s_push_button_callback    (void);
static void s_display_menu            (void);
static void s_menu_position_handling  (void);
static void s_menu_init               (void);
static menu_item_t * s_create_new_item(void);

static menu_item_t * s_menu_add_item     (char *title, menu_item_t *parent, menu_item_callback_t callback, uint8_t flags);
static void          s_menu_rechain (menu_item_t *parent);

// static menu_item_t * s_create_submenu (char *title, menu_item_t *parent, uint8_t flags);
static void s_menu_set_start_values   (menu_item_t *item);
static void s_long_push_button_callback (void);

#if (MENU_USAGE_MEMORY == MENU_USAGE_DYNAMIC_MEMORY)
// static void s_menu_free_recursively   (menu_item_t *item);
// static void s_menu_free_items         (void);
static void s_menu_free_items           (void);
#endif

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

    menu_item_t *menu_opt_bck  = s_menu_add_item ("Back",   menu_options, NULL, 0);
    menu_item_t *menu_pwm      = s_menu_add_item ("PWM",    menu_options, NULL, 0);
    menu_item_t *menu_lo_arm   = s_menu_add_item ("Lo Arm", menu_options, NULL, 0);
    menu_item_t *menu_hi_arm   = s_menu_add_item ("Hi Arm", menu_options, NULL, 0);

    menu_item_t *menu_pwm_back   = s_menu_add_item ("Back",      menu_pwm, NULL, 0);
    menu_item_t *menu_pwm_enable = s_menu_add_item ("Enable",    menu_pwm, NULL, 0);
    menu_item_t *menu_pwm_freq   = s_menu_add_item ("Frequency", menu_pwm, NULL, 0);

    menu_item_t *menu_lo_arm_back     = s_menu_add_item ("Back",     menu_lo_arm, NULL, 0);
    menu_item_t *menu_lo_arm_enable   = s_menu_add_item ("Enable",   menu_lo_arm, NULL, 0);
    menu_item_t *menu_lo_arm_delay    = s_menu_add_item ("Delay",    menu_lo_arm, NULL, 0);
    menu_item_t *menu_lo_arm_duration = s_menu_add_item ("Duration", menu_lo_arm, NULL, 0);

    menu_item_t *menu_hi_arm_back     = s_menu_add_item ("Back",     menu_hi_arm, NULL, 0);
    menu_item_t *menu_hi_arm_enable   = s_menu_add_item ("Enable",   menu_hi_arm, NULL, 0);
    menu_item_t *menu_hi_arm_delay    = s_menu_add_item ("Delay",    menu_hi_arm, NULL, 0);
    menu_item_t *menu_hi_arm_duration = s_menu_add_item ("Duration", menu_hi_arm, NULL, 0);

    s_menu_init();
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
    s_display_menu();
    taskReadKey(s_rotary_encoder_callback, s_push_button_callback, s_long_push_button_callback);
#if (MENU_USAGE_MEMORY == MENU_USAGE_DYNAMIC_MEMORY)
    s_menu_free_items ();
#endif    
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
 * @brief Инициализирует начальные значения для элементов меню.
 *
 * Эта функция устанавливает начальные значения указателей внутри структуры
 * `s_menu_handle`, чтобы правильно организовать цепочку элементов меню.
 * Она используется как часть процесса добавления нового элемента в меню, 
 * обеспечивая корректное построение и поддержку связного списка элементов.
 *
 * Основные задачи функции:
 * 
 * 1. **Установка текущего элемента**:
 *    - Если указатель на текущий элемент (`s_menu_handle.current`) ещё не был 
 *      установлен, функция задаёт его на указатель переданного элемента `item`.
 *    - Это действует, как начальная установка, и так текущий элемент оказывается
 *      первым добавленным элементом в списке.
 *
 * 2. **Установка первого (начального) элемента**:
 *    - Если указатель на начальный элемент (`s_menu_handle.start`) ещё не был 
 *      установлен, функция присваивает ему значение переданного указателя `item`.
 *    - Таким образом, обеспечивается, что первый элемент в цепочке всегда будет доступен
 *      через этот указатель, что упрощает операции обхода и построения меню.
 *
 * Данная функция является ключевой частью механизма управления и организации 
 * динамической структуры меню, необходимой для удаления элементов меню или
 * изменения порядка их следования.
 *
 * @param item Указатель на элемент меню, который необходимо установить как текущий 
 * и/или начальный, если они ещё не были установлены.
 */
static void s_menu_set_start_values(menu_item_t *item)
{
    if (s_menu_handle.current == NULL) 
    {
        s_menu_handle.current = item; // Установить текущее на первый добавленный элемент
    }

    if (s_menu_handle.start == NULL) // Установить первый созданный элемент
    {
        s_menu_handle.start = item;
    }
}

/**
 * @brief Переинициализация цепочки подменю по родителю
 */
static void s_menu_rechain (menu_item_t *parent)
{
    menu_item_t *first = NULL;
    menu_item_t *prev  = NULL;
    menu_item_t *item  = s_menu_handle.start;
    while(item->folowing)
    {
        if (item->parent == parent)
        {
            if (first == NULL)
            {
                first = item;
            }

            if (prev != NULL)
            {
                item->prev = prev;
                prev->next = item;
            }

            prev = item;
        }

        item = item->folowing;
    }

    first->prev = prev;
    prev->next = first;
}

/**
 * @brief Создаёт новый элемент меню и интегрирует его в кольцевой связный список.
 *
 * Эта функция создаёт новый элемент меню, задаёт его базовые свойства и добавляет 
 * его в кольцевую структуру связного списка. Если элемент является единственным в списке,
 * указатели `prev` и `next` настраиваются так, чтобы указывать на него же самого, 
 * что представляет собой замкнутый цикл. Если элемент добавляется в конец существующего 
 * списка, его связи устанавливаются таким образом, чтобы поддерживался кольцевой порядок.
 *
 * @note 
 *      - Если это первый элемент списка, его `prev` будет указывать на 
 *        последний элемент списка (то есть сам на себя), а также его 
 *        `next` будет указывать на самого себя.
 *      - Если переданный `parent` является `NULL`, новый элемент считается корневым элементом списка.
 *      - Если имеется дочерний элемент `child`, то при активации данного элемента 
 *        меню произойдёт переход на новую ветвь, обозначенную `child`.
 *
 * @todo Рассмотреть возможности раздельного управления для начального элемента текущей 
 *       цепочки и начального элемента при добавлении нового подменю. В противном случае, 
 *       вызов функции `s_create_submenu` до инициализации основного элемента может привести к ошибкам.
 *
 * @param title Указатель на строку, содержащую заголовок нового элемента меню.
 * @param callback Функция обратного вызова, вызываемая при выборе данного элемента меню.
 * 
 * @return Указатель на созданный элемент меню или `NULL` в случае неудачи.
 */
static menu_item_t* s_menu_add_item(char *title, menu_item_t *parent, menu_item_callback_t callback, uint8_t flags)
{
    menu_item_t *item = s_create_new_item();
    if (item == NULL)
        return NULL; // Ошибка создания нового элемента
    
    // Инициализия нового элемента
    strncpy(item->title, title, MENU_ITEM_TITLE_LEN);
    item->parent   = parent;
    item->child    = NULL;
    item->flags   |= flags;
    item->callback = callback;
    item->folowing = NULL;

    if (s_menu_handle.current) 
    {
        s_menu_handle.current->folowing = item;
    }

    s_menu_handle.current = item;
    if (s_menu_handle.start == NULL)
    {
        s_menu_handle.start = item;
    }

    s_menu_rechain(parent);
}
#if 0
static menu_item_t* s_menu_add_item(char *title, menu_item_t *parent, menu_item_callback_t callback, uint8_t flags)
{
    menu_item_t *item = s_create_new_item();
    if (item == NULL)
        return NULL; // Ошибка создания нового элемента

    // Инициализируем новый элемент
    strncpy(item->title, title, MENU_ITEM_TITLE_LEN);
    item->callback = callback;
    item->parent   = parent;
    item->child    = NULL;
    item->flags   |= flags;

    if (s_menu_handle.first == NULL) {
        // Если это первый элемент в списке
        s_menu_handle.first = item;
        s_menu_handle.last = item;
        item->prev = item; // Элемент указывает на самого себя
        item->next = item; // Элемент указывает на самого себя
    } else {
        // Добавляем элемент в конец списка
        item->prev = s_menu_handle.last;
        item->next = s_menu_handle.first;
        s_menu_handle.last->next  = item;
        s_menu_handle.first->prev = item;
        s_menu_handle.last = item; // Последний элемент обновляется в handle
    }

    s_menu_set_start_values(item);

    if (s_menu_handle.parent != NULL && s_menu_handle.parent->child == NULL)
    {
        // Установка дочернего элемента, если он не был задан
        s_menu_handle.parent->child = item;
        s_menu_handle.parent->child->flags |= MENU_FLAG_GOTO_CHILD;
    }

    return item;
}
#endif

/**
 * @brief Создание новой цепочки подменю с элементом возврата, который позволяет вернуться в родительское меню по нажатию энкодера.
 *
 * Эта функция создаёт новую цепочку подменю и добавляет в неё обязательный элемент для возврата (back) в родительское меню. 
 * Элемент возврата служит для того, чтобы пользователь мог вернуться в меню, откуда был вызван данный подпункт.
 *
 * @param title Заголовок элемента меню, который будет отображаться для пункта возврата.
 * Этот заголовок будет показан пользователю в интерфейсе подменю.
 *
 * @param parent Указатель на родительский элемент родительской цепочки меню. 
 * При нажатии пользователем энкодера на пункт возврата произойдёт возврат на указанный `parent` элемент меню.
 * Если `parent` равен `NULL`, это означает, что подпункт будет самостоятельным элементом без родительской структуры, 
 * но в типичных сценариях `parent` не должен быть пустым, так как элемент возврата предполагает наличие исходной точки.
 *
 * @return Указатель на созданный возвратный пункт меню.
 * Возвращаемое значение может быть использовано для дальнейших манипуляций или проверок.
 * Если функция не смогла создать новый элемент (например, из-за нехватки памяти), будет возвращено `NULL`.
 *
 * @note
 * - Элемент устанавливается как точка входа в новую цепочку подменю, и он сам по себе образует замкнутую 
 *   структуру, ссылаясь на самого себя через указатели `prev` и `next`.
 * - Дочерний элемент `parent` устанавливается, чтобы позволить возврат в родительское меню при активации элемента.
 * - После создания, новое подменю становится активным в глобальном контексте, являясь текущим целевым 
 *   для всех последующих операций добавления элементов в подменю.
 */
#if 0
static menu_item_t * s_create_submenu (char *title, menu_item_t *parent, uint8_t flags)
{
    menu_item_t *item = s_create_new_item();
    if (item == NULL)
        return NULL;

    strncpy(item->title, title, MENU_ITEM_TITLE_LEN);
    item->parent = parent;
    item->prev = item; // Кольцевание на самого себя
    item->next = item; // Кольцевание на самого себя
    item->child = NULL;
    item->callback = NULL;
    item->flags |= MENU_FLAG_GOTO_PARENT | flags;

    if (parent != NULL) {
        parent->child = item; // Установить дочерний элемент
        parent->flags |= MENU_FLAG_GOTO_CHILD;
    }

    // Задаем новое подменю как точку входа для последующих добавлений
    s_menu_handle.first = item;
    s_menu_handle.last = item;
    s_menu_handle.parent = parent; // parent записывается только в элементах, где по нажатию энкодера нужен переход в родительское меню

    s_menu_set_start_values(item);

    return item;
}
#endif

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
#if 0
static void s_menu_free_recursively(menu_item_t *item) 
{
    if (item == NULL && item->child == NULL)
        return;

    // Сначала обработать дочерние элементы, если они есть
    if (item->child != NULL) {
        s_menu_free_recursively(item->child);
    }
    
    // Обрабатывать и освобождать память от остальных в цепочке
    menu_item_t *next_item = item->next;
    menu_item_t *last = next_item->prev;
    menu_item_t *firts = last->next;
    while (next_item != last) {
        menu_item_t *temp = next_item;

        if(temp == NULL)
            return;

        next_item = next_item->next;
        
        // Если у элемента есть дети, освободите их память рекурсивно.
        if (temp->child != NULL) {
            s_menu_free_recursively(temp->child);
        }
        
        // Освободить текущий элемент.
        // printf("%s\r\n", temp->title);

        free(temp);
    }
    
    // Освободить первоначально переданный элемент
    free(item);
}
#endif

/**
 * @brief Освобождает память, занятую всеми элементами меню.
 * 
 * Эта функция является точкой входа для освобождения всех элементов меню,
 * представленных в виде связного списка, посредством вызова рекурсивной функции.
 *
 * @details
 * Функция сначала проверяет, пуст ли список элементов меню, проверяя
 * указатель `s_menu_handle.start`. Если список элементов пуст (указатель равен `NULL`),
 * функция немедленно возвращает управление, так как нет памяти для 
 * освобождения.
 *
 * Если же элементы меню присутствуют, функция вызывает `s_menu_free_recursively`,
 * которая рекурсивно освобождает память для каждого элемента меню начиная с `s_menu_handle.start`.
 *
 * @note Это ключевая процедура для предотвращения утечек памяти, так как она
 * гарантирует, что все выделенные ресурсы, связанные с элементами меню,
 * корректно освобождены.
 */
#if 0
static void s_menu_free_items(void)
{
    if (s_menu_handle.start == NULL) {
        return;
    }

    s_menu_free_recursively(s_menu_handle.start);
}

#endif

static void s_long_push_button_callback (void)
{
    if (s_menu_handle.current->parent)
    {
        s_menu_handle.current = s_menu_handle.current->parent;
        s_display_menu();
    } else 
    {
        s_menu_handle.current = s_menu_handle.start;
        s_display_menu();
    }
}