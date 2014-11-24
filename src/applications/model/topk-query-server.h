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

#ifndef TOPK_QUERY_SERVER_H
#define TOPK_QUERY_SERVER_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/address.h"

#define TOPK_QUERY_SERVER_DEBUG 0

namespace ns3 {

class Socket;
class Packet;

/**
 * \ingroup applications 
 * \defgroup topkquery TopkQuery
 */

/**
 * \ingroup topkquery
 * \brief A Topk Query server
 *
 * Every packet received is sent back.
 */
class TopkQueryServer : public Application 
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  TopkQueryServer ();
  virtual ~TopkQueryServer ();
  
  void ReceiveQuery( uint16_t node_from, uint16_t query_id, uint16_t num_images_rqstd );

 // uint64_t image_size_bytes; // size in bytes of each image

protected:
  virtual void DoDispose (void);

private:

  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void HandleRead (Ptr<Socket> socket);

  void ScheduleTrx (uint16_t from, int num_images_rqstd, int query_id); 
  //void SendPacket (Ptr<Socket> socket, Address from, int packetNum, int num_images_rqstd, int query_id); 
  void SendPacket (uint16_t from, int packetNum, int num_images_rqstd, int query_id); 

  void PopulateArpCache();

  EventId m_sendEvent;

  uint16_t m_port; //!< Port on which we listen for incoming packets.
  uint64_t image_size_bytes; // size in bytes of each image
  uint16_t num_nodes;
  double delay_padding; // delay time (in seconds) that server waits after sending each image to ensure no loss
  std::vector<Ptr<Socket> > m_socket; //!< Socket
  //Ptr<Socket> m_socket6; //!< IPv6 Socket
  Address m_local; //!< local multicast address
};

} // namespace ns3

#endif /* TOPK_QUERY_SERVER_H */

