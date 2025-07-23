#!/usr/bin/env python3

import extract_timing
import argparse
import math
import pandas as pd


def slack_score(test_path: str, target_path: str, period):
    """
    Calculate the score based on WNS and TNS values from the QoR report.

    Args:
        file_path: Path to the QoR report file_path
    """
    df_test = extract_timing.get_timing_analysis(test_path)
    test_wns = df_test[df_test["Path_Group"] == "OVERALL_SUMMARY"]["WNS_ns"].values[0]
    test_tns = df_test[df_test["Path_Group"] == "OVERALL_SUMMARY"]["TNS_ns"].values[0]
    print(f"Test WNS: {test_wns}, Test TNS: {test_tns}")
    df_target = extract_timing.get_timing_analysis(target_path)
    target_wns = df_target[df_target["Path_Group"] == "OVERALL_SUMMARY"][
        "WNS_ns"
    ].values[0]
    target_tns = df_target[df_target["Path_Group"] == "OVERALL_SUMMARY"][
        "TNS_ns"
    ].values[0]
    print(f"Target WNS: {target_wns}, Target TNS: {target_tns}")
    wns_score = abs(test_wns - target_wns) / abs(period)
    tns_score = (
        abs(math.log2(100 * period - test_tns))
        - abs(math.log2(100 * period - target_tns))
    ) * 0.1
    return {
        "target_wns": target_wns,
        "test_wns": test_wns,
        "wns_score": 100 - 100 * wns_score,
        "target_tns": target_tns,
        "test_tns": test_tns,
        "tns_score": 100 - 100 * tns_score,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Calculate score based on QoR report.")
    parser.add_argument("--test_path", type=str, help="Path to the QoR report file")
    parser.add_argument("--target_path", type=str, help="Path to the QoR report file")
    parser.add_argument("--period", type=float, help="Path to the QoR report file")
    args = parser.parse_args()

    score_value = slack_score(args.test_path, args.target_path, args.period)
    print(f"Score: {score_value}")
