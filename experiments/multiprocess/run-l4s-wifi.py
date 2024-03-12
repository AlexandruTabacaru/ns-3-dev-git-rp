#!/usr/bin/python3
import os
import shutil
import subprocess
import sys
from datetime import datetime
import pandas as pd
import numpy as np
import re
import multiprocessing


def buildPlotTitle(numCubic, numPrague, numBackground):
    # Build a plot title; customize as needed
    plotTitle = "Cubic=" + str(numCubic)
    plotTitle += " Prague=" + str(numPrague)
    plotTitle += " Background=" + str(numBackground)

    return plotTitle


def stageResultsDirectory(rootResultsdir, label):
    path_to_ns3_dir = "../../"
    path_to_ns3_script = "../../ns3"

    # Create a unique timestamped results directory
    formatted_date = datetime.now().strftime("%Y%m%d-%H%M%S")
    resultsdir = os.path.join(rootResultsdir, label + "_" + formatted_date)
    os.makedirs(resultsdir, exist_ok=False)

    # Copy the plotting file into the results directory
    shutil.copy("plot-l4s-wifi.py", resultsdir)
    # Copy the scenario program into the results directory
    shutil.copy(path_to_ns3_dir + "scratch/l4s-wifi.cc", resultsdir)

    # Copy this script into the results directory
    script_filename = os.path.basename(__file__)
    destination_filepath = os.path.join(resultsdir, script_filename)
    shutil.copy(script_filename, destination_filepath)

    return resultsdir

def runNS3Build(build_filepath):
    path_to_ns3_dir = "../../"
    path_to_ns3_script = "../../ns3"

    try:
        with open(build_filepath, "w") as out:
            subprocess.run(
                [path_to_ns3_script, "configure", "-d", "optimized"], stdout=out, stderr=out, check=True
            )
            subprocess.run(
                [path_to_ns3_script, "build"], stdout=out, stderr=out, check=True
            )
    except subprocess.CalledProcessError as e:
        print(f"Build error: {e}:  Check build.txt file for error.")
        sys.exit(e.returncode)


def runNS3Simulation(run_filepath, arguments, plotTitle):
    path_to_ns3_dir = "../../"
    path_to_ns3_script = "../../ns3"
    # Run l4s-wifi
    resultsDir = os.path.dirname(run_filepath)
    with open(run_filepath, "w") as out:
        result = subprocess.run(
            [
                "time",
                path_to_ns3_script,
                "run",
                "--no-build",
                "--cwd",
                resultsDir,
                "l4s-wifi",
                "--",
            ]
            + arguments.split(),
            stdout=out,
            stderr=subprocess.STDOUT,
            text=True,
        )
    # Save the parameters used in a 'commandlog.txt' file
    with open(resultsDir + "/" + "commandlog.txt", "w") as out:
        out.write("ns3 run l4s-wifi" + " -- " + arguments + "\n")
        out.close()

    # Unused for now-- for future reference
    command_get_repo_time = "git rev-parse --abbrev-ref HEAD"
    command_get_repo_commit = "git rev-parse --short HEAD"

    branch_output = subprocess.check_output(["git", "branch", "-vvv"])
    with open(resultsDir + "/" + "version.txt", "w") as out:
        command_output = branch_output.decode("utf-8").strip()
        command_output.split("\n")[0:1]
        out.write(
            "# Branch name                    commit hash                   commit message\n"
        )
        out.write("".join(command_output.split("\n")[0:1]))
        out.close()

    diff_output = subprocess.check_output(["git", "diff"])
    with open(resultsDir + "/" + "version.diff", "w") as out:
        command_output = diff_output.decode("utf-8").strip()
        out.write(command_output)
        out.write("\n")
        out.close()
    #
    # Make a plot
    subprocess.run(
        ["python3", "plot-l4s-wifi.py", plotTitle, resultsDir],
        stdout=subprocess.PIPE,
        text=True,
    )
    try:
        subprocess.run(
            [
                "/var/www/html/flaskapp/multiflow_ns3.sh",
                resultsDir + "/" + "l4s-wifi-2-0-ip.pcap",
                resultsDir + "/" + "l4s-wifi-0-0.pcap",
                resultsDir + "/" + "Test",
            ],
            stdout=subprocess.PIPE,
            text=True,
        )
    except:
        pass


    # Clean up raw data files
    try:
        subprocess.run(
            [
                "rm",
                resultsDir + "/" + "l4s-wifi-0-0.pcap",  
                resultsDir + "/" + "l4s-wifi-1-0.pcap",  
                resultsDir + "/" + "l4s-wifi-1-1.pcap",  
                resultsDir + "/" + "l4s-wifi-2-0-ip.pcap",  
                resultsDir + "/" + "l4s-wifi-2-0.pcap",
                resultsDir + "/" + "cubic-cong-state.dat",   
                resultsDir + "/" + "cubic-rtt.dat",            
                resultsDir + "/" + "cubic-throughput.dat",        
                resultsDir + "/" + "wifi-dualpi2-l-sojourn.dat",  
                resultsDir + "/" + "wifi-throughput.dat",
                resultsDir + "/" + "cubic-cwnd.dat",         
                resultsDir + "/" + "cubic-send-interval.dat",  
                resultsDir + "/" + "wifi-dualpi2-bytes.dat",      
                resultsDir + "/" + "wifi-phy-tx-psdu-begin.dat",
                resultsDir + "/" + "cubic-pacing-rate.dat",  
                resultsDir + "/" + "cubic-ssthresh.dat",       
                resultsDir + "/" + "wifi-dualpi2-c-sojourn.dat",  
                resultsDir + "/" + "wifi-queue-bytes.dat",
                resultsDir + "/" + "prague-cong-state.dat",  
                resultsDir + "/" + "prague-ecn-state.dat",    
                resultsDir + "/" + "prague-rtt.dat",            
                resultsDir + "/" + "prague-ssthresh.dat",
                resultsDir + "/" + "prague-cwnd.dat",        
                resultsDir + "/" + "prague-pacing-rate.dat",  
                resultsDir + "/" + "prague-send-interval.dat",  
                resultsDir + "/" + "prague-throughput.dat",
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,

        )
    except: 
        pass


def parse_csv_to_dataframe(file_name):
    try:
        df = pd.read_csv(file_name)

        result_dict = {}

        for index, row in df.iterrows():
            cmd_args = []

            for col in df.columns:
                if col != "Test Case":
                    value = str(row[col]).strip("\"'")
                    cmd_args.append(f"--{col}={value}")

            result_dict[row["Test Case"]] = " ".join(cmd_args)

        return result_dict

    except Exception as e:
        print(f"An error occurred while reading the file: {e}")
        return None


def compute_statistics(dataframe):
    if not dataframe.empty:
        return {
            "Average Latency": np.mean(dataframe["Latency"]) * 1000,
            "P10 Latency": np.percentile(dataframe["Latency"], 10) * 1000,
            "P90 Latency": np.percentile(dataframe["Latency"], 90) * 1000,
            "P99 Latency": np.percentile(dataframe["Latency"], 99) * 1000,
            "StdDev": np.std(dataframe["Latency"], ddof=1) * 1000,
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
        test_dir_path = os.path.join(root_dir, test_dir, "Test")
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

    # Remove the temp column after merging
    merged_df.drop("Test Case Match", axis=1, inplace=True)

    input_columns = [col for col in input_df.columns if col != "Test Case"]
    new_column_order = (
        ["Label"]
        + input_columns
        + [
            col
            for col in merged_df.columns
            if col not in input_columns + ["Label", "Test Case Match"]
        ]
    )
    merged_df = merged_df[new_column_order]

    final_csv_path = os.path.join(root_dir, "final_results.csv")
    merged_df.to_csv(final_csv_path, index=False)

    print(f"Final merged results saved to {final_csv_path}")

def run_simulation(test_case, arguments):
    path_to_ns3_dir = "../../"
    path_to_ns3_script = "../../ns3"

    plotTitle = f"Simulation {test_case}"


    print(f"Test Case: {test_case}, Arguments: {arguments}")
    resultsDir = stageResultsDirectory(rootResultsdir, test_case)
    runNS3Simulation(os.path.join(resultsDir, "run.txt"), arguments, plotTitle)

if __name__ == "__main__":
    # Create root multiprocessing directory
    formatted_date = datetime.now().strftime("%Y%m%d-%H%M%S")
    rootResultsdir = "multiresults" + "-" + formatted_date
    os.makedirs(rootResultsdir, exist_ok=False)

    build_filepath = os.path.join(rootResultsdir, "build.txt")
    runNS3Build(build_filepath)

    configFile = "config.csv"
    testCases = parse_csv_to_dataframe(configFile)

    pool_args = [(tc, args) for tc, args in testCases.items()]

    with multiprocessing.Pool(processes=multiprocessing.cpu_count()) as pool:
        pool.starmap(run_simulation, pool_args)

    # rootResultsdir = "multiresults-20240303-214151"
    processResults(rootResultsdir)
    merge_input_with_results(rootResultsdir)
    sys.exit()
