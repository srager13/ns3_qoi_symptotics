/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
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
 */
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv4.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/node-list.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/tdma-mac-queue.h"
#include "topk-query-client.h"
#include "topk-query-server.h"
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TopkQueryClientApplication");
NS_OBJECT_ENSURE_REGISTERED (TopkQueryClient);

TypeId
TopkQueryClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TopkQueryClient")
    .SetParent<Application> ()
    .AddConstructor<TopkQueryClient> ()
    .AddAttribute ("SumSimilarity", 
                   "QoI Sum Similarity requirement of query.",
                   DoubleValue (100),
                   MakeDoubleAccessor (&TopkQueryClient::sum_similarity),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("RemoteAddress", 
                   "The destination Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&TopkQueryClient::m_peerAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort", 
                   "The destination port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&TopkQueryClient::m_peerPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&TopkQueryClient::m_txTrace))
    .AddAttribute ("Timeliness", 
                   "Timeliness constraint",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&TopkQueryClient::timeliness),
                   MakeTimeChecker ())
    .AddAttribute("DataFilePath",
                   "Path to the directory containing the .csv stats file.",
                   StringValue("./"),
                   MakeStringAccessor (&TopkQueryClient::DataFilePath),
                   MakeStringChecker())
    .AddAttribute("SumSimFilename",
                   "File to look up sum sim vs. number of packets needed (in $NS3_DIR).",
                   StringValue("SumSimRequirements.csv"),
                   MakeStringAccessor (&TopkQueryClient::SumSimFilename),
                   MakeStringChecker())
    .AddAttribute ("NumPacketsPerImage", 
                   "Number of packets for each image.",
                   IntegerValue (0),
                   MakeIntegerAccessor (&TopkQueryClient::num_packets_per_image),
                   MakeIntegerChecker<int> ())
    .AddAttribute ("NumNodes", 
                   "Number of nodes in the scenario.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&TopkQueryClient::SetNumNodes),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("ImageSizeKBytes", 
                   "The size of each image (all assumed to be the same) in kilobytes.",
                   UintegerValue (2000),
                   MakeUintegerAccessor (&TopkQueryClient::image_size_kbytes),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PacketSizeBytes", 
                   "The size of each packet (all assumed to be the same) in bytes.",
                   UintegerValue (1450000),
                   MakeUintegerAccessor (&TopkQueryClient::packet_size_bytes),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("RunTime", "Total time of simulation.",
                   TimeValue (Seconds (50)),
                   MakeTimeAccessor (&TopkQueryClient::run_time),
                   MakeTimeChecker ())
    .AddAttribute ("RunSeed", "Trial number.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&TopkQueryClient::run_seed),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("NumRuns", "Total number of trials.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&TopkQueryClient::num_runs),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("ChannelRate", "Rate of each channel (in Mbps).",
                   DoubleValue(2.0),
                   MakeDoubleAccessor(&TopkQueryClient::channel_rate),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("OneFlow", "Only send one flow from last node to first node for testing purposes.",
                   BooleanValue(false),
                   MakeBooleanAccessor(&TopkQueryClient::one_flow),
                   MakeBooleanChecker ())
    .AddAttribute ("ClientDebug", "",
                   BooleanValue(false),
                   MakeBooleanAccessor(&TopkQueryClient::TOPK_QUERY_CLIENT_DEBUG),
                   MakeBooleanChecker ())
  ;
  return tid;
}

// constructor
TopkQueryClient::TopkQueryClient () :
  timeliness(Time("1s")),
  run_time(Time("100s"))
{
  NS_LOG_FUNCTION (this);
  m_sendEvent = EventId ();
  m_data = 0;
  num_queries_issued = 0;
  num_queries_satisfied = 0;
  num_queries_unanswered = 0;
  num_packets_rcvd_late = 0;
  num_packets_rcvd = 0;
  num_packets_dropped = 0;
  sum_number_flows = 0.0;
  max_number_flows = 0.0;
  num_times_counted_flows = 0.0;
  avg_queue_size = 0.0;
  max_q_size = 0;

  rand_ss_dist = CreateObject<NormalRandomVariable>();
  rand_ss_dist->SetAttribute("Mean", DoubleValue(0));
  rand_ss_dist->SetAttribute("Variance", DoubleValue(3));

  Simulator::Schedule( NanoSeconds(1), &TopkQueryClient::Init, this );
}

void
TopkQueryClient::Init()
{
  //rand_interval = CreateObject<NormalRandomVariable>();
  rand_interval = CreateObject<ExponentialRandomVariable>();
  /*if( TOPK_QUERY_CLIENT_DEBUG )
  {
    std::cout<<"Setting rand_interval to mean of " << timeliness.GetSeconds() << "\n";
  }
*/
  rand_interval->SetAttribute("Mean", DoubleValue(5.0));

  sum_extra_time = Seconds(0);
  sum_query_time = Seconds(0);
  max_query_time = Seconds(0);
  
  char buf[100];

  std::ifstream sumsim_fd;
  sprintf(buf, "%s.csv", SumSimFilename.c_str());
  sumsim_fd.open( buf, std::ifstream::in );
  if( !sumsim_fd.is_open() )
  {
    avg_num_packets_rqrd = 10;
    std::cout << "Error opening sum similarity requirements file: " << buf << "...setting average number of images to default ("<<avg_num_packets_rqrd << ")\n";
  }
  else
  {
    bool found_requ = false;
    double sum_sim;
    int num_packets;
    while( sumsim_fd.good() )
    {
      sumsim_fd.getline( buf, 32, ',' );  
      char *pEnd;
      sum_sim = strtod(buf,&pEnd);
      sumsim_fd.getline( buf, 32, '\n' );  
      num_packets = num_packets_per_image*(int)strtod(buf,&pEnd);
   
      if( TOPK_QUERY_CLIENT_DEBUG )
      {
        std::cout<<"From file: sum_sim = " << sum_sim << ", num_packets = " << num_packets <<" (input sum similarity = " << sum_similarity << ")\n";
      }

      if( sum_sim >= sum_similarity )
      {
        avg_num_packets_rqrd = num_packets;
        if( TOPK_QUERY_CLIENT_DEBUG )
        {
          std::cout<<"Found requirement. Setting avg_num_packets_rqrd to " << avg_num_packets_rqrd << "\n";
        }
        found_requ = true;
        break;
      }
    }
    sumsim_fd.close();
    if( !found_requ )
    {
      avg_num_packets_rqrd = 10;
      std::cout << "Error: didn't find valid requirement in file...setting average number of packets to default ("<<avg_num_packets_rqrd << ")\n";
    }
  }

  Time delay = run_time-Seconds(1);
  if( TOPK_QUERY_CLIENT_DEBUG )
  {
    std::cout<<"Scheduling PrintStats with delay = " << delay.GetSeconds() << "...run_time = " << run_time.GetSeconds() << "\n";
  }
  Simulator::Schedule( delay, &TopkQueryClient::PrintStats, this );
}

TopkQueryClient::~TopkQueryClient()
{
  NS_LOG_FUNCTION (this);

  delete [] m_data;
  m_data = 0;
}

void 
TopkQueryClient::SetRemote (Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = ip;
  m_peerPort = port;
}

void 
TopkQueryClient::SetRemote (Ipv4Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = Address (ip);
  m_peerPort = port;
}

void 
TopkQueryClient::SetRemote (Ipv6Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = Address (ip);
  m_peerPort = port;
}

void
TopkQueryClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void 
TopkQueryClient::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  //Ipv4Address ipv4 = GetNodeAddress(i, num_nodes);
  //if( TOPK_QUERY_CLIENT_DEBUG )
  //{
    //std::cout<< "Node " << GetNode()->GetId() << ": Ipv4Address = " << ipv4 << "\n";
  //}

  if( m_socket == 0 )
  {
    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket (GetNode (), tid);
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_peerPort);
    m_socket->Bind (local);
  }

  m_socket->SetRecvCallback (MakeCallback (&TopkQueryClient::HandleRead, this));

  Simulator::Schedule (Seconds(1), &TopkQueryClient::CheckForTimedOutQueries, this);
}

void 
TopkQueryClient::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket = 0;
    }

  Simulator::Cancel (m_sendEvent);
}


void 
TopkQueryClient::CreateNewActiveQuery ( uint16_t server_node, uint16_t query_id, uint16_t num_packets_rqstd )
{
  TopkQuery new_query;
  new_query.id =  query_id;
  new_query.server_node = server_node;
  new_query.num_packets_rqstd = num_packets_rqstd;
  new_query.num_packets_rcvd = 0;
  new_query.start_time = Simulator::Now();
  new_query.deadline = Simulator::Now() + timeliness;
  new_query.max_TF = 0;
  new_query.max_TF_node = 0;
 
  num_queries_issued++;

  // add to list of active queries 
  active_queries.push_back( new_query ); 

  //Ptr<TopkQueryServer> svrPtr = NodeList::GetNode(dest_node)->GetApplication(0)->GetObject<TopkQueryServer>();
  //svrPtr->ReceiveQuery( GetNode()->GetId(), new_query.id, num_images_needed );

  // schedule sending next query
  //ScheduleTransmit(timeliness);

  if( TOPK_QUERY_CLIENT_DEBUG )
  {
    std::cout << "At time " << Simulator::Now ().GetSeconds () << "s client in node " << GetNode()->GetId() << 
                 " created query " << new_query.id << " from " <<
                 "node " << server_node << "\n";
  }
}

void
TopkQueryClient::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      num_packets_rcvd++;

      bool found_query = false;
      TopkQueryTag tag( -1, -1, 0, -1 );
      if( packet->RemovePacketTag(tag) )
      {
        // find active query in the list and update packets_rcvd
        for( uint16_t i = 0; i < active_queries.size(); i++ )
        {
          if( active_queries[i].server_node == tag.sending_node && active_queries[i].id == tag.query_id )
          {
            found_query = true;
            if( Simulator::Now() <= active_queries[i].deadline )
              active_queries[i].num_packets_rcvd++;
           
            // check to see if past deadline or done with query.  If so, go to check query to deal with it and start new one. 
            if( Simulator::Now() >= active_queries[i].deadline || active_queries[i].num_packets_rcvd == active_queries[i].num_packets_rqstd )
            {
              CheckQuery( active_queries[i].id, active_queries[i].server_node );
            }
            if( TOPK_QUERY_CLIENT_DEBUG )
            {
		  	      std::cout<<"Node " << GetNode()->GetId() << " received return packet number " << tag.packet_num << 
                        " from " << tag.sending_node << " at time " << Simulator::Now().GetSeconds() << "\n\tso far, got " << active_queries[i].num_packets_rcvd << "/" <<
                         active_queries[i].num_packets_rqstd << " of query " << active_queries[i].id << "\n";
            }

            MaxTFTag tf_tag(0, 0);
            if( packet->RemovePacketTag(tf_tag) )
            {
		  	      //std::cout<<"Node " << GetNode()->GetId() << " found TF tag at dest: max_TF = " << tf_tag.max_TF << 
                          //", node = " << tf_tag.max_TF_node << "\n";
              if( tf_tag.max_TF > active_queries[i].max_TF )
              {
                active_queries[i].max_TF = tf_tag.max_TF;
                active_queries[i].max_TF_node = tf_tag.max_TF_node;
              }
            }
          }
        } 
        if( !found_query )
        {
          if( TOPK_QUERY_CLIENT_DEBUG )
          {
            std::cout<<"Node " << GetNode()->GetId() << " received return packet number " << tag.packet_num << 
                      " from " << tag.sending_node << " at time " << Simulator::Now().GetSeconds() << "\n\tDid not find query " << tag.query_id << "\n";
          }
          num_packets_rcvd_late++;
        }
      }
      else
      {
        std::cout << "ERROR:  In TopkQueryClient::HandleRead() - Received packet with no TopkQueryTag\n";
      }
      if (InetSocketAddress::IsMatchingType (from))
      {
        if( TOPK_QUERY_CLIENT_DEBUG )
        {
          std::cout<< "At time " << Simulator::Now ().GetSeconds () << "s client received " << packet->GetSize () << " bytes from " <<
                     InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                     InetSocketAddress::ConvertFrom (from).GetPort () << "\n";
        }
      }
    }
}

void
TopkQueryClient::CheckQuery( uint16_t id, uint16_t server )
{

  if( TOPK_QUERY_CLIENT_DEBUG )
  {
    std::cout<<"Node " << GetNode()->GetId() << " in TopkQueryClient::CheckQuery() at time " << Simulator::Now().GetSeconds() << "\n";
    std::cout<<"\tchecking query with id = " << id << "\n";
  }

  bool found_query = false;
  // check query with id to see if it received enough packets
  for( uint16_t i = 0; i < active_queries.size(); i++ )
  {
    if( active_queries[i].server_node == server && active_queries[i].id == id )
    {
      found_query = true;
      if( TOPK_QUERY_CLIENT_DEBUG )
      {
       std::cout<<"\tfound the query; received " << active_queries[i].num_packets_rcvd << " / " << active_queries[i].num_packets_rqstd << " packets\n";
      }
      double perc_rcvd = (double)active_queries[i].num_packets_rcvd/(double)active_queries[i].num_packets_rqstd;
      //if( active_queries[i].num_packets_rcvd >= active_queries[i].num_packets_rqstd )
      //if( perc_rcvd >= 0.99 ) // consider satisfied if we get 98% of the requested number of packets
      if( perc_rcvd == 1.0 ) // consider satisfied if we get 98% of the requested number of packets
      {
        num_queries_satisfied++;
        sum_extra_time += active_queries[i].deadline - Simulator::Now();
        sum_query_time += Simulator::Now() - active_queries[i].start_time;
        if( Simulator::Now() - active_queries[i].start_time > max_query_time )
        {
            max_query_time = Simulator::Now() - active_queries[i].start_time;

            if( TOPK_QUERY_CLIENT_DEBUG )
            {
              std::cout<<"Max query time: " << max_query_time.GetSeconds() << "...source = " << active_queries[i].server_node << ", dest = " << GetNode()->GetId() << ", max TF = " << active_queries[i].max_TF << ", max TF node = " << active_queries[i].max_TF_node << "\n";
            }
        }

        if( Simulator::Now() < run_time - timeliness-Seconds(110) )
        {
          delays.push_back( (Simulator::Now() - active_queries[i].start_time).GetSeconds() );
          src_nodes.push_back( active_queries[i].server_node );
        }
        
      }
      else
      {
        if( TOPK_QUERY_CLIENT_DEBUG )
        {
          std::cout<<"Unsatisfied Query: Time = " << (Simulator::Now()-active_queries[i].start_time).GetSeconds() << ", source = " << active_queries[i].server_node << ", dest = " << GetNode()->GetId() << ", max TF = " << active_queries[i].max_TF << ", max TF node = " << active_queries[i].max_TF_node << "\n";
        }
      }

      if( Simulator::Now() > Seconds(50) )
      {  
        FlowMaxTF fmtf;
        fmtf.orig_node = active_queries[i].server_node;
        fmtf.max_TF = active_queries[i].max_TF;
        fmtf.timestamp = Simulator::Now();
        flow_max_TFs.push_back(fmtf);
      }
      //if( active_queries[i].max_TF == 0 )
      //{
        //std::cout<<"Time " << Simulator::Now().GetSeconds() << ": Flow TF = 0; Source = " << active_queries[i].server_node << "; Dest = " << GetNode()->GetId() << "\n";
      //}

      if( active_queries[i].num_packets_rcvd == 0 )
      {
        num_queries_unanswered++;
      }

      // either way, remove it from the list of active queries
      active_queries.erase(active_queries.begin() + i);
    }
  }

  //ScheduleTransmit(Seconds(0));
  
  if( !found_query )
  {
    std::cout<<"ERROR:  Node " << GetNode()->GetId() << " did not find matching query with id = " << id << " at time " << Simulator::Now().GetSeconds() << "\n";
  }

}

void
TopkQueryClient::CheckForTimedOutQueries()
{
  //if( GetNode()->GetId() == 0 )
  //{
    //std::cout<< "Time = " << Simulator::Now().GetSeconds() << "\n";
  //}
  for( uint16_t i = 0; i < active_queries.size(); i++ )
  {
    if( Simulator::Now() > active_queries[i].deadline )
    {
      CheckQuery( active_queries[i].id, active_queries[i].server_node );
    }
  }
  Simulator::Schedule (Seconds(1), &TopkQueryClient::CheckForTimedOutQueries, this);

  //NetDeviceContainer::Get(GetNode()->GetId())
  //Ptr<TdmaMacQueue> tdmaQPtr = NodeList::GetNode(GetNode()->GetId())->GetObject<TdmaMacQueue>();
  //uint32_t num_flows = tdmaQPtr->GetNumberActiveFlows( );
  
  //std::cout<<"Node " << GetNode()->GetId() << ": number of flows currently being forwarded = " << num_flows << "\n"; 
}

void
TopkQueryClient::IncrementNumPacketsDropped()
{
  num_packets_dropped++;
}

// called from within TDMA mac queue
void 
TopkQueryClient::UpdateQueueStats( int current_num_flows, double avg_q_size, uint32_t current_q_size, double sum_num_flows, double num_times_counted, double max_num_flows )
{
  // only keep track of stats while simulation is in an equlibrium.
  //  Topk server stops creating new queries with 110 seconds left, so we stop at that point.
  //  otherwise, we would end up with a number of 0 TF values
  if( Simulator::Now() < run_time - timeliness - Seconds(110) )
  {
    TFs.push_back(current_num_flows);

    if( current_q_size > max_q_size )
      max_q_size = current_q_size;
    avg_queue_size = avg_q_size;
    sum_number_flows = sum_num_flows;
    if( max_num_flows > max_number_flows )
      max_number_flows = max_num_flows;
    num_times_counted_flows = num_times_counted;
  }
} 

void 
TopkQueryClient::PrintStats()
{
  char buf[1024];
  FILE *stats_fd;

  #ifdef SMALL_QUEUES
    sprintf(buf, "%s/TopkQueryClientStatsSmallQueues.csv", DataFilePath.c_str() );
  #else
    sprintf(buf, "%s/TopkQueryClientStats.csv", DataFilePath.c_str() );
  #endif
  stats_fd = fopen(buf, "a");
  if( stats_fd == NULL )
  {
    std::cout << "Error opening stats file: " << buf << "\n";
    return;
  }
  
  if( TOPK_QUERY_CLIENT_DEBUG )
  {
    std::cout<< "Printing Stats to " << buf << "....at time " << Simulator::Now().GetSeconds() << "\n";
  }

  double avg_extra_time = 0.0;
  double avg_query_time = 0.0;
  if( (double)num_queries_satisfied >= 0.9*(double)num_queries_issued )
  {
    avg_extra_time = sum_extra_time.GetSeconds()/(double)num_queries_satisfied;
    avg_query_time = sum_query_time.GetSeconds()/(double)num_queries_satisfied;
  }
  double perc_queries_satisfied = 0;
  if( num_queries_issued > 0 )
    perc_queries_satisfied = (double)num_queries_satisfied/(double)num_queries_issued; 

  int one_flow_run = 0;
  if( one_flow )
    one_flow_run = 1;
                   // 1   2   3   4   5   6  6b   7      8     9   10  11  12  13  14    15   16     17  18   19    20   21  22   23  24
  fprintf(stats_fd, "%i, %i, %i, %i, %i, %i, %i, %.1f, %.2f, %.1f, %i, %i, %i, %i, %i, %.3f, %.3f, %.2f, %i, %.2f, %.2f, %i, %i, %.3f, %.1f\n",
    GetNode()->GetId(), // 1 i 
    num_nodes, // 2 i
    num_queries_satisfied, // 3 i
    num_queries_issued, // 4 i
    num_packets_rcvd, // 5 i
    image_size_kbytes, // 6 i
    packet_size_bytes, // 6b i
    run_time.GetSeconds(), // 7 f
    timeliness.GetSeconds(), // 8 f
    sum_similarity, // 9 f
    run_seed, // 10 i
    num_runs, // 11 i
    num_queries_unanswered, // 12 i
    num_packets_rcvd_late,  // 13 i
    avg_num_packets_rqrd, // 14 i
    avg_query_time, // 15 f
    avg_extra_time, // 16 f
    perc_queries_satisfied, // 17 f
    num_packets_dropped, // 18 i
    avg_queue_size, // 19 .2f
    sum_number_flows/num_times_counted_flows, // 20 .2f  - avg num flows
    max_q_size,  // 21
    one_flow_run, // 22 
    max_query_time.GetSeconds(), // 23
    max_number_flows // 24
  );

  fclose(stats_fd);
        
  FILE *delay_fd;

  sprintf(buf, "%s/Node_delays.csv", DataFilePath.c_str() );
  delay_fd = fopen(buf, "a");
  if( delay_fd == NULL )
  {
    std::cout << "Error opening delay output file: " << buf << "\n";
    return;
  }
  for( uint16_t i = 0; i < delays.size(); i++ )
  {
    fprintf( delay_fd, "%i, %0.3f\n", src_nodes[i], delays[i] );
  }
  fclose(delay_fd);
  
  FILE *TF_fd;

  sprintf(buf, "%s/Node_TFs.csv", DataFilePath.c_str() );
  TF_fd = fopen(buf, "a");
  if( TF_fd == NULL )
  {
    std::cout << "Error opening TF output file: " << buf << "\n";
    return;
  }
  for( uint16_t i = 0; i < TFs.size(); i++ )
  {
    fprintf( TF_fd, "%i, %i\n", GetNode()->GetId(), TFs[i] );
  }
  fclose(TF_fd);
  
  FILE *FlowTF_fd;

  sprintf(buf, "%s/Flow_TFs.csv", DataFilePath.c_str() );
  FlowTF_fd = fopen(buf, "a");
  if( FlowTF_fd == NULL )
  {
    std::cout << "Error opening Flow_TF output file: " << buf << "\n";
    return;
  }
  for( uint16_t i = 0; i < flow_max_TFs.size(); i++ )
  {
    fprintf( FlowTF_fd, "%i, %i, %i, %.2f\n", flow_max_TFs[i].orig_node, flow_max_TFs[i].max_TF, GetNode()->GetId(), flow_max_TFs[i].timestamp.GetSeconds() );
  }
  fclose(FlowTF_fd);
}


} // Namespace ns3
