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

import fileinput

# When capturing multiple interfaces, dumpcap, tshark, wireshark all write the packets to the file out of order
# The multiflow.sh script uses tshark to output certain fields of each packet
# This program reads in that output format, and sorts the packets in chronological order in a streamed mode to support a multiprocess pipeline

def outputs(doneReading):
	
	# check to see if any of the if_buffers are empty and check to see if they are all empty
	dataReady=True
	dataRemaining=False
	for ifname in if_buffer:
		if len(if_buffer[ifname]) == 0:
			dataReady=False
		else:
			dataRemaining=True

	# while !doneReading and all if_buffers have packets, or if doneReading and any if_buffer has packets
	while ((doneReading and dataRemaining) or dataReady):		
		lowest_ts=float('inf')
		lowest_ts_ifname=''
		if doneReading or dataReady:
			for ifname in if_buffer:
				if len(if_buffer[ifname]) > 0:
					ts=float(if_buffer[ifname][0][0]) # get timestamp of packet at the head of each if_buffer
					if ts < lowest_ts: #find the lowest timestamp across all if_buffers
						lowest_ts = ts
						lowest_ts_ifname = ifname
		out_line = if_buffer[lowest_ts_ifname].pop(0) # pop that packet from the head of the if_buffer

		print('     '.join(out_line)) # write it to stdout

		# check status again to close out the while loop
		dataReady=True
		dataRemaining=False
		for ifname in if_buffer:
			if len(if_buffer[ifname]) == 0:
				dataReady=False
			else:
				dataRemaining=True



# Read the input data from stdin
linecount=0
if_buffer={}
for line in fileinput.input():
	linecount += 1
	x = line.split()
	# Expecting:
	# x[0]: frame.time_relative 
	# x[1]: frame.interface_name 
	# x[2]: ip.src 
	# x[3]: ip.dst 
	# x[4]: ip.proto 
	# x[5]: ip.dsfield.dscp 
	# x[6]: ip.dsfield.ecn 
	# x[7]: ip.id 
	# x[8]: frame.len 
	# x[9]: data (first 8 characters contain the port numbers)

	if x: # don't fail on blank lines
		# create a separate if_buffer for each ifname, and append the packet data to the appropriate buffer
		ifname = x[1]
		try:
			if_buffer[ifname].append(x)
		except KeyError:
			if_buffer[ifname] = [x]

	# wait at least 1000 lines to make sure we've seen all interface_names, then begin writing data to stdout
	if linecount > 1000:
		outputs(False)

# done reading file, write remaining data to stdout
if len(if_buffer)>0:
	outputs(True)







