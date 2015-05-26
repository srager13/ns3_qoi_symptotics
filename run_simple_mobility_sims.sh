#!/bin/bash

./waf --run "scratch/dus-simple-mobility --time=1000 --ns3::dus::RoutingProtocol::TIME_STATS=true --ns3::dus::RoutingProtocol::timeSlotDuration=250ms --ns3::dus::RoutingProtocol::controlInfoExchangeTime=50ms --ns3::dus::RoutingProtocol::GLOBAL_KNOWLEDGE=true --ns3::dus::RoutingProtocol::OUTPUT_RATES=true" > output.txt
