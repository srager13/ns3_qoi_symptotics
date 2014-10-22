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
    .AddAttribute ("NumReturnImages", "Number of images needed to be returned to satisfy QoI requirement of query.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&TopkQueryServer::num_return_images),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("ImageSizeBytes", "Size of image (all assumed to be the same) in bytes.",
                   UintegerValue (2000000),
                   MakeUintegerAccessor (&TopkQueryServer::image_size_bytes),
                   MakeUintegerChecker<uint64_t> ())
  ;
  return tid;
}

TopkQueryServer::TopkQueryServer ()
{
  NS_LOG_FUNCTION (this);
}

TopkQueryServer::~TopkQueryServer()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_socket6 = 0;
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

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
      m_socket->Bind (local);
      if (addressUtils::IsMulticast (m_local))
        {
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

  if (m_socket6 == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket6 = Socket::CreateSocket (GetNode (), tid);
      Inet6SocketAddress local6 = Inet6SocketAddress (Ipv6Address::GetAny (), m_port);
      m_socket6->Bind (local6);
      if (addressUtils::IsMulticast (local6))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket6);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, local6);
            }
          else
            {
              NS_FATAL_ERROR ("Error: Failed to join multicast group");
            }
        }
    }

  m_socket->SetRecvCallback (MakeCallback (&TopkQueryServer::HandleRead, this));
  m_socket6->SetRecvCallback (MakeCallback (&TopkQueryServer::HandleRead, this));
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
  if (m_socket6 != 0) 
    {
      m_socket6->Close ();
      m_socket6->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void 
TopkQueryServer::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

	int i;
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
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
                       Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port " <<
                       Inet6SocketAddress::ConvertFrom (from).GetPort ());
        }

      packet->RemoveAllPacketTags ();
      packet->RemoveAllByteTags ();

      std::cout<<"Server sending images from server to " << from <<"\n";
			for( i = 0; i < num_return_images; i++ )
			{
				Ptr<Packet> p = Create<Packet>(image_size_bytes);
      	if( socket->SendTo (p, 0, from) < 0 )
				{
					std::cout<< "ERROR: sending packet " << i + 1 << "from server failed at time " << Simulator::Now().GetSeconds() << "\n";
				}

      if (InetSocketAddress::IsMatchingType (from))
        {
          std::cout<<"At time " << Simulator::Now ().GetSeconds () << "s server sent " << p->GetSize () << " bytes to " <<
                       InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                       InetSocketAddress::ConvertFrom (from).GetPort () << "\n";
        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
          std::cout<<"At time " << Simulator::Now ().GetSeconds () << "s server sent " << p->GetSize () << " bytes to " <<
                       Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port " <<
                       Inet6SocketAddress::ConvertFrom (from).GetPort () << "\n";
        }
			}
    }
}

} // Namespace ns3
