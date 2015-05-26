/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/distUnivSched-module.h"
#include "ns3/distUnivSched-helper.h"
#include "ns3/node-container.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h" 
#include "ns3/v4ping-helper.h"
#include <iostream>
#include <cmath>

using namespace ns3;
/**
 * \brief Test script.
 * 
 * This script creates 2 nodes and sends packet from node 1 to node 2 over Wifi
 * 
 * [Node 1] -- Packet --> [Node 2] 
 * 
 * Application: send packets from 1 to 2
 */
class DistUnivSchedPingExample
{
public:
  DistUnivSchedPingExample ();
  /// Configure script parameters, \return true on successful configuration
  bool Configure (int argc, char **argv);
  /// Run simulation
  void Run ();
  /// Report results
  void Report (std::ostream & os);
  
private:
  ///\name parameters
  //\{
  /// Number of nodes
  uint32_t size;
  /// Distance between nodes, meters
  double step;
  /// Simulation time, seconds
  double totalTime;
  /// Write per-device PCAP traces if true
  bool pcap;
  //\}
  
  ///\name network
  //\{
  NodeContainer nodes;
  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;
  //\}
  
private:
  void CreateNodes ();
  void CreateDevices ();
  void InstallInternetStack ();
  void InstallApplications ();
};

int 
main (int argc, char *argv[])
{
  
  DistUnivSchedPingExample test;
  if (!test.Configure (argc, argv))
    NS_FATAL_ERROR ("Configuration failed. Aborted.");
  
  //dus::RoutingProtocol distUnivScheds[2];

  //distUnivScheds[0].StartTimeSlot();

  test.Run ();
  test.Report (std::cout);
  return 0;
}

//-----------------------------------------------------------------------------
DistUnivSchedPingExample::DistUnivSchedPingExample () :
size (2),
step (100),
totalTime (1),
pcap (true)
{
}

bool
DistUnivSchedPingExample::Configure (int argc, char **argv)
{
  // Enable DIST_UNIV_SCHED logs by default. Comment this if too noisy
  // LogComponentEnable("DistUnivSchedRoutingProtocol", LOG_LEVEL_ALL);
  
  SeedManager::SetSeed (12345);
  CommandLine cmd;
  
  cmd.AddValue ("pcap", "Write PCAP traces.", pcap);
  cmd.AddValue ("size", "Number of nodes.", size);
  cmd.AddValue ("time", "Simulation time, s.", totalTime);
  cmd.AddValue ("step", "Grid step, m", step);
  
   cmd.Parse (argc, argv);
  return true;
}

void
DistUnivSchedPingExample::Run ()
{
  //  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (1)); // enable rts cts all the time.
  CreateNodes ();
  CreateDevices ();
  InstallInternetStack ();
  InstallApplications ();
  
  std::cout << "Starting simulation for " << totalTime << " s ...\n";
  
  Simulator::Stop (Seconds (totalTime));
  Simulator::Run ();
  Simulator::Destroy ();
}

void
DistUnivSchedPingExample::Report (std::ostream &)
{ 
  std::cout << "In DistUnivSchedPingExample::Report()\n";
}

void
DistUnivSchedPingExample::CreateNodes ()
{
  std::cout << "Creating " << (unsigned)size << " nodes " << step << " m apart.\n";
  nodes.Create (size);
  // Name nodes
  for (uint32_t i = 0; i < size; ++i)
  {
    std::ostringstream os;
    os << "node-" << i;
    Names::Add (os.str (), nodes.Get (i));
  }
  // Create static grid
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (step),
                                 "DeltaY", DoubleValue (0),
                                 "GridWidth", UintegerValue (size),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);
  std::cout << "Set Mobility\n";
  
}

void
DistUnivSchedPingExample::CreateDevices ()
{
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate6Mbps"), "RtsCtsThreshold", UintegerValue (0));
  devices = wifi.Install (wifiPhy, wifiMac, nodes); 
  
  if (pcap)
  {
    wifiPhy.EnablePcapAll (std::string ("distUnivSched"));
  }
  std::cout << "done in CreateDevices().\n";
}

void
DistUnivSchedPingExample::InstallInternetStack ()
{
  std::cout<<"  in InstallInternetStack() before creating DistUnivSchedHelper\n";
  DistUnivSchedHelper dusHelper; //distUnivSched;
  std::cout<<"  in InstallInternetStack() after creating DistUnivSchedHelper\n";
   //you can configure DIST_UNIV_SCHED attributes here using distUnivSched.Set(name, value)
  InternetStackHelper stack;
  stack.SetRoutingHelper (dusHelper);  //distUnivSched);  //has effect on the next Install ()
  std::cout <<"  after SetRoutingHelper (distUnivSched) call \n";
  stack.Install (nodes);
  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.0.0.0");
  interfaces = address.Assign (devices);
  
  std::cout << "done in InstallInternerStack().\n";
}

void
DistUnivSchedPingExample::InstallApplications ()
{
  V4PingHelper ping (interfaces.GetAddress (size - 1));
  ping.SetAttribute ("Verbose", BooleanValue (true));
  
  ApplicationContainer p = ping.Install (nodes.Get (0));
  p.Start (Seconds (0));
  p.Stop (Seconds (totalTime) - Seconds (0.001));
  
   //move node away
  Ptr<Node> node = nodes.Get (size/2);
  Ptr<MobilityModel> mob = node->GetObject<MobilityModel> ();
  Simulator::Schedule (Seconds (totalTime/3), &MobilityModel::SetPosition, mob, Vector (1e5, 1e5, 1e5));
  
  std::cout << "done in InstallApplications().\n";
}

