# Default Configuration Parameters

The following is a list of arguments that can be configured using config.csv.

Ensure that all of the following are included in the config.csv file.

numCubic = 1  
numPrague = 1  
numBackground = 0  
numBytes = 0  
duration = 20  

> wanLinkDelay is 1/2 of the desired base RTT  

wanLinkDelay = "2500us"  
mcs = 11  
channelWidth=20  
spatialStreams=2  

> Default WifiMacQueue size is now 5000 packets, but can be changed below  

wifiQueueSize = "5000p"

> If maxAmsduSize is zero, it will disable A-MSDU.  If non-zero, it will
> try to form A-MSDUs (BE access category) up to the number of bytes specified

maxAmsduSize = 0

> The following three variables are related; if the first is disabled,
> the second two will have no effect

flowControl = 1  
limit = 100000  
scale = 1  

> Set rtsCtsThreshold to a low value such as 1000 (bytes) to enable RTS/CTS.
> Zero disables the explicit setting of the WifiRemoteStationManager attribute

rtsCtsThreshold = 0  
showProgress = 0  
enablePcap = 1  
useReno = 0  

# Publishing results

To distribute the results of a test, a standalone HTML file may be generated. This is achieved using docToolchain, which requires Docker to be installed on the host system to run correctly.
