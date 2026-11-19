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
            game.contract_ = Tetris::Contract::Gold;
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

    /** @brief Проверяет границы уровней, темп и сохранение поля при переходе. */
    static void levels() {
        Tetris game;
        const std::array<std::uint32_t, 3> expectedIntervals{1500, 950, 500};
        const std::array<std::uint32_t, 3> levelTwoIntervals{1363, 863, 454};
        const std::array<std::uint32_t, 3> levelThreeIntervals{1250, 791, 416};
        for (unsigned difficulty = 0; difficulty < expectedIntervals.size(); ++difficulty) {
            const Settings settings{static_cast<Difficulty>(difficulty), 1, 0};
            for (const int removed : {1, 2}) {
                game.start(settings);
                assert(game.fallInterval() == expectedIntervals[difficulty]);
                for (const auto previous : {9u, 19u}) {
                    game.lines_ = previous;
                    game.combo_ = 0;
                    game.contract_ = Tetris::Contract::Gold;
                    game.gold_ = {};
                    game.active_ = Tetris::Piece{0, 1, 3, 16};
                    game.board_ = {};
                    game.board_[14][0] = 2;
                    for (int row = Tetris::rows - removed; row < Tetris::rows; ++row) {
                        game.board_[row].fill(1);
                        game.board_[row][5] = 0;
                    }
                    const auto speedBefore = game.fallInterval();
                    const auto levelBefore = game.result().level;
                    const auto scoreBefore = game.result().score;
                    game.update(speedBefore);
                    assert(game.lines_ == previous + static_cast<unsigned>(removed));
                    assert(game.result().state == State::Playing && game.result().level == levelBefore + 1);
                    assert(game.result().score == scoreBefore + (removed == 1 ? 100 : 300) * levelBefore);
                    const auto expectedInterval = levelBefore == 1 ? levelTwoIntervals[difficulty]
                                                                    : levelThreeIntervals[difficulty];
                    assert(game.board_[14 + removed][0] == 2 && game.fallInterval() == expectedInterval);
                }
            }
            game.start(settings);
            assert(game.result().level == 1 && game.fallInterval() == expectedIntervals[difficulty]);
        }
        game.start(Settings{Difficulty::Normal, std::numeric_limits<std::uint32_t>::max(), 0});
        assert(game.fallInterval() == 100);
    }

    /** @brief Одинаковый ввод даёт ту же партию при другом разбиении игрового времени. */
    static void timing() {
        const Settings settings{Difficulty::Hard, 1, 0x12345678u};
        Tetris whole;
        Tetris split;
        whole.start(settings);
        split.start(settings);
        for (int i = 0; i < 3; ++i) {
            whole.handle({InputType::Left});
            split.handle({InputType::Left});
            whole.update(12911);
            split.update(10000);
            split.update(2000);
            split.update(911);
        }
        assert(whole.board_ == split.board_ && whole.rng_ == split.rng_);
        assert(whole.active_.shape == split.active_.shape && whole.active_.y == split.active_.y);
        assert(whole.result().timeMs == split.result().timeMs && whole.result().score == split.result().score);
    }

    /** @brief Проверяет проекцию, очередь и отсутствие побочных эффектов у отрисовки. */
    static void preview() {
        Tetris game;
        game.start({});
        game.board_[17][4] = 2;
        const auto marker = game.landing();
        const auto next = game.next_;
        const auto rng = game.rng_;
        const auto board = game.board_;
        Screen screen;
        screen.begin();
        game.draw(screen);
        const auto firstFrame = screen.plain();
        assert(firstFrame.find("СЛЕДУЮЩАЯ") != std::string::npos);
        assert(firstFrame.find("◇") != std::string::npos);
        screen.begin();
        game.draw(screen);
        assert(firstFrame == screen.plain() && game.rng_ == rng && game.board_ == board);
        for (int i = game.active_.y; i < marker.y; ++i) {
            game.handle({InputType::Down});
        }
        assert(game.active_.y == marker.y && game.board_ == board);
        game.handle({InputType::Down});
        assert(game.active_.shape == next && game.board_ != board);
    }

    /** @brief Проверяет выдачу целей, перенос метки и истечение срока. */
    static void contracts() {
        Tetris game;
        game.start({});
        game.board_[18][0] = game.board_[18][4] = 2;
        game.board_[17][1] = game.board_[17][3] = 2;
        game.updateContract({1, false});
        assert(game.contract_ == Tetris::Contract::Gold && game.rewards_ == 1);
        assert(game.result().score == 75);
        assert(game.gold_.y == 18 && game.gold_.x == 0 && game.gold_.moves == 6);
        game.board_[19].fill(1);
        auto cleared = game.clearLines();
        assert(cleared.count == 1 && !cleared.gold && game.gold_.y == 19);
        game.updateContract(cleared);
        assert(game.gold_.moves == 5);
        game.board_[19].fill(1);
        game.board_[18].fill(1);
        cleared = game.clearLines();
        assert(cleared.count == 2 && cleared.gold);
        game.updateContract(cleared);
        assert(game.contract_ == Tetris::Contract::Double && game.rewards_ == 2);
        assert(game.result().score == 325 && game.notice_ == Tetris::Notice::GoldPaid);

        game.updateContract({1, false});
        assert(game.contract_ == Tetris::Contract::Double);
        game.updateContract({2, false});
        assert(game.contract_ == Tetris::Contract::Streak);
        game.updateContract({1, false});
        assert(game.streakStarted_);
        game.updateContract({0, false});
        assert(!game.streakStarted_);
        game.updateContract({1, false});
        game.updateContract({1, false});
        assert(game.contract_ == Tetris::Contract::Gold && game.rewards_ == 4);
        assert(game.result().score == 675 && game.gold_.moves == 0);

        game.board_[19][0] = 2;
        game.updateContract({});
        assert(game.gold_.moves == 6);
        for (int i = 0; i < 6; ++i) {
            game.updateContract({});
        }
        assert(game.gold_.y == -1 && game.contract_ == Tetris::Contract::Double);
        assert(game.notice_ == Tetris::Notice::Missed && game.result().score == 675);
        game.start({});
        assert(game.contract_ == Tetris::Contract::First && game.gold_.y == -1);
        assert(game.rewards_ == 0 && game.noticeMs_ == 0);
    }

    /** @brief Проверяет бонус серии, её сброс и затухание эффекта по игровому времени. */
    static void series() {
        Tetris game;
        game.start({});
        game.contract_ = Tetris::Contract::Double;
        for (unsigned clear = 1; clear <= 2; ++clear) {
            game.board_ = {};
            game.board_[19].fill(1);
            game.board_[19][5] = 0;
            game.active_ = Tetris::Piece{0, 1, 3, 16};
            game.handle({InputType::Down});
            assert(game.combo_ == clear && game.noticeMs_ == 1200);
        }
        assert(game.result().score == 250);
        game.board_ = {};
        game.active_ = Tetris::Piece{0, 1, 3, 16};
        game.handle({InputType::Down});
        assert(game.combo_ == 0);
        game.update(1199);
        assert(game.noticeMs_ == 1);
        game.update(1);
        assert(game.noticeMs_ == 0);
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
    TetrisRulesTest::levels();
    TetrisRulesTest::timing();
    TetrisRulesTest::preview();
    TetrisRulesTest::contracts();
    TetrisRulesTest::series();
    TetrisRulesTest::lifecycle();
    std::cout << "PASS: bags, collisions, clearing, levels, ghost, preview, contracts, streaks, time and reset\n";
}
