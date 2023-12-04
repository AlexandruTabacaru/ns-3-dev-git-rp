# ns-3 L4S wired experiment documentation

## 1. Introduction

This experiment uses a Python script (**run-l4s-wired.py**) to launch one simulation execution of
the **l4s-wired.cc** ns-3 C++ simulation program, and to plot some initial time-series results.

The program makes use of a slightly extended version of the ns-3.40 public release:  1) a
model for TCP Prague has been added, 2) a DualPi2 coupled AQM has been added, and 3) the
ns-3 Wi-Fi model has been slightly extended to provide some flow control between the overlying
DualPi2 model and the underlying device queue.

However, this example replaces the Wi-Fi link with a wired link.

The C++ program itself is found at **scratch/l4s-wired.cc** as referenced from the top-level ns-3
directory.  There is no requirement to run this program from this experiment directory using
the Python script-- the C++ program can be run from the command-line like other ns-3 programs,
such as:

~~~
./ns3 run l4s-wired
~~~

This experiment directory provides a Python runner and plotting script to facilitate
experiment control.  Instead, the way to run the experiment from this directory is to
configure the parameters in **run-l4s-wired.py** as appropriate, and then to run it:

~~~
./run-l4s-wired.py
~~~

and then to recurse into the **results** directory that is created by the Python program
and work with the simulation output stored there.

This guide assumes that you have downloaded/cloned this version of ns-3 from CableLabs
and have built it in the usual way; see the [ns-3 Quick Start](https://www.nsnam.org/docs/tutorial/html/quick-start.html) for more information if needed.

See the initial experiment directory for more documentation on the
scenario; the main difference is the change in topology to replace the wifi with wired.

~~~
// Nodes 0               Node 1                     Node 2          Nodes 3+
//                                                         ------->
// server -------------> router ------------------> router -------> N clients
//        1 Gbps;               configurable rate;         -------> (foreground/background)
//        configurable          100 us base RTT            1 Gbps;
//        base RTT                                         100 us base RTT
~~~

**Figure 1: l4s-wired.cc topology**

The base RTT is configurable with the **wanLinkDelay** parameter (default 10 ms, leading
to a base RTT of 20 ms), and the bottleneck link rate is configurable with the
**bottleneckRate** parameter (default 100 Mbps).  The file transfer size is 50 MB by
default, also configurable by the **numBytes** parameter.

~~~
./ns3 run 'l4s-wired --PrintHelp'
~~~

