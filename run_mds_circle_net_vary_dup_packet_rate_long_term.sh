#!/bin/bash

i=0 # i is the number of duplicate packets per time slot
while [ $i -le 2 ]; do
  ./waf --run "scratch/mds-circle-global --dupPacketRate=$i --longTermAvg=1 --ns3::dus::RoutingProtocol::dataFilePath='./data_files/mds_circle_network/vary_dup_packet_rate/' " > output.txt
  i=$(($i + 1))
done

