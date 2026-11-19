"""Run the real Tetris firmware through the shared QEMU UART launcher."""
from pathlib import Path
import re
import socket
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
sys.path[:0] = [str(ROOT), str(ROOT / "tools")]
from run import Qemu
from terminal import mouse_bytes
from tests.terminal_model import TerminalModel

SCREENS = ROOT / "artifacts" / "screens"
CODE = re.compile(r"P1-[0-9A-Z]{6}(?:-[0-9A-Z]{6}){3}")
DEMO_CODE = b"P1-040020-800000-000003-ZZYV67"  # Tetris, Normal, level 1, seed 0.

# Position/rotate, descend that many steps, expect this total number of cleared lines.
# The first four figures earn the first bounty. The next three clear the marked row.
DEMO_MOVES = (
    (b"aaa", 19, 0), (b"waa", 18, 0), (b"dddd", 19, 0), (b"d", 19, 1),
    (b"dd", 19, 1), (b"a", 18, 1), (b"waaaa", 18, 2), (b"waa", 17, 2),
    (b"dddd", 19, 2), (b"waaaa", 17, 2), (b"dd", 18, 3), (b"d", 19, 4),
    (b"wwwddddd", 18, 4), (b"wdd", 18, 4), (b"ww", 18, 5), (b"wdddd", 18, 5),
    (b"wwaaa", 18, 6), (b"d", 19, 6), (b"wdd", 17, 6), (b"wa", 18, 7),
    (b"d", 18, 7), (b"dddd", 18, 7), (b"aaa", 18, 8), (b"aa", 19, 8),
    (b"waaaa", 18, 9), (b"wwa", 18, 10),
)


class Session:
    """Send terminal events and observe complete ESP32 frames."""

    def __init__(self, uart):
        self.uart = uart
        self.screen = TerminalModel()

    def receive(self):
        try:
            data = self.uart.recv(16384)
        except socket.timeout:
            return
        assert data, "QEMU UART unexpectedly closed"
        self.screen.feed(data)

    def wait(self, condition, description, timeout=10):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.receive()
            if self.screen.complete and condition():
                return
        self.screen.snapshot(SCREENS, "tetris-failure")
        raise AssertionError(f"Expected {description}\n{self.screen.text()}")

    def text(self, expected):
        self.wait(lambda: expected in self.screen.text(), expected)

    def key(self, key):
        self.uart.sendall(key)

    def click(self, y):
        self.key(mouse_bytes(0, 20, y) + mouse_bytes(0, 20, y, True))

    def code(self):
        return CODE.search(self.screen.text()).group()

    def falling(self):
        return {(x, y) for x, y, glyph in self.field() if glyph == "◆"}

    def field(self):
        return ((x, y, self.screen.cells[y][x][0])
                for y in range(5, 25) for x in range(18, 38, 2))

    def start(self, adjustment, difficulty):
        self.click(10)
        self.text("Меняет начальный темп")
        if adjustment:
            self.key(adjustment)
        self.text(f"< {difficulty} >")
        self.click(16)
        self.wait(lambda: len(self.falling()) == 4 and "Счёт: 0" in self.screen.text(),
                  "a fresh Tetris board")
        self.text("До уровня: 10")


def play_demo(session):
    """Play a reproducible challenge through both rewards and the first level transition."""
    session.click(16)
    session.text("Введи код друга")
    session.key(DEMO_CODE + b"\r")
    session.text("Код проверен")
    session.key(b"\r")
    session.text("Убери 1 ряд")
    assert session.code() == DEMO_CODE.decode()
    assert any(glyph == "◇" for _, _, glyph in session.field())
    assert "СЛЕДУЮЩАЯ" in session.screen.text()
    for placed, (position, steps, lines) in enumerate(DEMO_MOVES, 1):
        session.key(position + b"s" * steps)
        expected_cells = 4 * placed - 10 * lines
        session.wait(lambda: sum(glyph in "▪*" for _, _, glyph in session.field()) == expected_cells,
                     f"figure {placed} to lock with {lines} cleared lines")
        if placed == 4:
            session.text("Счёт: 175")
            session.text("ЗОЛОТАЯ ЖИЛА")
            assert sum(glyph == "*" for _, _, glyph in session.field()) == 1
            session.screen.snapshot(SCREENS, "tetris-gold")
        elif placed == 7:
            session.text("Жила взята!")
            session.text("Счёт: 525")
            session.text("Наград: 2")
    session.text("Уровень: 2")
    session.text("Линии: 10")
    session.screen.snapshot(SCREENS, "tetris-level-2")
    session.key(b"r")
    session.text("Убери 1 ряд")
    session.text("Уровень: 1")
    session.text("Наград: 0")
    assert session.code() == DEMO_CODE.decode()
    session.key(b"q")
    session.text("Выбери игру")


def main():
    """Exercise all difficulties, movement, pause, replay and game over."""
    with Qemu(port=5557) as qemu:
        session = Session(qemu.serial)
        session.text("Выбери игру")
        session.start(b"a", "Лёгкая")
        original_code = session.code()
        initial = session.falling()
        session.key(b"\x1b[D")
        left = min(x for x, _ in initial)
        session.wait(lambda: min((x for x, _ in session.falling()), default=-1) == left - 2,
                     "a left move from an ANSI arrow")
        session.key(b"r")
        session.wait(lambda: session.falling() == initial, "the same opening piece after replay")
        assert session.code() == original_code
        session.key(b"p")
        session.text("Продолжить")
        deadline = time.monotonic() + 1.4
        while time.monotonic() < deadline:
            session.receive()
        session.key(b"p")
        session.wait(lambda: session.falling() == initial, "the same board after a long pause")
        session.key(b"s")
        session.wait(lambda: session.falling() == {(x, y + 1) for x, y in initial}, "a soft drop")
        session.screen.snapshot(SCREENS, "tetris-easy")
        session.key(b"q")
        session.text("Выбери игру")

        session.start(b"d", "Средняя")
        assert session.code() != original_code
        session.screen.snapshot(SCREENS, "tetris-normal")
        session.key(b"q")
        session.text("Выбери игру")

        session.start(b"d", "Сложная")
        hard_code = session.code()
        for _ in range(20):
            session.key(b"s" * 16)
            session.receive()
            if "Партия окончена" in session.screen.text():
                break
        session.text("Партия окончена")
        session.key(b"r")
        session.wait(lambda: len(session.falling()) == 4, "restarting after game over")
        assert session.code() == hard_code
        session.key(b"q")
        session.text("Выбери игру")
        play_demo(session)
    print("PASS: ESP32 UART and menu, all difficulties, pause, replay, loss, bounty, gold and level 2")


if __name__ == "__main__":
    main()
