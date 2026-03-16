#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import concurrent.futures
import csv
import logging
import os
import subprocess
import sys

import gen_yaml
import orjson
import toml
import toml_decoder

# Configure logging
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

DATASET_COLUMNS = [
    "name",
    "design",
    "trans",
    "cap",
    "fanout",
    "length",
    "pta_delay",
    "pta_slack",
    "is_cell_arc",
    "is_topin_rise",
    "value_delay",
    "value_slack",
    "with_buffer",
]

WRITE_BATCH_SIZE = 50000
WRITE_BUFFER_SIZE = 8 * 1024 * 1024


def _extract_common_arc_data(record: dict, design: str) -> dict:
    key_info = record.get("key", {})
    value_info = record.get("value", {})
    pins = key_info.get("pins", [])
    first_pin = pins[0] if pins else {}
    last_pin = pins[-1] if pins else {}

    return {
        "name": record["name"],
        "design": design,
        "trans": first_pin.get("trans", 0),
        "cap": last_pin.get("cap", 0),
        "fanout": key_info.get("fanout", 0),
        "length": key_info.get("length", 0),
        "pta_delay": key_info.get("delay", 0),
        "pta_slack": key_info.get("slack", 0),
        "is_cell_arc": record["type"] == "cell arc",
        "is_topin_rise": last_pin.get("rf", False),
        "value_delay": value_info.get("delay", 0),
        "value_slack": value_info.get("slack", 0),
    }


def _iter_arc_records(input_file: str):
    if input_file.endswith(".jsonl"):
        with open(input_file, "rb") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                yield orjson.loads(line)
        return

    with open(input_file, "rb") as f:
        data = orjson.loads(f.read())
    for arc_name, arc_data in data.items():
        arc_data["name"] = arc_name
        yield arc_data


def _iter_buffer_check_rows(records, design: str):
    for record in records:
        key_info = record.get("key", {})
        value_info = record.get("value", {})
        key_slack = key_info.get("slack", 0)
        if key_slack >= 10000:
            continue

        row = _extract_common_arc_data(record, design)
        row["with_buffer"] = len(value_info.get("pins", [])) > 2
        yield row


def _iter_filter_check_rows(records, design: str):
    for record in records:
        key_info = record.get("key", {})
        value_info = record.get("value", {})
        key_slack = key_info.get("slack", 0)
        if key_slack >= 10000:
            continue

        value_slack = value_info.get("slack", 0)
        key_delay = key_info.get("delay", 0)
        value_delay = value_info.get("delay", 0)
        delay_ratio_valid = value_delay != 0
        delay_difference_valid = abs(key_delay - value_delay) > 0.005
        # delay_difference_valid = True

        row = _extract_common_arc_data(record, design)
        row["with_buffer"] = (
            (value_slack < 0 or key_slack < 0) and delay_ratio_valid and delay_difference_valid
        )
        yield row


def _get_input_file(output_dir: str, tuple_name: str) -> str:
    jsonl_file = f"{output_dir}/{tuple_name}.jsonl"
    if os.path.exists(jsonl_file):
        return jsonl_file
    return f"{output_dir}/{tuple_name}.json"


def _serialize_value(value):
    if isinstance(value, float):
        return f"{value:.6f}"
    return value


def _iter_serialized_row_values(rows):
    for row in rows:
        yield tuple(_serialize_value(row.get(column, "")) for column in DATASET_COLUMNS)


def _write_dataset_rows(output_csv: str, rows) -> int:
    row_count = 0
    with open(output_csv, "w", newline="", buffering=WRITE_BUFFER_SIZE) as f:
        writer = csv.writer(f)
        writer.writerow(DATASET_COLUMNS)

        batch = []
        for row_values in _iter_serialized_row_values(rows):
            batch.append(row_values)
            if len(batch) >= WRITE_BATCH_SIZE:
                writer.writerows(batch)
                row_count += len(batch)
                batch.clear()

        if batch:
            writer.writerows(batch)
            row_count += len(batch)
    return row_count


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "path",
        help="path to the toml file",
    )
    parser.add_argument(
        "-m",
        "--method",
        default="buffer_check",
        help="Method to generate the dataset. e.g. buffer_check",
    )
    args = parser.parse_args()

    with open(args.path, "r") as file:
        data = toml.load(file)
    analyse_type = data.get("variables", {}).get("ANALYSE_TYPE", "pair")
    if analyse_type != "arc":
        logging.info("Only arc analysis is supported currently.")
        sys.exit(0)

    toml_file = args.path
    base_name = os.path.splitext(os.path.basename(toml_file))[0]
    results = toml_decoder.process_toml(args.path)
    arc_yamls, _ = gen_yaml.generate_yaml(
        results, base_name, analyse_type, arc_output_format="jsonl"
    )

    yaml_files = list(arc_yamls.values())
    with concurrent.futures.ThreadPoolExecutor() as executor:
        futures = [
            executor.submit(subprocess.run, ["build/slack_tool", yaml_file], check=True)
            for yaml_file in yaml_files
        ]
        concurrent.futures.wait(futures)
        for future in futures:
            future.result()

    output_dir = base_name
    analyse_tuples = []
    for result in results:
        short = result["values"]["SHORT"]
        tuple_list = []
        for key in result["results"]["arc"].keys():
            tuple_list.append(f"{short}_{key}")
        analyse_tuples.append(tuple_list)

    if args.method == "buffer_check":
        row_builder = _iter_buffer_check_rows
    elif args.method == "filter_check":
        row_builder = _iter_filter_check_rows
    else:
        logging.error("Method %s not supported.", args.method)
        sys.exit(1)

    output_csv = f"{output_dir}/{base_name}_dataset.csv"

    def iter_all_rows():
        for name_pair in analyse_tuples:
            tuple_name = "-".join(name_pair)
            input_file = _get_input_file(output_dir, tuple_name)
            design_name = "_".join(name_pair[0].split("_")[:-1])
            yield from row_builder(_iter_arc_records(input_file), design_name)

    row_count = _write_dataset_rows(output_csv, iter_all_rows())
    logging.info("Wrote %s rows to %s", row_count, output_csv)
