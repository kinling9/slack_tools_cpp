#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import yaml
import os
import json_decoder
import logging


def generate_yaml_content(results: list, output_dir: str, analyse_type: str) -> tuple:
    arc_yaml = {
        "mode": f"{analyse_type} analyse",
        "rpts": {},
        "configs": {
            "output_dir": output_dir,
            "analyse_tuples": [],
            "enable_rise_fall": True,
            "enable_super_arc": True,
        },
    }
    endpoint_yaml = {
        "mode": "compare",
        "rpts": {},
        "configs": {
            "output_dir": output_dir,
            "analyse_tuples": [],
            "compare_mode": "endpoint",
            "slack_filter": "x < 1",
        },
    }

    arc_analyse_type = ""
    for result in results:
        short = result["values"]["SHORT"]
        analyse_tuple = []
        types = []

        for key, arc_inputs in result["results"]["arc"].items():
            # Handle arc inputs
            if isinstance(arc_inputs, dict):
                input_dicts = arc_inputs.copy()
                if "net_csv" in input_dicts:
                    input_dicts.pop("path", None)
                    input_dicts["type"] = "csv"
                    types.append("csv")
                else:
                    input_dicts["type"] = "leda"
                    types.append("leda")
                arc_yaml["rpts"][f"{short}_{key}"] = input_dicts
            else:
                arc_yaml["rpts"][f"{short}_{key}"] = {
                    "path": arc_inputs,
                    "type": "leda",
                }
                types.append("leda")

            # Handle endpoint inputs
            endpoint_input = result["results"]["endpoint"][key]
            endpoint_yaml["rpts"][f"{short}_{key}"] = {
                "path": endpoint_input,
                "type": "leda_endpoint",
            }
            analyse_tuple.append(f"{short}_{key}")

        # Determine arc analysis type
        type_map = {
            ("csv", "csv"): f"{analyse_type} analyse graph",
            ("leda", "leda"): f"{analyse_type} analyse",
            ("csv", "leda"): f"{analyse_type} analyse csv",
        }

        cur_arc_analyse_type = type_map.get(tuple(types))
        if not cur_arc_analyse_type:
            logging.error(f"type: {types} is not supported")
            exit(0)

        if arc_analyse_type and arc_analyse_type != cur_arc_analyse_type:
            logging.error(
                f"Mixed analyse types: {arc_analyse_type} and {cur_arc_analyse_type} are not supported"
            )
            exit(0)

        arc_analyse_type = cur_arc_analyse_type
        arc_yaml["configs"]["analyse_tuples"].append(analyse_tuple)
        endpoint_yaml["configs"]["analyse_tuples"].append(analyse_tuple)

    arc_yaml["mode"] = arc_analyse_type
    return arc_yaml, endpoint_yaml


def generate_yaml(
    results: list, output: str = "output", analyse_type: str = "pair"
) -> tuple:
    arc_yaml, endpoint_yaml = generate_yaml_content(results, output, analyse_type)
    if not os.path.exists("tmp_yml"):
        os.makedirs("tmp_yml")

    with open(f"tmp_yml/{output}_arc.yml", "w") as yaml_file:
        yaml.dump(
            arc_yaml,
            yaml_file,
            default_flow_style=False,
            sort_keys=False,
        )
    with open(f"tmp_yml/{output}_endpoint.yml", "w") as yaml_file:
        yaml.dump(
            endpoint_yaml,
            yaml_file,
            default_flow_style=False,
            sort_keys=False,
        )

    return f"tmp_yml/{output}_arc.yml", f"tmp_yml/{output}_endpoint.yml"


if __name__ == "__main__":

    # Configure logging
    logging.basicConfig(
        level=logging.DEBUG, format="%(asctime)s - %(levelname)s - %(message)s"
    )
    parser = argparse.ArgumentParser(description="Generate YAML content.")
    parser.add_argument(
        "path",
        type=str,
        help="Path to the JSON file containing the template and variables.",
    )
    args = parser.parse_args()
    json_file = args.path
    base_name = os.path.splitext(os.path.basename(json_file))[0]
    results = json_decoder.process_json(json_file)
    generate_yaml(results, base_name)
