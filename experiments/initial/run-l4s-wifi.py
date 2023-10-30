#!/usr/bin/python3
import os
import sys
import subprocess
import shutil
from datetime import datetime

# In future, add parametric job control here (multiprocessing.Pool)
numCubic = 1
numPrague = 1
numBackground = 0 
numBytes = 10000000
duration = 0
mcs = 2
flowControl = 1
limit = 65535
scale = 1
showProgress = 0

arguments = ' --numCubic=' + str(numCubic)
arguments += ' --numPrague=' + str(numPrague)

# Make a unique timestamped results directory
formatted_date = datetime.now().strftime("%Y%m%d-%H%M%S")
resultsdir = 'results-l4s-wifi' + formatted_date
os.makedirs(resultsdir, exist_ok=False)

# Copy the plotting file into the results directory
shutil.copy('plot-l4s-wifi.py', resultsdir)
# Copy any other files here

# Cd to the results directory
os.chdir(resultsdir)

# Copy this script into the results directory
script_filename = os.path.basename(__file__)
with open(__file__, 'r') as source:
    script_content = source.read()
with open(os.path.join(os.getcwd(), script_filename), 'w') as destination_file:
    destination_file.write(script_content)

try:
    subprocess.run(['../../../ns3', 'build'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
except subprocess.CalledProcessError as e:
    print(f"Error: {e}")
    sys.exit(e.returncode)

# Run l4s-wifi
with open('stdout', 'w') as out:
    result = subprocess.run(['../../../ns3', 'run', '--cwd', os.getcwd(), 'l4s-wifi', '--', arguments], stdout=out, text=True)

# Make a plot 
subprocess.run(['python3', 'plot-l4s-wifi.py'], stdout=subprocess.PIPE, text=True)

sys.exit()

# Below is unfinished porting
command1 = "../ns3 show profile"
command2 = "awk '{ print $NF }'"
process1 = subprocess.Popen(command1, shell=True, stdout=subprocess.PIPE)
process2 = subprocess.Popen(command2, shell=True, stdin=process1.stdout, stdout=subprocess.PIPE)
process1.stdout.close()
output = process2.communicate()[0]
profile = output.decode()
print(profile)


result = subprocess.run(['git', 'rev-parse', '--abbrev-ref', 'HEAD'], stdout=subprocess.PIPE, text=True)
repositoryVersion = result.stdout.strip()

print(repositoryVersion)

