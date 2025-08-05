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

    def calc_score(a, b, log_scale=False):
        a_c, b_c = clipped(a), clipped(b)
        diff = abs(a_c - b_c)
        if log_scale:
            a_log = math.log2(100 * period - a_c)
            b_log = math.log2(100 * period - b_c)
            return 0.1 * abs(abs(a_log) - abs(b_log))
        return diff / abs(period)

    # Ordered keys as: wns, wns100, tns, r2r_wns, r2r_tns
    ordered_keys = ["wns", "wns100", "tns", "r2r_wns", "r2r_tns"]
    log_scale_keys = {"tns", "r2r_tns"}

    result = {}
    for key in ordered_keys:
        test_val = test_data[key]
        target_val = target_data[key]
        score_val = calc_score(test_val, target_val, key in log_scale_keys)
        result[f"target_{key}"] = target_val
        result[f"test_{key}"] = test_val
        result[f"{key}_score"] = 100 - 100 * score_val

    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Calculate score based on QoR report.")
    parser.add_argument("--test_path", type=str, help="Path to the QoR report file")
    parser.add_argument("--target_path", type=str, help="Path to the QoR report file")
    parser.add_argument("--period", type=float, help="Path to the QoR report file")
    args = parser.parse_args()

    score_value = slack_score(args.test_path, args.target_path, args.period)
    print(f"Score: {score_value}")
