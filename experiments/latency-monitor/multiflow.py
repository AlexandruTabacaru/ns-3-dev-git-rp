# Copyright 2023 Cable Television Laboratories, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import sys
import csv
import numpy as np
import matplotlib 
import matplotlib.pyplot as plt
import matplotlib.image as image
import library


matplotlib.use('Agg')

# init global variables
t_steadystate = 0
max_flow_plots=20
min_pkts_per_flow=0

dir_ext = str(sys.argv.pop(4))
first_index = 0
dir_ext = dir_ext[first_index:]
nsi_name = str(sys.argv.pop(3))
cmci_name = str(sys.argv.pop(2))
f_name = sys.argv.pop(1)


# extract flows from tshark
parsed_file = library.parse_flows_file(cmci_name)

# calculate packet latencies, drops, etc.
parsed_data_up, parsed_data_down = library.parse_data(parsed_file, nsi_name)

del parsed_file


# merge smaller upstream flows to limit number of flows plotted 
pkt_count_up={}
for flow_id in parsed_data_up:
    pkt_count_up[flow_id]=parsed_data_up[flow_id][3]
pkt_count_up_sorted=sorted(pkt_count_up.items(), key=lambda x:x[1], reverse=True)
num_flow_plots=0
for num_flow_plots in range(0,min(max_flow_plots,len(pkt_count_up_sorted))):
    if pkt_count_up_sorted[num_flow_plots][1] < min_pkts_per_flow:
        break
if len(pkt_count_up_sorted)>num_flow_plots+1:
    aggregated_data=[{},{},{},0,0,{},{},{},{}]
    for flow_id,pkt_count in pkt_count_up_sorted[num_flow_plots:]:
        for i in [0,1,2,7,8]: # accumulate these
            aggregated_data[i]=aggregated_data[i]|parsed_data_up[flow_id][i]       
        for i in [3,4]: # sum these    
            aggregated_data[i]=aggregated_data[i]+parsed_data_up[flow_id][i]
        for i in [5,6]: # join these
            aggregated_data[i]={k: aggregated_data[i].get(k, 0) + parsed_data_up[flow_id][i].get(k,0) for k in set(aggregated_data[i]) | set(parsed_data_up[flow_id][i])}
        #print(aggregated_data[5])
        parsed_data_up.pop(flow_id)
    parsed_data_up["other flows"]=aggregated_data

print("========= upstream flows to be plotted ==========")
for flow_id in parsed_data_up:
    print(flow_id,parsed_data_up[flow_id][3],"pkts")

# merge smaller downstream flows to limit number of flows plotted 
pkt_count_down={}
for flow_id in parsed_data_down:
    pkt_count_down[flow_id]=parsed_data_down[flow_id][3]
pkt_count_down_sorted=sorted(pkt_count_down.items(), key=lambda x:x[1], reverse=True)
num_flow_plots=0
for num_flow_plots in range(0,min(max_flow_plots,len(pkt_count_down_sorted))):
    if pkt_count_down_sorted[num_flow_plots][1] < min_pkts_per_flow:
        break
if len(pkt_count_down_sorted)>num_flow_plots+1:
    aggregated_data=[{},{},{},0,0,{},{},{},{}]
    for flow_id,pkt_count in pkt_count_down_sorted[num_flow_plots:]:
        for i in [0,1,2,7,8]: # accumulate these
            aggregated_data[i]=aggregated_data[i]|parsed_data_down[flow_id][i]       
        for i in [3,4]: # sum these 
            aggregated_data[i]=aggregated_data[i]+parsed_data_down[flow_id][i]
        for i in [5,6]: # join these
            aggregated_data[i]={k: aggregated_data[i].get(k, 0) + parsed_data_down[flow_id][i].get(k,0) for k in set(aggregated_data[i]) | set(parsed_data_down[flow_id][i])}
        #print(aggregated_data[5])
        parsed_data_down.pop(flow_id)
    parsed_data_down["other flows"]=aggregated_data

print("========= downstream flows to be plotted ==========")
for flow_id in parsed_data_down:
    print(flow_id,parsed_data_down[flow_id][3],"pkts")


           

# UPSTREAM MULTIFLOW PLOTS
library.multiflow_plot_and_csv(parsed_data_up, True, t_steadystate, dir_ext)

# DOWNSTREAM MULTIFLOW PLOTS
library.multiflow_plot_and_csv(parsed_data_down, False, t_steadystate, dir_ext)


# UPSTREAM SINGLEFLOW PLOTS

count = 1
for flow in parsed_data_up:
    pkt_drop_times = parsed_data_up[flow][0]
    latency = parsed_data_up[flow][1]
    pkt_size = parsed_data_up[flow][2]
    total_packets = parsed_data_up[flow][3]
    unmatched_packets = parsed_data_up[flow][4]
    dscp = parsed_data_up[flow][5]
    ecn = parsed_data_up[flow][6]
    dscp3 = parsed_data_up[flow][7]
    CE_size = parsed_data_up[flow][8]
    name = str(count) + '_upstream.pdf'
    count += 1
    if not latency: continue

    library.plot_singleflow(pkt_drop_times, latency, pkt_size, total_packets, dscp, ecn, unmatched_packets, t_steadystate, name, dir_ext, dscp3, CE_size, flow)


# DOWNSTREAM SINGLEFLOW PLOTS

count = 1
for flow in parsed_data_down:
    pkt_drop_times = parsed_data_down[flow][0]
    latency = parsed_data_down[flow][1]
    pkt_size = parsed_data_down[flow][2]
    total_packets = parsed_data_down[flow][3]
    unmatched_packets = parsed_data_down[flow][4]
    dscp = parsed_data_down[flow][5]
    ecn = parsed_data_down[flow][6]
    dscp3 = parsed_data_down[flow][7]
    CE_size = parsed_data_down[flow][8]
    name = str(count) + '_downstream.pdf'
    count += 1
    if not latency: continue

    library.plot_singleflow(pkt_drop_times, latency, pkt_size, total_packets, dscp, ecn, unmatched_packets, t_steadystate, name, dir_ext, dscp3, CE_size, flow)


# CSV WRITING (probably want to use better filenaming)
# TODO: change this to writing out the latency data for each flow, rather than the packet data

# csv_labels = ['Timestamp', 'DSCP', 'ECN', 'Frame Length']
# for flow in parsed_file:
#     with open(dir_ext + '/' + str(flow) + '.csv', 'w') as csv_out:
#         writer = csv.writer(csv_out)
#         writer.writerow(csv_labels)
#         for line in parsed_file[flow]: 
#             data = [float(line[0])] + [int(line[2])] + [int(line[3])] + [float(line[5])]
#             writer.writerow(data)
#         csv_out.close()


csv_labels = ['Timestamp', 'Latency', 'Frame Length']
for flow in parsed_data_up:
    latency = parsed_data_up[flow][1]
    pkt_size = parsed_data_up[flow][2]    
    with open(dir_ext + '/' + str(flow) + 'latency.csv', 'w') as csv_out:
        writer = csv.writer(csv_out)
        writer.writerow(csv_labels)
        for timestamp in latency: 
            ll=latency[timestamp]
            si=pkt_size[timestamp]
            data = [float(timestamp)] + [float(ll)] + [int(si)]
            writer.writerow(data)
        csv_out.close()
for flow in parsed_data_down:
    latency = parsed_data_down[flow][1]
    pkt_size = parsed_data_down[flow][2]    
    with open(dir_ext + '/' + str(flow) + 'latency.csv', 'w') as csv_out:
        writer = csv.writer(csv_out)
        writer.writerow(csv_labels)
        for timestamp in latency: 
            ll=latency[timestamp]
            si=pkt_size[timestamp]
            data = [float(timestamp)] + [float(ll)] + [int(si)]
            writer.writerow(data)
        csv_out.close()

