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
import numpy as np

import gen_yaml
import plot_correlation
import filter_net
import toml_decoder
import slack_score

import logging

# Configure logging
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


def buffer_check_gen(data: dict, design: str) -> pd.DataFrame:
    # Prepare CSV data
    csv_data = []

    # Iterate through each net arc in the JSON
    for arc_key, arc_data in data.items():
        key_info = arc_data.get("key", {})
        value_info = arc_data.get("value", {})

        # Extract the required fields
        row = {
            "name": arc_key,
            "design": design,
            "trans": key_info.get("pins", [])[0].get("trans", 0),
            "cap": key_info.get("pins", [])[0].get("cap", 0),
            "fanout": key_info.get("fanout", 0),
            "length": key_info.get("length", 0),
            "pta_delay": key_info.get("delay", 0),
            "pta_slack": key_info.get("slack", 0),
            "is_cell_arc": arc_data["type"] == "cell arc",
            "with_buffer": len(value_info.get("pins", {})) > 2,
            "value_delay": value_info.get("delay", 0),
        }
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

        # Extract the required fields
        pins = key_info.get("pins", [])
        first_pin = pins[0] if pins else {}

        row = {
            "name": arc_key,
            "design": design,
            "trans": first_pin.get("trans", 0),
            "cap": first_pin.get("cap", 0),
            "fanout": key_info.get("fanout", 0),
            "length": key_info.get("length", 0),
            "pta_delay": key_delay,
            "pta_slack": key_info.get("slack", 0),
            "is_cell_arc": arc_data["type"] == "cell arc",
            "with_buffer": need_update,
            "value_delay": value_delay,
        }
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
    yaml_files = gen_yaml.generate_yaml(results, base_name, analyse_type)

    summary_df = pd.DataFrame()
    for res in results:
        if "summary" in res["results"]:
            script_dir = os.path.dirname(os.path.abspath(__file__))
            with open(
                os.path.join(script_dir, "..", "configs", "design_period.yml")
            ) as f:
                design_period = yaml.safe_load(f)
            keys = list(res["results"]["summary"].keys())
            score = slack_score.slack_score(
                res["results"]["summary"][keys[0]],
                res["results"]["summary"][keys[1]],
                (
                    design_period[res["values"]["SHORT"]]
                    if res["values"]["SHORT"] in design_period
                    else design_period[res["values"]["NAME"]]
                ),
            )
            df = pd.DataFrame([score])
            summary_df = pd.concat([summary_df, df], ignore_index=True)

    for yaml_file in yaml_files:
        # Run the command "build/slack_tool yml_file"
        subprocess.run(["build/slack_tool", yaml_file])

    all_r2_df = pd.DataFrame()

    arc_yaml_file = yaml_files[0]
    endpoint_yaml_file = yaml_files[1]

    with open(endpoint_yaml_file) as f:
        end_data = yaml.safe_load(f)
    output_dir = end_data["configs"]["output_dir"]
    analyse_tuples = end_data["configs"]["analyse_tuples"]

    for i in range(len(analyse_tuples)):
        name_pair = analyse_tuples[i]
        tuple_name = "-".join(name_pair)
        sub_df_end = plot_correlation.plot_text(
            [
                f"{output_dir}/{tuple_name}_scatter_0.txt",
                f"{output_dir}/{tuple_name}_scatter_1.txt",
            ],
            f"{output_dir}/{tuple_name}",
            f"{name_pair[0]}",
            f"{name_pair[1]}",
        )
        logging.info(f"Processing {tuple_name} and {tuple_name}")
        sub_df_arc = plot_correlation.plot_correlation(
            f"{output_dir}/{tuple_name}.json",
            f"{output_dir}/{tuple_name}",
            f"{name_pair[0]}",
            f"{name_pair[1]}",
        )
        sub_df_arc["name"] = sub_df_end["name"]

        # combine the two dataframes and remove the duplicate columns
        sub_df = pd.merge(
            sub_df_arc,
            sub_df_end,
        )
        mae, maxe = filter_net.sort_and_convert(
            f"{output_dir}/{tuple_name}.json",
            f"{output_dir}/{tuple_name}_filter.csv",
            100,
        )
        sub_df["mae"] = mae
        sub_df["maxe"] = maxe

        all_r2_df = pd.concat([all_r2_df, sub_df], ignore_index=True)
    if "tns_score" in summary_df.columns:
        all_r2_df = pd.merge(all_r2_df, summary_df, left_index=True, right_index=True)
        score_datas = {
            "wns_score": (100 - all_r2_df["wns_score"]) / 100,
            "wns100_score": (100 - all_r2_df["wns100_score"]) / 100,
            "tns_score": (100 - all_r2_df["tns_score"]) / 100,
            "r2r_tns_score": (100 - all_r2_df["r2r_tns_score"]) / 100,
            "r2r_wns_score": (100 - all_r2_df["r2r_wns_score"]) / 100,
            "arc_r2": all_r2_df["arc_r2"],
            "arc_pearsonr": all_r2_df["arc_pearsonr"],
            "end_r2": all_r2_df["end_r2"],
            "end_pearsonr": all_r2_df["end_pearsonr"],
            "mae": all_r2_df["mae"],
        }

        score_datas = {key: np.tanh(value) for key, value in score_datas.items()}
        all_r2_df["score"] = (
            0.15 * score_datas["wns_score"]
            + 0.35 * score_datas["wns100_score"]
            + 0.5 * score_datas["tns_score"]
            + 0.5 * score_datas["r2r_tns_score"]
            + 0.5 * score_datas["r2r_wns_score"]
            + all_r2_df["mae"]
            - (0.7 * score_datas["arc_r2"] + 0.3 * score_datas["arc_pearsonr"])
            - (0.3 * score_datas["end_r2"] + 0.7 * score_datas["end_pearsonr"])
        )
    print(all_r2_df)
    flow_name = args.path.split("/")[-1].split(".")[0]
    all_r2_df.to_csv(
        f"{output_dir}/{flow_name}_r2.csv", index=False, float_format="%.4f"
    )

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
        f"{output_dir}/{flow_name}_dataset.csv", index=False, float_format="%.6f"
    )
