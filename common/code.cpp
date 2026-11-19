/** @file code.cpp @brief 15-байтовый пакет, CRC16 и Crockford Base32. */
#include "code.h"
#include <array>
#include <cctype>

/** @brief Алфавит без визуально неоднозначных букв. */
static constexpr char alphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
/** @brief Вычисляет CRC-16/CCITT-FALSE блока. */
static std::uint16_t crc16(const std::uint8_t* bytes, std::size_t count) {
    std::uint16_t crc = 0xffff;
    for (std::size_t i = 0; i < count; ++i) {
        crc ^= static_cast<std::uint16_t>(bytes[i]) << 8;
        for (int bit = 0; bit < 8; ++bit)
            crc = static_cast<std::uint16_t>((crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0));
    }
    return crc;
}
bool validSettings(GameId game, const Settings& s) {
    if (static_cast<unsigned>(game) > 2 || static_cast<unsigned>(s.difficulty) > 2 || !s.level) return false;
    if (game != GameId::Minesweeper)
        return s.firstX == -1 && s.firstY == -1 && (game == GameId::Pong ? s.level <= 3 : s.level == 1);
    if (s.level > 5) return false;
    const int size = 6 + 2 * static_cast<int>(s.level);
    return s.firstX >= 0 && s.firstY >= 0 && s.firstX < size && s.firstY < size;
}
std::string encodeCode(GameId game, const Settings& s) {
    if (!validSettings(game, s)) return {};
    std::array<std::uint8_t, 15> packet{};
    packet[0] = 1;
    packet[1] = static_cast<std::uint8_t>(game);
    packet[2] = static_cast<std::uint8_t>(s.difficulty);
    for (int i = 0; i < 4; ++i) {
        packet[3 + i] = static_cast<std::uint8_t>(s.level >> (8 * i));
        packet[7 + i] = static_cast<std::uint8_t>(s.seed >> (8 * i));
    }
    packet[11] = static_cast<std::uint8_t>(s.firstX);
    packet[12] = static_cast<std::uint8_t>(s.firstY);
    const auto crc = crc16(packet.data(), 13);
    packet[13] = static_cast<std::uint8_t>(crc);
    packet[14] = static_cast<std::uint8_t>(crc >> 8);
    std::string code = "P1";
    for (int group = 0; group < 24; ++group) {
        if (group % 6 == 0) code += '-';
        unsigned value = 0;
        for (int bit = 0; bit < 5; ++bit) {
            const int position = group * 5 + bit;
            value = (value << 1) | ((packet[position / 8] >> (7 - position % 8)) & 1);
        }
        code += alphabet[value];
    }
    return code;
}
bool decodeCode(std::string_view code, GameId& game, Settings& settings, std::string& error) {
    std::string compact;
    for (unsigned char value : code) {
        if (value == '-' || std::isspace(value)) continue;
        compact += static_cast<char>(std::toupper(value));
    }
    if (compact.size() != 26 || compact.substr(0, 2) != "P1") {
        error = "Нужен полный код версии P1"; return false;
    }
    std::array<std::uint8_t, 15> packet{};
    for (int group = 0; group < 24; ++group) {
        char symbol = compact[group + 2];
        if (symbol == 'O') symbol = '0';
        if (symbol == 'I' || symbol == 'L') symbol = '1';
        unsigned value = 0;
        while (value < 32 && alphabet[value] != symbol) ++value;
        if (value == 32) { error = "В коде недопустимый символ"; return false; }
        for (int bit = 0; bit < 5; ++bit) {
            const int position = group * 5 + bit;
            packet[position / 8] |= ((value >> (4 - bit)) & 1) << (7 - position % 8);
        }
    }
    if (packet[0] != 1) { error = "Версия правил не поддерживается"; return false; }
    if (crc16(packet.data(), 13) != static_cast<unsigned>(packet[13] | (packet[14] << 8))) {
        error = "Код повреждён: проверь символы"; return false;
    }
    Settings decoded;
    const auto decodedGame = static_cast<GameId>(packet[1]);
    decoded.difficulty = static_cast<Difficulty>(packet[2]);
    decoded.level = decoded.seed = 0;
    for (int i = 0; i < 4; ++i) {
        decoded.level |= static_cast<std::uint32_t>(packet[3 + i]) << (8 * i);
        decoded.seed |= static_cast<std::uint32_t>(packet[7 + i]) << (8 * i);
    }
    decoded.firstX = packet[11] == 255 ? -1 : packet[11];
    decoded.firstY = packet[12] == 255 ? -1 : packet[12];
    if (!validSettings(decodedGame, decoded)) { error = "В коде неверные настройки"; return false; }
    game = decodedGame;
    settings = decoded;
    error.clear();
    return true;
}
