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

#include <math.h>
#include <stdio.h>
#include "dus-routing-protocol.h"
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
NS_OBJECT_ENSURE_REGISTERED (RoutingProtocol);

/// UDP Port for DUS control traffic
const uint32_t RoutingProtocol::DUS_PORT = 1300;


  // initialize nodeIdAssign so it gives unique values to each "node"
  int RoutingProtocol::nodeIdAssign = 0; 
  //-----------------------------------------------------------------------------

  

  RoutingProtocol::RoutingProtocol () :
    VARYING_Q_BITS (false),
    GLOBAL_KNOWLEDGE (false),
    LINE_NETWORK (false),
    BRUTE_FORCE (false),
    TIME_STATS (false),
    END_RUN_STATS (false),
    OUTPUT_RATES (false),
    VARY_CORRECT_QUEUE_INFO (false),
    VARY_CORRECT_RATE_INFO (false),
    probRateChange (0.0),
    maxRateChange (0.0),
    probQueueChange (0.0),
    maxQueueChange (0.0),
    PRINT_QUEUE_DIFFS (false),
    PRINT_RATE_DIFFS (false),
    PRINT_CHOSEN_RATE_DIFFS (false),
    timeSlotDuration (Time("1s")),
    controlPacketTrxTime (Time("2ms")), // 2 milliseconds
    maxBackoffWindow (10), // integer (in milliseconds)
    waitForAckTimeoutWindow (Time("10ms")),
    controlInfoExchangeTime (Time("100ms")),
    nextTimeSlotStartTime (Time("0ms")), 
    waitForAckTimeoutTime (Time("25ms")), // 25 milliseconds
    simulationTime (Time("10s")),
    qBits (13),
    rateBits(2),
    maxBufferSize(8192),
    numNodes (2),
    numRun(1),
    numCommodities (numNodes),
    timeSlotNum (0),
    numBitsPerPacket (4096.0), // refers to data payload size...used to determine number of packets that 
    batteryPowerLevel (5120.0), // this default value should let each node trx 10,000 packet at lowest data rate
    powerUsed (0.0),
    nodeRWSpeed(0.0),
    nodeRWPause(0.0),
    nodeLifetime(Time("0s"))
  {
    if( DIST_UNIV_SCHED_CONSTRUCTOR_DEBUG )
    {
      std::cout<<"Entering DistUnivSched() Constructor\n"; 
      std::cout<<"\tTime Slot Duration = " << timeSlotDuration.GetSeconds() << "\n";
    }
  
    nodeId = nodeIdAssign++;

    numPacketsFromApplication = 0;
    numPacketsFromApplicationThisSlot = 0;
    timeSlotNum = 0;
    controlInfoExchangeState = SEND_OWN_INFO;
    
    if( isGLOBAL_KNOWLEDGE() ) //#ifdef GLOBAL_KNOWLEDGE
    {      
      controlPacketTrxTime = (Time)0;
    }
    else
    {
      // Designed under the assumption that control packets are transmitted at 1 Mbps
      controlPacketTrxTime = MilliSeconds(1); // this will actually be changed before each control packet is sent...just initializing to something here
    }

    // Schedule Init function to initialize variables 
    Simulator::ScheduleNow( &RoutingProtocol::DistUnivSchedInit, this );

    if( isGLOBAL_KNOWLEDGE() || isLINE_NETWORK() || isBRUTE_FORCE() )
    {
      //if( getNodeId() == 0 ) 
      {
        // only schedule first node to kick off control info exchange process
        Simulator::Schedule ( Seconds (0.0), &RoutingProtocol::StartTimeSlot, this );
      }
    }
    else
    {
      // every node schedule first time slot to begin
      Simulator::Schedule ( Seconds (0.0), &RoutingProtocol::StartTimeSlot, this );
    }

    if( DIST_UNIV_SCHED_CONSTRUCTOR_DEBUG )
    {
      std::cout<<"Exiting DistUnivSched() constructor\n";
    }
    
  }

  TypeId
  RoutingProtocol::GetTypeId (void)
  {
   static TypeId tid = TypeId ("ns3::dus::RoutingProtocol")
    .SetParent<Ipv4RoutingProtocol> ()
    .AddConstructor<RoutingProtocol> ()
    .AddAttribute ("VARYING_Q_BITS", "True if using limited state feedback (representing queues with only 'k' bits).",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::setVARYING_Q_BITS),
                   MakeBooleanChecker ())
    .AddAttribute ("GLOBAL_KNOWLEDGE", "True if using global knowledge.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::GLOBAL_KNOWLEDGE),
                   MakeBooleanChecker ())
    .AddAttribute ("LINE_NETWORK", "True if exchanging control info up and down line network as in MILCOM 2011 paper.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::LINE_NETWORK),
                   MakeBooleanChecker ())
    .AddAttribute ("BRUTE_FORCE", "True if exchanging control info up and down line network num_exchange_rounds number of times to ensure complete dissemination.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::BRUTE_FORCE),
                   MakeBooleanChecker ())
    .AddAttribute ("FINITE_BATTERY", "True if transmissions reduce battery power level allowing nodes to eventually die.  Otherwise, batteries remain full.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::FINITE_BATTERY),
                   MakeBooleanChecker ())
    .AddAttribute ("TIME_STATS", "True if collecting statistics for each time slot.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::TIME_STATS),
                   MakeBooleanChecker ())
    .AddAttribute ("END_RUN_STATS", "True if collecting statistics only during last 25\% of simulation.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::END_RUN_STATS),
                   MakeBooleanChecker ())
    .AddAttribute ("OUTPUT_RATES", "True if printing channel rates available between all nodes throughout simulation.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::OUTPUT_RATES),
                   MakeBooleanChecker ())
    .AddAttribute ("VARY_CORRECT_QUEUE_INFO", "Description not available.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::VARY_CORRECT_QUEUE_INFO),
                   MakeBooleanChecker ())
    .AddAttribute ("VARY_CORRECT_RATE_INFO", "Description not available.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::VARY_CORRECT_RATE_INFO),
                   MakeBooleanChecker ())
    .AddAttribute ("PRINT_QUEUE_DIFFS", "Description not available.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::PRINT_QUEUE_DIFFS),
                   MakeBooleanChecker ())
    .AddAttribute ("PRINT_RATE_DIFFS", "Description not available.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::PRINT_RATE_DIFFS),
                   MakeBooleanChecker ())
    .AddAttribute ("PRINT_CHOSEN_RATE_DIFFS", "Description not available.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::PRINT_CHOSEN_RATE_DIFFS),
                   MakeBooleanChecker ())
    .AddAttribute ("probRateChange", "Description not available.",
                   DoubleValue (0.0),
                   MakeDoubleAccessor (&RoutingProtocol::probRateChange),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("maxRateChange", "Description not available.",
                   DoubleValue (0.0),
                   MakeDoubleAccessor (&RoutingProtocol::maxRateChange),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("probQueueChange", "Description not available.",
                   DoubleValue (0.0),
                   MakeDoubleAccessor (&RoutingProtocol::probQueueChange),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("maxQueueChange", "Description not available.",
                   DoubleValue (0.0),
                   MakeDoubleAccessor (&RoutingProtocol::maxQueueChange),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("timeSlotDuration", "Description not available.",
                   TimeValue (Seconds (2)),
                   MakeTimeAccessor (&RoutingProtocol::timeSlotDuration),
                   MakeTimeChecker ())
    .AddAttribute ("controlPacketTrxTime", "Description not available.",
                   TimeValue (MilliSeconds (2)),
                   MakeTimeAccessor (&RoutingProtocol::controlPacketTrxTime),
                   MakeTimeChecker ())
    .AddAttribute ("maxBackoffWindow", "Description not available.",
                   IntegerValue (2),
                   MakeIntegerAccessor (&RoutingProtocol::maxBackoffWindow),
                   MakeIntegerChecker<int> ())
    .AddAttribute ("waitForAckTimeoutWindow", "Description not available.",
                   TimeValue (MilliSeconds (10)),
                   MakeTimeAccessor (&RoutingProtocol::waitForAckTimeoutWindow),
                   MakeTimeChecker ())
    .AddAttribute ("controlInfoExchangeTime", "Description not available.",
                   TimeValue (MilliSeconds (100)),
                   MakeTimeAccessor (&RoutingProtocol::controlInfoExchangeTime),
                   MakeTimeChecker ())
    .AddAttribute ("waitForAckTimeoutTime", "Description not available.",
                   TimeValue (MilliSeconds (25)),
                   MakeTimeAccessor (&RoutingProtocol::waitForAckTimeoutTime),
                   MakeTimeChecker ())
    .AddAttribute ("simulationTime", "Description not available.",
                   TimeValue (Seconds (10)),
                   MakeTimeAccessor (&RoutingProtocol::simulationTime),
                   MakeTimeChecker ())
    .AddAttribute ("qBits", "Description not available.",
                   IntegerValue (13),
                   MakeIntegerAccessor (&RoutingProtocol::qBits),
                   MakeIntegerChecker<int> ())
    .AddAttribute ("rateBits", "Description not available.",
                   IntegerValue (3),
                   MakeIntegerAccessor (&RoutingProtocol::rateBits),
                   MakeIntegerChecker<int> ())
    .AddAttribute ("maxBufferSize", "Maximum size of buffer...used to calculate discrete queue values when limited feedback is used.",
                   IntegerValue (8192),
                   MakeIntegerAccessor (&RoutingProtocol::maxBufferSize),
                   MakeIntegerChecker<int> ())
    .AddAttribute ("numNodes", "Number of nodes in scenario.",
                   IntegerValue (2),
                   MakeIntegerAccessor (&RoutingProtocol::numNodes),
                   MakeIntegerChecker<int> ())
    .AddAttribute ("numRun", "Run number for random number generator, set in the simulation script.",
                   IntegerValue (1),
                   MakeIntegerAccessor (&RoutingProtocol::numRun),
                   MakeIntegerChecker<int> ())
    .AddAttribute ("numCommodities", "Number of commodities in scenario...should be equal to number of nodes.",
                   IntegerValue (2),
                   MakeIntegerAccessor (&RoutingProtocol::numCommodities),
                   MakeIntegerChecker<int> ())
    .AddAttribute ("numBitsPerPacket", "Packet size (in bits).",
                   DoubleValue (4096.0),
                   MakeDoubleAccessor (&RoutingProtocol::numBitsPerPacket),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("batteryPowerLevel", "Full battery life to be drained each trx (in mWsec).",
                   DoubleValue (5120.0),
                   MakeDoubleAccessor (&RoutingProtocol::batteryPowerLevel),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("nodeRWSpeed", "Maximum speed for random waypoint protocol.  Speeds are chosen uniformly between 0 and this number.",
                   DoubleValue (0.0),
                   MakeDoubleAccessor (&RoutingProtocol::nodeRWSpeed),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("nodeRWPause", "Maximum pause time for random waypoint protocol.  Pause times are chosen uniformly between 0 and this number.",
                   DoubleValue (0.0),
                   MakeDoubleAccessor (&RoutingProtocol::nodeRWPause),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("inputRate", "number of packets being generated by the application per second.",
                   DoubleValue (10.0),
                   MakeDoubleAccessor (&RoutingProtocol::setInputRate),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("dataFilePath",
                   "Path to the directory containing the .csv stats file.",
                   StringValue ("./"),
                   MakeStringAccessor (&RoutingProtocol::setDataFilePath),
                   MakeStringChecker ())
    .AddAttribute ("controlInfoExchangeChannelRate", "Rate used to send control info packets (in Mbits/Sec)",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&RoutingProtocol::setControlInfoExchangeChannelRate),
                   MakeDoubleChecker<double> ())
    ;  
    return tid;
  }

  RoutingProtocol::~RoutingProtocol ()
  {
    std::cout<<"In DistUnivSched destructor\n";
  }
    

  // /**
  // FUNCTION: DistUnivSchedInit
  // LAYER   : NETWORK
  // PURPOSE : Called at beginning of simulation to initialize some variables
  //           It is meant to ensure that attributes set in the simulation script
  //            are used instead of default values where calculations are performed.
  // PARAMETERS:
  // +None
  // RETURN   ::void:NULL
  // **/ 
  void 
  RoutingProtocol::DistUnivSchedInit( )
  {
    PacketMetadata::Enable();
   
    int i, j, k;
    
    time( &simStartTime );

    numTimeSlots = (int)(simulationTime.GetDouble()/timeSlotDuration.GetDouble()); 
    
    setInitBatteryPowerLevel( batteryPowerLevel );  // this is just set to output to stat file at end
    setPowerUsed( 0.0 );

    if( DIST_UNIV_SCHED_INIT_DEBUG )
    {
      std::cout<<"In DistUnivSchedInit:\n";
      std::cout<<"\tSimulation Time = " << simulationTime.GetSeconds() << "\n";
      std::cout<<"\tNumber of time slots = " << numTimeSlots << "\n";
      if( GLOBAL_KNOWLEDGE )
      {
        std::cout<<"\tGlobal Knowledge being used.\n";
      }
      else
      {
        std::cout<<"\tGlobal Knowledge not being used.\n";
      }
      if( isLINE_NETWORK() )
      {
        std::cout<<"\tLine network exchange being used.\n";
      }
      if( isBRUTE_FORCE() )
      {
        std::cout<<"\tBrute Force exchange being used.\n";
      }
      std::cout<<"\tnumber of nodes = " << numNodes << "\n";
      std::cout<<"\tnumber of commodities = " << numCommodities << "\n";
      std::cout << "Initial Battery Power = " << getInitBatteryPowerLevel() << "\n";
    }
    
    numTrxPoss = pow( ((double)2), numNodes );

    
    // BEGIN ALLOCATING MEMORY FOR VARIABLES
    
    queues = new DropTailQueue [numCommodities];

    otherBacklogs = new int* [numNodes];
    globalBacklogs = new int* [numNodes];
    //discreteBacklogs = new int* [numNodes];
    validInfo = new bool [numNodes];
    rates = new double [numNodes];
    globalRates = new double [numNodes];
    globalChosenRates = new double [numNodes];
    timeStamps = new Time* [numNodes];
    tempTimeStamps = new Time* [numNodes];
    nextTimeStamps = new Time* [numNodes];
    forwardNodeCtrlInfo = new bool [numNodes];
    nodeInfoFirstRcv = new bool [numNodes];
    validInfo = new bool [numNodes];
    diffBacklogs = new int** [numNodes];
    weights = new int*[numNodes];
    weightCommodity = new int* [numNodes];
    possTrx = new bool *[numNodes];
    
    channelRates = new double* [numNodes];
		globalChannelRates = new double* [numNodes];
    
    channelRateIndex = new int* [numNodes];
    tempChannelRates = new double** [numNodes];
    inputRate = 0.0;
    lastChosenResAllocScheme = 0;
    numControlPacketsSent = 0;
    numControlPacketsRcvd = 0;
    numDataAcksSent = 0;
    numDataAcksRcvd = 0;
    numCollisions = 0;
    numCtrlPcktCollisions = 0;
    numDataPcktCollisions = 0;
    numDataAckCollisions = 0;
    numCollisionsThisTimeSlot = 0;
    numWaitForAckTimeouts = 0;
    numExchangeRounds = numNodes-1;
    dataPacketsSent = new int [numNodes];
    dataPacketsRcvd = new int [numNodes];
    dataPacketsRcvdThisSlot = new int [numNodes];
    packetsDropped = 0;
    rcvdAckThisTimeSlot = false;
    trxThisTimeSlot = false;
    numIncorrectQueues = 0;
    numIncorrectRates = 0;
    lastSnr = 0.0;

    packetsRcvThisTimeSlot = new int [numCommodities];
    packetsTrxThisTimeSlot = new int [numCommodities];
    
    numBackoff = 0;
    
    controlPacketsSentThisTimeSlot = 0; 
    controlPacketsRcvdThisTimeSlot = 0;

    nodeCoordX = new double [numNodes];
    nodeCoordY = new double [numNodes];
    nodeDistance = new double [numNodes];

    for( i = 0; i < (int)numCommodities; i++ )
    {
      queues[i].setM_backlog(0);
      packetsRcvThisTimeSlot[i] = 0;
      packetsTrxThisTimeSlot[i] = 0;
    }
    
    for( i = 0; i < (int)numNodes; i++ )
    {
      
      rates[i] = 0.0;
      globalRates[i] = 0.0;
      globalChosenRates[i] = 0.0;
      nodeDistance[i] = 0.0;

      Ptr<MobilityModel> mobility = NodeList::GetNode((uint32_t)i)->GetObject<MobilityModel> ();
      Vector pos = mobility->GetPosition();
      if( DIST_UNIV_SCHED_INIT_DEBUG )
      {
        std::cout<<"Node " << i << " position: x = " << pos.x << ", y = " << pos.y << "\n";
      }

      nodeCoordX[i] = pos.x;
      nodeCoordY[i] = pos.y;
      
      otherBacklogs[i] = new int[numCommodities];
      globalBacklogs[i] = new int[numCommodities];
      //discreteBacklogs[i] = new int[numCommodities];
      for( j = 0; j < (int)numCommodities; j++ )
      {
        otherBacklogs[i][j] = 0;
        globalBacklogs[i][j] = 0;
        //discreteBacklogs[i][j] = 0;
      }
      diffBacklogs[i] = new int* [numNodes];
      weights[i] = new int[numNodes];
      weightCommodity[i] = new int[numNodes];
      possTrx[i] = new bool[numNodes];
      channelRates[i] = new double[numNodes];
      globalChannelRates[i] = new double[numNodes];
      channelRateIndex[i] = new int[numNodes];
      tempChannelRates[i] = new double* [numNodes];
      timeStamps[i] = new Time[numNodes];
      tempTimeStamps[i] = new Time[numNodes];
      nextTimeStamps[i] = new Time[numNodes];
      forwardNodeCtrlInfo[i] = false;
      nodeInfoFirstRcv[i] = false;
      if( i != getNodeId() )
        validInfo[i] = false;
      else
        validInfo[i] = true;
      for( j = 0; j < (int)numNodes; j++ )
      {
        diffBacklogs[i][j] = new int[numCommodities];
        for( k = 0; k < (int)numCommodities; k++ )
        {
          diffBacklogs[i][j][k] = 0;
        }
        weights[i][j] = 0;
        weightCommodity[i][j] = 0;
        possTrx[i][j] = false;
        channelRates[i][j] = 0.0;
        channelRateIndex[i][j] = 0;
        globalChannelRates[i][j] = 0.0;
        timeStamps[i][j] = (Time)0;
        tempTimeStamps[i][j] = (Time)0;
        nextTimeStamps[i][j] = (Time)0;
        
        tempChannelRates[i][j] = new double[(int)numTrxPoss];
        for( k = 0; k < (int)numTrxPoss; k++ )
        {
          tempChannelRates[i][j][k] = 0.0;
        }
      }
      dataPacketsSent[i] = 0;
      dataPacketsRcvd[i] = 0;
      dataPacketsRcvdThisSlot[i] = 0;
    }		

    for( i = 0; i < (int)DIST_UNIV_SCHED_MAX_QUEUE_DIFF; i++ )
    {
      amountQueuesWrong[i] = 0;
    }
    for( i = 0; i < (int)DIST_UNIV_SCHED_MAX_RATE_DIFF; i++ )
    {
      amountRatesWrong[i] = 0;
      chosenRatesWrong[i] = 0;
    }

    // Allocate distUnivSchedStats
    stats = new DistUnivSchedStats ( (int)simulationTime.GetSeconds(), (int)numCommodities, TIME_STATS );
    
    trxDelayRV = UniformVariable(0.0, (double)maxBackoffWindow/1000.0);
    
    // DONE ALLOCATING MEMORY FOR VARIABLES

    // open channel rates file if necessary
    if( OUTPUT_RATES && getNodeId() == 0 )
    {
      char buf[32];
		  sprintf(buf, "distUnivSchedChannelRates.csv");
  		channelRatesFd = fopen(buf, "w");
    }

    //double numBitsTotalDissem = getQBits()*(((getNumNodes()-1)*getNumNodes())/2)*2;
    double numBitsTotalDissem = getQBits()*getNumNodes()*(getNumNodes()-1)*2;
    setQueueDissemTotalTime( Time::FromDouble(numBitsTotalDissem/(getControlInfoExchangeChannelRate()*1000000.0), Time::S ) );

    if( DIST_UNIV_SCHED_LINE_NET_DEBUG )
    {
      std::cout<<"Total number of bits for queue dissemination = " << numBitsTotalDissem << "...rate = " << getControlInfoExchangeChannelRate() << "\n";
      std::cout<<"Queue Dissemination Total Time = " << Seconds(getQueueDissemTotalTime()) <<" and " << numBitsTotalDissem/(getControlInfoExchangeChannelRate()*1000000.0) << "\n";
    }
    
    // schedule print stats function for right before simulation is done
    Simulator::Schedule( simulationTime - NanoSeconds(1), &RoutingProtocol::PrintStats, this );
    
    // schedule collect queue lengths function to start after 1 second delay
    //   this function is needed to make fair comparisons of avg backlog between sims with different TSDs
    Simulator::Schedule( Seconds(1.0), &RoutingProtocol::CollectQueueLengths, this );
  }


  // /**
  // FUNCTION: StartTimeSlot
  // LAYER   : NETWORK
  // PURPOSE : Called at beginning of control information exchange period.  
  //				Resets all status of 
  //				Sets state to SEND_OWN_INFO, and sends first message
  //				of type DIST_UNIV_SCHED_SEND_CONTROL_INFO
  //				to start control info exchange period, and sends message
  //				to "complete" (transition to data trx phase) time slot at the correct time.
  // PARAMETERS:
  // +None
  // RETURN   ::void:NULL
  // **/ 
  void 
  RoutingProtocol::StartTimeSlot( )
  {
    if( DIST_UNIV_SCHED_START_TIME_SLOT_DEBUG )
    {    
      std::cout<<"Node " << getNodeId() << " in RoutingProtocol::StartTimeSlot() at time = " << Simulator::Now().GetSeconds() << "\n";
    }
  
    if( getBatteryPowerLevel() < 0.0 && getNodeLifetime().GetSeconds() == 0.0 )
    {
       setNodeLifetime( Simulator::Now() );
    }
  
    int i, j;
    j = 0;
	
    if( DIST_UNIV_SCHED_START_TIME_SLOT_DEBUG )
    {    
      Time currentTime = (Time)Simulator::Now().GetSeconds();
      std::cout<<"Node " << getNodeId() << " entered StartTimeSlot() at time = " << currentTime << "\n";
      
      for( i = 0; i < getNumCommodities(); i++ )
      {  
        std::cout<<"Node " << getNodeId() << ":  when entering StartTimeSlot():  \n\tqueues["<<i<<"].backlog = "<<queues[i].getM_backlog();
        std::cout<<"\n\tpackets_rcv_this_time_slot["<<i<<"] = "<<getPacketsRcvThisTimeSlot(i)<<"\n\tpackets_trx_this_time_slot["<<i<<"] = "<< getPacketsTrxThisTimeSlot(i) <<"\n";
      }
    }

	// Update Queues
    for( i = 0; i < (int)getNumCommodities(); i++ )
    {
        queues[i].setM_backlog( queues[i].getM_backlog() + ( getPacketsRcvThisTimeSlot(i)  - getPacketsTrxThisTimeSlot(i) ) );
        char buf [250];
        sprintf( buf, "Node %i:  After updating queues in StartTimeSlot():  queues[%i].backlog is less than zero!\n\
                \tqueues[%i].backlog = %i\n\
                \tpackets_rcv_this_time_slot[%i] = %i\n\
                \tpackets_trx_this_time_slot[%i] = %i\n", 
                getNodeId(), i,
                i, queues[i].getM_backlog(),
                i, packetsRcvThisTimeSlot[i],
                i, packetsTrxThisTimeSlot[i] );

        NS_ASSERT_MSG( queues[i].getM_backlog() >= 0, buf );
        setPacketsRcvThisTimeSlot( i, 0 ); // packets_rcv_this_time_slot[i] = 0;
        setPacketsTrxThisTimeSlot( i, 0 ); // packets_trx_this_time_slot[i] = 0;
        setOtherBacklogs( getNodeId(), i,  getOtherBacklogs( getNodeId(), i ) +  queues[i].getM_backlog() );
    }

  // reset control info exchange state
  setControlInfoExchangeState(SEND_OWN_INFO);
  // reset time to time out waiting for ack
  setWaitForAckTimeoutTime ( (Time)(Simulator::Now().GetSeconds() + getWaitForAckTimeoutWindow()) );

  // update timeSlotNum
  setTimeSlotNum(getTimeSlotNum()+1);

  // send packet to start new round of scheduling 
  if( isGLOBAL_KNOWLEDGE() || isLINE_NETWORK() || isBRUTE_FORCE() ) 
  {
/*    double delay = 0.0;
    if( isLINE_NETWORK() )
    {
      // pause amount of time it takes to exchange control info up and down the line network
      //   time depends on size of the network and number of q bits
      delay = 0.0;
    }*/
    if( getNodeId() == 0 )
    {
      // no delay...exchange control info immediately...complete time slot messages sent from ExchangeControlInfo* functions  
      Simulator::Schedule ( Seconds (0.0), &RoutingProtocol::GlobalExchangeControlInfoForward, this, (Ptr<Packet>)0 );
    }
    if( isLINE_NETWORK() )
    {
      // schedule all of the nodes' complete time slot events here
      Time delay = getQueueDissemTotalTime();
      Simulator::Schedule ( delay, &RoutingProtocol::CompleteTimeSlot, this );
    }
    if( isBRUTE_FORCE() )
    {
      Time delay = (Time)(getNumExchangeRounds()*getQueueDissemTotalTime()); 
      Simulator::Schedule ( delay, &RoutingProtocol::CompleteTimeSlot, this );
    }
    // set time that next time slot will start
    setNextTimeSlotStartTime ( Simulator::Now() + getTimeSlotDuration() );
    //std::cout<<"Setting next time slot start time to " << Seconds(Simulator::Now() + getTimeSlotDuration()) << "\n";
  }
  else // not using global knowledge or line network algorithm
  {
    // add on random backoff delay to avoid collisions
    Time delay = Seconds(trxDelayRV.GetValue());
    
    Time timeLeftInSlot = getTimeSlotDuration() - getControlInfoExchangeTime();
    if(DIST_UNIV_SCHED_START_TIME_SLOT_DEBUG)
    {
      std::cout<<"Node " << getNodeId() << ": will send next control info exchange packet at " << (Simulator::Now().GetSeconds() + delay) << "\n";
      std::cout<<"\ttimeLeftInSlot =  " << timeLeftInSlot << "\n";
    }
    
    // perform control info exchange
    Simulator::Schedule ( delay, &RoutingProtocol::SendControlInfoPacket, this );
    
    // schedule completion of time slot
    // pause for amount of time control info exchange time
    Simulator::Schedule ( getControlInfoExchangeTime(), &RoutingProtocol::CompleteTimeSlot, this );
   
    // set time that this time slot will complete 
    setTimeSlotCompleteTime( (Time)(Simulator::Now().GetSeconds() + getControlInfoExchangeTime()) );
    // set time that next time slot will start
    setNextTimeSlotStartTime ( Simulator::Now() + getTimeSlotDuration() );
    
    if(DIST_UNIV_SCHED_START_TIME_SLOT_DEBUG)
    {
      std::cout<< "Node " << getNodeId() << ": set nextTimeSlotStartTime to " << getNextTimeSlotStartTime() << "\n";
    }
  }
  
  // reset control information exchange variables for next round of exchanges
  setControlPacketsSentThisTimeSlot(0);
  setControlPacketsRcvdThisTimeSlot(0);
  setNumCollisionsThisTimeSlot(0); 
  setTimeSlotMessageSent(false);
  setTimeToTrxMoreData( true );

  for( i = 0; i < getNumNodes(); i++ )
  {
    setDataPacketsRcvdThisSlot(i, 0);
    setNodeInfoFirstRcv(i, true);
    if( i != getNodeId() )
      setValidInfo( i, false );
    else
      setValidInfo( getNodeId(), true );
  }
  setControlInfoExchangeState(SEND_OWN_INFO);
  
  if( DIST_UNIV_SCHED_START_TIME_SLOT_DEBUG )
  {
   // printf("Node %i: Sent new message to mark end of time slot...next control info exchange should start after %ld delay\n", 
   //      getNodeId(), timeSlotDuration);
   // printf("\t....about to exit DistUnivSchedHandleProtocolEvent.\n\n\n\n");
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
//        Makes decisions for commodity selection and resource
//        allocation according to universal scheduling algorithm.
//        Sets state to DATA_TRX, calls SendDataPacket()
//        to start date transmission period, and sends message
//        to start new time slot at the correct time.
// PARAMETERS:
// +none:
// RETURN   ::void:NULL
// **/
  void
  RoutingProtocol::CompleteTimeSlot()
  {
    if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << ": in RoutingProtocol::CompleteTimeSlot() at time = " << Simulator::Now().GetSeconds() << "\n";
    }

    /*
    if( getNodeId() == 0 && timeSlotNum%100 == 0 && !DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
    {
        printf("---------------------------------------------------------------------------------\n");
        printf("Node %i Performing necessary actions for time slot #%d\n", getNodeId(), timeSlotNum);
        printf("---------------------------------------------------------------------------------\n");
    }
    */

    if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
    {
        printf("New Time Slot...Now().GetSeconds() returns %f in seconds\n", (double)Simulator::Now().GetSeconds());
        printf("Number of collisions this time slot = %i\n", getNumCollisionsThisTimeSlot());
        printf("Number of control packets sent this time slot = %i\n", getControlPacketsSentThisTimeSlot());
        printf("Number of control packetsd received this time slot = %i\n", getControlPacketsRcvdThisTimeSlot());
    }

    int i, j, k, m;

    int tempChannelRateIndex;
    double tempChannelRate;

    // determine channel rates
    for( i = 0; i < getNumNodes(); i++ )
    {
        for( j = 0; j < getNumNodes(); j++ )
        {
            if( i != j )
            {
              GetChannelRateFromCoordinates( getNodeCoordX(i),
                                             getNodeCoordY(i),
                                             getNodeCoordX(j),
                                             getNodeCoordY(j),
                                             &tempChannelRate,
                                             &tempChannelRateIndex );
              if( isGLOBAL_KNOWLEDGE() || isLINE_NETWORK() || isBRUTE_FORCE() )
              {
                setChannelRates( i, j, tempChannelRate );
                if( i == getNodeId() )
                {
                  setChannelRateIndex( getNodeId(), j, tempChannelRateIndex );
                }
              }
              else
              {
                setGlobalChannelRates( i, j, tempChannelRate );
              }
            }
            else
            {
              // this channel rate index is to and from this node...set rate to 0, and rateIndex to 0
              if( i == getNodeId() )
              {
                setChannelRates(i, j, 0.0);
                setChannelRateIndex(i, j, 0);
              }
            }
        }
    }

    if( CALCULATE_RADIO_RANGES )
    {
      double xDistance, yDistance, nodeDistance;
      if( getNodeId() == 1 )
      {
        xDistance = abs((int)(getNodeCoordX(0) - getNodeCoordX(1)));
        yDistance = abs((int)(getNodeCoordY(0) - getNodeCoordY(1)));
      
        nodeDistance = sqrt( xDistance*xDistance + yDistance*yDistance );
        std::cout<< globalChannelRates[1][0] << ", " << nodeDistance << ", " << lastSnr << "\n";
      }
    }

    // collect statistics
    if( isRcvdAckThisTimeSlot() )
    {
        stats->setNumTimeSlotsRcvdAck ( stats->getNumTimeSlotsRcvdAck() + 1.0 );
    }
    else
    {
        stats->setNumTimeSlotsAckNotRcvd( stats->getNumTimeSlotsAckNotRcvd() + 1.0 );
        if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
        {
            printf( "Node %i:    Did not receive any ack for control info this time slot.\n", getNodeId() );
        }
    }

    // if using GLOBAL knowledge, then "otherBacklogs" contains the global knowledge values
    //        we need to fill in "globalBacklogs" with this information, so if VARY_QUEUE_INFO is set,
    //        we can keep track of and output how much the backlogs are varied by
    //        Similarly, "globalChannelRates" must be populated with "channelRates" as well
    // if not using GLOBAL knowledge, then "globalBacklogs" and "globalChannelRates" are already
    //        filled in with the correct global knowledge (so we don't need to do that here)
    // if using LINE_NETWORK or BRUTE_FORCE, the "otherBacklogs" contains the actual backlog information
    if( GLOBAL_KNOWLEDGE )
    {
        for( i = 0; i < getNumNodes(); i++ )
        {
            for( j = 0; j < getNumNodes(); j++ )
            {
                setGlobalChannelRates( i, j, getChannelRates(i, j) );
            }

            for( j = 0; j < getNumCommodities(); j++ )
            {
                setGlobalBacklogs( i, j, getOtherBacklogs(i, j) );
            }
        }
    }

    if( VARY_CORRECT_QUEUE_INFO )
    {
      VaryQueues();
    }
    if( VARY_CORRECT_RATE_INFO )
    {
      VaryRates();
    }
    
    // STEP : Observe queue backlogs and topology state and choose I(t) and mu(t) to maximize equation
    //            weights[node][node]
    //            diffBacklogs[node][node][commodity]

    //    find max-weights using differential backlogs
    for( i = 0; i < getNumNodes(); i++ )
    {
      for( j = 0; j < getNumNodes(); j++ )
      {

        if(DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG)
        {
           std::cout<<"\totherBacklogs[" << i << "][" << j << "] = " << getOtherBacklogs( i, j ) << "\n";
        }

        setWeights(i, j, -1);
        for( int c = 0; c < getNumCommodities(); c++ )
        {
          if( isLINE_NETWORK() || isBRUTE_FORCE() )
          {
            setDiffBacklogs( i, j, c, ( DiscreteQValue( getOtherBacklogs(i, c), getQBits() ) - DiscreteQValue( getOtherBacklogs(j, c), getQBits() ) ) );
          }
          else if( isVARYING_Q_BITS() )
          {
            // other backlogs should already have discrete values in them...placed there before exchanging occurs
            setDiffBacklogs( i, j, c, (getOtherBacklogs(i, c) - getOtherBacklogs(j, c)) );
          }
          else
          {
            setDiffBacklogs( i, j, c, (getOtherBacklogs(i, c) - getOtherBacklogs(j, c)) );
          }

          //if(DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG)
          //{
             //printf("node %i:    otherBacklogs[%i][%i] = %i, otherBacklogs[%i][%i] = %i\n",
                     //getNodeId(), i, c, getOtherBacklogs(i, c),
                     //j, c, getOtherBacklogs(j, c));
             //printf("node %i:    diffBacklog[%i][%i][%i] = %i\n",
                       //getNodeId(), i, j, c, getDiffBacklogs(i, j, c));
          //}

          if( getDiffBacklogs(i, j, c) < 0 )
          {
            setDiffBacklogs(i, j, c, 0);
          }
          if( getDiffBacklogs(i, j, c) > getWeights(i, j) )
          {
            setWeights(i, j, diffBacklogs[i][j][c]);
            setWeightCommodity(i, j, c);
          }
        }

        //if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
        //{
          //printf("weights[%i][%i] = %i\n", i, j, getWeights(i, j));
        //}
      }
    }
    if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
    {
      for( j = 0; j < getNumNodes(); j++ )
      {
        printf("node %i to node %i:    commodity chosen = %i\n", getNodeId(), j, getWeightCommodity(getNodeId(), j));
      }
    }

    // determine resource allocation by choosing which nodes can transmit
    // set capacity matrix for topology and all resource allocation schemes
    if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
    {
      printf("numTrxPoss = %f\n", getNumTrxPoss());
    }
    for( i = 0; i < getNumNodes(); i++ )
    {
      for( j = 0; j < getNumNodes(); j++ )
      {
        for( k = 1; k < numTrxPoss; k++ ) // k = 0 is scenario where no nodes are transmitting...should be useless
        {
                // FOR GENERAL TOPOLOGIES:
          unsigned trxScheme = (unsigned)k;
          if( ((trxScheme>>i)&(unsigned)1) == (unsigned)1 ) // node transmits if its place in the binary trx scheme number == 1
          {                                     //        i.e. 3 nodes:    k = 2 -> trxScheme = 010, so node 2 trx and nodes 1 and 3 are silent
            //printf("trxScheme = %u, i = %i \ttrxScheme>>i & 1 == 1\n", trxScheme, i);
            bool causedInterference = false;
            for( m = 0; m < getNumNodes(); m++ )
            {
              if( m == i )
              {
                continue;
              }
              // this accounts for one-hop interference
              //    i.e. if node m and node i are both supposed to transmit in scheme k, and they are within range of each other,
              //             then both rates are set to zero to prevent interference
              if( ((trxScheme>>m)&(unsigned)1) == (unsigned)1 && getChannelRates(m, i) > 0.0 )
              {
                //    printf("trxScheme = %u, i = %i, m = %i, \tcaused interference\n", trxScheme, i, m);
                            setTempChannelRates(i, j, k,    0.0);
                            causedInterference = true;
                break;
              }
              // trying to account for two-hop interference
              //    i.e. if node m and node i are both supposed to transmit, and they share a neighbor, 
              //             then both are set to zero to prevent interference
              if( ((trxScheme>>m)&(unsigned)1) == (unsigned)1 && getChannelRates(m, j) > 0.0 && getChannelRates(i, j) > 0.0 )
              {
                setTempChannelRates(i, j, k, 0.0);
                causedInterference = true;
                break;
              }
            }
            if( !causedInterference )
            {
              setTempChannelRates(i, j, k, getChannelRates(i, j));
            }
          }
          else
          {
            //printf("trxScheme = %u, i = %i \ttrxScheme>>i & 1 == 0\n", trxScheme, i);
            setTempChannelRates(i, j, k, 0.0);
          }
          //printf("tempChannelRate[%i][%i][%i] = %i\n", i, j, k, tempChannelRates[i][j][k]);
        }
      }
    }

    if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
    {
      for( k = 0; k < getNumTrxPoss(); k++ )
      {
        printf( "k = %i:\n", k );
        for( i = 0; i < getNumNodes(); i++ )
        {
          for( j = 0; j < getNumNodes(); j++ )
          {
            printf( "\t%f", getTempChannelRates(i, j, k) );
          }
          printf( "\n" );
        }
        printf( "\n" );
      }
    }

    //    choose resource allocation scheme (I(t)) that maximizes sum of transmission*weight for all i and j among those possible
    int chosenResAllocScheme = 0;
    double tempSum, maxSum = -1;
    int numValidSchemes = 0, validSchemes[1024];
    for( k = 1; k < getNumTrxPoss(); k++ )
    {
        tempSum = 0.0;
        for( i = 0; i < getNumNodes(); i++ )
        {
            for( j = 0; j < getNumNodes(); j++ )
            {
                tempSum = tempSum + (double)(getTempChannelRates(i, j, k)*(double)getWeights(i, j));
            }
        }
        if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
        {
            printf("tempSum for k = %i is %f\n", k, tempSum);
        }
        if( tempSum > maxSum )
        {
            maxSum = tempSum;
            chosenResAllocScheme = k;
            numValidSchemes = 0;
            validSchemes[numValidSchemes++] = k;
        }
        else
        {
            if( tempSum == maxSum )
            {
                if( numValidSchemes > 1024 )
                {
                  std::cout<<"Number of valid resource allocation schemes > 1024...need to increase memory allocation\n";
                  exit(-1);
                }
                validSchemes[numValidSchemes++] = k;
            }
        }
    }
    if( numValidSchemes > 1 )
    {
        // want to break ties deterministically
        if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
        {
            printf("numValidSchemes = %i\n", numValidSchemes);
        }
        // in case of tie, always choose first valid scheme
        //  (breaking of ties can be done arbitrarily, and this should cause
        //    nodes to make same decisions when using same information)
        chosenResAllocScheme = validSchemes[0];
       
        if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
        {
            printf("node %i: resAllocScheme # = %i...had to break tie\n", getNodeId(), chosenResAllocScheme);
        }
    }
    else
    {
        if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
        {
            printf("node %i: resAllocScheme # = %i...no tie\n", getNodeId(), chosenResAllocScheme);
        }
    }

    //set up rates matrix for chosen resource allocation scheme
    for( j = 0; j < getNumNodes(); j++ )
    {
        if( getTempChannelRates(getNodeId(), j, chosenResAllocScheme)  == 0.0 )
        {
          if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG)
          {
            printf("setting rate for [%i][%i] to 0\n", j, chosenResAllocScheme);
          }
          setRates(j, 0.0);
          setChannelRateIndex( getNodeId(), j, 0); // TODO : CHECK THIS - does this need to be set here?    
        }
        else
        {
          if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG)
          {
              printf("setting rate for [%i][%i] to %f\n", j, chosenResAllocScheme, getTempChannelRates(getNodeId(), j, chosenResAllocScheme));
          }
          setRates(j, getTempChannelRates(getNodeId(), j, chosenResAllocScheme));
        }
    }


    if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
    {
        //printf("numValidSchemes = %i, random number generated = %f\n", numValidSchemes, temp2);
        //printf("chosenResAllocScheme = %i, maxSum = %f\n", chosenResAllocScheme, maxSum);

        printf( "node %i: resAllocScheme # = %i .... time = %i\n", getNodeId(), chosenResAllocScheme, (int)Simulator::Now().GetSeconds() );
        //for( i = 0; i < getNumNodes(); i++ )
        //{
            //printf("%f\t", getRates(i));
        //}
        //printf("\n");
    }

    
    // SEND PACKETS

    double tempProduct;
    double tempProductMax = -1.0;
    int receivingNode = -1;
    int numTied = 0, tiedRecipients[50];
    for( j = getNumNodes()-1; j >=0; j-- ) // node calling this function (getNodeId()) is the transmitting node...j is the receiving node
    {
        // choose neighbor with largest rate*backlog product
        //        node will transmit to this node (until buffer is empty)
        tempProduct = (double)(getRates(j)*(double)getWeights(getNodeId(), j));
        if( tempProduct > tempProductMax )
        {
            tempProductMax = tempProduct;
            receivingNode = j;
            numTied = 0;
            tiedRecipients[numTied++] = j;
        }
        else
        {
            if( tempProduct == tempProductMax )
            {
                if( numTied > 50 )
                {
                    fprintf(stderr, "Number of valid resource allocation schemes > 50...need to increase memory allocation\n");
                }
                tiedRecipients[numTied++] = j;
            }
        }
    }
    if( numTied > 1 )
    {
        // randomly break ties
        if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
        {
            printf("number of tied recipients = %i\n", numTied);
        }

        // Always choose first of tied recipients
        //  This way nodes will make same choices when using same information
        receivingNode = tiedRecipients[0];

        if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
        {
            printf("Node %i: chosen recipient = %i...had to break tie\n", getNodeId(), receivingNode);
        }
    }
    else
    {
        if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
        {
            printf("Node %i:    chosen recipient = %i...no tie\n", getNodeId(), receivingNode);
        }
    }

    // Chosen recipient is decided, so we will zero out any available rates to other nodes
    for( i = 0; i < getNumNodes(); i++ )
    {
        if( i != receivingNode )
        {
            setRates(i, 0.0);
        }
    }
    
    
    // determine the number of packets that the node should be able to transmit in this time slot...for Debug/Validation purposes
    double numBitsCanTrx, numPacketsCanTrx;
    Time timeLeftInSlot = getTimeSlotDuration() - getControlInfoExchangeTime();
    // the number of packets that a node can send in this time slot is the rate
    //    bits = rate x time    (rates are in Mbps)
   
    numBitsCanTrx = getRates(receivingNode)*timeLeftInSlot.GetSeconds()*1000000.0;
    if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
    {
      std::cout<<"numBitsCanTrx = " << numBitsCanTrx << "...timeLeftInSlot.GetSeconds() = " << timeLeftInSlot.GetSeconds() << "...getRates() = " << getRates(receivingNode) << "\n";
    }
 
    numPacketsCanTrx = numBitsCanTrx/numBitsPerPacket;
    
    if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
    {
        printf( "Node %i:    attempting to transmit %f packets\n", getNodeId(), numPacketsCanTrx );
    }
    // Change State Appropriately
    setControlInfoExchangeState(DATA_TRX);

    // Start the process of sending data packets.    First call to SendDataPacket function is done here...following calls are done
    //     in Data Packet ACK handler, i.e. each successive packet is sent after last one is acknowledged
    if( getRates(receivingNode) > 0.0 )
    {
      if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
      {
          printf( "Node %i:    Time slot %i: Sending data w/ dest. %i to node %i at rate %f\n",
                   getNodeId(), getTimeSlotNum(), getWeightCommodity(getNodeId(), receivingNode),
                   receivingNode, getRates(receivingNode) );
      }

      SendDataPacket( receivingNode, getWeightCommodity(getNodeId(), receivingNode) );

      trxThisTimeSlot = true;
    }
    else
    {
      trxThisTimeSlot = false;
    }


    // send message to mark beginning of next time slot

    if( isGLOBAL_KNOWLEDGE() )
    {
        // pause for time slot duration to allow for transmission of packets...also ensures right number of time slots in simulation
      Simulator::Schedule ( getTimeSlotDuration(), &RoutingProtocol::StartTimeSlot, this );
    }
    else if( isLINE_NETWORK() || isBRUTE_FORCE() )
    {
      if( DIST_UNIV_SCHED_LINE_NET_DEBUG || DIST_UNIV_SCHED_BRUTE_FORCE_DEBUG )
      {
        std::cout<<"Scheduling next StartTimeSlot to begin at " << Seconds( getNextTimeSlotStartTime() ) << "\n";
      }
      Simulator::Schedule( getNextTimeSlotStartTime()-Simulator::Now(), &RoutingProtocol::StartTimeSlot, this );
    }
    else // distributed algorithm
    {
        // pause for time slot duration to allow for transmission of packets...also ensures right number of time slots in simulation
      Time delay = getTimeSlotDuration() - getControlInfoExchangeTime();
      Simulator::Schedule ( delay, &RoutingProtocol::StartTimeSlot, this );

      if( DIST_UNIV_SCHED_COMPLETE_TIME_SLOT_DEBUG )
      {
        std::cout<< "\tscheduling StartTimeSlot() to begin after delay of " << delay <<"\n";
      }        
    }

    if( getNodeId() == 0 && !GLOBAL_KNOWLEDGE && !isLINE_NETWORK() &&!isBRUTE_FORCE() ) // exchange global information to compare to the information disseminated in control info exchange period
    {
      if( !isLINE_NETWORK() && !isBRUTE_FORCE() )
      {
        // perform data exchange 
        // no delay...exchange control info immediately...complete time slot messages sent from ExchangeControlInfo* functions    
        Simulator::Schedule ( Seconds (0.0), &RoutingProtocol::GlobalExchangeControlInfoForward, this, (Ptr<Packet>)0 );
      }
      else
      {
        Simulator::Schedule ( Seconds( getNextTimeSlotStartTime() - Simulator::Now() ), &RoutingProtocol::GlobalExchangeControlInfoForward, this, (Ptr<Packet>)0 );
      }
    }

    // sending a message to check the exchanged control information against actual (global) control information
    if( (!isGLOBAL_KNOWLEDGE() && !isLINE_NETWORK() && !isBRUTE_FORCE() ) || VARY_CORRECT_QUEUE_INFO || VARY_CORRECT_RATE_INFO )
    {
//      std::cout<<"Scheduling ExchangedInfoMatchesGlobal\n";
      Simulator::Schedule ( NanoSeconds (1), &RoutingProtocol::ExchangedInfoMatchesGlobal, this );
    }

    if( OUTPUT_RATES && getNodeId() == 0 ) // if interested in 
    {
      OutputChannelRates();
    }
  }

// /**
// FUNCTION: OutputChannelRates
// LAYER   : NETWORK
// PURPOSE : Called each second throughout the simulation to gather snapshots of queue lengths.
//           The current queue lengths after each second will be added to the queue length sum
//           statistic to allow for fair comparison of avg total occupancy between simulations
//           using different time slot durations. (Avoids oversampling when TSD is lower)
//
// PARAMETERS: none
// 
// RETURN   ::void:NULL
// **/
  void
  RoutingProtocol::OutputChannelRates()
  {
    fprintf( channelRatesFd, "Time Slot %i\n", timeSlotNum );
    int i, j;
    for( i = 0; i < numNodes; i++ )
    {
      for( j = 0; j < numNodes; j++ )
      {
        if( GLOBAL_KNOWLEDGE ) // output global channel rates being used
        {
          fprintf( channelRatesFd, "%.1f,\t", globalChannelRates[i][j] );
        }
        else // output channel rates from ciet
        {
          fprintf( channelRatesFd, "%f,\t", channelRates[i][j] );
        }
      }
      fprintf( channelRatesFd, "\n" );
    }
    return;
  }

// /**
// FUNCTION: CollectQueueLengths
// LAYER   : NETWORK
// PURPOSE : Called each second throughout the simulation to gather snapshots of queue lengths.
//           The current queue lengths after each second will be added to the queue length sum
//           statistic to allow for fair comparison of avg total occupancy between simulations
//           using different time slot durations. (Avoids oversampling when TSD is lower)
//
// PARAMETERS: none
//              
// RETURN   ::void:NULL
// **/
  void 
  RoutingProtocol::CollectQueueLengths()
  {
    //std::cout<<"Node " << getNodeId() << " is in CollectQueueLengths()\n";
    int i;
    for( i = 0; i < numCommodities; i++ )
    {
      stats->setQueueLengthSum(i, stats->getQueueLengthSum(i) + (double)queues[i].getM_backlog() + (double)packetsRcvThisTimeSlot[i] - (double)packetsTrxThisTimeSlot[i]);
      if( TIME_STATS )
      {
        stats->setQueueLength( (int)Simulator::Now().GetSeconds(), i, (int)((double)queues[i].getM_backlog() + (double)packetsRcvThisTimeSlot[i] - (double)packetsTrxThisTimeSlot[i]) );

        // throughput this second is just the number of new packets that have reached the destination this second
        stats->setThroughput( (int)Simulator::Now().GetSeconds(), i, (int)(stats->getNumPacketsReachDestThisSecond( i )) );
        stats->setNumPacketsReachDestThisSecond( i, 0.0 );
      }
    }
    // call again in 1 second
    Simulator::Schedule( Seconds(1.0), &RoutingProtocol::CollectQueueLengths, this );
  }

// /**
// FUNCTION: GlobalExchangeControlInfoForward
// LAYER   : NETWORK
// PURPOSE : Function scheduled by simulator to be run.
//            If using global knowledge, the it is called right after the start time slot function
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
  RoutingProtocol::GlobalExchangeControlInfoForward( Ptr<Packet> packet )
  {
    if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " is in GlobalExchangeControlInfoForward() at time " << Simulator::Now().GetSeconds()  << "\n";

      if( isGLOBAL_KNOWLEDGE() )
      {
        std::cout<<"Using Global Knowledge.\n";
      }
      else
      {
        std::cout<<"Not Using Global Knowledge.\n";
      }
      if( isLINE_NETWORK() )
      {
        std::cout<<"Using Line Network Algorithm.\n";
      }
      if( isBRUTE_FORCE() )
      {
        std::cout<<"Using Brute Force Exchange Algorithm.\n";
      }
      else
      {
        std::cout<<"Not Using Line Network Algorithm.\n";
      }
      std::cout<<"Time Slot Duration = " << timeSlotDuration.GetSeconds() << ".\n";
    }
  
    int i, j;
    char *packetPtr, *origPacketPtr;
                              // node identifiers              backlogs                           channel rates                       coordinates 
    char *tempPacket = (char *)malloc( /*sizeof(int)*numNodes +*/ sizeof(int)*numNodes*numCommodities + sizeof(double)*numNodes*numNodes + sizeof(double)*numNodes*2 );

    uint32_t packetSize = 0;

    if( packet != 0 )
    {
      packetSize = packet->CopyData( (uint8_t *)tempPacket, (uint32_t)1000 );
      packetPtr = tempPacket;
    
      if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
      {
        std::cout<< "\tcopied " << packetSize <<" bytes from packet into local buffer\n";
      }
    }

    int temp_int;
    double temp_double;
    // extract queue backlogs from other nodes that have already included them
    if( getNodeId() != 0 )
    {
        if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
        {
            printf("packetPtr = %p\n", packetPtr);
        }
        for( i = 0; i < getNodeId(); i++ )
        {
            for( j = 0; j < getNumCommodities(); j++ )
            {
                if( GLOBAL_KNOWLEDGE )
                {
                  memcpy(&temp_int, packetPtr, sizeof(int));
                  setOtherBacklogs( i, j, temp_int );
                }
                else if( isLINE_NETWORK() )
                {
                  memcpy(&temp_int, packetPtr, sizeof(int));
                  setOtherBacklogs( i, j, temp_int );
                //  setDiscreteBacklogs( i, j, temp_int );
                }
                else
                {
                  memcpy(&temp_int, packetPtr, sizeof(int));
                  setGlobalBacklogs( i, j, temp_int );
                }
                if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
                {
                    printf("\tNode %i: (after extracting) otherBacklogs[%i][%i] = %i\n", getNodeId(), i, j, getOtherBacklogs(i,j));
                }
                packetPtr += sizeof(int);
            }
        }
        // extract channel rates
        if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
        {
            printf("packetPtr = %p\n", packetPtr);
        }
        for( i = 0; i < getNodeId(); i++ )
        {
            for( j = 0; j < getNumNodes(); j++ )
            {
                if( isGLOBAL_KNOWLEDGE() || isLINE_NETWORK() )
                {
                    memcpy(&temp_double, packetPtr, sizeof(double));
                    setChannelRates( i, j, temp_double );
                }
                else
                {
                    memcpy(&temp_double, packetPtr, sizeof(double));
                    setGlobalChannelRates( i, j, temp_double );
                }
                if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
                {
                    printf("\tNode %i: (after extracting) channelRates[%i][%i] = %f\n", getNodeId(), i, j, temp_double);
                }
                packetPtr += sizeof(double);
            }
        }
        // extract node coords
        for( i = 0; i < getNodeId(); i++ )
        {
          memcpy(&temp_double, packetPtr, sizeof(double));
          setNodeCoordX( i, temp_double );
          packetPtr += sizeof(double);

          memcpy(&temp_double, packetPtr, sizeof(double));
          setNodeCoordY( i, temp_double );
          packetPtr += sizeof(double);
          if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
          {
            printf("\tNode %i: (after extracting) nodeCoordX[%i] = %f,  nodeCoordY[%i] = %f\n", 
                   getNodeId(), i, getNodeCoordX(i), i, getNodeCoordY(i) );
          }
        }
    } 
    
    free( tempPacket );

    // make sure own backlogs are updated 
    for( i = 0; i < getNumCommodities(); i++ )
    {

      setOtherBacklogs( getNodeId(), i, queues[i].getM_backlog() );
      //setDiscreteBacklogs( getNodeId(), i, DiscreteQValues( queues[i].getM_backlog(), getQBits() ) );
      if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
      {
        printf("otherBacklogs[%i][%i] = %i, queues[%i].backlog = %i\n", 
                getNodeId(), i, getOtherBacklogs( getNodeId(), i ), 
                i, queues[i].getM_backlog());
      } 
    }
    
    if( !isGLOBAL_KNOWLEDGE() )
    {
      for( i = 0; i < getNumCommodities(); i++ )
      {    
        setGlobalBacklogs( getNodeId(), i, getOtherBacklogs(getNodeId(), i) );
      }
    }
      
    // update own coordinates
    Ptr<MobilityModel> mobility = NodeList::GetNode((uint32_t)getNodeId())->GetObject<MobilityModel> ();
    Vector pos = mobility->GetPosition();
    if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " position: x = " << pos.x << ", y = " << pos.y << " (after updating in Global Exchange Forward)\n";
    }

    setNodeCoordX(getNodeId(), (double)pos.x);
    setNodeCoordY(getNodeId(), (double)pos.y);

    if( getNodeId()+1 != getNumNodes() )
    {
      // allocate space for a backlog x commodity x node and rate x node x node and coordinate x node
      packetSize = ((getNumNodes()*getNumCommodities()*sizeof(int))+(sizeof(double)*getNumNodes()*getNumNodes())+(2*sizeof(double)*getNumNodes()));
   
      origPacketPtr = packetPtr = (char *)malloc(packetSize);

      // load all known backlogs into message (this node and all before it)
      for( i = 0; i < getNodeId()+1; i++ )
      {
        for( j = 0; j < getNumCommodities(); j++ )
        {
            if( isGLOBAL_KNOWLEDGE() )
           {
              temp_int = getOtherBacklogs( i, j );
                memcpy(packetPtr, &temp_int, sizeof(int));    
                if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
                {
                    printf("\tNode %i: loading otherBacklogs[%i][%i] = %i into packet\n", getNodeId(), i, j, getOtherBacklogs(i,j));
                }
            }
            else if( isLINE_NETWORK() )
            {
       //       temp_int = DiscreteQValue( getOtherBacklogs( i, j ), getQBits() );
              temp_int = getOtherBacklogs( i, j );
                memcpy(packetPtr, &temp_int, sizeof(int));    
                if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
                {
                    printf("\tNode %i: loading discrete backlog[%i][%i] = %i into packet\n", getNodeId(), i, j, temp_int);
                }
            }
            else
            {
              // in this case, global exchange is done with global values to evaluate errors in network state views
              temp_int = getGlobalBacklogs( i, j );
                memcpy(packetPtr, &temp_int, sizeof(int));    
                if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
                {
                    printf("\tNode %i: loading globalBacklogs[%i][%i] = %i into packet\n", getNodeId(), i, j, getGlobalBacklogs(i,j));
                }
            }
            //printf("\tvalue placed in packet: %i\n", (int)*packetPtr);
            packetPtr += sizeof(int);
        }
      }
      if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
      {
        printf("Loaded all known backlogs...\n");
      }

      // now load all known channel rates into message
      for( i = 0; i < getNodeId()+1; i++ )
      {
        for( j = 0; j < getNumNodes(); j++ )
        {
            if( isGLOBAL_KNOWLEDGE() || isLINE_NETWORK() )
            {
                if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
                {
                    printf("\tNode %i: loading channelRate[%i][%i] = %f into packet\n", getNodeId(), i, j, getChannelRates(i,j));
                }
                temp_double = getChannelRates(i,j);
                memcpy(packetPtr, &temp_double, sizeof(double));
            }
            else
            {
                if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG ) 
                {
                    printf("\tNode %i: loading globalChannelRate[%i][%i] = %f into packet\n", getNodeId(), i, j, getGlobalChannelRates(i,j));
                }
                temp_double = getGlobalChannelRates(i,j);
                memcpy(packetPtr, &temp_double, sizeof(double));
            }
            packetPtr += sizeof(double);
        }
      }
      if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
      {
        printf("\tLoaded channel rates\n");
      }    
      for( i = 0; i < getNodeId()+1; i++ )
      {
        if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
        {
            printf("\tNode %i: loading nodeCoord[%i].x = %f and nodeCoord[%i].y = %f into packet\n", 
                   getNodeId(), i, getNodeCoordX(i), i, getNodeCoordY(i) );
        }
        temp_double = getNodeCoordX(i);
        memcpy(packetPtr, &temp_double, sizeof(double));
        packetPtr += sizeof(double);

        temp_double = getNodeCoordY(i);
        memcpy(packetPtr, &temp_double, sizeof(double));
        packetPtr += sizeof(double);
      }
      if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
      {
          printf("\tLoaded node coordinates\n");
      }

      Ptr<Packet> p = Create<Packet>( (uint8_t const *)origPacketPtr, packetSize);
      free(origPacketPtr);

      Ptr<Node> node = NodeList::GetNode((uint32_t)getNodeId()+1);
      Ptr<RoutingProtocol> dusRp = node->GetObject<RoutingProtocol>();
      if ( dusRp == 0 )
      {
        std::cout<<"ERROR:  DUSRP == 0 in GlobalExchangeControlInfoForward...Exiting\n";
        exit(-1);
      }
      if ( isLINE_NETWORK() )
      {
        // TODO : figure out what this delay should actually be
        //double numBits = getQBits()*getNumNodes()*(getNodeId()+1);
        double numBits = getQBits()*getNumNodes();
        double packetDelay = numBits/(getControlInfoExchangeChannelRate()*1000000.0); // multiplied by 1000000 because it's in units of 1Mbps
        if( DIST_UNIV_SCHED_LINE_NET_DEBUG )
        {
          std::cout<<"Node " << getNodeId() << ":  packetDelay = " << packetDelay << "  (going forward)\n";
        }
        Simulator::Schedule( Seconds(packetDelay), &RoutingProtocol::GlobalExchangeControlInfoForward, dusRp, p ); 
      }
      else
      {
        Simulator::Schedule( Seconds(0.0), &RoutingProtocol::GlobalExchangeControlInfoForward, dusRp, p ); 
      }
    }
    else // this is the last node
    {
      Ptr<Node> node = NodeList::GetNode((uint32_t)getNodeId());
      Ptr<RoutingProtocol> dusRp = node->GetObject<RoutingProtocol>();
      if ( dusRp == 0 )
      {
        std::cout<<"ERROR:  DUSRP == 0 in GlobalExchangeControlInfoForward...Exiting\n";
        exit(-1);
      }
      // only last node sends this message...it starts chain of exchanging data backwards and then sends message to mark next time slot
      if ( isLINE_NETWORK() )
      {
        // don't need any delay here because it's in the same node
        Simulator::Schedule( Seconds(0.0), &RoutingProtocol::GlobalExchangeControlInfoBackward, this, (Ptr<Packet>)0 ); 
      }
      else
      {
        Simulator::Schedule( Seconds(0.0), &RoutingProtocol::GlobalExchangeControlInfoBackward, this, (Ptr<Packet>)0 ); 
      }
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
  RoutingProtocol::GlobalExchangeControlInfoBackward( Ptr<Packet> packet )
  {
    if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " is in GlobalExchangeControlInfoBackward() at time " << Simulator::Now().GetSeconds()  << "\n";
    }

    int i, j;
    char *packetPtr, *origPacketPtr;
    char *tempPacket = (char *)malloc( /*sizeof(int)*numNodes +*/ sizeof(int)*numNodes*numCommodities + sizeof(double)*numNodes*numNodes + sizeof(double)*numNodes*2 );
   
    uint32_t packetSize = 0;

    if( packet != 0 )
    {
      packetSize = packet->CopyData( (uint8_t *)tempPacket, (uint32_t)1000 );
      packetPtr = tempPacket;
    
      if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
      {
        std::cout<< "\tcopied " << packetSize <<" bytes from packet into local buffer\n";
      }
    }

    int temp_int;
    double temp_double;

    if( getNodeId()+1 != getNumNodes() )
    {
      //extract backlogs from packet
      for( i = getNodeId()+1; i < getNumNodes(); i++ )
      {
        for( j = 0; j < getNumCommodities(); j++ )
        {
            if( GLOBAL_KNOWLEDGE )
            {
                  memcpy(&temp_int, packetPtr, sizeof(int));
                  setOtherBacklogs( i, j, temp_int );
                
                if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
                {
                    printf("Node %i: (after extraction), temp_int = %i, otherBacklogs[%i][%i] = %i\n", getNodeId(), temp_int, i, j, getOtherBacklogs( i, j ) );
                }
            }
            else if( isLINE_NETWORK() )
            {
                  memcpy(&temp_int, packetPtr, sizeof(int));
                  setOtherBacklogs( i, j, temp_int );
                
                if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
                {
                    printf("Node %i: (after extraction), temp_int = %i, otherBacklogs[%i][%i] = %i\n", getNodeId(), temp_int, i, j, getOtherBacklogs( i, j ) );
                }
            }
            else
            {
                  memcpy(&temp_int, packetPtr, sizeof(int));
                  setGlobalBacklogs( i, j, temp_int );
                
                if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
                {
                    printf("Node %i: (after extraction) globalBacklogs[%i][%i] = %i\n", getNodeId(), i, j, getGlobalBacklogs( i, j ) );
                }
            }
            packetPtr += sizeof(int);
        }
      }
      //extract channel rates from packet
      for( i = getNodeId()+1; i < getNumNodes(); i++ )
      {
        for( j = 0; j < getNumNodes(); j++ )
        {
            if( isGLOBAL_KNOWLEDGE() || isLINE_NETWORK() )
            {
                    memcpy(&temp_double, packetPtr, sizeof(double));
                    setChannelRates( i, j, temp_double );
            
                if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
                {
                    printf("\tNode %i: (after extracting) channelRates[%i][%i] = %f\n", getNodeId(), i, j, getChannelRates( i, j ) );
                }
            }
            else
            {
                    memcpy(&temp_double, packetPtr, sizeof(double));
                    setGlobalChannelRates( i, j, temp_double );
                
                if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
                {
                    printf("\tNode %i: (after extracting) globalChannelRates[%i][%i] = %f\n", getNodeId(), i, j, getGlobalChannelRates( i, j ) );
                }
            }
            packetPtr += sizeof(double);
        }
      }
      // extract node coordinates
      for( i = getNodeId()+1; i < getNumNodes(); i++ )
      {
          memcpy(&temp_double, packetPtr, sizeof(double));
          setNodeCoordX( i, temp_double );
          packetPtr += sizeof(double);
          memcpy(&temp_double, packetPtr, sizeof(double));
          setNodeCoordY( i, temp_double );
          packetPtr += sizeof(double);
          if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
          {
            printf("\tNode %i: (after extracting) nodeCoordX[%i] = %f,  nodeCoordY[%i] = %f\n", 
                   getNodeId(), i, getNodeCoordX(i), i, getNodeCoordY(i) );
          }
      }
    }

    free( tempPacket );

    if( getNodeId() != 0 )
    {
    //   origPacketPtr = packetPtr = tempPacket; 
        
        // allocate space for a backlog x commodity x node and rate x node x node and coordinate x node
        packetSize = ((getNumNodes()*getNumCommodities()*sizeof(int))+(sizeof(double)*getNumNodes()*getNumNodes())+(2*sizeof(double)*getNumNodes()));
   
        origPacketPtr = packetPtr = (char *)malloc(packetSize);

        // load all known backlogs into message (this node and all after it)
        for( i = getNodeId(); i < getNumNodes(); i++ )
        {
            for( j = 0; j < getNumCommodities(); j++ )
            {  
                if( GLOBAL_KNOWLEDGE )
                {
                  temp_int = getOtherBacklogs( i, j );
                  memcpy(packetPtr, &temp_int, sizeof(int));    
                  if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
                  {
                    printf("\tNode %i: loading otherBacklogs[%i][%i] = %i into packet\n", getNodeId(), i, j, getOtherBacklogs( i, j ) );
                  }
                }
                else if( isLINE_NETWORK() )
                {
//                  temp_int = DiscreteQValue( getOtherBacklogs( i, j ), getQBits() );
                  temp_int = getOtherBacklogs( i, j );
                    memcpy(packetPtr, &temp_int, sizeof(int));    
                    if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
                    {
                        printf("\tNode %i: loading discrete backlog[%i][%i] = %i into packet\n", getNodeId(), i, j, temp_int);
                    }
                }
                else
                {
                  temp_int = getGlobalBacklogs( i, j );
                  memcpy(packetPtr, &temp_int, sizeof(int));    
                  if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
                  {
                    printf("\tNode %i: loading globalBacklogs[%i][%i] = %i into packet\n", getNodeId(), i, j, getOtherBacklogs( i, j ) );
                  }
                }
                packetPtr += sizeof(int);
            }
        }
        
        // also load all known channel rates into message
        for( i = getNodeId(); i < getNumNodes(); i++ )
        {
            for( j = 0; j < getNumNodes(); j++ )
            {
                if( GLOBAL_KNOWLEDGE || isLINE_NETWORK() )
                {
                  temp_double = getChannelRates(i,j);
                  memcpy(packetPtr, &temp_double, sizeof(double));
                  if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
                  {
                    printf("\tNode %i: loading ChannelRates[%i][%i] = %f into packet\n", getNodeId(), i, j, getChannelRates( i , j ) );
                  } 
                }
                else
                {
                  temp_double = getGlobalChannelRates(i,j);
                  memcpy(packetPtr, &temp_double, sizeof(double));
                  if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
                  {
                    printf("\tNode %i: loading globalChannelRates[%i][%i] = %f into packet\n", getNodeId(), i, j, getGlobalChannelRates( i, j ) );
                  }
                }
                packetPtr += sizeof(double);
            }
        }
        
        // also load all known node coordinates into message
        for( i = getNodeId(); i < getNumNodes(); i++ )
        {
           if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
          {
            std::cout<<"\tNode " << getNodeId() << ": loading  nodeCoordX[" << i << "] = " << getNodeCoordX(i) << ", nodeCoordY[" << i << "] = " << getNodeCoordY(i) << " into packet\n";
          }
          temp_double = getNodeCoordX(i);
          memcpy(packetPtr, &temp_double, sizeof(double));
          packetPtr += sizeof(double);
          temp_double = getNodeCoordY(i);
          memcpy(packetPtr, &temp_double, sizeof(double));
          packetPtr += sizeof(double);
        }
        
        Ptr<Packet> p = Create<Packet>( (uint8_t const *)origPacketPtr, packetSize);
        free(origPacketPtr);

        // send message to previous node in line and then send msg w/ delay to trigger next time slot
        Ptr<Node> node = NodeList::GetNode((uint32_t)getNodeId()-1);
        Ptr<RoutingProtocol> dusRp = node->GetObject<RoutingProtocol>();

        if( isLINE_NETWORK() )
        {
          //double numBits = getQBits()*(getNumNodes()-getNodeId()); // sending all nodes up the line from this one
          double numBits = getQBits()*getNumNodes();
          double packetDelay = numBits/(getControlInfoExchangeChannelRate()*1000000.0); // multiplied by 1000000 because it's in units of 1Mbps
          if( DIST_UNIV_SCHED_LINE_NET_DEBUG )
          {
            std::cout<<"Node " << getNodeId() << ":  packetDelay = " << packetDelay << "  (going backward)\n";
          }
          Simulator::Schedule( Seconds(packetDelay), &RoutingProtocol::GlobalExchangeControlInfoBackward, dusRp, p );
        }
        else
        {
          Simulator::Schedule( Seconds(0.0), &RoutingProtocol::GlobalExchangeControlInfoBackward, dusRp, p );
        }
        
        if( isGLOBAL_KNOWLEDGE() )
        {
          Simulator::Schedule ( Seconds(0.0), &RoutingProtocol::CompleteTimeSlot, this );
            
          if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
          {
            printf("node %i sent message to complete time slot with 0 delay\n", getNodeId());
          }
        }
        /*
        if( isLINE_NETWORK() )
        {
          // doing the scheduling of Complete time slot in Start time slot now
          
          double numBitsLeft;
          if( (getNodeId()-1)%2 == 0 )
          {
            numBitsLeft = getQBits()*(( (getNodeId()) +1)*((getNodeId()+1)/2));
          }
          else
          {
            numBitsLeft = getQBits()*(( (getNodeId()) +1)*((getNodeId()+1)/2) + (((getNodeId()+1)/2)+1));
          }
          double delayToCompleteTimeSlot = numBitsLeft/(getControlInfoExchangeChannelRate()*1000000.0);
          //delayToCompleteTimeSlot = getNextTimeSlotStartTime()-Simulator::Now();

          std::cout<<"Node " << getNodeId() << ": scheduling CompleteTimeSlot after " << Seconds(delayToCompleteTimeSlot) << " seconds\n";
         
          Simulator::Schedule( Seconds(delayToCompleteTimeSlot), &RoutingProtocol::CompleteTimeSlot, this );
        }
          */
    }
    else // node 0 - don't need to send any more exchange messages, just schedule complete time slot
    {
        if( isGLOBAL_KNOWLEDGE() )
        {
          if( DIST_UNIV_SCHED_EXCHANGE_GLOBAL_INFO_DEBUG )
          {
            printf("\n\nQueue data Exchange complete!\n\n");
          }
          // send COMPLETE_TIME_SLOT event to calculate routing decisions and transmit packets
          //else
          {
            Simulator::Schedule ( Seconds(0.0), &RoutingProtocol::CompleteTimeSlot, this );
          }

        }
        //if( isLINE_NETWORK() )
        //{
          //Simulator::Schedule ( Seconds(0.0), &RoutingProtocol::CompleteTimeSlot, this );
        //}
    }
  }


// /**
// FUNCTION: BruteForceExchangeControlInfoForward
// LAYER   : NETWORK
// PURPOSE : Function to pack a control packet with network state values to broadcast out.
//             Neighbor nodes will receive packet and update values accordingly in the receive 
//             control packet function (that's why we only pack the packets here).
// PARAMETERS:
// +none
// RETURN   ::void:NULL
// **/
  void 
  RoutingProtocol::BruteForceExchangeControlInfoForward()
  {
    if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " is in BruteForceExchangeControlInfoForward() at time " << Simulator::Now().GetSeconds()  << "\n";

      if( isBRUTE_FORCE() )
      {
        std::cout<<"Using Brute Force Exchange Algorithm.\n";
      }
      std::cout<<"Time Slot Duration = " << timeSlotDuration.GetSeconds() << ".\n";
    }
  
    int i;
    char *packetPtr, *origPacketPtr;
                              // valid info identifiers              backlogs                           channel rates                       coordinates 
    //char *tempPacket = (char *)malloc( sizeof(int)*numNodes + sizeof(int)*numNodes*numCommodities + sizeof(double)*numNodes*numNodes + sizeof(double)*numNodes*2 );

    uint32_t packetSize = 0;

    // make sure own backlogs are updated 
    for( i = 0; i < getNumCommodities(); i++ )
    {

      setOtherBacklogs( getNodeId(), i, queues[i].getM_backlog() );
      //setDiscreteBacklogs( getNodeId(), i, DiscreteQValues( queues[i].getM_backlog(), getQBits() ) );
      if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
      {
        printf("otherBacklogs[%i][%i] = %i, queues[%i].backlog = %i\n", 
                getNodeId(), i, getOtherBacklogs( getNodeId(), i ), 
                i, queues[i].getM_backlog());
      } 
    }
    
    // update own coordinates
    Ptr<MobilityModel> mobility = NodeList::GetNode((uint32_t)getNodeId())->GetObject<MobilityModel> ();
    Vector pos = mobility->GetPosition();
    if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " position: x = " << pos.x << ", y = " << pos.y << " (after updating in BruteForce Exchange Forward)\n";
    }

    setNodeCoordX(getNodeId(), (double)pos.x);
    setNodeCoordY(getNodeId(), (double)pos.y);

    if( getNodeId()+1 != getNumNodes() )
    {
      // allocate space for a valid info tag and  backlog x commodity x node and rate x node x node and coordinate x node
      packetSize = ((getNumNodes()*sizeof(int))+(getNumNodes()*getNumCommodities()*sizeof(int))+(sizeof(double)*getNumNodes()*getNumNodes())+(2*sizeof(double)*getNumNodes()));
   
      origPacketPtr = packetPtr = (char *)malloc(packetSize);

      BruteForceCreateControlPacket( packetPtr, packetSize );

      // Send Control Packet as subnet directed broadcast from each interface used by distUnivSched
      for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
            m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
      {
        Ptr<Socket> socket = j->first;
        Ipv4InterfaceAddress iface = j->second;


        if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
        { 
          std::cout<<"  trying to send out interface with address: ";
          iface.GetLocal().Print( std::cout );
          std::cout<<"\n";
        }

        // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
        Ipv4Address destination;
        if (iface.GetMask () == Ipv4Mask::GetOnes ())
        {
          destination = Ipv4Address ("255.255.255.255");
        }
        else
        { 
          destination = iface.GetBroadcast ();
        }

        Ptr<Packet> packet =  Create<Packet>( (uint8_t const *)origPacketPtr, packetSize);
        free(origPacketPtr);
                      
        TypeHeader tHeader (DUS_CTRL);
        packet->AddHeader (tHeader);
        if( !socket->GetAllowBroadcast() )
        {
          std::cout<<"ERROR: Socket does not allow broadcasts\n";
          exit(-1);
        }

        if( (socket->SendTo (packet, 0, InetSocketAddress (destination, DUS_PORT))) == -1 )
        {
          setNumControlPacketsSent(getNumControlPacketsSent()+1); // TODO : Double check this
          std::cout<<"ERROR:  socket->SendTo() in SendControlInfoPacket() failed\n";
        }
      }


      Ptr<Node> node = NodeList::GetNode((uint32_t)getNodeId()+1);
      Ptr<RoutingProtocol> dusRp = node->GetObject<RoutingProtocol>();
      if ( dusRp == 0 )
      {
        std::cout<<"ERROR:  DUSRP == 0 in BruteForceExchangeControlInfoForward...Exiting\n";
        exit(-1);
      }
      if ( isLINE_NETWORK() )
      {
        // TODO : figure out what this delay should actually be
        double numBits = 8*( sizeof(int)*getNumNodes() + sizeof(int)*getNumNodes()*getNumCommodities() + sizeof(double)*getNumNodes()*getNumNodes() + sizeof(double)*getNumNodes()*2 );
        double packetDelay = numBits/(getControlInfoExchangeChannelRate()*1000000.0); // multiplied by 1000000 because it's in units of 1Mbps
        if( DIST_UNIV_SCHED_LINE_NET_DEBUG )
        {
          std::cout<<"Node " << getNodeId() << ":  packetDelay = " << packetDelay << "  (going forward)\n";
        }
        Simulator::Schedule( Seconds(packetDelay), &RoutingProtocol::BruteForceExchangeControlInfoForward, dusRp ); 
      }
      else
      {
        Simulator::Schedule( Seconds(0.0), &RoutingProtocol::BruteForceExchangeControlInfoForward, dusRp ); 
      }
    }
    else // this is the last node
    {
      Ptr<Node> node = NodeList::GetNode((uint32_t)getNodeId());
      Ptr<RoutingProtocol> dusRp = node->GetObject<RoutingProtocol>();
      if ( dusRp == 0 )
      {
        std::cout<<"ERROR:  DUSRP == 0 in BruteForceExchangeControlInfoForward...Exiting\n";
        exit(-1);
      }
      // only last node sends this message...it starts chain of exchanging data backwards and then sends message to mark next time slot
      if ( isLINE_NETWORK() )
      {
        // don't need any delay here because it's in the same node
        Simulator::Schedule( Seconds(0.0), &RoutingProtocol::BruteForceExchangeControlInfoBackward, this ); 
      }
      else
      {
        Simulator::Schedule( Seconds(0.0), &RoutingProtocol::BruteForceExchangeControlInfoBackward, this ); 
      }
    }
  }
  
// /**
// FUNCTION: BruteForceExchangeControlInfoBackward
// LAYER   : NETWORK
// PURPOSE : Function scheduled by simulator to be run.
//           This function extracts control info from all nodes after it,
//            places its own and all nodes' after it control info into a 
//            packet that it forwards to the node before it.
//           If using global knowledge, this function must schedule the 
//            complete time slot function
// PARAMETERS:
// +none 
// RETURN   ::void:NULL
// **/
  void 
  RoutingProtocol::BruteForceExchangeControlInfoBackward()
  {
    if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " is in BruteForceExchangeControlInfoBackward() at time " << Simulator::Now().GetSeconds()  << "\n";
    }

    char *packetPtr, *origPacketPtr;
   
    uint32_t packetSize = 0;

/*
    if( packet != 0 )
    {
      packetSize = packet->CopyData( (uint8_t *)tempPacket, (uint32_t)1000 );
      packetPtr = tempPacket;
    
      if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
      {
        std::cout<< "\tcopied " << packetSize <<" bytes from packet into local buffer\n";
      }
    }

    int temp_int;
    double temp_double;

    if( getNodeId()+1 != getNumNodes() )
    {
      //extract backlogs from packet
      for( i = getNodeId()+1; i < getNumNodes(); i++ )
      {
        for( j = 0; j < getNumCommodities(); j++ )
        {
                  memcpy(&temp_int, packetPtr, sizeof(int));
                  setOtherBacklogs( i, j, temp_int );
                
                if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
                {
                    printf("Node %i: (after extraction), temp_int = %i, otherBacklogs[%i][%i] = %i\n", getNodeId(), temp_int, i, j, getOtherBacklogs( i, j ) );
                }
            packetPtr += sizeof(int);
        }
      }
      //extract channel rates from packet
      for( i = getNodeId()+1; i < getNumNodes(); i++ )
      {
        for( j = 0; j < getNumNodes(); j++ )
        {
                    memcpy(&temp_double, packetPtr, sizeof(double));
                    setChannelRates( i, j, temp_double );
            
                if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
                {
                    printf("\tNode %i: (after extracting) channelRates[%i][%i] = %f\n", getNodeId(), i, j, getChannelRates( i, j ) );
                }
            packetPtr += sizeof(double);
        }
      }
      // extract node coordinates
      for( i = getNodeId()+1; i < getNumNodes(); i++ )
      {
          memcpy(&temp_double, packetPtr, sizeof(double));
          setNodeCoordX( i, temp_double );
          packetPtr += sizeof(double);
          memcpy(&temp_double, packetPtr, sizeof(double));
          setNodeCoordY( i, temp_double );
          packetPtr += sizeof(double);
          if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
          {
            printf("\tNode %i: (after extracting) nodeCoordX[%i] = %f,  nodeCoordY[%i] = %f\n", 
                   getNodeId(), i, getNodeCoordX(i), i, getNodeCoordY(i) );
          }
      }
    }

    free( tempPacket );

*/
    double numBits = 8*( sizeof(int)*getNumNodes() + sizeof(int)*getNumNodes()*getNumCommodities() + sizeof(double)*getNumNodes()*getNumNodes() + sizeof(double)*getNumNodes()*2 );
    double packetDelay = numBits/(getControlInfoExchangeChannelRate()*1000000.0); // multiplied by 1000000 because it's in units of 1Mbps
    if( getNodeId() != 0 )
    {
    //   origPacketPtr = packetPtr = tempPacket; 
        
        // allocate space for a backlog x commodity x node and rate x node x node and coordinate x node
        packetSize = ((getNumNodes()*getNumCommodities()*sizeof(int))+(sizeof(double)*getNumNodes()*getNumNodes())+(2*sizeof(double)*getNumNodes()));
   
        origPacketPtr = packetPtr = (char *)malloc(packetSize);

      BruteForceCreateControlPacket( packetPtr, packetSize );

/*
        // load all known backlogs into message (this node and all after it)
        for( i = getNodeId(); i < getNumNodes(); i++ )
        {
            for( j = 0; j < getNumCommodities(); j++ )
            {  
                  temp_int = getOtherBacklogs( i, j );
                  memcpy(packetPtr, &temp_int, sizeof(int));    
                  if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
                  {
                    printf("\tNode %i: loading otherBacklogs[%i][%i] = %i into packet\n", getNodeId(), i, j, getOtherBacklogs( i, j ) );
                  }
                packetPtr += sizeof(int);
            }
        }
        
        // also load all known channel rates into message
        for( i = getNodeId(); i < getNumNodes(); i++ )
        {
            for( j = 0; j < getNumNodes(); j++ )
            {
                  temp_double = getChannelRates(i,j);
                  memcpy(packetPtr, &temp_double, sizeof(double));
                  if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
                  {
                    printf("\tNode %i: loading ChannelRates[%i][%i] = %f into packet\n", getNodeId(), i, j, getChannelRates( i , j ) );
                  } 
                packetPtr += sizeof(double);
            }
        }
        
        // also load all known node coordinates into message
        for( i = getNodeId(); i < getNumNodes(); i++ )
        {
           if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
          {
            std::cout<<"\tNode " << getNodeId() << ": loading  nodeCoordX[" << i << "] = " << getNodeCoordX(i) << ", nodeCoordY[" << i << "] = " << getNodeCoordY(i) << " into packet\n";
          }
          temp_double = getNodeCoordX(i);
          memcpy(packetPtr, &temp_double, sizeof(double));
          packetPtr += sizeof(double);
          temp_double = getNodeCoordY(i);
          memcpy(packetPtr, &temp_double, sizeof(double));
          packetPtr += sizeof(double);
        }
 
*/     

      // Send Control Packet as subnet directed broadcast from each interface used by distUnivSched
      for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
            m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
      {
        Ptr<Socket> socket = j->first;
        Ipv4InterfaceAddress iface = j->second;


        if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
        { 
          std::cout<<"  trying to send out interface with address: ";
          iface.GetLocal().Print( std::cout );
          std::cout<<"\n";
        }

        // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
        Ipv4Address destination;
        if (iface.GetMask () == Ipv4Mask::GetOnes ())
        {
          destination = Ipv4Address ("255.255.255.255");
        }
        else
        { 
          destination = iface.GetBroadcast ();
        }

        Ptr<Packet> packet =  Create<Packet>( (uint8_t const *)origPacketPtr, packetSize);
        free(origPacketPtr);
                      
        TypeHeader tHeader (DUS_CTRL);
        packet->AddHeader (tHeader);
        if( !socket->GetAllowBroadcast() )
        {
          std::cout<<"ERROR: Socket does not allow broadcasts\n";
          exit(-1);
        }

        if( (socket->SendTo (packet, 0, InetSocketAddress (destination, DUS_PORT))) == -1 )
        {
          setNumControlPacketsSent(getNumControlPacketsSent()+1); // TODO : Double check this
          std::cout<<"ERROR:  socket->SendTo() in SendControlInfoPacket() failed\n";
        }
      }

        free(origPacketPtr);

        // send message to previous node in line and then send msg w/ delay to trigger next time slot
        Ptr<Node> node = NodeList::GetNode((uint32_t)getNodeId()-1);
        Ptr<RoutingProtocol> dusRp = node->GetObject<RoutingProtocol>();

        if( DIST_UNIV_SCHED_BRUTE_FORCE_DEBUG )
        {
          std::cout<<"Node " << getNodeId() << ":  packetDelay = " << packetDelay << "  (going backward)\n";
        }
        Simulator::Schedule( Seconds(packetDelay), &RoutingProtocol::BruteForceExchangeControlInfoBackward, dusRp );
       
        
        if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
        {
          printf("node %i sent message to complete time slot with 0 delay\n", getNodeId());
        }
    }
    else // node 0 - don't need to send any more exchange messages, just schedule complete time slot
    {
      if( getNumExchangeRoundsCompleted() == getNumExchangeRounds() )
      {
          if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
          {
            printf("\n\nQueue data Exchange complete!\n\n");
          }
          // send COMPLETE_TIME_SLOT event to calculate routing decisions and transmit packets
          //else
          {
            Simulator::Schedule ( Seconds(0.0), &RoutingProtocol::CompleteTimeSlot, this );
          }
      }
      else
      {
        setNumExchangeRoundsCompleted( getNumExchangeRoundsCompleted() + 1 );
        Simulator::Schedule ( Seconds(packetDelay), &RoutingProtocol::BruteForceExchangeControlInfoBackward, this );
      }

    
    }
  }


  void 
  RoutingProtocol::SendControlInfoPacket( )
  {
    if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " is in SendControlInfoPacket() at time " << Simulator::Now().GetSeconds()  << "\n";
    }
    
    //int nodeIndex = getNodeId() - 1;
    int i, j, k;
    int numInfoToSend = 0;

    char *packetPtr, *origPacketPtr;
    uint32_t packetSize = 0;
    bool resendImmediately = false;
    
    if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
    {
        Time currentTime = Simulator::Now();
        printf("Node %i received MSG_NETWORK_DIST_UNIV_SCHED_SEND_CONTROL_INFO_PACKET message at %f\n", getNodeId(), currentTime.GetSeconds() );
    }
   
    // make sure physical layer data trx rate is set to lowest available rate
    //Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("OfdmRate6Mbps") );
    Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("DsssRate1Mbps") );

    if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
    {
        printf("Node %i:    setting data rate to 1 Mbps\n", getNodeId() );
    }
   
    if( IsPhyStateBusy() )
    {
      // wait random delay between 0 and maxBackoffWindow before transmitting packet to avoid collisions
      Time delay = Seconds(trxDelayRV.GetValue());
        
      if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
      {
         printf("Node %i: PhyState is BUSY...pausing for %i milliseconds\n", getNodeId(), (int)delay.GetMilliSeconds());
      }
        
      Simulator::Schedule ( delay, &RoutingProtocol::SendControlInfoPacket, this );
        
      return;
    }
    else
    {
      if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
      {
        std::cout<<"Node " << getNodeId() << ": PhyState is Idle...cleared to send control packet.\n";
      }
    }
    
    // make sure own backlogs are updated 
    for( i = 0; i < getNumCommodities(); i++ )
    {
          
      // TODO :: Check on this

      // we use our own addition to DropTailQueue called m_backlog, which we update at the end of each slot
      //   to prevent inconsistencies in network state info from packets arriving during exchange
      //   packets are actually queued using the queue functions, so getNPackets actually describes the
      //   number of packets in a queue at a given time
      if( isVARYING_Q_BITS() )
      {
        setOtherBacklogs( getNodeId(), i, DiscreteQValue( queues[i].getM_backlog(), getQBits() ) );
      }
      else
      {
        setOtherBacklogs( getNodeId(), i, queues[i].getM_backlog() );
      }
      if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
      {
        std::cout<<"\tqueues[" << i << "].GetNPackets() = " << queues[i].GetNPackets() << "\n";
      }
            
      if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
      {
        printf("\totherBacklogs[%i][%i] = %i, queues[%i].backlog = %i\n", 
                  getNodeId(), i, getOtherBacklogs(getNodeId(), i), 
                  i, queues[i].getM_backlog());
      }
    }
       
      switch ( getControlInfoExchangeState() )
      {
        case SEND_OWN_INFO: 
        {        
            if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
            {
                printf("Node %i: in state SEND_OWN_INFO\n", getNodeId() );
            }    
            
            numInfoToSend = 1;
            // only sending own info here
            setForwardNodeCtrlInfo(getNodeId(), true);
            
            if( isVARYING_Q_BITS() )
            {
                // allocate space for identifier and backlog x commodity and rate x node and    node x timestamp
                int numIdentifierBits = ceil( log2( getNumNodes() ) );
                int numBacklogBits = getQBits()*getNumCommodities();
                int numRateBits = getRateBits()*getNumNodes();
                int totalNumBits = numIdentifierBits + numBacklogBits + numRateBits;
                packetSize = ceil(double(totalNumBits)/8.0)*numInfoToSend;
                origPacketPtr = packetPtr = (char *)malloc( packetSize );
               
                VaryingBitsCreateControlPacket( packetPtr, packetSize, numInfoToSend );
            }
            else // if not VARYING_Q_BITS
            {
                // allocate space for identifier and backlog x commodity and rate x node and    node x timestamp
                packetSize = (sizeof(int)+(getNumCommodities()*sizeof(int))+(sizeof(double)*getNumNodes())+(sizeof(Time)*getNumNodes()));
                
                origPacketPtr = packetPtr = (char *)malloc( packetSize );

                if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                {
                    printf("\tpacketSize = %i: numCommodities = %i, numNodes = %i\n", packetSize, getNumCommodities(), getNumNodes());
                }
               
                if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                {
                    printf("\tbefore loading identifier (%i)...pointer = %p\n", getNodeId(), packetPtr);
                }
                int temp_int;
                double temp_double;
                Time temp_time;
                // load info identifier into packet
                temp_int = getNodeId();
                memcpy( packetPtr, &temp_int, sizeof(int) );
                packetPtr = (packetPtr+sizeof(int));
                
                if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                {
                    printf("\tafter loading identifier, before loading backlogs...pointer = %p\n", packetPtr);
                }
                
                
                // load own backlogs into message
                for( i = 0; i < getNumCommodities(); i++ )
                {
                    if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                    {
                        printf("\tNode %i: loading otherBacklogs[%i][%i] = %i into packet\n", getNodeId(), getNodeId(), i, getOtherBacklogs(getNodeId(), i));
                    }

                    temp_int = getOtherBacklogs(getNodeId(), i);
                    memcpy(packetPtr, &temp_int, sizeof(int));    
                
                    packetPtr = (packetPtr+sizeof(int));
                }
                
                if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                {
                    printf("\tLoaded own backlogs...\n");
                }
                
                if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                {
                    printf("packet pointer before loading channel rates = %p\n", packetPtr);
                }
                
                // now load all known channel rates into message
                for( i = 0; i < getNumNodes(); i++ )
                {
                    //printf("Trying to load channelRate[%i][%i] = %f\n", i, j, channelRates[i][j]);
                    if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                    {
                        printf("\tNode %i: loading channelRate[%i][%i] = %f into packet\n", getNodeId(), getNodeId(), i, getChannelRates(getNodeId(), i));
                    }

                    temp_double = getChannelRates(getNodeId(), i);
                    memcpy(packetPtr, &temp_double, sizeof(double));

                    packetPtr = (packetPtr+sizeof(double));
                    //packetPtr += sizeof(double);
                }
                if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                {
                    printf("\tLoaded channel rates\n");
                }
                
                //packetPtr = MESSAGE_ReturnPacket(exchangeMsg);
                //packetPtr += numNodes*numCommodities*sizeof(int);
                if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                {
                    printf("packet pointer before loading timeStamps = %p\n", packetPtr);
                }
                
                // now load time stamps into message
                for( i = 0; i < getNumNodes(); i++ )
                {
                    // beginning new exchange of data...make all other nodes' timestamps = 0 ?
                    if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                    {
                        //printf("\tNode %i: loading timeStamp[%i][%i] = %f into packet\n", getNodeId(), getNodeId(), i, getTimeStamps(getNodeId(), i));
                    }
                   
                    temp_time = getTimeStamps(getNodeId(), i);
                    memcpy(packetPtr, &temp_time, sizeof(Time));
                    //printf("\tplaced value %u into packet\n",(Time)*packetPtr );
                    packetPtr = (packetPtr+sizeof(Time));
                    
                }
                if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                {
                    printf("\tLoaded time stamps\n");
                }
            } // end if ( VARYING_Q_BITS ) else
            
            controlInfoExchangeState = WAIT_FOR_ACK_OR_NEW_INFO;
            
            break;
        }// end case SEND_OWN_INFO
            
        case WAIT_FOR_ACK_OR_NEW_INFO:
        {        
            if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
            {
                printf("Node %i: in state WAIT_FOR_ACK_OR_NEW_INFO\n", getNodeId() );
            }    
            
            if( Simulator::Now().GetSeconds() > getWaitForAckTimeoutTime() )
            {
                if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                {
                    printf( "\ttimed out waiting for ack to own info...changing to SEND_OWN_INFO state\n" );
                }
                
                // change state
                setControlInfoExchangeState(SEND_OWN_INFO);
                
                // reset timeout time
                setWaitForAckTimeoutTime(Simulator::Now() + getWaitForAckTimeoutWindow());
                
                // set flag to reenter function and send own info immediately
                resendImmediately = true;
                
                // keep track of how many timeouts occur
                setNumWaitForAckTimeouts( getNumWaitForAckTimeouts() + 1 );
            }

            break;
        }
            
        case SEND_OWN_AND_FWD_NEW_INFO:
        {        
            if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
            {
                printf("Node %i: in state SEND_OWN_AND_FWD_NEW_INFO\n", getNodeId() );
            }    
            //always want to send own info:
            setForwardNodeCtrlInfo(getNodeId(), true);
            
            numInfoToSend = 0;
            
            for( i = 0; i < getNumNodes(); i++ )
            {
                if( isForwardNodeCtrlInfo(i) )
                {
                    numInfoToSend++;
                }
            }
            
            if( VARYING_Q_BITS )
            {
                // allocate space for identifier and backlog x commodity and rate x node and    node x timestamp
                int numIdentifierBits = ceil( log2( getNumNodes() ) );
                int numBacklogBits = getQBits()*getNumCommodities();
                int numRateBits = getRateBits()*getNumNodes();
                int totalNumBits = numIdentifierBits + numBacklogBits + numRateBits;
                packetSize = ceil(double(totalNumBits)/8.0)*numInfoToSend;
                origPacketPtr = packetPtr = (char *)malloc( packetSize );
               
                VaryingBitsCreateControlPacket( packetPtr, packetSize, numInfoToSend );
            }
            else // if not VARYING_Q_BITS
            {
                // allocate space for numInfoToSend x identifier and numInfoToSend x backlog x commodity x node and rate x node x numInfoToSend and numInfoToSend x node x timestamp
                packetSize = ((numInfoToSend*sizeof(int))+(numInfoToSend*getNumCommodities()*sizeof(int))+(sizeof(double)*numInfoToSend*getNumNodes())+(sizeof(Time)*numInfoToSend*getNumNodes()));
                origPacketPtr = packetPtr = (char *)malloc( packetSize );
                
                if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                {
                    printf("packetSize = %i\n", packetSize);
                }
               
                int temp_int;
                double temp_double;
                Time temp_time;
                // load control info into message
                for( i = 0; i < numInfoToSend; i++ )
                {
                    for( k = 0; k < getNumNodes(); k++ )
                    {
                        if( isForwardNodeCtrlInfo(k) )
                        {
                            setForwardNodeCtrlInfo(k, false);
                            break;
                        }
                    }
                    
                    int tempNodeId = k; 
                    // first load node identifier    
                    if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                    {
                        printf( "\tNode %i: loading node identifier %i into packet\n", getNodeId(), tempNodeId );
                    }
                    
                    memcpy( packetPtr, &tempNodeId, sizeof(int) );
                    
                    packetPtr = (packetPtr+sizeof(int));
                    //packetPtr += sizeof(int);
                    
                    // then load backlogs for that node
                    for( j = 0; j < getNumCommodities(); j++ )
                    {    
                        if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                        {
                            printf("\tNode %i: loading otherBacklogs[%i][%i] = %i into packet\n", getNodeId(), k, j, getOtherBacklogs(k, j));
                        }
                       
                        temp_int = getOtherBacklogs(k, j);
                        memcpy(packetPtr, &temp_int, sizeof(int));
                
                        packetPtr = (packetPtr+sizeof(int));
                        //packetPtr += sizeof(int);
                    }
                    
                    // now load channel rates into message
                    for( j = 0; j < getNumNodes(); j++ )
                    {
                        if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                        {
                            printf("\tNode %i: loading channelRate[%i][%i] = %f into packet\n", getNodeId(), k, j, getChannelRates(k, j));
                        }
                   
                        temp_double = getChannelRates(k, j);
                        memcpy(packetPtr, &temp_double, sizeof(double));
                        
                        packetPtr = (packetPtr+sizeof(double));
                    }
                    
                    // now load timestamps
                    // beginning new exchange of data...make all other nodes' timestamps = 0 ?
                    for( j = 0; j < getNumNodes(); j++ )
                    {
                        if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                        {
                            //printf("\tNode %i: loading timeStamp[%i][%i] = %f into packet\n", getNodeId(), k, j, (double)timeStamps[k][j]/(double)SECOND);
                        }
                       
                        temp_time = getTimeStamps(k, j);
                        memcpy(packetPtr, &temp_time, sizeof(Time));
                        
                        packetPtr = (packetPtr+sizeof(Time));
                    }
                }
                if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                {
                    printf("\tLoaded all control info...\n");
                }
            } // end if ( VARYING_Q_BITS ) else
            
            setControlInfoExchangeState(WAIT_FOR_ACK_OR_NEW_INFO);
            
            break;
        } // end SEND_OWN_AND_FWD_NEW_INFO
            
        case JUST_FWD_NEW_INFO:
        {        
            if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
            {
                printf("Node %i: in state JUST_FWD_NEW_INFO\n", getNodeId() );
            }    
            numInfoToSend = 0;
            
            for( i = 0; i < getNumNodes(); i++ )
            {
                if( isForwardNodeCtrlInfo(i) )
                {
                    numInfoToSend++;
                }
            }
            
            if( numInfoToSend == 0 )
            {
                if( Simulator::Now().GetSeconds() > waitForAckTimeoutTime )
                {
                    if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                    {
                        printf( "\ttimed out waiting for ack to own info...changing to SEND_OWN_AND_FWD_NEW_INFO state\n" );
                    }
                    
                    // change state
                    setControlInfoExchangeState(SEND_OWN_AND_FWD_NEW_INFO);
                    
                    // reset timeout time
                    setWaitForAckTimeoutTime(Simulator::Now() + getWaitForAckTimeoutWindow());
                    
                    // set flag to reenter function and send own info immediately
                    resendImmediately = true;
                    
                    // keep track of how many timeouts occur
                    setNumWaitForAckTimeouts(getNumWaitForAckTimeouts()+1);
                }                    
                
                break;
            }
            
            if( VARYING_Q_BITS )
            {
                // allocate space for identifier and backlog x commodity and rate x node and    node x timestamp
                int numIdentifierBits = ceil( log2( getNumNodes() ) );
                int numBacklogBits = getQBits()*getNumCommodities();
                int numRateBits = getRateBits()*getNumNodes();
                int totalNumBits = numIdentifierBits + numBacklogBits + numRateBits;
                packetSize = ceil(double(totalNumBits)/8.0)*numInfoToSend;
                origPacketPtr = packetPtr = (char *)malloc( packetSize );
               
                VaryingBitsCreateControlPacket( packetPtr, packetSize, numInfoToSend );
            }
            else // if not VARYING_Q_BITS
            {
                // allocate space for numInfoToSend x identifier and numInfoToSend x backlog x commodity x node and rate x node x numInfoToSend and numInfoToSend x node x timestamp
                packetSize = ((numInfoToSend*sizeof(int))+(numInfoToSend*getNumCommodities()*sizeof(int))+(sizeof(double)*numInfoToSend*getNumNodes())+(sizeof(Time)*numInfoToSend*getNumNodes()));
                origPacketPtr = packetPtr = (char *)malloc( packetSize );
                
                if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                {
                    printf("packetSize = %i\n", packetSize);
                }
               
                int temp_int;
                double temp_double;
                Time temp_time;
                // load control info into message
                for( i = 0; i < numInfoToSend; i++ )
                {
                    for( k = 0; k < getNumNodes(); k++ )
                    {
                        if( isForwardNodeCtrlInfo(k) )
                        {
                            setForwardNodeCtrlInfo(k, false);
                            break;
                        }
                    }
                    
                    int tempNodeId = k; 
                    // first load node identifier
                    memcpy( packetPtr, &tempNodeId, sizeof(int) );
                    packetPtr = (packetPtr+sizeof(int));
                    
                    // then load backlogs for that node
                    for( j = 0; j < getNumCommodities(); j++ )
                    {
                        if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                        {
                            printf("\tNode %i: loading otherBacklogs[%i][%i] = %i into packet\n", getNodeId(), k, j, getOtherBacklogs(k, j));
                        }
                       
                        temp_int = getOtherBacklogs(k, j);
                        memcpy(packetPtr, &temp_int, sizeof(int));    

                        packetPtr = (packetPtr+sizeof(int));
                    }
                    
                    // now load channel rates into message
                    for( j = 0; j < getNumNodes(); j++ )
                    {
                        if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                        {
                            printf("\tNode %i: loading channelRate[%i][%i] = %f into packet\n", getNodeId(), k, j, getChannelRates(k, j));
                        }
                       
                        temp_double = getChannelRates(k, j);
                        memcpy(packetPtr, &temp_double, sizeof(double));
                        
                        packetPtr = (packetPtr+sizeof(double));
                    }
                    
                    // now load timestamps
                    // beginning new exchange of data...make all other nodes' timestamps = 0 ?
                    for( j = 0; j < getNumNodes(); j++ )
                    {
                        if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                        {
                            //printf("\tNode %i: loading timeStamp[%i][%i] = %f into packet\n", getNodeId(), k, j, (double)timeStamps[k][j]/(double)SECOND);
                        }
                       
                        temp_time = getTimeStamps(k, j);
                        memcpy(packetPtr, &temp_time, sizeof(Time));
                        
                        packetPtr = (packetPtr+sizeof(Time));
                    }
                    
                }
                if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                {
                    printf("\tLoaded all control info...\n");
                }
            } // end if ( VARYING_Q_BITS ) else
            
            break;
        }
            
        case DATA_TRX:
        {
            if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
            {
                printf( "Node %i:    State is DATA_TRX in SendControlInfoPacket()...not enough time to send own info?\n", getNodeId() );
            }
            return;
        }
            
        default:
        {
            fprintf( stderr, "ERROR: unknown value of controlInfoExchangeState in SendControlInfoPacket() in routingDistUnivSched.cpp.\n" );
        }
      }// end switch( controlInfoExchangeState )

      if( numInfoToSend != 0 )
      {

        if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
        {
            printf("\tNode %i: Sending packet to socket here (from DistUnivSchedHandleProtocolEvent).\n", getNodeId());
        }
        
          // delay is calculated as follows:
          //        number of bits in packet = (channel rates) + (queue backlogs) + (timestamps)
          //                                 = (n^2 * 32 bits) + (n^2 * 32 bits) + (n^2 * 32 bits)
          //                                 = 96 * n^2
          //        assuming broadcast rate of 1 Mbps:
          //        time to send packet = (96*n^2)/(1M) = 96e-6 * n^2 
          //        
          //        therefore, each node uses delay of = (time to send packet) * nodeId 
          //                to avoid collisions
                

          // assuming broadcast data rate of 1 Mbps
          double bitsInPacket = (double)packetSize*8.0;
          double trxTime = bitsInPacket/(getControlInfoExchangeChannelRate()*1000000.0); //1000000.0;
          Time tempTime = Time::FromDouble( trxTime, Time::S );

          if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
          {
            std::cout<<"BitsInPacket = " << bitsInPacket << "\n";
            std::cout<<"trxTime = " << trxTime << "\n";
            std::cout<<"TempTime = " << tempTime.GetNanoSeconds() << " (nanoseconds)\n";
          }

          setControlPacketTrxTime( tempTime );

          if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
          {
            std::cout<<"Time to exchange this control packet (size = " << packetSize << " bytes) = " << getControlPacketTrxTime().GetMicroSeconds() << " microsecs.\n";
          }
                
          if( Simulator::Now().GetSeconds() + (Time)getControlPacketTrxTime() < getTimeSlotCompleteTime() )
          {
            if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
            {
              printf("\tEnough time to send this packet...packetSize = %i, packetTrxTime = %i (microsecs), Now().GetSeconds() + packetTrxTime = %f\n", 
                                 packetSize,
                                 (int)getControlPacketTrxTime().GetMicroSeconds(), 
                                 Simulator::Now().GetSeconds()+getControlPacketTrxTime().GetSeconds() );
            }
                    
            setControlPacketsSentThisTimeSlot(getControlPacketsSentThisTimeSlot()+1);

            if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
            { 
              std::cout<<"Node " << getNodeId() << " trying to send control packet out all DUS interfaces\n";
            }

            // Send Control Packet as subnet directed broadcast from each interface used by distUnivSched
            for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
                  m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
            {
              Ptr<Socket> socket = j->first;
              Ipv4InterfaceAddress iface = j->second;


              if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
              { 
                std::cout<<"  trying to send out interface with address: ";
                iface.GetLocal().Print( std::cout );
                std::cout<<"\n";
              }

              // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
              Ipv4Address destination;
              if (iface.GetMask () == Ipv4Mask::GetOnes ())
              {
                destination = Ipv4Address ("255.255.255.255");
              }
              else
              { 
                destination = iface.GetBroadcast ();
              }

              Ptr<Packet> packet =  Create<Packet>( (uint8_t const *)origPacketPtr, packetSize);
                      
              TypeHeader tHeader (DUS_CTRL);
              packet->AddHeader (tHeader);
                      
              if( !socket->GetAllowBroadcast() )
              {
                std::cout<<"ERROR: Socket does not allow broadcasts\n";
                exit(-1);
              }

              if( (socket->SendTo (packet, 0, InetSocketAddress (destination, DUS_PORT))) == -1 )
              {
                setNumControlPacketsSent(getNumControlPacketsSent()+1); // TODO : Double check this
                std::cout<<"ERROR:  socket->SendTo() in SendControlInfoPacket() failed\n";
              }
            }
          }
          else
          {
            if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
            {
              printf("\tNot enough time to send this packet.\n");
            }
          }
          free( origPacketPtr ); 
      }// end if( numInfoToSend != 0 )
        
        
        // Check to see if there is enough time to send another control packet
        //        and send message to do so (if enough time) after backoff delay
      Time delay;

      if( resendImmediately ) // because we timed out waiting for ack
      {
          delay = Seconds(0);
      }
      else
      {
          // delay until trying to send the next packet should be longer than time to send this packet
          delay = Seconds(-1);
          while( delay < getControlPacketTrxTime() )
          {
            delay = Seconds(trxDelayRV.GetValue());
          }
      }
                
      if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
      {
          printf("Node %i: in SendControlInfoPacket()...checking to see if there's enough time left to send another control packet\n", getNodeId());
          //printf("controlPacketTrxTime = %f\n", (double)controlPacketTrxTime/(double)SECOND);
          //printf("\tcurrentTime + backoffDelay + packetTrxTime = %f\n", (double)((double)Simulator::Now().GetSeconds()+(double)delay+(double)controlPacketTrxTime)/(double)SECOND);
          //printf("\tnextTimeSlotStartTime = %f\n", (double)nextTimeSlotStartTime/(double)SECOND);
      }
                
    if( Simulator::Now().GetSeconds()+delay+getControlPacketTrxTime() < getTimeSlotCompleteTime() )
    {
        if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
        {
            printf("\tEnough time for another control packet!    Sending after delay of %i msecs\n", (int)delay.GetMilliSeconds());
            printf("\t(next time slot set to complete at %f)\n", getTimeSlotCompleteTime().GetSeconds());
        }
                    
        Simulator::Schedule ( delay, &RoutingProtocol::SendControlInfoPacket, this );
        if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
        {
            printf("\tNode %i:    control packets sent this time slot = %i \n", getNodeId(), 
                     getControlPacketsSentThisTimeSlot());
        }
    }
    else
    {
        if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
        {
            printf("\tnot enough time for another packet\n");
        } 
    }

  }




  void RoutingProtocol::RecvControlInfoPacket( Ptr<Packet> packet, Ipv4Address senderAddress )
  {
    if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG ) 
    {
      int senderAddrIndex = (senderAddress.Get()&(uint32_t)255) - 1;
      std::cout<<"Node " << getNodeId() << " in RecvControlInfoPacket()...packet came from " << senderAddrIndex << "\n";
    }
    numControlPacketsRcvd++;
    
    //char buf[MAX_STRING_LENGTH];
    
    //int nodeIndex = getNodeId() - 1;
    int i;//, j;
    char *origPacketPtr;
    
    bool rcvdNewInfo = false;
    
    char *packetPtr = (char *)malloc( 1000*sizeof(char) ); 

    origPacketPtr = packetPtr;
    
    uint32_t packetSize = 0;

    packetSize = packet->CopyData( (uint8_t *)packetPtr, (uint32_t)1000 );

    if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG ) 
    {
      std::cout<< "\tcopied " << packetSize <<" bytes from packet into local buffer\n";
    }

          
    if( isVARYING_Q_BITS() )
    {
      VaryingBitsUnpackControlPacket( packetPtr, packetSize, &rcvdNewInfo );
      if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
      {
        std::cout<<"\t After receiving and unpacking values:\n";
        for( i = 0; i < getNumNodes(); i++ )
        {
          int j;
          for( j = 0; j < getNumNodes(); j++ )
          {
            std::cout<<"\t\tOther backlogs[" << i << "][" << j << "] = " << getOtherBacklogs( i, j ) << "\n";
          }
        }
      }
    }
    else if( isBRUTE_FORCE() )
    {
      BruteForceUnpackControlPacket( packetPtr, packetSize );
    }
    else
    {
        if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG ) 
        {
            printf("Node %i received MSG_NETWORK_DIST_UNIV_SCHED_RCV_CONTROL_INFO_PACKET message of size %i at ", getNodeId(), (int)packetSize);    
            std::cout<< Simulator::Now ().GetSeconds () << "\n";
        }
        
        setControlPacketsRcvdThisTimeSlot( getControlPacketsRcvdThisTimeSlot() + 1 );
        
        while( packetPtr < origPacketPtr+packetSize )
        {
            // extract node identifier
            int nodeId;
            memset( &nodeId, 0, sizeof(int) );
            memcpy( &nodeId, packetPtr, sizeof(int) );
            packetPtr = packetPtr+sizeof(int);
            
            if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
            {
                printf("\tNode %i: (after extracting) nodeIdentifier = %i\n", getNodeId(), nodeId );
            }
            
            if( nodeId == getNodeId() )
            {
                setRcvdAckThisTimeSlot(true);
                setControlInfoExchangeState(JUST_FWD_NEW_INFO);
                
                packetPtr += ( (sizeof(int)*getNumNodes()) + (sizeof(double)*getNumNodes()) + (sizeof(Time)*getNumNodes()) );
            }
            else
            {
                if( isNodeInfoFirstRcv(nodeId) )
                {
                    rcvdNewInfo = true;
                    setForwardNodeCtrlInfo(nodeId, true);
                    setNodeInfoFirstRcv(nodeId, false);
                }
                
                
                // extract queue backlogs for nodeId
                for( i = 0; i < getNumNodes(); i++ )
                {
                    //            if( (tempTimeStamps[nodeId-1][i] > timeStamps[i][j]) && (i != getNodeId()-1))
                    //            {
                   
                    int temp_int;
                    memcpy(&temp_int, packetPtr, sizeof(int));
                    setOtherBacklogs( nodeId, i, temp_int);
                    if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                    {
                        printf("\tNode %i: (after extracting) otherBacklogs[%i][%i] = %i\n", getNodeId(), nodeId, i, getOtherBacklogs(nodeId,i));
                    }
                    //            }
                    packetPtr += sizeof(int);
                }
                
                // extract channel rates
                for( i = 0; i < getNumNodes(); i++ )
                {                                                                        // (don't want to copy others' views of own info
                    //        if( (tempTimeStamps[i][j] > timeStamps[i][j]) && (i != getNodeId()-1))
                    //        {
                    double temp_double;
                    memcpy(&temp_double, packetPtr, sizeof(double));
                    setChannelRates( nodeId, i, temp_double );
                    if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                    {
                        printf("\tNode %i: (after extracting) channelRates[%i][%i] = %f\n", getNodeId(), nodeId, i, getChannelRates(nodeId,i));
                    }
                    //                    timeStamps[i][j] = tempTimeStamps[i][j];
                    //                    if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                    //                    {
                    //                        printf("\tNode %i: updating time stamp[%i][%i] to %f\n", getNodeId(), i, j, (double)timeStamps[i][j]/(double)SECOND);
                    //                        printf("Node %i: received new information\n", getNodeId());
                    //                    }
                    //                allInfoRcvd = false;
                    //        }    
                    packetPtr += sizeof(double);                
                }
                
                // extract time stamps from received packet
                //    packetPtr = MESSAGE_ReturnPacket(msg);
                //    packetPtr += ((numNodes*numCommodities*sizeof(int))+(sizeof(double)*numNodes*numNodes));
                for( i = 0; i < getNumNodes(); i++ )
                {    
                    Time temp_time;
                    memcpy(&temp_time, packetPtr, sizeof(Time));
                    setTempTimeStamps( nodeId, i, temp_time );
                    /*
                    if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                    {
                        printf("\tNode %i: (after extracting) tempTimeStamps[%i][%i] = %f",
                                getNodeId(), nodeId, i, (double)getTempTimeStamps(nodeId, i));
                        printf("\t\t\tNode %i:    timeStamps[%i][%i] = %f\n", getNodeId(), nodeId, i, (double)timeStamps[nodeId][i]/(double)SECOND);
                    }
                    */
                    packetPtr += sizeof(Time);
                }
            }
        }
    }
    
    free( origPacketPtr ); 

    if( isBRUTE_FORCE() )
    {
      // no need to adjust state or anything because brute force exchange info forward and backward functions will take care of it
      return;
    }
    
    // change to appropriate control info exchange state
    switch ( getControlInfoExchangeState() ) 
    {
        case SEND_OWN_INFO:
        {
            if( rcvdNewInfo )
            {
                setControlInfoExchangeState(SEND_OWN_AND_FWD_NEW_INFO);
                
                if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                {
                    printf( "\tNode %i: Changing state to SEND_OWN_AND_FWD_NEW_INFO (from SEND_OWN_INFO)\n", getNodeId() );
                }
            }
            
            break;
        }
        case WAIT_FOR_ACK_OR_NEW_INFO:
        {
            if( isRcvdAckThisTimeSlot() )
            {
                setControlInfoExchangeState(JUST_FWD_NEW_INFO);
                
                if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                {
                    printf( "\tNode %i: Changing state to JUST_FWD_NEW_INFO (from WAIT_FOR_ACK_OR_NEW_INFO)\n", getNodeId() );
                }
            }
            else
            {
                setControlInfoExchangeState(SEND_OWN_AND_FWD_NEW_INFO);
                
                if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                {
                    printf( "\tNode %i: Changing state to SEND_OWN_AND_FWD_NEW_INFO (from WAIT_FOR_ACK_OR_NEW_INFO)\n", getNodeId() );
                }
            }
            
            break;
        }
        case SEND_OWN_AND_FWD_NEW_INFO:
        {
            if( isRcvdAckThisTimeSlot() )
            {
                setControlInfoExchangeState(JUST_FWD_NEW_INFO);
                
                if( DIST_UNIV_SCHED_EXCHANGE_INFO_DEBUG )
                {
                    printf( "\tNode %i: Changing state to JUST_FWD_NEW_INFO (from SEND_OWN_AND_FWD_NEW_INFO)\n", getNodeId() );
                }
            }
            
            break;
        }
        case JUST_FWD_NEW_INFO:
        {
            // don't need to change state
            
            break;
        }
        case DATA_TRX:
        {
            // don't need to change state
            
            break;
        }
        default:
        {
          printf( "ERROR:    Node %i:    unknown value of controlInfoExchangeState in RecvControlInfoPacket() in dus-routing-protocol.cc\n",
                    getNodeId() );
            break;
        }
    }

  }


// /**
// FUNCTION: SendDataPacket
// LAYER   : NETWORK
// PURPOSE : Function called initially by Complete Time Slot function and then subsequently
//				RecvDataAckPacket() to continually send data packets to
//				the selected recipient in dist. univ. sched. protocol until end of current
//				time slot (or until messageBuffer is empty).
// PARAMETERS:
// +node:Node *::Pointer to node
// +recipient:int:Index of node to which data packet should be sent
// +commodity:int:Index of commodity which is being sent
// RETURN   ::void:NULL
// **/
  void
  RoutingProtocol::SendDataPacket( int recipient, int commodity )
  {
    //  if there is not more time to send data packets then we break the send/send_ack cycle here
    //     this variable is set at the bottom of this function
    if( !isTimeToTrxMoreData() )
    {
      if( DIST_UNIV_SCHED_SEND_DATA_PACKET_DEBUG )
      {
        std::cout<<"Node " << getNodeId() << " in SendDataPacket(): not time to trx more data.\n";
      }
      return;
    }

    double powerCost;
    Time timeCost;
    if( DIST_UNIV_SCHED_SEND_DATA_PACKET_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " is in SendDataPacket()...sending data type " << commodity << " to node " << recipient << "\n";
    }

    if( commodity == -1 ) // commodity not set by calling function...use last chosen value in Complete Time Slot(), which should be weightCommodity[getNodeId()][receivingNode]
    {
      commodity = getWeightCommodity(getNodeId(), recipient);
    }
	
    if( DIST_UNIV_SCHED_SEND_DATA_PACKET_DEBUG )
    {
      printf("\tAttempting to send a packet with destination %i from node %i to node %i...number of packets in queue = %i\n", 
			     commodity, getNodeId(), recipient, (int)queues[commodity].GetNPackets());
      printf("\tBacklog= %i\n\tpacketsRcv = %i\n\tpacketsTrx = %i\n", 
			     queues[commodity].getM_backlog(),
			     getPacketsRcvThisTimeSlot(commodity),
			     getPacketsTrxThisTimeSlot(commodity));
    }
	
    if( getControlInfoExchangeState() != DATA_TRX )
    {
    	if( DIST_UNIV_SCHED_SEND_DATA_PACKET_DEBUG )
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
      //Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("OfdmRate6Mbps") );
      Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("DsssRate1Mbps") );
    }
    else if( getChannelRates( getNodeId(), recipient ) == 1.0 )
    {
      //Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("OfdmRate6Mbps") );
      Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("DsssRate1Mbps") );
    }
    else if( getChannelRates( getNodeId(), recipient ) == 2.0 )
    {
      //Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("OfdmRate9Mbps") );
      Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("DsssRate2Mbps") );
    }
    else if( getChannelRates( getNodeId(), recipient ) == 5.5 )
    {
      //Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("OfdmRate12Mbps") );
      Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("DsssRate5_5Mbps") );
    }
    else if( getChannelRates( getNodeId(), recipient ) == 11.0 )
    {
      //Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("OfdmRate18Mbps") );
      Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("DsssRate11Mbps") );
    }
    else
    {
      std::cout<<"ERROR: Could not find the matching channel rate from node " << getNodeId() << " to node " << recipient << " in SendDataPacket()\n";
      exit(-1);
    }

    if( DIST_UNIV_SCHED_SEND_DATA_PACKET_DEBUG )
    {
    	printf( "\tNode %i:    setting physical layer rate to index %i\n", getNodeId(), getChannelRateIndex( getNodeId(), recipient) ); 
    }
	
    if( DIST_UNIV_SCHED_SEND_DATA_PACKET_DEBUG )
    {
      printf("Attempting to send packet:\nFrom %i to %i, commodity %i\n",
              getNodeId(), recipient, commodity);
    }

    Ptr<Packet> packet;
    // No need to add packet tag with destination (commodity) because it was added before queueing

    if( queues[commodity].GetNPackets() > 0 )
    {
      packet = queues[commodity].Dequeue()->Copy(); 
    }
    else
    {
      if( DIST_UNIV_SCHED_SEND_DATA_PACKET_DEBUG )
      {
        std::cout<<"Node " << getNodeId() << " trying to send data packet to node " << recipient << ", but queue is empty.\n";
      }
      return;
    }
    uint32_t packetSize = packet->GetSize();
    // Send Control Packet as subnet directed broadcast from each interface used by distUnivSched
  
    std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin();
    //for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
    //    m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    Ptr<Socket> socket = j->first;
    Ipv4InterfaceAddress iface = j->second;

    if( DIST_UNIV_SCHED_SEND_DATA_PACKET_DEBUG )
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
  
    // update battery power used
    timeCost = MicroSeconds((uint64_t)(((double)packetSize*8.0)/getChannelRates(getNodeId(), recipient))); // size changed to bits, channel rate given in Mbps, so result is in MicroSeconds 
    powerCost = DUS_TRX_POWER_MW*(timeCost.GetSeconds()); // units are milliwatt-seconds /3600.0); // units are milliwatts and hours

    if( isFINITE_BATTERY() )
    { 
      setBatteryPowerLevel( getBatteryPowerLevel() - powerCost );
    }

    setPowerUsed( getPowerUsed() + powerCost );

    setDataPacketsSent(recipient, getDataPacketsSent(recipient) + 1);
	
		// wait until end of time slot to update queues
    setPacketsTrxThisTimeSlot(commodity, getPacketsTrxThisTimeSlot(commodity)+1); 

    Time timeToTrx = Simulator::Now() + timeCost + MilliSeconds(10);
    if( DIST_UNIV_SCHED_SEND_DATA_PACKET_DEBUG )
    {
      std::cout<<"\tNow() + timeCost = " << timeToTrx.GetSeconds() << "...next time slot starts at " << getNextTimeSlotStartTime().GetSeconds() << "\n";
    }
    if( timeToTrx > getNextTimeSlotStartTime() )
    {
      if( DIST_UNIV_SCHED_SEND_DATA_PACKET_DEBUG )
      {
        std::cout<<"\tSetting timeToTrxMoreData to false\n";
      }
      setTimeToTrxMoreData( false );
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
  RoutingProtocol::RecvDataPacket ( Ptr<Packet> packet, Ipv4Address senderAddress, Ipv4Address receiverAddress )
  {
    if( DIST_UNIV_SCHED_RECV_DATA_PACKET_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << ":  in RecvDataPacket()\n";
    }

    DistUnivSchedDestTag tag;
    tag.dest = -1;
    if( packet->RemovePacketTag (tag) )
    {
      if( DIST_UNIV_SCHED_RECV_DATA_PACKET_DEBUG )
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
      stats->setNumPacketsReachDest( tag.dest, stats->getNumPacketsReachDest( tag.dest ) + 1 );
      stats->setNumPacketsReachDestThisSecond( tag.dest, stats->getNumPacketsReachDestThisSecond( tag.dest ) + 1 );
      if( DIST_UNIV_SCHED_RECV_DATA_PACKET_DEBUG )
      {
        std::cout<<"Received packet at destination!\n";
      }
    }
    else
    {
      // packet has not reached destination...must place in proper queue to forward according to dus protocol
      //  don't update queue.m_backlog value here...that's done in complete time slot function to ensure consistency
      //  also need to remove SocketAddressTag since it will be added (again) when sent out after being dequeued
      SocketAddressTag sockAddrTag;
      packet->RemovePacketTag(sockAddrTag);
      queues[tag.dest].Enqueue( packet );
      setPacketsRcvThisTimeSlot( tag.dest, getPacketsRcvThisTimeSlot(tag.dest) + 1 );

      if( DIST_UNIV_SCHED_RECV_DATA_PACKET_DEBUG )
      {
        std::cout<<"\tpacketsRcvThisTimeSlot[" << tag.dest << "] = " << getPacketsRcvThisTimeSlot(tag.dest) << "\n";
        std::cout<<"\tQueueing packet into destination index " << tag.dest << "...backlog = " << queues[tag.dest].getM_backlog() << "\n";
        std::cout<<"\tGetNPackets = " << queues[tag.dest].GetNPackets() << "\n";
      }
    }

    if( localAddrIndex != senderAddrIndex )
    {
      // only want to keep track of data packets received from other nodes
      dataPacketsRcvd[senderAddrIndex]++;
      // packet was received from another node -> need to send an ACK for the data
      SendDataAckPacket( receiverAddress, senderAddress, tag.dest );
    }
  }

// /**
// FUNCTION: SendDataAckPacket
// LAYER   : NETWORK
// PURPOSE : Function called by RecvDataPacket().  Needs to send an ack packet to let the 
//            sending node know to release the data packet and send another one.
//            This loop should continue for entire data_trx portion of time slot
// PARAMETERS:
// +senderAddress : Ipv4Address : own address - node sending the ack
// +receiverAddress : Ipv4Address : address of node receiving the ack
// +commodity : int : Index of commodity which is being sent
// RETURN   ::void:NULL
// **/
  void
  RoutingProtocol::SendDataAckPacket( Ipv4Address senderAddress, Ipv4Address receiverAddress, int commodity )
  {
    int recipient = (receiverAddress.Get()&(uint32_t)255) - 1; 
    if( DIST_UNIV_SCHED_SEND_DATA_ACK_PACKET_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " in SendDataAckPacket() at time = " << Simulator::Now().GetSeconds() << "...commodity = "<<commodity<< "...sending ack to node "<<recipient<<"\n";
    }

    if( commodity == -1 ) // commodity not set by calling function...use last chosen value in Complete Time Slot(), which should be weightCommodity[getNodeId()][receivingNode]
    {
      commodity = getWeightCommodity(getNodeId(), recipient);
      NS_ASSERT_MSG( commodity > -1, "ERROR in SendDataAckPacket().  Don't know the commodity.  This shouldn't happen\n" );
      exit(-1);
    }
	
    // make sure physical layer rate is set to ideal rate
    if( getChannelRates( getNodeId(), recipient ) == 0.0 )
    {
      //Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("OfdmRate6Mbps") );
      Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("DsssRate1Mbps") );
    }
    else if( getChannelRates( getNodeId(), recipient ) == 1.0 )
    {
      //Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("OfdmRate6Mbps") );
      Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("DsssRate1Mbps") );
    }
    else if( getChannelRates( getNodeId(), recipient ) == 2.0 )
    {
      //Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("OfdmRate9Mbps") );
      Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("DsssRate2Mbps") );
    }
    else if( getChannelRates( getNodeId(), recipient ) == 5.5 )
    {
      //Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("OfdmRate12Mbps") );
      Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("DsssRate5_5Mbps") );
    }
    else if( getChannelRates( getNodeId(), recipient ) == 11.0 )
    {
      //Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("OfdmRate18Mbps") );
      Simulator::Schedule( Seconds(0.0), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("DsssRate11Mbps") );
    }
    else
    {
      std::cout<<"ERROR: Could not find the matching channel rate from node " << getNodeId() << " to node " << recipient << " in SendDataPacket()\n";
      exit(-1);
    }

    if( DIST_UNIV_SCHED_SEND_DATA_ACK_PACKET_DEBUG )
    {
      printf( "\tNode %i:    setting physical layer rate to index %i\n", getNodeId(), getChannelRateIndex( getNodeId(), recipient) );
    }
	
    if( DIST_UNIV_SCHED_SEND_DATA_ACK_PACKET_DEBUG )
    {
      printf("Attempting to send packet:\nFrom %i to %i, commodity %i\n",
		getNodeId(), recipient, commodity);
    }
	
    // Send Control Packet as subnet directed broadcast from each interface used by distUnivSched
    for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
           m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;


	    if( DIST_UNIV_SCHED_SEND_DATA_ACK_PACKET_DEBUG )
      { 
        std::cout<<"\ttrying to send out interface with address: ";
        iface.GetLocal().Print( std::cout );
        std::cout<<"\n";
      }

      Ptr<Packet> ackPacket = Create<Packet> ();
   
      DistUnivSchedDestTag tag (commodity);
      if (!ackPacket->PeekPacketTag (tag))
      {
        ackPacket->AddPacketTag (tag);
      }
      
      DistUnivSchedPacketTypeTag tag2 (DUS_DATA_ACK);
      if (!ackPacket->PeekPacketTag (tag2))
      {
        ackPacket->AddPacketTag (tag2);
      }

      TypeHeader tHeader (DUS_DATA_ACK);
      ackPacket->AddHeader (tHeader);

      char buf[16];
      sprintf( buf, "10.0.0.%i", recipient+1 );
      Ipv4Address tempAddr( buf );
      if( DIST_UNIV_SCHED_SEND_DATA_ACK_PACKET_DEBUG )
      { 
        std::cout<<"\tSending to address: ";
        tempAddr.Print(std::cout);
        std::cout<<"\n";
      }

      if( (socket->SendTo (ackPacket, 0, InetSocketAddress (tempAddr, DUS_PORT))) == -1 )
      {
        std::cout<<"ERROR:  socket->SendTo() in SendControlInfoPacket() failed\n";
      }
      else
      {
        numDataAcksSent++;
      }
    }
  }

// /**
// FUNCTION: RecvDataAckPacket
// LAYER   : NETWORK
// PURPOSE : Function called by Recv Dist Univ Sched() when it receives a 
//            data ack packet from another node.  tag.dest specifies the destination
//            of the data packet received, i.e., the commodity number of the data
// PARAMETERS:
// +packet : Ptr<Packet> : pointer to received packet
// +senderAddress : Ipv4Address : address of node sending ack packet...used to know where to send next data packet
// +receiverAddress : Ipv4Address : address of node receiving ack packet...used for ?
// RETURN   ::void:NULL
// **/
  void
  RoutingProtocol::RecvDataAckPacket ( Ptr<Packet> packet, Ipv4Address senderAddress, Ipv4Address receiverAddress )
  {
    if( DIST_UNIV_SCHED_RECV_DATA_ACK_PACKET_DEBUG )
    { 
      std::cout<<"Node " << getNodeId() << ":  in RoutingProtocol::RecvDataAckPacket() at time = " << Simulator::Now().GetSeconds() << "\n";
    }

    int commodity = -1;

    DistUnivSchedDestTag tag;
    if( packet->RemovePacketTag (tag) )
    {
      if( DIST_UNIV_SCHED_RECV_DATA_ACK_PACKET_DEBUG )
      { 
        std::cout<<"\tdestination index of packet = " << tag.dest << "\n";
      }
      commodity = tag.dest; 
    }
    NS_ASSERT_MSG( commodity > -1, "ERROR:  Received a data packet with no destination index tag.\n" );

    numDataAcksRcvd++;

    int senderAddrIndex = (senderAddress.Get()&(uint32_t)255) - 1;
    SendDataPacket( senderAddrIndex, commodity ); 

    // Destroy Packet?

  }


  void 
  RoutingProtocol::ExchangedInfoMatchesGlobal()
  {
    if( DIST_UNIV_SCHED_EXCHANGED_MATCHES_GLOBAL_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " is in ExchangedInfoMatchesGlobal()\n";
    }
  }

// /**
// FUNCTION: VaryingBitsCreateControlPacket
// LAYER   : NETWORK
// PURPOSE : Function called by Send Control Info Packet().  This is for the case when
//            the number of bits used to represent either queue states or channel states
//            is being varied while using the general network state exchange algorithm, 
//            i.e., not global or line network.
//           All network state values are packed into a block of memory here that is used
//            as the packet's data by the calling function.  In the case of varying q bits,
//            the values in other backlogs should already by discretized accordingly, so they
//            can be used directly here without conversion.
// PARAMETERS:
// +packetPtr : char * : 
// +packetSize : uint32_t * : the size of the packet (in bytes) 
// RETURN   ::void:NULL
// **/
  void
  RoutingProtocol::VaryingBitsCreateControlPacket( char *packetPtr, uint32_t packetSize, int numInfoToSend )
  {
    if( DIST_UNIV_SCHED_VARY_BITS_CREATE_PACKET_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << " is in VaryingBitsCreateControlPacket()\n";
    }

    int i, j, k, x;
    //int numInfoToSend = 0;

    int *packetArray;
    int packetArrayIndex = 0;
    
    // allocate space for identifier and backlog x commodity and rate x node and    node x timestamp
    int numIdentifierBits = ceil( log2( getNumNodes() ) );
    int numBacklogBits = getQBits()*getNumCommodities();
    int numRateBits = getRateBits()*getNumNodes();
    int totalNumBits = numIdentifierBits + numBacklogBits + numRateBits; // TODO : timestamps needed??

    int numBytesOneNodeInfo = (int)packetSize/numInfoToSend;

    packetArray = (int *)malloc( sizeof(int)*numBytesOneNodeInfo*8 );

    // initialize packetArray
    for( i = 0; i < totalNumBits; i++ )
    {
      packetArray[i] = 0;
    }

    if( DIST_UNIV_SCHED_VARY_BITS_CREATE_PACKET_DEBUG )
    {
        printf("\tpacketSize = %i bytes: numIdentifierBits = %i, numBacklogBits = %i, numRateBits = %i\n", packetSize, numIdentifierBits, numBacklogBits, numRateBits);
    }
   
    // Load everything into packetArray first, treating each slot in array as a bit in the packet
    //   then, each byte in the packet will be created individually and copied into packet as the last step
    uint32_t temp_int;

    for( x = 0; x < numInfoToSend; x++ )
    {
      for( k = 0; k < getNumNodes(); k++ )
      {
        if( isForwardNodeCtrlInfo(k) )
        {
          setForwardNodeCtrlInfo(k, false);
          break;
        }
      }
      // initialize packetArray and packetArrayIndex
      for( i = 0; i < totalNumBits; i++ )
      {
        packetArray[i] = 0;
      }
      packetArrayIndex = 0;

      // load info identifier into p
      temp_int = (uint32_t)k; //getNodeId();
      if( DIST_UNIV_SCHED_VARY_BITS_CREATE_PACKET_DEBUG )
      {
        printf("\tNode %i: loading node identifier = %i, %u into packet\n", getNodeId(), k, temp_int);
      }
      for( i = 0; i < numIdentifierBits; i++ )
      {
        if( ((temp_int>>i)&(uint32_t)1) == (uint32_t)1 ) // check bit i
        {
          packetArray[i] = 1;
        }
      }
  
      packetArrayIndex += numIdentifierBits;
  
      //setOtherBacklogs( getNodeId(), 2, 7 );
      
      for( i = 0; i < getNumNodes(); i++ )
      {
        temp_int = (uint32_t)getOtherBacklogs( k, i );
        if( DIST_UNIV_SCHED_VARY_BITS_CREATE_PACKET_DEBUG )
        {
          printf("\tNode %i: loading backlog[%i][%i] = %u into packet\n", getNodeId(), k, i, temp_int);
        }
  
        for( j = 0; j < getQBits(); j++ )
        {
          //std::cout<<"bit " << j << " = ";
          //printf( "temp_int>>%i: %x\n", j, temp_int>>j );
          if( ((temp_int>>j)&(uint32_t)1) == (uint32_t)1 ) // check bit j
          {
            packetArray[packetArrayIndex+(i*getQBits())+j] = 1;
          }
        }
      } 
  
      packetArrayIndex += numBacklogBits;
      
      // now load all known channel rates indices into message
      for( i = 0; i < getNumNodes(); i++ )
      {
        if( DIST_UNIV_SCHED_VARY_BITS_CREATE_PACKET_DEBUG )
        {
          printf("\tNode %i: loading channelRateIndex[%i][%i] = %i into packet\n", getNodeId(), k, i, getChannelRateIndex( k, i ));
        }
  
        temp_int = (uint32_t)getChannelRateIndex( k, i ); //TODO : I think this needs fixed
        for( j = 0; j < numRateBits; j++ )
        {
          if( ((temp_int>>j)*(uint32_t)1) == (uint32_t)1 ) // check bit j
          {
            packetArray[packetArrayIndex+(i*getRateBits())+j] = 1;
          }
        }
      }
      if( DIST_UNIV_SCHED_VARY_BITS_CREATE_PACKET_DEBUG )
      {
        printf("\tLoaded channel rates\nPacket Array:\n");
        PrintArray( packetArray, totalNumBits ); 
      }
    
      // fill packet with data
      for( i = 0; i < numBytesOneNodeInfo; i++ ) // number of bytes in single node info
      {
        temp_int = (uint32_t)0;
        for( j = 0; j < 8; j++ )
        {
          if( packetArray[ (i*8)+j ] == 1 )
          {
            temp_int = temp_int|((uint32_t)1<<j); 
          }
        }
        if( DIST_UNIV_SCHED_VARY_BITS_CREATE_PACKET_DEBUG )
        {
          std::cout<<"byte " << i << " of packet = " << temp_int <<"\n";
        }
        
        memcpy( packetPtr, &temp_int, sizeof(char) );
        packetPtr += sizeof(char);
  
      }
    }

    free( packetArray );

    if( DIST_UNIV_SCHED_VARY_BITS_CREATE_PACKET_DEBUG )
    {
      std::cout<<"Node " << getNodeId() << ":  Done in VaryingBitsCreateControlPacket()\n";
    }
  }

// /**
// FUNCTION: VaryingBitsUnpackControlPacket
// LAYER   : NETWORK
// PURPOSE : Function called by Recv Control Info Packet().  This is for the case when
//            the number of bits used to represent either queue states or channel states
//            is being varied while using the general network state exchange algorithm, 
//            i.e., not global or line network.
// PARAMETERS:
// +packetPtr : char ** : this is sent in to mark the beginning of the data memory block holding the 
//                         data copied from the packet...this memory is freed back in the caling function
// +packetSize : uint32_t  :
// +rcvdNewInfo : bool * : used to return whether or not anything received is new, which dictates the state transition
// RETURN   ::void:NULL
// **/
  void
  RoutingProtocol::VaryingBitsUnpackControlPacket( char *packetPtr, uint32_t packetSize, bool *rcvdNewInfo )
  {
    int i, j, k, x;

    int numIdentifierBits = ceil( log2( getNumNodes() ) );
    int numBacklogBits = getQBits()*getNumCommodities();
    int numRateBits = getRateBits()*getNumNodes();
    int numBytesOneNodeInfo = ceil(double(numIdentifierBits + numBacklogBits + numRateBits)/8.0);
    int numBitsOneNodeInfo = numBytesOneNodeInfo*8;
    int numInfoRcvd = packetSize/numBytesOneNodeInfo;
   // int totalNumBits = packetSize*8; // TODO : timestamps needed??
    
    int *packetArray = (int *)malloc( sizeof(int)*(numBytesOneNodeInfo*8) ); // this memory is malloced and freed both in this ftn

    if( DIST_UNIV_SCHED_VARY_BITS_UNPACK_PACKET_DEBUG )
    {
      std::cout<<"Node " << getNodeId() <<":  In VaryingBitsUnpackControlPacket() at " << Simulator::Now().GetSeconds() << "...packetSize = " << (int)packetSize << "\n";
    } 
    
    setControlPacketsRcvdThisTimeSlot( getControlPacketsRcvdThisTimeSlot() + 1 );

    uint8_t temp_int;

    for( x = 0; x < numInfoRcvd; x++ )
    {
      // first unpack every bit in a single node's info into packet array
      for( i = 0; i < numBytesOneNodeInfo; i++ )
      {
        temp_int = (uint8_t)0;
        memcpy( &temp_int, packetPtr, sizeof(uint8_t) );
        packetPtr += sizeof(uint8_t);

        if( DIST_UNIV_SCHED_VARY_BITS_UNPACK_PACKET_DEBUG )
        {
          std::cout<<"byte " << i << " = " << temp_int << "\n";
        }

        for( j = 0; j < 8; j++ )
        {
          if( ((temp_int>>j)&(uint8_t)1) == (uint32_t)1 )
          {
            packetArray[ (i*8)+j ] = 1;
          }
          else
          {
            packetArray[ (i*8)+j ] = 0;
          }
        }
      } 

      if( DIST_UNIV_SCHED_VARY_BITS_UNPACK_PACKET_DEBUG )
      {
        PrintArray( packetArray, numBytesOneNodeInfo*8 );
        std::cout<<"numBitsOneNodeInfo = " << numBitsOneNodeInfo <<"\n"; 
        std::cout<<"numIdentifierBits + numBacklogBits + numRateBits" << numIdentifierBits + numBacklogBits + numRateBits << "\n";
      }

      // now parse out bits in packet array and store values' meanings locally
 
      i = 0; 
      while( i < numIdentifierBits + numBacklogBits + numRateBits )// numBitsOneNodeInfo )
      {
        // extract node identifier
        int nodeId;
        temp_int = (uint8_t)0;
        for( j = 0; j < numIdentifierBits; j++ )
        {
          if( packetArray[i++] == 1 )
          {
            temp_int = temp_int | (uint8_t)1<<j; 
          } 
        }

        nodeId = temp_int;
      
        if( DIST_UNIV_SCHED_VARY_BITS_UNPACK_PACKET_DEBUG )
        {
          printf("\tNode %i: (after extracting) nodeIdentifier = %i\n", getNodeId(), nodeId );
        }
        
        if( nodeId == getNodeId() )
        {
          if( DIST_UNIV_SCHED_VARY_BITS_UNPACK_PACKET_DEBUG )
          {
            std::cout<< "Node identifier and nodeId are equal...setting ack rcvd and switching to just fwd new info state\n";
          }
          setRcvdAckThisTimeSlot(true);
          setControlInfoExchangeState(JUST_FWD_NEW_INFO);
        
          i += numBacklogBits + numRateBits;
          break;
        }
        else
        {
          if( isNodeInfoFirstRcv(nodeId) )
          {
              *rcvdNewInfo = true;
              setForwardNodeCtrlInfo(nodeId, true);
              setNodeInfoFirstRcv(nodeId, false);
          }
        
       
          // extract queue backlogs for nodeId
          for( j = 0; j < getNumNodes(); j++ )
          {
            temp_int = (uint8_t)0;
          
            for( k = 0; k < getQBits(); k++ )
            {
              if( packetArray[i++] == 1 )
              {
                temp_int = temp_int | (uint8_t)1<<k;
              }
            }
        
            setOtherBacklogs( nodeId, j, (int)temp_int);
            if( DIST_UNIV_SCHED_VARY_BITS_UNPACK_PACKET_DEBUG )
            {
              printf("\tNode %i: (after extracting) otherBacklogs[%i][%i] = %i\n", getNodeId(), nodeId, j, getOtherBacklogs(nodeId,j));
            }
          }
        
          // extract channel rates
          for( j = 0; j < getNumNodes(); j++ )
          {                                   
            temp_int = (uint8_t)0;
            for( k = 0; k < getRateBits(); k++ )
            {
              if( packetArray[i++] == 1 )
              {
                temp_int = temp_int | (uint8_t)1<<k;
              }
            }
            setChannelRateIndex( nodeId, j, (int)temp_int );
            if( DIST_UNIV_SCHED_VARY_BITS_UNPACK_PACKET_DEBUG )
            {
              printf("\tNode %i: (after extracting) channelRates[%i][%i] = %f\n", getNodeId(), nodeId, j, getChannelRates(nodeId,j));
            }
          }
        
        }
      }
    }
    free( packetArray );
  }

  void
  RoutingProtocol::BruteForceUnpackControlPacket( char *packetPtr, uint32_t packetSize )
  {
    int i, j; 
//    char *tempPacket = (char *)malloc( sizeof(int)*numNodes + sizeof(int)*numNodes*numCommodities + sizeof(double)*numNodes*numNodes + sizeof(double)*numNodes*2 );

    //packetSize = packet->CopyData( (uint8_t *)tempPacket, (uint32_t)1000 );
    //packetPtr = tempPacket;
    
    bool valid[getNumNodes()];

    int temp_int;
    double temp_double;
    // extract queue backlogs from other nodes that have valid info in this packet
    if( getNodeId() != 0 )
    {
        if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
        {
            printf("packetPtr = %p\n", packetPtr);
        }
       
        for( i = 0; i < getNumNodes(); i++ )
        { 
          // extract valid markers
          memcpy(&temp_int, packetPtr, sizeof(int));
          if( temp_int == 1 || i == getNumNodes()-1 )
          {
            valid[i] = true;
            setValidInfo( i, true );
          }
          else
          {
            valid[i] = false;
          }
        }

        for( i = 0; i < getNumNodes(); i++ )
        {
            if( !valid[i] )
            {
               packetPtr+= sizeof(int)*getNumCommodities();
               continue;
            } 
            for( j = 0; j < getNumCommodities(); j++ )
            {
                memcpy(&temp_int, packetPtr, sizeof(int));
                setOtherBacklogs( i, j, temp_int );
                if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
                {
                    printf("\tNode %i: (after extracting) otherBacklogs[%i][%i] = %i\n", getNodeId(), i, j, getOtherBacklogs(i,j));
                }
                packetPtr += sizeof(int);
            }
        }
        // extract channel rates
        if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
        {
            printf("packetPtr = %p\n", packetPtr);
        }
        for( i = 0; i < getNumNodes(); i++ )
        {
            if( !valid[i] )
            {
               packetPtr+= sizeof(double)*getNumNodes();
               continue;
            } 
            for( j = 0; j < getNumNodes(); j++ )
            {
                memcpy(&temp_double, packetPtr, sizeof(double));
                setChannelRates( i, j, temp_double );
                if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
                {
                    printf("\tNode %i: (after extracting) channelRates[%i][%i] = %f\n", getNodeId(), i, j, temp_double);
                }
                packetPtr += sizeof(double);
            }
        }
        // extract node coords
        for( i = 0; i < getNumNodes(); i++ )
        {
          if( !valid[i] )
          {
             packetPtr+= sizeof(double)*2;
             continue;
          } 
          memcpy(&temp_double, packetPtr, sizeof(double));
          setNodeCoordX( i, temp_double );
          packetPtr += sizeof(double);

          memcpy(&temp_double, packetPtr, sizeof(double));
          setNodeCoordY( i, temp_double );
          packetPtr += sizeof(double);
          if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
          {
            printf("\tNode %i: (after extracting) nodeCoordX[%i] = %f,  nodeCoordY[%i] = %f\n", 
                   getNodeId(), i, getNodeCoordX(i), i, getNodeCoordY(i) );
          }
        }
    } 
    
  }

  void
  RoutingProtocol::BruteForceCreateControlPacket( char *packetPtr, uint32_t packetSize )
  {
    int i, j;

    int temp_int;
    double temp_double;
      // load valid tags
      for( i = 0; i < getNumNodes(); i++ )
      {
        if( getValidInfo(i) )
        {
          temp_int = 1;
        }
        else
        {
          temp_int = 0;
        }
      }
      // load all known backlogs into message (this node and all before it)
      for( i = 0; i < getNumNodes(); i++ )
      {
        for( j = 0; j < getNumCommodities(); j++ )
        {
              temp_int = getOtherBacklogs( i, j );
              memcpy(packetPtr, &temp_int, sizeof(int));    
              if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
              {
                  printf("\tNode %i: loading otherBacklogs[%i][%i] = %i into packet\n", getNodeId(), i, j, getOtherBacklogs(i,j));
              }
            //printf("\tvalue placed in packet: %i\n", (int)*packetPtr);
            packetPtr += sizeof(int);
        }
      }
      if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
      {
        printf("Loaded all known backlogs...\n");
      }

      // now load all known channel rates into message
      for( i = 0; i < getNumNodes(); i++ )
      {
        for( j = 0; j < getNumNodes(); j++ )
        {
            if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
            {
                printf("\tNode %i: loading channelRate[%i][%i] = %f into packet\n", getNodeId(), i, j, getChannelRates(i,j));
            }
            temp_double = getChannelRates(i,j);
            memcpy(packetPtr, &temp_double, sizeof(double));
            packetPtr += sizeof(double);
        }
      }
      if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
      {
        printf("\tLoaded channel rates\n");
      }    
      for( i = 0; i < getNumNodes(); i++ )
      {
        if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
        {
            printf("\tNode %i: loading nodeCoord[%i].x = %f and nodeCoord[%i].y = %f into packet\n", 
                   getNodeId(), i, getNodeCoordX(i), i, getNodeCoordY(i) );
        }
        temp_double = getNodeCoordX(i);
        memcpy(packetPtr, &temp_double, sizeof(double));
        packetPtr += sizeof(double);

        temp_double = getNodeCoordY(i);
        memcpy(packetPtr, &temp_double, sizeof(double));
        packetPtr += sizeof(double);
      }
      if( DIST_UNIV_SCHED_EXCHANGE_BRUTE_FORCE_INFO_DEBUG )
      {
          printf("\tLoaded node coordinates\n");
      }
      
  }        



  int 
  RoutingProtocol::DiscreteQValue( int backlog, int qBits )
  {
    int discreteValue = (double)backlog/( (double)getMaxBufferSize()/pow( 2.0, (double)qBits ) );
    if( DIST_UNIV_SCHED_DISCRETE_Q_VALUE_DEBUG && getNodeId() == 0 )
    {
      std::cout<<"Buffer size = " << getMaxBufferSize() << ", qBits = " << getQBits() << "\n";
      std::cout<<"Actual queue value = " << backlog << ", discrete value = " << discreteValue << "\n";
    }
    
    return discreteValue;
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
  RoutingProtocol::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
  {
    if( DIST_UNIV_SCHED_ROUTE_OUTPUT_DEBUG )
    {
      std::cout<< "Node " << getNodeId() << " in RouteOutput()\n";
    }

    if (!p)
    {
      if( DIST_UNIV_SCHED_ROUTE_OUTPUT_DEBUG )
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
      p->RemovePacketTag(typeTag);
      if( typeTag.type != DUS_DATA_ACK )
      {
        std::cout<<"Expecting a DUS_DATA_ACK Packet tag here, but did not get it...exiting\n";
        exit(-1);
      }
      p->AddPacketTag(typeTag);

      if( DIST_UNIV_SCHED_ROUTE_OUTPUT_DEBUG )
      {
        std::cout<<"FOUND packet type tag = ";
        typeTag.Print(std::cout);
        std::cout<<"\tthis should be a DATA_ACK packet.\n";
      }

        // need to attach header 
        TypeHeader tHeader2 (DUS_DATA_ACK);
        p->AddHeader( tHeader2 );
      
        Ipv4Address nextHop = header.GetDestination();
      
        if( DIST_UNIV_SCHED_ROUTE_OUTPUT_DEBUG )
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
          if( DIST_UNIV_SCHED_ROUTE_OUTPUT_DEBUG )
          {
            std::cout<<"\tDestination Index = " << dst_index << "\n";
            std::cout<<"\tNext Hop Index = " << next_hop_index << "\n";
          }
          p->AddPacketTag (tag);
        }
      
        // return a route so the packet goes out the interface from here
        Ptr<Ipv4Route> route = getOutputRoutePointer(); //new Ipv4Route;
        Ipv4Address src; 
        std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); 

        Ipv4InterfaceAddress iface = j->second;
        src = iface.GetLocal();

        sockerr = Socket::ERROR_NOTERROR;

        if( DIST_UNIV_SCHED_ROUTE_OUTPUT_DEBUG )
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
      if( DIST_UNIV_SCHED_ROUTE_OUTPUT_DEBUG )
      {
        std::cout<<"\tData Packet...attaching a DUS_DATA Header.\n";
        std::cout<<"\tthis should be a DATA packet from the application.\n";
      }
      TypeHeader th(DUS_DATA);
      p->AddHeader(th);

      Ipv4Address dst = header.GetDestination();
      int dst_index = (dst.Get()&(uint32_t)255) - 1;

      if( DIST_UNIV_SCHED_ROUTE_OUTPUT_DEBUG )
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
      

      numPacketsFromApplication++;

      //std::cout<<"packet from application at time " << Simulator::Now().GetSeconds() << "\n";

      // route to loopback, so we can queue it in route input ftn and send from SendDataPacket() 
      return LoopbackRoute (header, oif);
    }
    else if( tHeader.Get() == DUS_CTRL )
    {
      if( DIST_UNIV_SCHED_ROUTE_OUTPUT_DEBUG )
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
      Ptr<Ipv4Route> route = getOutputRoutePointer(); //new Ipv4Route;
      Ipv4Address dst = header.GetDestination();
      Ipv4Address src; 
      std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); 

      Ipv4InterfaceAddress iface = j->second;
      src = iface.GetLocal();

      sockerr = Socket::ERROR_NOTERROR;

      if( DIST_UNIV_SCHED_ROUTE_OUTPUT_DEBUG )
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

      numControlPacketsSent++;

      return route;
    }
    else if( tHeader.Get() == DUS_DATA )
    {
      if( DIST_UNIV_SCHED_ROUTE_OUTPUT_DEBUG )
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
      
      if( DIST_UNIV_SCHED_ROUTE_OUTPUT_DEBUG )
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
        if( DIST_UNIV_SCHED_ROUTE_OUTPUT_DEBUG )
        {
          std::cout<<"\tDestination Index = " << dst_index << "\n";
          std::cout<<"\tNext Hop Index = " << next_hop_index << "\n";
        }
        p->AddPacketTag (tag);
      }
      
      // return a route so the packet goes out the interface from here
      Ptr<Ipv4Route> route = getOutputRoutePointer(); //new Ipv4Route;
      Ipv4Address src; 
      std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); 

      Ipv4InterfaceAddress iface = j->second;
      src = iface.GetLocal();

      sockerr = Socket::ERROR_NOTERROR;

      if( DIST_UNIV_SCHED_ROUTE_OUTPUT_DEBUG )
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

  bool 
  RoutingProtocol::RouteInput  (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb)
  {
    if( DIST_UNIV_SCHED_ROUTE_INPUT_DEBUG )
    {
      std::cout<< "Node " << getNodeId() << " in RouteInput()\n";
    }

   // NS_LOG_FUNCTION (this << p->GetUid () << header.GetDestination () << idev->GetAddress ());
    if (m_socketAddresses.empty ())
    {
      printf( "No distUnivSched interfaces\n" );
      return false;
    }
    NS_ASSERT (m_ipv4 != 0);
    NS_ASSERT (p != 0);
    // Check if input device supports IP
    NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
    int32_t iif = m_ipv4->GetInterfaceForDevice (idev);

    Ipv4Address dst = header.GetDestination ();
    Ipv4Address origin = header.GetSource ();

    if( DIST_UNIV_SCHED_ROUTE_INPUT_DEBUG )
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
          if( DIST_UNIV_SCHED_ROUTE_INPUT_DEBUG )
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
    if( DIST_UNIV_SCHED_ROUTE_INPUT_DEBUG )
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

  void 
  RoutingProtocol::NotifyInterfaceUp (uint32_t i)
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
    socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvPacket, this)); 
    socket->SetDataSentCallback (MakeCallback (&RoutingProtocol::DataSent, this) );
    socket->BindToNetDevice (l3->GetNetDevice (i));
    socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), DUS_PORT));
    socket->SetAllowBroadcast (true);
    socket->SetAttribute ("IpTtl", UintegerValue (1));
    m_socketAddresses.insert (std::make_pair (socket, iface));

    // Add local broadcast record to the routing table
    Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
   // RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ iface.GetBroadcast (), /*know seqno=*/ true, /*seqno=*/ 0, /*iface=*/ iface,
                                    ///*hops=*/ 1, /*next hop=*/ iface.GetBroadcast (), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
    //m_routingTable.AddRoute (rt);

    Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice> ();
    if (wifi == 0)
      return;
    Ptr<WifiMac> mac = wifi->GetMac ();
    if (mac == 0)
      return;
  }

  void 
  RoutingProtocol::NotifyInterfaceDown (uint32_t i)
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

  void 
  RoutingProtocol::NotifyAddAddress (uint32_t i, Ipv4InterfaceAddress address) 
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
        socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvPacket,this));
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

  void 
  RoutingProtocol::NotifyRemoveAddress (uint32_t i, Ipv4InterfaceAddress address) 
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
        socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvPacket, this));
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
    
  void 
  RoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
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

  void 
  RoutingProtocol::PrintRoutingTable (Ptr<OutputStreamWrapper> stream) const
  {
      *stream->GetStream () << "In PrintRoutingTable(). Time: " << Simulator::Now ().GetSeconds () << "s ";
      //*stream->GetStream () << "Node: " << m_ipv4->GetObject<Node> ()->GetId () << " Time: " << Simulator::Now ().GetSeconds () << "s ";
  }

  Ptr<Ipv4Route> 
  RoutingProtocol::LoopbackRoute (const Ipv4Header & hdr, Ptr<NetDevice> oif) const
  {
    //std::cout<<"Node "<< getNodeId() <<" in LoopbackRoute()\n";

    //NS_LOG_FUNCTION (this << hdr);
    NS_ASSERT (m_lo != 0);
    Ptr<Ipv4Route> rt = Create<Ipv4Route> ();
    rt->SetDestination (hdr.GetDestination ());
    //
    // Source address selection here is tricky.  The loopback route is
    // returned when AODV does not have a route; this causes the packet
    // to be looped back and handled (cached) in RouteInput() method
    // while a route is found. However, connection-oriented protocols
    // like TCP need to create an endpoint four-tuple (src, src port,
    // dst, dst port) and create a pseudo-header for checksumming.  So,
    // AODV needs to guess correctly what the eventual source address
    // will be.
    //
    // For single interface, single address nodes, this is not a problem.
    // When there are possibly multiple outgoing interfaces, the policy
    // implemented here is to pick the first available AODV interface.
    // If RouteOutput() caller specified an outgoing interface, that 
    // further constrains the selection of source address
    //
    std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin ();
    if (oif)
    {
      // Iterate to find an address on the oif device
      for (j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
        {
          Ipv4Address addr = j->second.GetLocal ();
          int32_t interface = m_ipv4->GetInterfaceForAddress (addr);
          if (oif == m_ipv4->GetNetDevice (static_cast<uint32_t> (interface)))
            {
              rt->SetSource (addr);
              break;
            }
        }
    }
    else
    {
      rt->SetSource (j->second.GetLocal ());
    }
    NS_ASSERT_MSG (rt->GetSource () != Ipv4Address (), "Valid DistUnivSched source address not found");
    rt->SetGateway (Ipv4Address ("127.0.0.1"));
    rt->SetOutputDevice (m_lo);
    return rt;
  }

  bool
  RoutingProtocol::IsMyOwnAddress (Ipv4Address src)
  {
    for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ipv4InterfaceAddress iface = j->second;
      if (src == iface.GetLocal ())
      {
        return true;
      }
    }
    return false;
  }

  Ptr<Socket>
  RoutingProtocol::FindSocketWithInterfaceAddress (Ipv4InterfaceAddress addr ) const
  {
    //NS_LOG_FUNCTION (this << addr);
    for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;
      if (iface == addr)
        return socket;
    }
    Ptr<Socket> socket;
    return socket;
  }

  // /**
  // FUNCTION: DataSent
  // LAYER   : NETWORK
  // PURPOSE : Called when a socket successfully sends data.
  // PARAMETERS:
  // +socket : Ptr<Socket> : socket data was sent from 
  // +amtDataSent : uint32_t : number of bytes sent
  // RETURN   ::void:NULL
  // **/ 
  void
  RoutingProtocol::DataSent( Ptr<Socket> socket, uint32_t amtDataSent )
  {
    if( DIST_UNIV_SCHED_SEND_DATA_PACKET_DEBUG )
    {
      std::cout<<"Node "<< getNodeId() << " in DataSent()...sent " << amtDataSent << " bytes of data out interface \n";
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
  RoutingProtocol::RecvPacket (Ptr<Socket> socket)
  {
    if( DIST_UNIV_SCHED_RECV_DIST_UNIV_SCHED_DEBUG )
    {
      std::cout<< "Node "<< getNodeId() << " in RecvPacket()\n";
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
    
    if( DIST_UNIV_SCHED_RECV_DIST_UNIV_SCHED_DEBUG )
    {
      std::cout<<"\treceived a packet from " << sender << " to " << receiver << "\n";
    }
        DistUnivSchedSnrTag snrTag(-1.0);
        if( packet->PeekPacketTag(snrTag) )
        {
          packet->RemovePacketTag(snrTag);
          if( DIST_UNIV_SCHED_RECV_DIST_UNIV_SCHED_DEBUG )
          {
            std::cout<< "\tNode " << getNodeId() << ": SNR of packet = " << snrTag.snr << "\n";
          }

          if( CALCULATE_RADIO_RANGES )
          {
            if( getNodeId() == 1 )
            {
              //std::cout<< "\tNode " << getNodeId() << ": SNR of packet = " << snrTag.snr << " at " << Simulator::Now().GetSeconds() << "\n";
              lastSnr = snrTag.snr;
            }
          }

          // TODO : Determine actual correlation between rates and SNR
          // For now:
          SetChannelRatesFromSnr( snrTag.snr, senderIndex, receiverIndex );
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
        if( DIST_UNIV_SCHED_RECV_DIST_UNIV_SCHED_DEBUG )
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
        if( DIST_UNIV_SCHED_RECV_DIST_UNIV_SCHED_DEBUG )
        {
          std::cout<< "\tDUS_CTRL Packet received (in RecvPacket)\n";
        }
        RecvControlInfoPacket( packet, sender );
        break;
      }
      case DUS_DATA:
      {
        if( DIST_UNIV_SCHED_RECV_DIST_UNIV_SCHED_DEBUG )
        {
          std::cout<< "\tDUS_DATA Packet received (in RecvPacket)\n";
        }
        RecvDataPacket ( packet, sender, receiver );
        break;
      }
      case DUS_DATA_ACK:
      {
        if( DIST_UNIV_SCHED_RECV_DIST_UNIV_SCHED_DEBUG )
        {
          std::cout<< "\tDUS_DATA_ACK Packet received (in RecvPacket)...type was in header\n";
        }
        RecvDataAckPacket ( packet, sender, receiver );
        break;
      }
      case OTHER:
      {
        if( DIST_UNIV_SCHED_RECV_DIST_UNIV_SCHED_DEBUG )
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

  bool 
  RoutingProtocol::IsPhyStateBusy()
  {
    int i = 1; // TODO : is this just good for single interface??

    Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();

    Ipv4InterfaceAddress iface = l3->GetAddress (i, 0);

    Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
     
    Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice> ();
    if (wifi == 0)
    {
      std::cout<<"ERROR:  GetObject<WifiNetDevice>() failed in GetPhyState()\n";
      //return false;
    }
    Ptr<WifiMac> mac = wifi->GetMac ();
    if (mac == 0)
    {
      std::cout<<"ERROR:  GetMac() failed in GetPhyState()\n";
      //return false;
    }
    
    Ptr<WifiPhy> phy = wifi->GetPhy();
    if (phy == 0)
    {
      std::cout<<"ERROR:  GetPhy() failed in GetPhyState()\n";
      //return false;
    }
   /* 
    if( phy->IsStateIdle() )
    {
      std::cout<<"\tIsStateIdle returns TRUE\n";
    }
    else
    {
      std::cout<<"\tIsStateIdle returns FALSE\n";
    }
    if( phy->IsStateBusy() )
    {
      std::cout<<"\tIsStateBusy returns TRUE\n";
    }
    else
    {
      std::cout<<"\tIsStateBusy returns FALSE\n";
    }
    */
    return phy->IsStateBusy();
  }

void RoutingProtocol::GetChannelRateFromCoordinates( double node_1x, double node_1y, double node_2x, double node_2y, double *channelRate, int *channelRateIndex )
{
	double nodeDistance, xDistance, yDistance;
	
	xDistance = abs((int)(node_1x - node_2x));
	yDistance = abs((int)(node_1y - node_2y));
	
	nodeDistance = sqrt( xDistance*xDistance + yDistance*yDistance );

  if( CALCULATE_RADIO_RANGES )
  {
    *channelRate = FAKE_CHANNEL_RATE;
    *channelRateIndex = 0;
    return;
  }
	
  if( DIST_UNIV_SCHED_RATES_FROM_COORDS_DEBUG )
  {
    printf( "In GetChannelRateFromCoordinates()...\n" );
    printf( "\tdistance between nodes = %f\n", nodeDistance );
  }

  if( nodeDistance > 174.0 )
  {
    *channelRate = 0.0; // rate = 0 if nodeDistance >  174m
    *channelRateIndex = 0;
  }
  else if( nodeDistance > 120.0 )
  {
    *channelRate = 1.0; // rate = 1 Mbps if 120m < nodeDistance < 174m
    *channelRateIndex = 1;
  }
  else if( nodeDistance > 85.0 )
  {
    *channelRate = 2.0; // rate = 2 Mbps if 85m < nodeDistance < 120m
    *channelRateIndex = 2;
  }
  else if( nodeDistance > 66.0 )
  {
    *channelRate = 5.5; // rate = 5.5 Mbps if 66m < nodeDistance < 85m
    *channelRateIndex = 3;
  }
  else
  {
    *channelRate = 11.0; // rate = 11 Mbps if nodeDistance < 66m
    *channelRateIndex = 4;
  }
}

  void
  RoutingProtocol::SetChannelRatesFromSnr( double snr, int senderIndex, int receiverIndex )
  {
    if( snr > 10.25 )
    {
      channelRates[senderIndex][receiverIndex] = 11.0;
      channelRates[receiverIndex][senderIndex] = 11.0;
      channelRateIndex[senderIndex][receiverIndex] = 4;
      channelRateIndex[receiverIndex][senderIndex] = 4;
    } 
    else if( snr > 5.02 )
    {
      channelRates[senderIndex][receiverIndex] = 5.5;
      channelRates[receiverIndex][senderIndex] = 5.5;
      channelRateIndex[senderIndex][receiverIndex] = 3;
      channelRateIndex[receiverIndex][senderIndex] = 3;
    } 
    else if( snr > 1.75 )
    {
      channelRates[senderIndex][receiverIndex] = 2.0;
      channelRates[receiverIndex][senderIndex] = 2.0;
      channelRateIndex[senderIndex][receiverIndex] = 2;
      channelRateIndex[receiverIndex][senderIndex] = 2;
    } 
    else if( snr > 0.6 )
    {
      channelRates[senderIndex][receiverIndex] = 1.0;
      channelRates[receiverIndex][senderIndex] = 1.0;
      channelRateIndex[senderIndex][receiverIndex] = 1;
      channelRateIndex[receiverIndex][senderIndex] = 1;
    } 
    else
    {
      channelRates[senderIndex][receiverIndex] = 0.0;
      channelRates[receiverIndex][senderIndex] = 0.0;
      channelRateIndex[senderIndex][receiverIndex] = 0;
      channelRateIndex[receiverIndex][senderIndex] = 0;
    } 
  }

	
/*
// Get channel rate using the SNR obtained from an incoming packet
//		*Relation between available rate and SNR is manually determined using
//		independent experimental runs and the Qualnet radioRange program
void GetChannelRateFromSnr( double snr, double *channelRate, int *channelRateIndex )
{
	if( snr < 10.2 )
	{
		*channelRate = 0; // rate = 0 if snr < 10.2
		*channelRateIndex = -1;
	}
	else if( snr < 14.8 )
	{
		*channelRate = 1000000/1000000.0; // rate = 1 Mbps if 10.2 < snr < 14.8
		*channelRateIndex = 0;
	}
	else if( snr < 15.6 )
	{
		*channelRate = 2000000/1000000.0; // rate = 2 Mbps if 14.8 < snr < 15.6
		*channelRateIndex = 1;
	}
	else if( snr < 18.8 )
	{
		*channelRate = 5500000/1000000.0; // rate = 5.5 Mbps if 15.6 < snr < 18.8
		*channelRateIndex = 2;
	}
	else
	{
		*channelRate = 11000000/1000000.0; // rate = 11 Mbps if snr > 18.8
		*channelRateIndex = 3;
	}
	
	return;
}
*/

/*
// Get channel rate index from the actual channel rate
//		*Mapping from channel rates to indices are fixed for 
//			802.11 radio with 1, 2, 5.5, and 11 Mbps channels
int GetIndexFromChannelRate( double channelRate )
{
	if( channelRate == 0.0 )
	{
		return -1;
	}
	if( channelRate == 1.0 )
	{
		return 0;
	}
	if( channelRate == 2.0 )
	{
		return 1;
	}
	if( channelRate == 5.5 )
	{
		return 2;
	}
	if( channelRate == 11.0 )
	{
		return 3;
	}
	
	return -2;
}
*/

/*
// Get channel rate from index value
//		*Mapping from channel rates to indices are fixed for 
//			802.11 radio with 1, 2, 5.5, and 11 Mbps channels
double GetChannelRateFromIndex( int rateIndex )
{
	switch( rateIndex )
	{
		case -1:
		{
			return 0.0;
		}
		case 0:
		{
			return 1.0;
		}
		case 1:
		{
			return 2.0;
		}
		case 2:
		{
			return 5.5;
		}
		case 3:
		{
			return 11.0;
		}
		default:
		{
			return -1;
		}
	}
	
	return -1;
}
*/

  void 
  RoutingProtocol::PrintStats()
  {
    
    char buf[1024];
    FILE *statsFd;

    int i, j;
    double avgTotalOccupancy = 0.0;
    double avgDeliveredThroughput = 0.0;
    double avgInputRate = 0.0;
    time( &simEndTime );
    double runTime = difftime( simEndTime, simStartTime );

    if( DIST_UNIV_SCHED_PRINT_STATS_DEBUG )
    {
      printf("\nNode %i in DistUnivSchedPrintStats()\n", nodeId);			

      printf("\nNumber of Data Packets from Application = %i\n", numPacketsFromApplication);

      printf("\n\tNumber of Control Packets Sent = %i\n", numControlPacketsSent);
      printf("\tNumber of Control Packets Rcvd = %i\n", numControlPacketsRcvd);
      printf("\tNumber of Data Packet Acks Sent = %i\n", numDataAcksSent);
      printf("\tNumber of Data Packet Acks Rcvd = %i\n", numDataAcksRcvd);
      printf("\tNumber of Packet Collisions = %i\n", numCollisions);
      //printf("\tNumber of Incorrect Queues Values = %i\n", numIncorrectQueues);
      //printf("\tNumber of Incorrect Rate Values = %i\n", numIncorrectRates);
	
      for( i = 0; i < numNodes; i++ )
      {
        printf("\tdata packets sent to %i: %i\n", i, dataPacketsSent[i]);
      }
      printf("\n");
      for( i = 0; i < numNodes; i++ )
      {
        printf("\tdata packets rcvd from %i: %i\n", i, dataPacketsRcvd[i]);
      }
      printf("\tRun Time: %f\n", runTime);
    }
  	
    for( i = 0; i < numCommodities; i++ )
    {
      if( DIST_UNIV_SCHED_PRINT_STATS_DEBUG ) 
      {    
        printf("node %i:  queueLengthSum[%i] = %f\n", nodeId, i, stats->getQueueLengthSum(i));
        //printf("node %i:  finalQueueLength[%i] = %i\n", node->nodeId, i, queues[i].backlog);
      }
      avgTotalOccupancy += stats->getQueueLengthSum(i)/simulationTime.GetSeconds();
	  	
      avgDeliveredThroughput += stats->getNumPacketsReachDest(i)/simulationTime.GetSeconds();

      packetsDropped += (int)queues[i].GetTotalDroppedPackets();
      if( DIST_UNIV_SCHED_PRINT_STATS_DEBUG ) 
      {    
        printf( "node %i:  queues[%i].GetTotalDroppedPackets() = %i\n", nodeId, i, (int)queues[i].GetTotalDroppedPackets() );
        //printf("node %i:  finalQueueLength[%i] = %i\n", node->nodeId, i, queues[i].backlog);
      }
    }
    
    avgInputRate = numPacketsFromApplication/simulationTime.GetSeconds();

    int globOutput = 0;
    int lineNet = 0;
    int bruteForce = 0;
    if ( GLOBAL_KNOWLEDGE )
    {
      globOutput = 1;
      sprintf(buf, "%sdistUnivSchedStats_Global.csv", getDataFilePath().c_str() );
      statsFd = fopen(buf, "a");
    }
    else if( isLINE_NETWORK() )
    {
      lineNet = 1;
      sprintf(buf, "%sdistUnivSchedStats_LineNet.csv", getDataFilePath().c_str() );
      statsFd = fopen(buf, "a");
    }
    else if( isBRUTE_FORCE() )
    {
      bruteForce = 1;
      sprintf(buf, "%sdistUnivSchedStats_BruteForce.csv", getDataFilePath().c_str() );
      statsFd = fopen(buf, "a");
    }
    else
    {
      sprintf(buf, "%sdistUnivSchedStats_DistAlg.csv", getDataFilePath().c_str() );
      statsFd = fopen(buf, "a");
    }
  


    if( getBatteryPowerLevel() > 0.0 )
    {
      setNodeLifetime( Simulator::Now() );
    }
	   
    // get node positions (this will only be useful if nodes are static
    Ptr<MobilityModel> mobility = NodeList::GetNode((uint32_t)getNodeId())->GetObject<MobilityModel> ();
    Vector pos = mobility->GetPosition();
	  
    if( statsFd == NULL )
    {
      printf( "ERROR:  Could not open stats file named '%s'.  Not printing any stats to output file.\n", buf );
      return;
    }

                   //    1    2   3     4     5     6     7   8   8a  8b  9  10    11  12    13    14    15  16  17  18    19  20  21  22  23  24    25    26    27    28    29    30    31
    fprintf(statsFd, "  %i,  %i, %i, %.2f, %.2f, %.2f, %.2f, %i, %i, %i, %.3f, %i, %.1f, %i, %.3f, %.1f, %.1f, %i, %i, %i, %.3f, %i, %i, %i, %i, %i, %.3f, %.2f, %.2f, %.1f, %.1f, %.1f, %.1f\n", 
	getNodeId(), // 1 i
        getNumNodes(), // 2 i
        getNumRun(), // 3 i
        pos.x, // 4 f 
        pos.y, // 5 f 
        getNodeRWPause(), // f 6
        getNodeRWSpeed(), // f 7
        globOutput, // 8 i
        lineNet, // 8a i
        bruteForce, // 8b i
	avgInputRate, // 9 f 
	qBits, // 10 i
        simulationTime.GetSeconds(), // 11 f
	getNumTimeSlots(), // 12 i
	controlInfoExchangeTime.GetSeconds(), // 13 f
	MilliSeconds(maxBackoffWindow).GetSeconds(), // 14 f
	timeSlotDuration.GetSeconds(), // 15 f
	numControlPacketsSent, //  i 16
	numControlPacketsRcvd, //  i 17
	numWaitForAckTimeouts, //  i 18 
	stats->getNumTimeSlotsRcvdAck()/(double)(numTimeSlots), //  f 19
	getNumCollisions(), //  i  20
        getNumCtrlPcktCollisions(), // i 21
        getNumDataPcktCollisions(), // i 22
        getNumDataAckCollisions(), // i 23
        getPacketsDropped(), //  i 24
        avgTotalOccupancy, // f 25
        avgDeliveredThroughput, // f 26
        avgDeliveredThroughput*(numBitsPerPacket/8.0), // f 27
        getNodeLifetime().GetSeconds(), // f 28
        getInitBatteryPowerLevel(), // f 29 
        getPowerUsed(), // f 30
        runTime // f 31
        );
     
        //stats.numTimeSlotsExchangedMatchesGlobal/(double)(numTimeSlots-1), // 17
        //1.0 - (double)numIncorrectQueues/(double)(numTimeSlots*numNodes*numCommodities), // 18
        //1.0 - (double)numIncorrectRates/(double)(numTimeSlots*numNodes*numNodes), // 19
        //1.0 - stats.numTimeSlotsCorrChosenRate/(double)numTimeSlots, // 20
        //stats.numTimeSlotsIncorrTrx/(double)(numTimeSlots-1), // 21
        //stats.numTimeSlotsIncorrSilent/(double)(numTimeSlots-1) ); // 22 17
				
        if( DIST_UNIV_SCHED_PRINT_STATS_DEBUG ) 
        {    
          printf( "AvgInputRate = %f\n", avgInputRate );
		
          //printf( "numIncorrectQueues = %i, numIncorrectRates = %i\n", numIncorrectQueues, numIncorrectRates );
		
          printf( "Number of time slots ACK not received: %f\n", stats->getNumTimeSlotsAckNotRcvd() );
          printf( "Number of time slots ACK received: %f\n", stats->getNumTimeSlotsRcvdAck() );
          printf( "Number of time slots Exchanged info matches global: %f\n", stats->getNumTimeSlotsExchangedMatchesGlobal() );
          printf( "Number of time slots Exchanged info does not match global: %f\n", stats->getNumTimeSlotsExchangedGlobalDiff() );
          printf( "Number of time slots Backlogs didn't match: %f\n", stats->getNumTimeSlotsBacklogsDiff() );
          printf( "Number of time slots Rates didn't match: %f\n", stats->getNumTimeSlotsRatesDiff() );
          printf( "Number of Time Slots Incorrectly Trx = %f\n", stats->getNumTimeSlotsIncorrTrx() );
          printf( "Number of Time Slots Incorrectly Silent = %f\n", stats->getNumTimeSlotsIncorrSilent() );
        }
		
      fclose( statsFd );
       
    if( TIME_STATS )
    {
      sprintf(buf, "distUnivSchedQueues.csv");
      FILE *queuesFd = fopen(buf, "a");

      for( i = 0; i < simulationTime.GetSeconds(); i++ )
      {
        fprintf( queuesFd, "%i, %i, ", getNodeId(), i+1 );
        for( j = 0; j < numCommodities; j++ )
        {
          fprintf( queuesFd, "%i, ", stats->getQueueLength( i, j ) );
        }
        fprintf( queuesFd, "\n" );
      }

      fclose( queuesFd );
      
      sprintf(buf, "distUnivSchedThroughput.csv");
      FILE *TputFd = fopen(buf, "a");

      for( i = 0; i < simulationTime.GetSeconds(); i++ )
      {
        fprintf( TputFd, "%i, %i, ", getNodeId(), i+1 );
        for( j = 0; j < numCommodities; j++ )
        {
          fprintf( TputFd, "%i, ", stats->getThroughput( i, j ) );
        }
        fprintf( TputFd, "\n" );
      }

      fclose( TputFd );
    }

    if( OUTPUT_RATES && getNodeId() == 0 )
    {
      fclose( channelRatesFd );
    }

  }

  void
  RoutingProtocol::VaryQueues()
  {
    // TODO : Implement varying queue and/or channel rates if desired
    // Vary Queue and/or Channel Rate values using max and probability values set in config file (if not 0)
    int i, j;
        if( DIST_UNIV_SCHED_VARY_QUEUE_INFO_DEBUG )
        {
            printf( "Node %i:    Varying Queue Backlogs:\n", getNodeId() );
        }
        for( i = 0; i < getNumNodes(); i++ )
        {
            for( j = 0; j < getNumCommodities(); j++ )
            {
                if( i != getNodeId() && i != j) // don't want to change own queue values...these will always be correct
                {                                 // and don't want to change values when i == j...these are always zero

                    if( DIST_UNIV_SCHED_VARY_QUEUE_INFO_DEBUG )
                    {
                        printf( "\tInitially: [%i][%i] = %i,", i, j, otherBacklogs[i][j] );
                    }
                    if( true ) // TODO : RANDOMErand( queueChangeProbSeed ) <= probQueueChange )
                    {
                        // determine by how much to change value
                        int numChange = 0; // TODO :  (RANDOMNrand( queueMaxChangeSeed )%maxQueueChange)+1;

                        // now determine if the change is added or subtracted and perform action
                        if( true ) // TODO : RANDOMErand( queueChangePosNegSeed ) <= 0.5 || otherBacklogs[i][j] == 0)
                        {
                            //add
                          //temp:
                          otherBacklogs[i][j] = 0;
                        /*
#ifdef DOUBLE_BUFFER_SIZE
                            otherBacklogs[i][j] = min( MAX_DIST_UNIV_SCHED_BUFFER_SIZE*2, otherBacklogs[i][j]+numChange );
#else
                            otherBacklogs[i][j] = min( MAX_DIST_UNIV_SCHED_BUFFER_SIZE, otherBacklogs[i][j]+numChange );
#endif
*/
                        }
                        else
                        {
                            //subtract, being sure not to go below zero
                            otherBacklogs[i][j] = max( 0, otherBacklogs[i][j]-numChange );
                        }
                    }
                    if( DIST_UNIV_SCHED_VARY_QUEUE_INFO_DEBUG )
                    {
                        printf( "\tAfter Varying: [%i][%i] = %i\n", i, j, otherBacklogs[i][j] );
                    }
                }
            }
        }
  }



  void
  RoutingProtocol::VaryRates()
  {
    int i, j;
    // Vary Channel Rate values using max and probability values set in config file (if not 0)
        for( i = 0; i < getNumNodes(); i++ )
        {
            for( j = 0; j < getNumNodes(); j++ )
            {
                if( i != getNodeId() && i != j ) // don't want to change own rate values...these will always be correct
                {                                 //    also don't want to change values where i==j, this queue is always zero
                    if( true )// RANDOMErand( rateChangeProbSeed ) <= probRateChange ) // TODO : Fix this rand number
                    { 
                        //int rateIndex = GetIndexFromChannelRate( channelRates[i][j] );
                      int rateIndex = 0;
                        
                        if( rateIndex == -2 )
                        {
                            //sprintf(buf, "ERROR:    Node %i:    In VaryRates() varying channel rate...GetIndexFromChannelRate returned -2.\n", getNodeId());
                       //     ERROR_Assert( false, buf );
                        }
                        
                        //if( DIST_UNIV_SCHED_VARY_RATE_INFO_DEBUG )
                        //{
                        //    printf( "Node %i:    In VaryRates()...channel rate from %i to %i = %f...GetIndexFromChannelRate returns %i\n",
                        //             getNodeId(), i+1, j+1, channelRates[i][j], rateIndex );
                        //}
                        
                        // determine by how much to change value
                        int numChange = 0; // TODO : Fix this rand num : (RANDOMNrand( rateMaxChangeSeed )%(maxRateChange)) + 1;

                        if( rateIndex == 3 ) // if already at max, have to subtract
                        {
                            rateIndex = max( -1, rateIndex-numChange );
                            if( DIST_UNIV_SCHED_VARY_RATE_INFO_DEBUG )
                            {
                                printf( "\tsubtracted %i to rate...now rate index from %i to %i = %i\n",
                                         numChange, i+1, j+1, rateIndex );
                            }
                        }
                        else if( rateIndex == -1 ) // if already at min, have to add
                        {
                            //add, being sure not to go above 3 (rate of 11 Mbps)
                            rateIndex = min( 3, rateIndex+numChange );
                    
                            if( DIST_UNIV_SCHED_VARY_RATE_INFO_DEBUG )
                            {
                                printf( "\tadded %i from rate...now rate index from %i to %i = %i\n",
                                         numChange, i+1, j+1, rateIndex );
                            }
                        }
                        else if( true ) // TODO : Fix this rand num : RANDOMErand( queueChangePosNegSeed ) <= 0.5 )    // if not at max or min, determine if change is added or subtracted randomly
                        {
                            //add, being sure not to go above 3 (rate of 11 Mbps)
                            rateIndex = min( 3, rateIndex+numChange );
                            
                            if( DIST_UNIV_SCHED_VARY_RATE_INFO_DEBUG )
                            {
                                printf( "\tadded %i to rate...now rate index from %i to %i = %i\n",
                                         numChange, i+1, j+1, rateIndex );
                            }
                        }
                        else
                        {
                            //subtract, being sure not to go below -1 (rate of 0 Mbps)
                            rateIndex = max( -1, rateIndex-numChange );
                            
                            if( DIST_UNIV_SCHED_VARY_RATE_INFO_DEBUG )
                            {
                                printf( "\tsubtracted %i from rate...now rate index from %i to %i = %i\n",
                                         numChange, i+1, j+1, rateIndex );
                            }
                        }
                        
                        channelRates[i][j] = 0;// TODO : GetChannelRateFromIndex( rateIndex );
                        
                        if( DIST_UNIV_SCHED_VARY_RATE_INFO_DEBUG )
                        {
                            printf( "\tnew channel rate from %i to %i = %f\n", i+1, j+1, channelRates[i][j] );
                        }
                        
                        if( channelRates[i][j] == -1 )
                        {
                            //sprintf(buf, "ERROR:    Node %i:    In VaryRates() varying channel rate...GetChannelRateFromIndex returned -1.\n", getNodeId());
                            //ERROR_Assert( false, buf );
                        }
                    }
                }
            }
    }
    // Done varying queue and/or channel rates
    // END TODO
  }

  void 
  RoutingProtocol::PrintArray( int *array, int length )
  {
    for( int i = 0; i < length; i++ )
    {
      std::cout<< array[i] << ", ";
    }
    std::cout<<"\n";
  }

NS_OBJECT_ENSURE_REGISTERED (TypeHeader);

TypeHeader::TypeHeader (MessageType t = OTHER) :
  m_type (t), m_valid (true)
{
}

TypeId
TypeHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::dus::TypeHeader")
    .SetParent<Header> ()
    .AddConstructor<TypeHeader> ()
  ;
  return tid;
}

TypeId
TypeHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
TypeHeader::GetSerializedSize () const
{
  return 1;
}

void
TypeHeader::Serialize (Buffer::Iterator i) const
{
  i.WriteU8 ((uint8_t) m_type);
}

uint32_t
TypeHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  uint8_t type = i.ReadU8 ();
  m_valid = true;
  switch (type)
    {
    case DUS_CTRL:
    case DUS_DATA:
      {
        m_type = (MessageType) type;
        break;
      }
    default:
      m_valid = false;
    }
  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
TypeHeader::Print (std::ostream &os) const
{
  switch (m_type)
    {
    case DUS_CTRL:
      {
        os << "DUS_CTRL";
        break;
      }
    case DUS_DATA:
      {
        os << "DUS_DATA";
        break;
      }
    case DUS_DATA_ACK:
      {
        os << "DUS_DATA_ACK";
        break;
      }
    case OTHER:
      {
        os << "OTHER";
        break;
      }
    default:
      os << "UNKNOWN_TYPE";
    }
}

bool
TypeHeader::operator== (TypeHeader const & o) const
{
  return (m_type == o.m_type && m_valid == o.m_valid);
}

std::ostream &
operator<< (std::ostream & os, TypeHeader const & h)
{
  h.Print (os);
  return os;
}

}
}
