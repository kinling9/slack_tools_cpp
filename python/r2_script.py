#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import yaml
import argparse
import subprocess
import pandas as pd
import re
import os
import json
import sys

import gen_yaml
import plot_correlation
import filter_net
import json_decoder
import slack_score

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "path",
        help="path to the json file",
    )
    args = parser.parse_args()

    with open(args.path, "r") as file:
        data = json.load(file)
    output = data.get("variables", {}).get("OUTPUT", "output")
    results = json_decoder.process_json(args.path)
    yaml_files = gen_yaml.generate_yaml(results, output)

    print(results)
    summary_df = pd.DataFrame()
    for res in results:
        if "summary" in res["results"]:
            keys = list(res["results"]["summary"].keys())
            score = slack_score.slack_score(
                res["results"]["summary"][keys[0]],
                res["results"]["summary"][keys[1]],
                0.35,
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
        data = yaml.safe_load(f)
    output_dir = data["configs"]["output_dir"]
    analyse_tuples = data["configs"]["analyse_tuples"]

    for pair in analyse_tuples:
        name = "-".join(pair)
        sub_df_end = plot_correlation.plot_text(
            [
                f"{output_dir}/{name}_scatter_0.txt",
                f"{output_dir}/{name}_scatter_1.txt",
            ],
            f"{output_dir}/{name}",
            f"{pair[0]}",
            f"{pair[1]}",
        )
        if os.path.exists(f"{output_dir}/{name}.json"):
            sub_df_arc = plot_correlation.plot_correlation(
                f"{output_dir}/{name}.json",
                f"{output_dir}/{name}",
                f"{pair[0]}",
                f"{pair[1]}",
            )
        else:
            pair[0] = re.sub(r"_[^_]+$", "_csv", pair[0])
            name = "-".join(pair)
            sub_df_arc = plot_correlation.plot_correlation(
                f"{output_dir}/{name}.json",
                f"{output_dir}/{name}",
                f"{pair[0]}",
                f"{pair[1]}",
            )
            sub_df_arc["name"] = sub_df_end["name"]

        # combine the two dataframes and remove the duplicate columns
        sub_df = pd.merge(
            sub_df_arc,
            sub_df_end,
        )
        mae, maxe = filter_net.sort_and_convert(
            f"{output_dir}/{name}.json", f"{output_dir}/{name}.filter.csv", 100
        )
        sub_df["mae"] = mae
        sub_df["maxe"] = maxe

        all_r2_df = pd.concat([all_r2_df, sub_df], ignore_index=True)
    all_r2_df = pd.merge(all_r2_df, summary_df, left_index=True, right_index=True)
    all_r2_df["score"] = (
        (100 - all_r2_df["wns_score"]) / 100
        + (100 - all_r2_df["tns_score"]) / 100
        + all_r2_df["mae"]
        - (0.7 * all_r2_df["arc"] + 0.3 * all_r2_df["arc_scaled"])
        - (0.3 * all_r2_df["end"] + 0.7 * all_r2_df["end_scaled"])
    )
    print(all_r2_df)
    flow_name = args.path.split("/")[-1].split(".")[0]
    all_r2_df.to_csv(
        f"{output_dir}/{flow_name}_r2.csv", index=False, float_format="%.4f"
    )
