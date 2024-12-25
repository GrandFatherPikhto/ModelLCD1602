# Проект заготовки "Menu" для LCD1602

Этот проект реализует систему меню для дисплея LCD1602, управляемую микроконтроллерами STM32 или ESP32. Меню поддерживает взаимодействие с пользователем через ротационный энкодер и кнопки.

## Содержание

- [Описание](#описание)
- [Функции](#функции)
- [Требования](#требования)
- [Установка](#установка)
- [Настройка](#настройка)
- [Использование](#использование)
- [Лицензия](#лицензия)

## Описание

Система меню обеспечивает иерархическую навигацию с возможностью установки функций обратного вызова для каждого элемента меню. Она поддерживает кольцевую навигацию и переходы на дочерние и родительские уровни.

## Функции

- **Иерархия меню**: Возможность создания многоуровневых меню.
- **Кольцевая структура**: Последний элемент связан с первым, что позволяет циклическую прокрутку.
- **Обратные вызовы**: Каждый элемент меню может запускать функцию обратного вызова.
- **Поддержка ротационного энкодера**: Навигация и выбор через энкодер.
- **Поддержка нажатия кнопок**: Переходы между меню и подменю через кнопки.

## Требования

- **Аппаратная часть**:
  - Дисплей LCD1602
  - микроконтроллер STM32 или ESP32
  - Ротационный энкодер с кнопкой

- **Программная часть**:
  - Компилятор C (например, GCC)
  - Среда разработки STM32CubeMX или платформа Arduino, если используется ESP32

## Установка

1. **Клонирование репозитория**:
```bash
   git clone https://github.com/your-username/menu-lcd1602.git
   cd menu-lcd1602
```
2. Компиляция кода:

Для STM32: Настройте проект в STM32CubeMX и скомпилируйте его в выбранной IDE (Keil, IAR или STM32CubeIDE).

Для ESP32: Используйте Arduino IDE или PlatformIO для компиляции скетча.

3. Настройка
Конфигурация дисплея и энкодера:

4. Подключите дисплей LCD1602 и ротационный энкодер к соответствующим пинам микроконтроллера.

5. Убедитесь, что используете правильные пины и протокол связи (например, I2C или GPIO).

6. Настройка проекта:

Параметры меню определяются в файле menu.h.

Настройте параметры, такие как MENU_USAGE_MEMORY, в зависимости от требуемого способа управления памятью (статический или динамический).

7. Использование

Инициализация: Вызовите функцию Menu_Init() для создания и инициализации иерархии меню.

Обработка ввода: Система использует ротационный энкодер для навигации и кнопки для подтверждения выбора или перехода.

Отображение меню: Меню отображается на LCD1602, обновляясь при изменении текущей позиции.

Пример кода для инициализации меню:

```c
void Menu_Init(void)
{
    menu_item_t *menu_start = s_menu_add_item("Start", NULL);
    menu_item_t *menu_test = s_menu_add_item("Test", NULL);
    // Дополнительные пункты и подменю...
}
```

