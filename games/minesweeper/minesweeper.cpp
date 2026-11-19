/** @file minesweeper.cpp @brief Временная интеграция модуля Minesweeper. */
#include "minesweeper.h"
void Minesweeper::start(const Settings& settings) { stub_.start(settings); }
void Minesweeper::handle(const Input& input) { stub_.handle(input); }
void Minesweeper::update(std::uint32_t ms) { stub_.update(ms); }
void Minesweeper::draw(Screen& screen) const { stub_.draw(screen); }
Result Minesweeper::result() const { return stub_.result(); }
