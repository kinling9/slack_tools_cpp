#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import json_decoder
import argparse
import yaml
import json
import os


def generate_yaml_content(json_file: str, output_dir: str = "output"):
    arc_yaml = {
        "mode": "pair analyse",
        "rpts": {},
        "configs": {
            "output_dir": "",
            "analyse_tuples": [],
            "enable_rise_fall": True,
            "enable_super_arc": True,
        },
    }
    endpoint_yaml = {
        "mode": "compare",
        "rpts": {},
        "configs": {
            "output_dir": "",
            "analyse_tuples": [],
            "compare_mode": "endpoint",
        },
    }
    # Generate the YAML content based on the provided structure
    results = json_decoder.process_json(json_file)
    arc_yaml["configs"]["output_dir"] = output_dir
    endpoint_yaml["configs"]["output_dir"] = output_dir
    for result in results:
        short = result["values"]["SHORT"]
        analyse_tuple = []
        for k, v in result["results"]["arc"].items():
            arc_yaml["rpts"][f"{short}_{k}"] = {
                "path": v,
                "type": "leda",
            }
            analyse_tuple.append(f"{short}_{k}")
        arc_yaml["configs"]["analyse_tuples"].append(analyse_tuple)
        for k, v in result["results"]["endpoint"].items():
            endpoint_yaml["rpts"][f"{short}_{k}"] = {
                "path": v,
                "type": "leda_endpoint",
            }
        endpoint_yaml["configs"]["analyse_tuples"].append(analyse_tuple)

    return arc_yaml, endpoint_yaml


def generate_yaml(json_file: str) -> tuple:

    with open(json_file, "r") as file:
        data = json.load(file)
    output = data.get("variables", {}).get("OUTPUT", "output")
    arc_yaml, endpoint_yaml = generate_yaml_content(json_file, output)
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
    parser = argparse.ArgumentParser(description="Generate YAML content.")
    parser.add_argument(
        "--json_file",
        type=str,
        required=True,
        help="Path to the JSON file containing the template and variables.",
    )
    args = parser.parse_args()
    generate_yaml(args.json_file)
