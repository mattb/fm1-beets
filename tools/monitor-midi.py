#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Receive Beets v1 SysEx and write validated JSON objects as JSONL."""
import argparse
import json
import math
import sys
import time

PREFIX = b"\x7dBEETS\x01"  # mido removes the enclosing F0/F7 bytes.
MAX_FRAME = 3072


def _reject_constant(value):
    raise ValueError(f"non-JSON numeric constant: {value}")


def _finite_float(value):
    number = float(value)
    if not math.isfinite(number):
        raise ValueError("JSON number exceeds finite range")
    return number


def _unique_object(pairs):
    obj = {}
    for key, value in pairs:
        if key in obj:
            raise ValueError(f"duplicate JSON key: {key}")
        obj[key] = value
    return obj


def parse_payload(data):
    """Return a v1 object, None for unrelated SysEx, or raise ValueError."""
    payload = bytes(data)
    if not payload.startswith(PREFIX):
        return None
    if len(payload) + 2 > MAX_FRAME:
        raise ValueError("frame exceeds 3072 bytes")
    body = payload[len(PREFIX):]
    if not body or any(byte < 0x20 or byte > 0x7e for byte in body):
        raise ValueError("JSON body must contain printable ASCII only (no LF/NUL)")
    obj = json.loads(body.decode("ascii"), parse_constant=_reject_constant, parse_float=_finite_float,
                     object_pairs_hook=_unique_object)
    if not isinstance(obj, dict) or type(obj.get("v")) is not int or obj["v"] != 1:
        raise ValueError("expected a v1 JSON object")
    if obj.get("type") not in ("state", "input"):
        raise ValueError("unknown v1 message type")
    for key in ("seq", "ms"):
        if type(obj.get(key)) is not int or not 0 <= obj[key] <= 0xffffffff:
            raise ValueError(f"{key} must be an unsigned 32-bit integer")
    return obj


def select_port(names, requested):
    if requested in names:
        return requested
    matches = [name for name in names if (requested or "beets").lower() in name.lower()]
    if not requested and not matches and len(names) == 1:
        return names[0]
    if len(matches) == 1:
        return matches[0]
    raise ValueError("no unique input port; use --list and --port with an exact name")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true", help="list MIDI input names and exit")
    parser.add_argument("--port", help="exact input name or unique case-insensitive substring")
    parser.add_argument("--count", type=int, default=0, help="stop after N valid frames (0: unlimited)")
    parser.add_argument("--timeout", type=float, default=0, help="overall capture seconds (0: unlimited)")
    args = parser.parse_args(argv)
    if args.count < 0 or not math.isfinite(args.timeout) or args.timeout < 0:
        parser.error("count and timeout must be finite and nonnegative")
    try:
        import mido
        names = mido.get_input_names()
        if args.list:
            for name in names:
                print(name)
            return 0
        name = select_port(names, args.port)
        count = 0
        with mido.open_input(name) as port:
            print(f"Listening on {name}", file=sys.stderr)
            deadline = time.monotonic() + args.timeout if args.timeout else None
            while True:
                if deadline is not None and time.monotonic() >= deadline:
                    print(f"Capture timeout: {count} valid frames", file=sys.stderr)
                    return 2 if count == 0 or (args.count and count < args.count) else 0
                message = port.poll()
                if message is None:
                    time.sleep(0.005)
                    continue
                if message.type != "sysex":
                    continue
                try:
                    obj = parse_payload(message.data)
                except (ValueError, RecursionError) as error:
                    print(f"Ignoring malformed Beets frame: {error}", file=sys.stderr)
                    continue
                if obj is None:
                    continue
                print(json.dumps(obj, ensure_ascii=True, separators=(",", ":"), allow_nan=False), flush=True)
                count += 1
                if args.count and count >= args.count:
                    return 0
    except ImportError as error:
        print(f"MIDI dependency missing: {error}; install mido python-rtmidi", file=sys.stderr)
        return 1
    except BrokenPipeError:
        return 0
    except (OSError, RuntimeError, ValueError) as error:
        print(f"MIDI receiver: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
