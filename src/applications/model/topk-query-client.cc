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
    .AddAttribute ("Interval", 
                   "The time to wait between packets",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&TopkQueryClient::m_interval),
                   MakeTimeChecker ())
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
    .AddAttribute ("ImageSizeBytes", 
                   "The size of each image (all assumed to be the same) in bytes.",
                   UintegerValue (2000000),
                   MakeUintegerAccessor (&TopkQueryClient::image_size_bytes),
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
  ;
  return tid;
}

// constructor
TopkQueryClient::TopkQueryClient () :
  m_interval(Time("10s")),
  timeliness(Time("1s")),
  run_time(Time("100s"))
{
  NS_LOG_FUNCTION (this);
  m_sent = 0;
  m_sendEvent = EventId ();
  m_data = 0;
  num_queries_issued = 0;
  num_queries_satisfied = 0;
  num_queries_unanswered = 0;
  num_packets_rcvd_late = 0;
  num_packets_rcvd = 0;
  query_ids = 1;
  num_packets_dropped = 0;

  rand_dest = CreateObject<UniformRandomVariable>();

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
  //rand_interval->SetAttribute("Mean", DoubleValue(timeliness.GetSeconds()+1.0));
  //rand_interval->SetAttribute("Variance", DoubleValue(3.0));
  
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
    int num_images;
    while( sumsim_fd.good() )
    {
      sumsim_fd.getline( buf, 32, ',' );  
      char *pEnd;
      sum_sim = strtod(buf,&pEnd);
      sumsim_fd.getline( buf, 32, '\n' );  
      num_images = num_packets_per_image*(int)strtod(buf,&pEnd);
   
      //if( TOPK_QUERY_CLIENT_DEBUG )
      {
        std::cout<<"From file: sum_sim = " << sum_sim << ", num_images = " << num_images <<" (input sum similarity = " << sum_similarity << ")\n";
      }

      if( sum_sim >= sum_similarity )
      {
        avg_num_packets_rqrd = num_images;
     //   if( TOPK_QUERY_CLIENT_DEBUG )
        {
          std::cout<<"Found requirement. Setting avg_num_packets_rqrd to " << avg_num_packets_rqrd << "\n";
        }
        found_requ = true;
        break;
      }
    }
    if( !found_requ )
    {
      avg_num_packets_rqrd = 10;
      std::cout << "Error: didn't find valid requirement in file...setting average number of images to default ("<<avg_num_packets_rqrd << ")\n";
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

  Time next = timeliness + Seconds(rand_interval->GetValue());
  //Time next = timeliness;
  if( next.GetSeconds() < 0.0 )
    next = timeliness;
  ScheduleTransmit (next);
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
TopkQueryClient::ScheduleTransmit (Time dt)
{
  //std::cout<<"Scheduling Send with delay = " << dt.GetSeconds() << "\n";
  NS_LOG_FUNCTION (this << dt);
  m_sendEvent = Simulator::Schedule (dt, &TopkQueryClient::Send, this);
}

void 
TopkQueryClient::Send (void)
{
  if( ONE_FLOW_DEBUG && GetNode()->GetId() != 0 )
  {
    return;
  } 
  // num images set from sum similarity requirement input.  
  // Here we vary slightly around that number just to provide a little bit of randomness
  int num_images_needed = avg_num_packets_rqrd; 

  // REMOVE
  //int rand_change =  (int)rand_ss_dist->GetValue();
  int rand_change = 0;
  num_images_needed += rand_change;
  if( TOPK_QUERY_CLIENT_DEBUG )
  {
    std::cout<<"rand_change = " << rand_change << ", resulting required image num = " << num_images_needed << "\n";
  }
  // choose random destination (not self)
  uint16_t dest;
  do
  {
    dest = rand_dest->GetInteger(0, num_nodes-1);
  }while( dest == GetNode()->GetId() );

  if( ONE_FLOW_DEBUG )
  {
    dest = num_nodes-1;
  }
 
  TopkQuery new_query;
  new_query.id =  query_ids;
  new_query.server_node = dest;
  new_query.num_images_rqstd = num_images_needed;
  new_query.num_images_rcvd = 0;
  new_query.start_time = Simulator::Now();
  new_query.deadline = Simulator::Now() + timeliness;
  // only count the issued queries that have a chance to finish before the end of the simulation
  if( Simulator::Now() + timeliness < run_time-Seconds(100) )
  {
    num_queries_issued++;
    // Schedule function to check on this query at the deadline to see if it was satisfied
    Simulator::Schedule( timeliness, &TopkQueryClient::CheckQuery, this, new_query.id, dest );
    if( TOPK_QUERY_CLIENT_DEBUG )
    {
      std::cout<<"sending query with id = " << new_query.id << "; number sent = " << num_queries_issued << "\n";
    }
  }
  else
  {
    return;
  }
 

  if( m_socket == 0 )
  {
		std::cout << "ERROR: socket[" << dest << "] == 0 in node " << GetNode()->GetId() << "\n";
  }

  uint32_t dest_node = dest;
  
  // add to list of active queries 
  active_queries.push_back( new_query ); 
  
  UpdateQueryIds();

  Ptr<TopkQueryServer> svrPtr = NodeList::GetNode(dest_node)->GetApplication(0)->GetObject<TopkQueryServer>();
  svrPtr->ReceiveQuery( GetNode()->GetId(), new_query.id, num_images_needed);

  ++m_sent;
  if( TOPK_QUERY_CLIENT_DEBUG )
  {
    std::cout << "At time " << Simulator::Now ().GetSeconds () << "s client in node " << GetNode()->GetId() << 
                 " sent " << m_size << " bytes to " <<
                 "node " << dest << "\n";
  }

  Time next = timeliness + Seconds(rand_interval->GetValue());
  //Time next = timeliness + Seconds(5.0);
  if( next.GetSeconds() < 0.0 )
    next = timeliness;
  if( Simulator::Now() + next + timeliness < run_time - Seconds(110) ) // adding 10 seconds for buffer
  {
    if( TOPK_QUERY_CLIENT_DEBUG )
    {
      std::cout<<"delay until schedule next send = " << next.GetSeconds() << "\n";
    }
    ScheduleTransmit (next);
  }
  else
  {
    if( TOPK_QUERY_CLIENT_DEBUG )
    {
      std::cout<<"not enough time for another send\n";
    }
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
        // find active query in the list and update images_rcvd
        for( uint16_t i = 0; i < active_queries.size(); i++ )
        {
          if( active_queries[i].server_node == tag.sending_node && active_queries[i].id == tag.query_id )
          {
            found_query = true;
            active_queries[i].num_images_rcvd++;
            if( TOPK_QUERY_CLIENT_DEBUG )
            {
		  	      std::cout<<"Node " << GetNode()->GetId() << " received return packet number " << tag.image_num << 
                        " from " << tag.sending_node << " at time " << Simulator::Now().GetSeconds() << "\n\tso far, got " << active_queries[i].num_images_rcvd << "/" <<
                         active_queries[i].num_images_rqstd << " of query " << active_queries[i].id << "\n";
            }
          }
        } 
        if( !found_query )
        {
          if( TOPK_QUERY_CLIENT_DEBUG )
          {
            std::cout<<"Node " << GetNode()->GetId() << " received return packet number " << tag.image_num << 
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
    std::cout<<"In TopkQueryClient::CheckQuery() at time " << Simulator::Now().GetSeconds() << "\n";
    std::cout<<"\tchecking query with id = " << id << "\n";
  }

  bool found_query = false;
  // check query with id to see if it received enough images
  for( uint16_t i = 0; i < active_queries.size(); i++ )
  {
    if( active_queries[i].server_node == server && active_queries[i].id == id )
    {
      found_query = true;
      if( TOPK_QUERY_CLIENT_DEBUG )
      {
       std::cout<<"\tfound the query; received " << active_queries[i].num_images_rcvd << " / " << active_queries[i].num_images_rqstd << " images\n";
      }
      double perc_rcvd = (double)active_queries[i].num_images_rcvd/(double)active_queries[i].num_images_rqstd;
      //if( active_queries[i].num_images_rcvd >= active_queries[i].num_images_rqstd )
      if( perc_rcvd >= 0.9 ) // consider satisfied if we get 90% of the requested number of images
      {
        num_queries_satisfied++;
      }

      if( active_queries[i].num_images_rcvd == 0 )
      {
        num_queries_unanswered++;
      }

      // either way, remove it from the list of active queries
      active_queries.erase(active_queries.begin() + i);
    }
  }
  
  if( !found_query )
  {
    std::cout<<"ERROR:  Node " << GetNode()->GetId() << " did not find matching query with id = " << id << " at time " << Simulator::Now().GetSeconds() << "\n";
  }

  return;
}

void
TopkQueryClient::IncrementNumPacketsDropped()
{
  num_packets_dropped++;
}

void 
TopkQueryClient::PrintStats()
{
  char buf[1024];
  FILE *stats_fd;

  sprintf(buf, "%s/TopkQueryClientStats.csv", DataFilePath.c_str() );
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


                   // 1   2   3   4   5   6   7      8     9   10  11  12  13  14    15
  fprintf(stats_fd, "%i, %i, %i, %i, %i, %i, %.1f, %.2f, %.1f, %i, %i, %i, %i, %i, %.2f\n",
    GetNode()->GetId(), // 1 i 
    num_nodes, // 2 i
    num_queries_satisfied, // 3 i
    num_queries_issued, // 4 i
    num_packets_rcvd, // 5 i
    image_size_bytes, // 6 i
    run_time.GetSeconds(), // 7 f
    timeliness.GetSeconds(), // 8 f
    sum_similarity, // 9 f
    run_seed, // 10 i
    num_runs, // 11 i
    num_queries_unanswered, // 12 i
    num_packets_rcvd_late,  // 13 i
    num_packets_dropped, // 14 i
    (double)num_queries_satisfied/(double)num_queries_issued // 15 f
  );

  fclose(stats_fd);
}


} // Namespace ns3
