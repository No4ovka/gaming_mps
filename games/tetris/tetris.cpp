/** @file tetris.cpp @brief Временная интеграция модуля Tetris. */
#include "tetris.h"
void Tetris::start(const Settings& settings) { stub_.start(settings); }
void Tetris::handle(const Input& input) { stub_.handle(input); }
void Tetris::update(std::uint32_t ms) { stub_.update(ms); }
void Tetris::draw(Screen& screen) const { stub_.draw(screen); }
Result Tetris::result() const { return stub_.result(); }
