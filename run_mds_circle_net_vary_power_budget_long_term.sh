#!/bin/bash

i=25 # i is 100*power budget
while [ $i -le 75 ]; do
  powerBudget=$i$(echo ".0/100.0")
  ./waf --run "scratch/mds-circle-global --V=2000 --powerBudg=$(echo "scale=3; "$powerBudget | bc -q) --longTermAvg=1 --ns3::dus::RoutingProtocol::dataFilePath='./data_files/mds_circle_network/vary_power_budget/' " > output.txt
  i=$(($i + 5))
done

