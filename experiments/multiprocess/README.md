# Simulation Campaign Scripts

`./run-l4s-wifi.py` will launch a full simulation campaign as specified in config/config.csv

The file config/config.csv can be built manually, according to the description below, or the script config/build_config.sh can be edited to create a new config/config.csv to cover a different set of conditions if desired.

# Default Configuration Parameters

The following is a list of arguments that can be configured using config.csv.

Ensure that all of the following are included in the config.csv file.

- numCubic = 1  
- numPrague = 1  
- numBackground = 0  
- numBytes = 0  
- duration = 20  
- wanLinkDelay = "2500us"  _(wanLinkDelay is 1/2 of the desired base RTT)_   
- mcs = 11  
- channelWidth=20  
- spatialStreams=2  
- wifiQueueSize = "8000p" _(Default WifiMacQueue size in l4s-wifi.cc is 5000 packets, but can be changed here)_  
- maxAmsduSize = 0 _(If maxAmsduSize is zero, it will disable A-MSDU.  If non-zero, it will try to form A-MSDUs (BE access category) up to the number of bytes specified)_

> The following three variables are related; if the first is disabled,
> the second two will have no effect

- flowControl = 1  
- limit = 100000  
- scale = 1  
>
- rtsCtsThreshold = 0  _(Set rtsCtsThreshold to a low value such as 1000 (bytes) to enable RTS/CTS. Zero disables the explicit setting of the WifiRemoteStationManager attribute)_  
- showProgress = 0  
- enablePcap = 1  
- useReno = 0  
- cwMin = 15
- cwMax = 1023
- aifsn = 3
- txopLimit = "2528us"

# Publishing results

To distribute the results of a test, a standalone HTML file may be generated. This is achieved using docToolchain, which requires Docker to be installed on the host system to run correctly.
