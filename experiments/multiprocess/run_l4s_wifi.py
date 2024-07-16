#!/usr/bin/python3
import os, glob
import shutil
import subprocess
import sys
import pandas as pd
from datetime import datetime
import multiprocessing
from processor.processor import process_results, merge_input_with_results, create_summary_csvs, hide_columns
from exporter.exporter import export
from pathlib import Path


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
        result = subprocess.run(
            [
                "../latency-monitor/multiflow_ns3.sh",
                resultsDir + "/" + "l4s-wifi-2-0-ip.pcap",
                resultsDir + "/" + "l4s-wifi-0-0.pcap",
                resultsDir,
            ],
            capture_output=True,
            text=True
        )
        # Log the stdout and stderr
        with open(resultsDir + "/" + "latencymonitor-log.txt", "w") as out:
            out.write(result.stdout)
            out.write(result.stderr)
            out.close()
    except Exception as e:
        with open(resultsDir + "/" + "latencymonitor-log.txt", "w") as out:
            out.write(f"An exception occurred: {e}")
            out.close()

    # Clean up raw data files

    files_to_delete = glob.glob(os.path.join(resultsDir, "*.pcap"))
    for file_path in files_to_delete:
        os.remove(file_path)

    files_to_delete = glob.glob(os.path.join(resultsDir, "*.dat"))
    for file_path in files_to_delete:
        os.remove(file_path)


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

def run_simulation(test_case, arguments, rootResultsdir, rng_run_val):
    path_to_ns3_dir = "../../"
    path_to_ns3_script = "../../ns3"

    # Maps to DualPi2QueueDisc::EnableWifiClassicLatencyEstimator 
    enableWifiClassicLatencyEstimator=1

    plotTitle = f"Simulation {test_case}"
    arguments += " --ns3::DualPi2QueueDisc::EnableWifiClassicLatencyEstimator=" + str(enableWifiClassicLatencyEstimator)
    arguments += " --rngRun=" + str(rng_run_val)
    
    print(f"Test Case: {test_case}, Arguments: {arguments}")
    resultsDir = stageResultsDirectory(rootResultsdir, test_case)
    runNS3Simulation(os.path.join(resultsDir, "run.txt"), arguments, plotTitle)

def run_campaign(rootResultsdir, rng_run_val):

    # Record the start time
    start_time = datetime.now()

    # Copy all of the configuration files into the results directory
    shutil.copytree("config", os.path.join(rootResultsdir, "config"))

    configFile = os.path.join(rootResultsdir, "config", "config.csv")
    testCases = parse_csv_to_dataframe(configFile)

    pool_args = [(tc, args, rootResultsdir, rng_run_val) for tc, args in testCases.items()]

    with multiprocessing.Pool(processes=multiprocessing.cpu_count()) as pool:
        pool.starmap(run_simulation, pool_args)

    # rootResultsdir = "multiresults-20240520-173429"
    process_results(rootResultsdir) # produces processed_results.csv

    merge_input_with_results(rootResultsdir) # produces results.csv

    hidden_columns = ['numBytes', 'wifiQueueSize', 'limit','Duration','spatialStreams',
        'Mean Lat. UL Cubic','P0 Lat. UL Cubic','P10 Lat. UL Cubic',
        'P90 Lat. UL Cubic','P99 Lat. UL Cubic',
        'StdDev Lat. UL Cubic', 'Mean Rate UL Cubic (Mbps)', 'CE % UL Cubic',
        'Mean Lat. UL Prague','P0 Lat. UL Prague','P10 Lat. UL Prague',
        'P90 Lat. UL Prague','P99 Lat. UL Prague',
        'StdDev Lat. UL Prague', 'Mean Rate UL Prague (Mbps)', 'CE % UL Prague',
        'StdDev Lat. DL Cubic', 'StdDev Lat. DL Prague',
        ]

    detailed_csv = hide_columns(rootResultsdir, hidden_columns) # produces detailed_results.csv

    summary_csvs = create_summary_csvs(rootResultsdir) # produces calc_detailed_results.csv and multiple csv files

    # Suppress results tables for DualPI disabled for now
    # summary_csvs = [csv for csv in summary_csvs if "noDualPI" not in str(csv)]

    # Export html files
    export(detailed_csv, summary_csvs, Path(rootResultsdir + "/config/intro.md"), Path("./exporter/dct_project"), rootResultsdir)

    # delete intermediate data csvs
    files_to_delete = glob.glob(os.path.join(rootResultsdir, "*", "TC*.csv"))
    for file_path in files_to_delete:
        os.remove(file_path)

    # Calculate the elapsed time
    end_time = datetime.now()
    elapsed_time = end_time - start_time
    log_filename = os.path.join(rootResultsdir, "logfile.txt")
    with open(log_filename, 'a') as log_file:
        log_file.write(f"Execution time: {elapsed_time.total_seconds()/60} minutes\n")

    return detailed_csv

if __name__ == "__main__":

    # allow a results directory to be specified on the command line, otherwise use the current directory
    if len(sys.argv) < 2:
        baseDir="."
    else:
        baseDir=sys.argv[1]    


    # Create root multiprocessing directory
    formatted_date = datetime.now().strftime("%Y%m%d-%H%M%S")
    rootResultsdir = os.path.join(baseDir,"multiresults" + "-" + formatted_date)
    os.makedirs(rootResultsdir, exist_ok=False)

    build_filepath = os.path.join(rootResultsdir, "build.txt")
    runNS3Build(build_filepath)

    run_campaign(rootResultsdir,1)
    sys.exit()
