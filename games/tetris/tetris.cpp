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
constexpr std::array<std::uint32_t, 3> baseIntervals{1500, 950, 500}; ///< Начальные интервалы по сложности.
constexpr std::uint32_t linesPerLevel = 10; ///< Сколько удалённых рядов нужно для следующего уровня.
constexpr std::uint32_t minInterval = 100; ///< Ограничение скорости на высоких уровнях, в мс.
constexpr std::uint32_t noticeDuration = 1200; ///< Длительность подсветки сообщения в мс.
constexpr unsigned goldMoves = 6; ///< Сколько фиксаций можно сделать, чтобы убрать жилу.
constexpr std::uint32_t firstReward = 75; ///< Награда за первую линию.
constexpr std::uint32_t doubleReward = 150; ///< Награда за две линии с одной фигуры.
constexpr std::uint32_t goldReward = 250; ///< Награда за золотую жилу.
constexpr std::uint32_t streakReward = 200; ///< Награда за две очистки подряд.
constexpr std::uint32_t comboRate = 50; ///< Очки за каждый шаг серии сверх первой очистки.
constexpr int cardWidth = 20; ///< Ширина вывески и афиши внутри общего поля.
constexpr int cardGap = 4; ///< Отступ карточек от логической доски.
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
    combo_ = 0;
    rewards_ = 0;
    noticeMs_ = 0;
    contract_ = Contract::First;
    notice_ = Notice::None;
    streakStarted_ = false;
    gold_ = {};
    difficulty_ = settings.difficulty;
    result_ = {};
    result_.level = std::max(std::uint32_t{1}, settings.level);
    next_ = takeShape();
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
        noticeMs_ -= std::min(noticeMs_, step);
        ms -= step;
        if (fallMs_ == interval) {
            fallMs_ = 0;
            descend();
        }
    }
}

void Tetris::draw(Screen& screen) const {
    drawSaloon(screen);
    drawField(screen);
    drawContract(screen);

    screen.panelText(0, "* КАССА САЛУНА *", Theme::accent);
    screen.panelText(1, "─────────────────────", Theme::border);
    screen.panelNumber(3, "Заработок:", result_.score);
    screen.panelNumber(5, "Линии:", lines_);
    screen.panelNumber(7, "Уровень:", result_.level);
    screen.panelNumber(9, "До уровня:", linesPerLevel - lines_ % linesPerLevel, Theme::muted);
    screen.panelText(12, "Сложность", Theme::muted);
    screen.panelText(13, difficultyNames[static_cast<unsigned>(difficulty_)]);
    screen.panelText(16, "─────────────────────", Theme::border);
    screen.panelNumber(18, "Серия:", combo_, combo_ > 1 ? Theme::accent : Theme::text);
    screen.panelNumber(20, "Оплат:", rewards_);
    if (noticeMs_ != 0) {
        constexpr std::array<std::string_view, 6> messages{
            "", "Очистка!", "Темп вырос!", "Работа оплачена", "Жила взята!", "Время жилы вышло"
        };
        const int left = Screen::boardX(columns);
        const int statusRow = Screen::boardY(rows) + rows + 2;
        screen.fill(left - 2, statusRow, 2 * columns + 4, 1, Theme::wood);
        screen.text(left, statusRow, messages[static_cast<unsigned>(notice_)],
                    notice_ == Notice::Missed ? Theme::white : Theme::text, Theme::wood);
    }
}

void Tetris::drawField(Screen& screen) const {
    const int left = Screen::boardX(columns);
    const int top = Screen::boardY(rows);
    const bool spark = notice_ != Notice::Missed && noticeMs_ > 700 && (noticeMs_ / 90) % 2 == 1;
    const Color rail = spark ? Theme::accent : Theme::border;
    screen.text(left + 7, top - 3, "ШАХТА", Theme::accent);
    screen.frame(left - 2, top - 1, 2 * columns + 4, rows + 2, rail);
    for (int y = 0; y < rows; ++y) {
        const bool brace = y % 5 == 4;
        screen.text(left - 2, top + y, brace ? "+│" : "▒│", rail, Theme::wood);
        screen.text(left + 2 * columns, top + y, brace ? "│+" : "│▒", rail, Theme::wood);
        for (int x = 0; x < columns; ++x) {
            const auto value = board_[y][x];
            const int cellX = left + 2 * x;
            if (x == gold_.x && y == gold_.y) {
                screen.cell(cellX, top + y, Theme::ink, Theme::accent, U'*');
            } else if (value != 0) {
                screen.cell(cellX, top + y, Theme::background, colors[value - 1], U'▪');
            } else if (brace) {
                screen.cell(cellX, top + y, Theme::border, Theme::background, U'·');
            }
        }
    }
    if (result_.state == State::Playing) {
        drawPiece(screen, landing(), left, top, true);
        drawPiece(screen, active_, left, top);
    }
}

void Tetris::drawPiece(Screen& screen, const Piece& piece, int left, int top, bool ghost) const {
    const auto occupied = masks[piece.shape][piece.rotation];
    for (int y = 0; y < maskSide; ++y) {
        for (int x = 0; x < maskSide; ++x) {
            if (occupied & bit(x, y)) {
                screen.cell(left + 2 * (piece.x + x), top + piece.y + y,
                            ghost ? Theme::muted : Theme::background,
                            ghost ? Theme::background : colors[piece.shape], ghost ? U'◇' : U'◆');
            }
        }
    }
}

void Tetris::drawSaloon(Screen& screen) const {
    const int left = Screen::boardX(columns) - cardGap - cardWidth;
    const int top = Screen::boardY(rows);
    screen.fill(left, top, cardWidth, 4, Theme::wood);
    screen.text(left + 7, top + 1, "САЛУН", Theme::white, Theme::wood);
    screen.text(left + 3, top + 2, "* ТРИ СТВОЛА *", Theme::white, Theme::wood);
    int controlsRow = top + 7;
    if (difficulty_ != Difficulty::Hard) {
        screen.text(left + 4, top + 6, "СЛЕДУЮЩАЯ", Theme::accent);
        screen.frame(left, top + 7, cardWidth, 6, Theme::border);
        drawPiece(screen, Piece{next_, 0, 0, 0}, left + (cardWidth - 2 * maskSide) / 2, top + 9);
        controlsRow = top + 14;
    }
    screen.text(left + 2, controlsRow, "A/D  сдвиг", Theme::muted);
    screen.text(left + 2, controlsRow + 1, "W    поворот", Theme::muted);
    screen.text(left + 2, controlsRow + 2, "S    вниз", Theme::muted);
    screen.text(left + 4, top + 19, "     _", Theme::green);
    screen.text(left + 4, top + 20, "  | | |", Theme::green);
    screen.text(left + 4, top + 21, "  |_| | |", Theme::green);
    screen.text(left + 4, top + 22, "____| |_|____", Theme::muted);
}

void Tetris::drawContract(Screen& screen) const {
    const int left = Screen::boardX(columns) + 2 * columns + cardGap;
    const int top = Screen::boardY(rows);
    screen.fill(left, top, cardWidth, 14, Theme::paper);
    screen.fill(left, top, cardWidth, 3, Theme::wood);
    screen.text(left + 2, top + 1, "* ЗАКАЗЫ ШАХТЫ *", Theme::white, Theme::wood);
    const auto line = [&](int row, std::string_view text, Color color = Theme::ink) {
        screen.text(left + 2, top + row, text, color, Theme::paper);
    };
    std::uint32_t reward = 0;
    switch (contract_) {
        case Contract::First:
            line(5, "Убери 1 ряд");
            reward = firstReward;
            break;
        case Contract::Gold:
            line(4, "ЗОЛОТАЯ ЖИЛА", Theme::paperMuted);
            line(6, "Закрой ряд");
            line(7, "со знаком *");
            if (gold_.moves != 0) {
                line(9, "Осталось фигур:", Theme::paperMuted);
                screen.number(left + 18, top + 9, gold_.moves, Theme::ink, Theme::paper);
            } else {
                line(9, "Ищем жилу", Theme::paperMuted);
            }
            reward = goldReward;
            break;
        case Contract::Double:
            line(5, "Два ряда");
            line(6, "за фигуру");
            reward = doubleReward;
            break;
        case Contract::Streak:
            line(5, "Две очистки");
            line(6, "подряд");
            line(8, streakStarted_ ? "1 из 2" : "0 из 2", Theme::paperMuted);
            reward = streakReward;
            break;
    }
    line(12, "Оплата", Theme::paperMuted);
    screen.number(left + 15, top + 12, reward, Theme::ink, Theme::paper);
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
    const Clear cleared = clearLines();
    const unsigned removed = cleared.count;
    const auto previousThreshold = lines_ / linesPerLevel;
    lines_ = std::min(lines_, std::numeric_limits<std::uint32_t>::max() - removed) + removed;
    addScore(std::uint64_t{points[removed]} * result_.level);
    const auto gained = lines_ / linesPerLevel - previousThreshold;
    result_.level = std::min(result_.level, std::numeric_limits<std::uint32_t>::max() - gained) + gained;
    if (removed == 0) {
        combo_ = 0;
    } else {
        combo_ = std::min(combo_, std::numeric_limits<std::uint32_t>::max() - 1) + 1;
        addScore(std::uint64_t{combo_ - 1} * comboRate);
        showNotice(gained ? Notice::Level : Notice::Clear);
    }
    updateContract(cleared);
    spawn();
}

void Tetris::addScore(std::uint64_t pointsEarned) {
    result_.score = static_cast<std::uint32_t>(std::min(
        std::uint64_t{result_.score} + pointsEarned, std::uint64_t{std::numeric_limits<std::uint32_t>::max()}));
}

Tetris::Clear Tetris::clearLines() {
    Clear cleared;
    int destination = rows - 1;
    const int goldSource = gold_.y;
    for (int source = rows - 1; source >= 0; --source) {
        const Row& row = board_[source];
        if (std::all_of(row.begin(), row.end(), [](auto value) { return value != 0; })) {
            if (source == goldSource) {
                cleared.gold = true;
                gold_.y = -1;
            }
            continue;
        }
        if (source == goldSource) {
            gold_.y = destination;
        }
        board_[destination] = row;
        --destination;
    }
    cleared.count = static_cast<unsigned>(destination + 1);
    std::fill_n(board_.begin(), cleared.count, Row{});
    return cleared;
}

void Tetris::updateContract(const Clear& cleared) {
    switch (contract_) {
        case Contract::First:
            if (cleared.count > 0) {
                payReward(Contract::Gold, firstReward, Notice::Paid);
            }
            break;
        case Contract::Gold:
            if (cleared.gold) {
                payReward(Contract::Double, goldReward, Notice::GoldPaid);
            } else if (gold_.y < 0) {
                markGold();
            } else if (--gold_.moves == 0) {
                gold_ = {};
                contract_ = Contract::Double;
                showNotice(Notice::Missed);
            }
            break;
        case Contract::Double:
            if (cleared.count >= 2) {
                payReward(Contract::Streak, doubleReward, Notice::Paid);
            }
            break;
        case Contract::Streak:
            if (streakStarted_ && cleared.count > 0) {
                payReward(Contract::Gold, streakReward, Notice::Paid);
            } else {
                streakStarted_ = cleared.count > 0;
            }
            break;
    }
}

void Tetris::markGold() {
    int mostFilled = 0;
    for (int y = rows - 1; y >= 0; --y) {
        const auto& row = board_[y];
        const int filled = std::count_if(row.begin(), row.end(), [](auto value) { return value != 0; });
        if (filled > mostFilled) {
            mostFilled = filled;
            const auto cell = std::find_if(row.begin(), row.end(), [](auto value) { return value != 0; });
            gold_ = Gold{static_cast<int>(cell - row.begin()), y, goldMoves};
        }
    }
}

void Tetris::payReward(Contract next, std::uint32_t pointsEarned, Notice notice) {
    addScore(pointsEarned);
    rewards_ = std::min(rewards_, std::numeric_limits<std::uint32_t>::max() - 1) + 1;
    streakStarted_ = false;
    gold_ = {};
    contract_ = next;
    if (next == Contract::Gold) {
        markGold();
    }
    showNotice(notice);
}

void Tetris::showNotice(Notice notice) {
    notice_ = notice;
    noticeMs_ = noticeDuration;
}

Tetris::Piece Tetris::landing() const {
    Piece probe = active_;
    do {
        ++probe.y;
    } while (fits(probe));
    --probe.y;
    return probe;
}

void Tetris::spawn() {
    active_ = Piece{next_, 0, (columns - maskSide) / 2, 0};
    if (!fits(active_)) {
        result_.state = State::Lost;
    } else {
        next_ = takeShape();
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
    const std::uint32_t base = baseIntervals[static_cast<unsigned>(difficulty_)];
    const auto interval = static_cast<std::uint32_t>(base * 10 / (std::uint64_t{result_.level} + 9));
    return std::max(minInterval, interval);
}
