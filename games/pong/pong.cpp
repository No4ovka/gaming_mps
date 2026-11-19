/** @file pong.cpp @brief Временная интеграция модуля Pong. */
#include "pong.h"
void Pong::start(const Settings& settings) { stub_.start(settings); }
void Pong::handle(const Input& input) { stub_.handle(input); }
void Pong::update(std::uint32_t ms) { stub_.update(ms); }
void Pong::draw(Screen& screen) const { stub_.draw(screen); }
Result Pong::result() const { return stub_.result(); }
