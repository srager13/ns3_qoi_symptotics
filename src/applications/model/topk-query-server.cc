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

#include "ns3/double.h"
#include "ns3/integer.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/address-utils.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/arp-l3-protocol.h"
#include "ns3/arp-cache.h"
#include "ns3/arp-header.h"
#include "ns3/node-list.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ipv4-interface.h"
#include "ns3/object-vector.h"
#include "ns3/pointer.h"

#include "topk-query-server.h"
#include "topk-query-client.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TopkQueryServerApplication");
NS_OBJECT_ENSURE_REGISTERED (TopkQueryServer);

TypeId
TopkQueryServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TopkQueryServer")
    .SetParent<Application> ()
    .AddConstructor<TopkQueryServer> ()
    .AddAttribute ("Port", "Port on which we listen for incoming packets.",
                   UintegerValue (9),
                   MakeUintegerAccessor (&TopkQueryServer::m_port),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("ImageSizeKBytes", "Size of image (all assumed to be the same) in kilobytes.",
                   UintegerValue (2000),
                   MakeUintegerAccessor (&TopkQueryServer::image_size_kbytes),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PacketSizeBytes", "Size of packet (all assumed to be the same) in bytes.",
                   UintegerValue (1450000),
                   MakeUintegerAccessor (&TopkQueryServer::packet_size_bytes),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("NumNodes", ".",
                   UintegerValue (9),
                   MakeUintegerAccessor (&TopkQueryServer::num_nodes),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("ContentionFactor", ".",
                   UintegerValue (1),
                   MakeUintegerAccessor (&TopkQueryServer::contention_factor),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("DelayPadding", "Delay time in seconds that server waits after sending each image to prevent overloading socket.",
                   DoubleValue(0.1),
                   MakeDoubleAccessor(&TopkQueryServer::delay_padding),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("ChannelRate", "Rate of each channel (in Mbps).",
                   DoubleValue(2.0),
                   MakeDoubleAccessor(&TopkQueryServer::channel_rate),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Timeliness", "Timeliness constraint for all queries.",
                    TimeValue(Seconds(1.0)),
                    MakeTimeAccessor (&TopkQueryServer::timeliness),
                    MakeTimeChecker())
    .AddAttribute ("RunTime", "Total time of simulation.",
                   TimeValue (Seconds (50)),
                   MakeTimeAccessor (&TopkQueryServer::run_time),
                   MakeTimeChecker ())
    .AddAttribute ("OneFlow", "Only send one flow from last node to first node for testing purposes.",
                    BooleanValue(false),
                    MakeBooleanAccessor(&TopkQueryServer::one_flow),
                    MakeBooleanChecker())
    .AddAttribute ("ServerDebug", "",
                    BooleanValue(false),
                    MakeBooleanAccessor(&TopkQueryServer::TOPK_QUERY_SERVER_DEBUG),
                    MakeBooleanChecker())
    .AddAttribute ("SourceNode", "Source of flow in one flow mode.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&TopkQueryServer::source_node),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("DestNode", "Destination of flow in one flow mode.",
                   UintegerValue (3),
                   MakeUintegerAccessor (&TopkQueryServer::dest_node),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute("SumSimFilename",
                   "File to look up sum sim vs. number of packets needed (in $NS3_DIR).",
                   StringValue("SumSimRequirements.csv"),
                   MakeStringAccessor (&TopkQueryServer::SumSimFilename),
                   MakeStringChecker())
    .AddAttribute ("SumSimilarity", 
                   "QoI Sum Similarity requirement of query.",
                   DoubleValue (100),
                   MakeDoubleAccessor (&TopkQueryServer::sum_similarity),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("NumPacketsPerImage", 
                   "Number of packets for each image.",
                   IntegerValue (0),
                   MakeIntegerAccessor (&TopkQueryServer::num_packets_per_image),
                   MakeIntegerChecker<int> ())
  ;
  return tid;
}

// constructor
TopkQueryServer::TopkQueryServer ():
  delay_padding(0.1)
{
  NS_LOG_FUNCTION (this);
  m_sendEvent = EventId();
  query_ids = 1;
  
  rand_dest = CreateObject<UniformRandomVariable>();
  
  time_jitter = CreateObject<NormalRandomVariable>();
  time_jitter->SetAttribute( "Mean", DoubleValue(0.0) );
  time_jitter->SetAttribute( "Variance", DoubleValue(0.0) );
 
  Simulator::Schedule( NanoSeconds(2), &TopkQueryServer::Init, this );
}

void
TopkQueryServer::Init()
{ 
  char buf[1024];
  std::ifstream sumsim_fd;
  sprintf(buf, "%s.csv", SumSimFilename.c_str());
  sumsim_fd.open( buf, std::ifstream::in );
  if( !sumsim_fd.is_open() )
  {
    avg_num_packets_rqrd = 10;
    std::cout << "Error opening sum similarity requirements file: " << buf << "...setting average number of packets to default ("<<avg_num_packets_rqrd << ")\n";
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
   
      if( TOPK_QUERY_SERVER_DEBUG )
      {
        std::cout<<"From file: sum_sim = " << sum_sim << ", num_packets = " << num_packets <<" (input sum similarity = " << sum_similarity << ")\n";
      }

      if( sum_sim >= sum_similarity )
      {
        avg_num_packets_rqrd = num_packets;
        if( TOPK_QUERY_SERVER_DEBUG )
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
      std::cout << "Error: didn't find valid requirement in file...setting average number of packets to default ("<<avg_num_packets_rqrd << ")\n";
    }
  }
}

TopkQueryServer::~TopkQueryServer()
{
  NS_LOG_FUNCTION (this);
}

void
TopkQueryServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void 
TopkQueryServer::StartApplication (void)
{
  NS_LOG_FUNCTION (this);
	if( TOPK_QUERY_SERVER_DEBUG )
	{
		std::cout<< "Node " << GetNode()->GetId() << " in TopkQueryServer::StartApplication at " << Simulator::Now().GetSeconds() << "\n";
	}
  for( int i = 0; i < num_nodes; i++ )
  {
    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    Ptr<Socket> sock = Socket::CreateSocket (GetNode (), tid);
    m_socket.push_back(sock);
    //InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
    //m_socket[i]->Bind (local);
    Ipv4Address ipv4 = TopkQueryClient::GetNodeAddress(i,num_nodes);
  //Ptr<Ipv4> ipv4 = NodeList::GetNode(i)->GetObject<Ipv4>();
  //Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1,0);
  //Ipv4Address ipv4 = iaddr.GetLocal();

    //std::cout<<"address returned from GetNodeAddress("<<i<<","<<num_nodes<<") = " << ipv4 << "\n";

    if( Ipv4Address::IsMatchingType(ipv4) == true )
    {
      //std::cout<<"\taddress is matching type to Ipv4, connecting to port " << m_port << "\n";
      m_socket[i]->Bind ();
      m_socket[i]->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(ipv4), m_port));
    }
  
    m_socket[i]->SetRecvCallback (MakeCallback (&TopkQueryServer::HandleRead, this));
  }

  PopulateArpCache();

  //Time next = timeliness;
  Time next = Seconds(5) + Seconds(time_jitter->GetValue());
  if( next.GetSeconds() < 0.0 )
  {
    next = timeliness;
  }
  Simulator::Schedule (next, &TopkQueryServer::StartNewSession, this);
}

void 
TopkQueryServer::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  for( int i = 0; i < num_nodes; i++ )
  {
  if (m_socket[i] != 0) 
    {
      m_socket[i]->Close ();
      m_socket[i]->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
  }
  Simulator::Cancel(m_sendEvent);
}

void
TopkQueryServer::StartNewSession()
{
  if( one_flow && GetNode()->GetId() != source_node )
  {
    return;
  } 
  
  // choose random destination (not self)
  uint16_t dest;
  do
  {
    dest = rand_dest->GetInteger(0, num_nodes-1);
  }while( dest == GetNode()->GetId() );

  if( one_flow )
  {
    //std::cout<< "One flow\n";
    dest = dest_node;
  }
  //std::cout<<"source node = " << GetNode()->GetId() << ", dest node = " << dest << "\n";
  
  // num packets set from sum similarity requirement input.  
  // Here we vary slightly around that number just to provide a little bit of randomness
  int num_packets_needed = avg_num_packets_rqrd; 

  // TODO::Add randomness?
  //int rand_change =  (int)rand_ss_dist->GetValue();
  int rand_change = 0;
  num_packets_needed += rand_change;
  if( TOPK_QUERY_SERVER_DEBUG )
  {
    std::cout<<"rand_change = " << rand_change << ", resulting required packet num = " << num_packets_needed << "\n";
  }
 
  uint16_t query_id = query_ids;
  // call function to create new active query in client
  Ptr<TopkQueryClient> clientPtr = NodeList::GetNode(dest)->GetApplication(1)->GetObject<TopkQueryClient>();
  clientPtr->CreateNewActiveQuery( GetNode()->GetId(), query_id, num_packets_needed );

  //std::cout<< GetNode()->GetId() << ", " << node_from << "\n";
  if( TOPK_QUERY_SERVER_DEBUG )
  {
    std::cout<< "Node " << GetNode()->GetId() << "'s server in StartNewSession at time " << Simulator::Now().GetSeconds() << ": \n" <<
            "\tdest = node " << dest << "\n" << 
            "\tquery_id = " << query_id << "\n" << 
            "\tnum_packets_needed = " << num_packets_needed << "\n";
  }


  if( TOPK_QUERY_SERVER_DEBUG )
  {
    std::cout<<"Server (Node " << GetNode()->GetId() << ") at time " << Simulator::Now().GetSeconds() << " sending " << num_packets_needed <<" packets from server to " << dest <<". (delay_padding = " << delay_padding << ")\n";
  }

  //Time delay = NanoSeconds(10); 
  Time delay = Seconds(0);
  Simulator::Schedule( delay, &TopkQueryServer::ScheduleTrx, this, dest, num_packets_needed, query_id ); 

  UpdateQueryIds();

  if( Simulator::Now() + timeliness < run_time - Seconds(110) ) // adding 10 seconds for buffer
  {
    // Added randomness to try to get more uniformity between flows
    Time dt = timeliness + Seconds(time_jitter->GetValue());
    dt = timeliness;
  
    //std::cout<<"Scheduling StartNewSession with delay = " << dt.GetSeconds() << "\n";
    Simulator::Schedule (dt, &TopkQueryServer::StartNewSession, this);
  }
}

void 
TopkQueryServer::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
                       InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                       InetSocketAddress::ConvertFrom (from).GetPort ());
        }

    }
}

void
TopkQueryServer::ScheduleTrx ( uint16_t from, int num_packets_rqstd, int query_id)
{
  if( TOPK_QUERY_SERVER_DEBUG )
  {
    std::cout<<"\tNode " << GetNode()->GetId() << " in ScheduleTrx...query from: " << from <<"\n";
  }
  
  double bit_rate = channel_rate*1000000.0;
  double num_bits = (packet_size_bytes)*8.0; // adding 28 for packet headers
  //double num_bits = (packet_size_bytes+28)*8.0; // adding 28 for packet headers

  // checking to make sure it's not more than even the first hop can handle
  if( 3 * num_packets_rqstd * (num_bits/bit_rate) > timeliness )
  {
    std::cout<<"ERROR: Not enough time for node to send packets (CF*K*Image_Size/channel_rate > timeliness) - in TopkQueryServer.cc\n";
    exit(-1);
  }
  
  for( int i = 0; i < num_packets_rqstd; i++ )
  {
    Time delay = Seconds( contention_factor * i * (num_bits/bit_rate) );
    //Time delay = Seconds( 3 * i * ( (num_bits/bit_rate) + 0.00) );
    //Time delay = Seconds( 5 * i * ( (num_bits/bit_rate) + 0.00) );
    Simulator::Schedule( delay, &TopkQueryServer::SendPacket, this, from, i+1, num_packets_rqstd, query_id ); 
  }

}
  
void
TopkQueryServer::SendPacket ( uint16_t from, int packetNum, int num_packets_rqstd, int query_id)
{
  if( TOPK_QUERY_SERVER_DEBUG )
  {
    std::cout<<"\tNode " << GetNode()->GetId() << " in SendPacket...packetNum = " << packetNum <<", client = " << from << 
                " at time " << Simulator::Now().GetSeconds() << "\n";
  }

  Ptr<Packet> p = Create<Packet>(packet_size_bytes);

  TopkQueryTag tag( query_id, num_packets_rqstd, packetNum, GetNode()->GetId() ); 
  p->AddPacketTag(tag);

  QueryDeadlineTag dl_tag( Simulator::Now().GetSeconds()+timeliness.GetSeconds() );
  p->AddPacketTag(dl_tag);

  MaxTFTag tf_tag( 0, 0 );
  p->AddPacketTag(tf_tag);

  int bytes_sent = m_socket[from]->Send (p);
  if( bytes_sent < 0 )
  {
    if( TOPK_QUERY_SERVER_DEBUG )
    {
      std::cout<< "ERROR: sending packet " << packetNum << "from server failed at time " << Simulator::Now().GetSeconds() << "\n";
    }
  }
  else
  {
    if( TOPK_QUERY_SERVER_DEBUG )
    {
      std::cout<<"At time " << Simulator::Now ().GetSeconds () << "s server (Node " << GetNode()->GetId() << ") sent packet " << packetNum << " ("<< bytes_sent << " bytes) to " << from << "\n";
    }
  }
}

void
TopkQueryServer::PopulateArpCache ()
{
  Ptr<ArpCache> arp = CreateObject<ArpCache> ();
  arp->SetAliveTimeout (Seconds(3600 * 24 * 365));
  for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i)
  {
    Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol> ();
    NS_ASSERT(ip !=0);
    ObjectVectorValue interfaces;
    ip->GetAttribute("InterfaceList", interfaces);
    for(ObjectVectorValue::Iterator j = interfaces.Begin(); j != interfaces.End (); j++)
    {
      Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();
      NS_ASSERT(ipIface != 0);
      Ptr<NetDevice> device = ipIface->GetDevice();
      NS_ASSERT(device != 0);
      Mac48Address addr = Mac48Address::ConvertFrom(device->GetAddress ());
      for(uint32_t k = 0; k < ipIface->GetNAddresses (); k ++)
      {
        Ipv4Address ipAddr = ipIface->GetAddress (k).GetLocal();
        if(ipAddr == Ipv4Address::GetLoopback())
          continue;
        ArpCache::Entry * entry = arp->Add(ipAddr);
        entry->MarkWaitReply(0);
        entry->MarkAlive(addr);
      }
    }
  }
  for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i)
  {
    Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol> ();
    NS_ASSERT(ip !=0);
    ObjectVectorValue interfaces;
    ip->GetAttribute("InterfaceList", interfaces);
    for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=
interfaces.End (); j ++)
    {
      Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();
      ipIface->SetAttribute("ArpCache", PointerValue(arp));
    }
  }
}

} // Namespace ns3
