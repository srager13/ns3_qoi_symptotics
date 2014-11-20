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
#include "qoi-query-flood-client.h"
#include "qoi-query-flood-server.h"
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("QoiQueryFloodClientApplication");
NS_OBJECT_ENSURE_REGISTERED (QoiQueryFloodClient);

TypeId
QoiQueryFloodClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::QoiQueryFloodClient")
    .SetParent<Application> ()
    .AddConstructor<QoiQueryFloodClient> ()
    .AddAttribute ("Interval", 
                   "The time to wait between packets",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&QoiQueryFloodClient::m_interval),
                   MakeTimeChecker ())
    .AddAttribute ("SumSimilarity", 
                   "QoI Sum Similarity requirement of query.",
                   DoubleValue (100),
                   MakeDoubleAccessor (&QoiQueryFloodClient::sum_similarity),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("NumPacketsPerImage", 
                   "Number of packets for each image.",
                   IntegerValue (1),
                   MakeIntegerAccessor (&QoiQueryFloodClient::num_packets_per_image),
                   MakeIntegerChecker<int> ())
    .AddAttribute ("RemoteAddress", 
                   "The destination Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&QoiQueryFloodClient::m_peerAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort", 
                   "The destination port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&QoiQueryFloodClient::m_peerPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PacketSize", "Size of echo data in outbound packets",
                   UintegerValue (100),
                   MakeUintegerAccessor (&QoiQueryFloodClient::SetDataSize,
                                         &QoiQueryFloodClient::GetDataSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&QoiQueryFloodClient::m_txTrace))
    .AddAttribute ("Timeliness", 
                   "Timeliness constraint",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&QoiQueryFloodClient::timeliness),
                   MakeTimeChecker ())
    .AddAttribute("SumSimFilename",
                   "File to look up sum sim vs. number of packets needed (in $NS3_DIR).",
                   StringValue("SumSimRequirements.csv"),
                   MakeStringAccessor (&QoiQueryFloodClient::SumSimFilename),
                   MakeStringChecker())
    .AddAttribute ("NumNodes", 
                   "Number of nodes in the scenario.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&QoiQueryFloodClient::SetNumNodes),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("ImageSizeBytes", 
                   "The size of each image (all assumed to be the same) in bytes.",
                   IntegerValue (2000000),
                   MakeIntegerAccessor (&QoiQueryFloodClient::image_size_bytes),
                   MakeIntegerChecker<int> ())
    .AddAttribute ("DelayPadding", 
                   "Delay time in seconds that client waits in between sending each packet to prevent overloading socket.",
                   DoubleValue (0.1),
                   MakeDoubleAccessor (&QoiQueryFloodClient::delay_padding),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("RunTime", "Total time of simulation.",
                   TimeValue (Seconds (50)),
                   MakeTimeAccessor (&QoiQueryFloodClient::run_time),
                   MakeTimeChecker ())
    .AddAttribute ("RunSeed", "Trial number.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&QoiQueryFloodClient::run_seed),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("NumRuns", "Total number of trials.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&QoiQueryFloodClient::num_runs),
                   MakeUintegerChecker<uint16_t> ())
  ;
  return tid;
}

// constructor
QoiQueryFloodClient::QoiQueryFloodClient () :
  m_interval(Time("5s")),
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
  num_packets_rcvd = 0;
  query_ids = 1;

  rand_dest = CreateObject<UniformRandomVariable>();

  rand_ss_dist = CreateObject<NormalRandomVariable>();
  rand_ss_dist->SetAttribute("Mean", DoubleValue(0));
  rand_ss_dist->SetAttribute("Variance", DoubleValue(3));

  Simulator::Schedule( NanoSeconds(1), &QoiQueryFloodClient::Init, this );
}

void
QoiQueryFloodClient::Init()
{
  rand_interval = CreateObject<ExponentialRandomVariable>();
  //rand_interval = CreateObject<NormalRandomVariable>();
  if( QOI_QUERY_FLOOD_CLIENT_DEBUG )
  {
    std::cout<<"Setting rand_interval to mean of " << timeliness.GetSeconds() << "\n";
  }
  //rand_interval->SetAttribute("Mean", DoubleValue(timeliness.GetSeconds()+1.0));
  //rand_interval->SetAttribute("Variance", DoubleValue(3.0));
  //rand_interval->SetAttribute("Bound", DoubleValue(timeliness.GetSeconds()+10.0));
  rand_interval->SetAttribute("Mean", DoubleValue(5.0));
  //rand_interval->SetAttribute("Bound", DoubleValue(m_interval.GetSeconds()+10.0));

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
    avg_num_images_rqrd = 10;
    std::cout << "Error opening sum similarity requirements file: " << buf << "...setting average number of images to default ("<<avg_num_images_rqrd << ")\n";
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
    
      if( QOI_QUERY_FLOOD_CLIENT_DEBUG )
      {
        std::cout<<"From file: sum_sim = " << sum_sim << ", num_images = " << num_images <<" (input sum similarity = " << sum_similarity << ")\n";
      }

      if( sum_sim >= sum_similarity )
      {
        avg_num_images_rqrd = num_images;
        if( QOI_QUERY_FLOOD_CLIENT_DEBUG )
        {
          std::cout<<"Found requirement. Setting avg_num_images_rqrd to " << avg_num_images_rqrd << "\n";
        }
        found_requ = true;
        break;
      }
    }
    if( !found_requ )
    {
      avg_num_images_rqrd = 10;
      std::cout << "Error: didn't find valid requirement in file...setting average number of images to default ("<<avg_num_images_rqrd << ")\n";
    }
  }

}

QoiQueryFloodClient::~QoiQueryFloodClient()
{
  NS_LOG_FUNCTION (this);

  delete [] m_data;
  m_data = 0;
  m_dataSize = 0;
}

void 
QoiQueryFloodClient::SetRemote (Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = ip;
  m_peerPort = port;
}

void 
QoiQueryFloodClient::SetRemote (Ipv4Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = Address (ip);
  m_peerPort = port;
}

void 
QoiQueryFloodClient::SetRemote (Ipv6Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = Address (ip);
  m_peerPort = port;
}

void
QoiQueryFloodClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void 
QoiQueryFloodClient::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<Ipv4> ipv4 = GetNode()->GetObject<Ipv4>();
  Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1,0);
  Ipv4Address iad = iaddr.GetLocal();
  if( QOI_QUERY_FLOOD_CLIENT_DEBUG )
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
      std::cout<<"ERROR:  Need to account for addresses higher than 1536 nodes (in qoi-query-flood-client.cc)\n";
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

    m_socket[i]->SetRecvCallback (MakeCallback (&QoiQueryFloodClient::HandleRead, this));
  }

  Time next = timeliness + Seconds(rand_interval->GetValue());
  if( next.GetSeconds() < 0.0 )
    next = timeliness;
  ScheduleTransmit (next);
}

void 
QoiQueryFloodClient::StopApplication ()
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
QoiQueryFloodClient::SetDataSize (uint32_t dataSize)
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
QoiQueryFloodClient::GetDataSize (void) const
{
  NS_LOG_FUNCTION (this);
  return m_size;
}

void 
QoiQueryFloodClient::SetFill (std::string fill)
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
QoiQueryFloodClient::SetFill (uint8_t fill, uint32_t dataSize)
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
QoiQueryFloodClient::SetFill (uint8_t *fill, uint32_t fillSize, uint32_t dataSize)
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
QoiQueryFloodClient::ScheduleTransmit (Time dt)
{
  //std::cout<<"Scheduling Send with delay = " << dt.GetSeconds() << "\n";
  NS_LOG_FUNCTION (this << dt);
  m_sendEvent = Simulator::Schedule (dt, &QoiQueryFloodClient::Send, this);
}

void 
QoiQueryFloodClient::Send (void)
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT (m_sendEvent.IsExpired ());
  // num images set from sum similarity requirement input.  
  // Here we vary slightly around that number just to provide a little bit of randomness
  int num_images_needed = avg_num_images_rqrd; 
  int rand_change =  (int)rand_ss_dist->GetValue();
  num_images_needed += rand_change;
  if( QOI_QUERY_FLOOD_CLIENT_DEBUG )
  {
    std::cout<<"rand_change = " << rand_change << ", resulting required image num = " << num_images_needed << "\n";
  }
 
  // only count the issued queries that have a chance to finish before the end of the simulation
  if( Simulator::Now() + timeliness < run_time )
  {
    num_queries_issued++;
    if( QOI_QUERY_FLOOD_CLIENT_DEBUG )
    {
      std::cout<<"sending query with id = " << query_ids << "; number sent = " << num_queries_issued << "\n";
    }
  }
  else
  {
    return;
  }

  // for every possible destination (not self), send num_images_needed packets
  for( uint16_t dest = 0; dest < num_nodes; dest++ ) 
  {
    if( dest == GetNode()->GetId() )
      continue;

    if( m_socket[dest] == 0 )
    {
      std::cout << "ERROR: socket[" << dest << "] == 0 in node " << GetNode()->GetId() << "\n";
    }
 
    for( int i = 0; i < num_images_needed; i++ )
    { 
      //Time delay = Seconds(i*(((image_size_bytes*8.0)/11000000.0)+delay_padding));
      double bit_rate = 2000000.0/8.0;
      double num_bits = image_size_bytes*8.0;
      //Time delay = Seconds(dest*i*(((image_size_bytes*8.0)/11000000.0)+delay_padding));
      Time delay = Seconds( i * ( (num_bits/bit_rate) ) );
      Simulator::Schedule( delay, &QoiQueryFloodClient::SendPacket, this, dest, num_images_needed, query_ids );
    }
  }
  
  UpdateQueryIds();

  // schedule sending next round of images
  Time next = timeliness + Seconds(rand_interval->GetValue());
  if( next.GetSeconds() < 0.0 )
    next = timeliness;
  if( Simulator::Now() + next + timeliness + Seconds(10.0) < run_time ) // adding 10 seconds for buffer
  {
    if( QOI_QUERY_FLOOD_CLIENT_DEBUG )
    {
      std::cout<<"delay until schedule next send = " << next.GetSeconds() << "\n";
    }
    ScheduleTransmit (next);
  }
  else
  {
    if( QOI_QUERY_FLOOD_CLIENT_DEBUG )
    {
      std::cout<<"not enough time for another send\n";
    }
  }
}

void 
QoiQueryFloodClient::SendPacket(uint16_t dest, int numImagesNeeded, int query_id)
{
  Ptr<Packet> p = Create<Packet>(image_size_bytes);

  QoiQueryTag tag( GetNode()->GetId(), query_id, numImagesNeeded, 0 );
  p->AddPacketTag(tag); 
  // call to the trace sinks before the packet is actually sent,
  // so that tags added to the packet can be sent as well
  m_txTrace (p);

  if( m_socket[dest]->Send (p) < 0 )
  {
    std::cout << "ERROR sending packet on socket from node " << GetNode()->GetId() << " to node " << dest << " at time " << Simulator::Now().GetSeconds() << "\n";
  }

  ++m_sent;

  uint8_t firstByte = dest&(uint8_t)255;
  uint8_t secondByte = (dest>>8)&(uint8_t)255;
    
  char buf[32];
  sprintf(buf, "10.1.%i.%i", secondByte+1, firstByte); 

  Ipv4Address ipv4 = Ipv4Address(buf);
  if (Ipv4Address::IsMatchingType (Ipv4Address(ipv4)))
  {
    if( QOI_QUERY_FLOOD_CLIENT_DEBUG )
    {
      std::cout << "At time " << Simulator::Now ().GetSeconds () << "s client in node " << GetNode()->GetId() << 
                   " sent " << image_size_bytes << " bytes of query " << query_id << " to " <<
                   "node " << dest << "\n";
   //                Ipv4Address::ConvertFrom (ipv4) << " port " << m_peerPort << "\n";
    }
  }

}

void
QoiQueryFloodClient::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      if( QOI_QUERY_FLOOD_CLIENT_DEBUG )
      {
		  	std::cout<<"Client in node " << GetNode()->GetId() << " received packet from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << "\n";
      }
      num_packets_rcvd++;

      if (InetSocketAddress::IsMatchingType (from))
        {
          if( QOI_QUERY_FLOOD_CLIENT_DEBUG )
          {
            std::cout<< "At time " << Simulator::Now ().GetSeconds () << "s client received " << packet->GetSize () << " bytes from " <<
                       InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                       InetSocketAddress::ConvertFrom (from).GetPort () << "\n";
          }
        }
    }
}

} // Namespace ns3
