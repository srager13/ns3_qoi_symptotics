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

#include <queue>
#include "distUnivSchedQueue.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/config.h"

namespace ns3 {
NS_OBJECT_ENSURE_REGISTERED (DistUnivSchedQueue);

  TypeId DistUnivSchedQueue::GetTypeId (void) 
  {
    static TypeId tid = TypeId ("ns3::DistUnivSchedQueue")
      .SetParent<DropTailQueue> ()
      .AddConstructor<DistUnivSchedQueue> ()
      ;
    return tid;
  }
  
  DistUnivSchedQueue::DistUnivSchedQueue () : DropTailQueue ()
  {
    if( DIST_UNIV_SCHED_QUEUE_DEBUG )
    {
      std::cout<<"In DistUnivSchedQueue Constructor\n";
    }
    SetMode( PACKETS );
  }
  
  DistUnivSchedQueue::DistUnivSchedQueue ( uint32_t maxPackets ) : DropTailQueue ()
  {
    if( DIST_UNIV_SCHED_QUEUE_DEBUG )
    {
      std::cout<<"In DistUnivSchedQueue Constructor...setting maxPackets to " << maxPackets << ".\n";
    }
  }
  
  DistUnivSchedQueue::~DistUnivSchedQueue ()
  {
    if( DIST_UNIV_SCHED_QUEUE_DEBUG )
    {
      std::cout<<"In DistUnivSchedQueue Constructor\n";
    }
  }
  
  void
  DistUnivSchedQueue::setM_backlog (int backlog)
  {
    m_backlog = backlog;
  }
  
  int
  DistUnivSchedQueue::getM_backlog ()
  {
    return m_backlog;
  }
   
  /*
  bool 
  DistUnivSchedQueue::DoEnqueue (Ptr<Packet> p)
  {   
    std::cout<<"In DoEnqueue in distUnivSchedQueue.cc\n";
    return true;
  }
  
  Ptr<Packet>
  DistUnivSchedQueue::DoDequeue (void)
  {
    std::cout<<"In DoDequeue in distUnivSchedQueue.cc\n";
    Ptr<Packet> p = NULL;
    return p;
  }
  
  Ptr<const Packet> 
  DistUnivSchedQueue::DoPeek (void) const
  {
    std::cout<<"In DoPeek in distUnivSchedQueue.cc\n";
    return NULL;
  }
  */
  
} // namespace ns3


