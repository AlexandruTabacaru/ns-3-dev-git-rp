#!/usr/bin/python3
import os
import shutil
import subprocess
import sys
from datetime import datetime

# If not executing this in an experiments subdirectory, need to change this
# The assumed starting directory is 'experiments/<name>'
path_to_ns3_dir = "../../"
# The assumed execution directory is 'experiments/<name>/results-YYmmdd--HHMMSS'
path_to_ns3_script = "../../../ns3"

# Make a unique timestamped results directory
formatted_date = datetime.now().strftime("%Y%m%d-%H%M%S")
resultsdir = "results-" + formatted_date
os.makedirs(resultsdir, exist_ok=False)

# Copy the plotting file into the results directory
shutil.copy("plot-l4s-loss-tests.py", resultsdir)
# Copy the scenario program into the results directory
shutil.copy(path_to_ns3_dir + "scratch/l4s-wired.cc", resultsdir)
# Copy any other files here

# Cd to the results directory
os.chdir(resultsdir)

# Copy this script into the results directory
script_filename = os.path.basename(__file__)
with open(__file__, "r") as source:
    script_content = source.read()
with open(os.path.join(os.getcwd(), script_filename), "w") as destination_file:
    destination_file.write(script_content)

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

# Unused for now-- for future reference
command_get_repo_time = "git rev-parse --abbrev-ref HEAD"
command_get_repo_commit = "git rev-parse --short HEAD"

### Define common parameters

wanLinkDelay = "10ms"
bottleneckRate = "100Mbps"
# Convert wanLinkDelay to an RTT value
numeric = '0123456789.'
for i,c in enumerate(wanLinkDelay):
    if c not in numeric:
        break
rttValue =  str(int(float(wanLinkDelay[:i]) * 2))
unit = wanLinkDelay[i:].lstrip()

### Define tests below

# TestName should be a string with no spaces
testName = "no-loss-cubic"
numCubic = 1
numPrague = 0 
numBytes = 0
duration = 10
# Base RTT is 2 * wanLinDelay
useReno = 0

arguments = " --numCubic=" + str(numCubic)
arguments += " --numPrague=" + str(numPrague)
arguments += " --numBytes=" + str(numBytes)
arguments += " --duration=" + str(duration)
arguments += " --wanLinkDelay=" + wanLinkDelay
arguments += " --bottleneckRate=" + bottleneckRate
arguments += " --useReno=" + str(useReno)
if 'testName' in vars():
    if testName != "":
        arguments += " --testName=" + testName

# Build a plot title; customize as needed
plotTitle = "No loss, Cubic,"
plotTitle += " " + rttValue + " " + unit + " base RTT"

try:
    with open("build.txt", "a") as out:
        subprocess.run(
            [path_to_ns3_script, "build"], stdout=out, stderr=out, check=True
        )
except subprocess.CalledProcessError as e:
    print(f"Build error: {e}:  Check build.txt file for error.")
    sys.exit(e.returncode)

# Run l4s-wired
with open("run.txt", "a") as out:
    result = subprocess.run(
        [
            path_to_ns3_script,
            "run",
            "--no-build",
            "--cwd",
            os.getcwd(),
            "l4s-wired",
            "--",
        ]
        + arguments.split(),
        stdout=out,
        text=True,
    )

# Save the parameters used in a 'commandlog.txt' file
with open("commandlog.txt", "a") as out:
    out.write("ns3 run l4s-wired" + " -- " + arguments + "\n")
    out.close()

# Make a plot
if 'testName' in vars():
    if testName != "" :
        subprocess.run(
            ["python3", "plot-l4s-loss-tests.py", "testName", testName, plotTitle], stdout=subprocess.PIPE, text=True
        )
else:
    subprocess.run(
        ["python3", "plot-l4s-loss-tests.py", plotTitle], stdout=subprocess.PIPE, text=True
    )

# TestName should be a string with no spaces
testName = "no-loss-prague"
numCubic = 0 
numPrague = 1
numBytes = 0
duration = 10
# Base RTT is 2 * wanLinDelay
useReno = 0 

arguments = " --numCubic=" + str(numCubic)
arguments += " --numPrague=" + str(numPrague)
arguments += " --numBytes=" + str(numBytes)
arguments += " --duration=" + str(duration)
arguments += " --wanLinkDelay=" + wanLinkDelay
arguments += " --bottleneckRate=" + bottleneckRate
arguments += " --useReno=" + str(useReno)
if 'testName' in vars():
    if testName != "":
        arguments += " --testName=" + testName

# Build a plot title; customize as needed
plotTitle = "No loss, Prague,"
plotTitle += " " + rttValue + " " + unit + " base RTT"

try:
    with open("build.txt", "a") as out:
        subprocess.run(
            [path_to_ns3_script, "build"], stdout=out, stderr=out, check=True
        )
except subprocess.CalledProcessError as e:
    print(f"Build error: {e}:  Check build.txt file for error.")
    sys.exit(e.returncode)

# Run l4s-wired
with open("run.txt", "a") as out:
    result = subprocess.run(
        [
            path_to_ns3_script,
            "run",
            "--no-build",
            "--cwd",
            os.getcwd(),
            "l4s-wired",
            "--",
        ]
        + arguments.split(),
        stdout=out,
        text=True,
    )

# Save the parameters used in a 'commandlog.txt' file
with open("commandlog.txt", "a") as out:
    out.write("ns3 run l4s-wired" + " -- " + arguments + "\n")
    out.close()

# Make a plot
if 'testName' in vars():
    if testName != "" :
        subprocess.run(
            ["python3", "plot-l4s-loss-tests.py", "testName", testName, plotTitle], stdout=subprocess.PIPE, text=True
        )
else:
    subprocess.run(
        ["python3", "plot-l4s-loss-tests.py", plotTitle], stdout=subprocess.PIPE, text=True
    )

# TestName should be a string with no spaces
testName = "burst-loss-cubic"
numCubic = 1 
numPrague = 0
numBytes = 0
duration = 10
# Base RTT is 2 * wanLinDelay
useReno = 0
#lossSequence = "20000"
lossBurst = "20000-20212"

arguments = " --numCubic=" + str(numCubic)
arguments += " --numPrague=" + str(numPrague)
arguments += " --numBytes=" + str(numBytes)
arguments += " --duration=" + str(duration)
arguments += " --wanLinkDelay=" + wanLinkDelay
arguments += " --bottleneckRate=" + bottleneckRate
arguments += " --useReno=" + str(useReno)
if 'testName' in vars():
    if testName != "":
        arguments += " --testName=" + testName
if 'lossSequence' in vars():
    if lossSequence != "":
        arguments += " --lossSequence=" + lossSequence
if 'lossBurst' in vars():
    if lossBurst != "":
        arguments += " --lossBurst=" + lossBurst

# Build a plot title; customize as needed
plotTitle = "single burst loss (213 packets), Cubic,"
plotTitle += " " + rttValue + " " + unit + " base RTT"

try:
    with open("build.txt", "a") as out:
        subprocess.run(
            [path_to_ns3_script, "build"], stdout=out, stderr=out, check=True
        )
except subprocess.CalledProcessError as e:
    print(f"Build error: {e}:  Check build.txt file for error.")
    sys.exit(e.returncode)

# Run l4s-wired
with open("run.txt", "a") as out:
    result = subprocess.run(
        [
            path_to_ns3_script,
            "run",
            "--no-build",
            "--cwd",
            os.getcwd(),
            "l4s-wired",
            "--",
        ]
        + arguments.split(),
        stdout=out,
        text=True,
    )

# Save the parameters used in a 'commandlog.txt' file
with open("commandlog.txt", "a") as out:
    out.write("ns3 run l4s-wired" + " -- " + arguments + "\n")
    out.close()

# Make a plot
if 'testName' in vars():
    if testName != "" :
        subprocess.run(
            ["python3", "plot-l4s-loss-tests.py", "testName", testName, plotTitle], stdout=subprocess.PIPE, text=True
        )
else:
    subprocess.run(
        ["python3", "plot-l4s-loss-tests.py", plotTitle], stdout=subprocess.PIPE, text=True
    )

# TestName should be a string with no spaces
testName = "burst-loss-prague"
numCubic = 0
numPrague = 1
numBytes = 0
duration = 10
# Base RTT is 2 * wanLinDelay
useReno = 0
lossBurst = "20000-20178"

arguments = " --numCubic=" + str(numCubic)
arguments += " --numPrague=" + str(numPrague)
arguments += " --numBytes=" + str(numBytes)
arguments += " --duration=" + str(duration)
arguments += " --wanLinkDelay=" + wanLinkDelay
arguments += " --bottleneckRate=" + bottleneckRate
arguments += " --useReno=" + str(useReno)
if 'testName' in vars():
    if testName != "":
        arguments += " --testName=" + testName
if 'lossSequence' in vars():
    if lossSequence != "":
        arguments += " --lossSequence=" + lossSequence
if 'lossBurst' in vars():
    if lossBurst != "":
        arguments += " --lossBurst=" + lossBurst

# Build a plot title; customize as needed
plotTitle = "single burst loss (180 packets), Prague,"
plotTitle += " " + rttValue + " " + unit + " base RTT"

try:
    with open("build.txt", "a") as out:
        subprocess.run(
            [path_to_ns3_script, "build"], stdout=out, stderr=out, check=True
        )
except subprocess.CalledProcessError as e:
    print(f"Build error: {e}:  Check build.txt file for error.")
    sys.exit(e.returncode)

# Run l4s-wired
with open("run.txt", "a") as out:
    result = subprocess.run(
        [
            path_to_ns3_script,
            "run",
            "--no-build",
            "--cwd",
            os.getcwd(),
            "l4s-wired",
            "--",
        ]
        + arguments.split(),
        stdout=out,
        text=True,
    )

# Save the parameters used in a 'commandlog.txt' file
with open("commandlog.txt", "a") as out:
    out.write("ns3 run l4s-wired" + " -- " + arguments + "\n")
    out.close()

# Make a plot
if 'testName' in vars():
    if testName != "" :
        subprocess.run(
            ["python3", "plot-l4s-loss-tests.py", "testName", testName, plotTitle], stdout=subprocess.PIPE, text=True
        )
else:
    subprocess.run(
        ["python3", "plot-l4s-loss-tests.py", plotTitle], stdout=subprocess.PIPE, text=True
    )

# TestName should be a string with no spaces
testName = "single-loss-prague"
numCubic = 0
numPrague = 1
numBytes = 0
duration = 10
# Base RTT is 2 * wanLinDelay
useReno = 0
lossSequence = "22000"
lossBurst = ""

arguments = " --numCubic=" + str(numCubic)
arguments += " --numPrague=" + str(numPrague)
arguments += " --numBytes=" + str(numBytes)
arguments += " --duration=" + str(duration)
arguments += " --wanLinkDelay=" + wanLinkDelay
arguments += " --bottleneckRate=" + bottleneckRate
arguments += " --useReno=" + str(useReno)
if 'testName' in vars():
    if testName != "":
        arguments += " --testName=" + testName
if 'lossSequence' in vars():
    if lossSequence != "":
        arguments += " --lossSequence=" + lossSequence
if 'lossBurst' in vars():
    if lossBurst != "":
        arguments += " --lossBurst=" + lossBurst

# Build a plot title; customize as needed
plotTitle = "single packet loss, Prague,"
plotTitle += " " + rttValue + " " + unit + " base RTT"

try:
    with open("build.txt", "a") as out:
        subprocess.run(
            [path_to_ns3_script, "build"], stdout=out, stderr=out, check=True
        )
except subprocess.CalledProcessError as e:
    print(f"Build error: {e}:  Check build.txt file for error.")
    sys.exit(e.returncode)

# Run l4s-wired
with open("run.txt", "a") as out:
    result = subprocess.run(
        [
            path_to_ns3_script,
            "run",
            "--no-build",
            "--cwd",
            os.getcwd(),
            "l4s-wired",
            "--",
        ]
        + arguments.split(),
        stdout=out,
        text=True,
    )

# Save the parameters used in a 'commandlog.txt' file
with open("commandlog.txt", "a") as out:
    out.write("ns3 run l4s-wired" + " -- " + arguments + "\n")
    out.close()

# Make a plot
if 'testName' in vars():
    if testName != "" :
        subprocess.run(
            ["python3", "plot-l4s-loss-tests.py", "testName", testName, plotTitle], stdout=subprocess.PIPE, text=True
        )
else:
    subprocess.run(
        ["python3", "plot-l4s-loss-tests.py", plotTitle], stdout=subprocess.PIPE, text=True
    )

### end of tests

try:
    subprocess.run(
        ["/var/www/html/flaskapp/multiflow_ns3.sh", "l4s-wired-2-1.pcap", "l4s-wired-0-0.pcap"], stdout=subprocess.PIPE, text=True
    )
except:
    pass

sys.exit()
