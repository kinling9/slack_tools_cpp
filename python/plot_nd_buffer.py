#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import json
import argparse
import csv
import matplotlib.pyplot as plt

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Process delay data from JSON to plot."
    )
    parser.add_argument("input_json", type=str, help="Input JSON file")

    args = parser.parse_args()

    input_json = args.input_json

    with open(input_json, "r") as file:
        data = json.load(file)

    # Example: Accessing values (adjust according to your JSON structure)
    buffer_num = [(len(item["value"]["pins"]) - 3) / 2 for _, item in data.items()]
    delta_delay = [
        sum([i["incr_delay"] for i in item["key"]["pins"][1:-2]])
        - sum([i["incr_delay"] for i in item["value"]["pins"][1:-2]])
        for _, item in data.items()
    ]

    plt.figure(figsize=(8, 6))
    plt.scatter(buffer_num, delta_delay, color="blue", label="Delta Delay")
    plt.xlabel("Buffer Number")
    plt.ylabel("Delta Delay")
    plt.title("Scatter Plot of Buffer Number vs Delta Delay")
    plt.legend()
    plt.grid(True)
    plt.show()
