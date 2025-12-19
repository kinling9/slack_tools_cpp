#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import yaml
import argparse
import subprocess
import pandas as pd
import os
import toml
import numpy as np
import concurrent.futures
from functools import partial

import gen_yaml
import plot_correlation
import filter_net
import toml_decoder
import slack_score

import logging
import matplotlib

matplotlib.use("agg")

# Configure logging
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

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
    toml_file = args.path
    base_name = os.path.splitext(os.path.basename(toml_file))[0]
    results = toml_decoder.process_toml(args.path)
    arc_yamls, endpoint_yamls = gen_yaml.generate_yaml(results, base_name, analyse_type)

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

    yaml_files = list(arc_yamls.values()) + list(endpoint_yamls.values())

    # Run commands in parallel using ThreadPoolExecutor
    with concurrent.futures.ThreadPoolExecutor() as executor:
        futures = [
            executor.submit(subprocess.run, ["build/slack_tool", yaml_file])
            for yaml_file in yaml_files
        ]
        # Wait for all processes to complete
        concurrent.futures.wait(futures)

    all_r2_df = pd.DataFrame()

    output_dir = base_name
    analyse_tuples = []
    for result in results:
        short = result["values"]["SHORT"]
        tuple_list = []
        for key in result["results"]["arc"].keys():
            tuple_list.append(f"{short}_{key}")
        analyse_tuples.append(tuple_list)

    def process_tuple(output_dir, name_pair):
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
        print(f"Processing {tuple_name} and {tuple_name}")
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

        return sub_df

    # Process tuples in parallel
    # FIXME: matplotlib cannot work in parallel properly, so we use max_workers=1 here.
    with concurrent.futures.ThreadPoolExecutor(max_workers=1) as executor:
        # Create partial function with output_dir pre-filled
        process_func = partial(process_tuple, output_dir)

        # Submit all tasks
        future_to_index = {
            executor.submit(process_func, analyse_tuples[i]): i
            for i in range(len(analyse_tuples))
        }

        # Collect results
        results = []
        for future in concurrent.futures.as_completed(future_to_index):
            try:
                result = future.result()
                results.append(result)
            except Exception as exc:
                index = future_to_index[future]
                print(f"analyse_tuples[{index}] generated an exception: {exc}")

        # Concatenate all results at once (more efficient than repeated concat)
        if results:
            all_r2_df = pd.concat(results, ignore_index=True)
        else:
            # Handle case where no results were successfully processed
            all_r2_df = pd.DataFrame()
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
    all_r2_df.to_csv(
        f"{output_dir}/{base_name}_r2.csv", index=False, float_format="%.4f"
    )
