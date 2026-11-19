/** @file tetris.h @brief Состояние и интерфейс игры с падающими фигурами. */
#pragma once
#include "game.h"
#include <array>

/** @brief Бесконечная партия на поле 10×20 с воспроизводимой очередью фигур. */
class Tetris final : public Game {
public:
    /** @brief Полностью начинает партию с переданными настройками. */
    void start(const Settings& settings) override;
    /** @brief Сдвигает, поворачивает или опускает активную фигуру. */
    void handle(const Input& input) override;
    /** @brief Продвигает автоматическое падение на время без пауз. */
    void update(std::uint32_t ms) override;
    /** @brief Рисует фигуры, поле и показатели через общий Screen. */
    void draw(Screen& screen) const override;
    /** @brief Возвращает состояние, счёт, уровень и время партии. */
    Result result() const override;

private:
    friend struct TetrisRulesTest; ///< Позволяет проверить столкновения и очистку на подготовленном поле.

    static constexpr int columns = 10; ///< Ширина игрового поля.
    static constexpr int rows = 20; ///< Высота игрового поля.
    static constexpr std::size_t shapeCount = 7; ///< Число разных фигур в мешке.
    using Row = std::array<std::uint8_t, columns>; ///< Ноль означает пустую клетку.

    /** @brief Положение и ориентация активной фигуры на поле. */
    struct Piece {
        std::uint8_t shape = 0; ///< Индекс вида фигуры, начиная с нуля.
        std::uint8_t rotation = 0; ///< Число поворотов на 90° по часовой стрелке.
        int x = 0; ///< Колонка левого края локальной маски.
        int y = 0; ///< Строка верхнего края локальной маски.
    };

    std::array<Row, rows> board_{}; ///< Фигуры, уже зафиксированные на поле.
    std::array<std::uint8_t, shapeCount> bag_{}; ///< Перемешанная группа фигур.
    std::size_t bagIndex_ = shapeCount; ///< Индекс следующей фигуры в мешке.
    std::uint32_t rng_ = 0; ///< Состояние локального генератора.
    std::uint32_t fallMs_ = 0; ///< Время до текущего шага падения, уже прошедшее.
    std::uint32_t lines_ = 0; ///< Число удалённых строк за партию.
    Difficulty difficulty_ = Difficulty::Normal; ///< Выбранный начальный темп.
    Piece active_{}; ///< Фигура, которую сейчас двигает игрок.
    Result result_{}; ///< Итоговые показатели с начала партии.

    /** @brief Проверяет, что все четыре клетки фигуры помещаются на поле. */
    bool fits(const Piece& piece) const;
    /** @brief Применяет горизонтальный сдвиг, когда он возможен. */
    void shift(int dx);
    /** @brief Пробует поворот у свободной клетки, стенки или дна. */
    void rotate();
    /** @brief Опускает фигуру на клетку или фиксирует её при препятствии. */
    void descend();
    /** @brief Записывает фигуру, удаляет линии, начисляет очки и создаёт следующую. */
    void lock();
    /** @brief Уплотняет неполные строки снизу и возвращает число удалённых. */
    unsigned clearLines();
    /** @brief Устанавливает новую фигуру или завершает партию при коллизии. */
    void spawn();
    /** @brief Берёт фигуру из мешка, перемешивая следующую семёрку при необходимости. */
    std::uint8_t takeShape();
    /** @brief Возвращает следующее 32-битное значение из локального генератора. */
    std::uint32_t random();
    /** @brief Возвращает интервал между шагами падения в миллисекундах. */
    std::uint32_t fallInterval() const;
};
