/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Hemanth Narra
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
 *
 * Author: Hemanth Narra <hemanthnarra222@gmail.com>
 *
 * James P.G. Sterbenz <jpgs@ittc.ku.edu>, director
 * ResiliNets Research Group  http://wiki.ittc.ku.edu/resilinets
 * Information and Telecommunication Technology Center (ITTC)
 * and Department of Electrical Engineering and Computer Science
 * The University of Kansas Lawrence, KS USA.
 *
 * Work supported in part by NSF FIND (Future Internet Design) Program
 * under grant CNS-0626918 (Postmodern Internet Architecture),
 * NSF grant CNS-1050226 (Multilayer Network Resilience Analysis and Experimentation on GENI),
 * US Department of Defense (DoD), and ITTC at The University of Kansas.
 */
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"
#include "tdma-mac-queue.h"
#include "ns3/node.h"
#include "ns3/node-list.h"
#include "ns3/topk-query-client.h"
#include "ns3/topk-query-server.h"

#define FLOW_COUNT_START_TIME 50
#define FLOW_COUNT_STOP_TIME 200

using namespace std;
NS_LOG_COMPONENT_DEFINE ("TdmaMacQueue");
namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (TdmaMacQueue);

TdmaMacQueue::Item::Item (Ptr<const Packet> packet,
                          const WifiMacHeader &hdr,
                          Time tstamp)
  : packet (packet),
    hdr (hdr),
    tstamp (tstamp)
{
}

TypeId
TdmaMacQueue::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TdmaMacQueue")
    .SetParent<Object> ()
    .AddConstructor<TdmaMacQueue> ()
    .AddAttribute ("MaxPacketNumber", "If a packet arrives when there are already this number of packets, it is dropped.",
                  #ifdef SMALL_QUEUES 
                   UintegerValue (250),
                  #else
                   UintegerValue (10000),
                  #endif
                   MakeUintegerAccessor (&TdmaMacQueue::m_maxSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxDelay", "If a packet stays longer than this delay in the queue, it is dropped.",
                   TimeValue (Seconds (50.0)),
                   MakeTimeAccessor (&TdmaMacQueue::m_maxDelay),
                   MakeTimeChecker ())
    .AddAttribute ("RunTime", "Total run time of simulation.",
                   TimeValue (Seconds (250.0)),
                   MakeTimeAccessor (&TdmaMacQueue::run_time),
                   MakeTimeChecker ())
    .AddAttribute ("NumNodes", 
                   "Number of nodes in the scenario.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&TdmaMacQueue::num_nodes),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute("DataFilePath",
                   "Path to the directory containing the .csv stats file.",
                   StringValue("./"),
                   MakeStringAccessor (&TdmaMacQueue::DataFilePath),
                   MakeStringChecker())
  ;
  return tid;
}

TdmaMacQueue::TdmaMacQueue ()
  : m_size (0),
    m_count (0)
{
  NS_LOG_FUNCTION_NOARGS ();
  sum_number_flows = 0;
  avg_number_flows = 0;
  number_times_counted_flows = 0;
  max_num_current_flows = 0;
  avg_queue_size = 0;
  num_dropped_packets = 0;
  Simulator::Schedule( Seconds(FLOW_COUNT_START_TIME), &TdmaMacQueue::UpdateNumberActiveFlows, this );
  Simulator::Schedule( Seconds(0.5), &TdmaMacQueue::CheckForTimedOutQueries, this ); // always check for timed out queries just
                                                                                      //  before updating # of active queries
  Simulator::Schedule( Seconds(FLOW_COUNT_STOP_TIME), &TdmaMacQueue::PrintStats, this );
  Simulator::Schedule( NanoSeconds(10), &TdmaMacQueue::SetNodeId, this );
}

TdmaMacQueue::~TdmaMacQueue ()
{
  Flush ();
}

void
TdmaMacQueue::SetNodeId ()
{
  node_id = m_netDevicePtr->GetNode()->GetId();
}
  

void
TdmaMacQueue::SetMaxSize (uint32_t maxSize)
{
  std::cout<<"Setting max size to " << maxSize << " in tdma-mac-queue.cc\n";
  m_maxSize = maxSize;
}

void
TdmaMacQueue::SetMacPtr (Ptr<TdmaMac> macPtr)
{
  m_macPtr = macPtr;
}

void
TdmaMacQueue::SetNetDevicePtr (Ptr<TdmaNetDevice> netDevicePtr)
{
  m_netDevicePtr = netDevicePtr;
}

void
TdmaMacQueue::SetMaxDelay (Time delay)
{
  m_maxDelay = delay;
}

void
TdmaMacQueue::SetTdmaMacTxDropCallback (Callback<void,Ptr<const Packet> > callback)
{
  m_txDropCallback = callback;
}

uint32_t
TdmaMacQueue::GetMaxSize (void) const
{
  return m_maxSize;
}

Time
TdmaMacQueue::GetMaxDelay (void) const
{
  return m_maxDelay;
}

bool
TdmaMacQueue::Enqueue (Ptr<const Packet> packet, const WifiMacHeader &hdr)
{
  TopkQueryTag tag(-1, -1, 0, -1);
  packet->PeekPacketTag(tag);
  QueryDeadlineTag dl_tag(0.0);
  packet->PeekPacketTag(dl_tag);
//  std::cout<<"Time: " << Simulator::Now().GetSeconds() << " Queue Size: " << m_size << " Max Size: " << m_maxSize << "\n";
  if( TDMA_MAC_QUEUE_DEBUG )
  {
    std::cout<<"TdmaMacQueue::Enqueue (Time = " << Simulator::Now().GetSeconds() << "):  Queue Size: " << 
                m_size << " Max Size: " << m_maxSize << "\n";
    std::cout<<"\tsending_node = " << tag.sending_node << "\n";
    std::cout<<"\tquery_id = " << tag.query_id << "\n";
    std::cout<<"\timage_num = " << tag.image_num << "\n";
    std::cout<<"\ttotal_num = " << tag.num_images_rqstd << "\n";
    std::cout<<"\tdeadline = " << dl_tag.deadline << "\n";
  }
  //Cleanup ();
  if (m_size == m_maxSize)  // TODO::drop packets past deadline?
  {
    num_dropped_packets++;
    Ptr<TopkQueryClient> clientPtr = m_netDevicePtr->GetNode()->GetApplication(1)->GetObject<TopkQueryClient>();
    clientPtr->IncrementNumPacketsDropped();

    if( TDMA_MAC_QUEUE_DEBUG )
    {
    std::cout<<"ERROR: Dropped packet in queue: \n\tTdmaMacQueue::Enqueue (Time = " << 
                Simulator::Now().GetSeconds() << "):  Queue Size: " << 
                GetSize () << " Max Size: " << GetMaxSize () << "\n";
    std::cout<<"\tsending_node = " << tag.sending_node << "\n";
    std::cout<<"\tquery_id = " << tag.query_id << "\n";
    std::cout<<"\timage_num = " << tag.image_num << "\n";
    std::cout<<"\ttotal_num = " << tag.num_images_rqstd << "\n";
    std::cout<<"\tdeadline = " << dl_tag.deadline << "\n";
    }

    return false;
  }
  
  Time now = Simulator::Now ();
  m_queue.push_back (Item (packet, hdr, now));
  m_size++;
  NS_LOG_DEBUG ("Inserted packet of size: " << packet->GetSize () );
    
  bool found_query = false;
  if( tag.sending_node == node_id || tag.sending_node == -1 || tag.query_id == -1 )
  {
    return true;
  }
    // find active query in the list and update images_rcvd
    //std::cout<<"Number of flows = " << flows.size() << "\n";
    for( uint16_t i = 0; i < flows.size(); i++ )
    {

      if( flows[i].server_node == tag.sending_node && flows[i].id == tag.query_id )
      {
        found_query = true;

        still_active[i] = true;

        flows[i].num_images_rcvd++;
            
        if( TDMA_MAC_QUEUE_DEBUG )
        {
          std::cout<<"Node " << node_id << ": Found flow (adding to packets_rcvd):\n";
          std::cout<<"\tsending node = " << flows[i].server_node << "\n";
          std::cout<<"\tquery id = " << flows[i].id << "\n";
          std::cout<<"\treceived images = " << flows[i].num_images_rcvd << " / " << flows[i].num_images_rqstd << "\n";
        }
       
        // check to see if past deadline or done with query.  If so, go to check query to deal with it and start new one. 
        //if( flows[i].num_images_rcvd == flows[i].num_images_rqstd )
        if( Simulator::Now() > flows[i].deadline )
        {
          // remove query
          //if( node_id == 1 )
          if( TDMA_MAC_QUEUE_DEBUG )
          {
            std::cout<<"Node " << node_id << ": Removing flow at time " << Simulator::Now().GetSeconds() << ":\n";
            std::cout<<"\tsending node = " << flows[i].server_node << "\n";
            std::cout<<"\tquery id = " << flows[i].id << "\n";
            std::cout<<"\tdeadline = " << flows[i].deadline.GetSeconds();
          }
          flows.erase(flows.begin() + i);
          still_active.erase(still_active.begin() + i);
        }

      }
    } 
    if( !found_query )
    {
      //if( node_id == 1 )
      if( TDMA_MAC_QUEUE_DEBUG )
      {
        std::cout<<"Node " << node_id << ": Creating new flow at time " << Simulator::Now().GetSeconds() << ": \n";
        std::cout<<"\tsending_node = " << tag.sending_node << "\n";
        std::cout<<"\tquery_id = " << tag.query_id << "\n";
      }

      // create new query 
      TopkQuery new_query;
      new_query.id =  tag.query_id;
      new_query.server_node = tag.sending_node;
      new_query.num_images_rqstd = tag.num_images_rqstd;
      new_query.num_images_rcvd = 1;
      new_query.start_time = Simulator::Now();
      new_query.deadline = Seconds(dl_tag.deadline);
          
      // add to list of active queries 
      flows.push_back( new_query ); 
      bool active = true;
      still_active.push_back( active );
    }

  return true;
}

void
TdmaMacQueue::Cleanup (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  if (m_queue.empty ())
    {
      return;
    }
  Time now = Simulator::Now ();
  uint32_t n = 0;
  for (PacketQueueI i = m_queue.begin (); i != m_queue.end (); )
    {
      if (i->tstamp + m_maxDelay > now)
        {
          i++;
        }
      else
        {
          m_count++;
          NS_LOG_DEBUG (Simulator::Now ().GetSeconds () << "s Dropping this packet as its exceeded queue time, pid: " << i->packet->GetUid ()
                                                        << " macPtr: " << m_macPtr
                                                        << " queueSize: " << m_queue.size ()
                                                        << " count:" << m_count);
          m_txDropCallback (i->packet);
          i = m_queue.erase (i);
          n++;
        }
    }
  m_size -= n;
}

Ptr<const Packet>
TdmaMacQueue::Dequeue (WifiMacHeader *hdr)
{
  NS_LOG_FUNCTION_NOARGS ();
  //Cleanup ();
  if (!m_queue.empty ())
    {
      Item i = m_queue.front ();
      m_queue.pop_front ();
      m_size--;
      *hdr = i.hdr;
      NS_LOG_DEBUG ("Dequeued packet of size: " << i.packet->GetSize ());
      return i.packet;
    }
  return 0;
}

Ptr<const Packet>
TdmaMacQueue::Peek (WifiMacHeader *hdr)
{
  NS_LOG_FUNCTION_NOARGS ();
  //Cleanup ();
  if (!m_queue.empty ())
    {
      Item i = m_queue.front ();
      *hdr = i.hdr;
      return i.packet;
    }
  return 0;
}

bool
TdmaMacQueue::IsEmpty (void)
{
  //Cleanup ();
  return m_queue.empty ();
}

uint32_t
TdmaMacQueue::GetSize (void)
{
  return m_size;
}

void
TdmaMacQueue::CheckForTimedOutQueries()
{
  for( uint16_t i = 0; i < flows.size(); i++ )
  {
    //if( !still_active[i] )
    if( Simulator::Now() > flows[i].deadline ) 
    {
      //if( node_id == 1 )
      if( TDMA_MAC_QUEUE_DEBUG )
      {
        std::cout<<"Node " << node_id << ": Removing flow in CheckForTimedOutQueries, time = " 
                  << Simulator::Now().GetSeconds() << ":\n";
        std::cout<<"\tsending node = " << flows[i].server_node << "\n";
        std::cout<<"\tquery id = " << flows[i].id << "\n";
      }
      // remove flow
      flows.erase(flows.begin() + i);
      still_active.erase(still_active.begin() + i);
    }
  }
  
  Simulator::Schedule( Seconds(1), &TdmaMacQueue::CheckForTimedOutQueries, this );
  
  for( uint16_t i = 0; i < flows.size(); i++ )
  {
    still_active[i] = false;
  }
}

void
TdmaMacQueue::UpdateNumberActiveFlows()
{
  int current_num_flows = 0;
  if( Simulator::Now() < Seconds(FLOW_COUNT_STOP_TIME) )
	{
    sum_number_flows += flows.size();
    current_num_flows += flows.size();

    if( current_num_flows > max_num_current_flows )
      max_num_current_flows = current_num_flows;

    avg_queue_size = avg_queue_size*number_times_counted_flows + m_size;
  
		number_times_counted_flows++;

    avg_queue_size = avg_queue_size/number_times_counted_flows;

    if( TDMA_MAC_QUEUE_DEBUG )
    {
      std::cout<<"Time " << Simulator::Now().GetSeconds() << ": Node " << node_id << ": average queue size = " 
                << avg_queue_size << ", current size = " << m_size << "\n";
      std::cout<<"Time " << Simulator::Now().GetSeconds() << ": Node " << node_id << ": average number flows = " 
                << avg_number_flows << " (sum = " << sum_number_flows << ")\n";
      std::cout<<"Time " << Simulator::Now().GetSeconds() << ": Node " << node_id << ": current number flows = " 
                << current_num_flows << "\n";
    }
 
		avg_number_flows = sum_number_flows/number_times_counted_flows;
    Simulator::Schedule( Seconds(1), &TdmaMacQueue::UpdateNumberActiveFlows, this );
    
    Ptr<TopkQueryClient> clientPtr = m_netDevicePtr->GetNode()->GetApplication(1)->GetObject<TopkQueryClient>();
    clientPtr->UpdateQueueStats( avg_queue_size, sum_number_flows, number_times_counted_flows );
	}
  return;
}

void
TdmaMacQueue::PrintStats()
{
  char buf[1024];
  FILE *stats_fd;

  sprintf(buf, "%s/TdmaQueueStats.csv", DataFilePath.c_str() );
  stats_fd = fopen(buf, "a");
  if( stats_fd == NULL )
  {
    std::cout << "Error opening stats file: " << buf << "\n";
    return;
  }
  
  if( TDMA_MAC_QUEUE_DEBUG )
  {
    std::cout<< "Printing Stats to " << buf << "....at time " << Simulator::Now().GetSeconds() << "\n";
  }


  int num_nodes = NodeList::GetNNodes();

                   // 1   2  2b   2c   3     4   5   6  6b   7      8     9   10  11  12  13  14    15
  fprintf(stats_fd, "%i, %i, %f, %i, %.2f, %.3f\n",
    node_id, // i
    num_nodes, // 2 i
    avg_queue_size, // 2b f
    num_dropped_packets, // 2c i
    max_num_current_flows, // 3 f
    sum_number_flows/number_times_counted_flows // 4 f
  );
  
  //std::cout<<"Node " << node_id << ": total number flows = " << sum_number_flows << ", number times counted = " << number_times_counted_flows << "\n";

  fclose(stats_fd);

}

void
TdmaMacQueue::Flush (void)
{
  m_queue.erase (m_queue.begin (), m_queue.end ());
  m_size = 0;
}

Mac48Address
TdmaMacQueue::GetAddressForPacket (enum WifiMacHeader::AddressType type, PacketQueueI it)
{
  if (type == WifiMacHeader::ADDR1)
    {
      return it->hdr.GetAddr1 ();
    }
  if (type == WifiMacHeader::ADDR2)
    {
      return it->hdr.GetAddr2 ();
    }
  if (type == WifiMacHeader::ADDR3)
    {
      return it->hdr.GetAddr3 ();
    }
  return 0;
}

bool
TdmaMacQueue::Remove (Ptr<const Packet> packet)
{
  PacketQueueI it = m_queue.begin ();
  for (; it != m_queue.end (); it++)
    {
      if (it->packet == packet)
        {
          m_queue.erase (it);
          m_size--;
          return true;
        }
    }
  return false;
}
} // namespace ns3
