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
    .AddAttribute ("ImageSizeBytes", "Size of image (all assumed to be the same) in bytes.",
                   UintegerValue (2000000),
                   MakeUintegerAccessor (&TopkQueryServer::image_size_bytes),
                   MakeUintegerChecker<uint64_t> ())
    .AddAttribute ("DelayPadding", "Delay time in seconds that server waits after sending each image to prevent overloading socket.",
                   DoubleValue(0.1),
                   MakeDoubleAccessor(&TopkQueryServer::delay_padding),
                   MakeDoubleChecker<double> ())
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
  m_socket = 0;
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
	if( TOPK_SERVER_SOCKET_DEBUG )
	{
		std::cout<< "Node " << GetNode()->GetId() << " in TopkQueryServer::StartApplication at " << Simulator::Now().GetSeconds() << "\n";
	}

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
      m_socket->Bind (local);
      if (addressUtils::IsMulticast (m_local))
        {
					//if( TOPK_SERVER_SOCKET_DEBUG )
					{
						std::cout<< "Socket is being set up for multicast\n";
					}
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, m_local);
            }
          else
            {
              NS_FATAL_ERROR ("Error: Failed to join multicast group");
            }
        }
    }

  m_socket->SetRecvCallback (MakeCallback (&TopkQueryServer::HandleRead, this));
}

void 
TopkQueryServer::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
  Simulator::Cancel(m_sendEvent);
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

      int num_images_rqstd = 0, query_id = -1;
      TopkQueryTag tag( -1, -1, 0 );
      if( packet->RemovePacketTag(tag) )
      {
        num_images_rqstd = tag.num_images_rqstd;
        query_id = tag.query_id;
      }

      if( TOPK_QUERY_SERVER_DEBUG )
      {
        std::cout<<"Server (Node " << GetNode()->GetId() << ") at time " << Simulator::Now().GetSeconds() << " sending " << num_images_rqstd <<" packets from server to " << from <<". (delay_padding = " << delay_padding << ")\n";
      }
       
      Time delay = NanoSeconds(10); 
      Simulator::Schedule( delay, &TopkQueryServer::ScheduleTrx, this, from, num_images_rqstd, query_id ); 
    }
}

void
TopkQueryServer::ScheduleTrx ( Address from, int num_images_rqstd, int query_id)
{
  for( int i = 0; i < num_images_rqstd; i++ )
  {
    //Time delay = Seconds(i*(((image_size_bytes*8.0)/2000000.0)+delay_padding));
    double bit_rate = 2000000.0/8.0;
    double num_bits = image_size_bytes*8.0;
    Time delay = Seconds( i * ( (num_bits/bit_rate) + 0.00) );
    //Time delay = Seconds(0.1*i);
    Simulator::Schedule( delay, &TopkQueryServer::SendPacket, this, from, i+1, num_images_rqstd, query_id ); 
    //Simulator::Schedule( delay, &TopkQueryServer::SendPacket, this, from, i, num_images_rqstd, query_id ); 
    //Simulator::Schedule( delay, &TopkQueryServer::SendPacket, this, socket, from, i, num_images_rqstd, query_id ); 
  }
}
  
void
//TopkQueryServer::SendPacket (Ptr<Socket> socket, Address from, int packetNum, int num_images_rqstd, int query_id)
TopkQueryServer::SendPacket ( Address from, int packetNum, int num_images_rqstd, int query_id)
{
//  NS_ASSERT(m_sendEvent.IsExpired());
  Ptr<Packet> p = Create<Packet>(image_size_bytes);

  TopkQueryTag tag( query_id, num_images_rqstd, packetNum ); 
  p->AddPacketTag(tag);
  //int bytes_sent = socket->SendTo (p, 0, from) < 0;
  int bytes_sent = m_socket->SendTo (p, 0, from) < 0;
  if( bytes_sent < 0 )
  {
    std::cout<< "ERROR: sending packet " << packetNum << "from server failed at time " << Simulator::Now().GetSeconds() << "\n";
  }
  else
  {
    if (InetSocketAddress::IsMatchingType (from))
    {
      if( TOPK_QUERY_SERVER_DEBUG )
      {
        std::cout<<"At time " << Simulator::Now ().GetSeconds () << "s server (Node " << GetNode()->GetId() << ") sent packet/image " << packetNum << " ("<< bytes_sent << " bytes) to " <<
                   InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                   InetSocketAddress::ConvertFrom (from).GetPort () << "\n";
      }
    }
  }
}


} // Namespace ns3
