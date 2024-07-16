#!/usr/bin/python3

import pandas as pd
import numpy as np
import glob, os, sys, re


# a function to process the 'Flow ID' column and extract CC type and flow number
def process_flow_id(flow_id):
    # Split the string and get the last item
    last_item = flow_id.split()[-1]
    
    # Extract x and y using regex
    match = re.search(r'(\d)(\d+)]', last_item)
    if match:
        x, y = match.groups()
        cc_type = 'Prague' if x == '1' else 'Cubic'
        return pd.Series([cc_type, y])
    else:
        return pd.Series([None, None])

# main code

# # read in all of the data from the summary tables
# allData = pd.DataFrame()
# for dir in glob.glob(os.path.join(sys.argv[1],"ED*-MS*")):
# 	testCase = os.path.basename(dir).split("_")[0]
# 	for subdir in glob.glob(os.path.join(dir,"00*")):
# 		df = pd.read_csv(os.path.join(subdir, "summary_table_downstream.csv"), encoding='ISO-8859-1')
# 		df.drop('Flow Number', axis=1, inplace=True)
# 		df.insert(0,'TestCase',testCase)
# 		chunkNumber = os.path.basename(subdir)
# 		df.insert(1, 'Chunk', chunkNumber)
# 		allData = pd.concat([allData, df], ignore_index=True)
# allData.sort_values(by=['TestCase', 'Flow ID', 'Chunk'], inplace=True)
# allData.to_csv(os.path.join(sys.argv[1],'allData.csv'), index=False)

interval = sys.argv[2] if len(sys.argv) > 2 else 1 # 1 second default value

# read in all of the data from the latency.csv files
allData = pd.DataFrame()
for dir in glob.glob(os.path.join(sys.argv[1],"ED*-MS*")):
	print(f"Reading: {dir}")
	testCase = os.path.basename(dir).split("_")[0]
	for subdir in glob.glob(os.path.join(dir,"00*")):
		for DsLatencyCsvFile in glob.glob(os.path.join(subdir, "TCP*10.1.1.1*192.168.1.2*latency.csv")):
			flowId = os.path.basename(DsLatencyCsvFile).rstrip("latency.csv")
			rawData = pd.read_csv(DsLatencyCsvFile, encoding='ISO-8859-1')
			rawData['Chunk'] =  (rawData['Timestamp']/interval).astype(int)
			df = rawData.groupby(['Chunk']).agg({
				'Latency': [
		        	('P0', lambda x: 1000*np.percentile(x, 0)),
			        ('P90', lambda x: 1000*np.percentile(x, 90)),
			        ('P99', lambda x: 1000*np.percentile(x, 99)),
			        ('P99.9', lambda x: 1000*np.percentile(x, 99.9)),
			        ('P100', lambda x: 1000*np.percentile(x, 100))
			    ],
			    'Frame Length': lambda x: 8 * x.sum() /1000000
		    }).rename(columns={'Frame Length': 'Mean data rate (Mbps)'}).reset_index()
			df.columns = ['_'.join(col).strip('_<lambda>') for col in df.columns.values]

			df.insert(0,'TestCase',testCase)
			df.insert(1,'Flow ID',flowId)
			allData = pd.concat([allData, df], ignore_index=True)
# print(allData)
allData.sort_values(by=['TestCase', 'Flow ID', 'Chunk'], inplace=True)
allData.to_csv(os.path.join(sys.argv[1],'allData.csv'), index=False)

# filter out the rows where the Flow ID is "other flows"
filtered_data = allData[allData['Flow ID'] != "other flows"].copy()

# Process the Flow ID column to extract CC-type (cubic or prague) and flow number
flow_id = filtered_data['Flow ID'].apply(process_flow_id)

# Insert new columns & delete the Flow ID column
filtered_data.insert(1, 'CC_Type', flow_id[0])
filtered_data.insert(2, 'FlowNum', flow_id[1])
filtered_data.drop('Flow ID', axis=1, inplace=True)

filtered_data.to_csv(os.path.join(sys.argv[1],'filteredData.csv'), index=False)

filtered_data = filtered_data.rename(columns={'Latency_P99': 'Packet Delay P99 (ms)'}) # rename column to the old name used below

# print(filtered_data)
# subtract the base delay from the P99 packet delay values to leave just the wifi link delay
filtered_data.loc[filtered_data['TestCase'].str.endswith('-TS1'), 'Packet Delay P99 (ms)'] -= 1
filtered_data.loc[filtered_data['TestCase'].str.endswith('-TS2'), 'Packet Delay P99 (ms)'] -= 5
filtered_data.loc[filtered_data['TestCase'].str.endswith('-TS3'), 'Packet Delay P99 (ms)'] -= 25

# filtered_data['Test Case'] = filtered_data['Test Case'].str.replace(r'-TS\d+$', '', regex=True)

# filter out the rows where Chunk is less than 10 (seconds), to ignore the startup chunks
filtered_data = filtered_data[filtered_data['Chunk'] > 10]

# Now, perform the groupby and aggregation
summary = filtered_data.groupby(['TestCase', 'CC_Type']).agg({
    'Packet Delay P99 (ms)': [
        ('P0', lambda x: round(np.percentile(x, 0),1)),
        ('P10', lambda x: round(np.percentile(x, 10),1)),
        ('P25', lambda x: round(np.percentile(x, 25),1)),
        ('P50', lambda x: round(np.percentile(x, 50),1)),
        ('P75', lambda x: round(np.percentile(x, 75),1)),
        ('P90', lambda x: round(np.percentile(x, 90),1)),
        ('P100', lambda x: round(np.percentile(x, 100),1))
    ],
    'Mean data rate (Mbps)': [
        ('P0', lambda x: round(np.percentile(x, 0),1)),
        ('P10', lambda x: round(np.percentile(x, 10),1)),
        ('P25', lambda x: round(np.percentile(x, 25),1)),
        ('P50', lambda x: round(np.percentile(x, 50),1)),
        ('P75', lambda x: round(np.percentile(x, 75),1)),
        ('P90', lambda x: round(np.percentile(x, 90),1)),
        ('P100', lambda x: round(np.percentile(x, 100),1)),
        ('Mean', lambda x: round(np.mean(x),1)),
        ('P90:P10 ratio', lambda x: round(np.percentile(x, 90)/np.percentile(x, 10) ,1))
    ],
    # 'Num Packets': 'sum',
    # 'Dropped Packets': 'sum'
}).reset_index()

# Flatten the column names
summary.columns = ['TestCase', 'CC_Type',
                  'Link Delay P99 P0 (ms)', 'Link Delay P99 P10 (ms)', 'Link Delay P99 P25 (ms)', 
                  'Link Delay P99 P50 (ms)', 'Link Delay P99 P75 (ms)', 'Link Delay P99 P90 (ms)', 
                  'Link Delay P99 P100 (ms)',
                  'Mean data rate P0 (Mbps)', 'Mean data rate P10 (Mbps)', 'Mean data rate P25 (Mbps)', 
                  'Mean data rate P50 (Mbps)', 'Mean data rate P75 (Mbps)', 'Mean data rate P90 (Mbps)', 
                  'Mean data rate P100 (Mbps)',
                  'Mean data rate mean (Mbps)', 'Rate ratio P90:P10',
                  # 'Total Packets','Total Dropped Packets'
                  ]

summary['Test Case'] = summary['Test Case'].str.replace(r'-TC2', '-TC1', regex=True)
summary.pivot(index='TestCase',columns='CC_Type')

summary.to_csv(os.path.join(sys.argv[1],'summary.csv'), index=False)

