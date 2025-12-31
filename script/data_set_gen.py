#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import yaml
import argparse
import subprocess
import pandas as pd
import os
import toml
import json
import sys
import concurrent.futures

import gen_yaml
import toml_decoder


import logging

# Configure logging
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


def _extract_common_arc_data(arc_key: str, arc_data: dict, design: str) -> dict:
    key_info = arc_data.get("key", {})
    value_info = arc_data.get("value", {})
    pins = key_info.get("pins", [])
    first_pin = pins[0] if pins else {}
    last_pin = pins[-1] if pins else {}

    return {
        "name": arc_key,
        "design": design,
        "trans": first_pin.get("trans", 0),
        "cap": last_pin.get("cap", 0),
        "fanout": key_info.get("fanout", 0),
        "length": key_info.get("length", 0),
        "pta_delay": key_info.get("delay", 0),
        "pta_slack": key_info.get("slack", 0),
        "is_cell_arc": arc_data["type"] == "cell arc",
        "is_topin_rise": last_pin.get("rf", False),
        "value_delay": value_info.get("delay", 0),
    }


def buffer_check_gen(data: dict, design: str) -> pd.DataFrame:
    # Prepare CSV data
    csv_data = []

    # Iterate through each net arc in the JSON
    for arc_key, arc_data in data.items():
        key_info = arc_data.get("key", {})
        value_info = arc_data.get("value", {})

        row = _extract_common_arc_data(arc_key, arc_data, design)
        row["with_buffer"] = len(value_info.get("pins", {})) > 2
        csv_data.append(row)

    df = pd.DataFrame(csv_data)
    return df


def filter_check_gen(data: dict, design: str) -> pd.DataFrame:
    # Prepare CSV data
    csv_data = []

    # Iterate through each net arc in the JSON
    for arc_key, arc_data in data.items():
        key_info = arc_data.get("key", {})
        value_info = arc_data.get("value", {})
        # Check conditions with protection against division by zero
        value_slack = value_info.get("slack", 0)
        key_delay = key_info.get("delay", 0)
        value_delay = value_info.get("delay", 0)

        # Avoid division by zero
        # delay_ratio_valid = (value_delay != 0) and (key_delay / value_delay > 1.3)
        # delay_difference_valid = key_delay - value_delay > 0.1
        delay_ratio_valid = (value_delay != 0) and (key_delay / value_delay > 1.1)
        delay_difference_valid = key_delay - value_delay > 0.005

        need_update = False
        if value_slack < 0 and delay_ratio_valid and delay_difference_valid:
            need_update = True

        row = _extract_common_arc_data(arc_key, arc_data, design)
        row["with_buffer"] = need_update
        csv_data.append(row)

    df = pd.DataFrame(csv_data)
    return df


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
    # output = data.get("variables", {}).get("OUTPUT", "output")
    analyse_type = data.get("variables", {}).get("ANALYSE_TYPE", "pair")
    if analyse_type != "arc":
        logging.info("Only arc analysis is supported currently.")
        sys.exit(0)
    toml_file = args.path
    base_name = os.path.splitext(os.path.basename(toml_file))[0]
    results = toml_decoder.process_toml(args.path)
    arc_yamls, endpoint_yamls = gen_yaml.generate_yaml(results, base_name, analyse_type)

    yaml_files = list(arc_yamls.values())

    # Run commands in parallel using ThreadPoolExecutor
    with concurrent.futures.ThreadPoolExecutor() as executor:
        futures = [
            executor.submit(subprocess.run, ["build/slack_tool", yaml_file])
            for yaml_file in yaml_files
        ]
        # Wait for all processes to complete
        concurrent.futures.wait(futures)

    output_dir = base_name
    analyse_tuples = []
    for result in results:
        short = result["values"]["SHORT"]
        tuple_list = []
        for key in result["results"]["arc"].keys():
            tuple_list.append(f"{short}_{key}")
        analyse_tuples.append(tuple_list)

    all_data_df = pd.DataFrame()
    for i in range(len(analyse_tuples)):
        name_pair = analyse_tuples[i]
        tuple_name = "-".join(name_pair)
        json_file = f"{output_dir}/{tuple_name}.json"
        with open(json_file, "r") as f:
            data = json.load(f)

        design_name = "_".join(name_pair[0].split("_")[:-1])
        current_df = None
        if args.method == "buffer_check":
            current_df = buffer_check_gen(data, design_name)
        elif args.method == "filter_check":
            current_df = filter_check_gen(data, design_name)
        else:
            logging.error(f"Method {args.method} not supported.")
            sys.exit(1)
        all_data_df = pd.concat([all_data_df, current_df], ignore_index=True)

    print(all_data_df)
    all_data_df.to_csv(
        f"{output_dir}/{base_name}_dataset.csv", index=False, float_format="%.6f"
    )
