#!/usr/bin/env python3
import numpy as np
import matplotlib.pyplot as plt
import json
import argparse


def gen_data(name, datas):
    if name == "detour":
        result = np.array(
            [
                [v[name][0], v[name][1]]
                for _, v in datas
                if v[name][0] > 1.05 or v[name][1] > 1.05
            ]
        )
    if name == "data_latency" or name == "clock_latency":
        result = np.array(
            [
                [v[name][0], v[name][1]]
                for _, v in datas
                if name in v and abs(v[name][0]) > 1e-5 and abs(v[name][1]) > 1e-5
            ]
        )
    else:
        result = np.array([[v[name][0], v[name][1]] for _, v in datas if name in v])
    return result.T


def plot_data(name, datas, labels: list):
    data = gen_data(name, datas)
    _, (ax0, ax1, ax2) = plt.subplots(1, 3, figsize=(15, 5))
    min_val = min(np.min(data[0]), np.min(data[1]))
    max_val = max(np.max(data[0]), np.max(data[1]))
    bins = np.linspace(min_val, max_val, 60)
    ax0.hist(data[0], bins=bins, alpha=0.7, label=labels[0])
    ax0.hist(data[1], bins=bins, alpha=0.7, label=labels[1])
    ax0.set_title(f"Distribution of {name}")
    ax0.legend()
    ax0.grid(True, alpha=0.3)
    ax1.scatter(data[0], data[1], alpha=0.5)
    func = np.polyfit(data[0], data[1], 1)
    xn = np.linspace(min(data[0]), max(data[0]), 1000)
    yn = np.poly1d(func)
    ax1.plot(xn, yn(xn))
    ax1.set_title(f"scatter of {name}")
    ax1.set_xlabel(labels[0])
    ax1.set_ylabel(labels[1])
    ax2.plot(data[0], label=labels[0])
    ax2.plot(data[1], label=labels[1])
    ax2.set_title(f"{name} change by critical(matched)")
    ax2.legend()
    ax2.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "name",
        help="name of the data to plot",
        choices=[
            "slack",
            "length",
            "detour",
            "cell_delay_pct",
            "net_delay_pct",
            "data_latency",
            "clock_latency",
            "clock_uncertainty",
            "input_external_delay",
            "output_external_delay",
            "library_setup_time",
        ],
    )
    parser.add_argument("path", help="path to the data file")
    parser.add_argument("-x", "--xlabel", help="xlabel of the plot", default="base")
    parser.add_argument("-y", "--ylabel", help="ylabel of the plot", default="target")
    args = parser.parse_args()
    with open(args.path) as f:
        data = json.load(f)
    plot_data(args.name, data, [args.xlabel, args.ylabel])
