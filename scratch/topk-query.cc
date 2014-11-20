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
#include "ns3/dsdv-helper.h"
#include "ns3/topk-query-helper.h"
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
  std::string phyMode ("DsssRate1Mbps");
  double distance = 250;  // m
  uint32_t packetSize = 10; // bytes
  uint32_t numPackets = 1;
  uint32_t numNodes = 25;  // by default, 5x5
  uint32_t sinkNode = 0;
  uint32_t sourceNode = 24;
  double interval = 1.0; // seconds
  double sumSimilarity = 10.0;
  double timeliness = 5.0;
  bool verbose = false;
  bool tracing = false;
	uint16_t numReturnImages = 10;
	uint64_t imageSizeBytes = 888;
  double delayPadding;
  std::string dataFilePath;
  std::string sumSimFilename;
	int clientNode = 1;
	int serverNode = 1;
  int runSeed = 1;
  int numRuns = 1;

  CommandLine cmd;

	cmd.AddValue ("runTime", "Run time of simulation (in seconds)", runTime);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("distance", "distance (m)", distance);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "number of packets generated", numPackets);
  cmd.AddValue ("interval", "interval (seconds) between packets", interval);
  cmd.AddValue ("sumSimilarity", "Sum similarity requirement. Translated into number of images as defined in SumSimRequirements.csv.", sumSimilarity);
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

/*
  // The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;
  if (verbose)
    {
      wifi.EnableLogComponents ();  // Turn on all Wifi logging
    }

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  // set it to zero; otherwise, gain will be added
  wifiPhy.Set ("RxGain", DoubleValue (-10) ); 
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO); 

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add a non-QoS upper mac, and disable rate control
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));
  // Set it to adhoc mode
  wifiMac.SetType ("ns3::AdhocWifiMac");
*/
	Config::SetDefault ("ns3::SimpleWirelessChannel::MaxRange", DoubleValue (distance+5));
	// default allocation, each node gets a slot to transmit
	TdmaHelper tdma = TdmaHelper (c.GetN (), c.GetN ()); // in this case selected, numSlots = nodes
	/* can make custom allocation through simulation script
	 * will override default allocation
	 */
	/*tdma.SetSlots(4,
	    0,1,1,0,0,
	    1,0,0,0,0,
	    2,0,0,1,0,
	    3,0,0,0,1);*/
	// if TDMA slot assignment is through a file
	//TdmaHelper tdma = TdmaHelper ("tdmaSlots.txt");
	TdmaControllerHelper controller;
	controller.Set ("SlotTime", TimeValue (MicroSeconds (1100)));
	controller.Set ("GaurdTime", TimeValue (MicroSeconds (100)));
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

/*
	OlsrHelper olsr;
	Ipv4StaticRoutingHelper staticRouting;
	Ipv4ListRoutingHelper list;
	list.Add(staticRouting,0);
	list.Add(olsr, 10);	
*/

	DsdvHelper dsdv;
	dsdv.Set("PeriodicUpdateInterval", TimeValue (Seconds (5)));
	dsdv.Set("SettlingTime", TimeValue (Seconds (1.0)));

  // Install IP layer
	InternetStackHelper internet;
	//internet.SetRoutingHelper(list);
	internet.SetRoutingHelper(dsdv);
	internet.Install(c);

	// Assign IP addresses
  Ipv4AddressHelper address;
  NS_LOG_INFO ("Assign IP Addresses.");
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = address.Assign (devices);

	// Create static routes from A to C
	//Ptr<Ipv4StaticRouting> staticRoutingA = ipv4RoutingHelper.GetStaticRouting (ipv4A);
	// The ifIndex for this outbound route is 1; the first p2p link added
	//staticRoutingA->AddHostRouteTo (Ipv4Address ("192.168.1.1"), Ipv4Address ("10.1.1.2"), 1);
	//Ptr<Ipv4StaticRouting> staticRoutingB = ipv4RoutingHelper.GetStaticRouting (ipv4B);
	// The ifIndex we want on node B is 2; 0 corresponds to loopback, and 1 to the first point to point link
	//staticRoutingB->AddHostRouteTo (Ipv4Address ("192.168.1.1"), Ipv4Address ("10.1.1.6"), 2);

	// Install sockets
/*  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (c.Get (sinkNode), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));

  Ptr<Socket> source = Socket::CreateSocket (c.Get (sourceNode), tid);
  InetSocketAddress remote = InetSocketAddress (i.GetAddress (sinkNode, 0), 80);
  source->Connect (remote);
*/

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


  // Output what we are doing
  //NS_LOG_UNCOND ("Testing from node " << sourceNode << " to " << sinkNode << " with grid distance " << distance);
//
// Create a TopkQueryServer application on node one.
//
  uint16_t port = 9;  // well-known echo port number
  TopkQueryServerHelper server (port);
	server.SetAttribute("ImageSizeBytes", UintegerValue(imageSizeBytes));
  server.SetAttribute("DelayPadding", DoubleValue(delayPadding));
  ApplicationContainer apps = server.Install (c);
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (runTime));

//
// Create a TopkQueryClient application to send UDP datagrams from node zero to
// node one.
//
//  Time interPacketInterval = Seconds (1.);
  uint32_t num_nodes = numNodes;
  TopkQueryClientHelper client (port, num_nodes);
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("SumSimilarity", DoubleValue (sumSimilarity));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  client.SetAttribute ("Timeliness", TimeValue (Seconds(timeliness)));
  client.SetAttribute ("DataFilePath", StringValue (dataFilePath));
  client.SetAttribute ("SumSimFilename", StringValue (sumSimFilename));
  client.SetAttribute ("ImageSizeBytes", UintegerValue (imageSizeBytes));
  client.SetAttribute ("RunTime", TimeValue (Seconds(runTime)));
  client.SetAttribute ("RunSeed", UintegerValue (runSeed));
  client.SetAttribute ("NumRuns", UintegerValue (numRuns));
  apps = client.Install(c);
  apps.Start (Seconds (15.0));
  apps.Stop (Seconds (runTime));

/*
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();
*/

  std::cout<<"Running for " << runTime << " seconds\n";

  Simulator::Stop (Seconds (runTime));
  Simulator::Run ();
  //flowMonitor->SerializeToXmlFile("Topk-query-flow-monitor.xml", true, true);
  Simulator::Destroy ();

  return 0;
}

