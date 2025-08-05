#!/usr/bin/env python3
"""
Script to extract WNS (Worst Negative Slack) and TNS (Total Negative Slack)
from QoR timing reports.
"""

import re
import sys
import pandas as pd
from typing import Dict, List, Tuple, Optional


def parse_qor_report(file_path: str) -> List[Dict[str, str]]:
    """
    Parse QoR report and extract timing information for each path group.

    Args:
        file_path: Path to the QoR report file

    Returns:
        List of dictionaries containing timing info for each path group
    """
    timing_groups = []

    with open(file_path, "r") as file:
        content = file.read()

    # Pattern to match timing path group sections
    group_pattern = (
        r"Timing Path Group '([^']+)' \([^)]+\)\s*\n\s*-+\s*\n(.*?)\s*-+\s*\n"
    )

    matches = re.findall(group_pattern, content, re.DOTALL)

    for group_name, group_content in matches:
        # Extract timing metrics
        levels_match = re.search(r"Levels of Logic:\s*(\d+)", group_content)
        path_length_match = re.search(
            r"Critical Path Length:\s*([\d.-]+)", group_content
        )
        slack_match = re.search(r"Critical Path Slack:\s*([\d.-]+)", group_content)
        clk_period_match = re.search(
            r"Critical Path Clk Period:\s*([\d.-]+)", group_content
        )
        tns_match = re.search(r"Total Negative Slack:\s*([\d.-]+)", group_content)
        violations_match = re.search(r"No. of Violating Paths:\s*(\d+)", group_content)

        timing_info = {
            "group_name": group_name,
            "levels_of_logic": levels_match.group(1) if levels_match else "N/A",
            "critical_path_length": (
                path_length_match.group(1) if path_length_match else "N/A"
            ),
            "wns": (
                slack_match.group(1) if slack_match else "N/A"
            ),  # WNS = Critical Path Slack
            "clk_period": clk_period_match.group(1) if clk_period_match else "N/A",
            "tns": tns_match.group(1) if tns_match else "N/A",
            "violating_paths": violations_match.group(1) if violations_match else "N/A",
        }

        timing_groups.append(timing_info)

    return timing_groups


def extract_timing_metrics(report_text: str) -> Dict[str, float]:
    """
    Extract WNS, TNS, and NVP from timing report text.

    Args:
        report_text (str): The timing report text

    Returns:
        Dict[str, float]: Dictionary with 'WNS', 'TNS', 'NVP' keys and their values
    """
    metrics = {}

    # Pattern for Worst Negative Slack (WNS)
    wns_pattern = r"Worst Negative Slack.*?:\s*([-+]?\d*\.?\d+)"
    wns_match = re.search(wns_pattern, report_text, re.IGNORECASE)
    if wns_match:
        metrics["WNS"] = float(wns_match.group(1))

    # Pattern for Total Negative Slack (TNS)
    tns_pattern = r"Total Negative Slack.*?:\s*([-+]?\d*\.?\d+)"
    tns_match = re.search(tns_pattern, report_text, re.IGNORECASE)
    if tns_match:
        metrics["TNS"] = float(tns_match.group(1))

    # Pattern for Number of Violating Paths (NVP)
    nvp_pattern = r"Number of Violating Paths.*?:\s*(\d+)"
    nvp_match = re.search(nvp_pattern, report_text, re.IGNORECASE)
    if nvp_match:
        metrics["NVP"] = float(nvp_match.group(1))

    wns100_pattern = r"The 100th Path Slack.*?:\s*([-+]?\d*\.?\d+)"
    wns100_match = re.search(wns100_pattern, report_text, re.IGNORECASE)
    if wns100_match:
        metrics["WNS_100"] = float(wns100_match.group(1))

    return metrics


def get_timing_summary_df(timing_groups: List[Dict[str, str]]) -> pd.DataFrame:
    """
    Convert timing groups data to pandas DataFrame with proper data types.

    Args:
        timing_groups: List of timing group dictionaries

    Returns:
        pandas DataFrame with timing summary
    """
    # Convert to DataFrame
    df = pd.DataFrame(timing_groups)

    # Convert numeric columns to proper data types
    numeric_columns = [
        "levels_of_logic",
        "critical_path_length",
        "wns",
        "clk_period",
        "tns",
        "violating_paths",
    ]

    for col in numeric_columns:
        df[col] = pd.to_numeric(df[col], errors="coerce")

    # Rename columns for clarity
    df = df.rename(
        columns={
            "group_name": "Path_Group",
            "levels_of_logic": "Logic_Levels",
            "critical_path_length": "Path_Length_ns",
            "wns": "WNS_ns",
            "clk_period": "Clock_Period_ns",
            "tns": "TNS_ns",
            "violating_paths": "Violations",
        }
    )

    # Reorder columns for better readability
    column_order = [
        "Path_Group",
        "WNS_ns",
        "TNS_ns",
        "Violations",
        "Logic_Levels",
        "Path_Length_ns",
        "Clock_Period_ns",
    ]
    df = df[column_order]

    return df


def get_timing_analysis(file_path: str) -> pd.DataFrame:
    """
    Main function to parse QoR report and return pandas DataFrame with timing analysis.

    Args:
        file_path: Path to the QoR report file

    Returns:
        pandas DataFrame with timing summary and analysis
    """
    timing_groups = parse_qor_report(file_path)

    with open(file_path, "r") as file:
        summary_groups = extract_timing_metrics(file.read())

    if not timing_groups:
        print("No timing path groups found in the report.")
        return pd.DataFrame()

    df = get_timing_summary_df(timing_groups)

    # Add summary statistics
    summary_stats = {
        "Path_Group": "OVERALL_SUMMARY",
        "WNS_ns": summary_groups["WNS"],  # Worst (most negative) slack
        "TNS_ns": summary_groups["TNS"],  # Total of all negative slack
        "WNS100_ns": summary_groups.get("WNS_100", None),  # 100th path slack
        "Violations": df["Violations"].sum(),  # Total violations
        "Logic_Levels": df["Logic_Levels"].max(),  # Max logic levels
        "Path_Length_ns": df["Path_Length_ns"].max(),  # Longest path
        "Clock_Period_ns": (
            df["Clock_Period_ns"].iloc[0] if len(df) > 0 else None
        ),  # Clock period (should be same for all)
    }

    # Add summary row
    summary_df = pd.DataFrame([summary_stats])
    df_with_summary = pd.concat([df, summary_df], ignore_index=True)

    return df_with_summary


def export_to_csv(
    timing_groups: List[Dict[str, str]], output_file: str = "timing_summary.csv"
):
    """Export timing data to CSV file."""
    df = get_timing_summary_df(timing_groups)
    df.to_csv(output_file, index=False)
    print(f"\nData exported to {output_file}")


def main():
    """Main function to parse QoR report and extract timing information."""
    if len(sys.argv) < 2:
        print("Usage: python qor_parser.py <qor_report_file> [--csv]")
        print("Example: python qor_parser.py paste.txt --csv")
        sys.exit(1)

    file_path = sys.argv[1]
    export_csv = "--csv" in sys.argv

    try:
        # Get DataFrame with timing analysis
        df = get_timing_analysis(file_path)

        if df.empty:
            print("No timing path groups found in the report.")
            return

        # Display the DataFrame
        print("\n" + "=" * 100)
        print("TIMING SUMMARY - DataFrame")
        print("=" * 100)
        print(df.to_string(index=False, float_format="%.3f"))
        print("=" * 100)

        # Also show the traditional table format
        timing_groups = parse_qor_report(file_path)
        # print_summary_table(timing_groups)

        if export_csv:
            export_to_csv(timing_groups)

    except FileNotFoundError:
        print(f"Error: File '{file_path}' not found.")
        sys.exit(1)
    except Exception as e:
        print(f"Error parsing file: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
