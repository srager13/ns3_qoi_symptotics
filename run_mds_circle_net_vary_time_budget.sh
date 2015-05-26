#!/bin/bash

j=5 # j is the number of nodes (size)
while [ $j -le 5 ]; do
  i=25 # i is the time budget each slot in milliseconds
  while [ $i -le 150 ]; do
    ./waf --run "scratch/mds-circle-global --size=$j --timeBudg=${i}ms --time=3 --packetsPerSec=5.0" > output.txt
    i=$(($i + 25))
  done
  j=$(($j + 1))
done

