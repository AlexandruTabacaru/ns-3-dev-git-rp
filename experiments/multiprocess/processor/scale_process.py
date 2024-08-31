#!/usr/bin/python3

import os, sys
import pandas as pd
import matplotlib.pyplot as plt

if len(sys.argv) < 2:
    baseDir="."
else:
    baseDir=sys.argv[1]


# Read the CSV file into a DataFrame
df = pd.read_csv(os.path.join(baseDir,'results.csv'))

# subtract the WAN delay from the latency numbers that we'll be using
wanDelay=pd.to_numeric(df['wanLinkDelay'].str.extract(r'(\d+)')[0])
df['P99 Lat. DL Cubic']=df['P99 Lat. DL Cubic'] - wanDelay
df['P99 Lat. DL Prague']=df['P99 Lat. DL Prague'] - wanDelay


# Create a new DataFrame with the data of interest

# first grab the data from the 2 cubic, 0 prague rows (xTC == 1)
new_df = df[df['xTC'] == 1][['xED', 'numBackgroundUdp', 'mcs', 'scale', 
                              'P99 Lat. DL Cubic', 'Mean Rate DL Cubic (Mbps)']]

new_df = new_df.rename(columns={
    'P99 Lat. DL Cubic': 'P99 Lat. 2 Cubic',
    'Mean Rate DL Cubic (Mbps)': 'Mean Rate 2 Cubic (Mbps)'
})

# Next add data from the 0 cubic, 2 prague rows (xTC == 2)
xTC2_data = df[df['xTC'] == 2][['xED', 'numBackgroundUdp', 'mcs', 'scale',
                                'P99 Lat. DL Prague', 'Mean Rate DL Prague (Mbps)']]
xTC2_data = xTC2_data.rename(columns={
    'P99 Lat. DL Prague': 'P99 Lat. 2 Prague',
    'Mean Rate DL Prague (Mbps)': 'Mean Rate 2 Prague (Mbps)'
})
new_df = pd.merge(new_df, xTC2_data, on=['xED', 'numBackgroundUdp', 'mcs', 'scale'])


# Then add data from the 1 cubic, 1 prague rows (xTC == 3)
xTC3_data = df[df['xTC'] == 3][['xED', 'numBackgroundUdp', 'mcs', 'scale',
                                'P99 Lat. DL Cubic', 'Mean Rate DL Cubic (Mbps)', 
                                'P99 Lat. DL Prague', 'Mean Rate DL Prague (Mbps)']]
xTC3_data = xTC3_data.rename(columns={
    'P99 Lat. DL Cubic': 'P99 Lat. 1:1 Cubic',
    'Mean Rate DL Cubic (Mbps)': 'Mean Rate 1:1 Cubic (Mbps)',
    'P99 Lat. DL Prague': 'P99 Lat. 1:1 Prague',
    'Mean Rate DL Prague (Mbps)': 'Mean Rate 1:1 Prague (Mbps)'
})
new_df = pd.merge(new_df, xTC3_data, on=['xED', 'numBackgroundUdp', 'mcs', 'scale'])

# Reset the index
new_df = new_df.reset_index(drop=True)

# Rename 'xED' column to 'EDCA' & give the values more meaningful info
new_df = new_df.rename(columns={'xED': 'EDCA'})
new_df['EDCA'] = new_df['EDCA'].replace({0: 'AC_BE default', 4: 'Fixed CW63 1ms'})

# Rename 'numBackgroundUdp' column to 'OBSS'
new_df = new_df.rename(columns={'numBackgroundUdp': 'OBSS'})

# Sort the data correctly
new_df = new_df.sort_values(by=['EDCA', 'OBSS', 'mcs', 'scale'])

# Reset the index after sorting
new_df = new_df.reset_index(drop=True)


# save the data as a CSV file
new_df.to_csv(os.path.join(baseDir,'scale_data.csv'))


# Generate the plots

# Define the combinations
edca_values = new_df['EDCA'].unique()
obss_values = new_df['OBSS'].unique()
mcs_values = new_df['mcs'].unique()

# Define the column prefixes for Mean Rate and P99 Lat
prefixes = ['Mean Rate', 'P99 Lat.']

for prefix in prefixes:
    for edca in edca_values:
        for obss in obss_values:
            for mcs in mcs_values:
                # Filter the data
                data = new_df[(new_df['EDCA'] == edca) & (new_df['OBSS'] == obss) & (new_df['mcs'] == mcs)]
                
                # Create the plot, Mean Rate on top, P99 Lat on bottom
                fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(8, 8))
                
                ax1.semilogx(data['scale'], data['Mean Rate 2 Cubic (Mbps)'], label='2Cubic', color='orange', linestyle='-')
                ax1.semilogx(data['scale'], data['Mean Rate 2 Prague (Mbps)'], label='2Prague', color='blue', linestyle='-')
                ax1.semilogx(data['scale'], data['Mean Rate 1:1 Cubic (Mbps)'], label='1:1Cubic', color='orange', linestyle='--')
                ax1.semilogx(data['scale'], data['Mean Rate 1:1 Prague (Mbps)'], label='1:1Prague', color='blue', linestyle='--')

                ax1.set_xlim(0.1, 10)  
                ax1.set_xticks([0.1, 1, 10], ['0.1', '1', '10'])
                ax1.autoscale(axis='y')
                y_min, y_max = ax1.get_ylim()
                ax1.set_ylim(0, y_max)
                ax1.grid(which='both', linestyle=':', linewidth='0.5')
                ax1.set_xlabel('Scale')
                ax1.set_ylabel('Mean Rate (Mbps)')
                ax1.set_title(f'Mean Rate - EDCA: {edca}, OBSS: {obss}, MCS: {mcs}')
                ax1.legend()
                
                ax2.semilogx(data['scale'], data['P99 Lat. 2 Cubic'], label='2Cubic', color='orange', linestyle='-')
                ax2.semilogx(data['scale'], data['P99 Lat. 2 Prague'], label='2Prague', color='blue', linestyle='-')
                ax2.semilogx(data['scale'], data['P99 Lat. 1:1 Cubic'], label='1:1Cubic', color='orange', linestyle='--')
                ax2.semilogx(data['scale'], data['P99 Lat. 1:1 Prague'], label='1:1Prague', color='blue', linestyle='--')

                ax2.set_xlim(0.1, 10)  
                ax2.set_xticks([0.1, 1, 10], ['0.1', '1', '10'])
                ax2.autoscale(axis='y')
                y_min, y_max = ax2.get_ylim()
                ax2.set_ylim(0, y_max)
                ax2.grid(which='both', linestyle=':', linewidth='0.5')
                ax2.set_xlabel('Scale')
                ax2.set_ylabel('P99 Latency (ms)')
                ax2.set_title(f'P99 Latency - EDCA: {edca}, OBSS: {obss}, MCS: {mcs}')
                ax2.legend()
                
                fig.tight_layout()

                # Save the plot
                plt.savefig(f'Scale_test_{edca}_{obss}obss_mcs{mcs}.svg'.replace(" ", "_"), format='svg')
                plt.close()







