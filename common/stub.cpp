/** @file stub.cpp @brief Диагностический экран, общий для трёх игровых модулей. */
#include "stub.h"
#include "menu.h"
#include "screen.h"

Stub::Stub(GameId game) : game_(game) {}
void Stub::start(const Settings& settings) { settings_ = settings; input_ = {}; events_ = 0; time_ = 0; }
void Stub::handle(const Input& input) { input_ = input; ++events_; }
void Stub::update(std::uint32_t ms) { time_ += ms; }
Result Stub::result() const {
    Result result;
    result.level = settings_.level;
    result.timeMs = time_;
    result.firstX = settings_.firstX;
    result.firstY = settings_.firstY;
    return result;
}
void Stub::draw(Screen& screen) const {
    if (game_ == GameId::Minesweeper) {
        const int size = 6 + 2 * static_cast<int>(settings_.level);
        const int left = Screen::boardX(size), top = Screen::boardY(size);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x) {
                const bool pointer = input_.x >= left && input_.y >= top && input_.x < left + 2 * size
                    && input_.y < top + size && (input_.x - left) / 2 == x && input_.y - top == y;
                screen.cell(left + 2 * x, top + y, Theme::muted,
                            pointer ? Theme::border : Theme::background, U'·');
            }
        screen.text(0, 0, "Проверка клеток и указателя", Theme::accent);
        screen.text(0, Screen::height - 1, "Это сетка ввода, мины не сгенерированы", Theme::muted);
    } else {
        screen.text(5, 5, gameName(game_), Theme::accent);
        screen.text(5, 8, "Игра ещё не подключена");
        screen.text(5, 10, "Это заглушка для проверки общей основы", Theme::muted);
        screen.text(5, 13, "Нажми стрелку или щёлкни по полю");
    }
    screen.panelText(0, "Модуль: заглушка", Theme::accent);
    screen.panelText(2, "Последнее действие:", Theme::muted);
    screen.panelText(3, inputName(input_.type));
    screen.panelNumber(5, "x:", input_.x);
    screen.panelNumber(6, "y:", input_.y);
    screen.panelNumber(8, "События:", events_);
    screen.panelNumber(10, "Время, с:", static_cast<std::int64_t>(time_ / 1000));
    screen.panelNumber(12, "seed:", settings_.seed);
    screen.panelNumber(14, "Уровень:", settings_.level);
    screen.panelText(16, difficultyName(settings_.difficulty));
    screen.panelText(18, "Механика отсутствует", Theme::muted);
}
