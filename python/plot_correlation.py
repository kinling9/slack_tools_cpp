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
    res = ""
    keep_until = len(ls_layer) - digit_shrink_level
    keep_until = max(0, keep_until)
    if digit_shrink_level == -1:
        keep_until = 0
    for i in range(0, keep_until):
        res += "/" if i > 0 else ""
        res += ls_layer[i]
    for i in range(max(0, keep_until), len(ls_layer)):
        res += "/" if i > 0 else ""
        res += replace_digits(ls_layer[i])
    return res


def gen_data(datas):
    data_dict = {
        k: {
            "delay": [v["key"]["delay"], v["value"]["delay"]],
            "size": 1,
            "type": v["type"],
            "from": v["from"],
            "to": v["to"],
        }
        for k, v in datas.items()
    }
    return data_dict


def group_arc(data_dict):
    data_dict_grouped = {}
    for key, value in data_dict.items():
        group_key = get_reg_group(value["from"]) + "-" + get_reg_group(value["to"])
        if group_key not in data_dict_grouped:
            data_dict_grouped[group_key] = {
                "delay": [0, 0],
                "size": 0,
                "type": "cell arc",
            }
        data_dict_grouped[group_key]["delay"] = [
            a + b for a, b in zip(value["delay"], data_dict_grouped[group_key]["delay"])
        ]
        data_dict_grouped[group_key]["size"] += 1
    return data_dict_grouped


def group_reg(data_dict):
    data_dict_grouped = {}
    for key, value in data_dict.items():
        group_key = get_reg_group(key)
        if group_key not in data_dict_grouped:
            data_dict_grouped[group_key] = {
                "delay": [0, 0],
                "size": 0,
                "type": "endpoint_grp",
            }
        data_dict_grouped[group_key]["delay"] = [
            a + b for a, b in zip(value["delay"], data_dict_grouped[group_key]["delay"])
        ]
        data_dict_grouped[group_key]["size"] += 1
    return data_dict_grouped


def plot_group(data_dict: dict, name: str, x_label, y_label):
    # Separate data into cell arcs and net arcs
    cell_x = []
    cell_y = []
    cell_size = []
    net_x = []
    net_y = []
    net_size = []
    scatter_type = ""

    for k, v in data_dict.items():
        if "type" in v and v["type"] != "net arc":
            cell_x.append(v["delay"][0])
            cell_y.append(v["delay"][1])
            cell_size.append(v["size"] * (100 if v["type"] != "endpoint_grp" else 1))
            scatter_type = v["type"]
        elif "type" in v and v["type"] == "net arc":
            net_x.append(v["delay"][0])
            net_y.append(v["delay"][1])
            net_size.append(v["size"] * 100)

    # Combine all data for regression line and histogram
    x_list = cell_x + net_x
    y_list = cell_y + net_y
    # size_list = cell_size + net_size
    fig, [ax0, ax1] = plt.subplots(
        nrows=1, ncols=2, figsize=(16, 6), gridspec_kw={"width_ratios": [1, 1.2]}
    )
    # ax0.axis("equal")
    ax0.set_xlabel(x_label)
    ax0.set_ylabel(y_label)
    ax0.axhline(y=0, color="green", linestyle="--")
    # Scatter plot for cell arcs (red)
    group_color = "red" if len(net_y) else "k"
    ax0.scatter(
        cell_x,
        cell_y,
        c=group_color,
        marker=".",
        linewidth=0,
        s=cell_size,
        alpha=0.5,
        label="cell arc" if len(net_y) else scatter_type,
    )

    # Scatter plot for net arcs (blue)
    if len(net_y):
        ax0.scatter(
            net_x,
            net_y,
            c="blue",
            marker=".",
            linewidth=0,
            s=net_size,
            alpha=0.5,
            label="net arc",
        )

    # Add legend for scatter plots
    ax0.legend()
    ax0.grid(ls=":", color="blue")

    func = np.polyfit(x_list, y_list, 1)
    xn = np.linspace(min(x_list), max(x_list), 1000)
    yn = np.poly1d(func)
    r2 = sklearn.metrics.r2_score(y_list, x_list)
    scale_r2 = sklearn.metrics.r2_score(y_list, yn(x_list))
    ax0.set_title("Scatter plot (r2 = {:0.4f})".format(r2), fontsize=10)
    ax0.plot(xn, yn(xn))
    ax0.plot(xn, xn)

    ax1.set_title("2D histogram", fontsize=10)
    h = ax1.hist2d(x_list, y_list, bins=50)
    ax1.set_aspect("equal", adjustable="box")
    ax1.axhline(y=0, color="green", linestyle="--")
    ax1.axvline(x=0, color="green", linestyle="--")
    fig.colorbar(h[3], ax=ax1)

    ax0.set_xlim(ax1.get_xlim())
    ax0.set_ylim(ax1.get_ylim())
    ax0.set_aspect("equal", adjustable="box")

    plt.savefig(name, bbox_inches="tight")
    return float(r2), float(scale_r2)


def plot_correlation(path, output_file, x_label, y_label):
    # collect data
    print(f"{datetime.now()}: start collecting data")

    with open(path) as f:
        data = json.load(f)
    data_dict = gen_data(data)

    data_dict_df = pd.DataFrame.from_dict(data_dict, orient="index")
    with open(f"{output_file}_arc.csv", "w") as f:
        data_dict_df.to_csv(f)

    print(f"# values: {len(data)}, matched groups = {len(data_dict)}")

    # plot
    print(f"{datetime.now()}: start plotting")
    r2_dict = {
        "arc": 0.0,
        "arc_scaled": 0.0,
        "num_arc": len(data_dict),
    }
    r2_result = plot_group(data_dict, f"{output_file}_arc.png", x_label, y_label)
    r2_dict["arc"] = r2_result[0]
    r2_dict["arc_scaled"] = r2_result[1]

    # After calculating r2_dict, create a DataFrame and save to CSV
    r2_dict_with_name = {"name": output_file.split("/")[-1], **r2_dict}
    r2_df = pd.DataFrame([r2_dict_with_name], columns=r2_dict_with_name.keys())
    print(f"{datetime.now()}: finish plotting")
    return r2_df


def collect_data_to_dict(data_file_name):
    data_dict = dict()
    data_file = open(data_file_name)
    for line in data_file:
        tokens = line.split()
        if tokens[0] in data_dict:
            print(
                f"Warning: ignore another occurance of key '{tokens[0]}' in file '{data_file_name}'"
            )
            continue
        data_dict[tokens[0]] = float(tokens[1])
    return data_dict


def plot_text(path: list, output_file, x_label, y_label):
    x_data_dict = collect_data_to_dict(path[0])
    y_data_dict = collect_data_to_dict(path[1])

    data_dict = {}

    for k, v in x_data_dict.items():
        if k in y_data_dict:
            data_dict[k] = {
                "delay": [v, y_data_dict[k]],
                "size": 1,
                "type": "endpoint",
            }
    grouped_dict = group_reg(data_dict)
    avg_dict = {
        k: {
            "delay": [i / v["size"] for i in v["delay"]],
            "size": v["size"],
            "type": v["type"],
        }
        for k, v in grouped_dict.items()
    }
    avg_dict_df = pd.DataFrame.from_dict(avg_dict, orient="index")
    with open(f"{output_file}_end_avg.csv", "w") as f:
        avg_dict_df.to_csv(f)

    r2_dict = {
        "end": 0.0,
        "end_scaled": 0.0,
        "end_group": 0.0,
        "end_group_scaled": 0.0,
        "num_end": len(data_dict),
        "num_group": len(avg_dict),
    }
    end_result = plot_group(data_dict, f"{output_file}_endpoint.png", x_label, y_label)
    end_group_result = plot_group(
        avg_dict, f"{output_file}_endpoint_avg.png", x_label, y_label
    )
    r2_dict["end"] = end_result[0]
    r2_dict["end_scaled"] = end_result[1]
    r2_dict["end_group"] = end_group_result[0]
    r2_dict["end_group_scaled"] = end_group_result[1]
    r2_dict_with_name = {"name": output_file.split("/")[-1], **r2_dict}
    r2_df = pd.DataFrame([r2_dict_with_name], columns=r2_dict_with_name.keys())
    return r2_df


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("path", help="path to the data file")
    parser.add_argument("-o", "--output_file", default="correlation")
    parser.add_argument("-x", "--x_label", default="estimate")
    parser.add_argument("-y", "--y_label", default="golden")
    args = parser.parse_args()
    plot_correlation(args.path, args.output_file, args.x_label, args.y_label)
