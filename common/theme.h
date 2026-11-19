/** @file theme.h @brief Общая палитра ANSI 256 цветов. */
#pragma once
#include <cstdint>

/** @brief Индекс цвета в палитре терминала. */
using Color = std::uint8_t;

/** @brief Цвета общей темы вестерна и игровых элементов. */
namespace Theme {
inline constexpr Color background = 234; ///< Тёмный фон.
inline constexpr Color text = 223; ///< Светлый основной текст.
inline constexpr Color muted = 137; ///< Второстепенные подписи.
inline constexpr Color accent = 179; ///< Песочный акцент и выбор.
inline constexpr Color border = 95; ///< Медные рамки.
inline constexpr Color red = 167; ///< Опасность и красные элементы.
inline constexpr Color green = 108; ///< Успех и зелёные элементы.
inline constexpr Color blue = 109; ///< Синие игровые элементы.
inline constexpr Color cyan = 116; ///< Голубые игровые элементы.
inline constexpr Color purple = 139; ///< Фиолетовые игровые элементы.
inline constexpr Color orange = 173; ///< Оранжевые игровые элементы.
inline constexpr Color white = 230; ///< Самые светлые элементы.
inline constexpr Color paper = 230; ///< Светлая бумага салунной афиши.
inline constexpr Color ink = 235; ///< Тёмная печать на афише.
inline constexpr Color wood = 94; ///< Дерево вывески и активная строка.
inline constexpr Color paperMuted = 94; ///< Второстепенная печать на бумаге.
inline constexpr Color paperError = 88; ///< Читаемая ошибка на бумаге.
}
