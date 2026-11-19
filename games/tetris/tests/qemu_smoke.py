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
        return {(x, y) for y, row in enumerate(self.screen.cells)
                for x, cell in enumerate(row) if cell[0] == "◆" and 18 <= x < 38 and 5 <= y < 25}

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
    print("PASS: ESP32 UART and menu, three difficulties, movement, pause, replay, loss")


if __name__ == "__main__":
    main()
