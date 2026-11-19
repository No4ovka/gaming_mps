"""Real ESP32/QEMU smoke checks over UART, followed by source-level GDB steps."""
import re
import shutil
import socket
import subprocess
import sys
import time
from run import Qemu, ROOT
from terminal import mouse_bytes
sys.path.insert(0, str(ROOT))
from tests.terminal_model import TerminalModel


def main():
    """Exercise the compiled firmware through its real UART input/output path."""
    artifacts = ROOT / 'artifacts'
    artifacts.mkdir(exist_ok=True)
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as occupied:
        occupied.bind(('127.0.0.1', 0))
        occupied.listen()
        try:
            with Qemu(port=occupied.getsockname()[1]):
                raise AssertionError('An occupied UART port must be rejected')
        except RuntimeError as error:
            assert 'занят' in str(error), error
    model = TerminalModel()
    with Qemu(port=5556) as qemu, (artifacts / 'uart-smoke.bin').open('wb') as raw:
        def pump(seconds):
            """Receive and decode UART during a bounded observation interval."""
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                try:
                    data = qemu.serial.recv(16384)
                    if not data: raise RuntimeError('UART closed unexpectedly')
                    raw.write(data)
                    model.feed(data)
                except socket.timeout: pass

        def wait(text, seconds=8):
            """Require the expected text to appear in a complete visible frame."""
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                pump(0.15)
                if model.complete and text in model.text(): return
            model.snapshot(artifacts / 'screens', 'failure')
            raise AssertionError(f'Expected screen text: {text}')

        def send(data, expected=None):
            """Send real terminal bytes to the emulated UART."""
            qemu.serial.sendall(data)
            if expected: wait(expected)
            else: pump(0.3)

        def click(x, y, expected=None, right=False):
            """Click a zero-based terminal cell through the production SGR protocol."""
            button = 2 if right else 0
            send(mouse_bytes(button, x, y) + mouse_bytes(button, x, y, True), expected)

        def code():
            """Extract the complete challenge code visible in the firmware frame."""
            found = re.search(r'P1-[0-9A-Z]{6}(?:-[0-9A-Z]{6}){3}', model.text())
            assert found, model.text()
            return found[0]

        wait('Выбери игру', 15)
        model.snapshot(artifacts / 'screens', 'main')
        click(20, 12, 'Меняет долю мин')
        click(20, 9, 'Сложная')
        for _ in range(4): click(20, 12)
        wait('16x16')
        model.snapshot(artifacts / 'screens', 'settings')
        click(20, 16, 'Модуль: заглушка')
        wait('Уровень 5')
        click(12, 7, 'ЛКМ')
        wait('x: 10'); wait('y: 2')
        click(43, 22, 'ПКМ', right=True)
        wait('x: 41'); wait('y: 17')
        model.snapshot(artifacts / 'screens', 'mines-mouse')
        send(mouse_bytes(35, 60, 10), 'Наведение')
        wait('x: -1'); wait('y: -1')
        before = re.search(r'События: (\d+)', model.text())[1]
        click(54, 24)
        assert re.search(r'События: (\d+)', model.text())[1] == before
        send(b'p', 'Пауза')
        model.snapshot(artifacts / 'screens', 'pause')
        pump(1.1)
        send(b'p', 'Проверка клеток')
        seed = re.search(r'seed: (\d+)', model.text())[1]
        send(b'r', 'События: 0')
        assert re.search(r'seed: (\d+)', model.text())[1] == seed
        send(b'q', 'Выбери игру')
        click(20, 10, 'Меняет начальный темп')
        click(20, 16, 'Игра ещё не подключена')
        first_code = code()
        send(b'\x1b[D', 'Влево')
        send(b'p', 'Пауза')
        send(b'r', 'События: 0')
        assert code() == first_code
        send(b'q', 'Выбери игру')
        click(20, 10, 'Меняет начальный темп')
        click(20, 16, 'Игра ещё не подключена')
        assert code() != first_code
        send(b'q', 'Выбери игру')
        click(20, 16, 'Введи код друга')
        vector = b'P1-041020-R00007-GNHM2B-ZZYZJ4'
        send(vector[:-1] + b'5\r', 'Код повреждён')
        model.snapshot(artifacts / 'screens', 'invalid-code')
        send(b'\x7f' * len(vector) + vector + b'\r', 'Код проверен')
        wait('Пин-понг'); wait('Начальный уровень: 3')
        model.snapshot(artifacts / 'screens', 'code-confirm')
        send(b'\r', 'Игра ещё не подключена')
        assert code() == vector.decode()
        model.snapshot(artifacts / 'screens', 'pong-code')
        send(b'q', 'Выбери игру')
        click(20, 18, 'Управление')
        wait('ПКМ / F — флаг')
        model.snapshot(artifacts / 'screens', 'help')
        send(b'\x1b')
        wait('Выбери игру')
        print('PASS: QEMU boot, settings, all 3 stubs, left/right/hover, bounds, pause, restart, fresh seed, valid/invalid codes, help')

    with Qemu(debug=True, port=5556) as qemu:
        debugger = shutil.which('xtensa-esp32-elf-gdb')
        assert debugger, 'GDB is absent from PATH'
        script = artifacts / 'step.gdb'
        script.write_text('set pagination off\nset confirm off\nset remotetimeout 15\n'
            'target remote 127.0.0.1:3333\nthbreak app_main\ncontinue\nlist\n'
            'echo FIRST_PC=\\n\np/x $pc\nnext\necho SECOND_PC=\\n\np/x $pc\n'
            'next\necho THIRD_PC=\\n\np/x $pc\nframe\ndetach\n', encoding='utf-8')
        result = subprocess.run([debugger, '--nx', '--batch', str(ROOT / 'build/gaming_mps.elf'),
                                 '-x', str(script)], capture_output=True, text=True,
                                encoding='utf-8', errors='replace', timeout=60)
        output = result.stdout + result.stderr
        (artifacts / 'gdb-step.log').write_text(output, encoding='utf-8')
        pcs = re.findall(r'\$[123] = (0x[0-9a-f]+)', output)
        assert result.returncode == 0 and len(set(pcs)) == 3, output
        assert 'main.cpp' in output and 'app_main' in output, output
        print('PASS: GDB breakpoint in app_main and two source steps:', ' -> '.join(pcs))
    print('ALL QEMU CHECKS PASSED')


if __name__ == '__main__':
    main()
