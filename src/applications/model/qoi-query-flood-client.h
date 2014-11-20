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

#ifndef QOI_QUERY_FLOOD_CLIENT_H
#define QOI_QUERY_FLOOD_CLIENT_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/traced-callback.h"
#include "ns3/tag.h"
#include "ns3/random-variable-stream.h"
#include <vector>

#define QOI_QUERY_FLOOD_CLIENT_DEBUG 0

namespace ns3 {

class Socket;
class Packet;

struct QoiQuery 
{
  uint16_t id;
  uint16_t num_images_sent;
  uint16_t num_images_rcvd;
  Time start_time;
  Time deadline;
};

struct QoiQueryTag : public Tag
{
  int sender_id, query_id, num_images_sent, image_num;

  QoiQueryTag ( int new_sender_id = -1, int new_query_id = -1, int new_num_images_sent = -1, int new_image_num = 0) : Tag(), sender_id(new_sender_id), query_id(new_query_id), num_images_sent(new_num_images_sent), image_num(new_image_num) {}

  static TypeId GetTypeId()
  {
    static TypeId tid = TypeId("ns3::QoiQueryTag").SetParent<Tag>();
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
    i.WriteU32 (sender_id);
    i.WriteU32 (query_id);
    i.WriteU32 (num_images_sent);
    i.WriteU32 (image_num);
  }
  
  void Deserialize (TagBuffer i) 
  {
    sender_id = i.ReadU32 ();
    query_id = i.ReadU32 ();
    num_images_sent = i.ReadU32 ();
    image_num = i.ReadU32 ();
  }
  
  void Print (std::ostream &os) const
  {
    os << "QoiQueryTag: [sender_id, query_id, num_images_sent, image_num] = [" << sender_id << ", " << query_id << "," << num_images_sent << "," << image_num  << "]\n";
  }

};

/**
 * \ingroup qoi-flood-query
 * \brief A Qoi query client
 *
 * Every packet sent should be returned by the server and received here.
 */
class QoiQueryFloodClient : public Application 
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  QoiQueryFloodClient ();

  virtual ~QoiQueryFloodClient ();

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

  void SetDataSize (uint32_t dataSize);
  uint32_t GetDataSize (void) const;
  void SetFill (std::string fill);
  void SetFill (uint8_t fill, uint32_t dataSize);
  void SetFill (uint8_t *fill, uint32_t fillSize, uint32_t dataSize);

  void SetNumNodes( uint16_t numNodes ) { num_nodes = numNodes; };

protected:
  virtual void DoDispose (void);

private:

  void Init();

  virtual void StartApplication (void);
  virtual void StopApplication (void);
  void ScheduleTransmit (Time dt);
  void Send (void);
  void SendPacket( uint16_t dest, int numImagesNeeded, int query_id );
  void HandleRead (Ptr<Socket> socket);

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
  int num_packets_per_image;
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
  uint32_t num_packets_rcvd;

  int image_size_bytes;
  double delay_padding;
	
  int avg_num_images_rqrd;
	Time timeliness;
  Time run_time;
  uint16_t run_seed;
  uint16_t num_runs;
  std::string SumSimFilename;

  uint16_t query_ids;

  //Ptr<NormalRandomVariable> rand_interval;
  Ptr<ExponentialRandomVariable> rand_interval;
  Ptr<UniformRandomVariable> rand_dest;
  Ptr<NormalRandomVariable> rand_ss_dist;
};

} // namespace ns3

#endif 
