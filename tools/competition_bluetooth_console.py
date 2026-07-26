#!/usr/bin/env python3
"""比赛用 HC-04 终端调试台。

默认按 HC-04 地址自动寻找 Windows SPP COM 口，实时显示遥测，支持短命令
修改 MCU 安全测试变量，并持续保存 CSV、JSONL 和 latest.json。
"""

from __future__ import annotations

import argparse
import ctypes
import os
import queue
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path

import serial
from serial.tools import list_ports

from bluetooth_debug_monitor import (
    DEFAULT_LOG_DIR,
    FrameDecoder,
    ParsedFrame,
    SessionLogger,
    display,
    now_text,
    parse_frame,
)


if hasattr(sys.stdin, "reconfigure"):
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")


DEFAULT_HC04_ADDRESS = "042506180395"
DEFAULT_BAUD = 9600
IDLE_FRAME_SECONDS = 0.2

COMMAND_RANGES = {
    "KP": (0, 5000),
    "KI": (0, 5000),
    "KD": (0, 5000),
    "TGT": (-10000, 10000),
    "USER": (-100000, 100000),
}

COMMAND_ALIASES = {
    "kp": "KP",
    "ki": "KI",
    "kd": "KD",
    "t": "TGT",
    "tgt": "TGT",
    "target": "TGT",
    "u": "USER",
    "user": "USER",
}


@dataclass
class ConsoleState:
    started_at: float = field(default_factory=time.monotonic)
    connected: bool = False
    port: str = "等待发现"
    rx_frames: int = 0
    tx_frames: int = 0
    reconnects: int = 0
    last_rx_at: float = 0.0
    last_tel: dict[str, object] = field(default_factory=dict)
    last_message: str = "等待 HC-04 数据"
    last_raw: str = ""
    last_error: str = ""


def normalize_address(value: str) -> str:
    return "".join(character for character in value.upper() if character in "0123456789ABCDEF")


def find_hc04_port(address: str) -> str | None:
    target = normalize_address(address)
    for port in list_ports.comports():
        hwid = normalize_address(port.hwid or "")
        if target and target in hwid:
            return port.device
    return None


def list_serial_ports() -> None:
    ports = sorted(list_ports.comports(), key=lambda item: item.device)
    if not ports:
        print("没有发现串口。")
        return
    for port in ports:
        print(f"{port.device:<7} {port.description}  {port.hwid}")


def enable_virtual_terminal() -> bool:
    if os.name != "nt":
        return True
    try:
        kernel32 = ctypes.windll.kernel32
        handle = kernel32.GetStdHandle(-11)
        mode = ctypes.c_uint32()
        if not kernel32.GetConsoleMode(handle, ctypes.byref(mode)):
            return False
        return bool(kernel32.SetConsoleMode(handle, mode.value | 0x0004))
    except (AttributeError, OSError):
        return False


def field_text(fields: dict[str, object], key: str, width: int = 8) -> str:
    value = fields.get(key, "--")
    return f"{value!s:>{width}}"


def link_status(state: ConsoleState, stale_seconds: float) -> tuple[str, str]:
    if not state.connected:
        return "未连接", "\033[31m"
    if state.last_rx_at == 0.0:
        return "已连接/等数据", "\033[33m"
    age = time.monotonic() - state.last_rx_at
    if age > stale_seconds:
        return f"数据超时 {age:.1f}s", "\033[33m"
    return f"数据正常 {age:.1f}s", "\033[32m"


def render_dashboard(
    state: ConsoleState,
    command_buffer: str,
    stale_seconds: float,
    latest_path: Path | None,
    baud: int,
) -> None:
    status, color = link_status(state, stale_seconds)
    fields = state.last_tel
    uptime = int(time.monotonic() - state.started_at)
    log_text = str(latest_path) if latest_path else "等待建立日志"

    lines = [
        "\033[2J\033[H",
        "HC-04 比赛蓝牙调试台",
        "=" * 72,
        f"链路  {color}{status}\033[0m    端口 {state.port} @ {baud}    运行 {uptime}s",
        f"统计  RX {state.rx_frames}    TX {state.tx_frames}    重连 {state.reconnects}",
        "-" * 72,
        "PID 参数",
        f"  KP {field_text(fields, 'KP')}    KI {field_text(fields, 'KI')}    KD {field_text(fields, 'KD')}",
        "控制数据（当前固件为安全模拟值，不驱动电机）",
        f"  目标 T {field_text(fields, 'T')}    测量 M {field_text(fields, 'M')}",
        f"  误差 E {field_text(fields, 'E')}    输出 O {field_text(fields, 'O')}",
        f"  序号 S {field_text(fields, 'S')}    用户 U {field_text(fields, 'U')}    命令 R {field_text(fields, 'R')}",
        "-" * 72,
        f"最近消息：{state.last_message}",
        f"最近错误：{state.last_error or '无'}",
        f"实时文件：{log_text}",
        "-" * 72,
        "命令：kp/ki/kd/tgt/user 数值 | get | ping | raw [帧] | help | quit",
        f"> {command_buffer}",
    ]
    sys.stdout.write("\n".join(lines))
    sys.stdout.flush()


def parse_console_command(command: str) -> tuple[str, str | None]:
    text = command.strip()
    if not text:
        return "none", None

    lowered = text.lower()
    if lowered in {"q", "quit", "exit"}:
        return "quit", None
    if lowered in {"h", "help", "?"}:
        return "message", "示例：kp 1800、tgt -80、user 42、get、ping"
    if lowered in {"get", "g"}:
        return "send", "[GET]"
    if lowered in {"ping", "p"}:
        return "send", "[PING]"
    if lowered.startswith("raw "):
        payload = text[4:].strip()
        if not payload:
            raise ValueError("raw 后面缺少数据")
        return "send", payload
    if text.startswith("[") and text.endswith("]"):
        return "send", text

    parts = text.split()
    if len(parts) == 2 and parts[0].lower() in COMMAND_ALIASES:
        key = COMMAND_ALIASES[parts[0].lower()]
        try:
            value = int(parts[1], 10)
        except ValueError as exc:
            raise ValueError(f"{key} 必须是整数") from exc
        minimum, maximum = COMMAND_RANGES[key]
        if value < minimum or value > maximum:
            raise ValueError(f"{key} 范围为 {minimum}..{maximum}")
        return "send", f"[SET:{key}={value}]"

    raise ValueError("未知命令；输入 help 查看示例")


def keyboard_commands(command_buffer: str) -> tuple[str, list[str], bool]:
    import msvcrt

    commands: list[str] = []
    stop = False
    while msvcrt.kbhit():
        character = msvcrt.getwch()
        if character in {"\x00", "\xe0"}:
            if msvcrt.kbhit():
                msvcrt.getwch()
            continue
        if character == "\x03":
            stop = True
        elif character in {"\r", "\n"}:
            if command_buffer.strip():
                commands.append(command_buffer)
            command_buffer = ""
        elif character == "\x08":
            command_buffer = command_buffer[:-1]
        elif character.isprintable() and len(command_buffer) < 120:
            command_buffer += character
    return command_buffer, commands, stop


def input_worker(command_queue: queue.Queue[str], stop_event: threading.Event) -> None:
    while not stop_event.is_set():
        try:
            line = input()
        except (EOFError, KeyboardInterrupt):
            stop_event.set()
            return
        command_queue.put(line)


def record_frame(
    logger: SessionLogger | None,
    direction: str,
    frame: ParsedFrame,
) -> None:
    if logger:
        logger.write(now_text(), direction, frame)


def show_plain(direction: str, frame: ParsedFrame, plain: bool) -> None:
    if plain:
        display(direction, frame)


def run_console(args: argparse.Namespace) -> int:
    state = ConsoleState()
    decoder = FrameDecoder()
    command_queue: queue.Queue[str] = queue.Queue()
    stop_event = threading.Event()
    serial_port: serial.Serial | None = None
    logger: SessionLogger | None = None
    command_buffer = ""
    last_render_at = 0.0
    last_byte_at = time.monotonic()
    next_connect_at = 0.0
    initial_commands_queued = False
    dashboard = sys.stdin.isatty() and sys.stdout.isatty() and not args.plain
    if dashboard and not enable_virtual_terminal():
        dashboard = False

    if not dashboard and sys.stdin.isatty() and not args.no_interactive:
        threading.Thread(
            target=input_worker,
            args=(command_queue, stop_event),
            daemon=True,
        ).start()

    try:
        while not stop_event.is_set():
            now = time.monotonic()
            if args.duration > 0 and (now - state.started_at) >= args.duration:
                break

            if dashboard:
                command_buffer, commands, keyboard_stop = keyboard_commands(command_buffer)
                for command in commands:
                    command_queue.put(command)
                if keyboard_stop:
                    break

            if serial_port is None and now >= next_connect_at:
                port = args.port or find_hc04_port(args.address)
                if port is None:
                    state.connected = False
                    state.port = "未找到"
                    state.last_error = (
                        "未找到 HC-04 SPP COM；运行 BluetoothSppTool.exe scan 8 / pair HC-04 1234 8"
                    )
                    if args.no_reconnect:
                        break
                    next_connect_at = now + args.reconnect_delay
                else:
                    state.port = port
                    try:
                        serial_port = serial.Serial(
                            port=port,
                            baudrate=args.baud,
                            bytesize=serial.EIGHTBITS,
                            parity=serial.PARITY_NONE,
                            stopbits=serial.STOPBITS_ONE,
                            timeout=0.05,
                            write_timeout=1.0,
                            xonxoff=False,
                            rtscts=False,
                            dsrdtr=False,
                        )
                        serial_port.reset_input_buffer()
                        decoder = FrameDecoder()
                        state.connected = True
                        state.last_error = ""
                        state.last_message = f"已连接 {port}，请求当前数据"
                        if logger is None and not args.no_log:
                            logger = SessionLogger(Path(args.log_dir).resolve(), port, args.baud)
                        command_queue.put("get")
                        if not initial_commands_queued:
                            for command in args.send or []:
                                command_queue.put(command)
                            initial_commands_queued = True
                    except (OSError, serial.SerialException) as exc:
                        serial_port = None
                        state.connected = False
                        state.last_error = f"打开 {port} 失败：{exc}"
                        state.reconnects += 1
                        if args.no_reconnect:
                            break
                        next_connect_at = now + args.reconnect_delay

            while not command_queue.empty():
                command = command_queue.get_nowait()
                try:
                    action, payload = parse_console_command(command)
                    if action == "quit":
                        stop_event.set()
                        break
                    if action == "message":
                        state.last_message = payload or ""
                        continue
                    if action != "send" or payload is None:
                        continue
                    if args.read_only:
                        state.last_error = "当前为只读模式，未发送命令"
                        continue
                    if serial_port is None:
                        state.last_error = "HC-04 尚未连接，命令未发送"
                        continue

                    serial_port.write(payload.encode("utf-8"))
                    serial_port.flush()
                    frame = parse_frame(payload)
                    state.tx_frames += 1
                    state.last_message = f"TX {payload}"
                    record_frame(logger, "TX", frame)
                    show_plain("TX", frame, not dashboard)
                    time.sleep(args.command_delay)
                except ValueError as exc:
                    state.last_error = str(exc)
                    if not dashboard:
                        print(f"命令错误：{exc}")
                except (OSError, serial.SerialException) as exc:
                    state.last_error = f"发送失败：{exc}"
                    serial_port.close()
                    serial_port = None
                    state.connected = False
                    state.reconnects += 1
                    next_connect_at = time.monotonic() + args.reconnect_delay
                    break

            if serial_port is not None:
                try:
                    data = serial_port.read(serial_port.in_waiting or 1)
                    if data:
                        last_byte_at = time.monotonic()
                        raw_frames = decoder.feed(data)
                    elif decoder.buffer and (time.monotonic() - last_byte_at) >= IDLE_FRAME_SECONDS:
                        raw_frames = decoder.flush_idle()
                    else:
                        raw_frames = []

                    for raw in raw_frames:
                        frame = parse_frame(raw)
                        state.rx_frames += 1
                        state.last_rx_at = time.monotonic()
                        state.last_raw = frame.raw
                        if frame.kind == "TEL":
                            state.last_tel = dict(frame.fields)
                            state.last_message = "收到 MCU 遥测"
                        elif frame.kind == "ACK":
                            state.last_message = "MCU ACK：" + " ".join(
                                f"{key}={value}" for key, value in frame.fields.items()
                            )
                        elif frame.kind == "NACK":
                            state.last_error = "MCU NACK：" + frame.raw
                        else:
                            state.last_message = "RX " + frame.raw
                        record_frame(logger, "RX", frame)
                        show_plain("RX", frame, not dashboard)
                except (OSError, serial.SerialException) as exc:
                    state.last_error = f"链路断开：{exc}"
                    serial_port.close()
                    serial_port = None
                    state.connected = False
                    state.reconnects += 1
                    if args.no_reconnect:
                        break
                    next_connect_at = time.monotonic() + args.reconnect_delay

            if dashboard and (time.monotonic() - last_render_at) >= args.refresh:
                latest_path = logger.latest_path if logger else None
                render_dashboard(state, command_buffer, args.stale_seconds, latest_path, args.baud)
                last_render_at = time.monotonic()

            time.sleep(0.01)

    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        if serial_port is not None:
            serial_port.close()
        if logger:
            logger.close()
        if dashboard:
            sys.stdout.write("\033[0m\n")

    print(f"结束：RX {state.rx_frames} 帧，TX {state.tx_frames} 帧，重连 {state.reconnects} 次。")
    if logger:
        print(f"CSV:    {logger.csv_path}")
        print(f"JSONL:  {logger.jsonl_path}")
        print(f"最新帧: {logger.latest_path}")
    return 0 if state.rx_frames > 0 else 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="HC-04 比赛蓝牙 PID 调试台")
    parser.add_argument("--port", help="手工指定 COM；默认按蓝牙地址自动查找")
    parser.add_argument("--address", default=DEFAULT_HC04_ADDRESS, help="HC-04 蓝牙地址")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="默认 9600")
    parser.add_argument("--list", action="store_true", help="列出串口后退出")
    parser.add_argument("--send", action="append", help="启动后发送短命令，可重复")
    parser.add_argument("--duration", type=float, default=0, help="运行秒数；0 为持续运行")
    parser.add_argument("--plain", action="store_true", help="逐帧打印，不刷新仪表盘")
    parser.add_argument("--read-only", action="store_true", help="禁止电脑向 MCU 发送命令")
    parser.add_argument("--no-interactive", action="store_true", help="禁用键盘命令")
    parser.add_argument("--no-reconnect", action="store_true", help="断线后退出")
    parser.add_argument("--reconnect-delay", type=float, default=1.0, help="重连间隔秒数")
    parser.add_argument("--command-delay", type=float, default=0.25, help="连续命令发送间隔")
    parser.add_argument("--stale-seconds", type=float, default=2.0, help="数据超时判定")
    parser.add_argument("--refresh", type=float, default=0.1, help="仪表盘刷新间隔")
    parser.add_argument("--no-log", action="store_true", help="不保存日志")
    parser.add_argument("--log-dir", default=str(DEFAULT_LOG_DIR), help="日志目录")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    if args.list:
        list_serial_ports()
        return 0
    return run_console(args)


if __name__ == "__main__":
    raise SystemExit(main())
