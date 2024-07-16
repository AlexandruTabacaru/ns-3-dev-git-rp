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

# This variant of the multiflow.sh script is designed to process old-style "libpcap" files, which don't have a frame.inerface_name
# This script expects two pcap files one corresponding to cmci and the other nsi

scriptDir=${0%/*}
#Timestamp=`date +%Y%m%d-%H%M%S`
cmci_file=$1
nsi_file=$2
Tag=${4:-MultiFlow}
DirExt=${3:-Test}

# cmci_1="cmci"
# nsi_1="nsi"

# file1=/tmp/file1$$
# file2=/tmp/file2$$
# files=/tmp/files$$_

# # usage: ./multiflow_ns3.sh <cmci_file>.pcap <nsi_file>.pcap <dir_to_save> <filename_ext>(optional)
# if [ ! -e $1 ]; then
# 	echo "File $1 does not exist"
# 	exit
# fi
# if [ ! -e $2 ]; then
# 	echo "File $2 does not exist"
# 	exit
# fi

# mkdir -p ${DirExt}
# filename=${Tag}_
# tshark --disable-protocol tcp --disable-protocol udp --disable-protocol icmp -r $1 -Y "ip" -T fields -e frame.time_epoch  -e ip.src -e ip.dst -e ip.proto -e ip.dsfield.dscp -e ip.dsfield.ecn -e ip.id -e frame.len -e data | \
# awk -v ifname="${cmci_1}" '{ print $1, ifname, $2,$3,$4,$5,$6,$7,$8,$9 } ' > $file1 &

# tshark --disable-protocol tcp --disable-protocol udp --disable-protocol icmp -r $2 -Y "ip" -T fields -e frame.time_epoch  -e ip.src -e ip.dst -e ip.proto -e ip.dsfield.dscp -e ip.dsfield.ecn -e ip.id -e frame.len -e data | \
# awk -v ifname="${nsi_1}" '{ print $1, ifname, $2,$3,$4,$5,$6,$7,$8,$9 } ' > $file2 &

# wait

file1="/tmp/file11167762"  
file2="/tmp/file21167762"
files="/tmp/files1167762_"

paste -d "\n" $file1 $file2 | \
tee /tmp/pasted.txt | \
python3 $scriptDir/pcap_sort.py | \
tee /tmp/sorted.txt | \
awk '{ if (NR ==1) {stime=$1}; printf "%.17g ", $1-stime; print $2,$3,$4,$5,$6,$7,$8,$9,$10}' | \
tee /tmp/awked.txt | \
$scriptDir/my_split.py 10 $files

# for file in ${files}*
# do
# 	count=${file##*_}
# 	subDir=${DirExt}/$count
# 	mkdir $subDir
# 	cat $file | python3 $scriptDir/multiflow.py $filename $cmci_1 $nsi_1 $subDir
#   rm $file
# done
# rm $file1 $file2
