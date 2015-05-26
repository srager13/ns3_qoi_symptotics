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
#define MAX_DIV_SCHED_CONSTRUCTOR_DEBUG 1
#define MAX_DIV_SCHED_INIT_DEBUG 1
#define MAX_DIV_SCHED_RECV_PACKET_DEBUG 1
#define MAX_DIV_SCHED_ROUTE_OUTPUT_DEBUG 1
#define MAX_DIV_SCHED_ROUTE_INPUT_DEBUG 1
#define MAX_DIV_SCHED_START_TIME_SLOT_DEBUG 1
#define MAX_DIV_SCHED_COMPLETE_TIME_SLOT_DEBUG 1
#define MAX_DIV_SCHED_SEND_PACKETS_DEBUG 1
#define MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG 1
#define MAX_DIV_SCHED_APPROX_ONE_SHOT_DEBUG 1
#define MAX_DIV_SCHED_APPROX_MAX_RATIO_DEBUG 1
#define MAX_DIV_SCHED_APPROX_GREEDY_VQ_DEBUG 1
#define MAX_DIV_SCHED_SEND_DATA_PACKET_DEBUG 1
#define MAX_DIV_SCHED_RECV_DATA_PACKET_DEBUG 1
#define MAX_DIV_SCHED_RECV_DATA_ACK_PACKET_DEBUG 1
#define MAX_DIV_SCHED_GENERATE_DUP_PACKETS_DEBUG 1
#define MAX_DIV_SCHED_GENERATE_RAND_COV_AREA_DEBUG 0
#define MAX_DIV_SCHED_GENERATE_AVG_COV_AREA_DEBUG 1
#define MAX_DIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG 1
#define MAX_DIV_SCHED_PRINT_STATS_DEBUG 1
#define MAX_DIV_SCHED_FIND_SHORTEST_PATH_DEBUG 1
#define MAX_DIV_SCHED_MAX_EXPECTED_DEBUG 1
#define MAX_DIV_SCHED_RESET_FRAME_DEBUG 1

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


  class MaxDivSched : public RoutingProtocol // (RoutingProtocol is DistUnivSched)
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
 
      void ResetFrame();
      
			void ClearQueues();

      void SendPackets( int maxDivDataSetNum ); //, vector< vector<int> > rTable );

      uint64_t FindOneShotMaxDivSet();
      
      int FindApproxOneShotMaxDivSet();
      
      uint64_t FindLongTermAvgMaxDivSet();

      int FindApproxVirtQueueRatioMaxDivSet();

      int FindApproxGreedyVQMaxDivSet();
  
      int FindApproxOneShotRatioMaxDivSet();
      
      uint64_t FindRandomMaxDivSet();
      
      uint64_t FindCacheProblemMaxDivSet();
      
      void FindFutureKnowledgeMaxDivSet();

      uint64_t FindCacheProblemSetsMaxDivSet();

      void MaxExpectedCoverage();
      
			void MaxSetsExpectedCoverage();
    
      virtual void GlobalExchangeControlInfoForward( Ptr<Packet> packet );
    
      virtual void GlobalExchangeControlInfoBackward( Ptr<Packet> packet );

      void SendDataPacket( int receivingNode, int commodity, Ptr<Packet> packetToSend ); //int dataItemIndex );

      double FindShortestPath( int source, int dest );
    
      void GenerateRandomAreaCoverageInfo( int *acx, int *acy, int *acxl, int *acyl, int *acs, int nodeId );
      
      void GenerateAverageCoverageInfo( int *acx, int *acy, int *acxl, int *acyl, int *acs, int nodeId );
      
      virtual void PrintStats();

      void setTotalNumDataItems( int newVal ) { totalNumDataItems = newVal; }
      void setNumDataItemsInQueue( int i, int newVal ) { numDataItemsInQueue[i] = newVal; }
      void setAreaWidth( int newVal ) { areaWidth = newVal; }
      void setAreaLength( int newVal ) { areaLength = newVal; }
      void setMinItemCovDist( int newVal ) { minItemCovDist = newVal; } 
      void setMaxItemCovDist( int newVal ) { maxItemCovDist = newVal; } 
      void setFixedPacketLength( int newVal ) { fixedPacketLength = newVal; } 
      void setAvgCovRadius( int newVal ) { avgCovRadius = newVal; } 
      void setFixedPacketSendSize( int newVal ) { fixedPacketSendSize = newVal; } 
      void setSumPacketSize( int newVal ) { sumPacketSize = newVal; } 
      void setNodePossCovDist( int newVal ) { nodePossCovDist = newVal; } 
      void setNodePossCovX( int newVal ) { nodePossCovX = newVal; } 
      void setNodePossCovY( int newVal ) { nodePossCovY = newVal; } 
      void setNumSlotsPerFrame( int newVal ) { numSlotsPerFrame = newVal; }
      void setSlotNumInFrame( int newVal ) { slotNumInFrame = newVal; }
      void setFrameNum( int newVal ) { frameNum = newVal; }
      void setHQx ( double newVal ) { HQx = newVal; }
      void setHQy ( double newVal ) { HQy = newVal; }
      void setNodePowerBudget( double newVal ) { nodePowerBudget = newVal; }
      void setAvgPowerBudget( double newVal ) { avgPowerBudget = newVal; }
      void setAvgCovRadiusVariance( double newVal ) { avgCovRadiusVariance = newVal; }
      void setV( int newVal ) { V = newVal; }
      void setTimeBudget( Time newVal ) { timeBudget = newVal; }
      void setNumTimesExceededPowerBudget( int i, int newVal ) { numTimesExceededPowerBudget[i] = newVal; }
      void setNumTimesExceededTimeBudget( int newVal ) { numTimesExceededTimeBudget = newVal; }
      void setNumBytesReachDestination( int newVal ) { numBytesReachDestination = newVal; }
      void setNumSlotsNoPath( int newVal ) { numSlotsNoPath = newVal; }
      void incrementNumSlotsNoPath( ) { numSlotsNoPath++; }
      void setPowerUsed( double newVal ) { powerUsed = newVal; }
      void setClearQueuesEachSlot( bool newVal ) { clearQueuesEachSlot = newVal; }
      void setFixedPacketSize( bool newVal ) { fixedPacketSize = newVal; }
      void setGeneratedDuplicatePacketsThisSlot( bool newVal ) { generatedDuplicatePacketsThisSlot = newVal; }
      void setLongTermAvgProblem( bool newVal ) { longTermAvgProblem = newVal; }
      void setOneShotProblem( bool newVal ) { oneShotProblem = newVal; }
      void setRandomChoiceProblem( bool newVal ) { randomChoiceProblem = newVal; }
      void setApproxOneShotProblem( bool newVal ) { approxOneShotProblem = newVal; }
      void setApproxOneShotRatioProblem( bool newVal ) { approxOneShotRatioProblem = newVal; }
      void setApproxVirtQueueRatioProblem( bool newVal ) { approxVirtQueueRatioProblem = newVal; }
      void setApproxGreedyVQProblem( bool newVal ) { approxGreedyVQProblem = newVal; }
      void setCacheProblem( bool newVal ) { cacheProblem = newVal; }
      void setFutureKnowledge( bool newVal ) { futureKnowledge = newVal; }
      void setSingleHop( bool newVal ) { singleHop = newVal; }
      void setSingleItem( bool newVal ) { singleItem = newVal; }
      void setOutputPowerVsTime( bool newVal ) { outputPowerVsTime = newVal; }
      void setSumOverlapOfChosenSets( double newVal ) { sumOverlapOfChosenSets = newVal; }
      void setSumOverlapOfMaxSets( double newVal ) { sumOverlapOfMaxSets = newVal; }
      void setSumCoverageOfChosenSets( double newVal ) { sumCoverageOfChosenSets = newVal; }
      void setSumCoverageOfMaxSets( double newVal ) { sumCoverageOfMaxSets = newVal; }
      void setAvgCoveragePerFrame( double newVal ) { avgCoveragePerFrame = newVal; }
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
      int getAvgCovRadius( ) { return avgCovRadius; } 
      int getFixedPacketSendSize( ) { return fixedPacketSendSize; } 
      int getSumPacketSize( ) { return sumPacketSize; } 
      int getNodePossCovDist( ) { return nodePossCovDist; } 
      int getNodePossCovX( ) { return nodePossCovX; } 
      int getNodePossCovY( ) { return nodePossCovY; } 
      int getNumSlotsPerFrame( ) { return numSlotsPerFrame; }
      int getSlotNumInFrame( ) { return slotNumInFrame; }
      int getFrameNum( ) { return frameNum; }
      double getHQx () { return HQx; }
      double getHQy () { return HQy; }
      double getNodePowerBudget( ) { return nodePowerBudget; }
      double getAvgPowerBudget( ) { return avgPowerBudget; }
      double getAvgCovRadiusVariance( ) { return avgCovRadiusVariance; }
      int getV( ) { return V; }
      //double ***HQCoverageEachSlot; // [area width][area length][time slot]
      Time getTimeBudget( ) { return timeBudget; }
      int getNumTimesExceededPowerBudget( int i ) { return numTimesExceededPowerBudget[i]; }
      int getNumTimesExceededTimeBudget( ) { return numTimesExceededTimeBudget; }
      int getNumBytesReachDestination( ) { return numBytesReachDestination; }
      int getNumSlotsNoPath( ) { return numSlotsNoPath; }
      double getPowerUsed( ) { return powerUsed; }
      bool getClearQueuesEachSlot( ) { return clearQueuesEachSlot; }
      bool getFixedPacketSize( ) { return fixedPacketSize; }
      bool getGeneratedDuplicatePacketsThisSlot( ) { return generatedDuplicatePacketsThisSlot; }
      bool getLongTermAvgProblem( ) { return longTermAvgProblem; }
      bool getOneShotProblem( ) { return oneShotProblem; }
      bool getRandomChoiceProblem( ) { return randomChoiceProblem; }
      bool getApproxOneShotProblem( ) { return approxOneShotProblem; }
      bool getApproxOneShotRatioProblem( ) { return approxOneShotRatioProblem; }
      bool getApproxVirtQueueRatioProblem( ) { return approxVirtQueueRatioProblem; }
      bool getApproxGreedyVQProblem( ) { return approxGreedyVQProblem; }
      bool getCacheProblem( ) { return cacheProblem; }
      bool getFutureKnowledge( ) { return futureKnowledge; }
      bool getSingleHop( ) { return singleHop; }
      bool getSingleItem( ) { return singleItem; }
      bool getOutputPowerVsTime( ) { return outputPowerVsTime; }
      double getSumOverlapOfChosenSets( ) { return sumOverlapOfChosenSets; }
      double getSumOverlapOfMaxSets( ) { return sumOverlapOfMaxSets; }
      double getSumCoverageOfChosenSets( ) { return sumCoverageOfChosenSets; }
      double getSumCoverageOfMaxSets( ) { return sumCoverageOfMaxSets; }
      double getAvgCoveragePerFrame( ) { return avgCoveragePerFrame; }
      double getPacketDataRate( ) { return packetDataRate; }
      int getDuplicatePacketDataRate( ) { return duplicatePacketDataRate; }
      double getSumDuplicateBytes( ) { return sumDuplicateBytes; }
      double getSumChannelRate( ) { return sumChannelRate; }
      int getNumSetsSkipped( ) { return numSetsSkipped; }

    private:

      double **locationCoverage; // location coverage array
      double **frameCoverage; // location coverage array
      double *virtPowerQueue; // [node]
      double *cumulativeCosts; // [node]
      double *expCumulativeCosts; // [node]
			uint64_t *futureMaxDivSetNum; // [node]
      int totalNumDataItems;
      int *numDataItemsInQueue; // [node]
      int areaWidth;
      int areaLength;
      int minItemCovDist; // size of side of packet's coverage if this is not fixed (if fixedPacketSize() if false)
      int maxItemCovDist;
      int fixedPacketLength; // size of one side of a packet's coverage if this is fixed (if fixedPacketSize() is true)
      int avgCovRadius; // size of one side of a packet's coverage if this is fixed (if fixedPacketSize() is true)
      int fixedPacketSendSize;
      int sumPacketSize;
      int nodePossCovDist;
      int nodePossCovX;
      int nodePossCovY;
      int numSlotsPerFrame;
      int slotNumInFrame;
			int frameNum;
      double HQx;
      double HQy;
      Ptr<UniformRandomVariable> coverageRV;
      Ptr<NormalRandomVariable> covRadiusRV;
      Ptr<UniformRandomVariable> randomChoiceRV;
      double nodePowerBudget; // for use in one-shot problem
      double avgPowerBudget; // for use in long-term average problem
      double avgCovRadiusVariance;
      int V; 
      Time timeBudget;
      int *numTimesExceededPowerBudget; // [node]
      int numTimesExceededTimeBudget;
      int numBytesReachDestination;
      int numSlotsNoPath;
      double powerUsed; // at this node
      bool clearQueuesEachSlot;
      bool fixedPacketSize;
      bool generatedDuplicatePacketsThisSlot;
      bool longTermAvgProblem;
      bool oneShotProblem;
      bool randomChoiceProblem;
      bool approxOneShotProblem;
      bool approxOneShotRatioProblem;
      bool approxVirtQueueRatioProblem;
      bool approxGreedyVQProblem;
      bool cacheProblem;
      bool futureKnowledge;
      bool singleHop;
      bool singleItem;
      bool outputPowerVsTime;
      double sumOverlapOfChosenSets;
      double sumOverlapOfMaxSets;
      double sumCoverageOfChosenSets;
      double sumCoverageOfMaxSets;
			double avgCoveragePerFrame;
      double packetDataRate; // total input rate at each node in packets per second...implicitly assumes all reporting nodes have the same input rate
      int duplicatePacketDataRate; // number of duplicate packets generated each time slot
      double sumDuplicateBytes;
      double sumChannelRate;
      int numSetsSkipped;
      FILE *powerVsTimeFile;
      time_t simStartTime;
      time_t simEndTime;

//      int **routingTable; // [node][node] - each row i is the path from that node to the HQ (node 0) 
                          // a -1 value represents no path to the HQ

      // dataItems and areaCoverageInfo are always correlated, such that the vector of data items
      //   in node x has the area coverage info in the vector with index [x] in areaCov...
			// If using future knowledge, then the second index in areaCov... is the number of time slot in the frame
      std::vector< std::vector<AreaCoverageInfo> > areaCoverageInfo; // [node][max_queue_size]
      std::vector< Ptr<Packet> > dataItems; // replaces "queues" of distUnivSched...all items have HQ (node 0) as destination

      std::vector<AreaCoverageInfo> collectedInfo; // data collected in the current frame
      //vector<AreaCoverageInfo> expCollectedInfo; // data collected in the current frame

      std::vector< std::vector<int> > routingTable;
  };

}
}

#endif /* __MAX-DIV-SCHED_H__ */

