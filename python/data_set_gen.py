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


def convert_dict_to_df(data: dict, design: str) -> pd.DataFrame:
    # Prepare CSV data
    csv_data = []

    # Iterate through each net arc in the JSON
    for net_arc_key, net_arc_data in data.items():
        if net_arc_data["type"] == "cell arc":
            continue
        key_info = net_arc_data.get("key", {})
        value_info = net_arc_data.get("value", {})

        # Extract the required fields
        row = {
            "name": net_arc_key,
            "design": design,
            "trans": key_info.get("pins", [])[0].get("trans", 0),
            "cap": key_info.get("pins", [])[0].get("cap", 0),
            "fanout": key_info.get("fanout", 0),
            "length": key_info.get("length", 0),
            "pta delay": key_info.get("delay", 0),
            "pta slack": key_info.get("slack", 0),
            "with buffer": len(value_info.get("pins", {})) > 2,
            "value delay": value_info.get("delay", 0),
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
            "arc": all_r2_df["arc"],
            "arc_scaled": all_r2_df["arc_scaled"],
            "end": all_r2_df["end"],
            "end_scaled": all_r2_df["end_scaled"],
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
            - (0.7 * score_datas["arc"] + 0.3 * score_datas["arc_scaled"])
            - (0.3 * score_datas["end"] + 0.7 * score_datas["end_scaled"])
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
        current_df = convert_dict_to_df(data, design_name)
        all_data_df = pd.concat([all_data_df, current_df], ignore_index=True)

    print(all_data_df)
    all_data_df.to_csv(
        f"{output_dir}/{flow_name}_dataset.csv", index=False, float_format="%.4f"
    )
