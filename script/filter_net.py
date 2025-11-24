#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import json
import argparse
import csv


def sort_and_convert(input_file, output_csv, top_n=None):
    try:
        with open(input_file, "r") as f:
            data = json.load(f)

        sorted_entries = sorted(
            data.items(), key=lambda x: abs(x[1].get("delta_delay", 0)), reverse=True
        )

        if top_n is not None:
            sorted_entries = sorted_entries[:top_n]
        sorted_data = {k: v for k, v in sorted_entries}

        mean_absolute_error = sum(
            abs(sorted_data[k]["delta_delay"]) for k in sorted_data
        ) / len(sorted_data)
        max_error = abs(sorted_data[sorted_entries[0][0]]["delta_delay"])
        # print(f"Mean Absolute Error: {mean_absolute_error:.2f} ns")
        # print(f"Max Absolute Error: {max_absolute_error:.2f} ns")

        with open(output_csv, "w", newline="") as csvfile:
            writer = csv.writer(csvfile)
            header = [
                "from_pin",
                "to_pin",
                "Delay Diff (ns)",
                "key_from_location",
                "key_to_location",
                "value_from_location",
                "value_to_location",
                "key_driving_cell",
                "value_driving_cell",
            ]
            writer.writerow(header)
            for _, entry_data in sorted_data.items():

                from_pin = entry_data.get("from", "N/A")
                to_pin = entry_data.get("to", "N/A")
                delta_delay = entry_data.get("delta_delay", "N/A")

                try:
                    first_pin_location = entry_data["key"]["pins"][0]["location"]
                    if isinstance(first_pin_location, list):
                        key_from_location = (
                            f"({first_pin_location[0]}, {first_pin_location[1]})"
                        )
                    else:
                        key_from_location = str(first_pin_location)
                except (KeyError, IndexError, TypeError):
                    key_from_location = "N/A"

                try:
                    pins = entry_data["key"]["pins"]
                    last_pin = pins[-1]
                    last_loc = last_pin["location"]
                    key_to_location = (
                        f"({last_loc[0]}, {last_loc[1]})"
                        if isinstance(last_loc, list)
                        else str(last_loc)
                    )
                except (KeyError, IndexError, TypeError):
                    key_to_location = "N/A"

                try:
                    value_first_pin = entry_data["value"]["pins"][0]
                    value_first_loc = value_first_pin["location"]
                    value_from_location = (
                        f"({value_first_loc[0]}, {value_first_loc[1]})"
                        if isinstance(value_first_loc, list)
                        else str(value_first_loc)
                    )
                except (KeyError, IndexError, TypeError):
                    value_from_location = "N/A"

                try:
                    value_pins = entry_data["value"]["pins"]
                    value_last_pin = value_pins[-1]
                    value_last_loc = value_last_pin["location"]
                    value_to_location = (
                        f"({value_last_loc[0]}, {value_last_loc[1]})"
                        if isinstance(value_last_loc, list)
                        else str(value_last_loc)
                    )
                except (KeyError, IndexError, TypeError):
                    value_to_location = "N/A"

                try:
                    key_cell = entry_data["key"]["pins"][0]["cell"]
                except (KeyError, IndexError, TypeError):
                    key_cell = "N/A"
                try:
                    value_cell = entry_data["value"]["pins"][0]["cell"]
                except (KeyError, IndexError, TypeError):
                    value_cell = "N/A"
                writer.writerow(
                    [
                        from_pin,
                        to_pin,
                        delta_delay,
                        key_from_location,
                        key_to_location,
                        value_from_location,
                        value_to_location,
                        key_cell,
                        value_cell,
                    ]
                )

        print(f"\nSuccess! Processed {len(sorted_data)} entries.")
        print(f"CSV file saved to: {output_csv}")
        return mean_absolute_error, max_error
    except FileNotFoundError:
        print(f"Error: Input file '{input_file}' not found.")
    except json.JSONDecodeError:
        print(f"Error: Invalid JSON format in '{input_file}'.")
    except ValueError:
        print("Error: top_n must be an integer.")
    except Exception as e:
        print(f"Unexpected error: {str(e)}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Process delay data from JSON to CSV.")
    parser.add_argument("input_json", type=str, help="Input JSON file")
    parser.add_argument("output_csv", type=str, help="Output CSV file")
    parser.add_argument(
        "top_n", type=int, nargs="?", default=None, help="Top N records to process"
    )

    args = parser.parse_args()

    input_json = args.input_json
    output_csv = args.output_csv
    top_n = args.top_n
    sort_and_convert(input_json, output_csv, top_n)
