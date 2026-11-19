/** @file minesweeper.cpp @brief Раскладка, раскрытие поля, управление и отрисовка сапёра. */
#include "minesweeper.h"
#include "input.h"
#include "menu.h"
#include "screen.h"
#include <algorithm>
#include <string>

/** @brief Число мин по уровням и сложностям Easy, Normal, Hard. */
static constexpr int mineTable[][3] = {
    {6, 10, 13}, {10, 15, 20}, {14, 22, 29}, {20, 29, 39}, {26, 38, 51}
};

/** @brief Цвет числовой подсказки по числу соседних мин. */
static constexpr Color hintColor[] = {
    Theme::text, Theme::blue, Theme::green, Theme::red, Theme::purple,
    Theme::orange, Theme::cyan, Theme::accent, Theme::white
};

/** @brief Продвигает 32-битный генератор с перемешиванием битов, включая seed 0. */
static std::uint32_t nextRandom(std::uint32_t& state) {
    state += 0x9E3779B9u;
    std::uint32_t bits = state;
    bits = (bits ^ (bits >> 16)) * 0x21F0AAADu;
    bits = (bits ^ (bits >> 15)) * 0x735A2D97u;
    return bits ^ (bits >> 15);
}

void Minesweeper::start(const Settings& settings) {
    result_ = {};
    result_.level = std::clamp(settings.level, std::uint32_t{1}, std::uint32_t{5});
    difficulty_ = settings.difficulty;
    size_ = 6 + 2 * static_cast<int>(result_.level);
    mines_ = mineTable[result_.level - 1][static_cast<unsigned>(difficulty_)];
    seed_ = settings.seed;

    cells_.fill(Cell{});
    flags_ = 0;
    safeLeft_ = size_ * size_ - mines_;
    cursor_ = {size_ / 2, size_ / 2};
    hover_ = {};

    const Point first{settings.firstX, settings.firstY};
    if (contains(first)) {
        cursor_ = first;
        open(first);
    }
}

bool Minesweeper::contains(Point position) const {
    return position.x >= 0 && position.x < size_ && position.y >= 0 && position.y < size_;
}

int Minesweeper::index(Point position) const {
    return position.y * size_ + position.x;
}

Minesweeper::Point Minesweeper::pointerCell(const Input& input) const {
    const int x = input.x - Screen::boardX(size_);
    const int y = input.y - Screen::boardY(size_);
    if (x < 0 || x >= 2 * size_ || y < 0 || y >= size_) return {};
    return {x / 2, y};
}

template<typename Action>
void Minesweeper::forNeighbors(int cellIndex, Action action) const {
    const int centerX = cellIndex % size_;
    const int centerY = cellIndex / size_;
    for (int y = std::max(0, centerY - 1); y <= std::min(size_ - 1, centerY + 1); ++y) {
        for (int x = std::max(0, centerX - 1); x <= std::min(size_ - 1, centerX + 1); ++x) {
            if (x != centerX || y != centerY) action(index({x, y}));
        }
    }
}

void Minesweeper::placeMines(Point first) {
    std::array<std::uint16_t, maxCells> candidates;
    int available = 0;
    const int excluded = index(first);
    for (int i = 0; i < size_ * size_; ++i) {
        if (i != excluded) candidates[available++] = static_cast<std::uint16_t>(i);
    }

    std::uint32_t state = seed_;
    for (int count = 0; count < mines_; ++count) {
        const auto selected = nextRandom(state) % static_cast<std::uint32_t>(available);
        const int cellIndex = candidates[selected];
        candidates[selected] = candidates[--available];
        cells_[cellIndex].mine = true;
        forNeighbors(cellIndex, [this](int neighbor) { ++cells_[neighbor].adjacent; });
    }
}

void Minesweeper::openSafeCells(int first) {
    std::array<std::uint16_t, maxCells> queue;
    std::size_t read = 0;
    std::size_t write = 0;
    const auto visit = [this, &queue, &write](int cellIndex) {
        Cell& cell = cells_[cellIndex];
        if (cell.open || cell.flag || cell.mine) return;
        cell.open = true;
        --safeLeft_;
        if (cell.adjacent == 0) queue[write++] = static_cast<std::uint16_t>(cellIndex);
    };

    visit(first);
    while (read < write) forNeighbors(queue[read++], visit);
}

void Minesweeper::open(Point position) {
    const int cellIndex = index(position);
    Cell& cell = cells_[cellIndex];
    if (cell.flag) return;
    if (cell.open) {
        openNeighbors(cellIndex);
        return;
    }

    if (result_.firstX < 0) {
        placeMines(position);
        result_.firstX = static_cast<std::int16_t>(position.x);
        result_.firstY = static_cast<std::int16_t>(position.y);
    }
    if (cell.mine) {
        cell.open = true;
        result_.state = State::Lost;
        return;
    }

    openSafeCells(cellIndex);
    if (safeLeft_ == 0) result_.state = State::Won;
}

int Minesweeper::neighboringFlags(int cellIndex) const {
    int count = 0;
    forNeighbors(cellIndex, [this, &count](int neighbor) { count += cells_[neighbor].flag; });
    return count;
}

void Minesweeper::openNeighbors(int cellIndex) {
    const int marked = neighboringFlags(cellIndex);
    if (cells_[cellIndex].adjacent == 0 || marked != cells_[cellIndex].adjacent) return;

    // Resolve every selected neighbor before checking for a win.
    forNeighbors(cellIndex, [this](int neighbor) {
        Cell& cell = cells_[neighbor];
        if (cell.open || cell.flag || result_.state == State::Lost) return;
        if (cell.mine) {
            cell.open = true;
            result_.state = State::Lost;
        } else {
            openSafeCells(neighbor);
        }
    });
    if (result_.state != State::Lost && safeLeft_ == 0) result_.state = State::Won;
}

void Minesweeper::toggleFlag(Point position) {
    Cell& cell = cells_[index(position)];
    if (cell.open) return;
    cell.flag = !cell.flag;
    flags_ += cell.flag ? 1 : -1;
}

void Minesweeper::handle(const Input& input) {
    if (result_.state != State::Playing) return;

    switch (input.type) {
        case InputType::PointerMove:
            hover_ = pointerCell(input);
            return;
        case InputType::PointerLeft:
        case InputType::PointerRight: {
            const Point position = pointerCell(input);
            if (!contains(position)) return;
            hover_ = cursor_ = position;
            if (input.type == InputType::PointerLeft) open(position);
            else toggleFlag(position);
            return;
        }
        case InputType::Up: cursor_.y = std::max(0, cursor_.y - 1); break;
        case InputType::Down: cursor_.y = std::min(size_ - 1, cursor_.y + 1); break;
        case InputType::Left: cursor_.x = std::max(0, cursor_.x - 1); break;
        case InputType::Right: cursor_.x = std::min(size_ - 1, cursor_.x + 1); break;
        case InputType::Confirm:
        case InputType::Open: open(cursor_); break;
        case InputType::Flag: toggleFlag(cursor_); break;
        default: return;
    }
    hover_ = {};
}

void Minesweeper::update(std::uint32_t ms) {
    if (result_.state == State::Playing && result_.firstX >= 0) result_.timeMs += ms;
}

void Minesweeper::drawCell(Screen& screen, Point position, Point selection, bool preview, bool ready) const {
    const Cell& cell = cells_[index(position)];
    char32_t glyph = U'#';
    Color foreground = Theme::muted;
    Color background = Theme::ink;
    const bool lost = result_.state == State::Lost;

    if (lost && cell.mine && (cell.open || !cell.flag)) {
        glyph = cell.open ? U'X' : U'*';
        foreground = cell.open ? Theme::white : Theme::red;
        if (cell.open) background = Theme::paperError;
    } else if (cell.flag) {
        glyph = lost && !cell.mine ? U'!' : U'F';
        foreground = lost && !cell.mine ? Theme::red : Theme::accent;
    } else if (cell.open) {
        glyph = cell.adjacent == 0 ? U' ' : U'0' + cell.adjacent;
        foreground = hintColor[cell.adjacent];
        background = Theme::background;
    }

    if (preview && !cell.open && !cell.flag) {
        foreground = ready ? Theme::ink : Theme::white;
        background = ready ? Theme::accent : Theme::border;
    }
    if (position == selection && result_.state == State::Playing) {
        background = Theme::wood;
        if (glyph == U'#') foreground = Theme::white;
    }
    screen.cell(Screen::boardX(size_) + 2 * position.x, Screen::boardY(size_) + position.y,
                foreground, background, glyph);
}

void Minesweeper::draw(Screen& screen) const {
    const int left = Screen::boardX(size_);
    const int top = Screen::boardY(size_);
    screen.frame(left - 1, top - 1, 2 * size_ + 2, size_ + 2);
    const Point selection = contains(hover_) ? hover_ : cursor_;
    std::array<bool, maxCells> preview{};
    bool inspecting = false;
    int flagDelta = 0;
    if (result_.state == State::Playing && contains(selection)) {
        const int selected = index(selection);
        const Cell& cell = cells_[selected];
        if (cell.open && cell.adjacent > 0) {
            inspecting = true;
            flagDelta = cell.adjacent - neighboringFlags(selected);
            forNeighbors(selected, [&preview](int neighbor) { preview[neighbor] = true; });
        }
    }
    for (int y = 0; y < size_; ++y) {
        for (int x = 0; x < size_; ++x) {
            drawCell(screen, {x, y}, selection, preview[index({x, y})], flagDelta == 0);
        }
    }

    const std::string side = std::to_string(size_);
    screen.panelText(0, "Поле: " + side + "x" + side, Theme::accent);
    screen.panelText(1, difficultyName(difficulty_));
    screen.panelNumber(3, "Мин:", mines_);
    screen.panelNumber(4, "Флагов:", flags_);
    screen.panelNumber(5, "Осталось мин:", mines_ - flags_);
    screen.panelNumber(7, "Время, с:", static_cast<std::int64_t>(result_.timeMs / 1000));
    screen.panelText(10, "Enter/ЛКМ: открыть");
    screen.panelText(11, "Пробел: открыть");
    screen.panelText(12, "F/ПКМ: флаг");
    screen.panelText(14, "Цифра: открыть рядом", Theme::muted);
    if (inspecting) {
        if (flagDelta == 0) screen.panelText(15, "Можно раскрыть", Theme::accent);
        else if (flagDelta > 0) screen.panelNumber(15, "Ещё флагов:", flagDelta);
        else screen.panelNumber(15, "Лишних флагов:", -flagDelta);
    }
}

Result Minesweeper::result() const {
    return result_;
}
