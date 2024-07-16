#!/bin/bash

# build config.csv file

TC=("0,0" "1,0" "0,1" "1,1" "2,2" "4,4" "2,0" "0,2" "4,0" "0,4")
MS=(0 1 4)
numBytes="0"
duration="30"
TS=('"0ms"' '"1ms"' '"5ms"' '"25ms"' '"20ms"')
mcs="6"
LS=(0 20 80 160)
spatialStreams="2"
AP=(0 1)
wifiQueueSize='"8000p"'
LIMIT=(0 46318 188366 376734) # 1 txop
# LIMIT=(0 92636 376732 753468) # 2 txop
# LIMIT=(0 185272 753464 1506936) # 4 txop
SCALE=(1 1.62 0.823 1 0.396 0.0989)
ED=('15,1023,3,"2528us"' '7,15,2,"4096us"' '3,7,2,"2080us"' '63,63,1,"2528us"' '63,63,1,"1000us"' '63,63,1,"250us"' )
echo "Test Case,numCubic,numPrague,numBackgroundUdp,numBytes,duration,wanLinkDelay,mcs,channelWidth,spatialStreams,flowControl,wifiQueueSize,limit,scale,cwMin,cwMax,aifsn,txopLimit" > config.csv

for i in 0; do #ED
ed=${ED[i]}
scale=${SCALE[i]}
for j in 0 1 2; do #MS
ms=${MS[j]}
for k in 0 1; do #AP
ap=${AP[k]}
for l in 1 2 3; do #LS
lS=${LS[l]}
limit=${LIMIT[l]}
for m in 1 2 3 4 5; do #TC
tc=${TC[m]}
for n in 1 2 3; do #TS
ts=${TS[n]}

testcase="ED${i}-MS${j}-AP${k}-LS${l}-TC${m}-TS${n}"

echo "${testcase},${tc},${ms},${numBytes},${duration},${ts},${mcs},${lS},${spatialStreams},${ap},${wifiQueueSize},${limit},${scale},${ed}" >> config.csv


done
done
done
done
done
done
