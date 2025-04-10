#!/usr/bin/env python3
import argparse
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import sklearn.metrics
from datetime import datetime
import sys
import re
import json
import pandas as pd

digit_shrink_level = -1
digits = re.compile(r"\d+")
bracket_number = re.compile(r"\[\d+\]")


def get_reg_group(reg_name):
    def replace_digits(s):
        return digits.sub("*", s)

    def remove_bracket_numbers(s):
        return bracket_number.sub("", s)

    ls_layer = reg_name.split("/")
    res = remove_bracket_numbers(ls_layer[0])
    keep_until = len(ls_layer) - digit_shrink_level - 1
    if digit_shrink_level == -1:
        keep_until = 1
    for i in range(1, keep_until):
        res += "/"
        res += ls_layer[i]
    for i in range(max(1, keep_until), len(ls_layer)):
        res += "/"
        res += replace_digits(ls_layer[i])
    return res


def gen_data(datas):
    data_dict = {
        k: {"delay": [v["key"]["delay"], v["value"]["delay"]], "size": 1}
        for k, v in datas.items()
    }
    return data_dict


def group_dict(data_dict):
    data_dict_grouped = {}
    for key, value in data_dict.items():
        group_key = get_reg_group(key)
        if group_key not in data_dict_grouped:
            data_dict_grouped[group_key] = {"delay": [0, 0], "size": 0}
        data_dict_grouped[group_key]["delay"] = [
            a + b for a, b in zip(value["delay"], data_dict_grouped[group_key]["delay"])
        ]
        data_dict_grouped[group_key]["size"] += 1
    return data_dict_grouped


def plot_group(data_dict: dict, name: str, x_label, y_label):
    x_list = [v["delay"][0] for v in data_dict.values()]
    y_list = [v["delay"][1] for v in data_dict.values()]
    size_list = [v["size"] * 100 for v in data_dict.values()]
    fig, [ax0, ax1] = plt.subplots(
        nrows=1, ncols=2, figsize=(16, 6), gridspec_kw={"width_ratios": [1, 1.2]}
    )

    ax0.set_xlabel(x_label)
    ax0.set_ylabel(y_label)
    ax0.axhline(y=0, color="green", linestyle="--")
    ax0.scatter(x_list, y_list, c="k", marker=".", linewidth=0, s=size_list, alpha=0.5)
    ax0.grid(ls=":", color="blue")

    func = np.polyfit(x_list, y_list, 1)
    xn = np.linspace(min(x_list), max(x_list), 1000)
    yn = np.poly1d(func)
    r2 = sklearn.metrics.r2_score(y_list, x_list)
    ax0.set_title("Scatter plot (r2 = {:0.4f})".format(r2), fontsize=10)
    ax0.plot(xn, yn(xn))

    ax1.set_title("2D histogram", fontsize=10)
    h = ax1.hist2d(x_list, y_list, bins=50)
    ax1.axhline(y=0, color="green", linestyle="--")
    ax1.axvline(x=0, color="green", linestyle="--")
    fig.colorbar(h[3], ax=ax1)

    ax0.set_xlim(ax1.get_xlim())
    ax0.set_ylim(ax1.get_ylim())

    plt.savefig(name, bbox_inches="tight")
    return float(r2)


def plot_correlation(path, output_file, x_label, y_label):
    # collect data
    print(f"{datetime.now()}: start collecting data")

    with open(path) as f:
        data = json.load(f)
    data_dict = gen_data(data)

    grouped_dict = group_dict(data_dict)

    avg_dict = {
        k: {"delay": [i / v["size"] for i in v["delay"]], "size": v["size"]}
        for k, v in grouped_dict.items()
    }

    print(f"# values: {len(data)}, matched groups = {len(data_dict)}")

    # plot
    print(f"{datetime.now()}: start plotting")
    r2_dict = {"raw": 0.0, "grouped": 0.0, "average": 0.0}
    r2_dict["raw"] = plot_group(data_dict, f"{output_file}.png", x_label, y_label)
    r2_dict["grouped"] = plot_group(
        grouped_dict, f"{output_file}_grouped.png", x_label, y_label
    )
    r2_dict["average"] = plot_group(
        avg_dict, f"{output_file}_average.png", x_label, y_label
    )

    # After calculating r2_dict, create a DataFrame and save to CSV
    r2_dict_with_name = {"name": output_file.split("/")[-1], **r2_dict}
    r2_df = pd.DataFrame([r2_dict_with_name], columns=r2_dict_with_name.keys())
    # r2_df.to_csv(f"{output_file}_r2_scores.csv", index=False)
    print(f"{datetime.now()}: finish plotting")
    return r2_df


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("path", help="path to the data file")
    parser.add_argument("-o", "--output_file", default="correlation")
    parser.add_argument("-x", "--x_label", default="estimate")
    parser.add_argument("-y", "--y_label", default="golden")
    args = parser.parse_args()
    plot_correlation(args.path, args.output_file, args.x_label, args.y_label)
