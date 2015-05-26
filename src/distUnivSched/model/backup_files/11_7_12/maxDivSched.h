/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef __MAX_DIV_SCHED_H__
#define __MAX_DIV_SCHED_H__

#include <time.h>
#include "ns3/dus-routing-protocol.h"
#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/mac48-address.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-address-generator.h"
#include "ns3/ipv4-address.h"

// DEBUG DEFINES
#define MAX_DIV_SCHED_CONSTRUCTOR_DEBUG 0
#define MAX_DIV_SCHED_INIT_DEBUG 0
#define MAX_DIV_SCHED_RECV_PACKET_DEBUG 0
#define MAX_DIV_SCHED_ROUTE_OUTPUT_DEBUG 0
#define MAX_DIV_SCHED_ROUTE_INPUT_DEBUG 0
#define MAX_DIV_SCHED_START_TIME_SLOT_DEBUG 0
#define MAX_DIV_SCHED_COMPLETE_TIME_SLOT_DEBUG 0
#define MAX_DIV_SCHED_SEND_PACKETS_DEBUG 0
#define MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG 0
#define MAX_DIV_SCHED_APPROX_ONE_SHOT_DEBUG 0
#define MAX_DIV_SCHED_APPROX_MAX_RATIO_DEBUG 0
#define MAX_DIV_SCHED_SEND_DATA_PACKET_DEBUG 0
#define MAX_DIV_SCHED_RECV_DATA_PACKET_DEBUG 0
#define MAX_DIV_SCHED_RECV_DATA_ACK_PACKET_DEBUG 0
#define MAX_DIV_SCHED_GENERATE_DUP_PACKETS_DEBUG 0
#define MAX_DIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG 0
#define MAX_DIV_SCHED_PRINT_STATS_DEBUG 0

// other definitions
#define MAX_DIV_SCHED_MAX_QUEUE_SIZE 32

#define MDS_TRX_POWER_MW 40.0
#define COST_FUNCTION(x) (x)

namespace ns3
{
namespace dus
{
/* ... */
  struct AreaCoverageInfo
  {
    int x;
    int y;
    int xLength;
    int yLength;
    int size;
    bool check;
  };
  
  // This is a tag that provides metadata for an application packet.
  //  It specifies what 2-D area is covered by the information in
  //  the packet.
  // The information provided are four integers that describe a 
  //  rectangle in some 2-D grid area.  X and Y describe the location
  //  of the "bottom left" corner of the coverage, and the xLength 
  //  and yLength provide the length of the horizontal and vertical 
  //  sides of the rectangle, respectively.
  struct AreaCoverageTag : public Tag
  {
    int x, y, xLength, yLength;
    int size;

    AreaCoverageTag (int newX = -1, int newY = -1, int newXLength = 0, int newYLength = 0, int newPacketSize = 0.0) : Tag (), x (newX), y (newY), xLength (newXLength), yLength( newYLength), size(newPacketSize) {}

    static TypeId GetTypeId ()
    {
      static TypeId tid = TypeId ("ns3::dus::AreaCoverageTag").SetParent<Tag> ();
      return tid;
    }

    TypeId  GetInstanceTypeId () const 
    {
      return GetTypeId ();
    }

    uint32_t GetSerializedSize () const
    {
      return 5*sizeof(int);
    }

    void  Serialize (TagBuffer i) const
    {
      i.WriteU32 (x);
      i.WriteU32 (y);
      i.WriteU32 (xLength);
      i.WriteU32 (yLength);
      i.WriteU32 (size);
    }

    void  Deserialize (TagBuffer i)
    {
      x = i.ReadU32 ();
      y = i.ReadU32 ();
      xLength = i.ReadU32 ();
      yLength = i.ReadU32 ();
      size = i.ReadU32 ();
    }

    void  Print (std::ostream &os) const
    {
      os << "AreaCoverageTag: [x,y,xLength,yLength] = [" << x << "," << y << "," << xLength << "," << yLength << "]\n";
    }
  };


  class MaxDivSched : public RoutingProtocol
  {
    public:
      static TypeId GetTypeId (void);

      // Constructor
      MaxDivSched();
      // Destructor
      virtual ~MaxDivSched();

      void MaxDivSchedInit();

      virtual void StartTimeSlot();

      // pure virtual functions from Ipv4RoutingProtocol
      virtual Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);

      virtual bool RouteInput  (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb);

      virtual void NotifyInterfaceUp (uint32_t interface);

      virtual void NotifyInterfaceDown (uint32_t interface);

      virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);

      virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);

      virtual void SetIpv4 (Ptr<Ipv4> ipv4);
    
      /// Receive and process control packet
      void RecvPacket (Ptr<Socket> socket);
    
      void RecvDataPacket( Ptr<Packet> packet, Ipv4Address senderAddress, Ipv4Address receiverAddress);

      void RecvDataAckPacket( Ptr<Packet> packet, Ipv4Address senderAddress, Ipv4Address receiverAddress);

      virtual void CompleteTimeSlot();

      void SendPackets( int maxDivDataSetNum );

      uint64_t FindOneShotMaxDivSet();
      
      int FindApproxOneShotMaxDivSet();
      
      uint64_t FindLongTermAvgMaxDivSet();

      int FindApproxMaxRatioMaxDivSet();
      
      int FindApproxVirtQueueRatioMaxDivSet();

      int FindApproxGreedyVQMaxDivSet();
      
      uint64_t FindRandomMaxDivSet();
    
      virtual void GlobalExchangeControlInfoForward( Ptr<Packet> packet );
    
      virtual void GlobalExchangeControlInfoBackward( Ptr<Packet> packet );

      void GenerateDuplicatePackets();

      void SendDataPacket( int receivingNode, int commodity, Ptr<Packet> packetToSend ); //int dataItemIndex );
    
      void GenerateRandomAreaCoverageInfo( int *acx, int *acy, int *acxl, int *acyl, int *acs );
      
      virtual void PrintStats();

      void setTotalNumDataItems( int newVal ) { totalNumDataItems = newVal; }
      void setNumDataItemsInQueue( int i, int newVal ) { numDataItemsInQueue[i] = newVal; }
      void setAreaWidth( int newVal ) { areaWidth = newVal; }
      void setAreaLength( int newVal ) { areaLength = newVal; }
      void setMinItemCovDist( int newVal ) { minItemCovDist = newVal; } 
      void setMaxItemCovDist( int newVal ) { maxItemCovDist = newVal; } 
      void setFixedPacketLength( int newVal ) { fixedPacketLength = newVal; } 
      void setNodePossCovDist( int newVal ) { nodePossCovDist = newVal; } 
      void setNodePossCovX( int newVal ) { nodePossCovX = newVal; } 
      void setNodePossCovY( int newVal ) { nodePossCovY = newVal; } 
      void setHQx ( double newVal ) { HQx = newVal; }
      void setHQy ( double newVal ) { HQy = newVal; }
      void setNodePowerBudget( double newVal ) { nodePowerBudget = newVal; }
      void setAvgPowerBudget( double newVal ) { avgPowerBudget = newVal; }
      void setV( int newVal ) { V = newVal; }
      void setTimeBudget( Time newVal ) { timeBudget = newVal; }
      void setNumTimesExceededPowerBudget( int i, int newVal ) { numTimesExceededPowerBudget[i] = newVal; }
      void setNumTimesExceededTimeBudget( int newVal ) { numTimesExceededTimeBudget = newVal; }
      void setNumBytesReachDestination( int newVal ) { numBytesReachDestination = newVal; }
      void setPowerUsed( double newVal ) { powerUsed = newVal; }
      void setClearQueuesEachSlot( bool newVal ) { clearQueuesEachSlot = newVal; }
      void setFixedPacketSize( bool newVal ) { fixedPacketSize = newVal; }
      void setGeneratedDuplicatePacketsThisSlot( bool newVal ) { generatedDuplicatePacketsThisSlot = newVal; }
      void setLongTermAvgProblem( bool newVal ) { longTermAvgProblem = newVal; }
      void setOneShotProblem( bool newVal ) { oneShotProblem = newVal; }
      void setRandomChoiceProblem( bool newVal ) { randomChoiceProblem = newVal; }
      void setApproxOneShotProblem( bool newVal ) { approxOneShotProblem = newVal; }
      void setApproxMaxRatioProblem( bool newVal ) { approxMaxRatioProblem = newVal; }
      void setApproxVirtQueueRatioProblem( bool newVal ) { approxVirtQueueRatioProblem = newVal; }
      void setApproxGreedyVQProblem( bool newVal ) { approxGreedyVQProblem = newVal; }
      void setOutputPowerVsTime( bool newVal ) { outputPowerVsTime = newVal; }
      void setSumOverlapOfChosenSets( double newVal ) { sumOverlapOfChosenSets = newVal; }
      void setSumOverlapOfMaxSets( double newVal ) { sumOverlapOfMaxSets = newVal; }
      void setSumCoverageOfChosenSets( double newVal ) { sumCoverageOfChosenSets = newVal; }
      void setSumCoverageOfMaxSets( double newVal ) { sumCoverageOfMaxSets = newVal; }
      void setPacketDataRate( double newVal ) { packetDataRate = newVal; }
      void setDuplicatePacketDataRate( int newVal ) { duplicatePacketDataRate = newVal; }
      void setSumDuplicateBytes( double newVal ) { sumDuplicateBytes = newVal; }
      void setSumChannelRate( double newVal  ) { sumChannelRate = newVal; }
      void setNumSetsSkipped( int newVal  ) { numSetsSkipped = newVal; }

      int getTotalNumDataItems( ) { return totalNumDataItems; }
      int getNumDataItemsInQueue( int i ) { return numDataItemsInQueue[i]; }
      int getAreaWidth( ) { return areaWidth; }
      int getAreaLength( ) { return areaLength; }
      int getMinItemCovDist( ) { return minItemCovDist; } 
      int getMaxItemCovDist( ) { return maxItemCovDist; } 
      int getFixedPacketLength( ) { return fixedPacketLength; } 
      int getNodePossCovDist( ) { return nodePossCovDist; } 
      int getNodePossCovX( ) { return nodePossCovX; } 
      int getNodePossCovY( ) { return nodePossCovY; } 
      double getHQx () { return HQx; }
      double getHQy () { return HQy; }
      double getNodePowerBudget( ) { return nodePowerBudget; }
      double getAvgPowerBudget( ) { return avgPowerBudget; }
      int getV( ) { return V; }
      Time getTimeBudget( ) { return timeBudget; }
      int getNumTimesExceededPowerBudget( int i ) { return numTimesExceededPowerBudget[i]; }
      int getNumTimesExceededTimeBudget( ) { return numTimesExceededTimeBudget; }
      int getNumBytesReachDestination( ) { return numBytesReachDestination; }
      double getPowerUsed( ) { return powerUsed; }
      bool getClearQueuesEachSlot( ) { return clearQueuesEachSlot; }
      bool getFixedPacketSize( ) { return fixedPacketSize; }
      bool getGeneratedDuplicatePacketsThisSlot( ) { return generatedDuplicatePacketsThisSlot; }
      bool getLongTermAvgProblem( ) { return longTermAvgProblem; }
      bool getOneShotProblem( ) { return oneShotProblem; }
      bool getRandomChoiceProblem( ) { return randomChoiceProblem; }
      bool getApproxOneShotProblem( ) { return approxOneShotProblem; }
      bool getApproxMaxRatioProblem( ) { return approxMaxRatioProblem; }
      bool getApproxVirtQueueRatioProblem( ) { return approxVirtQueueRatioProblem; }
      bool getApproxGreedyVQProblem( ) { return approxGreedyVQProblem; }
      bool getOutputPowerVsTime( ) { return outputPowerVsTime; }
      double getSumOverlapOfChosenSets( ) { return sumOverlapOfChosenSets; }
      double getSumOverlapOfMaxSets( ) { return sumOverlapOfMaxSets; }
      double getSumCoverageOfChosenSets( ) { return sumCoverageOfChosenSets; }
      double getSumCoverageOfMaxSets( ) { return sumCoverageOfMaxSets; }
      double getPacketDataRate( ) { return packetDataRate; }
      int getDuplicatePacketDataRate( ) { return duplicatePacketDataRate; }
      double getSumDuplicateBytes( ) { return sumDuplicateBytes; }
      double getSumChannelRate( ) { return sumChannelRate; }
      int getNumSetsSkipped( ) { return numSetsSkipped; }

    private:

      double **locationCoverage; // location coverage array
      double **HQCumulativeCoverage; // [area width][area length] only at HQ
      //double ***HQCoverageEachSlot; // [area width][area length][time slot]
      double *virtPowerQueue; // [node]
      int totalNumDataItems;
      int *numDataItemsInQueue; // [node]
      int areaWidth;
      int areaLength;
      int minItemCovDist;
      int maxItemCovDist;
      int fixedPacketLength;
      int nodePossCovDist;
      int nodePossCovX;
      int nodePossCovY;
      double HQx;
      double HQy;
      UniformVariable coverageRV;
      UniformVariable randomChoiceRV;
      double nodePowerBudget; // for use in one-shot problem
      double avgPowerBudget; // for use in long-term average problem
      int V; 
      Time timeBudget;
      int *numTimesExceededPowerBudget; // [node]
      int numTimesExceededTimeBudget;
      int numBytesReachDestination;
      double powerUsed; // at this node
      bool clearQueuesEachSlot;
      bool fixedPacketSize;
      bool generatedDuplicatePacketsThisSlot;
      bool longTermAvgProblem;
      bool oneShotProblem;
      bool randomChoiceProblem;
      bool approxOneShotProblem;
      bool approxMaxRatioProblem;
      bool approxVirtQueueRatioProblem;
      bool approxGreedyVQProblem;
      bool outputPowerVsTime;
      double sumOverlapOfChosenSets;
      double sumOverlapOfMaxSets;
      double sumCoverageOfChosenSets;
      double sumCoverageOfMaxSets;
      double packetDataRate; // total input rate at each node in packets per second...implicitly assumes all reporting nodes have the same input rate
      int duplicatePacketDataRate; // number of duplicate packets generated each time slot
      double sumDuplicateBytes;
      double sumChannelRate;
      int numSetsSkipped;
      FILE *powerVsTimeFile;
      time_t simStartTime;
      time_t simEndTime;

      // dataItems and areaCoverageInfo are always correlated, such that the vector of data items
      //   in node x has the area coverage info in the vector with index [x] in areaCov...
      vector< vector<AreaCoverageInfo> > areaCoverageInfo; // [node][max_queue_size]
      vector< Ptr<Packet> > dataItems; // replaces "queues" of distUnivSched...all items have HQ (node 0) as destination
  };

}
}

#endif /* __MAX-DIV-SCHED_H__ */

