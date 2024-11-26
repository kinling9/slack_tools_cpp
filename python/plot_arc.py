#!/usr/bin/env python3
import numpy as np
import matplotlib.pyplot as plt
import json
import argparse


def gen_data(name, datas):
    name = f"{name} arc"
    # result = np.array([[v[name][0], v[name][1]] for _, v in datas])
    arc_list = [i for i in datas if i["type"] == name]
    dly_array = np.array([[i["key"]["delay"], i["value"]["delay"]] for i in arc_list])
    length_array = np.array(
        [[i["key"]["length"], i["value"]["length"]] for i in arc_list]
    )
    dly_array = dly_array.T
    length_array = length_array.T
    return dly_array, length_array


def plot_one(name, data):
    _, (ax0, ax1) = plt.subplots(1, 2, figsize=(10, 5))
    min_val = min(np.min(data[0]), np.min(data[1]))
    max_val = max(np.max(data[0]), np.min(data[1]))
    bins = np.linspace(min_val, max_val, 60)
    ax0.hist(data[0], bins=bins, alpha=0.7, label="leda")
    ax0.hist(data[1], bins=bins, alpha=0.7, label="invs")
    ax0.set_title(f"Distribution of {name}")
    ax0.legend()
    ax0.grid(True, alpha=0.3)
    ax1.scatter(data[0], data[1], alpha=0.5)
    func = np.polyfit(data[0], data[1], 1)
    xn = np.linspace(min(data[0]), max(data[0]), 1000)
    yn = np.poly1d(func)
    ax1.plot(xn, yn(xn))
    ax1.set_title(f"scatter of {name}")
    ax1.set_xlabel("leda")
    ax1.set_ylabel("invs")
    plt.tight_layout()
    plt.show()


def plot_data(name, datas):
    dly_array, length_array = gen_data(name, datas)
    plot_one("delay", dly_array)
    if name == "net":
        plot_one("length", length_array)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "name",
        help="name of the data to plot",
        choices=["net", "cell"],
    )
    parser.add_argument("path", help="path to the data file")
    args = parser.parse_args()
    with open(args.path) as f:
        data = json.load(f)
    plot_data(args.name, data)
