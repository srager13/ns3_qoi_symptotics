/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "maxDivSched.h"
#include "ns3/dus-routing-protocol.h"
#include <math.h>
#include <stdio.h>
#include "ns3/inet-socket-address.h"
#include "ns3/integer.h"
#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include "ns3/wifi-mac.h"
#include "ns3/log.h"
#include "ns3/boolean.h"
#include "ns3/random-variable.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/wifi-net-device.h"
#include "ns3/adhoc-wifi-mac.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ipv4-interface.h"
#include "ns3/node.h"
#include "ns3/mobility-model.h"
#include "ns3/node-list.h"
#include "ns3/config.h"
#include <algorithm>
#include <limits>
#include "ns3/string.h"

namespace ns3 
{
namespace dus
{
NS_OBJECT_ENSURE_REGISTERED (MaxDivSched);

/* ... */

  MaxDivSched::MaxDivSched() :
    areaWidth( 500 ),
    areaLength( 500 ),
    minItemCovDist( 25 ),
    maxItemCovDist( 100 )
  {
    // Schedule Init function to initialize variables 
    Simulator::ScheduleNow( &MaxDivSched::MaxDivSchedInit, this );
  }
  
  TypeId
  MaxDivSched::GetTypeId (void)
  {
   static TypeId tid = TypeId ("ns3::dus::MaxDivSched")
    .SetParent<RoutingProtocol> ()
    .AddConstructor<MaxDivSched> ()
    .AddAttribute ("areaWidth", "Width of the (rectangular) environment (in meters).",
                   IntegerValue (350),
                   MakeIntegerAccessor (&MaxDivSched::setAreaWidth),
                   MakeIntegerChecker<int>())
    .AddAttribute ("areaLength", "Length of the (rectangular) environment (in meters).",
                   IntegerValue (350),
                   MakeIntegerAccessor (&MaxDivSched::setAreaLength),
                   MakeIntegerChecker<int>())
    .AddAttribute ("minItemCovDist", "Minimum length or width of location coverage generated for a data item (in meters).",
                   IntegerValue (10),
                   MakeIntegerAccessor (&MaxDivSched::setMinItemCovDist),
                   MakeIntegerChecker<int>())
    .AddAttribute ("maxItemCovDist", "Maximum length or width of location coverage generated for a data item (in meters).",
                   IntegerValue (46),
                   MakeIntegerAccessor (&MaxDivSched::setMaxItemCovDist),
                   MakeIntegerChecker<int>())
    .AddAttribute ("nodePossCovDist", "Length of each side in square surrounding node in which it can generate data item coverage. Units are in meters.",
                   IntegerValue (150),
                   MakeIntegerAccessor (&MaxDivSched::setNodePossCovDist),
                   MakeIntegerChecker<int>())
    .AddAttribute ("timeBudget", "Maximum total time of transmission allowed for a set of data items to be chosen in a time slot.",
                   TimeValue (MilliSeconds(100)),
                   MakeTimeAccessor (&MaxDivSched::timeBudget),
                   MakeTimeChecker())
    .AddAttribute ("nodePowerBudget", "Maximum power total allowed for each node for a set of data items to be chosen in a time slot.  Units are in ?",
                   DoubleValue (5.0),
                   MakeDoubleAccessor (&MaxDivSched::setNodePowerBudget),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("avgPowerBudget", "Maximum average power total allowed for each node for a set of data items to be chosen in a time slot.  Units are in ?",
                   DoubleValue (5.0),
                   MakeDoubleAccessor (&MaxDivSched::setAvgPowerBudget),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("V", "Parameter that sets importance of maximizing diversity versus acheiving avg power cost. (Zero ignores any power constraint)",
                   IntegerValue (0),
                   MakeIntegerAccessor (&MaxDivSched::setV),
                   MakeIntegerChecker<int>())
    .AddAttribute ("longTermAvgProblem", "If true, optimization problem seeks to maximize time average diversity subject to average power constraints.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&MaxDivSched::setLongTermAvgProblem),
                   MakeBooleanChecker ())
    .AddAttribute ("oneShotProblem", "If true, optimization problem seeks to maximize diversity each time slot subject to power & time constraints.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&MaxDivSched::setOneShotProblem),
                   MakeBooleanChecker ())
    .AddAttribute ("randomChoiceProblem", "If true, protocol chooses a random set of data that satisfies individual time and power constraints.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&MaxDivSched::setRandomChoiceProblem),
                   MakeBooleanChecker ())
    .AddAttribute ("approxOneShotProblem", "If true, protocol chooses the set of data that satisfies individual time and power constraints according to a greedy approach for one shot solution.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&MaxDivSched::setApproxOneShotProblem),
                   MakeBooleanChecker ())
    .AddAttribute ("approxMaxRatioProblem", "If true, protocol chooses the set of data that satisfies individual time and power constraints according to a greedy approach solution that chooses the max coverage/power ratio.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&MaxDivSched::setApproxMaxRatioProblem),
                   MakeBooleanChecker ())
    .AddAttribute ("approxGreedyVQProblem", "If true, protocol chooses the set of data that satisfies individual time and power constraints according to a greedy approach solution that chooses the max add_coverage - V*vq*power equation.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&MaxDivSched::setApproxGreedyVQProblem),
                   MakeBooleanChecker ())
    .AddAttribute ("outputPowerVsTime", "If true, will output total power used after each time slot.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&MaxDivSched::setOutputPowerVsTime),
                   MakeBooleanChecker ())
    .AddAttribute ("clearQueuesEachSlot", "If true, all queues of data items are cleared after each time slot.  Otherwise, all unsent data items remain.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&MaxDivSched::setClearQueuesEachSlot),
                   MakeBooleanChecker ())
    .AddAttribute ("fixedPacketSize", "If true, all data items generated will have the same size (square with sides of length = fixedPacketLength.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&MaxDivSched::setFixedPacketSize),
                   MakeBooleanChecker ())
    .AddAttribute ("fixedPacketLength", "Length of each side of packet's area coverage when fixed packet size is set to true. Units are in meters.",
                   IntegerValue (45),
                   MakeIntegerAccessor (&MaxDivSched::setFixedPacketLength),
                   MakeIntegerChecker<int>())
    .AddAttribute ("packetDataRate", "number of packets being generated by the application per second.",
                   DoubleValue (10.0),
                   MakeDoubleAccessor (&MaxDivSched::setPacketDataRate),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("duplicatePacketDataRate", "Number of duplicate packets that should be generated each time slot.",
                   IntegerValue(0),
                   MakeIntegerAccessor (&MaxDivSched::setDuplicatePacketDataRate),
                   MakeIntegerChecker<int>())
    ; 
    return tid;
  }

  MaxDivSched::~MaxDivSched ()
  {
    std::cout<<"In MaxDivSched destructor\n";
  }
  
  // /**
  // FUNCTION: MaxDivSchedInit
  // LAYER   : NETWORK
  // PURPOSE : Called at beginning of simulation to initialize some variables
  //           It is meant to ensure that attributes set in the simulation script
  //            are used instead of default values where calculations are performed.
  // PARAMETERS:
  // +None
  // RETURN   ::void:NULL
  // **/ 
  void 
  MaxDivSched::MaxDivSchedInit( )
  {
    if( MAX_DIV_SCHED_INIT_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << "in MaxDivSchedInit()\n";
    }

    int i, j; //, k;

    time( &simStartTime );
     
    locationCoverage = new double *[getAreaWidth()];

    for( i = 0; i < (int)getAreaWidth(); i++ )
    {

      locationCoverage[i] = new double [getAreaLength()];

      for( j = 0; j < (int)getAreaLength(); j++ )
      {

        locationCoverage[i][j] = 0.0;

      }
    }

    numDataItemsInQueue = new int [getNumNodes()];
    numTimesExceededPowerBudget = new int [getNumNodes()];
    virtPowerQueue = new double [getNumNodes()];

    for( i = 0; i < (int)getNumNodes(); i++ )
    {
      areaCoverageInfo.push_back( vector<AreaCoverageInfo>() ); // add an empty vector for each node

      Ptr<MobilityModel> mobility = NodeList::GetNode((uint32_t)i)->GetObject<MobilityModel> ();
      Vector pos = mobility->GetPosition();
      if( MAX_DIV_SCHED_INIT_DEBUG && getNodeId() == 0 ) // getnode == 0 becuase node 0 can print all nodes' positions
      {
        std::cout<<"Node (from MaxDivSchedInit) " << i << " position: x = " << pos.x << ", y = " << pos.y << "\n";
      }
      if( i == 0 )
      {
        setHQx( pos.x );
        setHQy( pos.y );
        setGlobalChannelRates( 0, 0, 0.0 );
      }
      else
      {
        double chRate;
        int chRateIndex;

        GetChannelRateFromCoordinates( HQx, HQy, pos.x, pos.y, &chRate, &chRateIndex );

        // set available channel rate to HQ (determined be distance from node to HQ)
        setGlobalChannelRates( i, 0, chRate );
        if( MAX_DIV_SCHED_INIT_DEBUG && getNodeId() == 0 )
        {
          std::cout<<"\tglobal channel rate from  " << i << " to HQ =  " << chRate << "\n";
        }
      }

      numDataItemsInQueue[i] = 0;
      numTimesExceededPowerBudget[i] = 0; 
      virtPowerQueue[i] = 0.0;
    }
    setTotalNumDataItems( 0 );
    numTimesExceededTimeBudget = 0; 
    powerUsed = 0.0;
    sumOverlapOfChosenSets = 0.0;
    sumOverlapOfMaxSets = 0.0;
    sumCoverageOfChosenSets = 0.0;
    sumCoverageOfMaxSets = 0.0;
    setGeneratedDuplicatePacketsThisSlot( false );
    setSumDuplicateBytes( 0.0 );
    setNumBytesReachDestination( 0 ); 
    
    if( getOutputPowerVsTime() )
    {
      char buf[100];
      sprintf( buf, "%smaxDivSched-power-vs-time-node-%i.csv", getDataFilePath().c_str(), getNodeId() );
      powerVsTimeFile = fopen( buf, "w" );
    }

    if( getLongTermAvgProblem() && getRandomChoiceProblem() )
    {
      std::cout<<"ERROR: Both Long-Term Average problem and Random Choice problem flags are set.  Should be one or the other.  Exiting.\n";
      exit(-1);
    }
  }
  
  // /**
  // FUNCTION: StartTimeSlot
  // LAYER   : NETWORK
  // PURPOSE : - Called at beginning of control information exchange period.  
  //				- Resets all status of relevant statistics.
  //				- Sets state to SEND_OWN_INFO, and sends first message
  //				 of type DIST_UNIV_SCHED_SEND_CONTROL_INFO
  //				 to start control info exchange period, and sends message
  //				 to 'complete' (transition to data trx phase) time slot at the correct time.
  // PARAMETERS:
  // +None
  // RETURN   ::void:NULL
  // **/ 
  void 
  MaxDivSched::StartTimeSlot( )
  {
    if( MAX_DIV_SCHED_START_TIME_SLOT_DEBUG )
    {    
      std::cout<<"Node " << getNodeId() << " in MaxDivSched::StartTimeSlot() at time = " << Simulator::Now().GetNanoSeconds() << " (nanoseconds)...time slot number " << getTimeSlotNum() << "\n";
    }

    if( getBatteryPowerLevel() < 0.0 && getNodeLifetime().GetSeconds() == 0.0 )
    {
      setNodeLifetime( Simulator::Now() );
    }

    int i, j;
    j = 0;
	
    // reset control info exchange state
    setControlInfoExchangeState(SEND_OWN_INFO);
    // reset time to time out waiting for ack
    setWaitForAckTimeoutTime ( (Time)(Simulator::Now().GetSeconds() + getWaitForAckTimeoutWindow()) );
    // update generate duplicate packets flag 
    setGeneratedDuplicatePacketsThisSlot( false );

    // update timeSlotNum
    setTimeSlotNum(getTimeSlotNum()+1);

    // send packet to start new round of scheduling or enter new time slot if freq > 1
    if( isGLOBAL_KNOWLEDGE() ) 
    {
      if( getNodeId() == 0 )
      {
      // perform data exchange again without control info exchange
      // no delay...exchange control info immediately...complete time slot messages sent from ExchangeControlInfo* functions  
        Simulator::Schedule ( Seconds (0.0), &MaxDivSched::GlobalExchangeControlInfoForward, this, (Ptr<Packet>)0 );
      }
    }
  
    // reset control information exchange variables for next round of exchanges
    setControlPacketsSentThisTimeSlot(0);
    setControlPacketsRcvdThisTimeSlot(0);
    setNumCollisionsThisTimeSlot(0); 
    setTimeSlotMessageSent(false);
    for( i = 0; i < getNumNodes(); i++ )
    {
      setDataPacketsRcvdThisSlot(i, 0);
      setNodeInfoFirstRcv(i, true);
    }
  
    // Clear all temporary channel rates and time stamps except for own...we will collect new channel rates from control packets before starting next time slot
    // NOT true ANYMORE...leaving channel rates as is
    for( i = 0; i < getNumNodes(); i++ )
    {
      for( j = 0; j < getNumNodes(); j++ )
      {
        if( i != (getNodeId()) && j!= (getNodeId()))
        {
          //        channelRates[i][j] = 0.0;
          setTimeStamps( i, j, (Time)0 );
        }
#ifdef USE_NEXT_TIME_STAMPS
        else
        { 
          setTimeStamps( i, j, (Time)0 );
        
          setNextTimeStamps( i, j, (Time)0 );
        
          setTimeStamps( i, j, getNextTimeStamps( i, j ) );
        }
#endif  
      }
    }
  }

// /**
// FUNCTION: CompleteTimeSlot
// LAYER   : NETWORK
// PURPOSE : Called at end of control information exchange period.  
//         - generates duplicate packets (with same coverage of packets in other nodes)
//           to create percentage of overlapping packets
//         - Makes decisions for which data items should be sent to HQ
//          using the area coverage information and channel rates exchanged.
//         - Sets state to DATA_TRX, calls SendDataPacket()
//          to start date transmission period, and sends message
//          to start new time slot at the correct time.
// PARAMETERS:
// +none:
// RETURN   ::void:NULL
// **/
  void MaxDivSched::CompleteTimeSlot()
  {
    if( MAX_DIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
    {
      std::cout<<"\nNode " << getNodeId() << ": in MaxDivSched::CompleteTimeSlot() at time = " << Simulator::Now().GetSeconds() << "\n";
    }

    
    if( getNodeId() == 0 && getTimeSlotNum()%100000 == 0 && !MAX_DIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
    {
      printf("---------------------------------------------------------------------------------\n");
      std::cout<<"\nNode " << getNodeId() << ": in MaxDivSched::CompleteTimeSlot() at time = " << Simulator::Now().GetSeconds() << "\n";
      printf("---------------------------------------------------------------------------------\n");
    }
      

    if( MAX_DIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
    {
        printf("\tNew Time Slot...Now().GetSeconds() returns %f in seconds\n", (double)Simulator::Now().GetSeconds());
    }

    int i, maxDivDataSetNum;
    
    // count how many data items are available at all nodes' queues
    int temp_sum = 0;
    for( i = 0; i < getNumNodes(); i++ )
    {
      temp_sum += (int)areaCoverageInfo[i].size();
      if( MAX_DIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
      {
        std::cout<<"\tnumber of items in node "<< i <<"'s queue = " << areaCoverageInfo[i].size() <<"\n";
      }
    }
    setTotalNumDataItems( temp_sum );
   
    // Reset Channel Rates     
    if( isGLOBAL_KNOWLEDGE() && getNodeId() != 0 )
    {
      Ptr<MobilityModel> mobility = NodeList::GetNode((uint32_t)getNodeId())->GetObject<MobilityModel> ();
      Vector pos = mobility->GetPosition();
      double chRate;
      int chRateIndex;

      GetChannelRateFromCoordinates( getHQx(), getHQy(), pos.x, pos.y, &chRate, &chRateIndex );
        
      // set available channel rate to HQ (determined be distance from node to HQ)
      setGlobalChannelRates( getNodeId(), 0, chRate );
    }
    
    for( i = 0; i < getNumNodes(); i++ )
    {
      if( MAX_DIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
      {
        std::cout<<"\trate from node "<< i <<" to HQ = " << getGlobalChannelRates( i, 0 ) <<"\n";
      }
    }

    // Change State Appropriately
    setControlInfoExchangeState(DATA_TRX);
    
    // Observe queues and available channel rates and choose set of data to transmit to HQ (node 0)
    if( getNodeId() == 0 )
    {
      // only node 0 will actually perform the calculations and will call the send packets function for
      //  all other nodes to decrease run time of simulation
      if( getRandomChoiceProblem() )
      {
        maxDivDataSetNum = FindRandomMaxDivSet();
      }
      else if( getLongTermAvgProblem() )
      {
          maxDivDataSetNum = FindLongTermAvgMaxDivSet( );
      }
      else if( getApproxMaxRatioProblem() )
      {
        maxDivDataSetNum = FindApproxMaxRatioMaxDivSet();
      }
      else if( getApproxOneShotProblem() )
      {
          maxDivDataSetNum = FindApproxOneShotMaxDivSet();
      }
      else if( getOneShotProblem() )
      {
        maxDivDataSetNum = FindOneShotMaxDivSet( );
      }
      else if( getApproxGreedyVQProblem() )
      {
        maxDivDataSetNum = FindApproxGreedyVQMaxDivSet();
      }
      else
      {
        std::cout<<"Type of algorithm not set.  Needs to be one of the following: \n";
        std::cout<<"\tOne Shot\n\tLong Term Avg.\n\tRandom\n\tApprox. One Shot\n\tApprox. Max Ratio\n";
      }
    
      if( MAX_DIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
      {
        printf("node %i:  Chose data set number %i\n", getNodeId(), maxDivDataSetNum );
      }

      // SEND PACKETS
      for( i = 0; i < getNumNodes(); i++ )
      {
        Ptr<Node> node = NodeList::GetNode(i);
        Ptr<MaxDivSched> dusRp = node->GetObject<MaxDivSched>();
        if ( dusRp == 0 )
        {
          std::cout<<"ERROR:  DUSRP == 0 in CompleteTimeSlot...Exiting\n";
          exit(-1);
        }
        Simulator::Schedule ( Seconds(0.0), &MaxDivSched::SendPackets, dusRp, maxDivDataSetNum );
      }
    }
    // send message to mark beginning of next time slot

    if( isGLOBAL_KNOWLEDGE() )
    {
        // pause for time slot duration to allow for transmission of packets...also ensures right number of time slots in simulation
      Simulator::Schedule ( getTimeSlotDuration(), &MaxDivSched::StartTimeSlot, this );
    }
    // keep track of channel rates to find average at end
    setSumChannelRate( getSumChannelRate() + getGlobalChannelRates(getNodeId(), 0) );

    //if( getNodeId() == 1 )
    //{
    //Ptr<MobilityModel> mobility = NodeList::GetNode((uint32_t)getNodeId())->GetObject<MobilityModel> ();
    //Vector pos = mobility->GetPosition();
    //printf( "Node %i:  Time slot = %i\tpos.x = %.2f, pos.y = %.2f\trate = %f\n", getNodeId(),  getTimeSlotNum(), pos.x, pos.y, getGlobalChannelRates(getNodeId(), 0) );
    //}

    // keep track of how many sets get to be skipped in optimization problem 
    // TODO: check this...should it be reset each time slot?
    setNumSetsSkipped( 0 );
  }

// /**
// FUNCTION: SendPackets
// LAYER   : NETWORK
// PURPOSE : Called from complete time slot function after determing which packets should be sent.
//            Copies packets and schedules the send data packet function to send it at the appropriate
//            time (to prevent collisions).  It then removes either all of the data in queues or just 
//            that which was sent, depending on the value of the clear queues each slot flag.
// PARAMETERS:
// + maxDivDataSetNum : int : this number is the binary representation of which packets should be sent out of entire data set
// RETURN   ::void:NULL
// **/
  void
  MaxDivSched::SendPackets( int maxDivDataSetNum )
  {
    int i, j, k;
    setTrxThisTimeSlot( false );
    double powerCost, powerCostForNode[getNumNodes()];
    unsigned chosenDataSet = (unsigned)maxDivDataSetNum;
    int dataItemIndex = 0;
    int indexInChosenSet = 1;
    Time timeCost;
    Time delayTime = Seconds(0.0);
    for( j = 0; j < (int)getNumNodes(); j++ )
    {
      powerCostForNode[j] = 0.0;
      for( k = 0; k < (int)areaCoverageInfo[j].size(); k++ )
      {
        if( ((chosenDataSet>>dataItemIndex)&(unsigned)1) == (unsigned)1 ) // data item is included in this dataSet
        {
          timeCost = MicroSeconds((uint64_t)(((double)areaCoverageInfo[j][k].size*8.0)/getGlobalChannelRates(j, 0))); // size changed to bits, channel rate given in Mbps, so result is in MicroSeconds 
          powerCost = MDS_TRX_POWER_MW*(timeCost.GetSeconds()); // units are milliwatt-seconds /3600.0); // units are milliwatts and hours
          powerCostForNode[j] += powerCost; 
          if( j == getNodeId() )
          {
            if( MAX_DIV_SCHED_SEND_PACKETS_DEBUG )
            {
              std::cout<<"\tnode " << getNodeId() << " sending data item " << dataItemIndex << " after " << delayTime.GetMicroSeconds() << "\n";
            }
            // need to send this data item
            // send packet with delay (to create a schedule)
            Simulator::Schedule ( delayTime, &MaxDivSched::SendDataPacket, this, 0, 0, dataItems[k]->Copy() );
            setTrxThisTimeSlot( true );
            // update time delay
            
            if( MAX_DIV_SCHED_SEND_PACKETS_DEBUG )
            {
              std::cout<<"Time cost of data item with size " << areaCoverageInfo[j][k].size << " at rate ";
              std::cout<< getGlobalChannelRates(j, 0) << " = " << timeCost.GetMicroSeconds() << " in microseconds\n";
            }
          }
          delayTime += timeCost+MicroSeconds(500);
          indexInChosenSet++;
        }
        // move to consider if next data item is in this set
        dataItemIndex++;
      }
    }

    if( MAX_DIV_SCHED_SEND_PACKETS_DEBUG && !isTrxThisTimeSlot() )
    {
      std::cout<<"\tThis node (" << getNodeId() << ") did not transmit anything in chosen data set.\n";
    }
    if( getClearQueuesEachSlot() )
    {
      dataItems.clear();

      for( i = 0; i < getNumNodes(); i++ )
      {
        areaCoverageInfo[i].clear();
      }
    }
    else
    {
    // now delete sent items from vector
    //   only need to care about items sent from this node...other nodes will
    //   update themselves and we will restock area coverage info in 
    //   the next control information exchange
    chosenDataSet = (unsigned)maxDivDataSetNum;
    dataItemIndex = 0;
    vector< Ptr<Packet> > temp_dataItems;
    vector<AreaCoverageInfo> temp_areaCoverageInfo;
    for( j = 0; j < (int)getNumNodes(); j++ )
    {
      std::cout<< areaCoverageInfo[j].size() << " ";
      for( k = 0; k < (int)areaCoverageInfo[j].size(); k++ )
      {
        if( MAX_DIV_SCHED_SEND_PACKETS_DEBUG )
        {
          std::cout<< "(" << j << "," << k << "), dataItemIndex = " << dataItemIndex;
        }

        if( j == getNodeId() && ((chosenDataSet>>dataItemIndex)&(unsigned)1) != (unsigned)1 ) // data item was not included in this dataSet (so it didn't get sent)
        {
          if( MAX_DIV_SCHED_SEND_PACKETS_DEBUG )
          {
            std::cout << " *need to keep* ";
          }
          // need to keep this data item
          temp_dataItems.push_back( dataItems[k]->Copy() );
          temp_areaCoverageInfo.push_back( areaCoverageInfo[j][k] );
        }
        // move to consider if next data item is in this set
        dataItemIndex++;
      }
      std::cout<<"\n";
    }
    dataItems.swap( temp_dataItems );
    areaCoverageInfo[getNodeId()].swap( temp_areaCoverageInfo );
    }
   
    if( MAX_DIV_SCHED_SEND_PACKETS_DEBUG )
    {
      std::cout<<"\tupdated data item queues\n";
    }

    if( getLongTermAvgProblem() || getApproxGreedyVQProblem() )
    {
      if( MAX_DIV_SCHED_SEND_PACKETS_DEBUG )
      {
        std::cout<<"Node " << getNodeId() << "'s view:\n";
      }
      // update virtual queues
      for( i = 0; i < getNumNodes(); i++ )
      {
        virtPowerQueue[i] = MAX( (virtPowerQueue[i] - getAvgPowerBudget()), 0 ) + powerCostForNode[i];
        if( MAX_DIV_SCHED_SEND_PACKETS_DEBUG )
        {
          std::cout<<"\tNode " << i << ": power used = " << powerCostForNode[i] << ", virtual queue = " << virtPowerQueue[i] << "\n";
        }
      }
    }

    // set power and remaining battery life
    setPowerUsed( getPowerUsed() + powerCostForNode[getNodeId()] );
  
    if( MAX_DIV_SCHED_SEND_PACKETS_DEBUG )
    {
      std::cout<<"\tNode " << getNodeId() << ": power used = " << powerCostForNode[getNodeId()] << ", battery before trx = " << getBatteryPowerLevel();
    }

    setBatteryPowerLevel( getBatteryPowerLevel() - powerCostForNode[getNodeId()] );

    if( MAX_DIV_SCHED_SEND_PACKETS_DEBUG )
    {
      std::cout<<", battery after trx = " << getBatteryPowerLevel() << "\n";
    }

    if( getOutputPowerVsTime() )
    {
      fprintf( powerVsTimeFile, "%f, ", powerCostForNode[getNodeId()] ); //getPowerUsed() );
    }
  }

// /**
// FUNCTION: FindOneShotMaxDivSet
// LAYER   : NETWORK
// PURPOSE : Called from complete time slot function to determine the set of packets with the most coverage
//            within the time and power budget constraints.
// PARAMETERS: none
//            
// RETURN   ::int: number that represents the binary vector of which items should be sent
// **/
  uint64_t
  MaxDivSched::FindOneShotMaxDivSet( )
  {
    uint64_t i;
    int j, k, m, n;

    if( MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " in MaxDivSched::FindOneShotMaxDivSet()\n";
    }
    Time timeCost, timeCostOfSet;
    double powerCost, powerCostForNode[getNumNodes()]; //, powerCostForNodeOfChosenSet[getNumNodes()];
    for( j = 0; j < getNumNodes(); j++ )
    {
      powerCostForNode[j] = 0.0;
      //powerCostForNodeOfChosenSet[i] = 0.0;
    }
    double sizeOfSet;
    double overlapOfSet, overlapOfChosenSet;
    double coverageOfSet, coverageOfChosenSet;

    // calculate coverage for all possible data item trx schemes
    double numPossDataSets = pow( ((double)2), getTotalNumDataItems() );
    int dataItemIndex;
    double maxDivScore = -1;
    uint64_t maxDivDataSetNum = 0;

    // for speed of computation, we keep track of sets that exceed time or power constraints
    //   and eliminate supersets of these from being considered
    vector<uint64_t> illegalSets;
    int numSkipped = 0;
    
    if( MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG )
    {
      std::cout<<"\tnumPossDataSets = " << numPossDataSets << "\n";
    }

    // check each possible set of items
    for( i = 0; i < (uint64_t)numPossDataSets; i++ )
    {
      if( i%1000 == 0 && MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG )
      {
        std::cout<<"\tchecking set " << i << " out of " << numPossDataSets << "\n";
      }

      // check to see if this set already has a subset that is over time or power budget
      //   before calculating coverage
      for( j = 0; j < (int)illegalSets.size(); j++ )
      {
        if( (illegalSets[j] & i) == illegalSets[j] )
        {
          numSkipped++;
          continue; 
        }
      }
      timeCostOfSet = Seconds(0.0);
      sizeOfSet = 0.0;
      overlapOfSet = 0.0;
      coverageOfSet = 0.0;
      if( MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG )
      {
        std::cout<< "\tData set # " << i << ": ";
      }
      // intitialize/reset location coverage area
      for( m = 0; m < getAreaWidth(); m++ )
      {
        for( n = 0; n < getAreaLength(); n++ )
        {
          locationCoverage[m][n] = 0.0;
        }
      }
      // calculate coverage score 
      unsigned dataSet = (unsigned)i; 
      dataItemIndex = 0;
      for( j = 0; j < (int)getNumNodes(); j++ )
      {
        powerCostForNode[j] = 0.0;
        for( k = 0; k < (int)areaCoverageInfo[j].size(); k++ )
        {
          if( ((dataSet>>dataItemIndex)&(unsigned)1) == (unsigned)1 ) // data item is included in this dataSet
          {
            // need to set coverage of dataItem
            int xIndex = areaCoverageInfo[j][k].x;
            int yIndex = areaCoverageInfo[j][k].y;

            // update cost of set
            timeCost = MicroSeconds((uint64_t)(((double)areaCoverageInfo[j][k].size*8.0)/getGlobalChannelRates(j, 0))); // size changed to bits, channel rate given in Mbps, so result is in MicroSeconds 
            if( MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG )
            {
              std::cout<<"Time cost of data item with size " << areaCoverageInfo[j][k].size << " at rate ";
              std::cout<< getGlobalChannelRates(j, 0) << " = " << timeCost.GetMicroSeconds() << " in microseconds\n";
            }
            timeCostOfSet += timeCost;
           
            powerCost = MDS_TRX_POWER_MW*(timeCost.GetSeconds()); // units are milliwatt-seconds /3600.0); // units are milliwatts and hours
            powerCostForNode[j] += powerCost; 

            sizeOfSet += (double)areaCoverageInfo[j][k].size;
            
            if( MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG )
            {
              std::cout<<"Power cost of data item with size " << areaCoverageInfo[j][k].size << " at rate ";
              std::cout<< getGlobalChannelRates(j, 0) << " = " << powerCost << " in milliwatt-seconds\n";
            }
          
            if( MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG )
            {
              std::cout<< "(" << j << "," << k << ")  ";
              std::cout<<" [" << xIndex <<"," << yIndex << "," << areaCoverageInfo[j][k].xLength << "," << areaCoverageInfo[j][k].yLength << "] ";
            }

            for( m = 0; m < areaCoverageInfo[j][k].xLength; m++ )
            {
              for( n = 0; n < areaCoverageInfo[j][k].yLength; n++ )
              {
                locationCoverage[xIndex+m][yIndex+n] = 1.0;
              }
            }
          }
          // move to consider if next data item is in this set
          dataItemIndex++;
        }
      }

      // check to make sure that total time to transmit set does not exceed time budget
      //   if it does, throw it out, and move to next set
      bool exceededTimeBudget = false;
      if( timeCostOfSet > timeBudget )
      {
        setNumTimesExceededTimeBudget( getNumTimesExceededTimeBudget() + 1 );
        exceededTimeBudget = true;
      }
      // check to make sure that total power cost of set does not exceed time budget
      //   if it does, throw it out, and move to next set
      bool exceededPowerBudget = false;
      for( j = 0; j < (int)getNumNodes(); j++ )
      {
        if( powerCostForNode[j] > getNodePowerBudget() )
        {
          setNumTimesExceededPowerBudget( j, getNumTimesExceededPowerBudget(j) + 1 );
          exceededPowerBudget = true;
        }
      }
      // now get diversity score by summing over entire location coverage area
      double diversityScore = 0.0;
      for( m = 0; m < getAreaWidth(); m++ )
      {
        for( n = 0; n < getAreaLength(); n++ )
        {
          diversityScore += locationCoverage[m][n];
        }
      }
      // calculate percent overlap = 100 * (coverage/size)
      //   and coverage = coverage/totalarea
      if( sizeOfSet != 0.0 )
      {
        overlapOfSet = 100.0 * (1.0 - diversityScore/(sizeOfSet*16.0)); // this factor of 16 makes up for 4 sq meters to each byte in packet size
        coverageOfSet = 100.0 * (diversityScore/(double)(getAreaWidth()*getAreaLength()));
      }
      if( i == (uint64_t)numPossDataSets - 1 )
      {
        setSumOverlapOfMaxSets( getSumOverlapOfMaxSets() + overlapOfSet );
        setSumCoverageOfMaxSets( getSumCoverageOfMaxSets() + coverageOfSet );
      }
      if( exceededPowerBudget || exceededTimeBudget )
      {
        illegalSets.push_back(i);
        continue;
      }
      // check to see if this is max diversity score
      if( diversityScore > maxDivScore )
      {
        maxDivScore = diversityScore;
        maxDivDataSetNum = i;
        overlapOfChosenSet = overlapOfSet;
        coverageOfChosenSet = coverageOfSet;
        //for( j = 0; j < (int)getNumNodes(); j++ )
        //{
        //  powerCostForNodeOfChosenSet[j] = powerCostForNode[j];
        //}
      }
      if( MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG )
      {
        std::cout<<"\nScore = " << diversityScore << "\n";
        std::cout<<"Overlap = " << overlapOfSet << "\n";
        std::cout<<"Size of set = " << sizeOfSet << "\n";
      }
    }
    setSumOverlapOfChosenSets( getSumOverlapOfChosenSets() + overlapOfChosenSet );
    setSumCoverageOfChosenSets( getSumCoverageOfChosenSets() + coverageOfChosenSet );
    setNumSetsSkipped( getNumSetsSkipped() + numSkipped );
    //std::cout<<"Number skipped = " << numSkipped << "\n";

    return maxDivDataSetNum;
  }

// /**
// FUNCTION: FindApproxOneShotMaxDivSet
// LAYER   : NETWORK
// PURPOSE : Called from complete time slot function to determine the set of packets with the most coverage
//            within the time and power budget constraints according to the greedy approximation algorithm
// PARAMETERS: none
//            
// RETURN   ::int: number that represents the binary vector of which items should be sent
// **/
  int
  MaxDivSched::FindApproxOneShotMaxDivSet( )
  {
    int i, j, m, n;

    if( MAX_DIV_SCHED_APPROX_ONE_SHOT_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " in MaxDivSched::FindApproxOneShotMaxDivSet()\n";
    }
    Time timeCost, timeCostSoFar, timeCostOfMax;
    double powerCost, powerCostForNode[getNumNodes()], powerCostOfMax;
    for( i = 0; i < getNumNodes(); i++ )
    {
      powerCostForNode[i] = 0.0;
    }
    double sizeOfSet;
    int numItemsInSet = 0;
    double overlapOfChosenSet;
    double coverageOfChosenSet;

    // calculate coverage for all possible data item trx schemes
    int maxDivDataSetNum = 0;
    double maxAdditionalCov;
    int maxAddNodeIndex, maxAddItemIndex;

    for( i = 0; i < (int)getNumNodes(); i++ )
    {
      powerCostForNode[i] = 0.0;
      for( j = 0; j < (int)areaCoverageInfo[i].size(); j++ )
      {
        areaCoverageInfo[i][j].check = true;
      }
    }
    // reset location coverage
    for( m = 0; m < getAreaWidth(); m++ )
    {
      for( n = 0; n < getAreaLength(); n++ )
      {
        locationCoverage[m][n] = 0.0;
      }
    }
   
    bool moreToCheck = true;
    while( moreToCheck ) // need to break this loop only when we're out of data items to be added
    { 
      if( MAX_DIV_SCHED_APPROX_ONE_SHOT_DEBUG )
      {
        std::cout<<"\tLooking to add item number " << numItemsInSet+1 << ":\n";
      }
      maxAdditionalCov = -1.0;
      moreToCheck = false;
      maxAddNodeIndex = -1;
      maxAddItemIndex = -1;

      // find item with highest additional coverage that doesn't exceed time or cost budgets
      for( i = 0; i < (int)getNumNodes(); i++ )
      {
        for( j = 0; j < (int)areaCoverageInfo[i].size(); j++ )
        {
          if( areaCoverageInfo[i][j].check )
          {
            moreToCheck = true;
            timeCost = MicroSeconds((uint64_t)(((double)areaCoverageInfo[i][j].size*8.0)/getGlobalChannelRates(i, 0))); // size changed to bits, channel rate given in Mbps, so result is in MicroSeconds 
            powerCost = MDS_TRX_POWER_MW*(timeCost.GetSeconds()); // units are milliwatt-seconds /3600.0); // units are milliwatts and hours
            if( timeCostSoFar + timeCost > getTimeBudget() )
            {
              // this data item would put set over time budget limit...eliminate it from contention and continue
              areaCoverageInfo[i][j].check = false;
              continue;
            }
            if( powerCostForNode[i] + powerCost > getNodePowerBudget() )
            {
              // this data item would put set over time budget limit...eliminate it from contention and continue
              areaCoverageInfo[i][j].check = false;
              continue;
            }
          }
          else
          {
            continue;
          }

          if( MAX_DIV_SCHED_APPROX_ONE_SHOT_DEBUG )
          {
            std::cout<<"\t\tChecking item " << j << " in queue of node " << i << "\n";
          }

          // item has not been added to set yet and does not violate any budgets, check if it is max additional coverage
          double additionalCov = 0.0; 
          int xIndex = areaCoverageInfo[i][j].x;
          int yIndex = areaCoverageInfo[i][j].y;
          for( m = 0; m < areaCoverageInfo[i][j].xLength; m++ )
          {
            for( n = 0; n < areaCoverageInfo[i][j].yLength; n++ )
            {
              if( locationCoverage[xIndex+m][yIndex+n] == 0.0 )
              {
                additionalCov++;
              }
            }
          }
          if( additionalCov > maxAdditionalCov )
          {
            maxAdditionalCov = additionalCov;
            maxAddNodeIndex = i;
            maxAddItemIndex = j;
            timeCostOfMax = timeCost;
            powerCostOfMax = powerCost;
          }
        }
      }
      if( !moreToCheck )
      {
        continue;
      }

      if( maxAddNodeIndex == -1 || maxAddItemIndex == -1 )
      {  
        // didn't find any items to add within budgets
        break;
      }
     
      // otherwise, add item with greatest additional coverage
      int dataItemIndex = 0;
      for( i = 0; i < maxAddNodeIndex; i++ )
      {
        for( j = 0; j < (int)areaCoverageInfo[i].size(); j++ )
        {
          dataItemIndex++;
        }
      }
      dataItemIndex += maxAddItemIndex + 1;
      maxDivDataSetNum = maxDivDataSetNum|((unsigned)1<<dataItemIndex);
      powerCostForNode[maxAddNodeIndex] += powerCostOfMax;
      timeCostSoFar += timeCostOfMax;
      sizeOfSet += (double)areaCoverageInfo[maxAddNodeIndex][maxAddItemIndex].size;
      areaCoverageInfo[maxAddNodeIndex][maxAddItemIndex].check = false;
      numItemsInSet++;

      if( MAX_DIV_SCHED_APPROX_ONE_SHOT_DEBUG )
      {
        std::cout<<"\tMax item is " << maxAddItemIndex << " in queue of node " << maxAddNodeIndex << "...data item index = " << dataItemIndex << "\n";
      }

      // update location coverage
      int xIndex = areaCoverageInfo[maxAddNodeIndex][maxAddItemIndex].x;
      int yIndex = areaCoverageInfo[maxAddNodeIndex][maxAddItemIndex].y;
      for( m = 0; m < areaCoverageInfo[maxAddNodeIndex][maxAddItemIndex].xLength; m++ )
      {
        for( n = 0; n < areaCoverageInfo[maxAddNodeIndex][maxAddItemIndex].yLength; n++ )
        {
          locationCoverage[xIndex+m][yIndex+n] = 1.0;
        }
      }
    }
    // now get diversity score by summing over entire location coverage area
    double diversityScore = 0.0;
    for( m = 0; m < getAreaWidth(); m++ )
    {
      for( n = 0; n < getAreaLength(); n++ )
      {
        diversityScore += locationCoverage[m][n];
      }
    }
    // calculate percent overlap = 100 * (coverage/size)
    //   and coverage = coverage/totalarea
    if( sizeOfSet != 0.0 )
    {
      overlapOfChosenSet = 100.0 * (1.0 - diversityScore/(sizeOfSet*16.0)); // this factor of 16 makes up for 4 sq meters to each byte in packet size
      coverageOfChosenSet = 100.0 * (diversityScore/(double)(getAreaWidth()*getAreaLength()));
    }

    setSumOverlapOfChosenSets( getSumOverlapOfChosenSets() + overlapOfChosenSet );
    setSumCoverageOfChosenSets( getSumCoverageOfChosenSets() + coverageOfChosenSet );
    
    // find coverage and overlap of entire data set for reference 
    sizeOfSet = 0.0;
    for( i = 0; i < (int)getNumNodes(); i++ )
    {
      for( j = 0; j < (int)areaCoverageInfo[i].size(); j++ )
      {
        // update location coverage
        int xIndex = areaCoverageInfo[i][j].x;
        int yIndex = areaCoverageInfo[i][j].y;
        for( m = 0; m < areaCoverageInfo[i][j].xLength; m++ )
        {
          for( n = 0; n < areaCoverageInfo[i][j].yLength; n++ )
          {
            locationCoverage[xIndex+m][yIndex+n] = 1.0;
          }
        }
        sizeOfSet += (double)areaCoverageInfo[i][j].size;
      }
    }
    diversityScore = 0.0;
    for( m = 0; m < getAreaWidth(); m++ )
    {
      for( n = 0; n < getAreaLength(); n++ )
      {
        diversityScore += locationCoverage[m][n];
      }
    }
     
    if( sizeOfSet != 0.0 )
    { 
      double overlapOfMaxSet = 100.0 * (1.0 - diversityScore/(sizeOfSet*16.0)); // this factor of 16 makes up for 4 sq meters to each byte in packet size
      double coverageOfMaxSet = 100.0 * (diversityScore/(double)(getAreaWidth()*getAreaLength()));

      setSumOverlapOfMaxSets( getSumOverlapOfMaxSets() + overlapOfMaxSet );
      setSumCoverageOfMaxSets( getSumCoverageOfMaxSets() + coverageOfMaxSet );
    }

    return maxDivDataSetNum;
  }

// /**
// FUNCTION: FindApproxGreedyVQProblem
// LAYER   : NETWORK
// PURPOSE : Called from complete time slot function to determine the set of packets with the most coverage
//            within the time and power budget constraints according to the greedy approximation algorithm that
//            that chooses the set according to the highest diversity - virt_queues*power_cost
// PARAMETERS: none
//            
// RETURN   ::int: number that represents the binary vector of which items should be sent
// **/
  int
  MaxDivSched::FindApproxGreedyVQMaxDivSet( )
  {
    int i, j, m, n;

    if( MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " in MaxDivSched::FindApproxGreedyVQMaxDivSet()\n";
    }
    Time timeCost, timeCostSoFar, timeCostOfMax;
    double powerCost, powerCostForNode[getNumNodes()], powerCostOfMax;
    for( i = 0; i < getNumNodes(); i++ )
    {
      powerCostForNode[i] = 0.0;
    }
    double sizeOfSet;
    int numItemsInSet = 0;
    double overlapOfChosenSet;
    double coverageOfChosenSet;

    // calculate coverage for all possible data item trx schemes
    int maxDivDataSetNum = 0;
    double maxScore;
    int maxAddNodeIndex, maxAddItemIndex;

    for( i = 0; i < (int)getNumNodes(); i++ )
    {
      powerCostForNode[i] = 0.0;
      for( j = 0; j < (int)areaCoverageInfo[i].size(); j++ )
      {
        areaCoverageInfo[i][j].check = true;
      }
    }
    // reset location coverage
    for( m = 0; m < getAreaWidth(); m++ )
    {
      for( n = 0; n < getAreaLength(); n++ )
      {
        locationCoverage[m][n] = 0.0;
      }
    }
   
    bool moreToCheck = true;
    while( moreToCheck ) // need to break this loop only when we're out of data items to be added
    { 
      if( MAX_DIV_SCHED_APPROX_ONE_SHOT_DEBUG )
      {
        std::cout<<"\tLooking to add item number " << numItemsInSet+1 << ":\n";
      }
      maxScore = -1.0;
      moreToCheck = false;
      maxAddNodeIndex = -1;
      maxAddItemIndex = -1;

      // find item with highest additional coverage that doesn't exceed time or cost budgets
      for( i = 0; i < (int)getNumNodes(); i++ )
      {
        for( j = 0; j < (int)areaCoverageInfo[i].size(); j++ )
        {
          if( areaCoverageInfo[i][j].check )
          {
            moreToCheck = true;
            timeCost = MicroSeconds((uint64_t)(((double)areaCoverageInfo[i][j].size*8.0)/getGlobalChannelRates(i, 0))); // size changed to bits, channel rate given in Mbps, so result is in MicroSeconds 
            powerCost = MDS_TRX_POWER_MW*(timeCost.GetSeconds()); // units are milliwatt-seconds /3600.0); // units are milliwatts and hours
            if( timeCostSoFar + timeCost > getTimeBudget() )
            {
              // this data item would put set over time budget limit...eliminate it from contention and continue
              areaCoverageInfo[i][j].check = false;
              continue;
            }
            if( powerCostForNode[i] + powerCost > getNodePowerBudget() )
            {
              // this data item would put set over time budget limit...eliminate it from contention and continue
              areaCoverageInfo[i][j].check = false;
              continue;
            }
          }
          else
          {
            continue;
          }

          if( MAX_DIV_SCHED_APPROX_ONE_SHOT_DEBUG )
          {
            std::cout<<"\t\tChecking item " << j << " in queue of node " << i << "\n";
          }

          // item has not been added to set yet and does not violate any budgets, check if it is max equaiton value
          double additionalCov = 0.0; 
          int xIndex = areaCoverageInfo[i][j].x;
          int yIndex = areaCoverageInfo[i][j].y;
          for( m = 0; m < areaCoverageInfo[i][j].xLength; m++ )
          {
            for( n = 0; n < areaCoverageInfo[i][j].yLength; n++ )
            {
              if( locationCoverage[xIndex+m][yIndex+n] == 0.0 )
              {
                additionalCov++;
              }
            }
          }
          // now calculate virtual queue * cost product sum
         // double costProductSum = 0.0;
         // for( m = 0; m < getNumNodes(); m++ )
         // {
         //   costProductSum += virtPowerQueue[m]*powerCostForNode[m];
         // }

         // double score = additionalCov - (double)getV()*costProductSum;
          double score = additionalCov - (double)getV()*virtPowerQueue[i]*powerCostForNode[i];

          if( score > maxScore )
          {
            maxScore = score;
            maxAddNodeIndex = i;
            maxAddItemIndex = j;
            timeCostOfMax = timeCost;
            powerCostOfMax = powerCost;
          }
        }
      }
      if( !moreToCheck )
      {
        continue;
      }

      if( maxAddNodeIndex == -1 || maxAddItemIndex == -1 )
      {  
        // didn't find any items to add within budgets
        break;
      }
     
      // otherwise, add item with greatest additional coverage
      int dataItemIndex = 0;
      for( i = 0; i < maxAddNodeIndex; i++ )
      {
        for( j = 0; j < (int)areaCoverageInfo[i].size(); j++ )
        {
          dataItemIndex++;
        }
      }
      dataItemIndex += maxAddItemIndex + 1;
      maxDivDataSetNum = maxDivDataSetNum|((unsigned)1<<dataItemIndex);
      powerCostForNode[maxAddNodeIndex] += powerCostOfMax;
      timeCostSoFar += timeCostOfMax;
      sizeOfSet += (double)areaCoverageInfo[maxAddNodeIndex][maxAddItemIndex].size;
      areaCoverageInfo[maxAddNodeIndex][maxAddItemIndex].check = false;
      numItemsInSet++;

      if( MAX_DIV_SCHED_APPROX_ONE_SHOT_DEBUG )
      {
        std::cout<<"\tMax item is " << maxAddItemIndex << " in queue of node " << maxAddNodeIndex << "...data item index = " << dataItemIndex << "\n";
      }

      // update location coverage
      int xIndex = areaCoverageInfo[maxAddNodeIndex][maxAddItemIndex].x;
      int yIndex = areaCoverageInfo[maxAddNodeIndex][maxAddItemIndex].y;
      for( m = 0; m < areaCoverageInfo[maxAddNodeIndex][maxAddItemIndex].xLength; m++ )
      {
        for( n = 0; n < areaCoverageInfo[maxAddNodeIndex][maxAddItemIndex].yLength; n++ )
        {
          locationCoverage[xIndex+m][yIndex+n] = 1.0;
        }
      }
    }
    // now get diversity score by summing over entire location coverage area
    double diversityScore = 0.0;
    for( m = 0; m < getAreaWidth(); m++ )
    {
      for( n = 0; n < getAreaLength(); n++ )
      {
        diversityScore += locationCoverage[m][n];
      }
    }
    // calculate percent overlap = 100 * (coverage/size)
    //   and coverage = coverage/totalarea
    if( sizeOfSet != 0.0 )
    {
      overlapOfChosenSet = 100.0 * (1.0 - diversityScore/(sizeOfSet*16.0)); // this factor of 16 makes up for 4 sq meters to each byte in packet size
      coverageOfChosenSet = 100.0 * (diversityScore/(double)(getAreaWidth()*getAreaLength()));
    }

    setSumOverlapOfChosenSets( getSumOverlapOfChosenSets() + overlapOfChosenSet );
    setSumCoverageOfChosenSets( getSumCoverageOfChosenSets() + coverageOfChosenSet );
    
    // find coverage and overlap of entire data set for reference 
    sizeOfSet = 0.0;
    for( i = 0; i < (int)getNumNodes(); i++ )
    {
      for( j = 0; j < (int)areaCoverageInfo[i].size(); j++ )
      {
        // update location coverage
        int xIndex = areaCoverageInfo[i][j].x;
        int yIndex = areaCoverageInfo[i][j].y;
        for( m = 0; m < areaCoverageInfo[i][j].xLength; m++ )
        {
          for( n = 0; n < areaCoverageInfo[i][j].yLength; n++ )
          {
            locationCoverage[xIndex+m][yIndex+n] = 1.0;
          }
        }
        sizeOfSet += (double)areaCoverageInfo[i][j].size;
      }
    }
    diversityScore = 0.0;
    for( m = 0; m < getAreaWidth(); m++ )
    {
      for( n = 0; n < getAreaLength(); n++ )
      {
        diversityScore += locationCoverage[m][n];
      }
    }
     
    if( sizeOfSet != 0.0 )
    { 
      double overlapOfMaxSet = 100.0 * (1.0 - diversityScore/(sizeOfSet*16.0)); // this factor of 16 makes up for 4 sq meters to each byte in packet size
      double coverageOfMaxSet = 100.0 * (diversityScore/(double)(getAreaWidth()*getAreaLength()));

      setSumOverlapOfMaxSets( getSumOverlapOfMaxSets() + overlapOfMaxSet );
      setSumCoverageOfMaxSets( getSumCoverageOfMaxSets() + coverageOfMaxSet );
    }

    return maxDivDataSetNum;
  }
// /**
// FUNCTION: FindLongTermAvgMaxDivSet
// LAYER   : NETWORK
// PURPOSE : Called from complete time slot function to determine the set of packets to transmit
//            within the time and power budget constraints by choosing the set with the highest 
//            value of the equation:  Coverage - V * sum_nodes(virt. q's * power cost)
// PARAMETERS: none
//            
// RETURN   ::int: number that represents the binary vector of which items should be sent
// **/
  uint64_t
  MaxDivSched::FindLongTermAvgMaxDivSet( )
  {
    if( MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " in MaxDivSched::FindLongTermAvgMaxDivSet()\n";
    }

    uint64_t i;
    int j, k, m, n;

    Time timeCost, timeCostOfSet;
    double powerCost, powerCostForNode[getNumNodes()]; //, powerCostForNodeOfChosenSet[getNumNodes()];
    for( j = 0; j < getNumNodes(); j++ )
    {
      powerCostForNode[j] = 0.0;
    }
    double sizeOfSet;
    double overlapOfSet, overlapOfChosenSet;
    double coverageOfSet, coverageOfChosenSet;

    // calculate coverage for all possible data item trx schemes
    double numPossDataSets = pow( ((double)2), getTotalNumDataItems() );
    int dataItemIndex;
    double maxDivScore = -1;
    uint64_t maxDivDataSetNum = -1;
    
    // for speed of computation, we keep track of sets that exceed time or power constraints
    //   and eliminate supersets of these from being considered
    vector<uint64_t> illegalSets;
    int numSkipped = 0;
    
    if( MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG )
    {
      std::cout<<"\tnumPossDataSets = " << numPossDataSets << "\n";
    }


    // check each possible set of items
    for( i = 0; i < (uint64_t)numPossDataSets; i++ )
    {
      timeCostOfSet = Seconds(0.0);
      sizeOfSet = 0.0;
      overlapOfSet = 0.0;
      coverageOfSet = 0.0;
      if( MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG )
      {
        std::cout<< "\tData set # " << i << ": ";
      }

      // check to see if this set already has a subset that is over time or power budget
      //   before calculating coverage
      for( j = 0; j < (int)illegalSets.size(); j++ )
      {
        if( (illegalSets[j] & i) == illegalSets[j] )
        {
          numSkipped++;
          continue; 
        }
      }
      // intitialize/reset location coverage area
      for( m = 0; m < getAreaWidth(); m++ )
      {
        for( n = 0; n < getAreaLength(); n++ )
        {
          locationCoverage[m][n] = 0.0;
        }
      }
      // calculate coverage score 
      unsigned dataSet = (unsigned)i; 
      dataItemIndex = 0;
      for( j = 0; j < (int)getNumNodes(); j++ )
      {
        powerCostForNode[j] = 0.0;
        for( k = 0; k < (int)areaCoverageInfo[j].size(); k++ )
        {
          if( ((dataSet>>dataItemIndex)&(unsigned)1) == (unsigned)1 ) // data item is included in this dataSet
          {
            // need to set coverage of dataItem
            int xIndex = areaCoverageInfo[j][k].x;
            int yIndex = areaCoverageInfo[j][k].y;

            // update cost of set
            timeCost = MicroSeconds((uint64_t)(((double)areaCoverageInfo[j][k].size*8.0)/getGlobalChannelRates(j, 0))); // size changed to bits, channel rate given in Mbps, so result is in MicroSeconds 
            if( MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG )
            {
              std::cout<<"Time cost of data item with size " << areaCoverageInfo[j][k].size << " at rate ";
              std::cout<< getGlobalChannelRates(j, 0) << " = " << timeCost.GetMicroSeconds() << " in microseconds\n";
            }
            timeCostOfSet += timeCost;
           
            powerCost = MDS_TRX_POWER_MW*(timeCost.GetSeconds()); // units are milliwatt-seconds /3600.0); // units are milliwatts and hours
            powerCostForNode[j] += powerCost; 

            sizeOfSet += (double)areaCoverageInfo[j][k].size;
            
            if( MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG )
            {
              std::cout<<"Power cost of data item with size " << areaCoverageInfo[j][k].size << " at rate ";
              std::cout<< getGlobalChannelRates(j, 0) << " = " << powerCost << " in milliwatt-seconds\n";
            }
          
            if( MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG )
            {
              std::cout<< "(" << j << "," << k << ")  ";
              std::cout<<" [" << xIndex <<"," << yIndex << "," << areaCoverageInfo[j][k].xLength << "," << areaCoverageInfo[j][k].yLength << "] ";
            }

            for( m = 0; m < areaCoverageInfo[j][k].xLength; m++ )
            {
              for( n = 0; n < areaCoverageInfo[j][k].yLength; n++ )
              {
                locationCoverage[xIndex+m][yIndex+n] = 1.0;
              }
            }
          }
          // move to consider if next data item is in this set
          dataItemIndex++;
        }
      }

      // check to make sure that total time to transmit set does not exceed time budget
      //   if it does, throw it out, and move to next set
      bool exceededTimeBudget = false;
      if( timeCostOfSet > timeBudget )
      {
        setNumTimesExceededTimeBudget( getNumTimesExceededTimeBudget() + 1 );
        exceededTimeBudget = true;
      }
      // now get diversity score by summing over entire location coverage area
      double coverageScore = 0.0;
      double diversityScore = 0.0;
      for( m = 0; m < getAreaWidth(); m++ )
      {
        for( n = 0; n < getAreaLength(); n++ )
        {
          coverageScore += locationCoverage[m][n];
        }
      }
      // calculate percent overlap = 100 * (coverage/size)
      if( sizeOfSet != 0.0 )
      {
        overlapOfSet = 100.0 * (1.0 - coverageScore/(sizeOfSet*16.0)); // this factor of 16 makes up for 4 sq meters to each byte in packet size
        coverageOfSet = 100.0 * (coverageScore/(double)(getAreaWidth()*getAreaLength()));
      }
      if( i == (uint64_t)numPossDataSets - 1 )
      {
        setSumOverlapOfMaxSets( getSumOverlapOfMaxSets() + overlapOfSet );
        setSumCoverageOfMaxSets( getSumCoverageOfMaxSets() + coverageOfSet );
      }
      if( exceededTimeBudget )
      {
        illegalSets.push_back(i);
        continue;
      }
      // now calculate virtual queue * cost product sum
      double costProductSum = 0.0;
      for( m = 0; m < getNumNodes(); m++ )
      {
        costProductSum += virtPowerQueue[m]*powerCostForNode[m];
      }
      //std::cout<<"\tset number: " << i << "\n";
      //std::cout<<"\tcoverageScore = " << coverageScore <<"\n";
      //std::cout<<"\tsum of Z*C = " << costProductSum <<"\n";

      // here diversity score is combination of coverage and virtual queue * cost product sum
      diversityScore = coverageScore - (double)getV()*costProductSum;
      //std::cout<<"\tScore = " << diversityScore << "\n\n";
      // check to see if this is max diversity score
      if( diversityScore > maxDivScore )
      {
        maxDivScore = diversityScore;
        maxDivDataSetNum = i;
        overlapOfChosenSet = overlapOfSet;
        coverageOfChosenSet = coverageOfSet;
        //for( j = 0; j < (int)getNumNodes(); j++ )
        //{
          //powerCostForNodeOfChosenSet[j] = powerCostForNode[j];
        //}
      }
      if( MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG )
      {
        std::cout<<"\nScore = " << diversityScore << "\n";
        std::cout<<"Overlap = " << overlapOfSet << "\n";
        std::cout<<"Size of set = " << sizeOfSet << "\n";
      }
    }
    //std::cout<<"\tChose set number: " << maxDivDataSetNum << "\n";
    setSumOverlapOfChosenSets( getSumOverlapOfChosenSets() + overlapOfChosenSet );
    setSumCoverageOfChosenSets( getSumCoverageOfChosenSets() + coverageOfChosenSet );
    setNumSetsSkipped( getNumSetsSkipped() + numSkipped );

    //std::cout<<"Number skipped = " << numSkipped << "\n";

    return maxDivDataSetNum;
  }

// /**
// FUNCTION: FindApproxMaxRatioMaxDivSet
// LAYER   : NETWORK
// PURPOSE : Called from complete time slot function to determine the set of packets with the most coverage
//            within the time and power budget constraints according to the greedy approximation algorithm
//            that looks at the ratio of coverage over power
// PARAMETERS: none
//            
// RETURN   ::int: number that represents the binary vector of which items should be sent
// **/
  int
  MaxDivSched::FindApproxMaxRatioMaxDivSet( )
  {
    if( MAX_DIV_SCHED_APPROX_MAX_RATIO_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " in MaxDivSched::FindApproxMaxRatioMaxDivSet()\n";
    }
    
    int i, j, m, n;

    Time timeCost, timeCostSoFar, timeCostOfMax;
    double powerCost, powerCostForNode[getNumNodes()], powerCostOfMax;
    for( i = 0; i < getNumNodes(); i++ )
    {
      powerCostForNode[i] = 0.0;
    }
    double sizeOfSet;
    int numItemsInSet = 0;
    double overlapOfChosenSet;
    double coverageOfChosenSet;

    // calculate coverage for all possible data item trx schemes
    int maxDivDataSetNum = 0;
    double maxRatio;
    int maxRatioNodeIndex, maxRatioItemIndex;

    for( i = 0; i < (int)getNumNodes(); i++ )
    {
      powerCostForNode[i] = 0.0;
      for( j = 0; j < (int)areaCoverageInfo[i].size(); j++ )
      {
        areaCoverageInfo[i][j].check = true;
      }
    }
    // reset location coverage
    for( m = 0; m < getAreaWidth(); m++ )
    {
      for( n = 0; n < getAreaLength(); n++ )
      {
        locationCoverage[m][n] = 0.0;
      }
    }
   
    bool moreToCheck = true;
    while( moreToCheck ) // need to break this loop only when we're out of data items to be added
    { 
      if( MAX_DIV_SCHED_APPROX_MAX_RATIO_DEBUG )
      {
        std::cout<<"\tLooking to add item number " << numItemsInSet+1 << ":\n";
      }
      maxRatio = -1.0;
      moreToCheck = false;
      maxRatioNodeIndex = -1;
      maxRatioItemIndex = -1;

      // find item with highest additional coverage that doesn't exceed time or cost budgets
      for( i = 0; i < (int)getNumNodes(); i++ )
      {
        for( j = 0; j < (int)areaCoverageInfo[i].size(); j++ )
        {
          if( areaCoverageInfo[i][j].check )
          {
            moreToCheck = true;
            timeCost = MicroSeconds((uint64_t)(((double)areaCoverageInfo[i][j].size*8.0)/getGlobalChannelRates(i, 0))); // size changed to bits, channel rate given in Mbps, so result is in MicroSeconds 
            powerCost = MDS_TRX_POWER_MW*(timeCost.GetSeconds()); // units are milliwatt-seconds /3600.0); // units are milliwatts and hours
            if( timeCostSoFar + timeCost > getTimeBudget() )
            {
              // this data item would put set over time budget limit...eliminate it from contention and continue
              areaCoverageInfo[i][j].check = false;
              continue;
            }
            if( powerCostForNode[i] + powerCost > getNodePowerBudget() )
            {
              // this data item would put set over time budget limit...eliminate it from contention and continue
              areaCoverageInfo[i][j].check = false;
              continue;
            }
          }
          else
          {
            continue;
          }

          if( MAX_DIV_SCHED_APPROX_MAX_RATIO_DEBUG )
          {
            std::cout<<"\t\tChecking item " << j << " in queue of node " << i << "\n";
          }

          // item has not been added to set yet and does not violate any budgets, check if it is max additional coverage
          double additionalCov = 0.0; 
          int xIndex = areaCoverageInfo[i][j].x;
          int yIndex = areaCoverageInfo[i][j].y;
          for( m = 0; m < areaCoverageInfo[i][j].xLength; m++ )
          {
            for( n = 0; n < areaCoverageInfo[i][j].yLength; n++ )
            {
              if( locationCoverage[xIndex+m][yIndex+n] == 0.0 )
              {
                additionalCov++;
              }
            }
          }
          if( additionalCov/powerCost > maxRatio )
          {
            maxRatio = additionalCov/powerCost;
            maxRatioNodeIndex = i;
            maxRatioItemIndex = j;
            timeCostOfMax = timeCost;
            powerCostOfMax = powerCost;
          }
        }
      }
      if( !moreToCheck )
      {
        continue;
      }

      if( maxRatioNodeIndex == -1 || maxRatioItemIndex == -1 )
      {
        // couldn't find any items to add within budgets
        break;
      }

      // add item with greatest additional coverage
      int dataItemIndex = 0;
      for( i = 0; i < maxRatioNodeIndex; i++ )
      {
        for( j = 0; j < (int)areaCoverageInfo[i].size(); j++ )
        {
          dataItemIndex++;
        }
      }
      dataItemIndex += maxRatioItemIndex + 1;
      maxDivDataSetNum = maxDivDataSetNum|((unsigned)1<<dataItemIndex);
      powerCostForNode[maxRatioNodeIndex] += powerCostOfMax;
      timeCostSoFar += timeCostOfMax;
      sizeOfSet += (double)areaCoverageInfo[maxRatioNodeIndex][maxRatioItemIndex].size;
      areaCoverageInfo[maxRatioNodeIndex][maxRatioItemIndex].check = false;
      numItemsInSet++;

      if( MAX_DIV_SCHED_APPROX_MAX_RATIO_DEBUG )
      {
        std::cout<<"\tMax item is " << maxRatioItemIndex << " in queue of node " << maxRatioNodeIndex << "...data item index = " << dataItemIndex << "\n";
      }

      // update location coverage
      int xIndex = areaCoverageInfo[maxRatioNodeIndex][maxRatioItemIndex].x;
      int yIndex = areaCoverageInfo[maxRatioNodeIndex][maxRatioItemIndex].y;
      for( m = 0; m < areaCoverageInfo[maxRatioNodeIndex][maxRatioItemIndex].xLength; m++ )
      {
        for( n = 0; n < areaCoverageInfo[maxRatioNodeIndex][maxRatioItemIndex].yLength; n++ )
        {
          locationCoverage[xIndex+m][yIndex+n] = 1.0;
        }
      }
    }
    // now get diversity score by summing over entire location coverage area
    double diversityScore = 0.0;
    for( m = 0; m < getAreaWidth(); m++ )
    {
      for( n = 0; n < getAreaLength(); n++ )
      {
        diversityScore += locationCoverage[m][n];
      }
    }
    // calculate percent overlap = 100 * (coverage/size)
    //   and coverage = coverage/totalarea
    if( sizeOfSet != 0.0 )
    {
      overlapOfChosenSet = 100.0 * (1.0 - diversityScore/(sizeOfSet*16.0)); // this factor of 16 makes up for 4 sq meters to each byte in packet size
      coverageOfChosenSet = 100.0 * (diversityScore/(double)(getAreaWidth()*getAreaLength()));
    }

    setSumOverlapOfChosenSets( getSumOverlapOfChosenSets() + overlapOfChosenSet );
    setSumCoverageOfChosenSets( getSumCoverageOfChosenSets() + coverageOfChosenSet );

    // find coverage and overlap of entire data set for reference 
    sizeOfSet = 0.0;
    for( i = 0; i < (int)getNumNodes(); i++ )
    {
      for( j = 0; j < (int)areaCoverageInfo[i].size(); j++ )
      {
        // update location coverage
        int xIndex = areaCoverageInfo[i][j].x;
        int yIndex = areaCoverageInfo[i][j].y;
        for( m = 0; m < areaCoverageInfo[i][j].xLength; m++ )
        {
          for( n = 0; n < areaCoverageInfo[i][j].yLength; n++ )
          {
            locationCoverage[xIndex+m][yIndex+n] = 1.0;
          }
        }
        sizeOfSet += (double)areaCoverageInfo[i][j].size;
      }
    }
    diversityScore = 0.0;
    for( m = 0; m < getAreaWidth(); m++ )
    {
      for( n = 0; n < getAreaLength(); n++ )
      {
        diversityScore += locationCoverage[m][n];
      }
    }
     
    if( sizeOfSet != 0.0 )
    { 
      double overlapOfMaxSet = 100.0 * (1.0 - diversityScore/(sizeOfSet*16.0)); // this factor of 16 makes up for 4 sq meters to each byte in packet size
      double coverageOfMaxSet = 100.0 * (diversityScore/(double)(getAreaWidth()*getAreaLength()));

      setSumOverlapOfMaxSets( getSumOverlapOfMaxSets() + overlapOfMaxSet );
      setSumCoverageOfMaxSets( getSumCoverageOfMaxSets() + coverageOfMaxSet );
    }

    return maxDivDataSetNum;
  }

// /**
// FUNCTION: FindRandomMaxDivSet
// LAYER   : NETWORK
// PURPOSE : Called from complete time slot function to determine the set of packets with the most coverage
//            within the time and power budget constraints.
// PARAMETERS: none
//            
// RETURN   ::int: number that represents the binary vector of which items should be sent
// **/
  uint64_t
  MaxDivSched::FindRandomMaxDivSet( )
  {
    int i, j, m, n;

    if( MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " in MaxDivSched::FindRandomMaxDivSet()\n";
    }
    Time timeCost;
    Time timeCostSoFar = Seconds(0);
    double powerCost, powerCostForNode[getNumNodes()];
    double sizeOfSet = 0;
    int numItemsInSet = 0;
    double overlapOfChosenSet = 0.0;
    double coverageOfChosenSet = 0.0;

    // calculate coverage for all possible data item trx schemes
    int maxDivDataSetNum = 0;

    for( i = 0; i < (int)getNumNodes(); i++ )
    {
      powerCostForNode[i] = 0.0;
    }
    // reset location coverage
    for( m = 0; m < getAreaWidth(); m++ )
    {
      for( n = 0; n < getAreaLength(); n++ )
      {
        locationCoverage[m][n] = 0.0;
      }
    }
   
    // find next item that doesn't exceed time or cost budgets
    for( i = 0; i < (int)getNumNodes(); i++ )
    {
      for( j = 0; j < (int)areaCoverageInfo[i].size(); j++ )
      {
        timeCost = MicroSeconds((uint64_t)(((double)areaCoverageInfo[i][j].size*8.0)/getGlobalChannelRates(i, 0))); // size changed to bits, channel rate given in Mbps, so result is in MicroSeconds 
        powerCost = MDS_TRX_POWER_MW*(timeCost.GetSeconds()); // units are milliwatt-seconds /3600.0); // units are milliwatts and hours
        if( timeCostSoFar + timeCost > getTimeBudget() )
        {
          // this data item would put set over time budget limit...eliminate it from contention and break both loops
          i = getNumNodes();
          break;
        }
        if( powerCostForNode[i] + powerCost > getNodePowerBudget() )
        {
          // this data item would put set over time budget limit...break this loop and go on to next node
          break;
        }

        if( MAX_DIV_SCHED_APPROX_ONE_SHOT_DEBUG )
        {
          std::cout<<"\t\tChecking item " << j << " in queue of node " << i << "\n";
        }

        // item does not violate any budgets, add it to the set
        int dataItemIndex = 0;
        for( m = 0; m < i; m++ )
        {
          for( n = 0; n < (int)areaCoverageInfo[m].size(); n++ )
          {
            dataItemIndex++;
          }
        }
        dataItemIndex += j + 1;
        maxDivDataSetNum = maxDivDataSetNum|((unsigned)1<<dataItemIndex);
        powerCostForNode[i] += powerCost;
        timeCostSoFar += timeCost;
        sizeOfSet += (double)areaCoverageInfo[i][j].size;
        numItemsInSet++;

        if( MAX_DIV_SCHED_FIND_MAX_DIV_SET_DEBUG )
        {
          std::cout<<"\tAdding item " << i << " in queue of node " << j << "...data item index = " << dataItemIndex << "\n";
        }

        // update location coverage
        int xIndex = areaCoverageInfo[i][j].x;
        int yIndex = areaCoverageInfo[i][j].y;
        for( m = 0; m < areaCoverageInfo[i][j].xLength; m++ )
        {
          for( n = 0; n < areaCoverageInfo[i][j].yLength; n++ )
          {
            locationCoverage[xIndex+m][yIndex+n] = 1.0;
          }
        }
      }
    }

    // now get diversity score by summing over entire location coverage area
    double diversityScore = 0.0;
    for( m = 0; m < getAreaWidth(); m++ )
    {
      for( n = 0; n < getAreaLength(); n++ )
      {
        diversityScore += locationCoverage[m][n];
      }
    }
    // calculate percent overlap = 100 * (coverage/size)
    //   and coverage = coverage/totalarea
    if( sizeOfSet != 0.0 )
    {
      overlapOfChosenSet = 100.0 * (1.0 - diversityScore/(sizeOfSet*16.0)); // this factor of 16 makes up for 4 sq meters to each byte in packet size
      coverageOfChosenSet = 100.0 * (diversityScore/(double)(getAreaWidth()*getAreaLength()));
    }

    setSumOverlapOfChosenSets( getSumOverlapOfChosenSets() + overlapOfChosenSet );
    setSumCoverageOfChosenSets( getSumCoverageOfChosenSets() + coverageOfChosenSet );
    
        for( m = 0; m < getAreaWidth(); m++ )
        {
          for( n = 0; n < getAreaLength(); n++ )
          {
            locationCoverage[m][n] = 0.0;
          }
        }
    // find coverage and overlap of entire data set for reference 
    sizeOfSet = 0.0;
    for( i = 0; i < (int)getNumNodes(); i++ )
    {
      for( j = 0; j < (int)areaCoverageInfo[i].size(); j++ )
      {
        // update location coverage
        int xIndex = areaCoverageInfo[i][j].x;
        int yIndex = areaCoverageInfo[i][j].y;
        for( m = 0; m < areaCoverageInfo[i][j].xLength; m++ )
        {
          for( n = 0; n < areaCoverageInfo[i][j].yLength; n++ )
          {
            locationCoverage[xIndex+m][yIndex+n] = 1.0;
          }
        }
        sizeOfSet += (double)areaCoverageInfo[i][j].size;
      }
    }
    diversityScore = 0.0;
    for( m = 0; m < getAreaWidth(); m++ )
    {
      for( n = 0; n < getAreaLength(); n++ )
      {
        diversityScore += locationCoverage[m][n];
      }
    }
     
    if( sizeOfSet != 0.0 )
    { 
      double overlapOfMaxSet = 100.0 * (1.0 - diversityScore/(sizeOfSet*16.0)); // this factor of 16 makes up for 4 sq meters to each byte in packet size
      double coverageOfMaxSet = 100.0 * (diversityScore/(double)(getAreaWidth()*getAreaLength()));

      setSumOverlapOfMaxSets( getSumOverlapOfMaxSets() + overlapOfMaxSet );
      setSumCoverageOfMaxSets( getSumCoverageOfMaxSets() + coverageOfMaxSet );
    }

    return maxDivDataSetNum;
  }

  // Functions overwritten from Ipv4RoutingProtocol:

  // /**
  // FUNCTION: RouteOutput
  // PURPOSE : Called to retrieve a route for a packet that originates in the node and is attempting 
  //            to be sent out an interface.  In dist univ sched protocol, this happens in different cases:
  //            1) Data packet from application - these packets are given the loopback route, so they are directed
  //                back to the node and picked up in the route input function, which queues them
  //            2) Control packet from protocol - these packets are provided with a route sending it 
  //                out the interface with the broadcast address
  //            3) Data packet from Send Data Packet function called at end of time slots. These packets are provided
  //                a route to send them out the interface with the correct next hop
  //            4) Data Ack packet from Send Data Ack Packet function
  // PARAMETERS:
  // + p : Ptr<Packet> : pointer to packet being routed 
  // + header : const Ipv4Header & : Header of packet
  // + oif : Ptr<NetDevice> : pointer to outgoing interface packet is being sent on
  // + sockerr : SocketErrno : place to return error value to socket (I think)
  // RETURN   ::void:NULL
  // **/ 
  Ptr<Ipv4Route> 
  MaxDivSched::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
  {
    if( MAX_DIV_SCHED_ROUTE_OUTPUT_DEBUG )
    {
      std::cout<< "Node " << getNodeId() << " in RouteOutput()\n";
    }

    if (!p)
    {
      if( MAX_DIV_SCHED_ROUTE_OUTPUT_DEBUG )
      {
        std::cout<<"\tno packet...calling LoopbackRoute()...ERROR?\n";
      }
      return LoopbackRoute (header, oif); // later
    }
    if (m_socketAddresses.empty ())
    {
      sockerr = Socket::ERROR_NOROUTETOHOST;
      std::cout<<"ERROR:  No distUnivSched interfaces";
      Ptr<Ipv4Route> route = NULL;
      return route;
    }

    TypeHeader tHeader (OTHER);
    p->RemoveHeader (tHeader);

    DistUnivSchedPacketTypeTag typeTag (OTHER);
    if( p->PeekPacketTag (typeTag) )
    {
      // This should be a DATA_ACK packet.  It is just looking for the interface 
      //  on which it should be sent, so we set this up and pass back the 'route'.
      p->RemovePacketTag(typeTag);
      if( typeTag.type != DUS_DATA_ACK )
      {
        std::cout<<"Node " << getNodeId() << ":  Expecting a DUS_DATA_ACK Packet tag here, but did not get it...exiting\n";
        exit(-1);
      }
      p->AddPacketTag(typeTag);

      if( MAX_DIV_SCHED_ROUTE_OUTPUT_DEBUG )
      {
        std::cout<<"FOUND packet type tag = ";
        typeTag.Print(std::cout);
        std::cout<<"\tthis should be a DATA_ACK packet.\n";
      }

        // need to attach header 
        TypeHeader tHeader2 (DUS_DATA_ACK);
        p->AddHeader( tHeader2 );
      
        Ipv4Address nextHop = header.GetDestination();
      
        if( MAX_DIV_SCHED_ROUTE_OUTPUT_DEBUG )
        {
          std::cout<<"\tNext Hop Address (Destination) = ";
          nextHop.Print( std::cout );
          std::cout<<"\n";
        }

        DistUnivSchedDestTag tag (-1);

        if (p->RemovePacketTag (tag))
        {
          int dst_index = tag.dest;
          int next_hop_index = (nextHop.Get()&(uint32_t)255) - 1;
          if( MAX_DIV_SCHED_ROUTE_OUTPUT_DEBUG )
          {
            std::cout<<"\tDestination Index = " << dst_index << "\n";
            std::cout<<"\tNext Hop Index = " << next_hop_index << "\n";
          }
          p->AddPacketTag (tag);
        }
      
        // return a route so the packet goes out the interface from here
        Ptr<Ipv4Route> route = getOutputRoutePointer(); // new Ipv4Route;
        Ipv4Address src; 
        std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); 

        Ipv4InterfaceAddress iface = j->second;
        src = iface.GetLocal();

        sockerr = Socket::ERROR_NOTERROR;

        if( MAX_DIV_SCHED_ROUTE_OUTPUT_DEBUG )
        {
          std::cout<<"  packet in RouteOutput - Next Hop address: ";
          nextHop.Print( std::cout );
          std::cout<<" - Source (local) address: ";
          src.Print( std::cout );
          std::cout<<"\n";
        }

        Ptr<NetDevice> nd = j->first->GetBoundNetDevice();
         
        route->SetDestination( nextHop );
        route->SetSource( src );
        route->SetGateway( nextHop );
        if( nd != 0 )
        { 
          route->SetOutputDevice( nd );
        }
        else
        {
          route->SetOutputDevice( oif );
        }

        return route;
    }

    if (!tHeader.IsValid ())
    {
      // packet goes from application to route output looking for a route
      //   here, we redirect to route input by giving the loopback addr

      if( getBatteryPowerLevel() < 0.0 )
      {
        // if the node has no battery power left, we drop all traffic from application to simulate it being dead
        //   we need it to continue operating to exchange the global information for the protocol simulation, though
      
        // keep updating the numb packet from application stat to output the correct input rate (before dying)
        setNumPacketsFromApplication ( getNumPacketsFromApplication() + 1 );
        setNumPacketsFromApplicationThisSlot ( getNumPacketsFromApplicationThisSlot() + 1 );
        return 0;
      }
      if( MAX_DIV_SCHED_ROUTE_OUTPUT_DEBUG )
      {
        std::cout<<"\tData Packet...attaching a DUS_DATA Header.\n";
        std::cout<<"\tthis should be a DATA packet from the application.\n";
      }
     
      AreaCoverageTag areaCovTag( 0, 0, 0, 0, 0 );
      GenerateRandomAreaCoverageInfo( &areaCovTag.x, &areaCovTag.y, &areaCovTag.xLength, &areaCovTag.yLength, &areaCovTag.size );

      if( MAX_DIV_SCHED_ROUTE_OUTPUT_DEBUG )
      {
        std::cout<<"Adding areaCoverageTag to packet in RouteOutput: x = " << areaCovTag.x << ", y = " << areaCovTag.y << ", ";
        std::cout<<"xLength = " << areaCovTag.xLength << ", yLength = " << areaCovTag.yLength << " size = " << areaCovTag.size << "\n";
      }
      
      if( !p->PeekPacketTag (areaCovTag) )
      {
        p->AddPacketTag(areaCovTag);
      }
  
      TypeHeader th(DUS_DATA);
      p->AddHeader(th);

      Ipv4Address dst = header.GetDestination();
      int dst_index = (dst.Get()&(uint32_t)255) - 1;

      if( MAX_DIV_SCHED_ROUTE_OUTPUT_DEBUG )
      {
        std::cout<<"\tDestination Index = " << dst_index << "\n";
      }
  
      DistUnivSchedDestTag tag (dst_index);
      if (!p->PeekPacketTag (tag))
      {
        p->AddPacketTag (tag);
      }
     
      DistUnivSchedPacketTypeTag typeTag (DUS_DATA);
    
      if( !p->PeekPacketTag (typeTag) )
      {
        p->AddPacketTag(typeTag);
      }

      // change size of packet to match size determined by area coverage info
      uint32_t packetSize = p->GetSize();
      p->RemoveAtEnd( packetSize - 32 );
      p->AddPaddingAtEnd( MIN(2200, (uint32_t)areaCovTag.size) );

      if( areaCovTag.size > 2200 )
      {
        std::cout<<"WARNING:  Size of data item was too large for packet...truncating size at 2200 bytes.\n";
      }
      
      setNumPacketsFromApplication ( getNumPacketsFromApplication() + 1 );
      setNumPacketsFromApplicationThisSlot ( getNumPacketsFromApplicationThisSlot() + 1 );

      // route to loopback, so we can queue it in route input ftn and send from SendDataPacket() 
      return LoopbackRoute (header, oif);
    }
    else if( tHeader.Get() == DUS_CTRL )
    {
      if( MAX_DIV_SCHED_ROUTE_OUTPUT_DEBUG )
      {
        std::cout<<"\tthis should be CTRL packet from SendControlInfoPacket().\n";
      }
      // need to reattach header if it is a control packet
      p->AddHeader( tHeader );
     
      
      DistUnivSchedPacketTypeTag typeTag (DUS_CTRL);
    
      if( !p->PeekPacketTag (typeTag) )
      {
        p->AddPacketTag(typeTag);
      }
      

      // return a route so the packet goes out the interface from here
      Ptr<Ipv4Route> route = getOutputRoutePointer(); // new Ipv4Route;
      Ipv4Address dst = header.GetDestination();
      Ipv4Address src; 
      std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); 

      Ipv4InterfaceAddress iface = j->second;
      src = iface.GetLocal();

      sockerr = Socket::ERROR_NOTERROR;

      if( MAX_DIV_SCHED_ROUTE_OUTPUT_DEBUG )
      {
        std::cout<<"  packet in RouteOutput - Destination address: ";
        dst.Print( std::cout );
        std::cout<<" - Source address: ";
        src.Print( std::cout );
        std::cout<<"\n";
      }

      Ptr<NetDevice> nd = j->first->GetBoundNetDevice();
      
      route->SetDestination( dst );
      route->SetSource( src );
      route->SetGateway( dst );
      if( nd != 0 )
      {
        route->SetOutputDevice( nd );
      }
      else
      {
        route->SetOutputDevice( oif );
      }

      setNumControlPacketsSent( getNumControlPacketsSent() + 1);

      return route;
    }
    else if( tHeader.Get() == DUS_DATA )
    {
      if( MAX_DIV_SCHED_ROUTE_OUTPUT_DEBUG )
      {
        std::cout<<"\tthis should be a DATA packet from SendDataPacket().\n";
      }

      // need to reattach header if it is a data packet
      p->AddHeader( tHeader );
     
      
      DistUnivSchedPacketTypeTag typeTag (DUS_DATA);
    
      if( !p->PeekPacketTag (typeTag) )
      {
        p->AddPacketTag(typeTag);
      }
      

      Ipv4Address nextHop = header.GetDestination();
      
      if( MAX_DIV_SCHED_ROUTE_OUTPUT_DEBUG )
      {
        std::cout<<"\tNext Hop Address (Destination) = ";
        nextHop.Print( std::cout );
        std::cout<<"\n";
      }

      DistUnivSchedDestTag tag (-1);

      if (p->RemovePacketTag (tag))
      {
        int dst_index = tag.dest;
        int next_hop_index = (nextHop.Get()&(uint32_t)255) - 1;
        if( MAX_DIV_SCHED_ROUTE_OUTPUT_DEBUG )
        {
          std::cout<<"\tDestination Index = " << dst_index << "\n";
          std::cout<<"\tNext Hop Index = " << next_hop_index << "\n";
        }
        p->AddPacketTag (tag);
      }
      
      // return a route so the packet goes out the interface from here
      //Ipv4Route routeObject;
      Ptr<Ipv4Route> route = getOutputRoutePointer(); // = new Ipv4Route;
      Ipv4Address src; 
      std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); 

      Ipv4InterfaceAddress iface = j->second;
      src = iface.GetLocal();

      sockerr = Socket::ERROR_NOTERROR;

      if( MAX_DIV_SCHED_ROUTE_OUTPUT_DEBUG )
      {
        std::cout<<"  packet in RouteOutput - Next Hop address: ";
        nextHop.Print( std::cout );
        std::cout<<" - Source (local) address: ";
        src.Print( std::cout );
        std::cout<<"\n";
      }

      Ptr<NetDevice> nd = j->first->GetBoundNetDevice();
      
      route->SetDestination( nextHop );
      route->SetSource( src );
      route->SetGateway( nextHop );
      if( nd != 0 )
      {
        route->SetOutputDevice( nd );
      }
      else
      {
        route->SetOutputDevice( oif );
      }

      return route;
    }

    return 0;
  }

  // /**
  // FUNCTION: RouteInput
  // PURPOSE : Called to retrieve a route for a packet that arrives at the node's interface from another node or 
  //            from its loopback path.  In this protocol, that can happen in the following ways:
  //            1) Data packet from application - these packets are received here after being sent 'out' from 
  //                the route output function with the loopback address.  These packets are forwarded on to 
  //                recv packet function where they are queued
  //            2) Data packet from another node.  These packets are also forwarded on to the recv packet
  //                functions where they are handled
  // PARAMETERS:
  // + p : Ptr<Packet> : pointer to packet being routed 
  // + header : const Ipv4Header & : Header of packet
  // + idev : Ptr<NetDevice> : pointer to interface on which the packet arrived (I think)
  // + ucb : UnicastForwardCallback : callback function 
  // + mcb : MulticastForwardCallback : callback function
  // + lcb : LocalDeliverCallback : callback function
  // + ecb : ErrorCallback : callback function
  // RETURN   ::bool:false if error
  // **/ 
  bool 
  MaxDivSched::RouteInput  (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb)
  {
    if( MAX_DIV_SCHED_ROUTE_INPUT_DEBUG )
    {
      std::cout<< "Node " << getNodeId() << " in RouteInput()\n";
    }

   // NS_LOG_FUNCTION (this << p->GetUid () << header.GetDestination () << idev->GetAddress ());
    if (m_socketAddresses.empty ())
    {
      printf( "No maxDivSched interfaces\n" );
      return false;
    }
    NS_ASSERT (m_ipv4 != 0);
    NS_ASSERT (p != 0);
    // Check if input device supports IP
    NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
    int32_t iif = m_ipv4->GetInterfaceForDevice (idev);

    Ipv4Address dst = header.GetDestination ();
    Ipv4Address origin = header.GetSource ();

    if( MAX_DIV_SCHED_ROUTE_INPUT_DEBUG )
    {
      std::cout<< "\tpacket destination = ";
      dst.Print( std::cout );
      std::cout<< " source = ";
      origin.Print( std::cout );
      std::cout<<"\n";
    }

    // DUS is not a multicast routing protocol
    if (dst.IsMulticast ())
    {
      std::cout<<"ERROR:  Node " << getNodeId() << " in RouteInput: dst.IsMulticast() returned true...exiting\n";
      exit(-1);
    }

    // Broadcast local delivery/forwarding
    for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
           m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {

      // If Packet is Broadcast, let local delivery handle
      Ipv4InterfaceAddress iface = j->second;
      if (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()) == iif)
        if (dst == iface.GetBroadcast () || dst.IsBroadcast ())
        {
          if( MAX_DIV_SCHED_ROUTE_INPUT_DEBUG )
          {
            std::cout<<"\treceived packet is broadcast\n";
            std::cout<<"\tthis should be a CTRL packet from another node\n";
          }

          Ptr<Packet> packet = p->Copy ();
          if (lcb.IsNull () == false)
          {
            //printf( "Broadcast local delivery to ? (localCallback function is null)\n" ); // << iface.GetLocal ());
            lcb (p, header, iif);
            // Fall through to additional processing
          }
          else
          {
            std::cout<<"ERROR: Unable to deliver packet locally due to null callback...exiting\n";
            exit(-1);
            ecb (p, header, Socket::ERROR_NOROUTETOHOST);
          }
          return true;
        }
    }

    // Packet is unicast...call local callback and handle packet there
    //  local callback is Recv Dist Univ Sched(), which decides if packet has reached destination or needs to be 
    //  queued for forwarding
    if( MAX_DIV_SCHED_ROUTE_INPUT_DEBUG )
    {
      std::cout<<"\treceived packet is unicast...trying to call local callback function\n";
    }
    if (lcb.IsNull () == false)
    {
      lcb (p, header, iif);
      return true;
    }
    else
    {
      std::cout<<"ERROR: in RouteInput() - Local Callback function is NULL.\n";
      exit(-1);
    }

    std::cout<<"ERROR: in RouteInput() - Received packet that is not broadcast and is not destined for this node...shouldn't happen.\n";
    exit(-1);
    return false;
  }

  // /**
  // FUNCTION: NotifyInterfaceUp
  // PURPOSE : Called as part of initialization process...code mostly copied from aodv protocol
  // PARAMETERS:
  // + i : uint32_t : number of interface
  // RETURN   ::void:NULL
  // **/ 
  void 
  MaxDivSched::NotifyInterfaceUp (uint32_t i)
  {
    //std::cout<< "Node " << getNodeId() << " in NotifyInterfaceUp()\n";
    
    Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
    if (l3->GetNAddresses (i) > 1)
    {
      std::cout<<"ERROR:  DUS does not work with more then one address per each interface.\n";
    }
    Ipv4InterfaceAddress iface = l3->GetAddress (i, 0);
    if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
    {
      std::cout<<"\tin NotifyInterfaceUp():  iface.GetLocal() == Ipv4Address(127.0.0.1)...returning\n";
      return;
    }
 
    // Create a socket to listen only on this interface
    Ptr<Socket> socket = Socket::CreateSocket ( GetObject<Node> (), UdpSocketFactory::GetTypeId () );
    NS_ASSERT (socket != 0);
    socket->SetRecvCallback (MakeCallback (&MaxDivSched::RecvPacket, this)); 
    socket->SetDataSentCallback (MakeCallback (&MaxDivSched::DataSent, this) );
    socket->BindToNetDevice (l3->GetNetDevice (i));
    socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), DUS_PORT));
    socket->SetAllowBroadcast (true);
    socket->SetAttribute ("IpTtl", UintegerValue (1));
    m_socketAddresses.insert (std::make_pair (socket, iface));

    // Add local broadcast record to the routing table
    Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));

    Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice> ();
    if (wifi == 0)
      return;
    Ptr<WifiMac> mac = wifi->GetMac ();
    if (mac == 0)
      return;
  }

  // /**
  // FUNCTION: NotifyInterfaceDown
  // PURPOSE : Not sure if this function is ever called...code mostly copied from aodv protocol
  // PARAMETERS:
  // + i : uint32_t : number of interface
  // RETURN   ::void:NULL
  // **/ 
  void 
  MaxDivSched::NotifyInterfaceDown (uint32_t i)
  {
    // Close socket 
    Ptr<Socket> socket = FindSocketWithInterfaceAddress (m_ipv4->GetAddress (i, 0));
    NS_ASSERT (socket);
    socket->Close ();
    m_socketAddresses.erase (socket);
    if (m_socketAddresses.empty ())
    {
      fprintf( stderr, "No dus interfaces" );
      return;
    }
  }

  // /**
  // FUNCTION: NotifyAddAddress
  // PURPOSE : Part of initialization...code mostly copied from aodv protocol
  // PARAMETERS:
  // + i : uint32_t : number of interface
  // RETURN   ::void:NULL
  // **/ 
  void 
  MaxDivSched::NotifyAddAddress (uint32_t i, Ipv4InterfaceAddress address) 
  {
    //std::cout<< "Node " << getNodeId() << " in NotifyAddAddress()\n";

    Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
    if (!l3->IsUp (i))
    {
      return;
    }
    if (l3->GetNAddresses (i) == 1)
    {
      Ipv4InterfaceAddress iface = l3->GetAddress (i, 0);
      Ptr<Socket> socket = FindSocketWithInterfaceAddress (iface);
      if (!socket)
      {
        if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
          return;
        // Create a socket to listen only on this interface
        Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
                                         UdpSocketFactory::GetTypeId ());
        NS_ASSERT (socket != 0);
        socket->SetRecvCallback (MakeCallback (&MaxDivSched::RecvPacket,this));
        socket->BindToNetDevice (l3->GetNetDevice (i));
        // Bind to any IP address so that broadcasts can be received
        socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), DUS_PORT));
        socket->SetAllowBroadcast (true);
        m_socketAddresses.insert (std::make_pair (socket, iface));
      }
    }
  else
    {
       fprintf( stderr, "DistUnivSched does not work with more then one address per each interface. Ignore added address");
    }
  }

  // /**
  // FUNCTION: NotifyRemoveAddress
  // PURPOSE : Not sure of functionality...code mostly copied from aodv protocol
  // PARAMETERS:
  // + i : uint32_t : number of interface
  // RETURN   ::void:NULL
  // **/ 
  void 
  MaxDivSched::NotifyRemoveAddress (uint32_t i, Ipv4InterfaceAddress address) 
  {
    //std::cout<< "Node " << getNodeId() << " in NotifyRemoveAddress()\n";
    
    Ptr<Socket> socket = FindSocketWithInterfaceAddress (address);
    if (socket)
    {
      m_socketAddresses.erase (socket);
      Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
      if (l3->GetNAddresses (i))
      {
        Ipv4InterfaceAddress iface = l3->GetAddress (i, 0);
        // Create a socket to listen only on this interface
        Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
                                                   UdpSocketFactory::GetTypeId ());
        NS_ASSERT (socket != 0);
        socket->SetRecvCallback (MakeCallback (&MaxDivSched::RecvPacket, this));
        // Bind to any IP address so that broadcasts can be received
        socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), DUS_PORT));
        socket->SetAllowBroadcast (true);
        m_socketAddresses.insert (std::make_pair (socket, iface));

      }
      if (m_socketAddresses.empty ())
      {
        return;
      }
    }
    else
    {
      std::cout<<"Remove address not participating in DUS operation\n";
    }
  }
    
  // /**
  // FUNCTION: SetIpv4
  // PURPOSE : Part of initialization...code mostly copied from aodv protocol
  // PARAMETERS:
  // + i : uint32_t : number of interface
  // RETURN   ::void:NULL
  // **/ 
  void 
  MaxDivSched::SetIpv4 (Ptr<Ipv4> ipv4)
  {
    //std::cout<< "Node " << getNodeId() << " in SetIpv4()\n";
    NS_ASSERT (ipv4 != 0);
    NS_ASSERT (m_ipv4 == 0);

    m_ipv4 = ipv4;

    // Create lo route. It is asserted that the only one interface up for now is loopback
    NS_ASSERT (m_ipv4->GetNInterfaces () == 1 && m_ipv4->GetAddress (0, 0).GetLocal () == Ipv4Address ("127.0.0.1"));
    m_lo = m_ipv4->GetNetDevice (0);
    NS_ASSERT (m_lo != 0);

  }

// /**
// FUNCTION: GlobalExchangeControlInfoForward
// LAYER   : NETWORK
// PURPOSE : Function scheduled by simulator to be run.
//            If using global knowledge, it is called right after the start time slot function
//             in the first node initially, which then schedules it with the next until the final
//             node is reached and info is sent backwards with global-exchange-ctrl-info-backward.
//            If not using global knowledge, this is called right after complete-time-slot function
//             to exchange data for comparison
// PARAMETERS:
// +node:Node *::Pointer to node
// +recipient:int:Index of node to which data packet should be sent
// +commodity:int:Index of commodity which is being sent
// RETURN   ::void:NULL
// **/
  void 
  MaxDivSched::GlobalExchangeControlInfoForward( Ptr<Packet> packet )
  {
    if( MAX_DIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " is in MaxDivSched::GlobalExchangeControlInfoForward() at " << Simulator::Now().GetNanoSeconds() <<  " (nanoseconds)\n";

      if( isGLOBAL_KNOWLEDGE() )
      {
        std::cout<<"Using Global Knowledge.\n";
      }
      else
      {
        std::cout<<"Not Using Global Knowledge.\n";
      }
      if( getGeneratedDuplicatePacketsThisSlot() )
      {
        std::cout<<"Did generate duplicate packets already.\n";
      }
      else
      {
        std::cout<<"Did not generate duplicate packets already.\n";
      }
      std::cout<<"Time Slot Duration = " << getTimeSlotDuration().GetSeconds() << ".\n";
    }

    if( !isGLOBAL_KNOWLEDGE() )
    {
      std::cout<<"Global Knowledge is not being used.  MaxDivSched does not support distributed operation yet...exiting.\n";
      exit(-1);
    }
  
    int i, j;
    char *packetPtr, *origPacketPtr;

    int sizeOfPacket = ( sizeof(int)*getNumNodes() +  sizeof(double)*getNumNodes() + sizeof(AreaCoverageInfo)*MAX_DIV_SCHED_MAX_QUEUE_SIZE*getNumNodes() );
                                           //     number of data items         rate available to HQ       areaCoverage structs
    char *tempPacket = (char *)malloc( sizeOfPacket );
    uint32_t packetSize = 0;

    if( packet != 0 )
    {
      packetSize = packet->CopyData( (uint8_t *)tempPacket, (uint32_t)sizeOfPacket );
      packetPtr = tempPacket;
    
      if( MAX_DIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
      {
        std::cout<< "\tcopied " << packetSize <<" bytes from packet into local buffer\n";
      }
    }

    int temp_int;
    double temp_double;
    AreaCoverageInfo temp_areaCoverageInfo;
    // extract area coverage info from other nodes that have already included them
    if( getNodeId() != 0 )
    {
      if( MAX_DIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
      {
        std::cout<<"\tnode " << getNodeId() << ": unpacking control info from other nodes\n";
      }
      for( i = 0; i < getNodeId(); i++ )
      {
        if( MAX_DIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
        {
          std::cout<<"\tgetting info from node " << i << "\n";
        }
          // get number of data items node has (which we will be extracting from packet)
          memcpy(&temp_int, packetPtr, sizeof(int));
          packetPtr += sizeof(int);

          setNumDataItemsInQueue( i, temp_int );

          // get channel rate from node to HQ
          memcpy(&temp_double, packetPtr, sizeof(double));
          packetPtr += sizeof(double);

          setGlobalChannelRates( i, 0, temp_double );
         
          if( isGLOBAL_KNOWLEDGE() )
          {
              // extract area coverage information for node i's queue
              areaCoverageInfo[i].clear();
              if( MAX_DIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
              {
                std::cout<<"\textracting number of data items currently in queue:  " << temp_int << "\n";
                std::cout<<"\textracting channel rate from node " << i << " to HQ:  " << temp_double << "\n";
                std::cout<<"\textracting " << temp_int << " pieces of area coverage info from node " << i << "\n";
              }
              for( j = 0; j < temp_int; j++ )
              {
                if( MAX_DIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
                {
                  std::cout<< "\t\textracting data item " << j << " of node " << i << "\n";
                }
               
                memcpy( &temp_areaCoverageInfo, packetPtr, sizeof(AreaCoverageInfo) ); 
                packetPtr += sizeof(AreaCoverageInfo);

                areaCoverageInfo[i].push_back( temp_areaCoverageInfo );
              }
          }
      }
    } 
    
    free( tempPacket );

    // TODO :: update own value of number of items in queues?

    if( getNodeId()+1 != getNumNodes() )
    {
      // allocate space for a backlog x commodity x node and rate x node x node and coordinate x node
      packetSize = ( sizeof(int)*getNumNodes() +  sizeof(double)*getNumNodes() + sizeof(AreaCoverageInfo)*MAX_DIV_SCHED_MAX_QUEUE_SIZE*getNumNodes() );
   
      origPacketPtr = packetPtr = (char *)malloc(packetSize);

      if( MAX_DIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
      {
        printf("Allocated packet(%i bytes)...packet pointer = %p\n", 
                    (int)(sizeof(int)*getNumNodes() +  sizeof(double)*getNumNodes() + sizeof(AreaCoverageInfo)*MAX_DIV_SCHED_MAX_QUEUE_SIZE*getNumNodes()), packetPtr );
      }

      // load all known backlogs into message (this node and all before it)
      for( i = 0; i < getNodeId()+1; i++ )
      {
        if( isGLOBAL_KNOWLEDGE() )
        {
          temp_int = (int)areaCoverageInfo[i].size();
          memcpy( packetPtr, &temp_int, sizeof(int) );
          packetPtr += sizeof(int);

          temp_double = getGlobalChannelRates( i, 0 );
          memcpy( packetPtr, &temp_double, sizeof(double) );
          packetPtr += sizeof(double);

          if( MAX_DIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
          {
            std::cout<<"\tloading number of data items currently in queue:  " << temp_int << "\n";
            std::cout<<"\tloading channel rate from node " << i << " to HQ:  " << temp_double << "\n";
            std::cout<<"\tloading " << (int)areaCoverageInfo[i].size() << " pieces of area coverage info from node " << i << "\n";
          }
          for( j = 0; j < (int)areaCoverageInfo[i].size(); j++ )
          {
            if( MAX_DIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
            {
              std::cout<< "\t\tloading data item " << j << " of node " << i << "\n";
            }

            temp_areaCoverageInfo.x = areaCoverageInfo[i][j].x;
            temp_areaCoverageInfo.y = areaCoverageInfo[i][j].y;
            temp_areaCoverageInfo.xLength = areaCoverageInfo[i][j].xLength;
            temp_areaCoverageInfo.yLength = areaCoverageInfo[i][j].yLength;
            temp_areaCoverageInfo.size = areaCoverageInfo[i][j].size;

            memcpy( packetPtr, &temp_areaCoverageInfo, sizeof(AreaCoverageInfo) );
            packetPtr += sizeof(AreaCoverageInfo);
          }
        }
      }
      if( MAX_DIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
      {
        printf("Loaded all known information...\n");
      }

      Ptr<Packet> p = Create<Packet>( (uint8_t const *)origPacketPtr, packetSize);
      free(origPacketPtr);

      Ptr<Node> node = NodeList::GetNode((uint32_t)getNodeId()+1);
      Ptr<MaxDivSched> dusRp = node->GetObject<MaxDivSched>();
      if ( dusRp == 0 )
      {
        std::cout<<"ERROR:  DUSRP == 0 in GlobalExchangeControlInfoForward...Exiting\n";
        exit(-1);
      }
      Simulator::Schedule( Seconds(0.0), &MaxDivSched::GlobalExchangeControlInfoForward, dusRp, p ); 
    }
    else
    {
      Ptr<Node> node = NodeList::GetNode((uint32_t)getNodeId());
      Ptr<MaxDivSched> dusRp = node->GetObject<MaxDivSched>();
      if ( dusRp == 0 )
      {
        std::cout<<"ERROR:  DUSRP == 0 in GlobalExchangeControlInfoForward...Exiting\n";
        exit(-1);
      }
      // only last node sends this message...it starts chain of exchanging data backwards and then sends message to mark next time slot
      Simulator::Schedule( Seconds(0.0), &MaxDivSched::GlobalExchangeControlInfoBackward, this, (Ptr<Packet>)0 ); 
    }
  }

// /**
// FUNCTION: GlobalExchangeControlInfoBackward
// LAYER   : NETWORK
// PURPOSE : Function scheduled by simulator to be run.
//           This function extracts control info from all nodes after it,
//            places its own and all nodes' after it control info into a 
//            packet that it forwards to the node before it.
//           If using global knowledge, this function must schedule the 
//            complete time slot function
// PARAMETERS:
// +packet : DistUnivSchedPacket * : pointer to packet with control info
// RETURN   ::void:NULL
// **/
  void 
  MaxDivSched::GlobalExchangeControlInfoBackward( Ptr<Packet> packet )
  {
    if( MAX_DIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " is in GlobalExchangeControlInfoBackward() at " << Simulator::Now().GetNanoSeconds() << " (nanoseconds)\n";
    }

    int i, j;
    char *packetPtr, *origPacketPtr;
    int sizeOfPacket = ( sizeof(int)*getNumNodes() +  sizeof(double)*getNumNodes() + sizeof(AreaCoverageInfo)*MAX_DIV_SCHED_MAX_QUEUE_SIZE*getNumNodes() );
    char *tempPacket = (char *)malloc( sizeOfPacket );

    uint32_t packetSize = 0;

    if( packet != 0 )
    {
      packetSize = packet->CopyData( (uint8_t *)tempPacket, (uint32_t)sizeOfPacket );
      packetPtr = tempPacket;
    
      if( MAX_DIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
      {
        std::cout<< "\tcopied " << packetSize <<" bytes from packet into local buffer\n";
      }
    }

    int temp_int;
    double temp_double;
    AreaCoverageInfo temp_areaCoverageInfo;

    if( getNodeId()+1 != getNumNodes() )
    {
      for( i = getNodeId()+1; i < getNumNodes(); i++ )
      {
          // get number of data items node has (which we will be extracting from packet)
          memcpy(&temp_int, packetPtr, sizeof(int));
          packetPtr += sizeof(int);

          setNumDataItemsInQueue( i, temp_int );

          // get channel rate from node to HQ
          memcpy(&temp_double, packetPtr, sizeof(double));
          packetPtr += sizeof(double);

          setGlobalChannelRates( i, 0, temp_double );
         
          if( isGLOBAL_KNOWLEDGE() )
          {
              // extract area coverage information for node i's queue
            areaCoverageInfo[i].clear();

              for( j = 0; j < temp_int; j++ )
              {
                memcpy( &temp_areaCoverageInfo, packetPtr, sizeof(AreaCoverageInfo) ); 
                packetPtr += sizeof(AreaCoverageInfo);

                areaCoverageInfo[i].push_back( temp_areaCoverageInfo );
              }
          }
      }
    }

    free( tempPacket );

    if( getNodeId() != 0 )
    {
    //   origPacketPtr = packetPtr = tempPacket; 
        
        // allocate space for a backlog x commodity x node and rate x node x node and coordinate x node
      packetSize = ( sizeof(int)*getNumNodes() +  sizeof(double)*getNumNodes() + sizeof(AreaCoverageInfo)*MAX_DIV_SCHED_MAX_QUEUE_SIZE*getNumNodes() );
   
      origPacketPtr = packetPtr = (char *)malloc(packetSize);

        // load all known backlogs into message (this node and all after it)
        for( i = getNodeId(); i < getNumNodes(); i++ )
        {
          if( isGLOBAL_KNOWLEDGE() )
          {
            temp_int = (int)areaCoverageInfo[i].size();
            memcpy( packetPtr, &temp_int, sizeof(int) );
            packetPtr += sizeof(int);

            temp_double = getGlobalChannelRates( i, 0 );
            memcpy( packetPtr, &temp_double, sizeof(double) );
            packetPtr += sizeof(double);

            for( j = 0; j < (int)areaCoverageInfo[i].size(); j++ )
            {
              temp_areaCoverageInfo.x = areaCoverageInfo[i][j].x;
              temp_areaCoverageInfo.y = areaCoverageInfo[i][j].y;
              temp_areaCoverageInfo.xLength = areaCoverageInfo[i][j].xLength;
              temp_areaCoverageInfo.yLength = areaCoverageInfo[i][j].yLength;
              temp_areaCoverageInfo.size = areaCoverageInfo[i][j].size;

              memcpy( packetPtr, &temp_areaCoverageInfo, sizeof(AreaCoverageInfo) );

              packetPtr += sizeof(AreaCoverageInfo);
            }
          }
        }
        
        Ptr<Packet> p = Create<Packet>( (uint8_t const *)origPacketPtr, packetSize);
        free(origPacketPtr);

        // send message to previous node in line and then send msg w/ delay to trigger next time slot
        Ptr<Node> node = NodeList::GetNode((uint32_t)getNodeId()-1);
        Ptr<MaxDivSched> dusRp = node->GetObject<MaxDivSched>();

        Simulator::Schedule( Seconds(0.0), &MaxDivSched::GlobalExchangeControlInfoBackward, dusRp, p );
        
        if( isGLOBAL_KNOWLEDGE() )
        {
          if( !getGeneratedDuplicatePacketsThisSlot() )
          {
            Simulator::Schedule ( Seconds(0.0), &MaxDivSched::GenerateDuplicatePackets, this );
          }
          else
          {
            // send COMPLETE_TIME_SLOT event to calculate routing decisions and transmit packets
            Simulator::Schedule ( Seconds(0.0), &MaxDivSched::CompleteTimeSlot, this );
          }
            
          if( MAX_DIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
          {
            printf("node %i sent message to complete time slot with 0 delay\n", getNodeId());
          }
        }
    }
    else // node 0 - don't need to send any more exchange messages, just schedule complete time slot
    {
        if( isGLOBAL_KNOWLEDGE() )
        {
            if( MAX_DIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
            {
                printf("\n\nQueue data Exchange complete!\n\n");
            }
          if( !getGeneratedDuplicatePacketsThisSlot() )
          {
            Simulator::Schedule ( Seconds(0.0), &MaxDivSched::GenerateDuplicatePackets, this );
          }
          else
          {
            // send COMPLETE_TIME_SLOT event to calculate routing decisions and transmit packets
            Simulator::Schedule ( Seconds(0.0), &MaxDivSched::CompleteTimeSlot, this );
          }
        }
    }
  }

// /**
// FUNCTION: GenerateDuplicatePackets
// LAYER   : NETWORK
// PURPOSE : Called after one round of global control information exchange 
//         - generates duplicate packets (with same coverage of packets in other nodes)
//           to create percentage of overlapping packets
//         - Then sets boolean flag and restarts global info exchange, which will then 
//           end in complete time slot function, not this
// PARAMETERS:
// +none:
// RETURN   ::void:NULL
// **/
  void MaxDivSched::GenerateDuplicatePackets()
  {
    if( MAX_DIV_SCHED_GENERATE_DUP_PACKETS_DEBUG )
    { 
      std::cout<<"Node " << getNodeId() << " in Generate Duplicate Packets function.\n";
    }

    int numPacketsToGenerate = getDuplicatePacketDataRate();
    
    if( MAX_DIV_SCHED_GENERATE_DUP_PACKETS_DEBUG )
    { 
      std::cout<<"\tneed to generate " << numPacketsToGenerate << " packets\n";
    }
    
    Ptr<MobilityModel> mobility = NodeList::GetNode((uint32_t)getNodeId())->GetObject<MobilityModel> ();
    Vector pos = mobility->GetPosition();

    int i, j, k;
    vector< vector<bool> > duplicated;
    vector< vector<double> > distanceFromNode;
    int centerX, centerY;
    double xDistance, yDistance, distance;

    if( dataItems.size() == 0 )
    { 
      // if there are no data items at all, we just skip this function
      //  this should only happen in the first time slot before any 
      //  data packets have a chance to be generated
      if( MAX_DIV_SCHED_GENERATE_DUP_PACKETS_DEBUG )
      { 
        std::cout<<"\tno other packets in queue...skipping generate duplicate packets\n";
      }

      // send packet to start new round of scheduling
      if( isGLOBAL_KNOWLEDGE() ) 
      {
        setGeneratedDuplicatePacketsThisSlot( true );
        if( getNodeId() == 0 )
        {
        // no delay...exchange control info immediately...complete time slot messages sent from ExchangeControlInfo* functions  
          Simulator::Schedule ( Seconds (0.0), &MaxDivSched::GlobalExchangeControlInfoForward, this, (Ptr<Packet>)0 );
        }
      }
      return;
    }

    for( i = 0; i < getNumNodes(); i++ )
    {
      vector<bool> tempDup;
      vector<double> tempDist;
      for( j = 0; j < (int)areaCoverageInfo[i].size(); j++ )
      {
        centerX = areaCoverageInfo[i][j].x + areaCoverageInfo[i][j].xLength/2;
        centerY = areaCoverageInfo[i][j].y + areaCoverageInfo[i][j].yLength/2;
        xDistance = abs( (int)(pos.x - centerX) );
        yDistance = abs( (int)(pos.y - centerY) );
        distance = sqrt( xDistance*xDistance + yDistance*yDistance );
        
        tempDup.push_back( false );
        tempDist.push_back( distance );
      }
      distanceFromNode.push_back( tempDist );
      duplicated.push_back( tempDup );
    }

    double minDist;
    int minDistNode, minDistIndex;
    for( k = 0; k < numPacketsToGenerate; k++ )
    {
      minDist = 1000000;
      minDistNode = minDistIndex = -1;
      for( i = 0; i < getNumNodes(); i++ )
      {
        for( j = 0; j < (int)areaCoverageInfo[i].size(); j++ )
        {
          if( distanceFromNode[i][j] < minDist && !duplicated[i][j] )
          {
            minDist = distanceFromNode[i][j];
            minDistNode = i;
            minDistIndex = j;
          }
        }
      }
      if( minDistNode == -1 )
      {
        std::cout<<"WARNING:  No unduplicated packets left to duplicate at node " << getNodeId() << "...duplicated " << k << " out of " << numPacketsToGenerate <<".\n";
      }
      // duplicate this packet
    
      Ptr<Packet> newPacket = dataItems[0]->Copy(); // just copy random packet already in queue...then change associated info
     
      // add area coverage tag
      AreaCoverageTag areaCovTag( areaCoverageInfo[minDistNode][minDistIndex].x, 
                                  areaCoverageInfo[minDistNode][minDistIndex].y, 
                                  areaCoverageInfo[minDistNode][minDistIndex].xLength, 
                                  areaCoverageInfo[minDistNode][minDistIndex].yLength, 
                                  areaCoverageInfo[minDistNode][minDistIndex].size );
      if( !newPacket->PeekPacketTag (areaCovTag) )
      {
        newPacket->AddPacketTag(areaCovTag);
      }
  
      TypeHeader th(DUS_DATA);
      newPacket->AddHeader(th);
     
      if( MAX_DIV_SCHED_GENERATE_DUP_PACKETS_DEBUG )
      {
        std::cout<<"\tgenerated duplicate packet from node " << minDistNode << " with index " << minDistIndex;
        std::cout<<"...stats: [" << areaCovTag.x << ", " << areaCovTag.y << ", " << areaCovTag.xLength << ", " << areaCovTag.yLength << ", " << areaCovTag.size << "]\n";
      }
  
      DistUnivSchedDestTag tag (0);
      if (!newPacket->PeekPacketTag (tag))
      {
        std::cout<<"Added destination packet tag (in generated duplicate packet\n";
        newPacket->AddPacketTag (tag);
      }
     
      // change size of packet to match size determined by area coverage info
      //   (leaving size in tag the same)
      uint32_t packetSize = newPacket->GetSize();
      newPacket->RemoveAtEnd( packetSize - 32 );
      newPacket->AddPaddingAtEnd( MIN(2200, (uint32_t)areaCovTag.size) );

      if( areaCovTag.size > 2200 )
      {
        std::cout<<"WARNING:  Size of data item was too large for packet...truncating size at 2200 bytes.\n";
      }

      // queue up packet and area coverage info
      dataItems.push_back( newPacket );
      areaCoverageInfo[getNodeId()].push_back( areaCoverageInfo[minDistNode][minDistIndex] );

      // keep track of how many bytes are duplicated
      setSumDuplicateBytes( getSumDuplicateBytes() + areaCoverageInfo[minDistNode][minDistIndex].size );

      // mark as duplicated
      duplicated[minDistNode][minDistIndex] = true;
      if( MAX_DIV_SCHED_GENERATE_DUP_PACKETS_DEBUG )
      {
        std::cout<<"\tAdded packet to queue, number of data items now = " << dataItems.size() << "\n";
      }
    }

    setGeneratedDuplicatePacketsThisSlot( true );
 
    // send packet to start new round of scheduling or enter new time slot if freq > 1
    if( isGLOBAL_KNOWLEDGE() ) 
    {
      if( getNodeId() == 0 )
      {
        std::cout<<"Node 0 calling function to start global exchange again\n";
      // perform data exchange again without control info exchange
      // no delay...exchange control info immediately...complete time slot messages sent from ExchangeControlInfo* functions  
        Simulator::Schedule ( Seconds (0.0), &MaxDivSched::GlobalExchangeControlInfoForward, this, (Ptr<Packet>)0 );
      }
    }
  }

  // /**
  // FUNCTION: RecvPacket
  // LAYER   : NETWORK
  // PURPOSE : Called when any packet is received (callback). 
  //           Packet can be Control or Data packet and may have
  //            been generated from this node's application layer
  //            or received from the channel.
  //           This function checks the type of the incoming packet
  //            using the TypeHeader (defined in distUnivSchedPacket.h)
  //            and calls RecvControlPacket() or Recv Data Packet ftn
  //            depending on the packet type.
  //           This function also updates the channel rates when receiving
  //            a packet from the physical layer, by checking the SNR value
  //            attached to the packet in yans-wifi-phy.cc.
  // PARAMETERS:
  // +socket : Ptr<Socket> : socket of incoming packet arrival...packet retrieved from here
  // RETURN   ::void:NULL
  // **/ 
  void
  MaxDivSched::RecvPacket (Ptr<Socket> socket)
  {
    if( MAX_DIV_SCHED_RECV_PACKET_DEBUG )
    {
      std::cout<< "Node "<< getNodeId() << " in MaxDivSched::RecvPacket() at " << Simulator::Now().GetSeconds() << "\n";
    }

    Address sourceAddress;
    Ptr<Packet> packet = socket->RecvFrom (sourceAddress);
    if( packet == 0 )
    {
      std::cout<<"No Packet to retrieve from the Socket!\n";
      return;
    }
    InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom (sourceAddress);
    Ipv4Address sender = inetSourceAddr.GetIpv4 ();
    Ipv4Address receiver = m_socketAddresses[socket].GetLocal ();

    int senderIndex = (sender.Get()&(unsigned)255) - 1;
    int receiverIndex = (receiver.Get()&(unsigned)255) - 1;
    
    if( MAX_DIV_SCHED_RECV_PACKET_DEBUG )
    {
      std::cout<<"\treceived a packet from " << sender << " to " << receiver << "\n";
    }
        DistUnivSchedSnrTag snrTag(-1.0);
        if( packet->PeekPacketTag(snrTag) )
        {
          packet->RemovePacketTag(snrTag);
          if( MAX_DIV_SCHED_RECV_PACKET_DEBUG )
          {
            std::cout<< "\tNode " << getNodeId() << ": SNR of packet = " << snrTag.snr << "\n";
          }

          if( CALCULATE_RADIO_RANGES )
          {
            if( getNodeId() == 1 )
            {
              //std::cout<< "\tNode " << getNodeId() << ": SNR of packet = " << snrTag.snr << " at " << Simulator::Now().GetSeconds() << "\n";
              setLastSnr ( snrTag.snr );
            }
          }

          // This is set up to fill channel rate values using SNR values according to a
          //  table with values for each possible 802.11b data rate as determined by a 
          //  simple experiment.
          // This shouldn't matter here, though, because for now we are only using 
          //  global channel rates...hence the warning if not
          SetChannelRatesFromSnr( snrTag.snr, senderIndex, receiverIndex );
          if( !isGLOBAL_KNOWLEDGE() )
          {
            std::cout<<"WARNING:  Global Knowledge is not being used, and channel rates are being set in MaxDivSched::RecvPacket according to SNR.  (Check if this is desired behavior.)\n";
          }
        }
        else
        {
        //  std::cout<<"ERROR:  No SNR tag attached to Data Ack Packet...Exiting.\n";
        //  exit(-1);
        }


    DistUnivSchedPacketTypeTag typeTag(OTHER);
    if( packet->RemovePacketTag(typeTag) )
    {
      if( typeTag.type == DUS_DATA_ACK )
      {
        if( MAX_DIV_SCHED_RECV_PACKET_DEBUG )
        {
          std::cout<< "\tDUS_DATA_ACK Packet received (in RecvPacket)...type was in tag\n";
        }
        RecvDataAckPacket ( packet, sender, receiver );
        return;
      }
    }

    TypeHeader tHeader (OTHER);
    packet->RemoveHeader (tHeader);
    if (!tHeader.IsValid ())
    {
      std::cout<<"DUS message " << packet->GetUid () << " with unknown type received: " << tHeader.Get () << ". Drop\n";
      return; // drop
    }
    switch (tHeader.Get ())
    {
      case DUS_CTRL:
      {
        if( MAX_DIV_SCHED_RECV_PACKET_DEBUG )
        {
          std::cout<< "\tDUS_CTRL Packet received (in RecvPacket)\n";
        }
        RecvControlInfoPacket( packet );
        break;
      }
      case DUS_DATA:
      {
        if( MAX_DIV_SCHED_RECV_PACKET_DEBUG )
        {
          std::cout<< "\tDUS_DATA Packet received (in RecvPacket)\n";
        }
        RecvDataPacket ( packet, sender, receiver );
        break;
      }
      case DUS_DATA_ACK:
      {
        if( MAX_DIV_SCHED_RECV_PACKET_DEBUG )
        {
          std::cout<< "\tDUS_DATA_ACK Packet received (in RecvPacket)...type was in header\n";
        }
        RecvDataAckPacket ( packet, sender, receiver );
        break;
      }
      case OTHER:
      {
        if( MAX_DIV_SCHED_RECV_PACKET_DEBUG )
        {
          std::cout<< "  OTHER Packet received (in RecvPacket)\n";
        }
        //RecvReplyAck (sender);
        break;
      }
      default:
      {
        break;
      }
    }
  }

// /**
// FUNCTION: RecvDataPacket
// LAYER   : NETWORK
// PURPOSE : Function called by Recv Dist Univ Sched() when it receives a 
//            data packet, either from the application/transport layer
//            or from another node
//           This function looks at the destination of the packet stored in the attached
//            tag of type DistUnivSchedTag.  If the destination matches the local address, then
//            the packet has reached its destination and is removed from the network.  Otherwise, 
//            the packet must be added to the correct queue, where it will be forwarded from later.
// PARAMETERS:
// +packet : Ptr<Packet> : pointer to received packet
// +senderAddress : Ipv4Address : address of node that sent the packet...used to see if packet comes 
//                                  from other node or was generated internally (by the application layer) 
// +receiverAddress : Ipv4Address : address of node receiving packet...used to see if packet has reached destination
// RETURN   ::void:NULL
// **/
  void
  MaxDivSched::RecvDataPacket ( Ptr<Packet> packet, Ipv4Address senderAddress, Ipv4Address receiverAddress )
  {
    if( MAX_DIV_SCHED_RECV_DATA_PACKET_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << ":  in MaxDivSched::RecvDataPacket()\n";
    }

    DistUnivSchedDestTag tag;
    if( packet->RemovePacketTag (tag) )
    {
      if( MAX_DIV_SCHED_RECV_DATA_PACKET_DEBUG )
      {
        std::cout<<"\tdestination index of packet = " << tag.dest << "\n";
      }
      packet->AddPacketTag (tag);
    }
    NS_ASSERT_MSG( tag.dest > -1, "ERROR:  Received a data packet with no destination index tag.\n" );

    int localAddrIndex = (receiverAddress.Get()&(uint32_t)255) - 1;
    int senderAddrIndex = (senderAddress.Get()&(uint32_t)255) - 1;

    if( localAddrIndex == tag.dest )
    {
      // packet has reached destination...remove from network
      setNumPacketsReachDest( tag.dest, getNumPacketsReachDest( tag.dest ) + 1 );
      setNumPacketsReachDestThisSecond( tag.dest, getNumPacketsReachDestThisSecond( tag.dest ) + 1 );
      if( MAX_DIV_SCHED_RECV_DATA_PACKET_DEBUG )
      {
        std::cout<<"Received packet at destination from " << senderAddrIndex << " at time " << Simulator::Now().GetMilliSeconds() << "\n";
      }

      AreaCoverageTag areaCovTag( 0, 0, 0, 0, 0 );
      if( !packet->PeekPacketTag (areaCovTag) )
      {
        std::cout<< "ERROR:  could not find area coverage information tag in RecvDataPacket...exiting.\n";
        exit(-1);
      }
      else
      {
        if( MAX_DIV_SCHED_RECV_DATA_PACKET_DEBUG )
        {
          std::cout<<"Checking areaCoverageTag of packet in RecvDataPacket: x = " << areaCovTag.x << ", y = " << areaCovTag.y << ", ";
          std::cout<<"xLength = " << areaCovTag.xLength << ", yLength = " << areaCovTag.yLength << ", size = " << areaCovTag.size << "\n";
        }
        setNumBytesReachDestination( getNumBytesReachDestination() + areaCovTag.size );
      }
    }
    else
    {
      // packet has not reached destination...must place in proper queue to forward according to dus protocol
      //  don't update queue.m_backlog value here...that's done in complete time slot function to ensure consistency
      //  also need to remove SocketAddressTag since it will be added (again) when sent out after being dequeued
      SocketAddressTag sockAddrTag;
      packet->RemovePacketTag(sockAddrTag);
      AreaCoverageInfo temp_areaCoverageInfo;
      AreaCoverageTag areaCovTag( 0, 0, 0, 0, 0 );
      if( !packet->PeekPacketTag (areaCovTag) )
      {
        std::cout<< "ERROR:  could not find area coverage information tag in RecvDataPacket...exiting.\n";
        exit(-1);
      }
      else
      {
        temp_areaCoverageInfo.x = areaCovTag.x;
        temp_areaCoverageInfo.y = areaCovTag.y;
        temp_areaCoverageInfo.xLength = areaCovTag.xLength;
        temp_areaCoverageInfo.yLength = areaCovTag.yLength;
        temp_areaCoverageInfo.size = areaCovTag.size;
        if( MAX_DIV_SCHED_ROUTE_OUTPUT_DEBUG )
        {
          std::cout<<"Checking areaCoverageTag of packet in RecvDataPacket: x = " << areaCovTag.x << ", y = " << areaCovTag.y << ", ";
          std::cout<<"xLength = " << areaCovTag.xLength << ", yLength = " << areaCovTag.yLength << ", size = " << areaCovTag.size << "\n";
        }
      }
      dataItems.push_back( packet );
      areaCoverageInfo[getNodeId()].push_back( temp_areaCoverageInfo );

      setPacketsRcvThisTimeSlot( tag.dest, getPacketsRcvThisTimeSlot(tag.dest) + 1 );

      if( MAX_DIV_SCHED_RECV_DATA_PACKET_DEBUG )
      {
        std::cout<<"\tpacketsRcvThisTimeSlot[" << tag.dest << "] = " << getPacketsRcvThisTimeSlot(tag.dest) << "\n";
        std::cout<<"\tQueueing packet into vector...number of data items (vector size) = " << dataItems.size() << "\n";
      }
    }

    if( localAddrIndex != senderAddrIndex )
    {
      // only want to keep track of data packets received from other nodes
      setDataPacketsRcvd( senderAddrIndex, getDataPacketsRcvd(senderAddrIndex) + 1 );
    
      // NOT SENDING DATA ACK PACKETS HERE
      // packet was received from another node -> need to send an ACK for the data
    //  SendDataAckPacket( receiverAddress, senderAddress, tag.dest );
    }
  }


// /**
// FUNCTION: RecvDataAckPacket
// LAYER   : NETWORK
// PURPOSE : Function called by Recv Dist Univ Sched() when it receives a 
//            data ack packet from another node
//           This function is not necessary at the moment, but may be used if 
//            any timeout/retransmission schemes are to be implemented
// PARAMETERS:
// +packet : Ptr<Packet> : pointer to received packet
// +senderAddress : Ipv4Address : address of node that sent the packet...used to see if packet comes 
//                                  from other node or was generated internally (by the application layer) 
// +receiverAddress : Ipv4Address : address of node receiving packet...used to see if packet has reached destination
// RETURN   ::void:NULL
// **/
  void
  MaxDivSched::RecvDataAckPacket ( Ptr<Packet> packet, Ipv4Address senderAddress, Ipv4Address receiverAddress )
  {
	  if( MAX_DIV_SCHED_RECV_DATA_ACK_PACKET_DEBUG )
    { 
      std::cout<<"Node " << getNodeId() << ":  in MaxDivSched::RecvDataAckPacket() at time = " << Simulator::Now().GetSeconds() << "\n";
    }

    int commodity = -1;

    DistUnivSchedDestTag tag;
    if( packet->RemovePacketTag (tag) )
    {
	    if( MAX_DIV_SCHED_RECV_DATA_ACK_PACKET_DEBUG )
      { 
        std::cout<<"\tdestination index of packet = " << tag.dest << "\n";
      }
      commodity = tag.dest; 
    }
    NS_ASSERT_MSG( commodity > -1, "ERROR:  Received a data ack packet with no destination index tag.\n" );

    setNumDataAcksRcvd( getNumDataAcksRcvd() + 1 );
    
  }

// /**
// FUNCTION: SendDataPacket
// LAYER   : NETWORK
// PURPOSE : Function called by Complete Time Slot function to send desired data item
//             from calling node to recipient node (should always be node 0, HQ).
//             The item will be sent here, but will be deleted from the vector in Complete 
//             Time Slot function to ensure integrity of the vector until all desired items are sent.
// PARAMETERS:
// +node:Node *::Pointer to node
// +recipient:int:Index of node to which data packet should be sent
// +commodity:int:Index of commodity which is being sent
// +dataItemIndex:int:Index into dataItems vector at which the item/packet to be sent is stored.
// RETURN   ::void:NULL
// **/
  void
  MaxDivSched::SendDataPacket( int recipient, int commodity, Ptr<Packet> packetToSend ) // int dataItemIndex )
  {
    if( MAX_DIV_SCHED_SEND_DATA_PACKET_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " is in MaxDivSched::SendDataPacket()...sending data type " << commodity << " to node " << recipient << " at " << Simulator::Now().GetSeconds() << "\n";
    }

    if( commodity == -1 ) // commodity not set by calling function...use last chosen value in Complete Time Slot(), which should be weightCommodity[getNodeId()][receivingNode]
    {
      commodity = getWeightCommodity(getNodeId(), recipient);
    }
	
    if( MAX_DIV_SCHED_SEND_DATA_PACKET_DEBUG )
    {
      printf("\tAttempting to send a packet with destination %i from node %i to node %i\n", 
			     commodity, getNodeId(), recipient );
    }
	
    if( getControlInfoExchangeState() != DATA_TRX )
    {
    	if( MAX_DIV_SCHED_SEND_DATA_PACKET_DEBUG )
    	{
    		printf( "\tstate != DATA_TRX...returning without sending packet\n" );
    	}
    	return;
    }
    
    if( commodity == -1 )
    {
    	commodity = getWeightCommodity(getNodeId(), recipient);
      std::cout<<"ERROR:  SendDataPacket() did not receive a valid value for commodity.  I don't think this should happen.  Exiting.\n";
      exit(-1);
    }

    // make sure physical layer rate is set to ideal rate
    if( getChannelRates( getNodeId(), recipient ) == 0.0 )
    {
      Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("DsssRate1Mbps") );
      //Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("OfdmRate6Mbps") );
    }
    else if( getChannelRates( getNodeId(), recipient ) == 1.0 )
    {
      Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("DsssRate1Mbps") );
      //Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("OfdmRate6Mbps") );
    }
    else if( getChannelRates( getNodeId(), recipient ) == 2.0 )
    {
      Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("DsssRate2Mbps") );
      //Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("OfdmRate9Mbps") );
    }
    else if( getChannelRates( getNodeId(), recipient ) == 5.5 )
    {
      Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("DsssRate5_5Mbps") );
      //Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("OfdmRate12Mbps") );
    }
    else if( getChannelRates( getNodeId(), recipient ) == 11.0 )
    {
      Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("DsssRate11Mbps") );
      //Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("OfdmRate18Mbps") );
    }
    else
    {
      std::cout<<"ERROR: Could not find the matching channel rate from node " << getNodeId() << " to node " << recipient << " in SendDataPacket()\n";
      exit(-1);
    }

    if( MAX_DIV_SCHED_SEND_DATA_PACKET_DEBUG )
    {
    	printf( "\tNode %i:    setting physical layer rate to index %i\n", getNodeId(), getChannelRateIndex(recipient) ); 
    }
	
    if( MAX_DIV_SCHED_SEND_DATA_PACKET_DEBUG )
    {
      printf("Attempting to send packet:\nFrom %i to %i, commodity %i\n",
      getNodeId(), recipient, commodity);
    }

    Ptr<Packet> packet;
    // No need to add packet tag with destination (commodity) because it was added before queueing
    packet = packetToSend; //dataItems[dataItemIndex]->Copy(); 
   
    /*
    if( dataItems.size() > 0 )
    {
      packet = packetToSend; //dataItems[dataItemIndex]->Copy(); 
    }
    else
    {
      if( MAX_DIV_SCHED_SEND_DATA_PACKET_DEBUG )
      {
        std::cout<<"Node " << getNodeId() << " trying to send data item to node " << recipient << ", but queue (vector) is empty.\n";
      }
      return;
    }
    */
    // Send Control Packet as subnet directed broadcast from each interface used by distUnivSched
  
    std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin();
    //for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
    //    m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    Ptr<Socket> socket = j->first;
    Ipv4InterfaceAddress iface = j->second;


    if( MAX_DIV_SCHED_SEND_DATA_PACKET_DEBUG )
    { 
      std::cout<<"\ttrying to send out interface with address: ";
      iface.GetLocal().Print( std::cout );
      std::cout<<"\n";
    }

    TypeHeader tHeader (DUS_DATA);
    packet->AddHeader (tHeader);
                        
    char buf[16];
    sprintf( buf, "10.0.0.%i", recipient+1 );
    Ipv4Address tempAddr( buf );
    if( (socket->SendTo (packet, 0, InetSocketAddress (tempAddr, DUS_PORT))) == -1 )
    {
      std::cout<<"ERROR:  socket->SendTo() in SendControlInfoPacket() failed\n";
    }
    setDataPacketsSent(recipient, getDataPacketsSent(recipient) + 1);
	
		// wait until end of time slot to update queues
    setPacketsTrxThisTimeSlot(commodity, getPacketsTrxThisTimeSlot(commodity)+1); 
  }

  
  // /**
  // FUNCTION: GenerateRandomAreaCoverageInfo
  // LAYER   : NETWORK
  // PURPOSE : This function generates the position and area of a rectangular coverage
  //          for a newly generated packet.
  //        - It determines a box of fixed size (with side length = node poss. cov. dist) 
  //          around the node's current location, adjusting for boundaries...the box is 
  //          always the same size, but will be pushed away from being centered by the node
  //          if forced to be by boundaries of the simulation environment
  //				- Next it generates the size of the packet by either using a fixed value or by
  //          generating a random width and length value both within min and max item cov. dist. parameters 
  //          (depending on fixed packet size parameter).  Then it randomly places the packet 
  //          at a place within the node's possible coverage area, such that it will fit entirely
  //          within this box.  (the size will be clipped if the generated packet size is bigger
  //          than the possible coverage box of the node...ideally the parameters should be set
  //          so this is unnecessary, though)
  // PARAMETERS:
  // + acx : int * : pointer to packet area coverage x value
  // + acy : int * : pointer to packet area coverage y value
  // + acxl : int * : pointer to packet area coverage x length value
  // + acyl : int * : pointer to packet area coverage y length value
  // + acs : int * : pointer to packet area coverage size value
  // NOTE : These values are all populated within this function
  // RETURN   ::void:NULL
  // **/ 
  void
  MaxDivSched::GenerateRandomAreaCoverageInfo( int *acx, int *acy, int *acxl, int *acyl, int *acs )
  {
    // get location of node
    Ptr<MobilityModel> mobility = NodeList::GetNode((uint32_t)getNodeId())->GetObject<MobilityModel> ();
    Vector pos = mobility->GetPosition();

    // possible area of coverage is box around node (
    setNodePossCovX( (int)(MAX( (pos.x - getNodePossCovDist()/2), 0 )) ); // make sure possible coverage box is not outside of
    setNodePossCovY( (int)(MAX( (pos.y - getNodePossCovDist()/2), 0 )) ); //   simulation environment

    if( getNodePossCovX() + getNodePossCovDist() > getAreaWidth() ) // would extend past right boundary...push back 
    {
      setNodePossCovX( getNodePossCovX() - (getNodePossCovDist() - (getAreaWidth() - getNodePossCovX())) );
    }
    if( getNodePossCovY() + getNodePossCovDist() > getAreaLength() ) // would extend past right boundary...push back 
    {
      setNodePossCovY( getNodePossCovY() - (getNodePossCovDist() - (getAreaLength() - getNodePossCovY())) );
    }

    int randX, randY, randXL, randYL;
     
    // first determine the width and length of packet coverage, then generate random placement 
    //   so that packet fits in possible coverage box
    if( getFixedPacketSize() )
    {
      randXL = randYL = getFixedPacketLength();
    }
    else
    {
      randXL = coverageRV.GetInteger( getMinItemCovDist(), getMaxItemCovDist() );
      randYL = coverageRV.GetInteger( getMinItemCovDist(), getMaxItemCovDist() );
   
      // this just makes sure packet is not bigger than possible coverage area...should not be necessary 
      //   as long as simulation is configured correctly
      randXL = MIN( randXL, getNodePossCovDist() );
      randYL = MIN( randYL, getNodePossCovDist()  );
    }
    
    randX = coverageRV.GetInteger( getNodePossCovX(), getNodePossCovX()+getNodePossCovDist()-randXL );
    randY = coverageRV.GetInteger( getNodePossCovY(), getNodePossCovY()+getNodePossCovDist()-randYL );

    *acx = randX;
    *acy = randY;
    *acxl = randXL;
    *acyl = randYL;

    // changed size of packet to be equal to one byte per 4 sq meters of coverage to increase overall coverage percentage
    *acs = (*acxl)/4 * (*acyl)/4;

    //std::cout<< "Generated: x = " << *acx << ", y = " << *acy << ", xLength = " << *acxl << ", yLength = " << *acyl << ", size = " << *acs
     //       << "... randX = " << randX << ", randY = " << randY << "\n";

    return;
  }

  // /**
  // FUNCTION: PrintStats
  // LAYER   : NETWORK
  // PURPOSE : This function is scheduled in the init function to run 1 nanosecond before the simulation is complete.  
  //            It opens the output file 'maxDivSchedStats.csv' in the path specified by dataFilePath
  //            and writes the run statistics to it. If the file does not exist, it creates it.  If the file 
  //            does exist, it will open it and append the stats to the end.
  // PARAMETERS: none
  // NOTE : These values are all populated within this function
  // RETURN   ::void:NULL
  // **/ 
  void 
  MaxDivSched::PrintStats()
  {
    char buf[1024];
    int i;
    double avgTotalOccupancy = 0.0;
    double avgDeliveredThroughput = 0.0;
    double avgDeliveredThroughputBytes = 0.0;
    double avgInputRate = 0.0;
    double avgCoverageOfChosenSets = 0.0;
    double avgCoverageOfMaxSets = 0.0;
    double avgOverlapOfChosenSets = 0.0;
    double avgOverlapOfMaxSets = 0.0;
    double avgChannelRate = 0.0;
    double avgDuplicateBytes = 0.0;
    double avgNumSetsSkipped = 0.0;

    time( &simEndTime );
    double runTime = difftime( simEndTime, simStartTime );

    if( MAX_DIV_SCHED_PRINT_STATS_DEBUG )
    {
      printf("\nNode %i in MaxDivSched::PrintStats()\n", getNodeId());			

      printf("\nNumber of Data Packets from Application = %i\n", getNumPacketsFromApplication());

      //printf("\n\tNumber of Control Packets Sent = %i\n", numControlPacketsSent);
      //printf("\tNumber of Control Packets Rcvd = %i\n", numControlPacketsRcvd);
      printf("\tNumber of Data Packet Acks Sent = %i\n", getNumDataAcksSent());
      printf("\tNumber of Data Packet Acks Rcvd = %i\n", getNumDataAcksRcvd());
      printf("\tNumber of Packet Collisions = %i\n", getNumCollisions());
      //printf("\tNumber of Incorrect Queues Values = %i\n", numIncorrectQueues);
      //printf("\tNumber of Incorrect Rate Values = %i\n", numIncorrectRates);
	
      for( i = 0; i < getNumNodes(); i++ )
      {
        printf("\tdata packets sent to %i: %i\n", i, getDataPacketsSent(i));
      }
      printf("\n");
      for( i = 0; i < getNumNodes(); i++ )
      {
        printf("\tdata packets rcvd from %i: %i\n", i, getDataPacketsRcvd(i));
      }
      printf("\tPower used: %f\n", getPowerUsed());
      printf("\tRun Time: %f\n", runTime);
    }
  	
    for( i = 0; i < getNumCommodities(); i++ )
    {
      avgTotalOccupancy += getQueueLengthSum(i)/getSimulationTime().GetSeconds();
	  	
      avgDeliveredThroughput += getNumPacketsReachDest(i)/getSimulationTime().GetSeconds();

      //packetsDropped += (int)queues[i].GetTotalDroppedPackets();
    }
    avgDeliveredThroughputBytes = getNumBytesReachDestination()/getSimulationTime().GetSeconds();

    avgChannelRate = getSumChannelRate()/(double)getNumTimeSlots();
    avgDuplicateBytes = getSumDuplicateBytes()/getSimulationTime().GetSeconds();
    
    // calculate average coverage
    /*
    for( i = 0; i < getAreaWidth(); i++ )
    {
      for( j = 0; j < getAreaLength(); j++ )
      {
        avgCoverageOfMaxSets += (HQCumulativeCoverage[i][j]/(double)((getNumTimeSlots()-1)*getAreaWidth()*getAreaLength()))*100.0; // subtract 1 from numTimeSlots b/c nothing is covered in first slot
      }
    }
    */
    avgCoverageOfChosenSets = getSumCoverageOfChosenSets()/(double)getNumTimeSlots();
    avgCoverageOfMaxSets = getSumCoverageOfMaxSets()/(double)getNumTimeSlots();

    avgOverlapOfChosenSets = getSumOverlapOfChosenSets()/(double)getNumTimeSlots();
    avgOverlapOfMaxSets = getSumOverlapOfMaxSets()/(double)getNumTimeSlots();

    avgNumSetsSkipped = (double)getNumSetsSkipped()/(double)getNumTimeSlots();
    
    // get node positions (this will only be useful if nodes are static
    Ptr<MobilityModel> mobility = NodeList::GetNode((uint32_t)getNodeId())->GetObject<MobilityModel> ();
    Vector pos = mobility->GetPosition();
    
    avgInputRate = getNumPacketsFromApplication()/getSimulationTime().GetSeconds();

    int globOutput = 0;
    if ( isGLOBAL_KNOWLEDGE() )
      globOutput = 1;

    int longTermAvgProb = 0;
    if ( getLongTermAvgProblem() )
    {
      sprintf(buf, "%smaxDivSchedStats_LongTermAvg.csv", getDataFilePath().c_str() );
      longTermAvgProb = 1;
    }

    int oneShotProb = 0;
    if ( getOneShotProblem() )
    {
      sprintf(buf, "%smaxDivSchedStats_OneShot.csv", getDataFilePath().c_str() );
      oneShotProb = 1;
    }

    int randomChoiceProb = 0;
    if ( getRandomChoiceProblem() )
    {
      sprintf(buf, "%smaxDivSchedStats_Random.csv", getDataFilePath().c_str() );
      randomChoiceProb = 1;
    }

    int approxOS = 0;
    if ( getApproxOneShotProblem() )
    {
      sprintf(buf, "%smaxDivSchedStats_ApproxOneShot.csv", getDataFilePath().c_str() );
      approxOS = 1; 
    }

    int approxMR = 0;
    if ( getApproxMaxRatioProblem() )
    {
      sprintf(buf, "%smaxDivSchedStats_ApproxMaxRatio.csv", getDataFilePath().c_str() );
      approxMR = 1; 
    }

    int approxGVQ = 0;
    if ( getApproxGreedyVQProblem() )
    {
      sprintf(buf, "%smaxDivSchedStats_GreedyVQ.csv", getDataFilePath().c_str() );
      approxGVQ = 1; 
    }
	  
    double powerBudget;
    if( getLongTermAvgProblem() || getApproxGreedyVQProblem() )
    {
      powerBudget = getAvgPowerBudget();
    }
    else
    {
      powerBudget = getNodePowerBudget();
    }
 
    //  if node is still alive, set lifetime to full sim time 
    if( getBatteryPowerLevel() > 0.0 )
    {
      setNodeLifetime( Simulator::Now() );
    }

    FILE *statsFd = fopen(buf, "a");

      if( statsFd == NULL )
      {
        printf( "ERROR:  Could not open stats file named '%s'.  Not printing any stats to output file.\n", buf );
        return;
      }

                   //    1  2   3     4      5   5-a   5-b   6  7    8   9  10  11  11-a  12    13  14   15      16  17  18   19  20  21  22  23   24    25    26    27    28    29    30   31   32     33  34    35  36    37    38      39
      fprintf(statsFd, "%i, %i, %i, %.3f, %.3f, %.2f, %.2f, %i, %i, %i, %i, %i, %i, %i, %.3f, %.3f, %i, %.3f, %.3f, %i, %.3f, %i, %i, %i, %i, %i, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %i, %.3f, %.3f, %.3f, %i, %.3f, %.1f, %.2f, %.1f\n", 
	getNodeId(), // i 1
        getNumNodes(), // i 2
        getNumRun(), // i 3
        pos.x, // f 4
        pos.y, // f 5
        getNodeRWPause(), // f 5-a
        getNodeRWSpeed(), // f 5-b
        globOutput, // i 6
        longTermAvgProb, // i 7
        oneShotProb, // i 8
        randomChoiceProb, // i 9
        approxOS, // i 10
        approxMR, // i 11
        approxGVQ, // i 11-a
	avgInputRate, // f 12
        getSimulationTime().GetSeconds(), // f 13
	getNumTimeSlots(), // i 14 
	getTimeSlotDuration().GetSeconds(), // f 15
        getTimeBudget().GetSeconds(), // f 16
        getNumTimesExceededTimeBudget(), // i 17
        powerBudget, // f 18
        getNumTimesExceededPowerBudget(getNodeId()), // i 19
	getNumCollisions(), //  i  20
        getNumDataPcktCollisions(), // i 21
        getNumDataAckCollisions(), // i 22
	getPacketsDropped(), //  i 23
        avgChannelRate, // f 24
	0.0, // avg. total occ. doesn't matter here (renewal system) //avgTotalOccupancy, // f 25
	avgDeliveredThroughput, // f 26
	avgDeliveredThroughputBytes, // f 27
        avgCoverageOfChosenSets, // f 28
        avgCoverageOfMaxSets, // f 29
        getPowerUsed()/(double)(getNumTimeSlots()-1), // f 30
        getDuplicatePacketDataRate(),  // i 31
        avgDuplicateBytes,  // f 32
        avgOverlapOfChosenSets, // f 33
        avgOverlapOfMaxSets, // f 34
        V, // i 35
        getNodeLifetime().GetSeconds(), // f 36
        getInitBatteryPowerLevel(), // f 37
        runTime, // f 38
        avgNumSetsSkipped // f 39
        ); 
	
      
  		if( MAX_DIV_SCHED_PRINT_STATS_DEBUG ) 
  		{    
    		printf( "AvgInputRate = %f\n", avgInputRate );
      
        std::cout<<"Sum Overlap of Chosen Sets = " << getSumOverlapOfChosenSets() << ", num time slots = " << (double)getNumTimeSlots() << "\n";
		
    		//printf( "numIncorrectQueues = %i, numIncorrectRates = %i\n", numIncorrectQueues, numIncorrectRates );
		
    		//printf( "Number of time slots ACK not received: %f\n", stats->getNumTimeSlotsAckNotRcvd() );
    		//printf( "Number of time slots ACK received: %f\n", stats->getNumTimeSlotsRcvdAck() );
		
    		//printf( "Number of time slots Exchanged info matches global: %f\n", stats->getNumTimeSlotsExchangedMatchesGlobal() );
    		//printf( "Number of time slots Exchanged info does not match global: %f\n", stats->getNumTimeSlotsExchangedGlobalDiff() );
    		//printf( "Number of time slots Backlogs didn't match: %f\n", stats->getNumTimeSlotsBacklogsDiff() );
    		//printf( "Number of time slots Rates didn't match: %f\n", stats->getNumTimeSlotsRatesDiff() );
    		//printf( "Number of Time Slots Incorrectly Trx = %f\n", stats->getNumTimeSlotsIncorrTrx() );
  		  //printf( "Number of Time Slots Incorrectly Silent = %f\n", stats->getNumTimeSlotsIncorrSilent() );
      }
		
  		fclose( statsFd );

      if( getOutputPowerVsTime() )
      {
        fclose( powerVsTimeFile );
      }
    
      /*
    if( isTIME_STATS() && getNodeId() == 0 )
    {
      sprintf(buf, "maxDivSchedCoverageMap.csv");
      FILE *coverageMapFd = fopen(buf, "a");

      for( i = 0; i < getNumTimeSlots(); i++ )
      {
        fprintf( coverageMapFd, "%i, %i\n ", getNodeId(), i+1 );
        for( j = 0; j < numCommodities; j++ )
        {
          fprintf( coverageMapFd, "%i, ",  );
        }
        fprintf( coverageMapFd, "\n" );
      }

      fclose( queuesFd );
    }

    if( isOUTPUT_RATES() && getNodeId() == 0 )
    {
      fclose( channelRatesFd );
    }
    */

  }
}
}

