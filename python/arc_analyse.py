#!/usr/bin/env python3
import yaml
import argparse
import subprocess
import plot_correlation
import pandas as pd

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "path",
        help="path to the yaml file",
    )
    args = parser.parse_args()
    with open(args.path) as f:
        data = yaml.safe_load(f)
    output_dir = data["configs"]["output_dir"]
    analyse_tuples = data["configs"]["analyse_tuples"]

    # Run the command "build/slack_tool yml_file"
    subprocess.run(["build/slack_tool", args.path])

    all_r2_df = pd.DataFrame()
    for pair in analyse_tuples:
        name = "-".join(pair)
        sub_df = plot_correlation.plot_correlation(
            f"{output_dir}/{name}.json",
            f"{output_dir}/{name}",
            f"{pair[0]}",
            f"{pair[1]}",
        )
        all_r2_df = pd.concat([all_r2_df, sub_df], ignore_index=True)
    print(all_r2_df)
    flow_name = args.path.split("/")[-1].split(".")[0]
    all_r2_df.to_csv(f"{output_dir}/{flow_name}_r2.csv", index=False)
