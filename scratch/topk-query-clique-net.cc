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
#include <stdio.h>

#define Q_SAT_PERC_INDEX 17

NS_LOG_COMPONENT_DEFINE ("TopkQueryExample");

using namespace ns3;
  
bool IsScalable( std::string dataFilePath, double q_comp_thresh, bool satAllQueries, bool oneFlow );

int FindNumImages( std::string SumSimFilename, double sum_similarity );

int main (int argc, char *argv[])
{
	double runTime = 100;
  double distance = 250;  // m
  uint32_t numNodes = 25;  // by default, 5x5
  uint32_t lastNumNodes = 0;
  uint32_t nodeInc = 5;
  uint32_t minNumNodes = 3;
  double sumSimilarity = 0.6;
  double timeliness = 5.0;
  bool tracing = false;
	uint64_t imageSizeKBytes = 2000;
	uint64_t packetSizeBytes = 2000;
  double delayPadding;
  std::string dataFilePath;
  std::string dataFilePath_2;
  std::string sumSimFilename;
  int runSeed = 1;
  int numRuns = 1;
  int numPacketsPerImage = 1;
  double channelRate = 2; // in Mbps
  bool oneFlow = false;
  double q_comp_thresh = 0.9;
  bool satAllQueries = false;

  CommandLine cmd;

	cmd.AddValue ("runTime", "Run time of simulation (in seconds)", runTime);
  cmd.AddValue ("distance", "distance (m)", distance);
  cmd.AddValue ("sumSimilarity", "Sum similarity requirement. Translated into number of images as defined in SumSimRequirements.csv.", sumSimilarity);
  cmd.AddValue ("timeliness", "Timeliness requirement of queries", timeliness);
  cmd.AddValue ("tracing", "turn on ascii and pcap tracing", tracing);
  cmd.AddValue ("numNodes", "number of nodes", numNodes);
	cmd.AddValue ("imageSizeKBytes", "Size of each image (in kilobytes).", imageSizeKBytes);
	cmd.AddValue ("packetSizeBytes", "Size of each packet (in bytes).", packetSizeBytes);
	cmd.AddValue ("delayPadding", "Time (in seconds) that server pauses in between successive images to prevent socket overload.", delayPadding);
  cmd.AddValue ("dataFilePath", "Path to print stats file to.", dataFilePath);
  cmd.AddValue ("dataFilePath_2", "Path to print stats file to.", dataFilePath_2);
  cmd.AddValue ("sumSimFilename", "Name of file to get sum similarity packet numbers from.", sumSimFilename);
  cmd.AddValue ("runSeed", "Seed to set the random number generator.", runSeed);
  cmd.AddValue ("numRuns", "Total number of trials.", numRuns);
  cmd.AddValue ("numPacketsPerImage", "Number of packets neeed to send each image.", numPacketsPerImage);
  cmd.AddValue ("channelRate", "Rate of each wireless channel (in Mbps).", channelRate);
  cmd.AddValue ("oneFlow", "True to only send one flow from last node to first node for testing.", oneFlow);
  cmd.AddValue ("satAllQueries", "Set true to set scalability defined by all nodes satisfying queries, not average.", satAllQueries);

  cmd.Parse (argc, argv);

  // set initial guess according to analysis
  int num_images = FindNumImages( sumSimFilename, sumSimilarity );
  if( numNodes == 0 )
  {
    numNodes = ((channelRate*1000000)*timeliness)/(num_images*imageSizeKBytes*8000);
  }
	if( numNodes < minNumNodes )
		numNodes = minNumNodes;
  std::cout<<"Initial guess = " << numNodes << ", num images = " << num_images <<"\n";

  bool found_limit = false;
  bool last_trial_scalable = false;
  bool first_run = true;
    
  char buf[1024];

  while( !found_limit )
  {
    // clear stats file
    sprintf(buf, "%s/TopkQueryClientStats.csv", dataFilePath.c_str());
    std::ofstream stats_file;
    stats_file.open( buf, std::ofstream::out | std::ofstream::trunc );
		stats_file.close();
    // set seed for random number generator
    SeedManager::SetRun(runSeed);

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
//    sprintf(buf, "./tdmaSlotAssignFiles/tdmaSlots_lineNet_%i.csv", numNodes);
    //TdmaHelper tdma = TdmaHelper (buf);
    TdmaHelper tdma = TdmaHelper (c.GetN (), c.GetN ()); // in this case selected, numSlots = nodes

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


    // Set node positions (no mobility)
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::UniformDiscPositionAllocator",
                                   "rho", DoubleValue((distance-5.0)/2.0),
                                   "X", DoubleValue ((distance-5.0)/2.0),
                                   "Y", DoubleValue ((distance-5.0)/2.0) );
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
          //  send to dest
     
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
    client.SetAttribute ("OneFlow", BooleanValue(oneFlow));
    apps = client.Install(c);
    apps.Start (Seconds (2.0));
    apps.Stop (Seconds (runTime-100.0));

  /*
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();
  */

    //std::cout<<"Running for " << runTime << " seconds\n";
    //std::cout<<"Sum similarity = " << sumSimilarity << "\n";

    Simulator::Stop (Seconds (runTime));
    Simulator::Run ();
    //flowMonitor->SerializeToXmlFile("Topk-query-flow-monitor.xml", true, true);
    Simulator::Destroy ();

    bool this_trial_scalable = IsScalable( dataFilePath, q_comp_thresh, satAllQueries, oneFlow );

    if( this_trial_scalable )
    {
      std::cout<<"Num nodes = " << numNodes << ": Scalable\n";
    }
    else
    {
      std::cout<<"Num nodes = " << numNodes << ": Not scalable\n";
    }
      

    // Last = T && This = F
    if( last_trial_scalable && !this_trial_scalable ) // can't be first run, had to be incrementing
    {
      found_limit = true;
    }
    // Last = F && This = F
    if( !last_trial_scalable && !this_trial_scalable )
    {
      // either first run or had been decrementing.  either way, haven't found limit yet.
      lastNumNodes = numNodes;
      numNodes = numNodes - nodeInc;
    }
    // Last = T && This = T
    if( last_trial_scalable && this_trial_scalable )
    {
      // had to be incrementing. haven't found limit yet.
      lastNumNodes = numNodes;
      numNodes = numNodes + nodeInc;
    }
    // Last = F && This = T
    if( !last_trial_scalable && this_trial_scalable ) // first time run?
    {
      if( !first_run )
      {
        // not first time run. had to be decrementing - found limit
        lastNumNodes = numNodes;
        found_limit = true;
      }
      else
      {
        // first time run. don't know anything yet. need to increment and test again.
        lastNumNodes = numNodes;
        numNodes = numNodes + nodeInc;
      }
    }
    
    last_trial_scalable = this_trial_scalable;

    if( lastNumNodes < minNumNodes )
    {
      lastNumNodes = 0;
      found_limit = true;
    }

    first_run = false;
  }

  sprintf(buf, "%s/Scalability.csv", dataFilePath_2.c_str());
  std::ofstream scal_file;
  scal_file.open( buf, std::ofstream::app );
  scal_file << sumSimilarity << ", " << timeliness << ", " << lastNumNodes << "\n";
 
  return 0;
}

bool IsScalable( std::string dataFilePath, double q_comp_thresh, bool satAllQueries, bool oneFlow )
{
  std::vector< std::vector<double> > stats;  
  char buf[1024];
  sprintf(buf, "%s/TopkQueryClientStats.csv", dataFilePath.c_str());
  std::ifstream stats_file (buf);
  std::string line;
  int i = 0;
  while( stats_file.good() )
  {
    std::vector<double> new_stats_line = std::vector<double>();
    stats.push_back( new_stats_line );
    getline( stats_file, line );
    char *line_dup = strdup(line.c_str());
    char *s;
    s = strtok( line_dup, "," );
    while (s != NULL)
    {
      //std::cout<< s << "\n";
      double val = atof(s);
      stats[i].push_back(val);
      s = strtok( NULL, ",");
    }
    i++;
  }
	stats_file.close();

  if( oneFlow )
  {
    if( stats[0][Q_SAT_PERC_INDEX] >= q_comp_thresh )
      return true;
    else
      return false;
  }

  // now need to check the performance to determine if it is scalable or not
  double query_sat_perc = 0;
  for( uint16_t i = 0; i < stats.size()-1; i++ )
  {
    //std::cout<<"size of stats[" << i << "] = " << stats[i].size() << "\n";
   // std::cout<<"\tquery_sat_perc = " << stats[i][Q_SAT_PERC_INDEX] << "\n";
    query_sat_perc += stats[i][Q_SAT_PERC_INDEX]; // index of query satisfied percentage = 16


    if( satAllQueries && stats[i][Q_SAT_PERC_INDEX] < q_comp_thresh )
      return false;
  }
  if( stats.size() == 0 )
  {
    std::cout<<"ERROR:  Did not read any stats in topk-query-line-net.cc";
  }
  else
  {
    query_sat_perc = query_sat_perc/(stats.size()-1);
//    std::cout<<"Query sat perc = " << query_sat_perc << " stats.size-1 = " << stats.size()-1 << "\n";
  }
  if( query_sat_perc >= q_comp_thresh )
  {
    return true;
  }
  return false;
}

int FindNumImages( std::string SumSimFilename, double sum_similarity )
{
  char buf[100];

  std::ifstream sumsim_fd;
  sprintf(buf, "%s.csv", SumSimFilename.c_str());
  sumsim_fd.open( buf, std::ifstream::in );
  if( !sumsim_fd.is_open() )
  {
    std::cout << "Error opening sum similarity requirements file: " << buf << "...returning -1\n";
    return -1;
  }
  else
  {
    bool found_requ = false;
    double sum_sim;
    int num_images;
    while( sumsim_fd.good() )
    {
      sumsim_fd.getline( buf, 32, ',' );  
      char *pEnd;
      sum_sim = strtod(buf,&pEnd);
      sumsim_fd.getline( buf, 32, '\n' );  
      num_images = (int)strtod(buf,&pEnd);
   
      if( TOPK_QUERY_CLIENT_DEBUG )
      {
        std::cout<<"From file: sum_sim = " << sum_sim << ", num_images = " << num_images <<" (input sum similarity = " << sum_similarity << ")\n";
      }

      if( sum_sim >= sum_similarity )
      {
        return num_images;
      }
    }
    if( !found_requ )
    {
      std::cout << "Error: didn't find valid requirement in file...returning -1\n";
    }
  }

  return -1;

}
