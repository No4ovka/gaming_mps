/** @file minesweeper.cpp @brief Раскладка, раскрытие поля, управление и отрисовка сапёра. */
#include "minesweeper.h"
#include "input.h"
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
    size_ = 6 + 2 * static_cast<int>(result_.level);
    mines_ = mineTable[result_.level - 1][static_cast<unsigned>(settings.difficulty)];
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

Minesweeper::Layout Minesweeper::boardLayout() const {
    constexpr int cellWidths[] = {4, 4, 4, 3, 3};
    constexpr int cellHeights[] = {2, 2, 2, 2, 1};
    const int level = (size_ - 8) / 2;
    const int width = cellWidths[level];
    const int height = cellHeights[level];
    return {{(Screen::width - size_ * width) / 2, (Screen::height - size_ * height) / 2},
            width, height};
}

Minesweeper::Point Minesweeper::pointerCell(const Input& input) const {
    const Layout layout = boardLayout();
    const int x = input.x - layout.origin.x;
    const int y = input.y - layout.origin.y;
    if (x < 0 || x >= size_ * layout.cellWidth || y < 0 || y >= size_ * layout.cellHeight) return {};
    return {x / layout.cellWidth, y / layout.cellHeight};
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
        ++result_.score;
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

void Minesweeper::drawCell(Screen& screen, const Layout& layout, Point position,
                           bool selected, bool preview, bool ready) const {
    const Cell& cell = cells_[index(position)];
    char32_t glyph = U'▪';
    Color foreground = Theme::border;
    Color background = Theme::ink;
    const bool lost = result_.state == State::Lost;

    if (lost && cell.mine && (cell.open || !cell.flag)) {
        glyph = cell.open ? U'X' : U'■';
        foreground = Theme::red;
    } else if (cell.flag) {
        glyph = lost && !cell.mine ? U'!' : U'▼';
        foreground = Theme::red;
    } else if (cell.open) {
        glyph = cell.adjacent == 0 ? U' ' : U'0' + cell.adjacent;
        foreground = hintColor[cell.adjacent];
        background = Theme::background;
    }

    if (preview && !cell.open && !cell.flag) {
        foreground = ready ? Theme::accent : Theme::text;
    }
    if (selected && !cell.flag) {
        foreground = Theme::white;
        background = Theme::wood;
    }

    const int x = layout.origin.x + layout.cellWidth * position.x;
    const int y = layout.origin.y + layout.cellHeight * position.y;
    const int faceWidth = layout.cellWidth - 1;
    screen.fill(x, y, faceWidth, 1, background);
    screen.cell(x + (faceWidth - 1) / 2, y, foreground, background, glyph);
    if (selected) screen.text(x + faceWidth, y, "│", Theme::muted);
}

void Minesweeper::draw(Screen& screen) const {
    const Layout layout = boardLayout();
    const int left = layout.origin.x;
    const int top = layout.origin.y;
    const int width = size_ * layout.cellWidth;
    const int height = size_ * layout.cellHeight;
    screen.frame(left - 1, top - 1, width + 2, height + 2, Theme::border);
    screen.text(left + (width - 19) / 2, top - 1, " ДИНАМИТНАЯ ДОЛИНА ", Theme::accent);
    const Point selection = contains(hover_) ? hover_ : cursor_;
    const bool playing = result_.state == State::Playing;
    std::array<bool, maxCells> preview{};
    bool ready = false;
    if (playing && contains(selection)) {
        const int selected = index(selection);
        const Cell& cell = cells_[selected];
        if (cell.open && cell.adjacent > 0) {
            ready = neighboringFlags(selected) == cell.adjacent;
            forNeighbors(selected, [&preview](int neighbor) { preview[neighbor] = true; });
        }
    }
    for (int y = 0; y < size_; ++y) {
        for (int x = 0; x < size_; ++x) {
            const Point position{x, y};
            drawCell(screen, layout, position, playing && position == selection,
                     preview[index(position)], ready);
        }
    }

    const std::string side = std::to_string(size_);
    screen.panelNumber(0, "Счёт:", result_.score, Theme::white);
    screen.panelText(1, "─────────────────────", Theme::border);
    screen.panelText(3, "Поле: " + side + "x" + side);
    screen.panelNumber(5, "Динамиты:", mines_);
    screen.panelNumber(7, "Банданы:", flags_, Theme::red);
    screen.panelNumber(9, "Время, с:", static_cast<std::int64_t>(result_.timeMs / 1000));
}

Result Minesweeper::result() const {
    return result_;
}
