#!/bin/bash

i=0 # i is V 
while [ $i -le 100 ]; do
  ./waf --run "scratch/mds-circle-global --V=$i --ns3::dus::RoutingProtocol::dataFilePath='./data_files/mds_circle_network/vary_V/' " > output.txt
  i=$(($i + 10))
done

