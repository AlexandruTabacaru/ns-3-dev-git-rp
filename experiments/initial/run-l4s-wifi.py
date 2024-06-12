#!/usr/bin/python3
import os
import shutil
import subprocess
import sys
from datetime import datetime

# Get the directory path of the current script
script_dir = os.path.abspath(os.path.dirname(__file__))
experiments_dir = os.path.dirname(script_dir)
ns3_dir = os.path.dirname(experiments_dir)
ns3_script = os.path.join(ns3_dir, "ns3")

# In future, add parametric job control here (multiprocessing.Pool)
numCubic = 1
numPrague = 1
numBytes = 0
numBackgroundUdp = 0
duration = 20
# wanLinkDelay is 1/2 of the desired base RTT
wanLinkDelay = "2500us"
mcs = 11
channelWidth=20
spatialStreams=2
# Default WifiMacQueue size is now 8000 packets, but can be changed below
wifiQueueSize = "8000p"
# If maxAmsduSize is zero, it will disable A-MSDU.  If non-zero, it will
# try to form A-MSDUs (BE access category) up to the number of bytes specified
maxAmsduSize = 0
# The following three variables are related; if the first is disabled,
# the second two will have no effect
flowControl = 1
limit = 100000
scale = 1
# Set rtsCtsThreshold to a low value such as 1000 (bytes) to enable RTS/CTS
# Zero disables the explicit setting of the WifiRemoteStationManager attribute
rtsCtsThreshold = 0
# Allow customization of AC_BE EDCA parameters (for all Wi-Fi devices)
cwMin = 15
cwMax = 1023
aifsn = 3
txopLimit = "2528us"
# The below is to output some simulation progress when running from command-line
# It is more for C++ invocation and not very useful for Python invocation
showProgress = 0
# enablePcapAll will generate pcap traces for all interfaces
enablePcapAll = 0
# enablePcap will generate pcap traces only for endpoints
enablePcap = 1
# enableTracesAll will generate all time-series traces
enableTracesAll = 0
# enableTraces will generate time-series traces needed by plot-l4s-wifi.py
enableTraces = 1
useReno = 0
# Maps to DualPi2QueueDisc::EnableWifiClassicLatencyEstimator 
enableWifiClassicLatencyEstimator=0

arguments = " --numCubic=" + str(numCubic)
arguments += " --numPrague=" + str(numPrague)
arguments += " --numBytes=" + str(numBytes)
arguments += " --numBackgroundUdp=" + str(numBackgroundUdp)
arguments += " --duration=" + str(duration)
arguments += " --wanLinkDelay=" + wanLinkDelay
arguments += " --mcs=" + str(mcs)
arguments += " --channelWidth=" + str(channelWidth)
arguments += " --spatialStreams=" + str(spatialStreams)
arguments += " --wifiQueueSize=" + wifiQueueSize
arguments += " --flowControl=" + str(flowControl)
arguments += " --limit=" + str(limit)
arguments += " --scale=" + str(scale)
arguments += " --rtsCtsThreshold=" + str(rtsCtsThreshold)
arguments += " --maxAmsduSize=" + str(maxAmsduSize)
arguments += " --cwMin=" + str(cwMin)
arguments += " --cwMax=" + str(cwMax)
arguments += " --aifsn=" + str(aifsn)
arguments += " --txopLimit=" + txopLimit
arguments += " --showProgress=" + str(showProgress)
arguments += " --enablePcapAll=" + str(enablePcapAll)
arguments += " --enablePcap=" + str(enablePcap)
arguments += " --enableTracesAll=" + str(enableTracesAll)
arguments += " --enableTraces=" + str(enableTraces)
arguments += " --useReno=" + str(useReno)
arguments += " --ns3::DualPi2QueueDisc::EnableWifiClassicLatencyEstimator=" + str(enableWifiClassicLatencyEstimator)

# Build a plot title; customize as needed
plotTitle = "Cubic=" + str(numCubic)
plotTitle += " Prague=" + str(numPrague)
plotTitle += " BackgroundUdp=" + str(numBackgroundUdp)
plotTitle += " Base RTT= 2*" + str(wanLinkDelay)
plotTitle += " Flow control=" + str(flowControl)

# Make a unique timestamped results directory
formatted_date = datetime.now().strftime("%Y%m%d-%H%M%S")
resultsdir = "results-" + formatted_date
os.makedirs(resultsdir, exist_ok=False)

# Copy the plotting file into the results directory
shutil.copy("plot-l4s-wifi.py", resultsdir)
# Copy the scenario program into the results directory
shutil.copy(ns3_dir + "/scratch/l4s-wifi.cc", resultsdir)
# Copy any other files here

# Cd to the results directory
os.chdir(resultsdir)

# Copy this script into the results directory
script_filename = os.path.basename(__file__)
with open(__file__, "r") as source:
    script_content = source.read()
with open(os.path.join(os.getcwd(), script_filename), "w") as destination_file:
    destination_file.write(script_content)

try:
    with open("build.txt", "w") as out:
        subprocess.run(
            [ns3_script, "build"], stdout=out, stderr=out, check=True
        )
except subprocess.CalledProcessError as e:
    print(f"Build error: {e}:  Check build.txt file for error.")
    sys.exit(e.returncode)

# Run l4s-wifi
with open("run.txt", "w") as out:
    result = subprocess.run(
        [
            ns3_script,
            "run",
            "--no-build",
            "--cwd",
            os.getcwd(),
            "l4s-wifi",
            "--",
        ]
        + arguments.split(),
        stdout=out,
        text=True,
    )

# Save the parameters used in a 'commandlog.txt' file
with open("commandlog.txt", "w") as out:
    out.write("ns3 run l4s-wifi" + " -- " + arguments + "\n")
    out.close()

# Unused for now-- for future reference
command_get_repo_time = "git rev-parse --abbrev-ref HEAD"
command_get_repo_commit = "git rev-parse --short HEAD"

branch_output = subprocess.check_output(["git", "branch", "-vvv"])
with open("version.txt", "w") as out:
    command_output = branch_output.decode("utf-8").strip()
    command_output.split("\n")[0:1]
    out.write(
        "# Branch name                    commit hash                   commit message\n"
    )
    out.write("".join(command_output.split("\n")[0:1]))
    out.close()

diff_output = subprocess.check_output(["git", "diff"])
with open("version.diff", "w") as out:
    command_output = diff_output.decode("utf-8").strip()
    out.write(command_output)
    out.write("\n")
    out.close()

# Make a plot
if enableTracesAll or enableTraces:
    subprocess.run(
        ["python3", "plot-l4s-wifi.py", plotTitle], stdout=subprocess.PIPE, text=True
    )
import subprocess

try:
    result = subprocess.run(
        [os.path.join(experiments_dir,"latency-monitor","multiflow_ns3.sh"), "l4s-wifi-2-0-ip.pcap", "l4s-wifi-0-0.pcap"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    # Check if the command resulted in an error
    if result.returncode != 0:
        print(f"Error: {result.stderr}")
    else:
        print(f"Output: {result.stdout}")
except Exception as e:
    print(f"An exception occurred: {e}")

# Report to terminal
with open("run.txt", "r") as runfile:
    for line in runfile:
        print(line.strip())
runfile.close()

sys.exit()
