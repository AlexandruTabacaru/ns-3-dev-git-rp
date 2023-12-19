#!/usr/bin/env python3

# This is a basic plotting program that uses Python Matplotlib to plot three time-series:
# 1) Cubic primary foreground TCP throughput (cubic-throughput.dat)
# 2) Prague primary foreground TCP throughput (prague-throughput.dat)
# 3) Overlying DualPI2 bytes in queue (both C- and L- queues) (wired-dualpi2-bytes.dat)
#
# The output file is named 'l4s-wired.pdf'

import matplotlib

matplotlib.use("agg")
import os
import re
import sys

import matplotlib.pyplot as plt
import numpy as np

plotname = "l4s-wired.pdf"
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

f = open("wired-dualpi2-bytes.dat", "r")
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
fig, (ax2, ax3, ax5) = plt.subplots(
    nrows=3, figsize=(8, 11), sharex=False, sharey=False
)
fig.subplots_adjust(hspace=space_between_subplots)

ax2.plot(
    cubic_time,
    cubic_samples_plot,
    marker="",
    color="black",
    label="100ms throughput samples",
)
ax2.set_xlim(
    0,
)
ax2.set_ylim(0, 120)
ax2.set_xlabel("Time (s)")
ax2.set_ylabel("Throughput (Mbps)")
if add_horizontal_line_for_averaging:
    if len(cubic_samples_avg):
        cubic_avg = round(sum(cubic_samples_avg) / len(cubic_samples_avg), 1)
        cubic_label = r"Avg {} Mbps (2s < t < {}s)".format(cubic_avg, end_of_averaging_period)
        ax2.axhline(y=cubic_avg, color="r", linestyle="dashed", label=cubic_label)
ax2.set_title("Cubic throughput")
ax2.legend(loc="upper right", framealpha=1, prop={"size": 6})

ax3.plot(
    prague_time,
    prague_samples_plot,
    marker="",
    color="black",
    label="100ms throughput samples",
)
ax3.set_xlim(
    0,
)
ax3.set_ylim(0, 120)
ax3.set_xlabel("Time (s)")
ax3.set_ylabel("Throughput (Mbps)")
if add_horizontal_line_for_averaging:
    if len(prague_samples_avg):
        prague_avg = round(sum(prague_samples_avg) / len(prague_samples_avg), 1)
        prague_label = r"Avg {} Mbps (2s < t < {}s)".format(prague_avg, end_of_averaging_period)
        ax3.axhline(y=prague_avg, color="r", linestyle="dashed", label=prague_label)
ax3.set_title("Prague throughput")
ax3.legend(loc="upper right", prop={"size": 6})

ax5.plot(dualpi2_time, dualpi2_samples_plot, marker="", color="black", label="Bytes")
ax5.set_ylim(0, 600000)
ax5.set_xlabel("Time (s)")
ax5.set_ylabel("Bytes in DualPI2")
ax5.set_title("Bytes in DualPI2 queue")

fig.suptitle(title)
plt.savefig(plotname, format="pdf")
plt.close()
