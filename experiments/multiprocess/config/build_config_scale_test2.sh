#!/bin/bash

# build config.csv file

TC=("0,0" "2,0" "0,2" "1,1")
numBytes="0"
duration="60"
TS=('"0ms"' '"1ms"' '"5ms"' '"25ms"' '"20ms"')
spatialStreams="2"
AP=(0 1)
wifiQueueSize='"8000p"'
SCALE=(\
0.2 \
0.4 \
0.6 \
0.8 \
1.0 \
1.2 \
1.4 \
1.6 \
1.8 \
2.0 \
2.2 \
2.4 \
2.6 \
2.8 \
3 \
4 \
6 \
8 \
10 \
)

lS=80
MS=(0 1 4 8 1 1 1) 
MCS=(9 9 9 9 2 6 11)


ED=('15,1023,3,"2528us"' '7,15,2,"4096us"' '3,7,2,"2080us"' '63,63,1,"2528us"' '63,63,1,"1000us"' '63,63,1,"250us"' )

echo "Test Case,\
numCubic,\
numPrague,\
numBackgroundUdp,\
numBytes,\
duration,\
wanLinkDelay,\
mcs,\
channelWidth,\
spatialStreams,\
flowControl,\
wifiQueueSize,\
scale,\
cwMin,\
cwMax,\
aifsn,\
txopLimit" > config.csv

scale=${SCALE}

for i in 0 4; do #ED
ed=${ED[i]}
for j in 0 1 2 3 4 5 6; do #MS
ms=${MS[j]}
l=$j
mcs=${MCS[l]}
for k in 1; do #AP
ap=${AP[k]}
for m in 1 2 3; do #TC
tc=${TC[m]}
for n in 2; do #TS
ts=${TS[n]}
for o in "${!SCALE[@]}"; do
scale=${SCALE[$o]}

testcase="ED${i}-MS${j}-AP${k}-LS${l}-TC${m}-TS${n}-SC${o}"

echo "${testcase},${tc},${ms},${numBytes},${duration},${ts},${mcs},${lS},${spatialStreams},${ap},${wifiQueueSize},${scale},${ed}" >> config.csv

done
done
done
done
done
done

