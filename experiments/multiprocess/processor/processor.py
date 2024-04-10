import os
import pandas as pd
import numpy as np
import re
from collections import defaultdict
from pathlib import Path
import json

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

def process_results(root_dir):
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
                stats_cubic = compute_statistics(data["cubic"][direction],digits=0)
            else:
                stats_cubic = default_stats
            bandwidth_cubic = round(data["cubic"][direction]["Bandwidth (Mbps)"].mean()) if "Bandwidth (Mbps)" in data["cubic"][direction].columns else 0
            ce_cubic = data["cubic"][direction]["CE %"].mean() if "CE %" in data["cubic"][direction].columns else 0

            if not data["prague"][direction].empty and "Latency" in data["prague"][direction].columns:
                stats_prague = compute_statistics(data["prague"][direction],digits=0)
            else:
                stats_prague = default_stats
            bandwidth_prague = round(data["prague"][direction]["Bandwidth (Mbps)"].mean()) if "Bandwidth (Mbps)" in data["prague"][direction].columns else 0
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
    
    num_digits_lrr=1
    num_digits_lat_ben=0
    
    df = pd.read_csv(os.path.join(root_dir, "results.csv"))

    # Extract TS# values from the Test Case column
    df['TS#'] = df['Test Case'].str.extract(r'TS(\d+)').astype(int)

    # Compute the max_ts value
    max_ts = df['TS#'].max()

    df['Log Rate Ratio'] = np.nan
    df['Latency Benefit'] = np.nan

    # Initialize additional columns for tracking used values
    df['calc_ABW_DL_Prague_Mbps'] = np.nan
    df['calc_ABW_DL_Cubic_Mbps'] = np.nan
    df['calc_P99_Latency_DL_Prague'] = np.nan
    df['calc_P99_Latency_DL_Cubic'] = np.nan

    for index, row in df.iterrows():
        test_case = row['Test Case']
        tc_num = int(test_case.split('-')[3][2:])  # Extract TC# as integer

        if tc_num == 1:
            continue

        # For TC2 and above, compute values
        if tc_num >= 2:
            if tc_num == 2:
                # For TC2, use different rows for Prague and Cubic values
                target_index = index - max_ts
                if target_index >= 0 and target_index < len(df):
                    cubic_value = df.iloc[target_index]['Average Bandwidth DL Cubic (Mbps)']
                    prague_value = row['Average Bandwidth DL Prague (Mbps)']
                    if cubic_value > 0:  # Prevent division by zero
                        df.at[index, 'Log Rate Ratio'] = round(np.log10(prague_value / cubic_value), 3)
                        df.at[index, 'calc_ABW_DL_Prague_Mbps'] = prague_value
                        df.at[index, 'calc_ABW_DL_Cubic_Mbps'] = cubic_value
                    else:
                        df.at[index, 'Log Rate Ratio'] = np.nan

                    df.at[index, 'Latency Benefit'] = round(df.iloc[target_index]['P99 Latency DL Cubic'] - row['P99 Latency DL Prague'], 3)
                    df.at[index, 'calc_P99_Latency_DL_Prague'] = row['P99 Latency DL Prague']
                    df.at[index, 'calc_P99_Latency_DL_Cubic'] = df.iloc[target_index]['P99 Latency DL Cubic']
            else:
                # For TC3 and above, use same row values
                cubic_value = row['Average Bandwidth DL Cubic (Mbps)']
                prague_value = row['Average Bandwidth DL Prague (Mbps)']
                if cubic_value > 0:  # Prevent division by zero
                    df.at[index, 'Log Rate Ratio'] = round(np.log10(prague_value / cubic_value), 3)
                    df.at[index, 'calc_ABW_DL_Prague_Mbps'] = prague_value
                    df.at[index, 'calc_ABW_DL_Cubic_Mbps'] = cubic_value
                else:
                    df.at[index, 'Log Rate Ratio'] = np.nan

                df.at[index, 'Latency Benefit'] = round(row['P99 Latency DL Cubic'] - row['P99 Latency DL Prague'], 3)
                df.at[index, 'calc_P99_Latency_DL_Prague'] = row['P99 Latency DL Prague']
                df.at[index, 'calc_P99_Latency_DL_Cubic'] = row['P99 Latency DL Cubic']

    # Temporary detailed results with calculation information used.
    # Used by process_summmary_csv
    calc_csv_path = os.path.join(root_dir, "calc_detailed_results.csv")
    df.to_csv(calc_csv_path, index=False)
    print(f"Intermediary calculated metrics saved to {calc_csv_path}")

    # Specify columns intended to be dropped
    intended_columns_to_drop = ['Test Case Match', 'MS', 'AP', 'TC', 'TS', 'LS', 'TS#',
                                'calc_ABW_DL_Prague_Mbps', 'calc_ABW_DL_Cubic_Mbps', 
                                'calc_P99_Latency_DL_Prague', 'calc_P99_Latency_DL_Cubic'] + hidden_columns
    columns_to_drop = [col for col in intended_columns_to_drop if col in df.columns]

    # Drop the specified columns from the DataFrame
    df.drop(columns=columns_to_drop, inplace=True)

    # Save the cleaned DataFrame to "detailed_results.csv"
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

    # Extract MS#, AP#, LS#, TC#, TS# for sorting
    merged_df[['MS', 'AP', 'LS', 'TC', 'TS']] = merged_df['Test Case'].str.extract(r'MS(\d+)-AP(\d+)-LS(\d+)-TC(\d+)-TS(\d+)')
    merged_df[['MS', 'AP', 'LS', 'TC', 'TS']] = merged_df[['MS', 'AP', 'LS', 'TC', 'TS']].astype(int)

    # Generate the 'link' column
    merged_df['link'] = merged_df['Label'].apply(lambda label: f"link:./{os.path.join(label)}[{label}]/")
    merged_df['Label'] = merged_df['link']

    # Sort by MS#, AP#, LS#, TC#, TS#
    sorted_merged_df = merged_df.sort_values(by=['MS', 'AP', 'LS', 'TC', 'TS'])

    # Reorder columns to place 'Label', 'Test Case', input_df columns, then the rest
    input_df_columns = [col for col in input_df.columns if col != 'Test Case']  # Avoid duplicating 'Test Case'
    final_columns = ['Label', 'Test Case'] + input_df_columns + \
                    [col for col in sorted_merged_df.columns if col not in ['Label', 'Test Case'] + input_df_columns + ['MS', 'AP', 'TC', 'TS', 'LS', 'Test Case Match']]

    # Add 'link' as the last column
    final_columns = [col for col in final_columns if col != 'link'] + ['link']

    sorted_merged_df = sorted_merged_df[final_columns]

    # Drop temporary columns
    # columns_to_drop = ['Test Case Match', 'MS', 'AP', 'TC', 'TS', 'LS']
    # sorted_merged_df.drop(columns_to_drop, errors='ignore', axis=1, inplace=True)

    final_csv_path = os.path.join(root_dir, "results.csv")
    sorted_merged_df.to_csv(final_csv_path, index=False)

    print(f"Final merged results saved to {final_csv_path}")

def process_summary_csv(rootResultsdir):
    # Load the configuration from campaigns.json
    script_dir = os.path.dirname(__file__)
    with open(os.path.join(script_dir, 'campaigns.json'), 'r') as config_file:
        campaigns_config = json.load(config_file)

    df = pd.read_csv(os.path.join(rootResultsdir, "calc_detailed_results.csv"))

    # Use subset of csv
    data_subset = df[['Test Case', 'wanLinkDelay', 'channelWidth', 'Log Rate Ratio', 'Latency Benefit',
                      'calc_ABW_DL_Prague_Mbps', 'calc_ABW_DL_Cubic_Mbps', 
                      'calc_P99_Latency_DL_Prague', 'calc_P99_Latency_DL_Cubic']].copy()

    # Process the Test Case to determine the number of files, rows, and columns
    # Test Case format: MS#-AP#-LS#-TC#-TS#
    data_subset['MS'] = data_subset['Test Case'].apply(lambda x: int(x.split('-')[0][2:]))  # Number of Campaigns
    data_subset['LS'] = data_subset['Test Case'].apply(lambda x: int(x.split('-')[2][2:]))  # Number of CSV files
    data_subset['TC'] = data_subset['Test Case'].apply(lambda x: int(x.split('-')[3][2:]))  # Number of rows
    data_subset['TS'] = data_subset['Test Case'].apply(lambda x: int(x.split('-')[4][2:]))  # Number of columns

    # Filter out TC1
    valid_data_subset = data_subset[(data_subset['TC'] > 1) & data_subset['Log Rate Ratio'].notna() & data_subset['Latency Benefit'].notna()]

    max_ls = valid_data_subset['LS'].max()
    max_ts = valid_data_subset['TS'].max()

    output_data_valid = defaultdict(lambda: np.zeros((max_ts, max(valid_data_subset['TC']) - 1), dtype=object))
    output_data_extended = defaultdict(lambda: np.zeros((max_ts, max(valid_data_subset['TC']) - 1), dtype=object))

    row_labels_map = {1: "2ms base RTT", 2: "10ms base RTT", 3: "50ms base RTT"}
    column_labels_map = {2: "1v1", 3: "1+1", 4: "2+2", 5: "4+4"}

    for _, row in valid_data_subset.iterrows():
        ms_index, ls_index, row_index, col_index = row['MS'], row['LS'], row['TS'] - 1, row['TC'] - 2

        # Original cell content
        cell_content = f"{row['Log Rate Ratio']:+.1f} +\n{row['Latency Benefit']:.0f}ms"
        output_data_valid[(ms_index, ls_index, row['channelWidth'])][row_index, col_index] = cell_content

        # Extended cell content
        extended_cell_content = (f"{row['Log Rate Ratio']:+.1f} "
                         f"[a: {row['calc_ABW_DL_Prague_Mbps']:.0f}M, b: {row['calc_ABW_DL_Cubic_Mbps']:.0f}M] +\n"
                         f"{row['Latency Benefit']:.0f}ms "
                         f"[a: {row['calc_P99_Latency_DL_Prague']:.0f}ms, b: {row['calc_P99_Latency_DL_Cubic']:.0f}ms]")
        output_data_extended[(ms_index, ls_index, row['channelWidth'])][row_index, col_index] = extended_cell_content

    # Generate original and extended CSV files
    output_files = []
    for (ms_index, ls_index, channel_width), data_matrix in output_data_valid.items():
        ms_prefix = campaigns_config["MS"][str(ms_index)]  # Use the MS prefix from the config
        ls_prefix = campaigns_config["LS"][str(ls_index)]  # Use the LS prefix from the config

        # Construct the file name dynamically based on MS and LS
        file_name_base = f"{ms_prefix}_" if ms_prefix else ""
        file_name = f"{file_name_base}{ls_prefix}_Channel_orig.csv".replace(" ", "_")
        extended_file_name = f"{file_name_base}{ls_prefix}_Channel.csv".replace(" ", "_")

 
        full_path = os.path.join(rootResultsdir, file_name)
        extended_full_path = os.path.join(rootResultsdir, extended_file_name)

        # Save original summary CSV
        df_output = pd.DataFrame(data_matrix, columns=[column_labels_map[i + 2] for i in range(data_matrix.shape[1])])
        df_output.index = [row_labels_map[i + 1] for i in range(max_ts)]
        df_output.index.name = ""
        df_output.to_csv(full_path)
        # GWhite temporarily commenting this out
        #output_files.append(Path(full_path))

        # Save extended summary CSV
        extended_matrix = output_data_extended[(ms_index, ls_index, channel_width)]
        df_extended_output = pd.DataFrame(extended_matrix, columns=[column_labels_map[i + 2] for i in range(extended_matrix.shape[1])])
        df_extended_output.index = [row_labels_map[i + 1] for i in range(max_ts)]
        df_extended_output.index.name = ""
        df_extended_output.to_csv(extended_full_path)
        output_files.append(Path(extended_full_path))

        print(f"Summary csv saved to {full_path}")
        print(f"Extended summary csv saved to {extended_full_path}")

    return output_files

