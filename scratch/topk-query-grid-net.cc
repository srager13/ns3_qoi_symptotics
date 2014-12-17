/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

//
// This program configures a grid (default 5x5) of nodes on an 
// 802.11b physical layer, with
// 802.11b NICs in adhoc mode, and by default, sends one packet of 1000 
// (application) bytes to node 1.
//
// The default layout is like this, on a 2-D grid.
//
// n20  n21  n22  n23  n24
// n15  n16  n17  n18  n19
// n10  n11  n12  n13  n14
// n5   n6   n7   n8   n9
// n0   n1   n2   n3   n4
//
// the layout is affected by the parameters given to GridPositionAllocator;
// by default, GridWidth is 5 and numNodes is 25..
//
// There are a number of command-line options available to control
// the default behavior.  The list of available command-line options
// can be listed with the following command:
// ./waf --run "wifi-simple-adhoc-grid --help"
//
// Note that all ns-3 attributes (not just the ones exposed in the below
// script) can be changed at command line; see the ns-3 documentation.
//
// For instance, for this configuration, the physical layer will
// stop successfully receiving packets when distance increases beyond
// the default of 500m.
// To see this effect, try running:
//
// ./waf --run "wifi-simple-adhoc --distance=500"
// ./waf --run "wifi-simple-adhoc --distance=1000"
// ./waf --run "wifi-simple-adhoc --distance=1500"
// 
// The source node and sink node can be changed like this:
// 
// ./waf --run "wifi-simple-adhoc --sourceNode=20 --sinkNode=10"
//
// This script can also be helpful to put the Wifi layer into verbose
// logging mode; this command will turn on all wifi logging:
// 
// ./waf --run "wifi-simple-adhoc-grid --verbose=1"
//
// By default, trace file writing is off-- to enable it, try:
// ./waf --run "wifi-simple-adhoc-grid --tracing=1"
//
// When you are done tracing, you will notice many pcap trace files 
// in your directory.  If you have tcpdump installed, you can try this:
//
// tcpdump -r wifi-simple-adhoc-grid-0-0.pcap -nn -tt
//

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/topk-query-helper.h"
#include "ns3/dsdv-helper.h"
#include "ns3/applications-module.h"
#include "ns3/tdma-helper.h"
#include "ns3/flow-monitor-helper.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

NS_LOG_COMPONENT_DEFINE ("TopkQueryExample");

using namespace ns3;


int main (int argc, char *argv[])
{
	double runTime = 100;
  std::string phyMode ("DsssRate11Mbps");
  double distance = 250;  // m
  uint32_t numNodes = 25;  // by default, 5x5
  double sumSimilarity = 0.6;
  double timeliness = 5.0;
  bool tracing = false;
	uint64_t imageSizeKBytes = 2000;
	uint64_t packetSizeBytes = 2000;
  double delayPadding;
  std::string dataFilePath;
  std::string sumSimFilename;
  int runSeed = 1;
  int numRuns = 1;
  int numPacketsPerImage = 1;
  double channelRate = 2; // in Mbps

  CommandLine cmd;

	cmd.AddValue ("runTime", "Run time of simulation (in seconds)", runTime);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("distance", "distance (m)", distance);
  cmd.AddValue ("sumSimilarity", "Sum similarity requirement. Translated into number of images as defined in SumSimRequirements.csv.", sumSimilarity);
  cmd.AddValue ("timeliness", "Timeliness requirement of queries", timeliness);
  cmd.AddValue ("tracing", "turn on ascii and pcap tracing", tracing);
  cmd.AddValue ("numNodes", "number of nodes", numNodes);
	cmd.AddValue ("imageSizeKBytes", "Size of each image (in kilobytes).", imageSizeKBytes);
	cmd.AddValue ("packetSizeBytes", "Size of each packet (in bytes).", packetSizeBytes);
	cmd.AddValue ("delayPadding", "Time (in seconds) that server pauses in between successive images to prevent socket overload.", delayPadding);
  cmd.AddValue ("dataFilePath", "Path to print stats file to.", dataFilePath);
  cmd.AddValue ("sumSimFilename", "Name of file to get sum similarity packet numbers from.", sumSimFilename);
  cmd.AddValue ("runSeed", "Seed to set the random number generator.", runSeed);
  cmd.AddValue ("numRuns", "Total number of trials.", numRuns);
  cmd.AddValue ("numPacketsPerImage", "Number of packets neeed to send each image.", numPacketsPerImage);
  cmd.AddValue ("channelRate", "Rate of each wireless channel (in Mbps).", channelRate);

  cmd.Parse (argc, argv);

  // set seed for random number generator
  SeedManager::SetRun(runSeed);

  // disable fragmentation for frames below 2200 bytes
  //Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  // turn off RTS/CTS for frames below 2200 bytes
  //Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  // Fix non-unicast data rate to be the same as that of unicast
  //Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", 
   //                   StringValue (phyMode));

  NodeContainer c;
  c.Create (numNodes);

	Config::SetDefault ("ns3::SimpleWirelessChannel::MaxRange", DoubleValue (distance+5));
	// default allocation, each node gets a slot to transmit
	/* can make custom allocation through simulation script
	 * will override default allocation
	 */
	/*tdma.SetSlots(4,
	    0,1,1,0,0,
	    1,0,0,0,0,
	    2,0,0,1,0,
	    3,0,0,0,1);*/
	// if TDMA slot assignment is through a file
  char buf[50];
  sprintf(buf, "./tdmaSlotAssignFiles/tdmaSlots_%i.txt", numNodes);
  TdmaHelper tdma = TdmaHelper (buf);
//    TdmaHelper tdma =  = TdmaHelper (c.GetN (), c.GetN ()); // in this case selected, numSlots = nodes

	TdmaControllerHelper controller;
  sprintf(buf, "%ib/s", (int)channelRate*1000000);
  std::cout<<"Channel rate = " << buf << "\n";
	controller.Set ("DataRate", DataRateValue (DataRate(buf))); // Mbps
  double slot_time = ((packetSizeBytes+40)*8.0)/(channelRate*1000000);
  controller.Set("SlotTime", TimeValue(Seconds(slot_time))); // time to transmit one 1400 byte packet at 2Mbps is 5600 uSec
	controller.Set ("GaurdTime", TimeValue (MicroSeconds (0)));
	controller.Set ("InterFrameTime", TimeValue (MicroSeconds (0)));
	tdma.SetTdmaControllerHelper (controller);
  NetDeviceContainer devices = tdma.Install (c);


  uint16_t sq_rt_num_nodes = (uint16_t)sqrt(numNodes);
	// Set node positions (no mobility)
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (distance),
                                 "GridWidth", UintegerValue (sq_rt_num_nodes),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (c);


	Ipv4StaticRoutingHelper staticRouting;
	Ipv4ListRoutingHelper list;
	list.Add(staticRouting,0);

  // Install IP layer
	InternetStackHelper internet;
	internet.SetRoutingHelper(list);
	internet.Install(c);

	// Assign IP addresses
  Ipv4AddressHelper address;
  if( numNodes <= 255 )
  {
    address.SetBase ("10.1.1.0", "255.255.255.0");
  } 
  else if ( numNodes <= 511 )
  {
    address.SetBase ("10.1.2.0", "255.255.254.0");
  }
  else if ( numNodes <= 1023 )
  {
    address.SetBase ("10.1.4.0", "255.255.252.0");
  }
  else if ( numNodes <= 2047 )
  {
    address.SetBase ("10.1.8.0", "255.255.248.0");
  }
  else if ( numNodes <= 4095 )
  {
    address.SetBase ("10.1.16.0", "255.255.240.0");
  }
  Ipv4InterfaceContainer i = address.Assign (devices);

  std::vector< Ptr<Ipv4> > ipv4;
  std::vector< Ptr<Ipv4StaticRouting> > ipv4staticRouting;
  for( uint16_t i = 0; i < numNodes; i++ )
  {
    Ptr<Ipv4> ipv4ptr = c.Get(i)->GetObject<Ipv4>();
    ipv4.push_back(ipv4ptr);
    Ptr<Ipv4StaticRouting> routingPointer = staticRouting.GetStaticRouting(ipv4[i]);
    ipv4staticRouting.push_back(routingPointer);
  }

  uint16_t second_byte_start = 1;
  if( numNodes <= 255 )
    second_byte_start = (uint16_t)1;
  else if( numNodes <= 511 )
    second_byte_start = (uint16_t)2;
  else if( numNodes <= 1023 )
    second_byte_start = (uint16_t)4;
  else if( numNodes <= 2047 )
    second_byte_start = (uint16_t)8;
  else if( numNodes <= 4095 )
    second_byte_start = (uint16_t)16;
   
  for( uint16_t i = 0; i < numNodes; i++ )
  {
    for( uint16_t j = 0; j < numNodes; j++ )
    {
        if( i == j )
        {
          continue;
        }

        // need to make static routes based on following structure:
        //
        //  if dest%sqrtnn > me%sqrtnn, then route right
        //  if dest%sqrtnn < me%sqrtnn, then route left
        //  if dest%sqrtnn == me%sqrtnn, then (go up/down)
        //    if dest/sqrtnn < me/sqrtnn, then go down
        //    if dest/sqrtnn > me/sqrtnn, then go up

   
        // AddHostRouteTo( dest, nextHop, interface, metric) 
        //  i is current node, j is dest
        char dest[32];
        char nextHop[32];
        uint8_t firstByte = (j+1)&(uint8_t)255;
        uint8_t secondByte = 1;
        if( j < 255 )
        {
          secondByte = second_byte_start;
        }
        else if( j < 511 )
        {
          secondByte = second_byte_start + (uint8_t)1;
        }
        else if( j < 767 )
        {
          secondByte = second_byte_start + (uint8_t)2;
        }
        else if( j < 1023 )
        {
          secondByte = second_byte_start + (uint8_t)3;
        }
        else if( j < 1279 )
        {
          secondByte = second_byte_start + (uint8_t)4;
        }
        else if( j < 1535 )
        {
          secondByte = second_byte_start + (uint8_t)5;
        }
        else if( j < 1791 )
        {
          secondByte = second_byte_start + (uint8_t)6;
        }
        else if( j < 2047 )
        {
          secondByte = second_byte_start + (uint8_t)7;
        }
        else if( j < 2303 )
        {
          secondByte = second_byte_start + (uint8_t)8;
        }
        else if( j < 2559 )
        {
          secondByte = second_byte_start + (uint8_t)9;
        }
        else if( j < 2815 )
        {
          secondByte = second_byte_start + (uint8_t)10;
        }
        else 
        {
          std::cout<<"ERROR:  Need to account for addresses higher than 2815 nodes (in scratch/topk-query-static-routing.cc)\n";
          exit(-1);
        }
        sprintf(dest, "10.1.%d.%d", secondByte, firstByte);
       
        if( j%sq_rt_num_nodes > i%sq_rt_num_nodes )
        {
          firstByte = (i+1+1)&(uint8_t)255;
          if( i+1 < 255 )
          {
            secondByte = second_byte_start;
          }
          else if( i+1 < 511 )
          {
            secondByte = second_byte_start + (uint8_t)1;
          }
          else if( i+1 < 767 )
          {
            secondByte = second_byte_start + (uint8_t)2;
          }
          else if( i+1 < 1023 )
          {
            secondByte = second_byte_start + (uint8_t)3;
          }
          else if( i+1 < 1279 )
          {
            secondByte = second_byte_start + (uint8_t)4;
          }
          else if( i+1 < 1535 )
          {
            secondByte = second_byte_start + (uint8_t)5;
          }
          else if( i+1 < 1791 )
          {
            secondByte = second_byte_start + (uint8_t)6;
          }
          else if( i+1 < 2047 )
          {
            secondByte = second_byte_start + (uint8_t)7;
          }
          else if( i+1 < 2303 )
          {
            secondByte = second_byte_start + (uint8_t)8;
          }
          else if( i+1 < 2559 )
          {
            secondByte = second_byte_start + (uint8_t)9;
          }
          else if( i+1 < 2815 )
          {
            secondByte = second_byte_start + (uint8_t)10;
          }

        }
        else if( j%sq_rt_num_nodes < i%sq_rt_num_nodes )
        {
          firstByte = (i-1+1)&(uint8_t)255;
          if( i-1 < 255 )
          {
            secondByte = second_byte_start;
          }
          else if( i-1 < 511 )
          {
            secondByte = second_byte_start + (uint8_t)1;
          }
          else if( i-1 < 767 )
          {
            secondByte = second_byte_start + (uint8_t)2;
          }
          else if( i-1 < 1023 )
          {
            secondByte = second_byte_start + (uint8_t)3;
          }
          else if( i-1 < 1279 )
          {
            secondByte = second_byte_start + (uint8_t)4;
          }
          else if( i-1 < 1535 )
          {
            secondByte = second_byte_start + (uint8_t)5;
          }
          else if( i-1 < 1791 )
          {
            secondByte = second_byte_start + (uint8_t)6;
          }
          else if( i-1 < 2047 )
          {
            secondByte = second_byte_start + (uint8_t)7;
          }
          else if( i-1 < 2303 )
          {
            secondByte = second_byte_start + (uint8_t)8;
          }
          else if( i-1 < 2559 )
          {
            secondByte = second_byte_start + (uint8_t)9;
          }
          else if( i-1 < 2815 )
          {
            secondByte = second_byte_start + (uint8_t)10;
          }

        }
        else if( j/sq_rt_num_nodes < i/sq_rt_num_nodes )
        {
          firstByte = (i-sq_rt_num_nodes+1)&(uint8_t)255;
          if( i-sq_rt_num_nodes < 255 )
          {
            secondByte = second_byte_start;
          }
          else if( i-sq_rt_num_nodes < 511 )
          {
            secondByte = second_byte_start + (uint8_t)1;
          }
          else if( i-sq_rt_num_nodes < 767 )
          {
            secondByte = second_byte_start + (uint8_t)2;
          }
          else if( i-sq_rt_num_nodes < 1023 )
          {
            secondByte = second_byte_start + (uint8_t)3;
          }
          else if( i-sq_rt_num_nodes < 1279 )
          {
            secondByte = second_byte_start + (uint8_t)4;
          }
          else if( i-sq_rt_num_nodes < 1535 )
          {
            secondByte = second_byte_start + (uint8_t)5;
          }
          else if( i-sq_rt_num_nodes < 1791 )
          {
            secondByte = second_byte_start + (uint8_t)6;
          }
          else if( i-sq_rt_num_nodes < 2047 )
          {
            secondByte = second_byte_start + (uint8_t)7;
          }
          else if( i-sq_rt_num_nodes < 2303 )
          {
            secondByte = second_byte_start + (uint8_t)8;
          }
          else if( i-sq_rt_num_nodes < 2559 )
          {
            secondByte = second_byte_start + (uint8_t)9;
          }
          else if( i-sq_rt_num_nodes < 2815 )
          {
            secondByte = second_byte_start + (uint8_t)10;
          }
        }
        else if( j/sq_rt_num_nodes > i/sq_rt_num_nodes )
        {
          firstByte = (i+sq_rt_num_nodes+1)&(uint8_t)255;
          if( i+sq_rt_num_nodes < 255 )
          {
            secondByte = second_byte_start;
          }
          else if( i+sq_rt_num_nodes < 511 )
          {
            secondByte = second_byte_start + (uint8_t)1;
          }
          else if( i+sq_rt_num_nodes < 767 )
          {
            secondByte = second_byte_start + (uint8_t)2;
          }
          else if( i+sq_rt_num_nodes < 1023 )
          {
            secondByte = second_byte_start + (uint8_t)3;
          }
          else if( i+sq_rt_num_nodes < 1279 )
          {
            secondByte = second_byte_start + (uint8_t)4;
          }
          else if( i+sq_rt_num_nodes < 1535 )
          {
            secondByte = second_byte_start + (uint8_t)5;
          }
          else if( i+sq_rt_num_nodes < 1791 )
          {
            secondByte = second_byte_start + (uint8_t)6;
          }
          else if( i+sq_rt_num_nodes < 2047 )
          {
            secondByte = second_byte_start + (uint8_t)7;
          }
          else if( i+sq_rt_num_nodes < 2303 )
          {
            secondByte = second_byte_start + (uint8_t)8;
          }
          else if( i+sq_rt_num_nodes < 2559 )
          {
            secondByte = second_byte_start + (uint8_t)9;
          }
          else if( i+sq_rt_num_nodes < 2815 )
          {
            secondByte = second_byte_start + (uint8_t)10;
          }
        }
        else
        {
          std::cout<<"ERROR: Don't know where to set next hop for node " << i << "'s path to " << j << "\n";
        }

        sprintf(nextHop, "10.1.%d.%d", secondByte, firstByte);
        //std::cout<<"For node " << i << ": setting next hop of " << nextHop << " for destination of " << dest << "\n"; 
        ipv4staticRouting[i]->AddHostRouteTo(Ipv4Address(dest),Ipv4Address(nextHop),1);
    }
  } 

	// Create static routes from A to C
	//Ptr<Ipv4StaticRouting> staticRoutingA = ipv4RoutingHelper.GetStaticRouting (ipv4A);
	// The ifIndex for this outbound route is 1; the first p2p link added
	//staticRoutingA->AddHostRouteTo (Ipv4Address ("192.168.1.1"), Ipv4Address ("10.1.1.2"), 1);
	//Ptr<Ipv4StaticRouting> staticRoutingB = ipv4RoutingHelper.GetStaticRouting (ipv4B);
	// The ifIndex we want on node B is 2; 0 corresponds to loopback, and 1 to the first point to point link
	//staticRoutingB->AddHostRouteTo (Ipv4Address ("192.168.1.1"), Ipv4Address ("10.1.1.6"), 2);

  if (tracing == true)
    {
			AsciiTraceHelper ascii;
			std::ostringstream oss;
			oss << "topk-query_" << numNodes << "_Nodes.tr";
			std::string tr_name = oss.str();
 
			Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream (tr_name);
			tdma.EnableAsciiAll (stream);

      //wifiPhy.EnablePcap ("topk-query", devices);
      // Trace routing tables
      //Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("topk-query.routes", std::ios::out);
      //olsr.PrintRoutingTableAllEvery (Seconds (2), routingStream);

      // To do-- enable an IP-level trace that shows forwarding events only
    }

  if( runTime < 100.0 )
  {
    std::cout<<"ERROR:  Applications stop running 100 seconds before simulation end, so run time needs to be longer than 100.\n";
    exit(-1);
  }

  // Output what we are doing
  //NS_LOG_UNCOND ("Testing from node " << sourceNode << " to " << sinkNode << " with grid distance " << distance);
//
// Create a TopkQueryServer application on node one.
//
  uint16_t port = 9;  // well-known echo port number
  TopkQueryServerHelper server (port);
	server.SetAttribute("ImageSizeKBytes", UintegerValue(imageSizeKBytes));
	server.SetAttribute("PacketSizeBytes", UintegerValue(packetSizeBytes));
	server.SetAttribute("NumNodes", UintegerValue(numNodes));
  server.SetAttribute("DelayPadding", DoubleValue(delayPadding));
  server.SetAttribute("ChannelRate", DoubleValue(channelRate));
  server.SetAttribute ("Timeliness", TimeValue (Seconds(timeliness)));
  ApplicationContainer apps = server.Install (c);
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (runTime-100.0));

//
// Create a TopkQueryClient application to send UDP datagrams from node zero to
// node one.
//
//  Time interPacketInterval = Seconds (1.);
  uint32_t num_nodes = numNodes;
  TopkQueryClientHelper client (port, num_nodes);
  client.SetAttribute ("SumSimilarity", DoubleValue (sumSimilarity));
  client.SetAttribute ("Timeliness", TimeValue (Seconds(timeliness)));
  client.SetAttribute ("DataFilePath", StringValue (dataFilePath));
  client.SetAttribute ("SumSimFilename", StringValue (sumSimFilename));
  client.SetAttribute ("ImageSizeKBytes", UintegerValue (imageSizeKBytes));
  client.SetAttribute ("PacketSizeBytes", UintegerValue (packetSizeBytes));
  client.SetAttribute ("RunTime", TimeValue (Seconds(runTime)));
  client.SetAttribute ("RunSeed", UintegerValue (runSeed));
  client.SetAttribute ("NumRuns", UintegerValue (numRuns));
  client.SetAttribute ("NumPacketsPerImage", IntegerValue (numPacketsPerImage));
  client.SetAttribute ("ChannelRate", DoubleValue (channelRate));
  apps = client.Install(c);
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (runTime-100.0));

/*
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();
*/

  std::cout<<"Running for " << runTime << " seconds\n";
  std::cout<<"Sum similarity = " << sumSimilarity << "\n";

  Simulator::Stop (Seconds (runTime));
  Simulator::Run ();
  //flowMonitor->SerializeToXmlFile("Topk-query-flow-monitor.xml", true, true);
  Simulator::Destroy ();

  return 0;
}

