#!/usr/bin/env python3
"""Reconnect-safe ESP32-S3 native USB serial capture."""

from __future__ import annotations

import argparse
import os
import signal
import sys
import time

import serial


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--duration", type=int, choices=(0, 60, 180), default=60)
    parser.add_argument("--log", required=True)
    parser.add_argument("--retry-delay", type=float, default=0.25)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    stopped = False

    def request_stop(_signum: int, _frame: object) -> None:
        nonlocal stopped
        stopped = True

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)

    deadline = time.monotonic() + args.duration if args.duration else None
    stdout_fd = sys.stdout.fileno()

    def emit(message: str, log_file: object) -> None:
        data = (message + "\n").encode("utf-8", errors="replace")
        os.write(stdout_fd, data)
        log_file.write(data)
        log_file.flush()

    with open(args.log, "ab", buffering=0) as log_file:
        while not stopped and (deadline is None or time.monotonic() < deadline):
            connection: serial.Serial | None = None
            try:
                # Arduino HWCDC gates Serial output on DTR. Keep RTS low so the
                # ESP32-S3 never sees the DTR+RTS reset combination.
                connection = serial.Serial(
                    port=None,
                    baudrate=args.baud,
                    timeout=0.25,
                    write_timeout=0.25,
                )
                connection.dtr = True
                connection.rts = False
                connection.port = args.port
                connection.open()
                emit(f"[MONITOR] connected {args.port}", log_file)

                while not stopped and (deadline is None or time.monotonic() < deadline):
                    chunk = connection.read(4096)
                    if chunk:
                        os.write(stdout_fd, chunk)
                        log_file.write(chunk)
                    elif not os.path.exists(args.port):
                        raise serial.SerialException("port disappeared")
            except (OSError, PermissionError, serial.SerialException) as exc:
                if connection is not None and connection.is_open:
                    connection.close()
                if stopped or (deadline is not None and time.monotonic() >= deadline):
                    break
                emit(f"[MONITOR] waiting: {exc}", log_file)
                time.sleep(args.retry_delay)
            finally:
                if connection is not None and connection.is_open:
                    connection.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
