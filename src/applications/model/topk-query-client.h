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

#ifndef TOPK_QUERY_CLIENT_H
#define TOPK_QUERY_CLIENT_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/traced-callback.h"
#include "ns3/tag.h"
#include "ns3/random-variable-stream.h"
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#define TOPK_QUERY_CLIENT_DEBUG 0
#define ONE_FLOW_DEBUG 1

namespace ns3 {

class Socket;
class Packet;

struct TopkQuery 
{
  uint16_t id;
  uint16_t server_node;
  uint16_t num_images_rqstd;
  uint16_t num_images_rcvd;
  Time start_time;
  Time deadline;
};

struct TopkQueryTag : public Tag
{
  int query_id, num_images_rqstd, image_num, sending_node;

  TopkQueryTag ( int new_query_id = -1, int new_num_images_rqstd = -1, int new_image_num = 0, int sn = 0) : Tag(), query_id(new_query_id), num_images_rqstd(new_num_images_rqstd), image_num(new_image_num), sending_node(sn) {}

  static TypeId GetTypeId()
  {
    static TypeId tid = TypeId("ns3::TopkQueryTag").SetParent<Tag>();
    return tid;
  }

  TypeId GetInstanceTypeId() const
  {
    return GetTypeId();
  }

  uint32_t GetSerializedSize () const
  {
    return 4*sizeof(int);
  }

  void Serialize (TagBuffer i) const
  {
    i.WriteU32 (query_id);
    i.WriteU32 (num_images_rqstd);
    i.WriteU32 (image_num);
    i.WriteU32 (sending_node);
  }
  
  void Deserialize (TagBuffer i) 
  {
    query_id = i.ReadU32 ();
    num_images_rqstd = i.ReadU32 ();
    image_num = i.ReadU32 ();
    sending_node = i.ReadU32 ();
  }
  
  void Print (std::ostream &os) const
  {
    os << "TopkQueryTag: [query_id, num_images_rqstd, image_num, sending_node] = [" << query_id << "," << num_images_rqstd << "," << image_num  << "," << sending_node << "]\n";
  }

};

/**
 * \ingroup topkquery
 * \brief A Topk Query client
 *
 * Every packet sent should be returned by the server and received here.
 */
class TopkQueryClient : public Application 
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  TopkQueryClient ();

  virtual ~TopkQueryClient ();

 static Ipv4Address GetNodeAddress( uint32_t node_id, uint16_t num_nodes )
  {
    uint8_t firstByte = (node_id+1)&(uint8_t)255;

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
    if( node_id < 255 )
    {
      secondByte = second_byte_start;
    }
    else if( node_id < 511 )
    {
      secondByte = second_byte_start + (uint8_t)1;
    }
    else if( node_id < 767 )
    {
      secondByte = second_byte_start + (uint8_t)2;
    }
    else if( node_id < 1023 )
    {
      secondByte = second_byte_start + (uint8_t)3;
    }
    else if( node_id < 1279 )
    {
      secondByte = second_byte_start + (uint8_t)4;
    }
    else if( node_id < 1535 )
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

    return Ipv4Address(buf);
  }
     
  /**
   * \brief set the remote address and port
   * \param ip remote IPv4 address
   * \param port remote port
   */
  void SetRemote (Ipv4Address ip, uint16_t port);
  /**
   * \brief set the remote address and port
   * \param ip remote IPv6 address
   * \param port remote port
   */
  void SetRemote (Ipv6Address ip, uint16_t port);
  /**
   * \brief set the remote address and port
   * \param ip remote IP address
   * \param port remote port
   */
  void SetRemote (Address ip, uint16_t port);

  /**
   * Set the data fill of the packet (what is sent as data to the server) to 
   * the zero-terminated contents of the fill string string.
   *
   * \warning The size of resulting echo packets will be automatically adjusted
   * to reflect the size of the fill string -- this means that the PacketSize
   * attribute may be changed as a result of this call.
   *
   * \param fill The string to use as the actual echo data bytes.
   */

  void SetNumNodes( uint16_t numNodes ) { num_nodes = numNodes; };

  void IncrementNumPacketsDropped();

protected:
  virtual void DoDispose (void);

private:

  void Init();

  virtual void StartApplication (void);
  virtual void StopApplication (void);

  /**
   * \brief Schedule the next packet transmission
   * \param dt time interval between packets.
   */
  void ScheduleTransmit (Time dt);
  /**
   * \brief Send a packet
   */
  void Send (void);

  /**
   * \brief Handle a packet reception.
   *
   * This function is called by lower layers.
   *
   * \param socket the socket the packet was received to.
   */
  void HandleRead (Ptr<Socket> socket);

  void CheckQuery ( uint16_t id, uint16_t server ); 
  void PrintStats();

  void UpdateQueryIds ()
  {
    if( query_ids == 65000 )
    {
      query_ids = 1;
    }
    else
    {
      query_ids++;
    }
  }

  Time m_interval; //!< Packet inter-send time
  double sum_similarity;
  uint32_t m_size; //!< Size of the sent packet

  uint8_t *m_data; //!< packet payload data

  uint32_t m_sent; //!< Counter for sent packets
  //std::vector<Ptr<Socket> > m_socket; //!< Socket
  Ptr<Socket> m_socket; //!< Socket
  Address m_peerAddress; //!< Remote peer address
  uint16_t m_peerPort; //!< Remote peer port
  EventId m_sendEvent; //!< Event to send the next packet

  /// Callbacks for tracing the packet Tx events
  TracedCallback<Ptr<const Packet> > m_txTrace;

  uint16_t num_nodes;
  uint32_t num_queries_issued;
  uint32_t num_queries_satisfied;
  uint32_t num_queries_unanswered;
  uint32_t num_packets_rcvd_late;
  uint32_t num_packets_rcvd;

  uint16_t image_size_bytes;
	
  int avg_num_packets_rqrd;
	Time timeliness;
  Time run_time;
  uint16_t run_seed;
  uint16_t num_runs;
  std::string DataFilePath;
  std::string SumSimFilename;
  int num_packets_per_image;

  std::vector<TopkQuery> active_queries;
  uint16_t query_ids;
  uint16_t num_packets_dropped;

  //Ptr<NormalRandomVariable> rand_interval;
  Ptr<ExponentialRandomVariable> rand_interval;
  Ptr<UniformRandomVariable> rand_dest;
  Ptr<NormalRandomVariable> rand_ss_dist;
};

} // namespace ns3

#endif /* TOPK_QUERY_CLIENT_H */
