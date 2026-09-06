#!/usr/bin/env python3
"""Capture Cardputer serial bytes without reconnecting the port.

This deliberately does less than a normal serial monitor. It opens the TTY once,
configures raw mode, writes every received byte to an output file immediately,
and exits on disconnect/read failure. There is no automatic close/reopen loop
that can lose the panic boundary while the USB CDC device resets.

Linux only; uses the Python standard library and termios.
"""

from __future__ import annotations

import argparse
import errno
import os
import selectors
import sys
import termios
import time
import tty
from pathlib import Path


_BAUD = {
    9600: termios.B9600,
    19200: termios.B19200,
    38400: termios.B38400,
    57600: termios.B57600,
    115200: termios.B115200,
    230400: termios.B230400,
}


def configure_raw(fd: int, baud: int) -> None:
    speed = _BAUD.get(baud)
    if speed is None:
        supported = ", ".join(str(value) for value in sorted(_BAUD))
        raise SystemExit(f"unsupported baud {baud}; supported: {supported}")

    # Start from Python's raw-mode definition, then explicitly retain local
    # receiver semantics and disable hang-up-on-close. The descriptor remains
    # open for the entire capture.
    tty.setraw(fd, termios.TCSANOW)
    attrs = termios.tcgetattr(fd)
    attrs[2] |= termios.CLOCAL | termios.CREAD
    attrs[2] &= ~termios.HUPCL
    attrs[4] = speed
    attrs[5] = speed
    termios.tcsetattr(fd, termios.TCSANOW, attrs)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="single-open raw Cardputer serial capture for panic/backtrace evidence")
    parser.add_argument("--port", required=True, help="TTY path, e.g. /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--output", required=True, type=Path,
                        help="raw output file; every device byte is flushed immediately")
    parser.add_argument("--duration", type=float, default=0.0,
                        help="optional capture duration in seconds; 0 means until Ctrl-C/disconnect")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)

    flags = os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK
    try:
        fd = os.open(args.port, flags)
    except OSError as exc:
        print(f"capture: cannot open {args.port}: {exc}", file=sys.stderr)
        return 2

    selector = selectors.DefaultSelector()
    started = time.monotonic()
    total = 0

    try:
        configure_raw(fd, args.baud)
        selector.register(fd, selectors.EVENT_READ)
        with args.output.open("wb", buffering=0) as output:
            print(
                f"capture: one-open session port={args.port} baud={args.baud} "
                f"output={args.output}",
                file=sys.stderr,
            )
            print("capture: Ctrl-C to stop; reconnect is intentionally disabled", file=sys.stderr)

            while True:
                if args.duration > 0 and time.monotonic() - started >= args.duration:
                    break

                events = selector.select(timeout=0.25)
                if not events:
                    continue

                try:
                    chunk = os.read(fd, 4096)
                except BlockingIOError:
                    continue
                except OSError as exc:
                    if exc.errno in (errno.EIO, errno.ENODEV, errno.EBADF):
                        print(
                            f"\ncapture: device disconnected/read ended after {total} bytes; "
                            "not reconnecting",
                            file=sys.stderr,
                        )
                        break
                    raise

                if not chunk:
                    print(
                        f"\ncapture: EOF after {total} bytes; not reconnecting",
                        file=sys.stderr,
                    )
                    break

                output.write(chunk)
                total += len(chunk)
                try:
                    sys.stdout.buffer.write(chunk)
                    sys.stdout.buffer.flush()
                except BrokenPipeError:
                    # Preserve the evidence file even if stdout was piped to a
                    # consumer that exited.
                    pass

    except KeyboardInterrupt:
        print(f"\ncapture: stopped by user after {total} bytes", file=sys.stderr)
    finally:
        try:
            selector.unregister(fd)
        except Exception:
            pass
        selector.close()
        os.close(fd)

    elapsed = time.monotonic() - started
    print(f"capture: saved {total} bytes in {elapsed:.1f}s to {args.output}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
