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

    /** @brief Текущая цель на афише салуна. */
    enum class Contract : std::uint8_t { First, Gold, Double, Streak };
    /** @brief Короткое сообщение о последнем игровом событии. */
    enum class Notice : std::uint8_t { None, Clear, Level, Paid, GoldPaid, Missed };

    /** @brief Положение золотой клетки и сколько фиксаций осталось до срока. */
    struct Gold {
        int x = -1; ///< Колонка клетки на доске.
        int y = -1; ///< Строка клетки с учётом сдвига рядов.
        unsigned moves = 0; ///< Ноль означает, что метка пока не поставлена.
    };

    /** @brief Результат одной очистки поля. */
    struct Clear {
        unsigned count = 0; ///< Число удалённых строк.
        bool gold = false; ///< Удалена строка с золотой клеткой.
    };

    std::array<Row, rows> board_{}; ///< Фигуры, уже зафиксированные на поле.
    std::array<std::uint8_t, shapeCount> bag_{}; ///< Перемешанная группа фигур.
    std::size_t bagIndex_ = shapeCount; ///< Индекс следующей фигуры в мешке.
    std::uint32_t rng_ = 0; ///< Состояние локального генератора.
    std::uint32_t fallMs_ = 0; ///< Время до текущего шага падения, уже прошедшее.
    std::uint32_t lines_ = 0; ///< Число удалённых строк за партию.
    std::uint32_t combo_ = 0; ///< Сколько последних фиксаций подряд удаляли линии.
    std::uint32_t rewards_ = 0; ///< Число выполненных заданий.
    std::uint32_t noticeMs_ = 0; ///< Сколько игрового времени видно сообщение.
    Difficulty difficulty_ = Difficulty::Normal; ///< Выбранный начальный темп.
    Piece active_{}; ///< Фигура, которую сейчас двигает игрок.
    std::uint8_t next_ = 0; ///< Заранее выбранный вид следующей фигуры.
    Contract contract_ = Contract::First; ///< Задание на афише.
    Notice notice_ = Notice::None; ///< Сообщение, которое сейчас показано.
    bool streakStarted_ = false; ///< Первая очистка для задания на две подряд.
    Gold gold_{}; ///< Метка жилы; переносится вместе с собственной строкой.
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
    /** @brief Начисляет очки с насыщением до предела поля Result::score. */
    void addScore(std::uint64_t pointsEarned);
    /** @brief Уплотняет неполные строки и переносит метку золотой клетки. */
    Clear clearLines();
    /** @brief Проводит начисление и проверяет условие текущего задания. */
    void updateContract(const Clear& cleared);
    /** @brief Ставит метку в наиболее заполненной строке с клетками. */
    void markGold();
    /** @brief Выдаёт бонус, переводит афишу на следующую цель и ставит метку при необходимости. */
    void payReward(Contract next, std::uint32_t pointsEarned, Notice notice);
    /** @brief Показывает краткое сообщение на время игры. */
    void showNotice(Notice notice);
    /** @brief Возвращает положение, куда фигура упадёт без дальнейшего ввода. */
    Piece landing() const;
    /** @brief Рисует шахту с зафиксированными клетками, фигурой и её контуром. */
    void drawField(Screen& screen) const;
    /** @brief Рисует фигуру в заданной области экрана. */
    void drawPiece(Screen& screen, const Piece& piece, int left, int top, bool ghost = false) const;
    /** @brief Рисует бумажное объявление с текущим заданием. */
    void drawContract(Screen& screen) const;
    /** @brief Рисует табличку салуна, следующую фигуру и подсказки. */
    void drawSaloon(Screen& screen) const;
    /** @brief Устанавливает новую фигуру или завершает партию при коллизии. */
    void spawn();
    /** @brief Берёт фигуру из мешка, перемешивая следующую семёрку при необходимости. */
    std::uint8_t takeShape();
    /** @brief Возвращает следующее 32-битное значение из локального генератора. */
    std::uint32_t random();
    /** @brief Возвращает интервал между шагами падения в миллисекундах. */
    std::uint32_t fallInterval() const;
};
