/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 IITP RAS
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
 * Based on 
 *      NS-2 DIST_UNIV_SCHED model developed by the CMU/MONARCH group and optimized and
 *      tuned by Samir Das and Mahesh Marina, University of Cincinnati;
 * 
 *      DIST_UNIV_SCHED-UU implementation by Erik Nordström of Uppsala University
 *      http://core.it.uu.se/core/index.php/DIST_UNIV_SCHED-UU
 *
 * Authors: Elena Buchatskaia <borovkovaes@iitp.ru>
 *          Pavel Boyko <boyko@iitp.ru>
 */
#ifndef __DISTUNIVSCHED_H__
#define __DISTUNIVSCHED_H__

#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/mac48-address.h"
#include "ns3/ipv4-routing-protocol.h"
#include "distUnivSchedStats.h"
#include "distUnivSchedQueue.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-address-generator.h"
#include "ns3/ipv4-address.h"

#define DIST_UNIV_SCHED_MAX_QUEUE_DIFF  50
#define DIST_UNIV_SCHED_MAX_RATE_DIFF  3

// DEBUG DEFINES
#define DIST_UNIV_SCHED_CONSTRUCTOR_DEBUG 0
#define DIST_UNIV_SCHED_INIT_DEBUG 0
#define DIST_UNIV_SCHED_START_TIME_SLOT_DEBUG 0
#define DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG 0
#define DIST_UNIV_SCHED_SEND_DATA_PACKET_DEBUG 0
#define DIST_UNIV_SCHED_RECV_DATA_PACKET_DEBUG 0
#define DIST_UNIV_SCHED_SEND_DATA_ACK_PACKET_DEBUG 0
#define DIST_UNIV_SCHED_RECV_DATA_ACK_PACKET_DEBUG 0
#define DIST_UNIV_SCHED_RECV_DIST_UNIV_SCHED_DEBUG 0
#define DIST_UNIV_SCHED_EXCHANGED_MATCHES_GLOBAL_DEBUG 0
#define DIST_UNIV_SCHED_ROUTE_OUTPUT_DEBUG 0
#define DIST_UNIV_SCHED_ROUTE_INPUT_DEBUG 0
#define DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG 0
#define DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG 0
#define DIST_UNIV_SCHED_PRINT_STATS_DEBUG 0
#define DIST_UNIV_SCHED_RATES_FROM_COORDS_DEBUG 0 
#define DIST_UNIV_SCHED_VARY_RATE_INFO_DEBUG 0
#define DIST_UNIV_SCHED_VARY_QUEUE_INFO_DEBUG 0

// Use this if recalculating radio ranges and channel rates
#define CALCULATE_RADIO_RANGES 0
#define FAKE_CHANNEL_RATE 1.0 

#define DUS_TRX_POWER_MW 40.0

#define MIN(a,b) ((a < b) ? a : b)
#define MAX(a,b) ((a > b) ? a : b)

namespace ns3 
{
namespace dus
{     

  enum MessageType
  {
    DUS_CTRL = 1,
    DUS_DATA = 2,
    DUS_DATA_ACK = 3,
    OTHER = 4
  };

  class TypeHeader : public Header
  {
  public:
    /// c-tor
    TypeHeader (MessageType t);

    ///\name Header serialization/deserialization
    //\{
    static TypeId GetTypeId ();
    TypeId GetInstanceTypeId () const;
    uint32_t GetSerializedSize () const;
    void Serialize (Buffer::Iterator start) const;
    uint32_t Deserialize (Buffer::Iterator start);
    void Print (std::ostream &os) const;
    //\}

    /// Return type
    MessageType Get () const { return m_type; }
    /// Check that type if valid
    bool IsValid () const { return m_valid; }
    bool operator== (TypeHeader const & o) const;
  private:
    MessageType m_type;
    bool m_valid;
  };


  struct DistUnivSchedSnrTag : public Tag
  {
    double snr;

    DistUnivSchedSnrTag (double s = -1.0) : Tag (), snr (s) {}

    static TypeId GetTypeId ()
    {
      static TypeId tid = TypeId ("ns3::dus::DistUnivSchedSnrTag").SetParent<Tag> ();
      return tid;
    }

    TypeId  GetInstanceTypeId () const 
    {
      return GetTypeId ();
    }

    uint32_t GetSerializedSize () const
    {
      return sizeof(double);
    }

    void  Serialize (TagBuffer i) const
    {
      i.WriteDouble (snr);
    }

    void  Deserialize (TagBuffer i)
    {
      snr = i.ReadDouble ();
    }

    void  Print (std::ostream &os) const
    {
      os << "DistUnivSchedSnrTag: SNR = " << snr;
    }
  };
  
  struct DistUnivSchedDestTag : public Tag
  {
    /// destination index of data packet used to index queues
    ///   comes from bottom 8 bits of IPv4 address - 1
    ///   assumes IP addresses start at *.*.*.1
    int dest;

    DistUnivSchedDestTag (int d = -1) : Tag (), dest (d) {}

    static TypeId GetTypeId ()
    {
      static TypeId tid = TypeId ("ns3::dus::DistUnivSchedDestTag").SetParent<Tag> ();
      return tid;
    }

    TypeId  GetInstanceTypeId () const 
    {
      return GetTypeId ();
    }

    uint32_t GetSerializedSize () const
    {
      return sizeof(int);
    }

    void  Serialize (TagBuffer i) const
    {
      i.WriteU32 (dest);
    }

    void  Deserialize (TagBuffer i)
    {
      dest = i.ReadU32 ();
    }

    void  Print (std::ostream &os) const
    {
      os << "DistUnivSchedDestTag: Destination Index = " << dest;
    }
  };


/* ... */


  /**
   * \ingroup distUnivSched
   * 
   * \brief DIST_UNIV_SCHED routing protocol
   */
  class RoutingProtocol : public Ipv4RoutingProtocol
  {
  public:
    DropTailQueue *queues; //[commodity];
    static TypeId GetTypeId (void);
    static const uint32_t DUS_PORT;
    UniformVariable trxDelayRV;
    
    // Constructor
    RoutingProtocol ();
    
    // Destructor
    virtual ~RoutingProtocol ();

    // pure virtual functions from Ipv4RoutingProtocol
    virtual Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);

    virtual bool RouteInput  (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb);

    virtual void NotifyInterfaceUp (uint32_t interface);

    virtual void NotifyInterfaceDown (uint32_t interface);

    virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);

    virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);
    
    virtual void SetIpv4 (Ptr<Ipv4> ipv4);

    virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream) const;

    Ptr<Ipv4Route> LoopbackRoute (const Ipv4Header & hdr, Ptr<NetDevice> oif) const;

    bool IsMyOwnAddress (Ipv4Address src);

    /// Find socket with local interface address iface
    Ptr<Socket> FindSocketWithInterfaceAddress (Ipv4InterfaceAddress iface) const;

    void DistUnivSchedInit( );

    virtual void StartTimeSlot( );

    virtual void CompleteTimeSlot();

    void CollectQueueLengths();

    void OutputChannelRates();

    virtual void GlobalExchangeControlInfoForward( Ptr<Packet> packet );
    
    virtual void GlobalExchangeControlInfoBackward( Ptr<Packet> packet );
    
    void SendControlInfoPacket( );

    void RecvControlInfoPacket( Ptr<Packet> );

    void SendDataPacket( int receivingNode, int commodity );

    virtual void RecvDataPacket( Ptr<Packet> packet, Ipv4Address senderAddress, Ipv4Address receiverAddress);

    void SendDataAckPacket( Ipv4Address senderAddress, Ipv4Address receiverAddress, int commodity );

    void RecvDataAckPacket( Ptr<Packet> packet, Ipv4Address senderAddress, Ipv4Address receiverAddress);

    void ExchangedInfoMatchesGlobal();

    void VaryingQBitsCreateControlPacket();

    void DataSent( Ptr<Socket> socket, uint32_t amtDataSent );
  
    /// Receive and process control packet
    void RecvPacket (Ptr<Socket> socket);

    bool IsPhyStateBusy();

    void GetChannelRateFromCoordinates( double node_1x, double node_1y, double node_2x, double node_2y, double *channelRate, int *channelRateIndex );

    void SetChannelRatesFromSnr( double snr, int senderIndex, int receiverIndex );

    virtual void PrintStats();

    void VaryQueues();

    void VaryRates();
      
    typedef enum {
      SEND_OWN_INFO,
      WAIT_FOR_ACK_OR_NEW_INFO,
      SEND_OWN_AND_FWD_NEW_INFO,
      JUST_FWD_NEW_INFO,
      DATA_TRX
    }DistUnivSchedControlInfoExchangeStates;

    // START GET FUNCTIONS
    
    bool isVARYING_Q_BITS () { return VARYING_Q_BITS; }
    bool isGLOBAL_KNOWLEDGE () { return GLOBAL_KNOWLEDGE; }
    bool isVARYING_FREQ () { return VARYING_FREQ; }
    bool isTIME_STATS () { return TIME_STATS; }
    bool isEND_RUN_STATS () { return END_RUN_STATS; }
    bool isOUTPUT_RATES () { return OUTPUT_RATES; } 
    bool isVARY_CORRECT_QUEUE_INFO () { return VARY_CORRECT_QUEUE_INFO; }
    bool isVARY_CORRECT_RATE_INFO () { return VARY_CORRECT_RATE_INFO; }
    bool isPRINT_QUEUE_DIFFS () { return PRINT_QUEUE_DIFFS; }
    bool isPRINT_RATE_DIFFS () { return PRINT_RATE_DIFFS; }
    bool isPRINT_CHOSEN_RATE_DIFFS () { return PRINT_CHOSEN_RATE_DIFFS; }
    
    Time getTimeSlotDuration () { return timeSlotDuration; }                                              
    Time getQueueDissemTotalTime () { return queueDissemTotalTime; }
    Time getControlPacketTrxTime () { return controlPacketTrxTime; }                  
    int getMaxBackoffWindow () { return maxBackoffWindow; }
    Time getWaitForAckTimeoutWindow () { return waitForAckTimeoutWindow; }               
    Time getControlInfoExchangeTime () { return controlInfoExchangeTime; }               
    Time getNextTimeSlotStartTime () { return nextTimeSlotStartTime; }                 
    Time getWaitForAckTimeoutTime () { return waitForAckTimeoutTime; }  
    Time getSimulationTime () { return simulationTime; }  
   

    std::string getDataFilePath () { return (std::string (dataFilePath)); }
   
    int getNodeId () { return nodeId; }
    int getV () { return V; }
    int getQBits () { return qBits; }
    int getSimTime () { return simTime; }
    int getNumNodes () { return numNodes; }
    int getNumRun () { return numRun; }
    int getNumCommodities () { return numCommodities; }
    int getTimeSlotNum () { return timeSlotNum; } // starts at 1
    int getNumTimeSlots () { return numTimeSlots; }
    int getNumPacketsFromApplication () { return numPacketsFromApplication; }
    int getNumPacketsFromApplicationThisSlot () { return numPacketsFromApplicationThisSlot; }
    int getSchedFreq () { return schedFreq; } // nodes will choose to schedule every schedFreq time slots
    int getNumControlPacketsSent () { return numControlPacketsSent; }
    int getNumControlPacketsRcvd () { return numControlPacketsRcvd; }
    int getNumDataAcksSent () { return numDataAcksSent; }
    int getNumDataAcksRcvd () { return numDataAcksRcvd; }
    int getNumCollisions () { return numCollisions; }
    int getNumCtrlPcktCollisions () { return numCtrlPcktCollisions; }
    int getNumDataPcktCollisions () { return numDataPcktCollisions; }
    int getNumDataAckCollisions () { return numDataAckCollisions; }
    int getNumCollisionsThisTimeSlot () { return numCollisionsThisTimeSlot; }
    int getNumWaitForAckTimeouts () { return numWaitForAckTimeouts; }
    double getNumBitsPerPacket () { return numBitsPerPacket; }
    double getBatteryPowerLevel () { return batteryPowerLevel; }
    double getInitBatteryPowerLevel () { return initBatteryPowerLevel; }
    double getNodeRWSpeed () { return nodeRWSpeed; }
    double getNodeRWPause () { return nodeRWPause; }
    Time getNodeLifetime () { return nodeLifetime; }
    double getLastSnr () { return lastSnr; }
    double getNumPacketsReachDest( int i ) { return stats->getNumPacketsReachDest(i); }
    double getNumPacketsReachDestThisSecond( int i ) { return stats->getNumPacketsReachDestThisSecond(i); }
    double getQueueLengthSum( int i ) { return stats->getQueueLengthSum(i); }
    
    
    bool isStatsPrinted () { return statsPrinted; }

    int getOtherBacklogs (int i, int j) { return otherBacklogs[i][j]; } //[node][commodity];
    int getGlobalBacklogs (int i, int j) { return globalBacklogs[i][j]; } //[node][commodity];
    int getPreviousQueues (int i) { return previousQueues[i]; } // [commodity]
    //    Message ***messageBuffers; //[MAX_NUM_COMMODITIES][MAX_DIST_UNIV_SCHED_BUFFER_SIZE];
    //    Message **backoffBuffer; // [MAX_BACKOFF_BUFFER_SIZE]
    double getRates (int i) { return rates[i]; } //[node(dest)];
    double getGlobalRates (int i) { return globalRates[i]; } // [node(dest)]
    double getGlobalChosenRates (int i) { return globalChosenRates[i]; } // [node(dest)]
    Time getTimeStamps ( int i, int j ) { return timeStamps[i][j]; }  // [node][node]
    Time getTempTimeStamps ( int i, int j ) { return tempTimeStamps[i][j]; }  // [node][node]
    Time getNextTimeStamps ( int i, int j ) { return nextTimeStamps[i][j]; }  // [node][node]
    bool isForwardNodeCtrlInfo (int i) { return forwardNodeCtrlInfo[i]; } // [node]
    bool isNodeInfoFirstRcv (int i) { return nodeInfoFirstRcv[i]; } // [node]
    bool isRcvdAckThisTimeSlot () { return rcvdAckThisTimeSlot; }
    bool isTrxThisTimeSlot () { return trxThisTimeSlot; } 
    bool isGlobalTrxThisTimeSlot () { return globalTrxThisTimeSlot; }
    Time getBackoffEndTime() { return backoffEndTime; }
    int getNumBackoff () { return numBackoff; }
    double getArrivalRates (int i) { return arrivalRates[i]; } // [session]
    int getDiffBacklogs (int i, int j, int k) { return diffBacklogs[i][j][k]; } // [node][node][commodity]
    int getWeights (int i, int j) { return weights[i][j]; } // [node][node]
    int getWeightCommodity (int i, int j) { return weightCommodity[i][j]; } // [node][node]
    bool isPossTrx (int i, int j) { return possTrx[i][j]; } //[MAX_NUM_NODES][MAX_NUM_NODES];
    double getNumTrxPoss () { return numTrxPoss; }

    double getNodeCoordX ( int i ) { return nodeCoordX[i]; } //
    double getNodeCoordY ( int i ) { return nodeCoordY[i]; } //
    double getNodeDistance (int i) { return nodeDistance[i]; } //[node]
    double getChannelRates (int i, int j) { return channelRates[i][j]; } // [node][node]
    double getGlobalChannelRates (int i, int j) { return globalChannelRates[i][j]; } // [node][node] // these are used to compare the exchanged control info to the actual control info
    int getChannelRateIndex (int i) { return channelRateIndex[i]; } // [node]
    double getTempChannelRates (int i, int j, int k) { return tempChannelRates[i][j][k]; } // [node][node]
    double getInputRate () { return inputRate; } // total input rate at this node in packets per second...calculated only using cbr interval(1/interval)
//    int lastChosenResAllocScheme;
    int getPacketsRcvThisTimeSlot (int i) { return packetsRcvThisTimeSlot[i]; } // [commodity]
    int getPacketsTrxThisTimeSlot (int i) { return packetsTrxThisTimeSlot[i]; } // [commodity]
    int getDataPacketsSent (int i) { return dataPacketsSent[i]; } // [node]
    int getDataPacketsRcvd (int i) { return dataPacketsRcvd[i]; } // [node]
    int getDataPacketsRcvdThisSlot (int i) { return dataPacketsRcvdThisSlot[i]; } // [node]
    bool isTimeSlotMessageSent () { return timeSlotMessageSent; }
    DistUnivSchedControlInfoExchangeStates getControlInfoExchangeState () { return controlInfoExchangeState; }    
    
    // info about #'s of control packets being exchanged
    int getNumControlPacketsInBuffer () { return numControlPacketsInBuffer; }
    int getControlPacketsPerSlot () { return controlPacketsPerSlot; }
    int getControlPacketsSentThisTimeSlot () { return controlPacketsSentThisTimeSlot; }
    int getControlPacketsRcvdThisTimeSlot () { return controlPacketsRcvdThisTimeSlot; }
    int getPacketsDropped () { return packetsDropped; }

    Ptr<Ipv4Route> getOutputRoutePointer() { return &outputRoute; }
    
    // for keeping track of differences between distributed control info and global info
//    int numIncorrectQueues; // 
//    int numIncorrectRates;
//    int amountQueuesWrong[DIST_UNIV_SCHED_MAX_QUEUE_DIFF];
//    int amountRatesWrong[DIST_UNIV_SCHED_MAX_RATE_DIFF];
//    int chosenRatesWrong[DIST_UNIV_SCHED_MAX_RATE_DIFF];
    
    // END GET FUNCTIONS

    // START SET FUNCTIONS
    
    void setVARYING_Q_BITS ( bool newVal ) {  VARYING_Q_BITS = newVal; }
    void setGLOBAL_KNOWLEDGE ( bool newVal ) {  GLOBAL_KNOWLEDGE = newVal; }
    void setVARYING_FREQ ( bool newVal ) {  VARYING_FREQ = newVal; }
    void setTIME_STATS ( bool newVal ) {  TIME_STATS = newVal; }
    void setEND_RUN_STATS ( bool newVal ) {  END_RUN_STATS = newVal; }
    void setOUTPUT_RATES ( bool newVal ) {  OUTPUT_RATES = newVal; } 
    void setVARY_CORRECT_QUEUE_INFO ( bool newVal ) {  VARY_CORRECT_QUEUE_INFO = newVal; }
    void setVARY_CORRECT_RATE_INFO ( bool newVal ) {  VARY_CORRECT_RATE_INFO = newVal; }
    void setPRINT_QUEUE_DIFFS ( bool newVal ) {  PRINT_QUEUE_DIFFS = newVal; }
    void setPRINT_RATE_DIFFS ( bool newVal ) {  PRINT_RATE_DIFFS = newVal; }
    void setPRINT_CHOSEN_RATE_DIFFS ( bool newVal ) {  PRINT_CHOSEN_RATE_DIFFS = newVal; }
    
    void setTimeSlotDuration ( Time newVal ) { timeSlotDuration = timeSlotDuration; }
    void setQueueDissemTotalTime ( Time newVal  ) { queueDissemTotalTime = newVal; }
    void setControlPacketTrxTime  ( Time newVal  ) { controlPacketTrxTime = newVal; } 
    void setMaxBackoffWindow ( int newVal  ) { maxBackoffWindow = newVal; }
    void setWaitForAckTimeoutWindow ( Time newVal  ) { waitForAckTimeoutWindow = newVal; } 
    void setControlInfoExchangeTime ( Time newVal  ) { controlInfoExchangeTime = newVal; } 
    void setNextTimeSlotStartTime ( Time newVal  ) { nextTimeSlotStartTime = newVal; } 
    void setWaitForAckTimeoutTime ( Time newVal  ) { waitForAckTimeoutTime = newVal; }
    void setSimulationTime ( Time newVal  ) { simulationTime = newVal; }
    
    void setDataFilePath (std::string path) { strcpy( dataFilePath, (char *)path.c_str() ); }
   
    void setV ( int newVal ) { V = newVal; }
    void setQBits ( int newVal ) { qBits = newVal; }
    void setSimTime ( int newVal ) { simTime = newVal; }
    void setNumNodes ( int newVal ) { numNodes = newVal; }
    void setNumRun ( int newVal ) { numRun = newVal; }
    void setNumCommodities ( int newVal ) { numCommodities = newVal; }
    void setTimeSlotNum ( int newVal ) { timeSlotNum = newVal; } // starts at 1
    void setNumTimeSlots ( int newVal ) { numTimeSlots = newVal; }
    void setNumPacketsFromApplication ( int newVal ) { numPacketsFromApplication = newVal; }
    void setNumPacketsFromApplicationThisSlot ( int newVal ) { numPacketsFromApplicationThisSlot = newVal; }
    void setSchedFreq ( int newVal ) { schedFreq = newVal; } // nodes will choose to schedule every schedFreq time slots
    void setNumControlPacketsSent ( int newVal ) { numControlPacketsSent = newVal; }
    void setNumControlPacketsRcvd ( int newVal ) { numControlPacketsRcvd = newVal; }
    void setNumDataAcksSent ( int newVal ) { numDataAcksSent = newVal; }
    void setNumDataAcksRcvd ( int newVal ) { numDataAcksRcvd = newVal; }
    void setNumCollisions ( int newVal ) { numCollisions = newVal; }
    void setNumCtrlPcktCollisions ( int newVal ) { numCtrlPcktCollisions = newVal; }
    void setNumDataPcktCollisions ( int newVal ) { numDataPcktCollisions = newVal; }
    void setNumDataAckCollisions ( int newVal ) { numDataAckCollisions = newVal; }
    void setNumCollisionsThisTimeSlot ( int newVal ) { numCollisionsThisTimeSlot = newVal; }
    void setNumWaitForAckTimeouts ( int newVal ) { numWaitForAckTimeouts = newVal; }
    void setNumBitsPerPacket ( double newVal ) { numBitsPerPacket = newVal; }
    void setBatteryPowerLevel ( double newVal ) { batteryPowerLevel = newVal; }
    void setInitBatteryPowerLevel ( double newVal ) { initBatteryPowerLevel = newVal; }
    void setNodeRWSpeed ( double newVal ) { nodeRWSpeed = newVal; }
    void setNodeRWPause ( double newVal ) { nodeRWPause = newVal; }
    void setNodeLifetime ( Time newVal ) { nodeLifetime = newVal; }
    void setLastSnr ( double newVal ) { lastSnr = newVal; }
    void setNumPacketsReachDest ( int i, double newVal ) { stats->setNumPacketsReachDest( i, newVal ); }
    void setNumPacketsReachDestThisSecond ( int i, double newVal ) { stats->setNumPacketsReachDestThisSecond( i, newVal ); }
    
    void setStatsPrinted ( bool newVal ) { statsPrinted = newVal; }
    void setForwardNodeCtrlInfo (int i, bool newVal ) { forwardNodeCtrlInfo[i] = newVal; } // [node]
    void setNodeInfoFirstRcv (int i, bool newVal ) { nodeInfoFirstRcv[i] = newVal; } // [node]
    void setRcvdAckThisTimeSlot ( bool newVal ) { rcvdAckThisTimeSlot = newVal; }
    void setTrxThisTimeSlot ( bool newVal ) { trxThisTimeSlot = newVal; } 
    void setGlobalTrxThisTimeSlot ( bool newVal ) { globalTrxThisTimeSlot = newVal; }
    void setTimeSlotMessageSent ( bool newVal ) { timeSlotMessageSent = newVal; }
    void setPossTrx (int i, int j, bool newVal ) { possTrx[i][j] = newVal; } //[MAX_NUM_NODES][MAX_NUM_NODES] = newVal;

    void setOtherBacklogs ( int i, int j, int newVal ) {  otherBacklogs[i][j] = newVal; } //[node][commodity] = newVal;
    void setGlobalBacklogs ( int i, int j, int newVal ) {  globalBacklogs[i][j] = newVal; } //[node][commodity] = newVal;
    void setPreviousQueues ( int i, int newVal ) {  previousQueues[i] = newVal; } // [commodity]
    void setNumBackoff ( int newVal ) {numBackoff = newVal; }
    void setDiffBacklogs ( int i, int j, int k, int newVal ) {  diffBacklogs[i][j][k] = newVal; } // [node][node][commodity]
    void setWeights ( int i, int j, int newVal ) {  weights[i][j] = newVal; } // [node][node]
    void setWeightCommodity ( int i, int j, int newVal ) {  weightCommodity[i][j] = newVal; } // [node][node]
    void setChannelRateIndex ( int i, int newVal ) {  channelRateIndex[i] = newVal; } // [node]
    void setPacketsRcvThisTimeSlot ( int i, int newVal ) {  packetsRcvThisTimeSlot[i] = newVal; } // [commodity]
    void setPacketsTrxThisTimeSlot ( int i, int newVal ) {  packetsTrxThisTimeSlot[i] = newVal; } // [commodity]
    void setDataPacketsSent ( int i, int newVal ) {  dataPacketsSent[i] = newVal; } // [node]
    void setDataPacketsRcvd ( int i, int newVal ) {  dataPacketsRcvd[i] = newVal; } // [node]
    void setDataPacketsRcvdThisSlot ( int i, int newVal ) {  dataPacketsRcvdThisSlot[i] = newVal; } // [node]
    // info about #'s of control packets being exchanged
    void setNumControlPacketsInBuffer ( int newVal ) { numControlPacketsInBuffer = newVal; }
    void setControlPacketsPerSlot ( int newVal ) { controlPacketsPerSlot = newVal; }
    void setControlPacketsSentThisTimeSlot ( int newVal ) { controlPacketsSentThisTimeSlot = newVal; }
    void setControlPacketsRcvdThisTimeSlot ( int newVal ) { controlPacketsRcvdThisTimeSlot = newVal; }
    void setPacketsDropped ( int newVal ) { packetsDropped = newVal; }
    //    Message ***messageBuffers; //[MAX_NUM_COMMODITIES][MAX_DIST_UNIV_SCHED_BUFFER_SIZE];
    //    Message **backoffBuffer; // [MAX_BACKOFF_BUFFER_SIZE]
    void setRates ( int i, double newVal ) {  rates[i] = newVal; } //[node(dest)] = newVal;
    void setGlobalRates ( int i, double newVal ) {  globalRates[i] = newVal; } // [node(dest)]
    void setGlobalChosenRates ( int i, double newVal ) {  globalChosenRates[i] = newVal; } // [node(dest)]
    void setNumTrxPoss ( double newVal ) {  numTrxPoss = newVal; }
    void setArrivalRates ( int i, double newVal ) {  arrivalRates[i] = newVal; } // [session]
    void setNodeCoordX ( int i, double newVal ) {  nodeCoordX[i] = newVal; } //
    void setNodeCoordY ( int i, double newVal ) {  nodeCoordY[i] = newVal; } //
    void setNodeDistance ( int i, double newVal ) {  nodeDistance[i] = newVal; } //[node]
    void setChannelRates ( int i, int j, double newVal ) {  channelRates[i][j] = newVal; } // [node][node]
    void setTempChannelRates ( int i, int j, int k, double newVal ) {  tempChannelRates[i][j][k] = newVal; } // [node][node]
    void setInputRate ( double newVal ) {  inputRate = newVal; } // total input rate at this node in packets per second...calculated only using cbr interval(1/interval)
    void setGlobalChannelRates ( int i, int j, double newVal ) {  globalChannelRates[i][j] = newVal; } // [node][node] // these are used to compare the exchanged control info to the actual control info

    void setTimeStamps ( int i, int j, Time newVal ) { timeStamps[i][j] = newVal; } // [node][node]
    void setTempTimeStamps ( int i, int j, Time newVal ) { tempTimeStamps[i][j] = newVal; }  // [node][node]
    void setNextTimeStamps ( int i, int j, Time newVal ) { nextTimeStamps[i][j] = newVal; }  // [node][node]

    void setBackoffEndTime ( Time newVal ) { backoffEndTime = newVal; }

//    int lastChosenResAllocScheme;
    void setControlInfoExchangeState ( DistUnivSchedControlInfoExchangeStates newVal ) { controlInfoExchangeState = newVal; }    
    
    
    // for keeping track of differences between distributed control info and global info
//    int numIncorrectQueues; // 
//    int numIncorrectRates;
//    int amountQueuesWrong[DIST_UNIV_SCHED_MAX_QUEUE_DIFF];
//    int amountRatesWrong[DIST_UNIV_SCHED_MAX_RATE_DIFF];
//    int chosenRatesWrong[DIST_UNIV_SCHED_MAX_RATE_DIFF];
    
    // END SET FUNCTIONS
    


    /// IP Protocol
    Ptr<Ipv4> m_ipv4;  
    /// Raw socket per each IP interface, map socket -> iface address (IP + mask)  
    std::map< Ptr<Socket>, Ipv4InterfaceAddress > m_socketAddresses;
    /// Loopback device used to defer RREQ until packet will be fully formed
    Ptr<NetDevice> m_lo;
  private:
  
    // PROGRAM OPTIONS
    
    bool VARYING_Q_BITS; //#define VARYING_Q_BITS  // Compiling with this option and setting Q_BITS to the max (13 when buffer size is 8192)
    //   is standard distributed universal scheduling algorithm
    bool GLOBAL_KNOWLEDGE;  //#define GLOBAL_KNOWLEDGE  // Global knowledge cannot be compiled with varying_q_bits or adaptive_threholds
    bool VARYING_FREQ;  //#define VARYING_FREQ
    bool TIME_STATS;  //#define DIST_UNIV_SCHED_TIME_STATS
    bool END_RUN_STATS;  //#define END_RUN_STATS // only record statistics (throughput and avg total occupancy) for the last 25% of the simulation time
    //		allows observation of steady state operation
    bool OUTPUT_RATES;  //#define OUTPUT_RATES // make output files (one for each node) that record the available channel rates for each time slo
    bool VARY_CORRECT_QUEUE_INFO; // if true, global knowledge should also be true...this option exchanges global info and artificially (and randomly) modifies
    //		values to see effects of incorrect information on scheduling decisions
    bool VARY_CORRECT_RATE_INFO; // if true, global knowledge should also be true...this option exchanges global info and artificially (and randomly) modifies
    //		values to see effects of incorrect information on scheduling decisions
    double probRateChange;
    double maxRateChange;
    double probQueueChange;
    double maxQueueChange;
    bool PRINT_QUEUE_DIFFS; // if true, nodes will print the array with #'s of time each backlog value was diff from global by that amount
    bool PRINT_RATE_DIFFS; // if true, nodes will print the array with #'s of time each rate value was diff from global by that amount
    bool PRINT_CHOSEN_RATE_DIFFS; // if true, nodes will print the array with #'s of time each rate chosen by dist. version was diff from global version by that amount

    FILE *channelRatesFd;
    
    
    Time       timeSlotDuration;                                              
    Time       queueDissemTotalTime;
    Time       controlPacketTrxTime;                  
    int        maxBackoffWindow;
    Time       waitForAckTimeoutWindow;               
    Time       controlInfoExchangeTime;               
    Time       nextTimeSlotStartTime;                 
    Time       waitForAckTimeoutTime;  
    Time       simulationTime;

    char dataFilePath[1024];
    
    DistUnivSchedStats *stats;

  
    int  nodeId;  
    static int nodeIdAssign;
    int 	V;
    int	qBits;
    int simTime;
    int numNodes; 
    int numRun; 
    int numCommodities;
    int timeSlotNum; // starts at 1
    int numTimeSlots;
    int numPacketsFromApplication;
    int numPacketsFromApplicationThisSlot;
    int schedFreq; // nodes will choose to schedule every schedFreq time slots
    int numControlPacketsSent;
    int numControlPacketsRcvd;
    int numDataAcksSent;
    int numDataAcksRcvd;
    int numCollisions;
    int numCtrlPcktCollisions;
    int numDataPcktCollisions;
    int numDataAckCollisions;
    int numCollisionsThisTimeSlot;
    int numWaitForAckTimeouts;
    double numBitsPerPacket;
    double lastSnr;
    double batteryPowerLevel;
    double initBatteryPowerLevel;
    double nodeRWSpeed;
    double nodeRWPause;
    Time nodeLifetime;
    
    bool statsPrinted;
    bool processHello;
    bool processAck;
    int **otherBacklogs; //[node][commodity];
    int **globalBacklogs; //[node][commodity];
    long unsigned **packedBacklogs; //[node][commodity]
    int *previousQueues; // [commodity]
    double *virtQueueH; //[session???](currently) or [commodity?]
    double *auxVar; //[NUM_COMMODITIES]
//    Message ***messageBuffers; //[MAX_NUM_COMMODITIES][MAX_DIST_UNIV_SCHED_BUFFER_SIZE];
//    Message **backoffBuffer; // [MAX_BACKOFF_BUFFER_SIZE]
    double *rates; //[node(dest)];
    double *globalRates; // [node(dest)]
    double *globalChosenRates; // [node(dest)]
    Time **timeStamps; // [node][node]
    Time **tempTimeStamps; // [node][node]
    Time **nextTimeStamps; // [node][node]
    bool *forwardNodeCtrlInfo; // [node]
    bool *nodeInfoFirstRcv; // [node]
    bool rcvdAckThisTimeSlot;
    bool trxThisTimeSlot; 
    bool globalTrxThisTimeSlot;
    Time backoffEndTime;
    int numBackoff;
    double *arrivalRates; // [session]
    int ***diffBacklogs; // [node][node][commodity]
    int **weights; // [node][node]
    int **weightCommodity; // [node][node]
    bool **possTrx; //[MAX_NUM_NODES][MAX_NUM_NODES];
    double numTrxPoss;
    double horizCellSize, vertCellSize;
    double *nodeCoordX; // [node]
    double *nodeCoordY; // [node]
    double *nodeDistance; //[node]
    double **channelRates; // [node][node]
    double **globalChannelRates; // [node][node] // these are used to compare the exchanged control info to the actual control info
    int *channelRateIndex; // [node]
    double ***tempChannelRates; // [node][node]
    double inputRate; // total input rate at this node in packets per second...calculated only using cbr interval(1/interval)

    int lastChosenResAllocScheme; 
    int *packetsRcvThisTimeSlot; // [commodity] (data packets, used to update queue[].m_backlog at end of each slot)
    int *packetsTrxThisTimeSlot; // [commodity] (data packets, "")
    int *dataPacketsSent; // [node]
    int *dataPacketsRcvd; // [node]
    int *dataPacketsRcvdThisSlot; // [node]
    bool timeSlotMessageSent;
    DistUnivSchedControlInfoExchangeStates controlInfoExchangeState;
    
    
    // info about #'s of control packets being exchanged
    int numControlPacketsInBuffer;
    int controlPacketsPerSlot;
    int controlPacketsSentThisTimeSlot; 
    int controlPacketsRcvdThisTimeSlot;
    int packetsDropped;
    
    // for keeping track of differences between distributed control info and global info
    int numIncorrectQueues; // 
    int numIncorrectRates;
    int amountQueuesWrong[DIST_UNIV_SCHED_MAX_QUEUE_DIFF];
    int amountRatesWrong[DIST_UNIV_SCHED_MAX_RATE_DIFF];
    int chosenRatesWrong[DIST_UNIV_SCHED_MAX_RATE_DIFF];

    Ipv4Route outputRoute;
    
  };

  struct DistUnivSchedPacketTypeTag : public Tag
  {
    /// destination index of data packet used to index queues
    ///   comes from bottom 8 bits of IPv4 address - 1
    ///   assumes IP addresses start at *.*.*.1
    MessageType type;

    DistUnivSchedPacketTypeTag (MessageType t = OTHER) : Tag (), type (t) {}

    static TypeId GetTypeId ()
    {
      static TypeId tid = TypeId ("ns3::dus::DistUnivSchedPacketTypeTag").SetParent<Tag> ();
      return tid;
    }

    TypeId  GetInstanceTypeId () const 
    {
      return GetTypeId ();
    }

    uint32_t GetSerializedSize () const
    {
      return sizeof(MessageType);
    }

    void  Serialize (TagBuffer i) const
    {
      i.WriteU32 (type);
    }

    void  Deserialize (TagBuffer i)
    {
      type = (MessageType)i.ReadU32 ();
    }

    void  Print (std::ostream &os) const
    {
      os << "DistUnivSchedPacketTypeTag: Type of Packet = ";
      switch( type )
      {
        case DUS_CTRL:
          {
            os << "DUS_CTRL\n";
            break;
          }
        case DUS_DATA:
          {
            os << "DUS_DATA\n";
            break;
          }
        case DUS_DATA_ACK:
          {
            os << "DUS_DATA_ACK\n";
            break;
          }
        case OTHER:
          {
            os << "OTHER\n";
            break;
          }
        default:
          {
            os << "Unknown\n";
          }
      }
    }
  };
}
}  
#endif /* __DISTUNIVSCHED_H__ */

