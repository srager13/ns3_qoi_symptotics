#!/bin/bash

j=5 # j is the number of nodes (size)
while [ $j -le 15 ]; do
  i=5 # i is the percentage of packets that should overlap with over nodes'
  while [ $i -le 95 ]; do
    ./waf --run "scratch/mds-circle-global --size=$j --ns3::dus::MaxDivSched::percentOverlapPackets=${i}ms --time=10" > output.txt
    i=$(($i + 1))
  done
  j=$(($j + 1))
done

