#!/usr/bin/python3

import fileinput
import sys

duration = float(sys.argv[1])
fileprefix = sys.argv[2]

n=5 
count=0
max_time=duration

filename = fileprefix + str(count).zfill(n)
# print(filename)
file = open(filename, 'w')

for line in fileinput.input('-'):
	t = float(line.split(maxsplit=1)[0])
	if t < max_time:
		file.write(line)
		# pass
	else:
		file.close()
		count += 1
		max_time += duration
		filename = fileprefix + str(count).zfill(n)
		# print(filename)
		file = open(filename, 'w')
		file.write(line)

file.close()
