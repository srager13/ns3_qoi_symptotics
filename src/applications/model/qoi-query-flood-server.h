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

#ifndef QOI_FLOOD_QUERY_SERVER_H
#define QOI_FLOOD_QUERY_SERVER_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/address.h"
#include "ns3/qoi-query-flood-client.h"

#define QOI_QUERY_FLOOD_SERVER_DEBUG 0

namespace ns3 {

class Socket;
class Packet;

/**
 * \ingroup applications 
 * \defgroup topkquery Qoi Query
 */

/**
 * \ingroup topkquery
 * \brief A Qoi Query server
 *
 * Every packet received is sent back.
 */
class QoiQueryFloodServer : public Application 
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  QoiQueryFloodServer ();
  virtual ~QoiQueryFloodServer ();
  

  void SetNumNodes( uint16_t numNodes ) { num_nodes = numNodes; };
  void CheckQuery( int sender_id, int query_id );

protected:
  virtual void DoDispose (void);

private:

  virtual void StartApplication (void);
  virtual void StopApplication (void);

  /**
   * \brief Handle a packet reception.
   *
   * This function is called by lower layers.
   *
   * \param socket the socket the packet was received to.
   */
  void Init();
  void HandleRead (Ptr<Socket> socket);
  //void SendPacket (Ptr<Socket> socket, Address from, int packetNum, int query_id); 
  void PrintStats();

  double sum_similarity;
  uint16_t m_port; //!< Port on which we listen for incoming packets.
  int image_size_bytes; // size in bytes of each image
  int avg_num_images_rqrd;
	Time timeliness;
  Time run_time;
  uint16_t run_seed;
  uint16_t num_runs;
  std::string SumSimFilename;

  double delay_padding; // delay time (in seconds) that server waits after sending each image to ensure no loss
  std::string DataFilePath;
  uint16_t num_nodes;
  uint32_t num_queries_issued;
  uint32_t num_queries_satisfied;
  uint32_t num_packets_rcvd;
  uint32_t num_packets_rcvd_late;
  Ptr<Socket> m_socket; //!< IPv4 Socket
  Ptr<Socket> m_socket6; //!< IPv6 Socket
  Address m_local; //!< local multicast address

  std::vector< std::vector<QoiQuery> > active_queries;
  std::vector< std::vector<QoiQuery> > old_queries;
};

} // namespace ns3

#endif /* QOI_FLOOD_QUERY_SERVER_H */

