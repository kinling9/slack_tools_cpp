#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import orjson
import argparse
import seaborn as sns
import pandas as pd
import matplotlib.pyplot as plt


def calculate_buffer_num(data_item):
    """Calculate buffer number based on pins length."""
    return (len(data_item["value"]["pins"]) - 3) / 2


def calculate_delta_delay(data_item):
    """Calculate delta delay between key and value pins."""
    key_pin_delays = sum(pin["incr_delay"] for pin in data_item["key"]["pins"][2:])
    value_pin_delays = sum(pin["incr_delay"] for pin in data_item["value"]["pins"][2:])
    return key_pin_delays - value_pin_delays


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Process delay data from JSON to plot."
    )
    parser.add_argument("input_json", type=str, help="Input JSON file")
    args = parser.parse_args()

    input_json = args.input_json

    with open(input_json, "rb") as file:
        data = orjson.loads(file.read())

    buffer_num = [calculate_buffer_num(item) for _, item in data.items()]
    delta_delay = [calculate_delta_delay(item) for _, item in data.items()]

    # Create a DataFrame for Seaborn
    df = pd.DataFrame({"Buffer Number": buffer_num, "Delta Delay": delta_delay})

    # Generate the violin plot
    plt.figure(figsize=(8, 6))
    sns.violinplot(x="Buffer Number", y="Delta Delay", data=df)
    plt.title("Distribution of Delta Delay by Buffer Number")
    plt.xlabel("Buffer Number")
    plt.ylabel("Delta Delay")
    plt.grid(True)
    plt.show()
