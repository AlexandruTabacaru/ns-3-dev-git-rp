#!/usr/bin/python3

# run the run_l4s_wifi.py campaign N times and then aggregate the results using proc_monte_carlo.py


import os, sys
import pandas as pd
import numpy as np
from scipy import stats
from datetime import datetime
from run_l4s_wifi import run_campaign, runNS3Build
from proc_monte_carlo import process_monte_carlo_data

# Create root multiprocessing directory
formatted_date = datetime.now().strftime("%Y%m%d-%H%M%S")
rootResultsdir = "montecarlo" + "-" + formatted_date
os.makedirs(rootResultsdir, exist_ok=False)

# Build simulator
build_filepath = os.path.join(rootResultsdir, "build.txt")
runNS3Build(build_filepath)

NumRuns=10
RunStart=0
result_files=[]

for rn in range(NumRuns):
	run=rn+RunStart
	ResultsDir=os.path.join(rootResultsdir,"run"+str(run))
	result=run_campaign(ResultsDir,run)
	result_files.append(result)


# rootResultsdir="montecarlo-20240612-155842"

process_monte_carlo_data(rootResultsdir)

sys.exit()

