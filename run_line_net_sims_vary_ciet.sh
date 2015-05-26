#!/bin/bash

i=10 # i is control info exchange time in milliseconds
while [ $i -le 30 ]
do
  ./waf --run "scratch/dus-line-net --size=5 --time=100 --ns3::dus::RoutingProtocol::controlInfoExchangeTime=${i}ms" > output.txt
  i=$(($i + 10))
done
