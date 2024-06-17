#!/usr/bin/python3

import os, sys
import pandas as pd
import numpy as np
from scipy import stats
from datetime import datetime
from run_l4s_wifi import run_campaign, runNS3Build

# Create root multiprocessing directory
formatted_date = datetime.now().strftime("%Y%m%d-%H%M%S")
rootResultsdir = "montecarlo" + "-" + formatted_date
os.makedirs(rootResultsdir, exist_ok=False)

# Build simulator
build_filepath = os.path.join(rootResultsdir, "build.txt")
runNS3Build(build_filepath)

NumRuns=5
result_files=[]

for rn in range(NumRuns):
	run=rn+5
	ResultsDir=os.path.join(rootResultsdir,"run"+str(run))
	result=run_campaign(ResultsDir,run)
	result_files.append(result)


# rootResultsdir="montecarlo-20240612-155842"
# result_files=[os.path.join(rootResultsdir,"run0","detailed_results.csv"), os.path.join(rootResultsdir,"run1","detailed_results.csv")]

# read in each of the detailed_results.csv files
combined_df=pd.DataFrame()
for run, file in enumerate(result_files):
	df = pd.read_csv(file, header=0)
	df['Run Number']=run
	combined_df = pd.concat([combined_df,df], ignore_index=True)

# print(combined_df)

# Calculate the Mean and 95% Confidence Interval for a data set
def confidence_interval(x):
	if np.isinf(x).any() or np.isnan(x).any() or np.all(x == 0):
		return [np.nan, np.nan, np.nan]
	else:
		x_mean=np.mean(x)
		x_sem=stats.sem(x)
		if x_sem == 0:
			return [x_mean, x_mean, x_mean]
		else:
			# print(list(x)+[ x_mean,x_sem])
			ci = stats.norm.interval(0.95, loc=x_mean, scale=x_sem)
			ci = np.round(ci,1)
			return [x_mean]+list(ci)

# Which columns from the detailed_results.csv do we want to calculate summary stats for
my_Columns=['P99 Lat. DL Cubic',
'Mean Rate DL Cubic (Mbps)',	
'P99 Lat. DL Prague',
'Mean Rate DL Prague (Mbps)']

# do the summary stat calculations
agg_functions = {col: confidence_interval for col in my_Columns}
summary_df = combined_df.groupby('Test Case').agg(agg_functions).reset_index()

# expand the results into new columns
for col in my_Columns:
	result_expanded = pd.DataFrame(summary_df[col].tolist(), index=summary_df.index)
	result_expanded.columns = [col+' Mean', col+' CI_low', col+' CI_high']
	summary_df = summary_df.drop(columns=[col]).join(result_expanded)

finalResultFile=os.path.join(rootResultsdir,"result.csv")
summary_df.to_csv(finalResultFile, index=False)

# print(summary_df)

sys.exit()

