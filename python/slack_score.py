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
    test_r2r_wns = df_test[df_test["Path_Group"] == "reg2reg"]["WNS_ns"].values[0]
    test_r2r_tns = df_test[df_test["Path_Group"] == "reg2reg"]["TNS_ns"].values[0]
    test_wns100 = df_test[df_test["Path_Group"] == "OVERALL_SUMMARY"][
        "WNS100_ns"
    ].values[0]
    print(
        f"Test WNS: {test_wns}, Test TNS: {test_tns}, Test R2R WNS: {test_r2r_wns}, Test R2R TNS: {test_r2r_tns}"
    )
    df_target = extract_timing.get_timing_analysis(target_path)
    target_wns = df_target[df_target["Path_Group"] == "OVERALL_SUMMARY"][
        "WNS_ns"
    ].values[0]
    target_tns = df_target[df_target["Path_Group"] == "OVERALL_SUMMARY"][
        "TNS_ns"
    ].values[0]
    target_r2r_wns = df_target[df_target["Path_Group"] == "reg2reg"]["WNS_ns"].values[0]
    target_r2r_tns = df_target[df_target["Path_Group"] == "reg2reg"]["TNS_ns"].values[0]
    target_wns100 = df_target[df_target["Path_Group"] == "OVERALL_SUMMARY"][
        "WNS100_ns"
    ].values[0]

    print(
        f"Target WNS: {target_wns}, Target TNS: {target_tns}, Target R2R WNS: {target_r2r_wns}, Target R2R TNS: {target_r2r_tns}"
    )
    test_wns_cal = min(test_wns, 0)
    test_tns_cal = min(test_tns, 0)
    test_r2r_wns_cal = min(test_r2r_wns, 0)
    test_r2r_tns_cal = min(test_r2r_tns, 0)
    test_wns100_cal = min(test_wns100, 0)
    target_wns_cal = min(target_wns, 0)
    target_tns_cal = min(target_tns, 0)
    target_r2r_wns_cal = min(target_r2r_wns, 0)
    target_r2r_tns_cal = min(target_r2r_tns, 0)
    target_wns100_cal = min(target_wns100, 0)
    wns_score = abs(test_wns_cal - target_wns_cal) / abs(period)
    tns_score = (
        abs(
            abs(math.log2(100 * period - test_tns_cal))
            - abs(math.log2(100 * period - target_tns_cal))
        )
        * 0.1
    )
    r2r_wns_score = abs(test_r2r_wns_cal - target_r2r_wns_cal) / abs(period)
    r2r_tns_score = (
        abs(
            abs(math.log2(100 * period - test_r2r_tns_cal))
            - abs(math.log2(100 * period - target_r2r_tns_cal))
        )
        * 0.1
    )
    wns100_score = (
        abs(
            abs(math.log2(100 * period - test_wns100_cal))
            - abs(math.log2(100 * period - target_wns100_cal))
        )
        * 0.1
    )
    return {
        "target_wns": target_wns,
        "test_wns": test_wns,
        "wns_score": 100 - 100 * wns_score,
        "target_wns100": target_wns100,
        "test_wns100": test_wns100,
        "wns100_score": 100 - 100 * wns100_score,
        "target_tns": target_tns,
        "test_tns": test_tns,
        "tns_score": 100 - 100 * tns_score,
        "target_r2r_wns": target_r2r_wns,
        "test_r2r_wns": test_r2r_wns,
        "r2r_wns_score": 100 - 100 * r2r_wns_score,
        "target_r2r_tns": target_r2r_tns,
        "test_r2r_tns": test_r2r_tns,
        "r2r_tns_score": 100 - 100 * r2r_tns_score,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Calculate score based on QoR report.")
    parser.add_argument("--test_path", type=str, help="Path to the QoR report file")
    parser.add_argument("--target_path", type=str, help="Path to the QoR report file")
    parser.add_argument("--period", type=float, help="Path to the QoR report file")
    args = parser.parse_args()

    score_value = slack_score(args.test_path, args.target_path, args.period)
    print(f"Score: {score_value}")
