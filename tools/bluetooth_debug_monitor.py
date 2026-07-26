#!/usr/bin/env python3
"""HC-04/HC-05 串口调试监视器。

终端直接显示接收帧，同时保存 CSV、JSONL 和最新一帧 latest.json。
兼容现有 [MASTER:PING] 测试帧，也支持后续 MCU 输出：
    @TEL,TICK=123,KP=1.5,KI=0.2,KD=0.1,OUT=-12\r\n
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import queue
import sys
import threading
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any

try:
    import serial
    from serial.tools import list_ports
except ImportError as exc:
    print("缺少 pyserial，请运行：py -3 -m pip install pyserial", file=sys.stderr)
    raise SystemExit(2) from exc


if hasattr(sys.stdin, "reconfigure"):
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")


PROJECT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_LOG_DIR = PROJECT_ROOT / "build" / "Debug" / "bluetooth_logs"
MAX_PENDING_BYTES = 4096


@dataclass
class ParsedFrame:
    kind: str
    source: str
    event: str
    fields: dict[str, Any]
    raw: str


class FrameDecoder:
    """从串口字节流中提取 [方括号帧] 和 CR/LF 文本帧。"""

    def __init__(self) -> None:
        self.buffer = bytearray()

    def feed(self, data: bytes) -> list[str]:
        self.buffer.extend(data)
        frames: list[str] = []

        while self.buffer:
            while self.buffer and self.buffer[0] in (ord("\r"), ord("\n")):
                del self.buffer[0]
            if not self.buffer:
                break

            if self.buffer[0] == ord("["):
                end = self.buffer.find(b"]")
                if end < 0:
                    break
                frames.append(self._decode(bytes(self.buffer[: end + 1])))
                del self.buffer[: end + 1]
                continue

            line_end = self._first_line_end()
            bracket_start = self.buffer.find(b"[")

            if line_end >= 0 and (bracket_start < 0 or line_end < bracket_start):
                frames.append(self._decode(bytes(self.buffer[:line_end])))
                del self.buffer[: line_end + 1]
                continue

            if bracket_start > 0:
                prefix = bytes(self.buffer[:bracket_start])
                if prefix:
                    frames.append(self._decode(prefix))
                del self.buffer[:bracket_start]
                continue

            if len(self.buffer) > MAX_PENDING_BYTES:
                frames.append(self._decode(bytes(self.buffer)))
                self.buffer.clear()
            break

        return [frame for frame in frames if frame]

    def flush_idle(self) -> list[str]:
        if not self.buffer:
            return []
        raw = self._decode(bytes(self.buffer))
        self.buffer.clear()
        return [raw] if raw else []

    def _first_line_end(self) -> int:
        cr = self.buffer.find(b"\r")
        lf = self.buffer.find(b"\n")
        candidates = [index for index in (cr, lf) if index >= 0]
        return min(candidates) if candidates else -1

    @staticmethod
    def _decode(data: bytes) -> str:
        return data.decode("utf-8", errors="replace").strip()


class SessionLogger:
    CSV_FIELDS = (
        "timestamp",
        "direction",
        "port",
        "kind",
        "source",
        "event",
        "fields_json",
        "raw",
    )

    def __init__(self, log_dir: Path, port: str, baud: int) -> None:
        log_dir.mkdir(parents=True, exist_ok=True)
        session_name = datetime.now().strftime("bluetooth_%Y%m%d_%H%M%S")
        self.csv_path = log_dir / f"{session_name}.csv"
        self.jsonl_path = log_dir / f"{session_name}.jsonl"
        self.latest_path = log_dir / "latest.json"
        self.port = port
        self.baud = baud

        self._csv_file = self.csv_path.open("w", newline="", encoding="utf-8-sig")
        self._jsonl_file = self.jsonl_path.open("w", encoding="utf-8")
        self._csv_writer = csv.DictWriter(self._csv_file, fieldnames=self.CSV_FIELDS)
        self._csv_writer.writeheader()
        self._csv_file.flush()

    def write(self, timestamp: str, direction: str, frame: ParsedFrame) -> None:
        record = {
            "timestamp": timestamp,
            "direction": direction,
            "port": self.port,
            "baud": self.baud,
            "kind": frame.kind,
            "source": frame.source,
            "event": frame.event,
            "fields": frame.fields,
            "raw": frame.raw,
        }
        self._csv_writer.writerow(
            {
                "timestamp": timestamp,
                "direction": direction,
                "port": self.port,
                "kind": frame.kind,
                "source": frame.source,
                "event": frame.event,
                "fields_json": json.dumps(frame.fields, ensure_ascii=False),
                "raw": frame.raw,
            }
        )
        self._jsonl_file.write(json.dumps(record, ensure_ascii=False) + "\n")
        self._csv_file.flush()
        self._jsonl_file.flush()

        if direction == "RX":
            latest_tmp = self.latest_path.with_suffix(".tmp")
            latest_tmp.write_text(
                json.dumps(record, ensure_ascii=False, indent=2), encoding="utf-8"
            )
            os.replace(latest_tmp, self.latest_path)

    def close(self) -> None:
        self._csv_file.close()
        self._jsonl_file.close()


def parse_scalar(value: str) -> Any:
    value = value.strip()
    lowered = value.lower()
    if lowered in {"true", "false"}:
        return lowered == "true"
    try:
        return int(value, 0)
    except ValueError:
        pass
    try:
        return float(value)
    except ValueError:
        return value


def parse_frame(raw: str) -> ParsedFrame:
    text = raw.strip()
    if text.startswith("[") and text.endswith("]"):
        payload = text[1:-1].strip()
        source, separator, event = payload.partition(":")
        return ParsedFrame(
            kind="LINK",
            source=source if separator else "",
            event=event if separator else payload,
            fields={},
            raw=text,
        )

    tokens = next(csv.reader([text])) if text else []
    head = tokens[0].lstrip("@").strip() if tokens else "RAW"
    fields: dict[str, Any] = {}
    positional: list[Any] = []

    for token in tokens[1:]:
        key, separator, value = token.partition("=")
        if separator:
            fields[key.strip()] = parse_scalar(value)
        elif token.strip():
            positional.append(parse_scalar(token))
    if positional:
        fields["values"] = positional

    kind = head.upper() if head else "RAW"
    if text.upper() in {"OK", "ERROR"} or text.upper().startswith("ERROR:"):
        kind = "AT"

    return ParsedFrame(kind=kind, source="", event="", fields=fields, raw=text)


def now_text() -> str:
    return datetime.now().astimezone().isoformat(timespec="milliseconds")


def display(direction: str, frame: ParsedFrame) -> None:
    clock = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    summary_parts = []
    if frame.source:
        summary_parts.append(f"source={frame.source}")
    if frame.event:
        summary_parts.append(f"event={frame.event}")
    summary_parts.extend(f"{key}={value}" for key, value in frame.fields.items())
    summary = "  ".join(summary_parts) if summary_parts else frame.raw
    print(f"[{clock}] {direction:<2} {frame.kind:<8} {summary}", flush=True)


def show_ports() -> int:
    ports = sorted(list_ports.comports(), key=lambda item: item.device)
    if not ports:
        print("没有发现串口。")
        return 1
    print("可用串口：")
    for item in ports:
        hardware = f"VID:PID={item.vid:04X}:{item.pid:04X}" if item.vid is not None else ""
        print(f"  {item.device:<7} {item.description} {hardware}".rstrip())
    return 0


def select_port(requested: str | None) -> str:
    if requested:
        return requested.upper()

    ch340_ports = [
        item.device
        for item in list_ports.comports()
        if item.vid == 0x1A86 and item.pid == 0x7523
    ]
    if len(ch340_ports) == 1:
        return ch340_ports[0]
    if not ch340_ports:
        raise RuntimeError("没有找到 CH340，请用 --list 查看串口，并通过 --port 指定。")
    raise RuntimeError(
        "发现多个 CH340：" + ", ".join(ch340_ports) + "；请通过 --port 指定。"
    )


def input_worker(commands: queue.Queue[str], stop_event: threading.Event) -> None:
    while not stop_event.is_set():
        try:
            line = input()
        except (EOFError, KeyboardInterrupt):
            stop_event.set()
            return
        if line.strip().lower() in {"/q", "/quit", "/exit"}:
            stop_event.set()
            return
        if line:
            commands.put(line)


def run_demo(log_dir: Path, no_log: bool) -> int:
    samples = [
        "[MASTER:PING]",
        "@TEL,TICK=100,KP=1.2,KI=0.08,KD=0.3,ERROR=15,OUT=26",
        "@TEL,TICK=110,KP=1.2,KI=0.08,KD=0.3,ERROR=8,OUT=17",
    ]
    logger = None if no_log else SessionLogger(log_dir, "DEMO", 9600)
    try:
        for raw in samples:
            frame = parse_frame(raw)
            timestamp = now_text()
            display("RX", frame)
            if logger:
                logger.write(timestamp, "RX", frame)
            time.sleep(0.15)
    finally:
        if logger:
            logger.close()
            print(f"CSV:    {logger.csv_path}")
            print(f"JSONL:  {logger.jsonl_path}")
            print(f"最新帧: {logger.latest_path}")
    return 0


def run_monitor(args: argparse.Namespace) -> int:
    port = select_port(args.port)
    log_dir = Path(args.log_dir).resolve()
    logger = None if args.no_log else SessionLogger(log_dir, port, args.baud)
    decoder = FrameDecoder()
    commands: queue.Queue[str] = queue.Queue()
    stop_event = threading.Event()
    rx_frames = 0
    tx_frames = 0
    at_ok = False
    last_rx_time = time.monotonic()
    started = time.monotonic()

    try:
        with serial.Serial(
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
        ) as serial_port:
            serial_port.reset_input_buffer()
            print(f"已打开 {port}，{args.baud} 8N1。Ctrl+C 或输入 /quit 退出。")
            if logger:
                print(f"实时数据文件：{logger.latest_path}")

            interactive = args.interactive or (sys.stdin.isatty() and not args.no_interactive)
            if interactive:
                print("可直接输入文本后回车发送，例如 [PC:TEST]")
                threading.Thread(
                    target=input_worker, args=(commands, stop_event), daemon=True
                ).start()

            initial_commands = list(args.send or [])
            if args.at_test:
                initial_commands.insert(0, "AT")
            for command in initial_commands:
                commands.put(command)

            duration = args.duration
            if args.at_test and duration == 0:
                duration = 2.0

            while not stop_event.is_set():
                if duration > 0 and (time.monotonic() - started) >= duration:
                    break

                while not commands.empty():
                    command = commands.get_nowait()
                    payload = command.encode("utf-8")
                    serial_port.write(payload)
                    serial_port.flush()
                    frame = parse_frame(command)
                    timestamp = now_text()
                    display("TX", frame)
                    if logger:
                        logger.write(timestamp, "TX", frame)
                    tx_frames += 1
                    time.sleep(args.send_delay)

                data = serial_port.read(serial_port.in_waiting or 1)
                if data:
                    last_rx_time = time.monotonic()
                    frames = decoder.feed(data)
                elif decoder.buffer and (time.monotonic() - last_rx_time) >= args.idle_flush:
                    frames = decoder.flush_idle()
                else:
                    frames = []

                for raw in frames:
                    frame = parse_frame(raw)
                    if frame.raw.upper().startswith("OK"):
                        at_ok = True
                    timestamp = now_text()
                    display("RX", frame)
                    if logger:
                        logger.write(timestamp, "RX", frame)
                    rx_frames += 1

    except serial.SerialException as exc:
        print(f"串口错误：{exc}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        if logger:
            logger.close()
            print(f"CSV:    {logger.csv_path}")
            print(f"JSONL:  {logger.jsonl_path}")
            print(f"最新帧: {logger.latest_path}")

    print(f"结束：发送 {tx_frames} 帧，接收 {rx_frames} 帧。")
    if args.at_test and not at_ok:
        print("AT 未收到 OK：检查是否为 HC-04、是否未连接、9600 波特率及 TX/RX 交叉。")
        return 1
    return 0


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="HC-04/HC-05 串口监视、发送和 CSV/JSONL 记录工具"
    )
    parser.add_argument("--list", action="store_true", help="列出串口后退出")
    parser.add_argument("--port", help="串口，例如 COM15；只有一个 CH340 时可省略")
    parser.add_argument("--baud", type=int, default=9600, help="波特率，默认 9600")
    parser.add_argument("--duration", type=float, default=0, help="运行秒数，0 表示持续运行")
    parser.add_argument("--send", action="append", help="打开后发送文本，可重复指定")
    parser.add_argument("--send-delay", type=float, default=0.2, help="连续发送之间的间隔秒数")
    parser.add_argument("--at-test", action="store_true", help="发送裸 AT 并等待 HC-04 回应")
    parser.add_argument("--demo", action="store_true", help="不打开串口，演示 PID 数据显示")
    parser.add_argument("--interactive", action="store_true", help="强制启用键盘发送")
    parser.add_argument("--no-interactive", action="store_true", help="禁用键盘发送")
    parser.add_argument("--idle-flush", type=float, default=0.2, help="无结束符数据的判帧空闲时间")
    parser.add_argument("--no-log", action="store_true", help="不保存 CSV/JSONL")
    parser.add_argument("--log-dir", default=str(DEFAULT_LOG_DIR), help="日志目录")
    return parser


def main() -> int:
    parser = build_argument_parser()
    args = parser.parse_args()
    if args.list:
        return show_ports()
    if args.demo:
        return run_demo(Path(args.log_dir).resolve(), args.no_log)
    try:
        return run_monitor(args)
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        show_ports()
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
