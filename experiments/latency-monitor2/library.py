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

import sys
import os
import fileinput
import matplotlib.pyplot as plt
import matplotlib.image as image
import matplotlib.ticker as mticker
import numpy as np
import warnings
import csv


# process the tshark output
# returns : parsed_data : dictionary with key = flow_id and value = 2d list of all other data fields per packet
def parse_flows_file(if_name):
    pkt_count = {}
    parsed_data = {}

    for line in fileinput.input():
        x = line.split()
        # Expecting:
        # x[0]: frame.time_relative 
        # x[1]: frame.interface_name 
        # x[2]: ip.src 
        # x[3]: ip.dst 
        # x[4]: ip.proto 
        # x[5]: ip.dsfield.dscp 
        # x[6]: ip.dsfield.ecn 
        # x[7]: ip.id 
        # x[8]: frame.len 
        # x[9]: data (first 8 characters contain the port numbers)
        
        if len(x) < 10: continue # skip any rows that don't have all of the fields

        ipsrc = x[2]
        ipdst = x[3]
        if x[4] == "6":
            proto = "TCP"
        elif x[4] == "17":
            proto = "UDP"
        elif x[4] == "1":
            proto = "ICMP"
        else:
            proto = "UNK"

        if proto == "ICMP":
            src = "0"
            dst = "0"
        else:
            ports = x[-1][:8]
            if not ports: continue
            try:
                src = str(int(ports[:4], 16))
                dst = str(int(ports[4:], 16))
            except ValueError: continue

        # limit payload to first 120 characters
        x[9]=x[9][:120]

        # packet_id=x[7] + str(hash(x[9]))                # concatenate ip.id and hash of data
        # packet_id=str(hash(x[9]))                # just use hash of data (some network gear is mangling the ip.id )
        if proto == "TCP":
            packet_id=str(hash(x[9][0:32]+x[9][36:])) #ignore the TCP checksum
        elif proto == "UDP":
            packet_id=str(hash(x[9][0:12]+x[9][16:])) #ignore the UDP checksum
        else:
            packet_id=str(hash(x[9]))                # just use hash of data (some network gear is mangling the ip.id )

        # flow_id = proto + ' [' + ipsrc + ':' + src + '] -> [' + ipdst + ':' + dst + ']'
        flow_id = proto + ' [' + ipsrc + ' ' + src + '] to [' + ipdst + ' ' + dst + ']'

        try:
            pkt_count[flow_id] += 1
            parsed_data[flow_id].append(x[0:2] + x[5:9] + [packet_id])
        except KeyError:
            pkt_count[flow_id] = 1
            parsed_data[flow_id] = [x[0:2] + x[5:9] + [packet_id]]		

    # # remove any flows with fewer than 200 pkts
    # for flow in pkt_count:
    #     if pkt_count[flow] < 200:
    #         parsed_data.pop(flow)
    #         print(f"ignoring flow with {pkt_count[flow]} packets: {flow} ")


    # identify flows only seen on one interface, might be flow halves of NATed flows
    singleIfFlows={}
    for flow in parsed_data:
        # this could be simpler if all we're looking for are flows with a single ifname
        pkt_if_cnt={}
        for pkt in parsed_data[flow]:
            ifname=pkt[1]
            try:
                pkt_if_cnt[ifname] += 1
            except KeyError:
                pkt_if_cnt[ifname] = 1
        if len(pkt_if_cnt) ==1:
            singleIfFlows[flow] = ifname

    # join two halves of NATed flows
    for flow1 in singleIfFlows:
        #print(f"{singleIfFlows[flow1]}  {if_name}") 
        if (singleIfFlows[flow1] == if_name):
            for flow2 in singleIfFlows:
                if (singleIfFlows[flow1] and # check that there is an ifname, see 10 lines down
                    singleIfFlows[flow2] and # check that there is an ifname, see 10 lines down
                    flow1 != flow2 and 
                    singleIfFlows[flow1] != singleIfFlows[flow2] and  # if the 2 flows were found on different ifnames
    #                abs(pkt_count[flow1]-pkt_count[flow2])<100 and 
                    getProto(flow1) == getProto(flow2) and
                    (getSrc(flow1) == getSrc(flow2) or getDst(flow1) == getDst(flow2)) ): # this check won't work with dual-NAT
                    print(f"NAT detected: joining {flow1} {singleIfFlows[flow1]} {pkt_count[flow1]}pkts and {flow2} {singleIfFlows[flow2]} {pkt_count[flow2]}pkts")
                    parsed_data[flow1].extend(parsed_data[flow2])
                    parsed_data.pop(flow2)
                    parsed_data[flow1].sort()
                    # for packet in parsed_data[flow1]:
                    #     print(packet)
                    singleIfFlows[flow1]=[] # delete the ifname so we can avoid trying to match this one again
                    singleIfFlows[flow2]=[] # delete the ifname so we can avoid trying to match this one again
                    break

    print(f"{len(parsed_data)} flows found")

    # for flow in parsed_data:
    #     print(f"{flow} {pkt_count[flow]}")

    return parsed_data


def getProto(flow_id):
    return flow_id.split()[0]

def getSrc(flow_id):
    return flow_id.split()[1:3]

def getDst(flow_id):
    return flow_id.split()[4:]



# returns two arrays, one containing upstream flows, the other containing downstream flows, each indexed by flow_id
# for each flow, the following info:
#    [  0_array_of_dropped_pkt_timestamps (key=pktid), 
#       1_array_of_matched_pkt_latencies (key=timestamp), 
#       2_array_of_matched_pkt_sizes (key=timstamp), 
#       3_count_of_matched_pkts_unmatched_pkts_and_dropped_pkts, 
#       4_count_of_unmatched_packets, 
#       5_array_of_receive_side_dscp_counts (key=DSCP), 
#       6_array_of_receive_side_ecn_counts (key=ECN), 
#       7_array_of_sanctioned_pkt_latencies (key=timestamp),
#       8_array_of_sizes_of_CE_marked_pkts (key=timestamp) ]
def parse_data(parsed_data, if_name):
    # new 2 way implememtation, pass in NSI-side if_name
    # print(if_name)
    up_data = {}
    down_data = {}
    for flow in parsed_data:

        # at this point, we don't know whether this is an upstream flow or a downstream flow, so generate two copies of the output data, and select the correct one at the end
        pkt_times = {}
        pkt_times2 = {}
        latency1 = {}
        latency2 = {}
        dscp1 = {}
        dscp2 = {}
        ecn1 = {}
        ecn2 = {}
        pkt_size1 = {}
        pkt_size2 = {}
        dscp31 = {}
        dscp32 = {}
        CE_size1 = {}
        CE_size2 = {}
        total_packets1 = 0
        total_packets2 = 0

        x = parsed_data[flow]

        # process all the packets for the flow
        for i in range(len(x)):
            data = list(x[i])
            # data[0] is the timestamp
            # data[1] is the interface_name
            # data[2] is the DSCP
            # data[3] is the ECN
            # data[4] is the IP id
            # data[5] is the frame length
            # data[6] is the IP payload

            if (len(data) >= 7):                            # only use packets that have all fields

                packet_id = data[6]
                
                if (data[1]==if_name):                  #if this is a NSI-side packet
                    if (packet_id in pkt_times2): #if we've already found the CMCI-side packet
                        index1 = float(data[0])                 # use NSI-side timestamp as index1
                        index2=pkt_times2[packet_id]     # use CMCI-side timestamp as index2
                        
                        # handle the case where two packets have the same timestamp
                        while (index1 in latency1):
                            index1 += 0.0000000001
                        while (index2 in latency2):
                            index2 += 0.0000000001

                        latency1[index1] = -(float(data[0]) - pkt_times2[packet_id])      # store time delta as corresponding to NSI packet arrival time
                        latency2[index2] = latency1[index1]
                        pkt_size1[index1] = data[5]
                        pkt_size2[index2] = pkt_size1[index1]
                        if (int(data[2]) == 3) and ((int(data[3]) == 1) or (int(data[3] == 3))): # if DSCP=3 and either ECT1 or CE
                            dscp31[index1] = float(data[0]) - pkt_times2[packet_id]
                            dscp32[index2] = dscp31[index1]
                        if int(data[3]) == 3:
                            CE_size1[index1] = data[5]
                            CE_size2[index2] = CE_size1[index1]
                        pkt_times2.pop(packet_id)                       # match found, delete packet_time array entry
                        total_packets2 += 1
                        # count DSCP and ECN values on NSI packets
                        try:
                            dscp2[data[2]] += 1
                        except KeyError:
                            dscp2[data[2]] = 1
                        try:
                            ecn2[data[3]] += 1
                        except KeyError:
                            ecn2[data[3]] = 1
                        # print(f"N\t{packet_id}\t{index2}\t{index1}\t{data[1]}")
                    else:
                        pkt_times[packet_id] = float(data[0]) # store the NSI-side timestamp
                        # print(f"n\t{packet_id}\t{data[0]}\t{data[1]}")


                else: #this is an CMCI-side packet
                    if (packet_id in pkt_times):                    # if a matching NSI-side packet has already been found
                        index1=pkt_times[packet_id]                 # use NSI-side timestamp as index1
                        index2 = float(data[0])                            # use CMCI-side timestamp as index2

                        # handle the case where two packets have the same timestamp
                        while (index1 in latency1):
                            index1 += 0.0000000001
                        while (index2 in latency2):
                            index2 += 0.0000000001

                        latency1[index1] = float(data[0]) - pkt_times[packet_id]  # store time delta as corresponding to NSI packet arrival time
                        latency2[index2] = latency1[index1]
                        pkt_size1[index1] = data[5]
                        pkt_size2[index2] = pkt_size1[index1]
                        if (int(data[2]) == 3) and ((int(data[3]) == 1) or (int(data[3] == 3))): # if DSCP=3 and either ECT1 or CE
                            dscp31[index1] = float(data[0]) - pkt_times[packet_id]
                            dscp32[index2] = dscp31[index1]
                        if int(data[3]) == 3:
                            CE_size1[index1] = data[5]
                            CE_size2[index2] = CE_size1[index1]
                        pkt_times.pop(packet_id)                        # match found, delete packet_time array entry
                        total_packets1 += 1
                        # count DSCP and ECN values on CMCI packets
                        try:
                            dscp1[data[2]] += 1
                        except KeyError:
                            dscp1[data[2]] = 1
                        try:
                            ecn1[data[3]] += 1
                        except KeyError:
                            ecn1[data[3]] = 1
                        # print(f"C\t{packet_id}\t{index1}\t{index2}\t{data[1]}")

                    else:
                        pkt_times2[packet_id] = float(data[0]) #store the CMCI-side timestamp
                        # print(f"c\t{packet_id}\t{data[0]}\t{data[1]}")


        # now determine whether the flow was upstream or downstream and store the correct output data
        latency_a = np.array(list(latency1.values()))
        if np.size(latency_a) > 0:
            #print(f"{flow}  -- {len(latency1)} pkts - {np.mean(latency_a)}")
            if np.mean(latency_a) > 0:  # if downstream ...
                # for downstream flow, use latency1, dscp1, ecn1, pkt_times, etc.

                # pkt_times holds the remaining packets seen on NSI but not on CMCI
                # pkt_times2 holds the remaining packets seen on CMCI but not on NSI

                # since the capture interfaces don't start up at the same time, there could be packets at start or end that shouldn't be counted as unmatched_packets or dropped_packets
                # clean these arrays by finding:
                #       the first matched pkt in latency1: the entry with the lowest value of latency1 keys (i.e. NSI_timestamp), and calculate its CMCI_timestamp
                #       the last matched pkt in latency1: the entry with the highest value of latency1 keys (i.e. NSI_timestamp), and calculate its CMCI_timestamp

                # first_match_nsi_timestamp = min(latency1.keys())
                # last_match_nsi_timestamp = max(latency1.keys())
                ## faster way to find min and max with one traversal
                first_match_nsi_timestamp = last_match_nsi_timestamp = next(iter(latency1.keys()))
                for key in latency1.keys():
                    if key < first_match_nsi_timestamp:
                        first_match_nsi_timestamp = key
                    if key > last_match_nsi_timestamp:
                        last_match_nsi_timestamp = key

                first_match_cmci_timestamp = first_match_nsi_timestamp + float(latency1[first_match_nsi_timestamp])
                last_match_cmci_timestamp = last_match_nsi_timestamp + float(latency1[last_match_nsi_timestamp])

                # remove any pkt_times packets that have a NSI_timestamp before the first match or after the last match
                for packet_id in list(pkt_times):
                    if (float(pkt_times[packet_id]) < first_match_nsi_timestamp) or (float(pkt_times[packet_id]) > last_match_nsi_timestamp):
                        pkt_times.pop(packet_id)
                # the remaining packets in pkt_times were actually dropped
                dropped_packets = len(pkt_times)

                # remove any pkt_times2 packets that have a CMCI_timestamp before the first match or after the last match
                for packet_id in list(pkt_times2):
                    if (float(pkt_times2[packet_id]) < first_match_cmci_timestamp) or (float(pkt_times2[packet_id]) > last_match_cmci_timestamp):
                        pkt_times2.pop(packet_id)
                # the remaining packets in pkt_times2 were actually unmatched (i.e. "missing")
                unmatched_packets = len(pkt_times2)

                if (unmatched_packets > 0):
                    print(f'{flow} Missing packet timestamps: {pkt_times2.values()}')


                # add the data about this flow to the downstream flows list
                down_data[flow] = [pkt_times, latency1, pkt_size1, total_packets1 + unmatched_packets + dropped_packets, unmatched_packets, dscp1, ecn1, dscp31, CE_size1]
 
            else:  # upstream ...
                # for upstream flow, use latency2, dscp2, ecn2, pkt_times2 & flip sign to make the latency values positive

                for key in latency2:
                    latency2[key] = -1 * latency2[key]

                # pkt_times holds the remaining packets seen on NSI but not on CMCI
                # pkt_times2 holds the remaining packets seen on CMCI but not on NSI

                # since the capture interfaces don't start up at the same time, there could be packets at start or end that shouldn't be counted as unmatched_packets or dropped_packets
                # clean these arrays by finding:
                #       the first matched pkt in latency2: the entry with the lowest value of latency2 keys (i.e. CMCI_timestamp), and calculate its NSI_timestamp
                #       the last matched pkt in latency2: the entry with the highest value of latency2 keys (i.e. CMCI_timestamp), and calculate its NSI_timestamp
                first_match_cmci_timestamp = min(latency2.keys())
                first_match_nsi_timestamp = first_match_cmci_timestamp + float(latency2[first_match_cmci_timestamp])
                last_match_cmci_timestamp = max(latency2.keys())
                last_match_nsi_timestamp = last_match_cmci_timestamp + float(latency2[last_match_cmci_timestamp])

                # remove any pkt_times2 packets that have a CMCI_timestamp before the first match or after the last match                
                for packet_id in list(pkt_times2):
                    if (float(pkt_times2[packet_id]) > last_match_cmci_timestamp) or (float(pkt_times2[packet_id]) < first_match_cmci_timestamp):
                        pkt_times2.pop(packet_id)
                # the remaining packets in pkt_times2 were actually dropped
                dropped_packets = len(pkt_times2)

                # remove any pkt_times packets that have a NSI_timestamp before the first match or after the last match
                for packet_id in list(pkt_times):
                    if (float(pkt_times[packet_id]) < first_match_nsi_timestamp) or (float(pkt_times[packet_id]) > last_match_nsi_timestamp):
                        pkt_times.pop(packet_id)           
                # the remaining packets in pkt_times were actually unmatched (shown as "missing")
                unmatched_packets = len(pkt_times)

                if (unmatched_packets > 0):
                    print(f'{flow} Missing packet timestamps: {pkt_times.values()}')


                # add the data about this flow to the upstream flows list
                up_data[flow] = [pkt_times2, latency2, pkt_size2, total_packets2 + unmatched_packets + dropped_packets, unmatched_packets, dscp2, ecn2, dscp32, CE_size2]
        else:
            # only one interface captured packets these flows, flow will be excluded from plots
            if len(pkt_times) > 0:
                print(f'Flow only captured on NSI interface, not included in plots: {flow} -- {len(pkt_times)} pkts')
            elif len(pkt_times2) > 0:
                print(f'Flow only captured on CMCI interface, not included in plots: {flow} -- {len(pkt_times2)} pkts')


    return up_data, down_data

# takes in the latency dictionary for a certain hash key to process into timeseries data
# returns : evenly spaced times and latency at each time. each array is length n
def get_latency(latency):

    latency_array=np.array(list(latency.values()))
    times = np.array(list(latency.keys()), dtype=float)
    indices = times.argsort()
    times = times[indices]
    latency_array = latency_array[indices]



    return times, latency_array


# takes in latency values at given time array from the get_latency function return
# returns the array pair for a CCDF of latency values
def get_ccdf(latency_array):

    # bins = np.size(latency_array)**(1/3) # freedman-diaconis bin number
    # y, yedges = np.histogram(latency_array, int(bins)) # creating histogram data
    # y = y / np.size(latency_array) # counts fn -> PDF
    # y = 1 - np.cumsum(y) # PDF -> CCDF

    yedges=np.sort(latency_array)
    y = np.linspace(1,0,np.size(latency_array))

    return yedges, y



# pkt_size is a dict {'timestamp' : frame length}
# returns tuple of (throughput array (bps), integer end time, integer start time)
def get_throughput(pkt_size : dict, interval : float):
    # interval (in seconds) for averaging
    bps = {}
    start_t=sys.maxsize
    end_t = 0
    for timestamp in pkt_size: # adding all throughput traffic within an interval
        try:
            bps[int(float(timestamp) / interval)] += float(pkt_size[timestamp])
        except KeyError:
            bps[int(float(timestamp) / interval)] = float(pkt_size[timestamp])
    for t in bps:
        bps[t] *= float(8/interval) # convert bytes per interval to bps
        if (int(t)<start_t): # find starting time
            start_t=int(t)
        if (int(t) > end_t): # find ending time
            end_t = int(t)
    bps_array = []
    for t in range(0, end_t + 1):
        try:
            bps_val = bps[t]
        except KeyError:
            bps_val = 0
        bps_array.append(bps_val)
    bps_array = np.array(bps_array)
    return bps_array, end_t, start_t


def calculate_stats(pkt_drop_times, latency, pkt_size, unmatched_packets, t_steadystate):

    droppedPkts = 0
    missingPkts = 0

    # in stats overlay display number and % loss (only CMCI)
    if (len(pkt_drop_times) != 0):
        droppedPkts = len(pkt_drop_times)

    if (unmatched_packets > 0):
        missingPkts = unmatched_packets

    # Calculate latency percentiles
    latency_array=np.array(list(latency.values()))
    # p=np.percentile(latency_array,[0, 90, 99, 99.9])
    p=np.percentile(latency_array,[0, 0.1, 1, 10, 50, 90, 99, 99.9, 100])*1000

    # Calculate steady state mean bps over 1 second intervals and P10 of bps 
    bps_array,end_t,start_t = get_throughput(pkt_size, 1.0)

    if (t_steadystate + start_t < end_t):
        mean_bps=np.mean(bps_array[(t_steadystate + start_t):(end_t+1)])
        p10_bps=np.percentile(bps_array[(t_steadystate + start_t):(end_t+1)],0.1)
    else:
        mean_bps=np.mean(bps_array)
        p10_bps=0

    # Calculate bps over 100ms intervals
    bps_array,end_t,start_t = get_throughput(pkt_size, 0.1)
    if (len(bps_array)>int(t_steadystate * (1 / 0.1))):
        mean_bps_new = np.mean(bps_array[int(t_steadystate * (1 / 0.1)):])
    else:
        mean_bps_new = 0

    # 90% threshold of steady state mean
    bps_array_init = bps_array[bps_array > 0]

    time_passed = 0
    while (bps_array_init[time_passed] < (0.9*mean_bps_new)):
        time_passed += 1

    return [ *list(p),
            (p[6]-p[2]), (p[7]-p[1]),
            droppedPkts, missingPkts, 
            mean_bps/1000000, p10_bps/1000000, 100*p10_bps/mean_bps, time_passed * 100]




def multiflow_plot_and_csv(parsed_data, upstream, t_steadystate, dir_ext):
    if upstream:
        no_flows_found_string="Zero upstream flows found - exiting"
        plot_title="Throughput Timeseries Upstream"
        plot_filename='Aggregate_thruput_upstream.pdf'
        csv_filename='summary_table_upstream.csv'
    else:
        no_flows_found_string="Zero downstream flows found - exiting"
        plot_title="Throughput Timeseries Downstream"
        plot_filename='Aggregate_thruput_downstream.pdf'
        csv_filename='summary_table_downstream.csv'

    # setting up matplotlib defaults
    fig = plt.figure(figsize=(16,9))
    ax = fig.add_axes([0.1,0.2,0.75,0.7])

    if (len(parsed_data) > 0):
        # loop thru flows to extract throughput data per flow
        y = []
        mean  = [] # might be unnecessary
        end_t = []
        interval=0.1

        for flow in parsed_data:
            #print(flow, len(parsed_data[flow][0]),len(parsed_data[flow][1]), len(parsed_data[flow][2]),parsed_data[flow][3])
            #_y, _end_t, _start_t = get_throughput(parsed_data[flow][2], interval)

            #calculate egress data rate for the flow
            latency = parsed_data[flow][1]
            pkt_size = parsed_data[flow][2]
            e_pkt_size = {}
            for ingress_ts in pkt_size:
                egress_ts = ingress_ts + latency[ingress_ts]
                while (egress_ts in e_pkt_size): # hack to handle egress_ts conflicts
                    egress_ts += 0.0000000001
                e_pkt_size[egress_ts] = pkt_size[ingress_ts]
            _y, _end_t, _start_t = get_throughput(e_pkt_size, interval)

            if (len(_y) > int(t_steadystate * (1 / interval))):
                _mean = np.mean(_y[int(t_steadystate * (1 / interval)):])
            else:
                _mean =0
            y.append(_y)
            mean.append(_mean)
            end_t.append(_end_t)

        # bail out if no plottable flows are found
        if len(end_t) < 1:
            print(no_flows_found_string)
            quit()

        # init time scale data 
        end_t = max(end_t)
        x = np.arange(0, end_t + 1, 1)*interval

        # fixing array sizing 
        y_resized = []
        for flow in y:
            if len(flow) <= end_t:
                for i in range(end_t - len(flow) + 1):
                    flow = np.append(flow, 0)
                y_resized.append(flow)
            else:
                y_resized.append(flow)

        # total throughput
        y_sum = np.zeros(np.size(x), dtype=float)
        for flow in y_resized:
            y_sum += flow

        cmap = plt.get_cmap('viridis')

        colors = cmap(np.linspace(0, 1, len(y)))

        # plotting throughput
        for flow in range(len(y_resized)):
            ax.plot(x, y_resized[flow] / 1000000, c=colors[flow], lw=0.5, label=f'Flow {flow + 1}')
        ax.plot(x, y_sum / 1000000, c='r', lw=1, label='Total')
        ax.grid()
        ax.set_title(plot_title)
        ax.set(xlabel="Time (s)", ylabel="Mbps")
        legend = ax.legend(loc='center left', bbox_to_anchor=(1, 0.5))

        for obj in legend.legend_handles:
            obj.set_linewidth(2.5)

        #######
        # adding logo
        logo = image.imread(os.path.join(sys.path[0],'logo.png'))
        ax2 = fig.add_axes([0.78,0,0.2,0.2])
        ax2.imshow(logo)
        ax2.axis('off')

        #######
        # adding title
        fig.suptitle(plot_filename, fontsize=20)

        #######
        # adding dirname
        dirname = dir_ext.split('/')[-1]
        fig.text(0.1,0.1,dirname)


        # save plot as figure
        plt.savefig(dir_ext +  '/' + plot_filename, format='pdf')

        # SUMMARY TABLE

        csv_labels = ['Flow Number','Flow ID','Packet Delay P0 (ms)', 'Packet Delay P90 (ms)', 'Packet Delay P99 (ms)', 'Packet Delay P99.9 (ms)',  
                    'PDV P99 (ms)', 'PDV P99.9 (ms)', 'Mean data rate (Mbps)', 'P10 of data rate (Mbps)', 'P10 % of mean', 'Ramp time to 90% Tput (ms)', 
                    'Num Packets', 'Num NotECT','Num ECT0', 'Num ECT1', 'Num CE','Dropped Packets', 'Missing Packets']

        with open(dir_ext + '/' + csv_filename,'w') as csv_out:
            writer = csv.writer(csv_out)
            writer.writerow(csv_labels)

            keys = list(parsed_data.keys())

            for flow in range(len(y)):
                flow_data = parsed_data[keys[flow]]
                pkt_drop_times = flow_data[0]
                latency = flow_data[1]
                if not latency: continue
                pkt_size = flow_data[2]
                total_packets = flow_data[3]
                unmatched_packets = flow_data[4]
                ecn=flow_data[6]
                ECN=[0,0,0,0]
                for key in ecn:
                    ECN[int(key)]=ecn[key]
                stats=calculate_stats(pkt_drop_times, latency, pkt_size, unmatched_packets, t_steadystate)
                stats=["{:.3f}".format(value) if i in list(range(0,11))+list(range(13,15)) else str(int(value)) if i in [11, 12, 16] else "{:.1f}".format(value) for i,value in enumerate(stats)]
                data=[flow+1, keys[flow], stats[0], stats[5], stats[6], stats[7], stats[9], stats[10], stats[13],
                    stats[14], stats[15], stats[16], total_packets, ECN[0], ECN[2], ECN[1], ECN[3], stats[11], stats[12]]
                writer.writerow(data)
            csv_out.close()



# takes in each field from parsed_flow dictionary
def plot_singleflow(pkt_drop_times, latency, pkt_size, total_packets, dscp, ecn, unmatched_packets, t_steadystate, f_name, dir_ext, dscp3, CE_size, flow):
    # matplotlib defaults
    fig = plt.figure(figsize=(16,9))

    #######
    # latency timeseries plot
    ax0 = fig.add_axes([0.1,0.7,0.85,0.15])

    times, latency_array = get_latency(latency)

    ax0.set_xlim([0,np.max(times)])

    # dropped packets
    if (len(pkt_drop_times) != 0):
        dropped_pkts = list(pkt_drop_times.values())
        zero_arr = [0] * len(dropped_pkts)
        ax0.plot(dropped_pkts, zero_arr, '|', c='r', markersize=5, label='Dropped Packets')

    # sanctioned packets
    if (len(dscp3) != 0):
        times3, latency3 = get_latency(dscp3)
        ax0.plot(times3, 1000 * latency3, 'o', c='m', markersize=1, label='Sanctioned Packets')

    # all packets
    ax0.plot(times, 1000 * latency_array, c='b', lw=0.25)
    ax0.set_ylim(ymin=0)

    ax0.grid()

    if (len(pkt_drop_times)+len(dscp3) != 0):
        ax0.legend()

    ax0.set_title("Latency Timeseries")
    ax0.set(ylabel="Latency (ms)")
    #labels0 = ax0.get_xticks().tolist()
    #ax0.xaxis.set_major_locator(mticker.FixedLocator(labels0))
    #ax0.set_xticklabels([(str("{:.0f}".format(x)) + 's') for x in labels0])
    ax0.xaxis.set_major_formatter('{x:g}s')

    #######
    # CCDF
    ax1 = fig.add_axes([0.1,0.45,0.35,0.15])

    yedges, y = get_ccdf(latency_array)

    ax1.semilogy(1000*yedges, y, label="ccdf", c='r') # plot setup
    ax1.grid()
    ax1.set_title("CCDF of Packet Latency")
    ax1.set(xlabel="Packet Latency (ms)", ylabel="Percentile")
    tick_loc = []
    for i in range(5):
        tick_loc.append(10 ** (-i))
    ax1.set_yticks(tick_loc)
    ax1.set_yticklabels(['P0', 'P90', 'P99', 'P99.9', 'P99.99'])

    # labels1 = ax1.get_xticks().tolist()
    # ax1.xaxis.set_major_locator(mticker.FixedLocator(labels1))

    # if (labels1[-1] * 1000 < 10):
    #     ax1.set_xticklabels([("{:.1f}".format(x*1000)) for x in labels1])
    # else: ax1.set_xticklabels([("{:.0f}".format(x*1000)) for x in labels1])
    ax1.xaxis.set_major_formatter('{x:g}')

    #######
    # bps timeseries
    ax2 = fig.add_axes([0.1,0.2,0.85,0.15])
    interval=0.1
    # calculate ingress data rate over time
    bps_array, end_t, start_t = get_throughput(pkt_size, interval)
    if (len(bps_array) > int(t_steadystate * (1 / interval))):
        mean_bps = np.mean(bps_array[int(t_steadystate * (1 / interval)):])
    else:
        mean_bps=0
    times = np.arange(0, end_t, interval)
    mbps_array = np.array(bps_array) / 1000000

    if (len(bps_array)<2):
        return

    #calculate egress data rate over time
    pkt_size2={}
    for ingress_ts in pkt_size:
        egress_ts = ingress_ts + latency[ingress_ts]
        while (egress_ts in pkt_size2): # hack to handle egress_ts conflicts
            egress_ts += 0.0000000001
        pkt_size2[egress_ts] = pkt_size[ingress_ts]
    e_bps_array, e_end_t, e_start_t = get_throughput(pkt_size2, interval)
    if (len(e_bps_array) > int(t_steadystate * (1 / interval))):
        e_mean_bps = np.mean(e_bps_array[int(t_steadystate * (1 / interval)):])
    else:
        e_mean_bps=0
    e_mbps_array = np.array(e_bps_array) / 1000000

    # calculate CE marking percentage over time
    #   note, this calculates CE-marking rate based on the *arrival* timestamp rather than the departure timestamp
    if (len(CE_size) > 0):
        ce_bps_array,ce_end_t,ce_start_t =get_throughput(CE_size,interval)
        ce_bps_array = np.pad(ce_bps_array,(0,max(0,len(bps_array)-len(ce_bps_array)))) # pad out the ce_bps_array with zeros to match the length of bps_array
        with warnings.catch_warnings():
            warnings.simplefilter("ignore") # suppress printing the RuntimeWarning caused by zeros in the bps_array
            ce_arr = np.divide(ce_bps_array,bps_array) # note this uses bps_array, not e_bps_array, since CE_size uses ingress timestamp
    else:
        ce_arr = np.zeros(np.size(mbps_array))

    # catch 1 off sizing issues in arrays
    max_index = min(np.size(times), np.size(mbps_array), np.size(ce_arr), np.size(e_mbps_array))
    times = times[:max_index]
    mbps_array = mbps_array[:max_index]
    e_mbps_array = e_mbps_array[:max_index]    
    ce_arr = ce_arr[:max_index]

    twin_ax2 = ax2.twinx()
    twin_ax2.plot(times, 100 * ce_arr, c='r', lw=0.75, label='% CE')
    twin_ax2.set_ylim([0,100])
    ax2.set_xlim([0,np.max(times)])
    ax2.plot(times, mbps_array, c='b', lw=0.75, label='ingress') # plot ingress rate in blue
    ax2.plot(times, e_mbps_array, c='0.5', lw=0.75, label='egress') # plot egress rate in 0.5 gray
    ax2.set_ylim(ymin=0)
    ax2.grid()
    ax2.legend()
    twin_ax2.legend()
    ax2.set_title("Mbps Timeseries")
    ax2.set(ylabel="Mbps")
    # labels2 = ax2.get_xticks().tolist()
    # ax2.xaxis.set_major_locator(mticker.FixedLocator(labels2))
    # ax2.set_xticklabels([(str("{:.0f}".format(x)) + 's') for x in labels2])
    ax2.xaxis.set_major_formatter('{x:g}s')
    
    #######
    # adding logo
    logo = image.imread(os.path.join(sys.path[0],'logo.png'))
    ax3 = fig.add_axes([0.78,0,0.2,0.2])
    ax3.imshow(logo)
    ax3.axis('off')

    #######
    # adding stats tables
    PD_labels = ['P0', 'P0.1','P1','P10','P50','P90', 'P99', 'P99.9', 'P100', 'P99 PDV', 'P99.9 PDV']
    DR_labels = ['Mean data rate', 'P10 of data rate', 'P10 % of mean', 'Ramp time to 90%']

    p = calculate_stats(pkt_drop_times, latency, pkt_size, unmatched_packets, t_steadystate)
    
    # format packet delay percentile strings
    PD_stats=np.array(["{:.3f}".format(i) + ' ms' for i in p[0:11]],ndmin=2).T

    droppedPktCnt = p[11]
    missingPktCnt = p[12]

    # format data rate related strings
    DR_stats = np.array([
            str("{:.1f}".format(p[13])) + ' Mbps', 
            str("{:.1f}".format(p[14])) + ' Mbps', 
            str("{:.1f}".format(p[15])) + '%', 
            str(p[16]) + ' ms'
            ],ndmin=2).T

    ax4 = fig.add_axes([0.52, 0.45, 0.07, 0.15])
    ax4.axis('off')
    ax4.axis('tight')
    t1 = ax4.table(cellText=PD_stats, rowLabels=PD_labels, loc='upper right')
    t1.set_fontsize(8)
    ax4.set_title('Packet Delay Statistics', fontsize=10, x=0.25) 

    ax6 = fig.add_axes([0.72, 0.6, 0.07, 0.1])
    t3 = ax6.table(cellText=DR_stats, rowLabels=DR_labels)
    t3.set_fontsize(8)
    ax6.axis('off')
    ax6.set_title('Data Rate Statistics', fontsize=10, x=-0.2, y=0)


    # will need a more complete fix
    DSCP_data = []
    DSCP_labels = []
    for key in dscp:
        DSCP_data.append([str(dscp[key]), str(round(100*dscp[key]/total_packets,2)) + "%"])
        DSCP_labels.append('DSCP ' + str(key))

    ECN_data = []
    ECN_labels = []
    for key in ecn:
        ECN_data.append([str(ecn[key]), str(round(100*ecn[key]/total_packets,2)) + "%"])
        key = int(key)
        if key == 0:
            ECN_labels.append('Not ECT')
        elif key == 1:
            ECN_labels.append('ECT1')
        elif key == 2:
            ECN_labels.append('ECT0')
        elif key == 3:
            ECN_labels.append('CE')

    ECN_labels.append('Drops')
    ECN_data.append([str(droppedPktCnt) , str(round(100*int(droppedPktCnt)/total_packets,2)) + "%"])

    ECN_labels.append('Missing')
    ECN_data.append([str(missingPktCnt) , str(round(100*int(missingPktCnt)/total_packets,2)) + "%"])

    ECN_labels.append('Total')
    ECN_data.append([total_packets, "100%"])

    DE_data = np.array(DSCP_data + ECN_data)
    DE_labels = DSCP_labels + ECN_labels


    ax7 = fig.add_axes([0.875, 0.6, 0.07, 0.1])
    t4 = ax7.table(cellText=DE_data, rowLabels=DE_labels)
    t4.set_fontsize(8)
    ax7.axis('off')
    ax7.set_title('Packet Counts', fontsize=10, x=0.275,  y=0)

    #######
    # adding title
    title = f_name
    fig.suptitle(title + ' ' + flow, fontsize=18)

    #######
    # adding dirname
    dirname = dir_ext.split('/')[-1]
    fig.text(0.1,0.1,dirname)

    # save plot as pdf
    plt.savefig(dir_ext + '/' + f_name, format='pdf')

    plt.close()


if __name__ == '__main__':
    print('Oops, this file only contains library helper functions. Try running another.')
