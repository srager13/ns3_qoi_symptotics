#!/bin/bash

i=0 # i is V 
while [ $i -le 2000 ]; do
  ./waf --run "scratch/mds-circle-global --V=$i --longTermAvg=1 --powerBudg=0.3 --ns3::dus::RoutingProtocol::dataFilePath='./data_files/mds_circle_network/vary_V/' " > output.txt
#  ./waf --run "scratch/mds-circle-global --V=$i --longTermAvg=1 " > output.txt
  i=$(($i + 200))
done

