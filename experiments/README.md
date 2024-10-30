# Wi-Fi L4S Experiments

This is the main directory for defining and launching simulation campaigns.
This directory contains multiple experiments that leverage the simulation topologies in the [scratch](../scratch) directory.  

The currently defined simulation programs are:  
  
[initial](./initial): a simple simulator script that will launch a single test of Wi-Fi with L4S.  
[multiprocess](multiprocess): the core experimental model used for generating the results published in the WBA Implementation Guidelines doc.  

There are also some utility programs included here:  
  
[latency-monitor](./latency-monitor): a utility that is used to calculate statistics and create graphs of latency & throughput.  
[loss-tests](loss-tests): some experiments used to validate the TCP Prague implementation in ns3.  
[wired](wired): a model that uses a wired link in place of the Wi-Fi link (used for some validation tests).  

