#!/bin/bash

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

# This variant of the multiflow.sh script is designed to process old-style "libpcap" files, which don't have a frame.interface_name
# This script expects two pcap files one corresponding to cmci and the other nsi
# Additionally, this script handles large simulations by splitting the pcap data into smaller, fixed duration, chunks prior to post-processing


scriptDir=${0%/*}
#Timestamp=`date +%Y%m%d-%H%M%S`
cmci_file=$1
nsi_file=$2
Tag=${4:-MultiFlow}
DirExt=${3:-Test}

cmci_1="cmci"
nsi_1="nsi"

# the multiflow.py script wasn't designed for handling very large pcap files, it reads the entire file into memory for processing. 
# for long simulation durations at high data rates, this can result in memory issues. 
# Chunk duration (in seconds, below) breaks up the pcap data into smaller chunks for reading in to multiflow.py
chunk_duration=10

# once multiflow.py is done with its work, combine.py will read in the results and then calculate statistics over smaller intervals
analysis_interval=1

file1=/tmp/file1$$
file2=/tmp/file2$$
files=/tmp/files$$_

# usage: ./multiflow_ns3.sh <cmci_file>.pcap <nsi_file>.pcap <dir_to_save> <filename_ext>(optional)
if [ ! -e $1 ]; then
	echo "File $1 does not exist"
	exit
fi
if [ ! -e $2 ]; then
	echo "File $2 does not exist"
	exit
fi

mkdir -p ${DirExt}
filename=${Tag}_
tshark --disable-protocol tcp --disable-protocol udp --disable-protocol icmp -r $1 -Y "ip" -T fields -e frame.time_epoch  -e ip.src -e ip.dst -e ip.proto -e ip.dsfield.dscp -e ip.dsfield.ecn -e ip.id -e frame.len -e data | \
awk -v ifname="${cmci_1}" '{ print $1, ifname, $2,$3,$4,$5,$6,$7,$8,$9 } ' > $file1 &

tshark --disable-protocol tcp --disable-protocol udp --disable-protocol icmp -r $2 -Y "ip" -T fields -e frame.time_epoch  -e ip.src -e ip.dst -e ip.proto -e ip.dsfield.dscp -e ip.dsfield.ecn -e ip.id -e frame.len -e data | \
awk -v ifname="${nsi_1}" '{ print $1, ifname, $2,$3,$4,$5,$6,$7,$8,$9 } ' > $file2 &

wait
paste -d "\n" $file1 $file2 | \
python3 $scriptDir/pcap_sort.py | \
awk '{ if (NR ==1) {stime=$1}; printf "%.17g ", $1-stime; print $2,$3,$4,$5,$6,$7,$8,$9,$10}' | \
$scriptDir/my_split.py $chunk_duration $files

for file in ${files}*
do
	count=${file##*_}
	subDir=${DirExt}/$count
	mkdir $subDir
	cat $file | python3 $scriptDir/multiflow.py $filename $cmci_1 $nsi_1 $subDir
	rm $file
done
rm $file1 $file2

$scriptDir/combine.py ${DirExt} $analysis_interval


