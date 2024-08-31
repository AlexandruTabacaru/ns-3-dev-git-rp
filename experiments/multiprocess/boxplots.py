#!/usr/bin/python3

import pandas as pd
import numpy as np
import glob, os, sys, re
import matplotlib.pyplot as plt

df = pd.read_csv('filteredData.csv')

# subtract the base delay from the P99 packet delay values to leave just the wifi link delay
df.loc[df['TestCase'].str.endswith('-TS1'), 'Latency_P99'] -= 1
df.loc[df['TestCase'].str.endswith('-TS2'), 'Latency_P99'] -= 5
df.loc[df['TestCase'].str.endswith('-TS3'), 'Latency_P99'] -= 25

# filter out the rows where Chunk is less than 10 (seconds), to ignore the startup chunks
df = df[df['Chunk'] > 10]

# re-label TC2 as TC1 so we can plot them together
df['TestCase'] = df['TestCase'].str.replace(r'-TC2', '-TC1', regex=True)

# Define the positions to group into pairs
positions = [1, 2, 4, 5, 7, 8, 10, 11]

# Define cubic, prague colors
colors = ['orange', 'blue']

# Define title strings
MSstrings=['Clear Channel','One Competing STA', 'Four Competing STAs']
LSstrings=['','20 MHz Channel','80 MHz Channel','160 MHz Channel']
APstrings=['NoAQM','DualPI2']

# Generate P99 Latency & Throughput plots
latency_data = []
throughput_data = []
for ed in [0,1,2,3,4,5]:
	for ms in [0,1,2]:
		for ap in [0, 1]:

			# Initialize figures and axes for both latency and throughput
			figs, axs = {}, {}
			figs['latency'], axs['latency'] = plt.subplots(1, 3, figsize=(20, 5))
			figs['throughput'], axs['throughput'] = plt.subplots(1, 3, figsize=(20, 5))

			# Initialize lists for latency and throughput (size = tc range below)
			latency_lists = [[],[],[],[]]
			thruput_lists = [[],[],[],[]]

			for ls in [1, 2, 3]:
				for tc in [1, 3, 4, 5]:
					pattern=f"ED{ed}-MS{ms}-AP{ap}-LS{ls}-TC{tc}-TS."
					for cc in ['Cubic', 'Prague']:
						# Filter rows where the TestCase column matches the regex pattern
						matching_rows = df[df['TestCase'].str.contains(pattern, regex=True, na=False) & (df['CC_Type'] == cc)]
				
						# Extract the Latency_P99 values and convert to a list
						latency_values = matching_rows['Latency_P99'].tolist()
						# Append the list of Latency_P99 values to the main list
						latency_lists[ls].append(latency_values)
						if latency_values:
							# Calculate median latency & append it to the latency_data table
							median_latency=np.median(latency_values)
							latency_data.append([ed, ms, ap, ls, tc, cc, median_latency])

						# Extract the Throughput values and convert to a list
						thruput_values = matching_rows['Mean data rate (Mbps)'].tolist()
						# Append the list of Throughput values to the main list
						thruput_lists[ls].append(thruput_values)
						if thruput_values:
							# Calculate median througput & append it to the throughput_data table
							median_thruput=np.median(thruput_values)
							mean_thruput  = np.mean(thruput_values)
							throughput_data.append([ed, ms, ap, ls, tc, cc, median_thruput, mean_thruput])

				# Generate Latency Plot

				# Create the box plot
				bplot = axs['latency'][ls-1].boxplot(latency_lists[ls], positions=positions, whis=(5,95), sym='', 
					patch_artist=True, medianprops=dict(color='black'))

				# Alternate colors
				for i, box in enumerate(bplot['boxes']):
					box.set_facecolor(colors[i % 2])

				# Set the x-axis ticks, grid, axis labels & title 
				axs['latency'][ls-1].set_xticks([1.5, 4.5, 7.5, 10.5], ['1 vs. 1', '1 + 1', '2 + 2', '4 + 4'])
				axs['latency'][ls-1].yaxis.grid(True, linestyle='-', which='major', color='lightgrey',
					alpha=0.5)
				axs['latency'][ls-1].set(
					axisbelow=True,  # Hide the grid behind plot objects
					title=f"{LSstrings[ls]} - {MSstrings[ms]}",
					xlabel='Traffic Load',
					ylabel='P99 Latency (ms)',
				)

				# Generate Throughput Plot

				# Create the box plot
				bplot = axs['throughput'][ls-1].boxplot(thruput_lists[ls], positions=positions, whis=(5,95), sym='', 
					patch_artist=True, medianprops=dict(color='black'))

				# Alternate colors
				for i, box in enumerate(bplot['boxes']):
					box.set_facecolor(colors[i % 2])

				# Set the x-axis ticks, grid, axis labels & title 
				axs['throughput'][ls-1].set_xticks([1.5, 4.5, 7.5, 10.5], ['1 vs. 1', '1 + 1', '2 + 2', '4 + 4'])
				axs['throughput'][ls-1].yaxis.grid(True, linestyle='-', which='major', color='lightgrey',
					alpha=0.5)
				axs['throughput'][ls-1].set(
					axisbelow=True,  # Hide the grid behind plot objects
					title=f"{LSstrings[ls]} - {MSstrings[ms]}",
					xlabel='Traffic Load',
					ylabel='Mean Per-Flow Throughput (Mbps)',
				)

			# Save the figures as SVG files
			if any(any(values) for values in latency_lists):
				figs['latency'].savefig(f"ED{ed}-{MSstrings[ms]}-{APstrings[ap]}-Latency.svg".replace(" ","_"), format='svg', bbox_inches='tight')
			if any(any(values) for values in thruput_lists):
				figs['throughput'].savefig(f"ED{ed}-{MSstrings[ms]}-{APstrings[ap]}Throughput.svg".replace(" ","_"), format='svg', bbox_inches='tight')
			
			plt.close(figs['latency'])				
			plt.close(figs['throughput'])

latency_df = pd.DataFrame(latency_data, columns=['ed', 'ms', 'ap', 'ls', 'tc', 'cc', 'latency'])
throughput_df = pd.DataFrame(throughput_data, columns=['ed', 'ms', 'ap', 'ls', 'tc', 'cc', 'median throughput','mean throughput'])

result_df = pd.merge(latency_df, throughput_df, on=['ed', 'ms', 'ap', 'ls', 'tc', 'cc'])
result_df.to_csv("Median_Results.csv", index=False)
