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
  ;
  return tid;
}

TopkQueryServer::TopkQueryServer ():
  delay_padding(0.1)
{
  NS_LOG_FUNCTION (this);
  m_sendEvent = EventId();
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
TopkQueryServer::ReceiveQuery( uint16_t node_from, uint16_t query_id, uint16_t num_images_rqstd )
{
  //std::cout<< GetNode()->GetId() << ", " << node_from << "\n";
  if( TOPK_QUERY_SERVER_DEBUG )
  {
    std::cout<< "Node " << GetNode()->GetId() << "'s server in ReceiveQuery at time " << Simulator::Now().GetSeconds() << ": \n" <<
            "\tquery from node " << node_from << "\n" << 
            "\tquery_id = " << query_id << "\n" << 
            "\tnum_images_rqstd = " << num_images_rqstd << "\n";
  }


  if( TOPK_QUERY_SERVER_DEBUG )
  {
    std::cout<<"Server (Node " << GetNode()->GetId() << ") at time " << Simulator::Now().GetSeconds() << " sending " << num_images_rqstd <<" packets from server to " << node_from <<". (delay_padding = " << delay_padding << ")\n";
  }

  //Time delay = NanoSeconds(10); 
  Time delay = Seconds(0);
  Simulator::Schedule( delay, &TopkQueryServer::ScheduleTrx, this, node_from, num_images_rqstd, query_id ); 
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
TopkQueryServer::ScheduleTrx ( uint16_t from, int num_images_rqstd, int query_id)
{
  if( TOPK_QUERY_SERVER_DEBUG )
  {
    std::cout<<"\tNode " << GetNode()->GetId() << " in ScheduleTrx...query from: " << from <<"\n";
  }
  
  double bit_rate = channel_rate*1000000.0;
  double num_bits = (packet_size_bytes+28)*8.0; // adding 28 for packet headers

  // checking to make sure it's not more than even the first hop can handle
  if( 3 * num_images_rqstd * (num_bits/bit_rate) > timeliness )
  {
    std::cout<<"ERROR: Not enough time for node to send packets (CF*K*Image_Size/channel_rate > timeliness) - in TopkQueryServer.cc\n";
    exit(-1);
  }
  
  for( int i = 0; i < num_images_rqstd; i++ )
  {
    Time delay = Seconds( 3 * i * ( (num_bits/bit_rate) + 0.00) );
    Simulator::Schedule( delay, &TopkQueryServer::SendPacket, this, from, i+1, num_images_rqstd, query_id ); 
  }

/*
  Time delay = Seconds( 3*num_images_rqstd*(num_bits/bit_rate) ); // 3 for Contention Factor (line net...grid should be 5)
  Ptr<TopkQueryClient> clientPtr = NodeList::GetNode(from)->GetApplication(1)->GetObject<TopkQueryClient>();
  clientPtr->ScheduleTransmit( delay );
*/
}
  
void
TopkQueryServer::SendPacket ( uint16_t from, int packetNum, int num_images_rqstd, int query_id)
{
  if( TOPK_QUERY_SERVER_DEBUG )
  {
    std::cout<<"\tNode " << GetNode()->GetId() << " in SendPacket...packetNum = " << packetNum <<", client = " << from << 
                " at time " << Simulator::Now().GetSeconds() << "\n";
  }

  Ptr<Packet> p = Create<Packet>(packet_size_bytes);

  TopkQueryTag tag( query_id, num_images_rqstd, packetNum, GetNode()->GetId() ); 
  p->AddPacketTag(tag);
  QueryDeadlineTag dl_tag( Simulator::Now().GetSeconds()+timeliness.GetSeconds() );
  p->AddPacketTag(dl_tag);
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
      std::cout<<"At time " << Simulator::Now ().GetSeconds () << "s server (Node " << GetNode()->GetId() << ") sent packet/image " << packetNum << " ("<< bytes_sent << " bytes) to " << from << "\n";
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
