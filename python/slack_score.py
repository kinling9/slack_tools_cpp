#!/usr/bin/env python3

import extract_timing
import argparse
import math
import pandas as pd


def slack_score(test_path: str, target_path: str, period):
    def get_values(df):
        overall = df[df["Path_Group"] == "OVERALL_SUMMARY"].iloc[0]
        r2r = df[df["Path_Group"] == "reg2reg"].iloc[0]
        return {
            "wns": overall["WNS_ns"],
            "tns": overall["TNS_ns"],
            "wns100": overall["WNS100_ns"],
            "r2r_wns": r2r["WNS_ns"],
            "r2r_tns": r2r["TNS_ns"],
        }

    def clipped(val):
        return min(val, 0)

    test_data = get_values(extract_timing.get_timing_analysis(test_path))
    target_data = get_values(extract_timing.get_timing_analysis(target_path))

    def score(a, b, log_scale=False):
        a_c, b_c = clipped(a), clipped(b)
        if log_scale:
            a_log = math.log2(100 * period - a_c)
            b_log = math.log2(100 * period - b_c)
            return 0.1 * abs(abs(a_log) - abs(b_log))
        return abs(a_c - b_c) / abs(period)

    return (
        {
            f"{key}_score": 100
            - 100 * score(test_data[key], target_data[key], "tns" in key)
            for key in ["wns", "tns", "wns100", "r2r_wns", "r2r_tns"]
        }
        | {f"test_{k}": v for k, v in test_data.items()}
        | {f"target_{k}": v for k, v in target_data.items()}
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Calculate score based on QoR report.")
    parser.add_argument("--test_path", type=str, help="Path to the QoR report file")
    parser.add_argument("--target_path", type=str, help="Path to the QoR report file")
    parser.add_argument("--period", type=float, help="Path to the QoR report file")
    args = parser.parse_args()

    score_value = slack_score(args.test_path, args.target_path, args.period)
    print(f"Score: {score_value}")
