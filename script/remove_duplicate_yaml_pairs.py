#!/usr/bin/env python3

import argparse
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Remove duplicate 'key: value' entries from the earlier section of a YAML-like "
            "file when the same pair appears in a later line range."
        )
    )
    parser.add_argument("file", type=Path, help="Path to the YAML-like file.")
    parser.add_argument(
        "--tail-start-line",
        type=int,
        required=True,
        help="1-based start line of the preserved tail range.",
    )
    parser.add_argument(
        "--tail-end-line",
        type=int,
        default=None,
        help="1-based end line of the preserved tail range. Defaults to EOF.",
    )
    parser.add_argument(
        "--in-place",
        action="store_true",
        help="Overwrite the input file. Otherwise print the result to stdout.",
    )
    return parser.parse_args()


def parse_pair(line: str):
    stripped = line.strip()
    if not stripped or stripped.startswith("#") or ":" not in stripped:
        return None

    key, value = stripped.split(":", 1)
    return key.strip(), value.strip()


def remove_duplicates(lines: list[str], tail_start_line: int, tail_end_line: int | None):
    if tail_start_line < 1:
        raise ValueError("--tail-start-line must be >= 1")

    tail_start_index = tail_start_line - 1
    tail_end_index = len(lines) if tail_end_line is None else tail_end_line

    if tail_end_index < tail_start_line:
        raise ValueError("--tail-end-line must be >= --tail-start-line")

    head_lines = lines[:tail_start_index]
    tail_lines = lines[tail_start_index:tail_end_index]
    remaining_lines = lines[tail_end_index:]

    tail_pairs = {
        pair for pair in (parse_pair(line) for line in tail_lines) if pair is not None
    }

    filtered_head = [
        line for line in head_lines if parse_pair(line) not in tail_pairs
    ]

    return filtered_head + tail_lines + remaining_lines


def main():
    args = parse_args()
    lines = args.file.read_text().splitlines(keepends=True)
    updated_lines = remove_duplicates(lines, args.tail_start_line, args.tail_end_line)
    output = "".join(updated_lines)

    if args.in_place:
        args.file.write_text(output)
    else:
        print(output, end="")


if __name__ == "__main__":
    main()
