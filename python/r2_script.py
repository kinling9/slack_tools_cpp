#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import yaml
import argparse
import subprocess
import pandas as pd
import gen_yaml
import plot_correlation
import filter_net

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "path",
        help="path to the json file",
    )
    args = parser.parse_args()

    yaml_files = gen_yaml.generate_yaml(args.path)

    for yaml_file in yaml_files:
        # Run the command "build/slack_tool yml_file"
        subprocess.run(["build/slack_tool", yaml_file])

    all_r2_df = pd.DataFrame()

    arc_yaml_file = yaml_files[0]
    endpoint_yaml_file = yaml_files[1]

    with open(arc_yaml_file) as f:
        data = yaml.safe_load(f)
    output_dir = data["configs"]["output_dir"]
    analyse_tuples = data["configs"]["analyse_tuples"]

    for pair in analyse_tuples:
        name = "-".join(pair)
        sub_df_arc = plot_correlation.plot_correlation(
            f"{output_dir}/{name}.json",
            f"{output_dir}/{name}",
            f"{pair[0]}",
            f"{pair[1]}",
        )
        sub_df_end = plot_correlation.plot_text(
            [
                f"{output_dir}/{name}_scatter_0.txt",
                f"{output_dir}/{name}_scatter_1.txt",
            ],
            f"{output_dir}/{name}",
            f"{pair[0]}",
            f"{pair[1]}",
        )
        # combine the two dataframes and remove the duplicate columns
        sub_df = pd.merge(
            sub_df_arc,
            sub_df_end,
        )
        filter_net.sort_and_convert(
            f"{output_dir}/{name}.json", f"{output_dir}/{name}.filter.csv", 100
        )

        all_r2_df = pd.concat([all_r2_df, sub_df], ignore_index=True)

    print(all_r2_df)
    flow_name = args.path.split("/")[-1].split(".")[0]
    all_r2_df.to_csv(
        f"{output_dir}/{flow_name}_r2.csv", index=False, float_format="%.4f"
    )
