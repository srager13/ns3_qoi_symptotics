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
#include "ns3/qoi-query-flood-helper.h"
#include "ns3/dsdv-helper.h"
#include "ns3/applications-module.h"
#include "ns3/tdma-helper.h"
#include "ns3/flow-monitor-helper.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

NS_LOG_COMPONENT_DEFINE ("QoiQueryFloodExample");

using namespace ns3;


int main (int argc, char *argv[])
{
	double runTime = 100;
  std::string phyMode ("DsssRate2Mbps");
  double distance = 250;  // m
  uint32_t packetSize = 10; // bytes
  uint32_t numPackets = 1;
  uint32_t numNodes = 25;  // by default, 5x5
  uint32_t sinkNode = 0;
  uint32_t sourceNode = 24;
  double interval = 1.0; // seconds
  double sumSimilarity = 10.0;
  int packetsPerImage = 1;
  double timeliness = 5.0;
  bool verbose = false;
  bool tracing = false;
	uint16_t numReturnImages = 10;
	int imageSizeBytes = 888;
  double delayPadding;
  std::string dataFilePath;
  std::string sumSimFilename;
	int clientNode = 1;
	int serverNode = 1;
  int runSeed = 1;
  int numRuns = 1;
  int numPacketsPerImage = 1;

  CommandLine cmd;

	cmd.AddValue ("runTime", "Run time of simulation (in seconds)", runTime);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("distance", "distance (m)", distance);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "number of packets generated", numPackets);
  cmd.AddValue ("interval", "interval (seconds) between packets", interval);
  cmd.AddValue ("sumSimilarity", "Sum similarity requirement. Translated into number of images as defined in SumSimRequirements.csv.", sumSimilarity);
  cmd.AddValue ("packetsPerImage", "Number of packets needed to send for each image required.", packetsPerImage);
  cmd.AddValue ("timeliness", "Timeliness requirement of queries", timeliness);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("tracing", "turn on ascii and pcap tracing", tracing);
  cmd.AddValue ("numNodes", "number of nodes", numNodes);
  cmd.AddValue ("sinkNode", "Receiver node number", sinkNode);
  cmd.AddValue ("sourceNode", "Sender node number", sourceNode);
	cmd.AddValue ("numReturnImages", "Default number of images for server to return.", numReturnImages);
	cmd.AddValue ("imageSizeBytes", "Size of each image (in bytes).", imageSizeBytes);
	cmd.AddValue ("delayPadding", "Time (in seconds) that server pauses in between successive images to prevent socket overload.", delayPadding);
  cmd.AddValue ("dataFilePath", "Path to print stats file to.", dataFilePath);
  cmd.AddValue ("sumSimFilename", "Name of file to get sum similarity packet numbers from.", sumSimFilename);
	cmd.AddValue ("clientNode", "Node where the client resides (testing - this should be temporary).", clientNode);
	cmd.AddValue ("serverNode", "Node where the server resides (testing - this should be temporary).", serverNode);
  cmd.AddValue ("runSeed", "Seed to set the random number generator.", runSeed);
  cmd.AddValue ("numRuns", "Total number of trials.", numRuns);
  cmd.AddValue ("numPacketsPerImage", "Number of packets neeed to send each image.", numPacketsPerImage);

  cmd.Parse (argc, argv);

  // set seed for random number generator
  SeedManager::SetRun(runSeed);

  // Convert to time object
  Time interPacketInterval = Seconds (interval);

  // disable fragmentation for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  // turn off RTS/CTS for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", 
                      StringValue (phyMode));

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
	controller.Set ("DataRate", DataRateValue (DataRate("2000000b/s")));
	//controller.Set ("SlotTime", TimeValue (MicroSeconds (1100)));
  controller.Set("SlotTime", TimeValue(MicroSeconds(5800))); // time to transmit one 1400 byte packet at 2Mbps is 5600 uSec
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
        else 
        {
          std::cout<<"ERROR:  Need to account for addresses higher than 1536 nodes (in scratch/qoi-query-flood.cc)\n";
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

          //secondByte = (((i+1)>>8)+1)&(uint8_t)255;
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
			oss << "qoi-query-flood_" << numNodes << "_Nodes.tr";
			std::string tr_name = oss.str();
 
			Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream (tr_name);
			tdma.EnableAsciiAll (stream);

      //wifiPhy.EnablePcap ("qoi-query-flood", devices);
      // Trace routing tables
      //Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("qoi-query-flood.routes", std::ios::out);
      //olsr.PrintRoutingTableAllEvery (Seconds (2), routingStream);

      // To do-- enable an IP-level trace that shows forwarding events only
    }


  // Output what we are doing
  //NS_LOG_UNCOND ("Testing from node " << sourceNode << " to " << sinkNode << " with grid distance " << distance);
//
// Create a QoiQueryFloodServer application on node one.
//
  uint32_t num_nodes = numNodes;
  uint16_t port = 9;  // well-known echo port number
  QoiQueryFloodServerHelper server (port, num_nodes);
  server.SetAttribute ("SumSimilarity", DoubleValue (sumSimilarity));
  server.SetAttribute ("Timeliness", TimeValue (Seconds(timeliness)));
  server.SetAttribute ("DataFilePath", StringValue (dataFilePath));
  server.SetAttribute ("SumSimFilename", StringValue (sumSimFilename));
	server.SetAttribute ("ImageSizeBytes", IntegerValue(imageSizeBytes));
  server.SetAttribute ("RunTime", TimeValue (Seconds(runTime)));
  server.SetAttribute ("RunSeed", UintegerValue (runSeed));
  server.SetAttribute ("NumRuns", UintegerValue (numRuns));
  server.SetAttribute ("DelayPadding", DoubleValue(delayPadding));
  ApplicationContainer apps = server.Install (c);
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (runTime));

//
// Create a QoiQueryFloodClient application to send UDP datagrams from node zero to
// node one.
//
//  Time interPacketInterval = Seconds (1.);
  QoiQueryFloodClientHelper client (port, num_nodes);
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("SumSimilarity", DoubleValue (sumSimilarity));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  client.SetAttribute ("Timeliness", TimeValue (Seconds(timeliness)));
  client.SetAttribute ("SumSimFilename", StringValue (sumSimFilename));
  client.SetAttribute ("ImageSizeBytes", IntegerValue (imageSizeBytes));
  client.SetAttribute ("RunTime", TimeValue (Seconds(runTime)));
  client.SetAttribute ("RunSeed", UintegerValue (runSeed));
  client.SetAttribute ("NumRuns", UintegerValue (numRuns));
  client.SetAttribute ("NumPacketsPerImage", IntegerValue (numPacketsPerImage));
  apps = client.Install(c);
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (runTime));

/*
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();
*/

  std::cout<<"Running for " << runTime << " seconds\n";

  Simulator::Stop (Seconds (runTime));
  Simulator::Run ();
  //flowMonitor->SerializeToXmlFile("Qoi-query-flood-flow-monitor.xml", true, true);
  Simulator::Destroy ();

  return 0;
}

