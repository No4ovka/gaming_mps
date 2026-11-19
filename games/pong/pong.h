/** @file pong.h @brief Контракт модуля Pong; пока диагностическая заглушка. */
#pragma once
#include "game.h"
#include "stub.h"

/** @brief Pong: точка подключения будущей игровой механики. */
class Pong final : public Game {
public:
    /** @brief Полностью начинает партию с переданными настройками. */
    void start(const Settings& settings) override;
    /** @brief Принимает готовое действие общей оболочки. */
    void handle(const Input& input) override;
    /** @brief Получает прошедшее игровое время без пауз. */
    void update(std::uint32_t ms) override;
    /** @brief Рисует состояние через общий Screen. */
    void draw(Screen& screen) const override;
    /** @brief Сообщает состояние партии оболочке. */
    Result result() const override;
private:
    Stub stub_{GameId::Pong}; ///< Удаляется при реализации настоящей механики.
};
