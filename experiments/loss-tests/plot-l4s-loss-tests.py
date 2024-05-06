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
# - If invoked like this:  plot-l4s-loss-tests.py testName no-loss <...title string>
#   then the testName 'no-loss' will be used to open dat files
# - If not invoked with a test name; i.e.:  plot-l4s-loss-tests.py <...title string>
#   then the title string is all arguments 1 and beyond 
title = ""
testName = ""
if (sys.argv[1] == "testName"):
    testName = sys.argv[2]
    title += " ".join(sys.argv[3:])
    plotname = "l4s-wired." + testName + ".pdf"
elif len(sys.argv) > 1 and sys.argv[1] is not None:
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
if testName != "":
    fileName = "cubic-throughput." + testName + ".dat"
else:
    fileName = "cubic-throughput.dat"
if os.path.exists(fileName):
    f = open(fileName, "r")
    for line in f:
        columns = line.split()
        t = float(columns[0])
        cubic_time.append(t)
        cubic_samples_plot.append(float(columns[1]))
        if t >= 2 and t <= end_of_averaging_period:
            cubic_samples_avg.append(float(columns[1]))
    f.close()

cubic_cwnd_time = []
cubic_cwnd_samples_plot = []
if testName != "":
    fileName = "cubic-cwnd." + testName + ".dat"
else:
    fileName = "cubic-cwnd.dat"
if os.path.exists(fileName):
    f = open(fileName, "r")
    for line in f:
        columns = line.split()
        t = float(columns[0])
        cubic_cwnd_time.append(t)
        cubic_cwnd_samples_plot.append(float(columns[1])/1448)
    f.close()

cubic_ssthresh_time = []
cubic_ssthresh_samples_plot = []
if testName != "":
    fileName = "cubic-ssthresh." + testName + ".dat"
else:
    fileName = "cubic-ssthresh.dat"
if os.path.exists(fileName):
    f = open(fileName, "r")
    for line in f:
        columns = line.split()
        if columns[1] == "4294967295":
            continue
        t = float(columns[0])
        cubic_ssthresh_time.append(t)
        cubic_ssthresh_samples_plot.append(float(columns[1])/1448)
    f.close()

prague_time = []
prague_samples_plot = []
prague_samples_avg = []
if testName != "":
    fileName = "prague-throughput." + testName + ".dat"
else:
    fileName = "prague-throughput.dat"
if os.path.exists(fileName):
    f = open(fileName, "r")
    for line in f:
        columns = line.split()
        t = float(columns[0])
        prague_time.append(t)
        prague_samples_plot.append(float(columns[1]))
        if t >= 2 and t <= end_of_averaging_period:
           prague_samples_avg.append(float(columns[1]))
    f.close()

prague_cwnd_time = []
prague_cwnd_samples_plot = []
if testName != "":
    fileName = "prague-cwnd." + testName + ".dat"
else:
    fileName = "prague-cwnd.dat"
if os.path.exists(fileName):
    f = open(fileName, "r")
    for line in f:
        columns = line.split()
        t = float(columns[0])
        prague_cwnd_time.append(t)
        prague_cwnd_samples_plot.append(float(columns[1])/1448)
    f.close()

prague_ssthresh_time = []
prague_ssthresh_samples_plot = []
if testName != "":
    fileName = "prague-ssthresh." + testName + ".dat"
else:
    fileName = "prague-ssthresh.dat"
if os.path.exists(fileName):
    f = open(fileName, "r")
    for line in f:
        columns = line.split()
        if columns[1] == "4294967295":
            continue
        t = float(columns[0])
        prague_ssthresh_time.append(t)
        prague_ssthresh_samples_plot.append(float(columns[1])/1448)
    f.close()

if testName != "":
    fileName = "wired-dualpi2-bytes." + testName + ".dat"
else:
    fileName = "wired-dualpi2-bytes.dat"
f = open(fileName, "r")
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
fig, (ax2, ax3, ax4, ax5, ax6, ax7, ax8) = plt.subplots(
    nrows=7, figsize=(8, 11), sharex=False, sharey=False
)
fig.subplots_adjust(hspace=space_between_subplots)

ax2.scatter(
    cubic_time,
    cubic_samples_plot,
    marker=".",
    s=5,
    color="black",
)
ax2.set_xlim(
    0,10
)
ax2.set_ylim(
    0,120
)
ax2.set_xlabel("Time (s)")
ax2.set_ylabel("Throughput (Mbps)")
#ax2.legend(loc="upper right", framealpha=1, prop={"size": 6})
ax2.set_title("Cubic throughput")

ax3.scatter(
    cubic_cwnd_time,
    cubic_cwnd_samples_plot,
    marker=".",
    color="black",
    label="cwnd",
)
ax3.set_xlim(
    0,10
)
ax3.set_ylim(
    0,
)
ax3.set_xlabel("Time (s)")
ax3.set_ylabel("segments")
#ax3.legend(loc="upper right", framealpha=1, prop={"size": 6})
ax3.set_title("Cubic cwnd")

ax4.scatter(
    cubic_ssthresh_time,
    cubic_ssthresh_samples_plot,
    marker=".",
    color="black",
    label="ssthresh",
)
ax4.set_xlim(
    0,10
)
ax4.set_ylim(
    0,
)
ax4.set_xlabel("Time (s)")
ax4.set_ylabel("segments")
#ax4.legend(loc="upper right", framealpha=1, prop={"size": 6})
ax4.set_title("Cubic ssthresh")

ax5.scatter(
    prague_time,
    prague_samples_plot,
    marker=".",
    color="black",
)
ax5.set_xlim(
    0,10
)
ax5.set_ylim(
    0,120
)
ax5.set_xlabel("Time (s)")
ax5.set_ylabel("Throughput (Mbps)")
ax5.set_title("Prague throughput")
#ax5.legend(loc="upper right", prop={"size": 6})

ax6.scatter(
    prague_cwnd_time,
    prague_cwnd_samples_plot,
    marker=".",
    color="black",
    label="cwnd",
)
ax6.set_xlim(
    0,10
)
ax6.set_ylim(
    0,
)
ax6.set_xlabel("Time (s)")
ax6.set_ylabel("segments")
ax6.set_title("Prague cwnd")
#ax6.legend(loc="upper right", prop={"size": 6})

ax7.scatter(
    prague_ssthresh_time,
    prague_ssthresh_samples_plot,
    marker=".",
    color="black",
    label="ssthresh",
)
ax7.set_xlim(
    0,10
)
ax7.set_ylim(
    0,
)
ax7.set_xlabel("Time (s)")
ax7.set_ylabel("segments")
ax7.set_title("Prague ssthresh")
#ax7.legend(loc="upper right", prop={"size": 6})

ax8.scatter(dualpi2_time, dualpi2_samples_plot, marker=".", color="black", label="Bytes")
ax8.set_ylim(0, 600000)
ax8.set_xlabel("Time (s)")
ax8.set_ylabel("Bytes")
ax8.set_title("Bytes in DualPI2 queue")
ax8.set_xlim(
    0,10
)

fig.suptitle(title)
plt.savefig(plotname, format="pdf")
plt.close()
