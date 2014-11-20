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
#include <vector>

#define TOPK_QUERY_CLIENT_DEBUG 0

namespace ns3 {

class Socket;
class Packet;

struct TopkQuery 
{
  uint16_t id;
  uint16_t num_images_rqstd;
  uint16_t num_images_rcvd;
  Time start_time;
  Time deadline;
};

struct TopkQueryTag : public Tag
{
  int query_id, num_images_rqstd, image_num;

  TopkQueryTag ( int new_query_id = -1, int new_num_images_rqstd = -1, int new_image_num = 0) : Tag(), query_id(new_query_id), num_images_rqstd(new_num_images_rqstd), image_num(new_image_num) {}

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
    return 3*sizeof(int);
  }

  void Serialize (TagBuffer i) const
  {
    i.WriteU32 (query_id);
    i.WriteU32 (num_images_rqstd);
    i.WriteU32 (image_num);
  }
  
  void Deserialize (TagBuffer i) 
  {
    query_id = i.ReadU32 ();
    num_images_rqstd = i.ReadU32 ();
    image_num = i.ReadU32 ();
  }
  
  void Print (std::ostream &os) const
  {
    os << "TopkQueryTag: [query_id, num_images_rqstd, image_num] = [" << query_id << "," << num_images_rqstd << "," << image_num  << "]\n";
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
   * Set the data size of the packet (the number of bytes that are sent as data
   * to the server).  The contents of the data are set to unspecified (don't
   * care) by this call.
   *
   * \warning If you have set the fill data for the client using one of the
   * SetFill calls, this will undo those effects.
   *
   * \param dataSize The size of the echo data you want to sent.
   */
  void SetDataSize (uint32_t dataSize);

  /**
   * Get the number of data bytes that will be sent to the server.
   *
   * \warning The number of bytes may be modified by calling any one of the 
   * SetFill methods.  If you have called SetFill, then the number of 
   * data bytes will correspond to the size of an initialized data buffer.
   * If you have not called a SetFill method, the number of data bytes will
   * correspond to the number of don't care bytes that will be sent.
   *
   * \returns The number of data bytes.
   */
  uint32_t GetDataSize (void) const;

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
  void SetFill (std::string fill);

  /**
   * Set the data fill of the packet (what is sent as data to the server) to 
   * the repeated contents of the fill byte.  i.e., the fill byte will be 
   * used to initialize the contents of the data packet.
   * 
   * \warning The size of resulting echo packets will be automatically adjusted
   * to reflect the dataSize parameter -- this means that the PacketSize
   * attribute may be changed as a result of this call.
   *
   * \param fill The byte to be repeated in constructing the packet data..
   * \param dataSize The desired size of the resulting echo packet data.
   */
  void SetFill (uint8_t fill, uint32_t dataSize);

  /**
   * Set the data fill of the packet (what is sent as data to the server) to
   * the contents of the fill buffer, repeated as many times as is required.
   *
   * Initializing the packet to the contents of a provided single buffer is 
   * accomplished by setting the fillSize set to your desired dataSize
   * (and providing an appropriate buffer).
   *
   * \warning The size of resulting echo packets will be automatically adjusted
   * to reflect the dataSize parameter -- this means that the PacketSize
   * attribute of the Application may be changed as a result of this call.
   *
   * \param fill The fill pattern to use when constructing packets.
   * \param fillSize The number of bytes in the provided fill pattern.
   * \param dataSize The desired size of the final echo data.
   */
  void SetFill (uint8_t *fill, uint32_t fillSize, uint32_t dataSize);

  void SetNumNodes( uint16_t numNodes ) { num_nodes = numNodes; };

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

  void CheckQuery ( uint16_t id ); 
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

  uint32_t m_dataSize; //!< packet payload size (must be equal to m_size)
  uint8_t *m_data; //!< packet payload data

  uint32_t m_sent; //!< Counter for sent packets
  std::vector<Ptr<Socket> > m_socket; //!< Socket
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

  //Ptr<NormalRandomVariable> rand_interval;
  Ptr<ExponentialRandomVariable> rand_interval;
  Ptr<UniformRandomVariable> rand_dest;
  Ptr<NormalRandomVariable> rand_ss_dist;
};

} // namespace ns3

#endif /* TOPK_QUERY_CLIENT_H */
