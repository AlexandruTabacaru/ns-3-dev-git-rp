#!/usr/bin/python3
import os
import shutil
import subprocess
import sys
from datetime import datetime
import pandas as pd
import numpy as np
import re


def buildArguments(
    numCubic,
    numPrague,
    numBackground,
    numBytes,
    duration,
    wanLinkDelay,
    mcs,
    channelWidth,
    spatialStreams,
    flowControl,
    limit,
    scale,
):
    arguments = " --numCubic=" + str(numCubic)
    arguments += " --numPrague=" + str(numPrague)
    arguments += " --numBackground=" + str(numBackground)
    arguments += " --numBytes=" + str(numBytes)
    arguments += " --duration=" + str(duration)
    arguments += " --wanLinkDelay=" + wanLinkDelay
    arguments += " --mcs=" + str(mcs)
    arguments += " --channelWidth=" + str(channelWidth)
    arguments += " --spatialStreams=" + str(spatialStreams)
    arguments += " --flowControl=" + str(flowControl)
    arguments += " --limit=" + str(limit)
    arguments += " --scale=" + str(scale)
    arguments += " --rtsCtsThreshold=" + str(rtsCtsThreshold)
    arguments += " --showProgress=" + str(showProgress)
    arguments += " --useReno=" + str(useReno)

    return arguments


def buildPlotTitle(numCubic, numPrague, numBackground):
    # Build a plot title; customize as needed
    plotTitle = "Cubic=" + str(numCubic)
    plotTitle += " Prague=" + str(numPrague)
    plotTitle += " Background=" + str(numBackground)

    return plotTitle


def stageResultsDirectory(rootResultsdir, label):
    # Create a unique timestamped results directory
    formatted_date = datetime.now().strftime("%Y%m%d-%H%M%S")
    resultsdir = os.path.join(rootResultsdir, label + "-" + formatted_date)
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
    try:
        with open(build_filepath, "w") as out:
            subprocess.run(
                [path_to_ns3_script, "build"], stdout=out, stderr=out, check=True
            )
    except subprocess.CalledProcessError as e:
        print(f"Build error: {e}:  Check build.txt file for error.")
        sys.exit(e.returncode)


def runNS3Simulation(run_filepath, arguments):
    # Run l4s-wifi
    resultsDir = os.path.dirname(run_filepath)
    with open(run_filepath, "w") as out:
        result = subprocess.run(
            [
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


def parse_csv_to_dataframe(file_name):
    try:
        df = pd.read_csv(file_name)

        result_dict = {}

        for index, row in df.iterrows():
            cmd_args = []

            for col in df.columns:
                if col != "Test Case":
                    # Convert the value to string and strip quotes
                    value = str(row[col]).strip("\"'")
                    # Add each field and its value to the command string
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
        port_number = int(
            port_match.group(1)
        )  # The first group captures the port number
        if 100 <= port_number <= 199:
            return "prague"
        elif 200 <= port_number <= 299:
            return "cubic"
    return None


def process_test_directory(test_dir_path):
    # Initialize dictionaries to hold DataFrames for each category and direction
    data = {
        "cubic": {"UL": pd.DataFrame(), "DL": pd.DataFrame()},
        "prague": {"UL": pd.DataFrame(), "DL": pd.DataFrame()},
    }

    for file in os.listdir(test_dir_path):
        if file.endswith("latency.csv"):
            direction = "UL" if "192.168.1.2" in file.split(" to ")[0] else "DL"
            category = is_cubic_or_prague(file)
            if category:
                df = pd.read_csv(os.path.join(test_dir_path, file))
                # Calculate bandwidth
                bandwidth = (df["Frame Length"].sum() / df["Timestamp"].max()) / 1000000
                df["Bandwidth"] = bandwidth
                if not data[category][direction].empty:
                    data[category][direction] = pd.concat(
                        [data[category][direction], df], ignore_index=True
                    )
                else:
                    data[category][direction] = df

    return data


def merge_input_with_results(root_dir):
    input_df = pd.read_csv("config.csv")  # Contains "Test Case"
    results_df = pd.read_csv(os.path.join(root_dir, "results_combined.csv"))

    # Test Case should match first part of the Label
    results_df["Test Case Match"] = results_df["Label"].apply(lambda x: x.split("-")[0])

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

    final_csv_path = os.path.join(root_dir, "merged_results.csv")
    merged_df.to_csv(final_csv_path, index=False)

    print(f"Final merged results saved to {final_csv_path}")


def processResults(root_dir):
    all_results = []

    test_run_dirs = [
        d
        for d in os.listdir(root_dir)
        if os.path.isdir(os.path.join(root_dir, d)) and d.startswith("TC")
    ]

    for test_dir in test_run_dirs:
        test_dir_path = os.path.join(root_dir, test_dir, "Test")
        data = process_test_directory(test_dir_path)

        # Initialize a template for the results dictionary
        test_results_template = {"Label": test_dir}

        # Collect and combine statistics for both categories side by side for each direction
        for direction in ["UL", "DL"]:
            # Compute statistics for both categories
            stats_cubic = compute_statistics(data["cubic"][direction])
            bandwidth_cubic = (
                data["cubic"][direction]["Bandwidth"].mean()
                if not data["cubic"][direction].empty
                else 0
            )
            stats_prague = compute_statistics(data["prague"][direction])
            bandwidth_prague = (
                data["prague"][direction]["Bandwidth"].mean()
                if not data["prague"][direction].empty
                else 0
            )

            # Merge stats into the results template, prefixing keys with the category
            if stats_cubic and stats_prague:
                for key in stats_cubic:
                    test_results_template[f"{key} {direction} Cubic"] = stats_cubic[key]
                test_results_template[
                    f"Average Bandwidth {direction} Cubic"
                ] = bandwidth_cubic

                for key in stats_prague:
                    test_results_template[f"{key} {direction} Prague"] = stats_prague[
                        key
                    ]
                test_results_template[
                    f"Average Bandwidth {direction} Prague"
                ] = bandwidth_prague

        all_results.append(test_results_template)

    df_results = pd.DataFrame(all_results)
    df_results.to_csv(os.path.join(root_dir, "results_combined.csv"), index=False)

    print(
        f"Combined results with bandwidth saved to {os.path.join(root_dir, 'results_combined.csv')}"
    )


if __name__ == "__main__":
    # If not executing this in an experiments subdirectory, need to change this
    # The assumed starting directory is 'experiments/<name>'
    path_to_ns3_dir = "../../"
    # The assumed execution directory is 'experiments/<name>/results-YYmmdd--HHMMSS'
    path_to_ns3_script = "../../ns3"

    # # In future, add parametric job control here (multiprocessing.Pool)
    # numCubic = 1
    # numPrague = 1
    # numBackground = 0
    # numBytes = 100000000
    # duration = 30
    # # wanLinkDelay is 1/2 of the desired base RTT
    # wanLinkDelay = "10ms"
    # mcs = 2
    # channelWidth = 80
    # spatialStreams = 1
    # flowControl = 1
    # limit = 65535
    # scale = 1
    #
    # # Set rtsCtsThreshold to a low value such as 1000 (bytes) to enable RTS/CTS
    # # Zero disables the explicit setting of the WifiRemoteStationManager attribute
    # rtsCtsThreshold = 0
    # showProgress = 0
    # useReno = 0

    configFile = "config.csv"
    testCases = parse_csv_to_dataframe(configFile)

    # Create root multiprocessing directory
    formatted_date = datetime.now().strftime("%Y%m%d-%H%M%S")
    rootResultsdir = "multiresults" + "-" + formatted_date
    os.makedirs(rootResultsdir, exist_ok=False)

    for tc, arguments in testCases.items():
        print(f"Test Case: {tc}, Arguments: {arguments}")
        # plotTitle = buildPlotTitle(numCubic, numPrague, numBackground)

        plotTitle = tc
        resultsDir = stageResultsDirectory(rootResultsdir, tc)
        runNS3Build(os.path.join(resultsDir, "build.txt"))
        runNS3Simulation(os.path.join(resultsDir, "run.txt"), arguments)

    processResults(rootResultsdir)
    merge_input_with_results(rootResultsdir)
    sys.exit()
