#!/bin/bash

./waf --run "scratch/dus-line-net --size=5 --time=500 --ns3::dus::RoutingProtocol::TIME_STATS=true --ns3::dus::RoutingProtocol::timeSlotDuration=250ms --ns3::dus::RoutingProtocol::controlInfoExchangeTime=50ms" > output.txt
