/** @file rules.cpp @brief Проверки механики тетриса на тестовых полях. */
#include "input.h"
#include "screen.h"
#include "tetris.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>

/** @brief Граничные сценарии механики, включая подготовленные раскладки. */
struct TetrisRulesTest {
    /** @brief Проверяет все фигуры и повторяемость мешков для seed 0. */
    static void bags() {
        Tetris game;
        for (unsigned attempt = 0; attempt < 2; ++attempt) {
            game.start({});
            for (auto& bag : order[attempt]) {
                for (auto& shape : bag) {
                    shape = game.active_.shape;
                    game.spawn();
                }
                auto sorted = bag;
                std::sort(sorted.begin(), sorted.end());
                const std::array<std::uint8_t, 7> expected{0, 1, 2, 3, 4, 5, 6};
                assert(sorted == expected);
            }
        }
        assert(order[0] == order[1]);
    }

    /** @brief Проверяет границы, поворот и препятствия для всех типов фигур. */
    static void collisions() {
        Tetris game;
        for (std::uint8_t shape = 0; shape < Tetris::shapeCount; ++shape) {
            for (int direction : {-1, 1}) {
                game.start({});
                game.active_ = Tetris::Piece{shape, 0, 3, 5};
                for (int i = 0; i < Tetris::columns; ++i) {
                    game.shift(direction);
                }
                auto outside = game.active_;
                outside.x += direction;
                assert(!game.fits(outside));
                for (int i = 0; i < 4; ++i) {
                    game.rotate();
                    assert(game.fits(game.active_));
                }
            }
        }

        game.start({});
        game.active_ = Tetris::Piece{0, 0, 3, 18};
        game.rotate();
        assert(game.active_.rotation == 1 && game.active_.y == 16);
        const auto before = game.active_;
        game.board_[19][6] = 1;
        game.shift(1);
        assert(game.active_.x == before.x);
        assert(game.fits(game.active_));
    }

    /** @brief Проверяет одновременную очистку 1–4 линий с переносом клеток. */
    static void lines() {
        const std::array<std::uint32_t, 5> expectedPoints{0, 100, 300, 500, 800};
        for (int count = 1; count <= 4; ++count) {
            Tetris game;
            game.start({});
            game.active_ = Tetris::Piece{0, 1, 3, 16};
            game.board_[14][0] = 2;
            for (int row = Tetris::rows - count; row < Tetris::rows; ++row) {
                game.board_[row].fill(1);
                game.board_[row][5] = 0;
            }
            assert(game.fits(game.active_));
            game.handle({InputType::Down});
            assert(game.lines_ == static_cast<unsigned>(count));
            assert(game.result().score == expectedPoints[count]);
            assert(game.board_[14 + count][0] == 2);
            assert(game.board_[14][0] == 0);
            for (int row = 0; row < count; ++row) {
                assert(game.board_[row] == Tetris::Row{});
            }
            unsigned occupied = 0;
            for (const auto& row : game.board_) {
                occupied += std::count_if(row.begin(), row.end(), [](auto value) { return value != 0; });
            }
            assert(occupied == 5u - static_cast<unsigned>(count));
        }
    }

    /** @brief Проверяет деление времени, верхнюю границу и сброс после поражения. */
    static void lifecycle() {
        const Settings settings{Difficulty::Normal, 1, 0};
        Tetris game;
        game.start(settings);
        Screen screen;
        screen.begin();
        game.draw(screen);
        const auto initial = screen.plain();
        game.handle({InputType::Open});
        game.handle({InputType::Confirm});
        game.handle({InputType::PointerLeft, 1, 1});
        game.draw(screen);
        assert(initial == screen.plain());
        const auto interval = game.fallInterval();
        game.update(interval - 1);
        assert(game.active_.y == 0 && game.result().timeMs == interval - 1);
        game.update(1);
        assert(game.active_.y == 1 && game.result().timeMs == interval);

        game.update(std::numeric_limits<std::uint32_t>::max());
        assert(game.result().state == State::Lost);
        assert(game.result().timeMs < 120000);
        const auto stopped = game.result();
        const auto board = game.board_;
        game.handle({InputType::Down});
        game.update(10000);
        assert(game.result().timeMs == stopped.timeMs && game.board_ == board);

        game.start(settings);
        assert(game.result().state == State::Playing);
        assert(game.result().timeMs == 0 && game.result().score == 0 && game.lines_ == 0);
        screen.begin();
        game.draw(screen);
        assert(screen.plain() == initial);
    }

    static inline std::array<std::array<std::array<std::uint8_t, 7>, 2>, 2> order{};
    ///< Очереди в двух одинаково запущенных партиях.
};

/** @brief Запускает проверки модуля. */
int main() {
    TetrisRulesTest::bags();
    TetrisRulesTest::collisions();
    TetrisRulesTest::lines();
    TetrisRulesTest::lifecycle();
    std::cout << "PASS: bags, collisions, line clearing, game time, game over and reset\n";
}
