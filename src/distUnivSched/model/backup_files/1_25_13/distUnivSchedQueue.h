/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 University of Washington
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

#ifndef DISTUNIVSCHEDQUEUE_H
#define DISTUNIVSCHEDQUEUE_H

#define DIST_UNIV_SCHED_QUEUE_DEBUG 0

#include <queue>
#include "ns3/packet.h"
#include "ns3/drop-tail-queue.h"

namespace ns3 {
  
  /**
   * \ingroup queue
   *
   * \brief 
   */
  class DistUnivSchedQueue : public DropTailQueue {
  public:
    static TypeId GetTypeId (void);
    /**
     * \brief DistUnivSchedQueue Constructor
     *
     * 
     */
    DistUnivSchedQueue ();
    DistUnivSchedQueue ( uint32_t maxPackets );
    virtual ~DistUnivSchedQueue();
    
    // GET AND SET FUNCTIONS
    void setM_backlog (int backlog);
    int getM_backlog ();

  private:
  
  int m_backlog; 
/*
    virtual bool DoEnqueue (Ptr<Packet> p);
    virtual Ptr<Packet> DoDequeue (void);
    virtual Ptr<const Packet> DoPeek (void) const;
    */
  };
  
} // namespace ns3

#endif /* DISTUNIVSCHEDQUEUE_H */

