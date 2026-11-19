/** @file stub.h @brief Честная заглушка для проверки интеграции игр. */
#pragma once
#include "game.h"
#include "input.h"

/** @brief Общая диагностика ввода без имитации игровой механики. */
class Stub {
public:
    /** @brief Выбирает подпись и размеры диагностического поля. */
    explicit Stub(GameId game);
    /** @brief Сбрасывает диагностическое состояние. */
    void start(const Settings& settings);
    /** @brief Запоминает последнее событие, включая локальные координаты мыши. */
    void handle(const Input& input);
    /** @brief Учитывает игровое время диагностики. */
    void update(std::uint32_t ms);
    /** @brief Показывает отсутствие механики, настройки и полученный ввод. */
    void draw(Screen& screen) const;
    /** @brief Возвращает Playing; заглушка не изображает победу или поражение. */
    Result result() const;
private:
    GameId game_; ///< Какой игровой модуль заменяется заглушкой.
    Settings settings_{}; ///< Принятые стартовые условия.
    Input input_{}; ///< Последнее переданное действие.
    std::uint32_t events_ = 0; ///< Число принятых событий.
    std::uint64_t time_ = 0; ///< Время работы без пауз.
};
