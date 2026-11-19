/** @file tetris.cpp @brief Правила и отрисовка тетриса. */
#include "tetris.h"
#include "input.h"
#include "screen.h"
#include <algorithm>
#include <limits>

namespace {
constexpr int maskSide = 4; ///< Сторона локальной маски фигуры.
using Mask = std::uint16_t; ///< Занятые клетки маски по строкам, от младшего бита.

/** @brief Собирает четыре строки маски, по четыре бита в каждой. */
constexpr Mask mask(int a, int b, int c = 0, int d = 0) {
    return static_cast<Mask>(a | (b << 4) | (c << 8) | (d << 12));
}

/** @brief Возвращает бит локальной клетки фигуры. */
constexpr Mask bit(int x, int y) {
    return static_cast<Mask>(1u << (y * maskSide + x));
}

/** @brief Вращает маску по часовой стрелке внутри квадрата заданного размера. */
constexpr Mask clockwise(Mask original, int side) {
    Mask rotated = 0;
    for (int y = 0; y < side; ++y) {
        for (int x = 0; x < side; ++x) {
            if (original & bit(x, y)) {
                rotated |= bit(side - y - 1, x);
            }
        }
    }
    return rotated;
}

constexpr std::array<Mask, 7> startingMasks{
    mask(0, 15), mask(6, 6), mask(2, 7), mask(6, 3), mask(3, 6), mask(1, 7), mask(4, 7)
}; ///< Начальные ориентации I, O, T, S, Z, J, L.

/** @brief Предварительно вычисляет все четыре ориентации семи фигур. */
constexpr auto makeMasks() {
    std::array<std::array<Mask, 4>, startingMasks.size()> masks{};
    for (std::size_t shape = 0; shape < masks.size(); ++shape) {
        masks[shape][0] = startingMasks[shape];
        for (std::size_t rotation = 1; rotation < masks[shape].size(); ++rotation) {
            masks[shape][rotation] = shape == 1 ? startingMasks[shape]
                : clockwise(masks[shape][rotation - 1], shape == 0 ? 4 : 3);
        }
    }
    return masks;
}

constexpr auto masks = makeMasks(); ///< Маски всех ориентаций.
constexpr std::array<Color, startingMasks.size()> colors{
    Theme::cyan, Theme::accent, Theme::purple, Theme::green, Theme::red, Theme::blue, Theme::orange
}; ///< Цвет заполненных клеток для I, O, T, S, Z, J, L.
constexpr std::array<std::uint32_t, 5> points{0, 100, 300, 500, 800}; ///< Очки за очистку от 0 до 4 строк.
constexpr std::array<std::uint32_t, 3> baseIntervals{1200, 950, 740}; ///< Начальные интервалы по сложности.
constexpr std::array<std::string_view, 3> difficultyNames{
    "Лёгкая", "Средняя", "Сложная"
}; ///< Названия сложности на панели.

/** @brief Смещение фигуры при попытке повернуть у препятствия. */
struct Offset {
    int x; ///< Горизонтальная поправка.
    int y; ///< Вертикальная поправка.
};
constexpr Offset kicks[]{{0, 0}, {-1, 0}, {1, 0}, {-2, 0}, {2, 0}, {0, -1}, {-1, -1}, {1, -1},
                         {0, -2}}; ///< Проверяются по порядку, включая поворот на месте.
}

void Tetris::start(const Settings& settings) {
    board_ = {};
    bagIndex_ = shapeCount;
    rng_ = settings.seed;
    fallMs_ = 0;
    lines_ = 0;
    difficulty_ = settings.difficulty;
    result_ = {};
    spawn();
}

void Tetris::handle(const Input& input) {
    if (result_.state != State::Playing) {
        return;
    }
    switch (input.type) {
        case InputType::Left: shift(-1); break;
        case InputType::Right: shift(1); break;
        case InputType::Up: rotate(); break;
        case InputType::Down:
            fallMs_ = 0;
            descend();
            break;
        default: break;
    }
}

void Tetris::update(std::uint32_t ms) {
    while (ms != 0 && result_.state == State::Playing) {
        const auto interval = fallInterval();
        const auto step = std::min(ms, interval - fallMs_);
        fallMs_ += step;
        result_.timeMs += step;
        ms -= step;
        if (fallMs_ == interval) {
            fallMs_ = 0;
            descend();
        }
    }
}

void Tetris::draw(Screen& screen) const {
    const int left = Screen::boardX(columns);
    const int top = Screen::boardY(rows);
    for (int y = 0; y < rows; ++y) {
        screen.text(left - 1, top + y, "│", Theme::border);
        screen.text(left + 2 * columns, top + y, "│", Theme::border);
        for (int x = 0; x < columns; ++x) {
            const auto value = board_[y][x];
            if (value != 0) {
                screen.cell(left + 2 * x, top + y, Theme::background, colors[value - 1], U'▪');
            }
        }
    }
    if (result_.state == State::Playing) {
        const auto activeMask = masks[active_.shape][active_.rotation];
        for (int y = 0; y < maskSide; ++y) {
            for (int x = 0; x < maskSide; ++x) {
                if (activeMask & bit(x, y)) {
                    screen.cell(left + 2 * (active_.x + x), top + active_.y + y,
                                Theme::background, colors[active_.shape], U'◆');
                }
            }
        }
    }
    screen.panelNumber(0, "Счёт:", result_.score);
    screen.panelNumber(2, "Линии:", lines_);
    screen.panelNumber(4, "Уровень:", result_.level);
    screen.panelText(6, difficultyNames[static_cast<unsigned>(difficulty_)], Theme::muted);
}

Result Tetris::result() const {
    return result_;
}

bool Tetris::fits(const Piece& piece) const {
    const auto occupied = masks[piece.shape][piece.rotation];
    for (int y = 0; y < maskSide; ++y) {
        for (int x = 0; x < maskSide; ++x) {
            if (occupied & bit(x, y)) {
                const int column = piece.x + x;
                const int row = piece.y + y;
                if (column < 0 || column >= columns || row < 0 || row >= rows ||
                    board_[row][column] != 0) {
                    return false;
                }
            }
        }
    }
    return true;
}

void Tetris::shift(int dx) {
    Piece moved = active_;
    moved.x += dx;
    if (fits(moved)) {
        active_ = moved;
    }
}

void Tetris::rotate() {
    Piece turned = active_;
    turned.rotation = (active_.rotation + 1) % 4;
    for (const auto& kick : kicks) {
        turned.x = active_.x + kick.x;
        turned.y = active_.y + kick.y;
        if (fits(turned)) {
            active_ = turned;
            return;
        }
    }
}

void Tetris::descend() {
    Piece lowered = active_;
    ++lowered.y;
    if (fits(lowered)) {
        active_ = lowered;
    } else {
        lock();
    }
}

void Tetris::lock() {
    const auto occupied = masks[active_.shape][active_.rotation];
    for (int y = 0; y < maskSide; ++y) {
        for (int x = 0; x < maskSide; ++x) {
            if (occupied & bit(x, y)) {
                board_[active_.y + y][active_.x + x] = active_.shape + 1;
            }
        }
    }
    const unsigned removed = clearLines();
    lines_ = std::min(lines_, std::numeric_limits<std::uint32_t>::max() - removed) + removed;
    result_.score = std::min(result_.score, std::numeric_limits<std::uint32_t>::max() - points[removed])
        + points[removed];
    spawn();
}

unsigned Tetris::clearLines() {
    int destination = rows - 1;
    for (int source = rows - 1; source >= 0; --source) {
        const Row& row = board_[source];
        if (std::all_of(row.begin(), row.end(), [](auto value) { return value != 0; })) {
            continue;
        }
        board_[destination] = row;
        --destination;
    }
    const auto removed = static_cast<unsigned>(destination + 1);
    std::fill_n(board_.begin(), removed, Row{});
    return removed;
}

void Tetris::spawn() {
    active_ = Piece{takeShape(), 0, (columns - maskSide) / 2, 0};
    if (!fits(active_)) {
        result_.state = State::Lost;
    }
}

std::uint8_t Tetris::takeShape() {
    if (bagIndex_ == shapeCount) {
        for (std::size_t i = 0; i < shapeCount; ++i) {
            bag_[i] = static_cast<std::uint8_t>(i);
        }
        for (std::size_t i = shapeCount - 1; i > 0; --i) {
            const auto j = static_cast<std::size_t>((std::uint64_t{random()} * (i + 1)) >> 32);
            std::swap(bag_[i], bag_[j]);
        }
        bagIndex_ = 0;
    }
    return bag_[bagIndex_++];
}

std::uint32_t Tetris::random() {
    rng_ += 0x9e3779b9u;
    std::uint32_t value = rng_;
    value = (value ^ (value >> 16)) * 0x85ebca6bu;
    value = (value ^ (value >> 13)) * 0xc2b2ae35u;
    return value ^ (value >> 16);
}

std::uint32_t Tetris::fallInterval() const {
    return baseIntervals[static_cast<unsigned>(difficulty_)];
}
