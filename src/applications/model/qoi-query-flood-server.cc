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
#include "ns3/integer.h"
#include "ns3/string.h"
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>

#include "qoi-query-flood-server.h"
#include "qoi-query-flood-client.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("QoiQueryFloodServerApplication");
NS_OBJECT_ENSURE_REGISTERED (QoiQueryFloodServer);

TypeId
QoiQueryFloodServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::QoiQueryFloodServer")
    .SetParent<Application> ()
    .AddConstructor<QoiQueryFloodServer> ()
    .AddAttribute ("Port", "Port on which we listen for incoming packets.",
                   UintegerValue (9),
                   MakeUintegerAccessor (&QoiQueryFloodServer::m_port),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("SumSimilarity", 
                   "QoI Sum Similarity requirement of query.",
                   DoubleValue (100),
                   MakeDoubleAccessor (&QoiQueryFloodServer::sum_similarity),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("ImageSizeBytes", "Size of image (all assumed to be the same) in bytes.",
                   IntegerValue (2000000),
                   MakeIntegerAccessor (&QoiQueryFloodServer::image_size_bytes),
                   MakeIntegerChecker<int> ())
    .AddAttribute ("DelayPadding", "Delay time in seconds that server waits after sending each image to prevent overloading socket.",
                   DoubleValue(0.1),
                   MakeDoubleAccessor(&QoiQueryFloodServer::delay_padding),
                   MakeDoubleChecker<double> ())
    .AddAttribute("DataFilePath",
                   "Path to the directory containing the .csv stats file.",
                   StringValue("./"),
                   MakeStringAccessor (&QoiQueryFloodServer::DataFilePath),
                   MakeStringChecker())
    .AddAttribute ("Timeliness", 
                   "Timeliness constraint",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&QoiQueryFloodServer::timeliness),
                   MakeTimeChecker ())
    .AddAttribute("SumSimFilename",
                   "File to look up sum sim vs. number of packets needed (in $NS3_DIR).",
                   StringValue("SumSimRequirements.csv"),
                   MakeStringAccessor (&QoiQueryFloodServer::SumSimFilename),
                   MakeStringChecker())
    .AddAttribute ("NumNodes", 
                   "Number of nodes in the scenario.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&QoiQueryFloodServer::SetNumNodes),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("RunTime", "Total time of simulation.",
                   TimeValue (Seconds (50)),
                   MakeTimeAccessor (&QoiQueryFloodServer::run_time),
                   MakeTimeChecker ())
    .AddAttribute ("RunSeed", "Trial number.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&QoiQueryFloodServer::run_seed),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("NumRuns", "Total number of trials.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&QoiQueryFloodServer::num_runs),
                   MakeUintegerChecker<uint16_t> ())
  ;
  return tid;
}

QoiQueryFloodServer::QoiQueryFloodServer ():
  delay_padding(0.1)
{
  NS_LOG_FUNCTION (this);
  num_queries_issued = 0;
  num_queries_satisfied = 0;
  num_packets_rcvd = 0;
  num_packets_rcvd_late = 0;
  Simulator::Schedule( NanoSeconds(1), &QoiQueryFloodServer::Init, this );
}

void
QoiQueryFloodServer::Init()
{

  for( int i = 0; i < num_nodes; i++ )
  {
    active_queries.push_back( std::vector<QoiQuery>() );
    old_queries.push_back( std::vector<QoiQuery>() );
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
      num_images = 10*(int)strtod(buf,&pEnd);
    
      if( QOI_QUERY_FLOOD_SERVER_DEBUG )
      {
        std::cout<<"From file: sum_sim = " << sum_sim << ", num_images = " << num_images <<" (input sum similarity = " << sum_similarity << ")\n";
      }

      if( sum_sim >= sum_similarity )
      {
        avg_num_images_rqrd = num_images;
        if( QOI_QUERY_FLOOD_SERVER_DEBUG )
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

  Time delay = run_time-Simulator::Now()-NanoSeconds(100);
  if( QOI_QUERY_FLOOD_SERVER_DEBUG )
  {
    std::cout<<"Scheduling PrintStats with delay = " << delay.GetSeconds() << "...run_time = " << run_time.GetSeconds() << "\n";
  }
  Simulator::Schedule( delay, &QoiQueryFloodServer::PrintStats, this );
}

QoiQueryFloodServer::~QoiQueryFloodServer()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_socket6 = 0;
}

void
QoiQueryFloodServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void 
QoiQueryFloodServer::StartApplication (void)
{
  NS_LOG_FUNCTION (this);
	if( QOI_QUERY_FLOOD_SERVER_DEBUG )
	{
		std::cout<< "Node " << GetNode()->GetId() << " in QoiQueryFloodServer::StartApplication at " << Simulator::Now().GetSeconds() << "\n";
	}

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
      m_socket->Bind (local);
      if (addressUtils::IsMulticast (m_local))
        {
	        if( QOI_QUERY_FLOOD_SERVER_DEBUG )
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

  m_socket->SetRecvCallback (MakeCallback (&QoiQueryFloodServer::HandleRead, this));
  m_socket6->SetRecvCallback (MakeCallback (&QoiQueryFloodServer::HandleRead, this));
}

void 
QoiQueryFloodServer::StopApplication ()
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
QoiQueryFloodServer::HandleRead (Ptr<Socket> socket)
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
      /*else if (Inet6SocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
                       Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port " <<
                       Inet6SocketAddress::ConvertFrom (from).GetPort ());
        }
      */

      int num_images_sent = 0, query_id = -1, sender_id = -1, query_index = -1;
      QoiQueryTag tag( -1, -1, -1, 0 );
      if( packet->RemovePacketTag(tag) )
      {
        bool query_not_found = true;
        query_id = tag.query_id;
        sender_id = tag.sender_id;
        num_images_sent = tag.num_images_sent;

        if( QOI_QUERY_FLOOD_SERVER_DEBUG )
        {
          std::cout<<"Node " << GetNode()->GetId() << " looking for query " << query_id << " from node " << sender_id << "\n";
        }
        // find active query in the list and update images_rcvd
        for( uint16_t i = 0; i < active_queries[tag.sender_id].size(); i++ )
        {
          if( active_queries[tag.sender_id][i].id == tag.query_id )
          {
            if( QOI_QUERY_FLOOD_SERVER_DEBUG )
            {
              std::cout<<"\tfound active query " << tag.query_id << " from node " << sender_id << "\n";
            }
            query_not_found = false;
            active_queries[tag.sender_id][i].num_images_rcvd++;
            query_index = i;
            num_packets_rcvd++;
            break;
          }
        } 
        if( query_not_found )
        {
          for( uint16_t i = 0; i < old_queries[tag.sender_id].size(); i++ )
          {
            if( old_queries[tag.sender_id][i].id == tag.query_id )
            {
              if( QOI_QUERY_FLOOD_SERVER_DEBUG )
              {
                std::cout<<"\tfound old query " << tag.query_id << " from node " << sender_id << "\n";
              }
              query_not_found = false;
              num_packets_rcvd_late++;
              break;
            }
          }
        }
        if( query_not_found )
        {
          if( QOI_QUERY_FLOOD_SERVER_DEBUG )
          {
            std::cout<<"\tcould not find query " << tag.query_id << " from node " << sender_id << "...creating new one\n";
          }
          QoiQuery new_query;
          new_query.id =  tag.query_id;
          new_query.num_images_sent = tag.num_images_sent;
          num_images_sent = tag.num_images_sent;
          new_query.num_images_rcvd = 1;
          new_query.start_time = Simulator::Now();
          new_query.deadline = Simulator::Now() + timeliness;
          active_queries[tag.sender_id].push_back( new_query );
          query_index = active_queries[tag.sender_id].size();

          num_queries_issued++;
          num_packets_rcvd++;
    
          // Schedule function to check on this query at the deadline to see if it was satisfied
          if( Simulator::Now() + timeliness < run_time )
            Simulator::Schedule( timeliness, &QoiQueryFloodServer::CheckQuery, this, tag.sender_id, tag.query_id );
 
        }
      }

      if( QOI_QUERY_FLOOD_SERVER_DEBUG )
      {
        if( sender_id == -1 )
        {
          std::cout<<"ERROR: sender id was not found in the qoi query tag.\n";
        }
        else if( query_index == -1 )
        {
          std::cout<<"ERROR: query id was not found in the qoi query tag.\n";
        }
        else
        { 
          std::cout<<"Server (Node " << GetNode()->GetId() << ") received image " << active_queries[sender_id][query_index].num_images_rcvd << "/" << num_images_sent << " images from node " << sender_id <<"\n";
        }
      }
/*			for( i = 0; i < num_images_sent; i++ )
			{
        Time delay = Seconds(i*(((image_size_bytes*8.0)/11000000.0)+delay_padding));
        //Time delay = Seconds(0.1*i);
        Simulator::Schedule( delay, &QoiQueryFloodServer::SendPacket, this, socket, from, i, query_id ); 

			}
*/
    }
}

void
QoiQueryFloodServer::CheckQuery( int sender_id, int id )
{

  if( QOI_QUERY_FLOOD_SERVER_DEBUG )
  {
    std::cout<<"In QoiQueryFloodServer::CheckQuery() at time " << Simulator::Now().GetSeconds() << "\n";
    std::cout<<"\tchecking query with id = " << id << "\n";
  }

  // check query with id to see if it received enough images
  for( uint16_t i = 0; i < active_queries[sender_id].size(); i++ )
  {
    if( active_queries[sender_id][i].id == id )
    {
      if( QOI_QUERY_FLOOD_SERVER_DEBUG )
      {
       std::cout<<"\tfound the query; received " << active_queries[sender_id][i].num_images_rcvd << " / " << active_queries[sender_id][i].num_images_sent << " images\n";
      }
      double perc_rcvd = (double)active_queries[sender_id][i].num_images_rcvd/(double)active_queries[sender_id][i].num_images_sent;
      //if( active_queries[i].num_images_rcvd >= active_queries[i].num_images_sent )
      if( perc_rcvd >= 0.9 ) // consider satisfied if we get 90% of the requested number of images
      {
        num_queries_satisfied++;
      }

      // either way, add it to the old queries and remove it from the list of active queries
      old_queries[sender_id].push_back(active_queries[sender_id][i]);
      active_queries[sender_id].erase(active_queries[sender_id].begin() + i);
    }
  }

  return;
}

void 
QoiQueryFloodServer::PrintStats()
{
  char buf[1024];
  FILE *stats_fd;

  sprintf(buf, "%s/QoiQueryFloodServerStats.csv", DataFilePath.c_str() );
  stats_fd = fopen(buf, "a");
  if( stats_fd == NULL )
  {
    std::cout << "Error opening stats file: " << buf << "\n";
    return;
  }
  
  if( QOI_QUERY_FLOOD_SERVER_DEBUG )
  {
    std::cout<< "Printing Stats to " << buf << "....at time " << Simulator::Now().GetSeconds() << "\n";
  }
                   // 1   2   3   4   5   6   7      8     9   10  11  12  13    14
  fprintf(stats_fd, "%i, %i, %i, %i, %i, %i, %.1f, %.2f, %.1f, %i, %i, %i, %i, %.2f\n",
    GetNode()->GetId(),       // 1 i 
    num_nodes,                // 2 i
    num_queries_satisfied,    // 3 i
    num_queries_issued,       // 4 i
    num_packets_rcvd,         // 5 u
    image_size_bytes,         // 6 i
    run_time.GetSeconds(),    // 7 f
    timeliness.GetSeconds(),  // 8 f
    sum_similarity,           // 9 f
    run_seed,                 // 10 i
    num_runs,                  // 11 i
    0,                        // 12 i
    num_packets_rcvd_late,    // 13 i
    (double)num_queries_satisfied/(double)num_queries_issued // 14 f
  );

  fclose(stats_fd);
}


} // Namespace ns3
