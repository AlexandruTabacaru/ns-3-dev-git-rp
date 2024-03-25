import os
import pandas as pd
import numpy as np
import re
from collections import defaultdict
from pathlib import Path

def compute_statistics(dataframe, digits=3):
    if not dataframe.empty:
        return {
            "Average Latency": round(np.mean(dataframe["Latency"]) * 1000, digits),
            "P10 Latency": round(np.percentile(dataframe["Latency"], 10) * 1000, digits),
            "P90 Latency": round(np.percentile(dataframe["Latency"], 90) * 1000, digits),
            "P99 Latency": round(np.percentile(dataframe["Latency"], 99) * 1000, digits),
            "StdDev": round(np.std(dataframe["Latency"], ddof=1) * 1000, digits),
        }
    return None


def create_final_dataframe(results):
    df = pd.DataFrame(results)
    return df

def is_cubic_or_prague(filename):
    port_match = re.search(r"192\.168\.1\.2\s(\d{3})", filename)
    if port_match:
        port_number = int(port_match.group(1))
        if 100 <= port_number <= 199:
            return "prague"
        elif 200 <= port_number <= 299:
            return "cubic"
    return None

def process_test_directory(test_dir_path):
    data = {
        "cubic": {"UL": pd.DataFrame(), "DL": pd.DataFrame()},
        "prague": {"UL": pd.DataFrame(), "DL": pd.DataFrame()}
    }

    # Process latency files
    for file in os.listdir(test_dir_path):
        if file.endswith("latency.csv"):
            direction = "UL" if "192.168.1.2" in file.split(" to ")[0] else "DL"
            category = is_cubic_or_prague(file)
            if category:
                try:
                    df = pd.read_csv(os.path.join(test_dir_path, file), encoding='ISO-8859-1')
                    if not data[category][direction].empty:
                        data[category][direction] = pd.concat([data[category][direction], df], ignore_index=True)
                    else:
                        data[category][direction] = df
                except UnicodeDecodeError:
                    print(f"Error decoding file: {file}")

    # Process Bandwidth Calculation
    for direction, summary_file in [("DL", "summary_table_downstream.csv"), ("UL", "summary_table_upstream.csv")]:
        summary_path = os.path.join(test_dir_path, summary_file)
        if os.path.exists(summary_path):
            try:
                summary_df = pd.read_csv(summary_path, encoding='ISO-8859-1')
                # Assume 'Flow ID' column exists and categorization logic is applied here
                for category in ['cubic', 'prague']:
                    # Filter rows based on category using is_cubic_or_prague logic on 'Flow ID'
                    filtered_df = summary_df[summary_df['Flow ID'].apply(lambda x: is_cubic_or_prague(x) == category)]
                    bandwidth = filtered_df['Mean data rate (Mbps)'].sum() if not filtered_df.empty else 0
                    if not data[category][direction].empty:
                        data[category][direction]['Bandwidth (Mbps)'] = bandwidth
                    else:
                        # Create an empty DataFrame with a 'Bandwidth (Mbps)' column if it doesn't exist
                        data[category][direction] = pd.DataFrame({'Bandwidth (Mbps)': [bandwidth]})
            except UnicodeDecodeError as e:
                print(f"Error reading {summary_file}: {e}")

    # Process "CE %" from summary tables
    for direction, summary_file in [("DL", "summary_table_downstream.csv"), ("UL", "summary_table_upstream.csv")]:
        summary_path = os.path.join(test_dir_path, summary_file)
        if os.path.exists(summary_path):
            summary_df = pd.read_csv(summary_path, encoding='ISO-8859-1')
            # Ensure 'Num CE' and 'Num Packets' are numeric and handle potential missing values
            summary_df['Num CE'] = pd.to_numeric(summary_df['Num CE'], errors='coerce').fillna(0)
            summary_df['Num Packets'] = pd.to_numeric(summary_df['Num Packets'], errors='coerce').fillna(1)  # Avoid division by zero
            summary_df['CE %'] = (summary_df['Num CE'] / summary_df['Num Packets']) * 100
            
            for category in ['cubic', 'prague']:
                filtered_df = summary_df[summary_df['Flow ID'].apply(lambda x: is_cubic_or_prague(x) == category)]
                ce_percentage = filtered_df['CE %'].sum() if not filtered_df.empty else 0
                
                if 'CE %' not in data[category][direction].columns:
                    data[category][direction] = data[category][direction].assign(**{'CE %': ce_percentage})
                else:
                    data[category][direction]['CE %'] += ce_percentage

    return data

def processResults(root_dir):
    all_results = []

    test_run_dirs = [
        d for d in os.listdir(root_dir) if os.path.isdir(os.path.join(root_dir, d))
    ]

    for test_dir in test_run_dirs:
        test_dir_path = os.path.join(root_dir, test_dir)
        data = process_test_directory(test_dir_path)

        # Initialize a template for the results dictionary for this test directory
        test_results_template = {"Label": test_dir}

        for direction in ["UL", "DL"]:
            # Initialize default values
            default_stats = {"Average Latency": None, "P10 Latency": None, "P90 Latency": None, "P99 Latency": None, "StdDev Latency": None}

            # Compute statistics for cubic and prague if data is available
            if not data["cubic"][direction].empty and "Latency" in data["cubic"][direction].columns:
                stats_cubic = compute_statistics(data["cubic"][direction])
            else:
                stats_cubic = default_stats
            bandwidth_cubic = data["cubic"][direction]["Bandwidth (Mbps)"].mean() if "Bandwidth (Mbps)" in data["cubic"][direction].columns else 0
            ce_cubic = data["cubic"][direction]["CE %"].mean() if "CE %" in data["cubic"][direction].columns else 0

            if not data["prague"][direction].empty and "Latency" in data["prague"][direction].columns:
                stats_prague = compute_statistics(data["prague"][direction])
            else:
                stats_prague = default_stats
            bandwidth_prague = data["prague"][direction]["Bandwidth (Mbps)"].mean() if "Bandwidth (Mbps)" in data["prague"][direction].columns else 0
            ce_prague = data["prague"][direction]["CE %"].mean() if "CE %" in data["prague"][direction].columns else 0

            # Merge cubic and prague stats into the results template
            for key, value in stats_cubic.items():
                test_results_template[f"{key} {direction} Cubic"] = value
            test_results_template[f"Average Bandwidth {direction} Cubic (Mbps)"] = bandwidth_cubic
            test_results_template[f"CE % {direction} Cubic"] = ce_cubic

            for key, value in stats_prague.items():
                test_results_template[f"{key} {direction} Prague"] = value
            test_results_template[f"Average Bandwidth {direction} Prague (Mbps)"] = bandwidth_prague
            test_results_template[f"CE % {direction} Prague"] = ce_prague

        all_results.append(test_results_template)


    df_results = pd.DataFrame(all_results)
    results_csv_path = os.path.join(root_dir, "processed_results.csv")
    df_results.to_csv(results_csv_path, index=False)

    print(f"Combined results saved to {os.path.join(root_dir, 'processed_results.csv')}")

def post_process(root_dir, hidden_columns):
    df = pd.read_csv(os.path.join(root_dir, "results.csv"))

    # Extract TS# values from the Test Case column
    df['TS#'] = df['Test Case'].str.extract(r'TS(\d+)').astype(int)

    # Compute the max_ts value
    max_ts = df['TS#'].max()

    df['Log Rate Ratio'] = np.nan
    df['Latency Benefit'] = np.nan

    for index, row in df.iterrows():
        test_case = row['Test Case']
        tc_num = int(test_case.split('-')[1][2:])  # Extract TC# as integer

        if tc_num == 1:
            continue

        # For TC2 and above, compute values
        if tc_num >= 2:
            if tc_num == 2:
                # For TC2, use different rows for Prague and Cubic values
                target_index = index - max_ts
                if target_index >= 0:
                    try:
                        df.at[index, 'Log Rate Ratio'] = round(np.log10(
                            row['Average Bandwidth DL Prague (Mbps)'] /
                            df.at[target_index, 'Average Bandwidth DL Cubic (Mbps)']
                        ), 3)
                        df.at[index, 'Latency Benefit'] = round(
                            (df.at[target_index, 'P99 Latency DL Cubic'] - 
                            row['P99 Latency DL Prague']),3)
                    except ZeroDivisionError:
                        df.at[index, 'Log Rate Ratio'] = np.nan
            else:
                # For TC3 and above, use same row values
                try:
                    df.at[index, 'Log Rate Ratio'] = round(np.log10(
                        row['Average Bandwidth DL Prague (Mbps)'] / 
                        row['Average Bandwidth DL Cubic (Mbps)']
                    ),3)
                except ZeroDivisionError:
                    df.at[index, 'Log Rate Ratio'] = np.nan
                df.at[index, 'Latency Benefit'] = round(
                    row['P99 Latency DL Cubic'] - 
                    row['P99 Latency DL Prague'],3)

    # Drop temporary columns
    columns_to_drop = ['Test Case Match', 'AP', 'TC', 'TS', 'LS', 'TS#'] + hidden_columns
    df.drop(columns_to_drop, errors='ignore', axis=1, inplace=True)

    detailed_csv_path = os.path.join(root_dir, "detailed_results.csv")
    df.to_csv(detailed_csv_path, index=False)

    print(f"Post Processed Final merged results saved to {detailed_csv_path}")
    return Path(detailed_csv_path)


def merge_input_with_results(root_dir):
    input_df = pd.read_csv("config.csv")  # Contains "Test Case"
    results_df = pd.read_csv(os.path.join(root_dir, "processed_results.csv"))

    # Match the test case label from directory names so they line up
    results_df["Test Case Match"] = results_df["Label"].apply(lambda x: x.split("_")[0])

    merged_df = pd.merge(
        results_df,
        input_df,
        left_on="Test Case Match",
        right_on="Test Case",
        how="left",
    )

    # Extract AP#, LS#, TC#, TS# for sorting
    merged_df[['AP', 'TC', 'TS', 'LS']] = merged_df['Test Case'].str.extract(r'AP(\d+)-TC(\d+)-TS(\d+)-LS(\d+)')
    merged_df[['AP', 'TC', 'TS', 'LS']] = merged_df[['AP', 'TC', 'TS', 'LS']].astype(int)

    # Generate the 'link' column
    merged_df['link'] = merged_df['Label'].apply(lambda label: f"link:./{os.path.join(label)}[{label}]")
    merged_df['Label'] = merged_df['link']

    # Sort by AP#, LS#, TC#, TS#
    sorted_merged_df = merged_df.sort_values(by=['AP', 'LS', 'TC', 'TS'])

    # Reorder columns to place 'Label', 'Test Case', input_df columns, then the rest
    input_df_columns = [col for col in input_df.columns if col != 'Test Case']  # Avoid duplicating 'Test Case'
    final_columns = ['Label', 'Test Case'] + input_df_columns + \
                    [col for col in sorted_merged_df.columns if col not in ['Label', 'Test Case'] + input_df_columns + ['AP', 'TC', 'TS', 'LS', 'Test Case Match']]

    # Add 'link' as the last column
    final_columns = [col for col in final_columns if col != 'link'] + ['link']

    sorted_merged_df = sorted_merged_df[final_columns]

    # Drop temporary columns
    # columns_to_drop = ['Test Case Match', 'AP', 'TC', 'TS', 'LS']
    # sorted_merged_df.drop(columns_to_drop, errors='ignore', axis=1, inplace=True)

    final_csv_path = os.path.join(root_dir, "results.csv")
    sorted_merged_df.to_csv(final_csv_path, index=False)

    print(f"Final merged results saved to {final_csv_path}")

def process_summary_csv(rootResultsdir):
    df = pd.read_csv(os.path.join(rootResultsdir, "detailed_results.csv"))

    # Use subset of csv
    data_subset = df[['Test Case', 'wanLinkDelay', 'channelWidth', 'Log Rate Ratio', 'Latency Benefit']].copy()

    # Process the Test Case to determine the number of files, rows, and columns
    data_subset['LS'] = data_subset['Test Case'].apply(lambda x: int(x.split('-')[-1][2:])) # Number of CSV files
    data_subset['TS'] = data_subset['Test Case'].apply(lambda x: int(x.split('-')[2][2:])) # Number of rows
    data_subset['TC'] = data_subset['Test Case'].apply(lambda x: int(x.split('-')[1][2:])) # Number of columns

    # Filter out TC1
    valid_data_subset = data_subset[(data_subset['TC'] > 1) & data_subset['Log Rate Ratio'].notna() & data_subset['Latency Benefit'].notna()]

    max_ls = valid_data_subset['LS'].max()
    max_ts = valid_data_subset['TS'].max()

    output_data_valid = defaultdict(lambda: np.zeros((max_ts, max(valid_data_subset['TC']) - 1), dtype=object))

    row_labels_map = {1: "2ms base RTT", 2: "10ms base RTT", 3: "50ms base RTT"}
    column_labels_map = {2: "1v1", 3: "1+1", 4: "2+2", 5: "4+4"}

    for _, row in valid_data_subset.iterrows():
        output_index = row['LS']
        row_index = row['TS'] - 1
        col_index = row['TC'] - 2

        cell_content = f"{row['Log Rate Ratio']:+.1f} +\n{row['Latency Benefit']:.1f}ms"

        output_data_valid[(output_index, row['channelWidth'])][row_index, col_index] = cell_content

    # Generate CSV files
    output_files = []
    for (ls_index, channel_width), data_matrix in output_data_valid.items():
        # Create a DataFrame for the CSV output
        df_output = pd.DataFrame(data_matrix, columns=[column_labels_map[i+2] for i in range(data_matrix.shape[1])])
        df_output.index = [row_labels_map[i+1] for i in range(max_ts)]
        df_output.index.name = ""


        file_name = os.path.join(rootResultsdir, f"{channel_width}_MHz_Channel.csv")
        df_output.to_csv(file_name)

        output_files.append(Path(file_name))

        print(f"Summary csv saved to {file_name}")

    return output_files
