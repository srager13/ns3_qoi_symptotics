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
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "topk-query-client.h"
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
    .AddAttribute ("PacketSize", "Size of echo data in outbound packets",
                   UintegerValue (100),
                   MakeUintegerAccessor (&TopkQueryClient::SetDataSize,
                                         &TopkQueryClient::GetDataSize),
                   MakeUintegerChecker<uint32_t> ())
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
  m_dataSize = 0;
  num_queries_issued = 0;
  num_queries_satisfied = 0;
  num_queries_unanswered = 0;
  num_packets_rcvd_late = 0;
  num_packets_rcvd = 0;
  query_ids = 1;

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
  
  for( int i = 0; i < num_nodes; i++ )
  {
    Ptr<Socket> sock = 0;
    m_socket.push_back(sock);
  }
  
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
    
      if( TOPK_QUERY_CLIENT_DEBUG )
      {
        std::cout<<"From file: sum_sim = " << sum_sim << ", num_images = " << num_images <<" (input sum similarity = " << sum_similarity << ")\n";
      }

      if( sum_sim >= sum_similarity )
      {
        avg_num_packets_rqrd = num_images;
        if( TOPK_QUERY_CLIENT_DEBUG )
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
  m_dataSize = 0;
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

  Ptr<Ipv4> ipv4 = GetNode()->GetObject<Ipv4>();
  Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1,0);
  Ipv4Address iad = iaddr.GetLocal();
  if( TOPK_QUERY_CLIENT_DEBUG )
  {
    std::cout << "Node " << GetNode()->GetId() <<"'s IP address = " << iad << "\n";
  }

  for( uint16_t i = 0; i < num_nodes; i++ )
  {
    if ( GetNode()->GetId() == i )
    {
      continue;
    }

    uint8_t firstByte = (i+1)&(uint8_t)255;

    uint16_t second_byte_start = 1;
    if( num_nodes <= 255 )
      second_byte_start = (uint16_t)1;
    else if( num_nodes <= 511 )
      second_byte_start = (uint16_t)2;
    else if( num_nodes <= 1023 )
      second_byte_start = (uint16_t)4;
    else if( num_nodes <= 2047 )
      second_byte_start = (uint16_t)8;
    else if( num_nodes <= 4095 )
      second_byte_start = (uint16_t)16;
    
    uint8_t secondByte = 1;
    if( i < 255 )
    {
      secondByte = second_byte_start;
    }
    else if( i < 511 )
    {
      secondByte = second_byte_start + (uint8_t)1;
    }
    else if( i < 767 )
    {
      secondByte = second_byte_start + (uint8_t)2;
    }
    else if( i < 1023 )
    {
      secondByte = second_byte_start + (uint8_t)3;
    }
    else if( i < 1279 )
    {
      secondByte = second_byte_start + (uint8_t)4;
    }
    else if( i < 1535 )
    {
      secondByte = second_byte_start + (uint8_t)5;
    }
    else
    {
      std::cout<<"ERROR:  Need to account for addresses higher than 1536 nodes (in topk-query-client.cc)\n";
      exit(-1);
    }

    char buf[32];
    sprintf(buf, "10.1.%i.%i", secondByte, firstByte); 

    //std::cout<<"Node " << GetNode()->GetId() <<" connecting socket to address: " << buf << "\n";

    Ipv4Address ipv4 = Ipv4Address(buf);

    if( m_socket[i] == 0 )
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket[i] = Socket::CreateSocket (GetNode (), tid);
      if (Ipv4Address::IsMatchingType(ipv4) == true)
        {
          m_socket[i]->Bind();
          m_socket[i]->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(ipv4), m_peerPort));
        }
      // will not work with IPV6 addresses!
      /*else if (Ipv6Address::IsMatchingType(Ipv6Address(ipv6)) == true)
        {
          m_socket[i]->Bind6();
          m_socket[i]->Connect (Inet6SocketAddress (Ipv6Address::ConvertFrom(Ipv6Address(ipv6)), m_peerPort));
        }
      */
    }

    m_socket[i]->SetRecvCallback (MakeCallback (&TopkQueryClient::HandleRead, this));
  }

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

  for( int i = 0; i < num_nodes; i++ )
  {
    if (m_socket[i] != 0) 
      {
        m_socket[i]->Close ();
        m_socket[i]->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
        m_socket[i] = 0;
      }
  }

  Simulator::Cancel (m_sendEvent);
}

void 
TopkQueryClient::SetDataSize (uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << dataSize);

  //
  // If the client is setting the echo packet data size this way, we infer
  // that she doesn't care about the contents of the packet at all, so 
  // neither will we.
  //
  delete [] m_data;
  m_data = 0;
  m_dataSize = 0;
  m_size = dataSize;
}

uint32_t 
TopkQueryClient::GetDataSize (void) const
{
  NS_LOG_FUNCTION (this);
  return m_size;
}

void 
TopkQueryClient::SetFill (std::string fill)
{
  NS_LOG_FUNCTION (this << fill);

  uint32_t dataSize = fill.size () + 1;

  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  memcpy (m_data, fill.c_str (), dataSize);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
TopkQueryClient::SetFill (uint8_t fill, uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << fill << dataSize);
  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  memset (m_data, fill, dataSize);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
TopkQueryClient::SetFill (uint8_t *fill, uint32_t fillSize, uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << fill << fillSize << dataSize);
  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  if (fillSize >= dataSize)
    {
      memcpy (m_data, fill, dataSize);
      m_size = dataSize;
      return;
    }

  //
  // Do all but the final fill.
  //
  uint32_t filled = 0;
  while (filled + fillSize < dataSize)
    {
      memcpy (&m_data[filled], fill, fillSize);
      filled += fillSize;
    }

  //
  // Last fill may be partial
  //
  memcpy (&m_data[filled], fill, dataSize - filled);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
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

  NS_LOG_FUNCTION (this);

  NS_ASSERT (m_sendEvent.IsExpired ());

  Ptr<Packet> p;
  if (m_dataSize)
    {
      //
      // If m_dataSize is non-zero, we have a data buffer of the same size that we
      // are expected to copy and send.  This state of affairs is created if one of
      // the Fill functions is called.  In this case, m_size must have been set
      // to agree with m_dataSize
      //
      NS_ASSERT_MSG (m_dataSize == m_size, "TopkQueryClient::Send(): m_size and m_dataSize inconsistent");
      NS_ASSERT_MSG (m_data, "TopkQueryClient::Send(): m_dataSize but no m_data");
      p = Create<Packet> (m_data, m_dataSize);
    }
  else
    {
      //
      // If m_dataSize is zero, the client has indicated that it doesn't care
      // about the data itself either by specifying the data size by setting
      // the corresponding attribute or by not calling a SetFill function.  In
      // this case, we don't worry about it either.  But we do allow m_size
      // to have a value different from the (zero) m_dataSize.
      //
      p = Create<Packet> (m_size);
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
 
  TopkQuery new_query;
  new_query.id =  query_ids;
  new_query.num_images_rqstd = num_images_needed;
  new_query.num_images_rcvd = 0;
  new_query.start_time = Simulator::Now();
  new_query.deadline = Simulator::Now() + timeliness;
  // only count the issued queries that have a chance to finish before the end of the simulation
  if( Simulator::Now() + timeliness < run_time )
  {
    num_queries_issued++;
    // Schedule function to check on this query at the deadline to see if it was satisfied
    Simulator::Schedule( timeliness, &TopkQueryClient::CheckQuery, this, new_query.id );
    if( TOPK_QUERY_CLIENT_DEBUG )
    {
      std::cout<<"sending query with id = " << new_query.id << "; number sent = " << num_queries_issued << "\n";
    }
  }
  else
  {
    return;
  }

  // add to list of active queries 
  active_queries.push_back( new_query ); 
  
  UpdateQueryIds();
 
  TopkQueryTag tag( new_query.id, num_images_needed, 0 );
  p->AddPacketTag(tag); 
  // call to the trace sinks before the packet is actually sent,
  // so that tags added to the packet can be sent as well
  m_txTrace (p);

  // choose random destination (not self)
  uint16_t dest;
  do
  {
    dest = rand_dest->GetInteger(0, num_nodes-1);
  }while( dest == GetNode()->GetId() );

  if( m_socket[dest] == 0 )
  {
		std::cout << "ERROR: socket[" << dest << "] == 0 in node " << GetNode()->GetId() << "\n";
  }

  if( m_socket[dest]->Send (p) < 0 )
	{
		std::cout << "ERROR sending packet on socket from node " << GetNode()->GetId() << " to node " << dest << " at time " << Simulator::Now().GetSeconds() << "\n";
	}

  ++m_sent;

  uint8_t firstByte = dest&(uint8_t)255;
  uint8_t secondByte = (dest>>8)&(uint8_t)255;
  //uint8_t ipv6[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,secondByte,firstByte};
    
  char buf[32];
  sprintf(buf, "10.1.%i.%i", secondByte+1, firstByte); 

  Ipv4Address ipv4 = Ipv4Address(buf);
  if (Ipv4Address::IsMatchingType (Ipv4Address(ipv4)))
  {
    if( TOPK_QUERY_CLIENT_DEBUG )
    {
      std::cout << "At time " << Simulator::Now ().GetSeconds () << "s client in node " << GetNode()->GetId() << 
                   " sent " << m_size << " bytes to " <<
                   Ipv4Address::ConvertFrom (ipv4) << " port " << m_peerPort << "\n";
    }
  }
  /*else if (Ipv6Address::IsMatchingType (Ipv6Address(ipv6)))
  {
    std::cout << "At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                  Ipv6Address::ConvertFrom (Ipv6Address(ipv6)) << " port " << m_peerPort << "\n";
  }*/

  Time next = timeliness + Seconds(rand_interval->GetValue());
  //Time next = timeliness + Seconds(5.0);
  if( next.GetSeconds() < 0.0 )
    next = timeliness;
  if( Simulator::Now() + next + timeliness + Seconds(10.0) < run_time ) // adding 10 seconds for buffer
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
      TopkQueryTag tag( -1, -1, 0 );
      if( packet->RemovePacketTag(tag) )
      {
        // find active query in the list and update images_rcvd
        for( uint16_t i = 0; i < active_queries.size(); i++ )
        {
          if( active_queries[i].id == tag.query_id )
          {
            found_query = true;
            active_queries[i].num_images_rcvd++;
            if( TOPK_QUERY_CLIENT_DEBUG )
            {
		  	      std::cout<<"Node " << GetNode()->GetId() << " received return packet number " << tag.image_num << "\n\tso far, got " << active_queries[i].num_images_rcvd << "/" <<
                         active_queries[i].num_images_rqstd << " of query " << active_queries[i].id << "\n";
            }
          }
        } 
        if( !found_query )
        {
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
TopkQueryClient::CheckQuery( uint16_t id )
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
    if( active_queries[i].id == id )
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


                   // 1   2   3   4   5   6   7      8     9   10  11  12  13   14
  fprintf(stats_fd, "%i, %i, %i, %i, %i, %i, %.1f, %.2f, %.1f, %i, %i, %i, %i, %.2f\n",
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
    (double)num_queries_satisfied/(double)num_queries_issued // 14 f
  );

  fclose(stats_fd);
}


} // Namespace ns3
