#!/usr/bin/python3

import os, sys
import pandas as pd
import numpy as np
from scipy import stats
from datetime import datetime
import glob


rootResultsdir="montecarlo-20240612-174947"


def process_monte_carlo_data(rootResultsdir):

	result_files=glob.glob(os.path.join(rootResultsdir,"run*","detailed_results.csv"))


	## read in each of the detailed_results.csv files
	combined_df=pd.DataFrame()
	for run, file in enumerate(result_files):
		df = pd.read_csv(file, header=0)
		df['Run Number']=run
		combined_df = pd.concat([combined_df,df], ignore_index=True)

	# print(combined_df)

	## Subtract wanLinkDelay from each of the Latency results
	# strip the "ms" from the wanLinkDelay values, and convert them to integers
	combined_df['wanLinkDelay'] = combined_df['wanLinkDelay'].str.replace('ms', '').astype(int)

	# get the set of column headings that contain 'Lat.' (i.e. they contain a latency value)
	lat_columns = [col for col in combined_df.columns if 'Lat.' in col]

	# Subtract wanLinkDelay from each latency column
	for col in lat_columns:
		combined_df[col] = combined_df[col] - combined_df['wanLinkDelay']


	# remove the '-TS*' from the Test Case column (i.e. remove the indication of wanLinkDelay so that rows will be grouped regardless of wanLinkDelay)
	combined_df['Test Case'] = combined_df['Test Case'].str.slice(0, -4)


	# Create a function to calculate the Mean and 95% Confidence Interval for a set of values
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


	# Split the "Test Case" column into multiple columns
	expanded_columns = summary_df['Test Case'].str.split('-', expand=True)

	# Name the new columns
	expanded_columns.columns = ['ED', 'MS', 'AP', 'LS', 'TC']

	# Concatenate the new columns to the original DataFrame
	summary_df = pd.concat([expanded_columns, summary_df], axis=1)

	replace_dict = {
	    'LS1': '20 MHz Channel',
	    'LS2': '80 MHz Channel',
	    'LS3': '160 MHz Channel'
	}
	summary_df['LS'] = summary_df['LS'].replace(replace_dict)

	summary_df['Cubic P99 Lat.'] = summary_df.apply(
	    lambda row: f"{row['P99 Lat. DL Cubic CI_low']} - {row['P99 Lat. DL Cubic CI_high']} ms",
	    axis=1
	)
	summary_df['Cubic Mean Rate'] = summary_df.apply(
	    lambda row: f"{row['Mean Rate DL Cubic (Mbps) CI_low']} - {row['Mean Rate DL Cubic (Mbps) CI_high']} Mbps",
	    axis=1
	)
	summary_df['Prague P99 Lat.'] = summary_df.apply(
	    lambda row: f"{row['P99 Lat. DL Prague CI_low']} - {row['P99 Lat. DL Prague CI_high']} ms",
	    axis=1
	)
	summary_df['Prague Mean Rate'] = summary_df.apply(
	    lambda row: f"{row['Mean Rate DL Prague (Mbps) CI_low']} - {row['Mean Rate DL Prague (Mbps) CI_high']} Mbps",
	    axis=1
	)

	# Single Flow results, Campaigns 1 & 2

	# Create new DataFrames with the single flow Cubic, no AQM results
	filtered_df = summary_df[
	    (summary_df['ED'] == 'ED0') &
	    (summary_df['AP'] == 'AP0') &
	    (summary_df['TC'] == 'TC1')
	]
	thruput_df1=filtered_df[['MS','LS','Cubic Mean Rate']].reset_index(drop=True)
	latency_df1=filtered_df[['MS','LS','Cubic P99 Lat.']].reset_index(drop=True)
	thruput_df1.columns=['MS','Throughput','Classic flow No AQM']
	latency_df1.columns=['MS','P99 Latency','Classic flow No AQM']

	# Create new DataFrames with the single flow Cubic, DualPI2 results
	filtered_df = summary_df[
	    (summary_df['ED'] == 'ED0') &
	    (summary_df['AP'] == 'AP1') &
	    (summary_df['TC'] == 'TC1')
	]
	thruput_df2=filtered_df[['MS','LS','Cubic Mean Rate']].reset_index(drop=True)
	latency_df2=filtered_df[['MS','LS','Cubic P99 Lat.']].reset_index(drop=True)
	thruput_df2.columns=['MS','Throughput','Classic flow DualPI2 AQM']
	latency_df2.columns=['MS','P99 Latency','Classic flow DualPI2 AQM']

	# Create new DataFrames with the single flow Prague, DualPI2 results
	filtered_df = summary_df[
	    (summary_df['ED'] == 'ED0') &
	    (summary_df['AP'] == 'AP1') &
	    (summary_df['TC'] == 'TC2')
	]
	thruput_df3=filtered_df[['MS','LS','Prague Mean Rate']].reset_index(drop=True)
	latency_df3=filtered_df[['MS','LS','Prague P99 Lat.']].reset_index(drop=True)
	thruput_df3.columns=['MS','Throughput','L4S flow DualPI2 AQM']
	latency_df3.columns=['MS','P99 Latency','L4S flow DualPI2 AQM']


	thruput_df = pd.concat([thruput_df1, thruput_df2['Classic flow DualPI2 AQM'], thruput_df3['L4S flow DualPI2 AQM']],axis=1)
	latency_df = pd.concat([latency_df1, latency_df2['Classic flow DualPI2 AQM'], latency_df3['L4S flow DualPI2 AQM']],axis=1)

	thruput_df.to_csv(os.path.join(rootResultsdir,"single_flow_thruput.csv"), index=False)
	latency_df.to_csv(os.path.join(rootResultsdir,"single_flow_latency.csv"), index=False)


if __name__ == "__main__":
	process_monte_carlo_data(rootResultsdir)


