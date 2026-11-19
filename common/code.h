/** @file code.h @brief Версионированные коды стартовых условий. */
#pragma once
#include "game.h"
#include <string>
#include <string_view>

/** @brief Проверяет допустимость всех стартовых условий версии 1. */
bool validSettings(GameId game, const Settings& settings);
/** @brief Кодирует условия; возвращает пустую строку для неполного/неверного старта. */
std::string encodeCode(GameId game, const Settings& settings);
/** @brief Проверяет и декодирует код; при ошибке оставляет выходные условия неизменными. */
bool decodeCode(std::string_view code, GameId& game, Settings& settings, std::string& error);
