#!/usr/bin/env python3

# This is a basic plotting program that uses Python Matplotlib to plot five time-series:
# 1) Wifi throughput (wifi-throughput.dat)
# 2) Cubic primary foreground TCP throughput (cubic-throughput.dat)
# 3) Prague primary foreground TCP throughput (prague-throughput.dat)
# 4) Wifi AC_BE queue bytes after dequeue (wifi-dequeue-bytes.dat)
# 5) Overlying DualPI2 bytes in queue (both C- and L- queues) (wifi-dualpi2-bytes.dat)
#
# The output file is named 'l4s-wifi.pdf'

import matplotlib

matplotlib.use("agg")
import os
import re
import sys

import matplotlib.pyplot as plt
import numpy as np

plotname = "l4s-wifi.pdf"
# Plot title will be any arguments passed to this script
if len(sys.argv) > 1 and sys.argv[1] is not None:
    title = " ".join(sys.argv[1:])
else:
    title = "Untitled"
space_between_subplots = 1.0

# Add a red dashed line to the plot for the average value between 2s and
# a user-defined end time (default 8s)
add_horizontal_line_for_averaging = False
end_of_averaging_period=8

f = open("wifi-throughput.dat", "r")
wifi_time = []
wifi_samples_plot = []
wifi_samples_avg = []
for line in f:
    columns = line.split()
    t = float(columns[0])
    wifi_time.append(t)
    wifi_samples_plot.append(float(columns[1]))
    if t >= 2 and t <= end_of_averaging_period:
        wifi_samples_avg.append(float(columns[1]))
f.close()

cubic_time = []
cubic_samples_plot = []
cubic_samples_avg = []
if os.path.exists("cubic-throughput.dat"):
    f = open("cubic-throughput.dat", "r")
    for line in f:
        columns = line.split()
        t = float(columns[0])
        cubic_time.append(t)
        cubic_samples_plot.append(float(columns[1]))
        if t >= 2 and t <= end_of_averaging_period:
            cubic_samples_avg.append(float(columns[1]))
    f.close()

cubic_rtt_time = []
cubic_rtt_samples_plot = []
if os.path.exists("cubic-rtt.dat"):
    f = open("cubic-rtt.dat", "r")
    for line in f:
        columns = line.split()
        t = float(columns[0])
        cubic_rtt_time.append(t)
        cubic_rtt_samples_plot.append(float(columns[1]))
    f.close()

cubic_cwnd_time = []
cubic_cwnd_samples_plot = []
if os.path.exists("cubic-cwnd.dat"):
    f = open("cubic-cwnd.dat", "r")
    for line in f:
        columns = line.split()
        t = float(columns[0])
        cubic_cwnd_time.append(t)
        cubic_cwnd_samples_plot.append(float(columns[1])/1448)
    f.close()

prague_time = []
prague_samples_plot = []
prague_samples_avg = []
if os.path.exists("prague-throughput.dat"):
    f = open("prague-throughput.dat", "r")
    for line in f:
        columns = line.split()
        t = float(columns[0])
        prague_time.append(t)
        prague_samples_plot.append(float(columns[1]))
        if t >= 2 and t <= end_of_averaging_period:
            prague_samples_avg.append(float(columns[1]))
    f.close()

prague_rtt_time = []
prague_rtt_samples_plot = []
if os.path.exists("prague-rtt.dat"):
    f = open("prague-rtt.dat", "r")
    for line in f:
        columns = line.split()
        t = float(columns[0])
        prague_rtt_time.append(t)
        prague_rtt_samples_plot.append(float(columns[1]))
    f.close()

prague_cwnd_time = []
prague_cwnd_samples_plot = []
if os.path.exists("prague-cwnd.dat"):
    f = open("prague-cwnd.dat", "r")
    for line in f:
        columns = line.split()
        t = float(columns[0])
        prague_cwnd_time.append(t)
        prague_cwnd_samples_plot.append(float(columns[1])/1448)
    f.close()

f = open("wifi-queue-bytes.dat", "r")
bytes_time = []
bytes_samples_plot = []
bytes_samples_avg = []
for line in f:
    columns = line.split()
    t = float(columns[0])
    bytes_time.append(t)
    bytes_samples_plot.append(float(columns[1]))
    if t >= 2 and t <= end_of_averaging_period:
        bytes_samples_avg.append(float(columns[1]))
f.close()

f = open("wifi-dualpi2-bytes.dat", "r")
dualpi2_time = []
dualpi2_samples_plot = []
dualpi2_samples_avg = []
for line in f:
    columns = line.split()
    t = float(columns[0])
    dualpi2_time.append(t)
    dualpi2_samples_plot.append(float(columns[1]))
    if t >= 2 and t <= end_of_averaging_period:
        dualpi2_samples_avg.append(float(columns[1]))
f.close()

# Create subplots and orient the page in portrait mode
fig, (ax1, ax2, ax3, ax4, ax5, ax6, ax7, ax8, ax9) = plt.subplots(
    nrows=9, figsize=(8, 11), sharex=False, sharey=False
)
fig.subplots_adjust(hspace=space_between_subplots)
ax1.plot(
    wifi_time,
    wifi_samples_plot,
    marker="",
    color="black",
)
ax1.set_ylim(
    0,
)
#ax1.set_xlabel("Time (s)", fontsize=8)
ax1.set_ylabel("Throughput (Mbps)", fontsize=8)
if add_horizontal_line_for_averaging:
    wifi_avg = round(sum(wifi_samples_avg) / len(wifi_samples_avg), 1)
    wifi_label = r"Avg {} Mbps (2s < t < {}s)".format(wifi_avg, end_of_averaging_period)
    ax1.axhline(y=wifi_avg, color="r", linestyle="dashed", label=wifi_label)
ax1.set_title("Access point sending rate (all flows, measured at MAC layer)", fontsize=8)
ax1.tick_params(axis='x', labelsize=8)
ax1.tick_params(axis='y', labelsize=8)
#ax1.legend(loc="upper right", framealpha=1, prop={"size": 6})

ax2.plot(
    cubic_time,
    cubic_samples_plot,
    marker="",
    color="black",
)
ax2.set_ylim(
    0,
)
#ax2.set_xlabel("Time (s)", fontsize=8)
ax2.set_ylabel("Throughput\n(Mbps)", fontsize=8)
if add_horizontal_line_for_averaging:
    if len(cubic_samples_avg):
        cubic_avg = round(sum(cubic_samples_avg) / len(cubic_samples_avg), 1)
        cubic_label = r"Avg {} Mbps (2s < t < {}s)".format(cubic_avg, end_of_averaging_period)
        ax2.axhline(y=cubic_avg, color="r", linestyle="dashed", label=cubic_label)
ax2.set_title("Cubic throughput at receiver", fontsize=8)
ax2.tick_params(axis='x', labelsize=8)
ax2.tick_params(axis='y', labelsize=8)
#ax2.legend(loc="upper right", framealpha=1, prop={"size": 6})

ax3.plot(
    cubic_rtt_time,
    cubic_rtt_samples_plot,
    marker="",
    color="black",
)
ax3.set_ylim(
    0,
)
#ax3.set_xlabel("Time (s)", fontsize=8)
ax3.set_ylabel("Rtt (ms)", fontsize=8)
ax3.set_title("Cubic unsmoothed rtt", fontsize=8)
ax3.tick_params(axis='x', labelsize=8)
ax3.tick_params(axis='y', labelsize=8)
#ax3.legend(loc="upper right", framealpha=1, prop={"size": 6})

ax4.plot(
    cubic_cwnd_time,
    cubic_cwnd_samples_plot,
    marker="",
    color="black",
)
ax4.set_ylim(
    0,
)
#ax4.set_xlabel("Time (s)", fontsize=8)
ax4.set_ylabel("cwnd (segments)", fontsize=8)
ax4.set_title("Cubic cwnd (segments)", fontsize=8)
ax4.tick_params(axis='x', labelsize=8)
ax4.tick_params(axis='y', labelsize=8)
#ax4.legend(loc="upper right", framealpha=1, prop={"size": 6})

ax5.plot(
    prague_time,
    prague_samples_plot,
    marker="",
    color="black",
)
ax5.set_ylim(
    0,
)
#ax5.set_ylim(0, 120)
#ax5.set_xlabel("Time (s)", fontsize=8)
ax5.set_ylabel("Throughput (Mbps)", fontsize=8)
if add_horizontal_line_for_averaging:
    if len(prague_samples_avg):
        prague_avg = round(sum(prague_samples_avg) / len(prague_samples_avg), 1)
        prague_label = r"Avg {} Mbps (2s < t < {}s)".format(prague_avg, end_of_averaging_period)
        ax3.axhline(y=prague_avg, color="r", linestyle="dashed", label=prague_label)
ax5.set_title("Prague throughput at receiver", fontsize=8)
ax5.tick_params(axis='x', labelsize=8)
ax5.tick_params(axis='y', labelsize=8)
#ax5.legend(loc="upper right", prop={"size": 6})

ax6.plot(
    prague_rtt_time,
    prague_rtt_samples_plot,
    marker="",
    color="black",
)
ax6.set_ylim(
    0,
)
#ax6.set_xlabel("Time (s)", fontsize=8)
ax6.set_ylabel("Rtt (ms)", fontsize=8)
ax6.set_title("Prague unsmoothed rtt", fontsize=8)
ax6.tick_params(axis='x', labelsize=8)
ax6.tick_params(axis='y', labelsize=8)
#ax6.legend(loc="upper right", framealpha=1, prop={"size": 6})

ax7.plot(
    prague_cwnd_time,
    prague_cwnd_samples_plot,
    marker="",
    color="black",
)
ax7.set_ylim(
    0,
)
#ax7.set_xlabel("Time (s)", fontsize=8)
ax7.set_ylabel("cwnd (segments)", fontsize=8)
ax7.set_title("Prague cwnd (segments)", fontsize=8)
ax7.tick_params(axis='x', labelsize=8)
ax7.tick_params(axis='y', labelsize=8)
#ax7.legend(loc="upper right", framealpha=1, prop={"size": 6})

ax8.plot(bytes_time, bytes_samples_plot, marker="", color="black")
#ax8.set_ylim(0, 600000)
#ax8.set_xlabel("Time (s)", fontsize=8)
ax8.set_ylabel("Bytes in AC_BE", fontsize=8)
ax8.tick_params(axis='x', labelsize=8)
ax8.tick_params(axis='y', labelsize=8)
ax8.set_title("Bytes in Wi-Fi device AC_BE queue", fontsize=8)

ax9.plot(dualpi2_time, dualpi2_samples_plot, marker="", color="black")
#ax9.set_ylim(0, 600000)
ax9.set_xlabel("Time (s)", fontsize=8)
ax9.tick_params(axis='x', labelsize=8)
ax9.tick_params(axis='y', labelsize=8)
ax9.set_ylabel("Bytes in DualPI2", fontsize=8)
ax9.set_title("Bytes in overlying DualPI2 AC_BE queue", fontsize=8)

fig.suptitle(title)
plt.savefig(plotname, format="pdf")
plt.close()
