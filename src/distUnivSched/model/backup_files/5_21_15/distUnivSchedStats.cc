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

#include "distUnivSchedStats.h"

namespace ns3
{ 
NS_OBJECT_ENSURE_REGISTERED (DistUnivSchedStats);

  TypeId
  DistUnivSchedStats::GetTypeId (void)
  {
   static TypeId tid = TypeId ("ns3::DistUnivSchedStats")
    .SetParent<Object> ()
    //.AddConstructor<DistUnivSchedStats> (int, int, int)
    ;  
    return tid;
  }
  //-----------------------------------------------------------------------------
  DistUnivSchedStats::DistUnivSchedStats ( int numSeconds, int numCommodities, bool TIME_STATS )
  {
    int i, j;
    if( DIST_UNIV_SCHED_STATS_DEBUG )
    {
      std::cout<<"In DistUnivSchedStats constructor\n";
    }

    // Init stats.

    if( TIME_STATS )
    {
      packetsReceived = new double* [numSeconds];
      packetsTrx = new int* [numSeconds];
      throughput = new int* [numSeconds];
      avgThroughput = new double* [numSeconds];
      queueLength = new int* [numSeconds];
      avgQueueLength = new double* [numSeconds];
    }
    throughputSum = new double [numCommodities];
    queueLengthSum = new double [numCommodities];
    numPacketsReachDest = new double [numCommodities];
    numPacketsReachDestThisSecond = new double [numCommodities];
    numTimeSlotsRcvdAck = 0.0;
    numTimeSlotsAckNotRcvd = 0.0;
    numTimeSlotsExchangedMatchesGlobal = 0.0;
    numTimeSlotsExchangedGlobalDiff = 0.0;
    numTimeSlotsBacklogsDiff = 0.0;
    numTimeSlotsRatesDiff = 0.0;
    numTimeSlotsCorrChosenRate = 0.0;
    numTimeSlotsIncorrTrx = 0.0;
    numTimeSlotsIncorrSilent = 0.0;
    if( TIME_STATS )
    {
      for( i = 0; i < numSeconds; i++ )
      {
        packetsReceived[i] = new double [numCommodities];
        packetsTrx[i] = new int[numCommodities];
        throughput[i] = new int [numCommodities];
        avgThroughput[i] = new double [numCommodities];
        queueLength[i] = new int[numCommodities];
        avgQueueLength[i] = new double[numCommodities];
        for( j = 0; j < numCommodities; j++ )
        {
          packetsReceived[i][j] = 0.0;
          packetsTrx[i][j] = 0;
          throughput[i][j] = 0;
          avgThroughput[i][j] = 0.0;
          queueLength[i][j] = 0;
          avgQueueLength[i][j] = 0.0;
        }
      }
    }
    for( i = 0; i < numCommodities; i++ )
    {
      queueLengthSum[i] = 0.0;
      throughputSum[i] = 0.0;
      numPacketsReachDest[i] = 0.0;
      numPacketsReachDestThisSecond[i] = 0.0;
    }
    
    
    if( DIST_UNIV_SCHED_STATS_DEBUG )
    {
      std::cout<< "Exiting DistUnivSchedStats constructor\n";
    }
  }

  

  DistUnivSchedStats::~DistUnivSchedStats ()
  {
    if( DIST_UNIV_SCHED_STATS_DEBUG )
    {
      std::cout<<"In DistUnivSchedStats destructor\n";
    }
  }

  // start GET FUNCTIONS
  double 
  DistUnivSchedStats::getPacketsReceived (int i, int j) // [time slot number][commodity]
  {
    return packetsReceived[i][j];
  }
  int DistUnivSchedStats::getPacketsTrx (int i, int j) // [time slot number][commodity]
  {
    return packetsTrx[i][j];
  }
  int DistUnivSchedStats::getThroughput(int i, int j) // [time slot number][commodity]
  {
    return throughput[i][j];
  }
  double DistUnivSchedStats::getAvgThroughput(int i, int j) // [time slot number][commodity]
  {
    return avgThroughput[i][j];
  }
  double DistUnivSchedStats::getThroughputSum (int i) // [commodity]
  {
    return throughputSum[i];
  }
  int DistUnivSchedStats::getQueueLength(int i, int j) // [time slot number][commodity]
  {
    return queueLength[i][j];
  }
  double DistUnivSchedStats::getAvgQueueLength(int i, int j) // [time slot number][commodity]
  {
    return avgQueueLength[i][j];
  }
  double DistUnivSchedStats::getQueueLengthSum (int i) // [commodity]
  {
    return queueLengthSum[i];
  }
  double DistUnivSchedStats::getNumPacketsReachDest (int i) // [commodity]
  {
    return numPacketsReachDest[i];
  }
  double DistUnivSchedStats::getNumPacketsReachDestThisSecond (int i) // [commodity]
  {
    return numPacketsReachDestThisSecond[i];
  }
  double DistUnivSchedStats::getNumTimeSlotsRcvdAck ()
  {
    return numTimeSlotsRcvdAck;
  }
  double DistUnivSchedStats::getNumTimeSlotsAckNotRcvd ()
  {
    return numTimeSlotsAckNotRcvd;
  }
  double DistUnivSchedStats::getNumTimeSlotsExchangedMatchesGlobal ()
  {
    return numTimeSlotsExchangedMatchesGlobal;
  }
  double DistUnivSchedStats::getNumTimeSlotsExchangedGlobalDiff ()
  {
    return numTimeSlotsExchangedGlobalDiff;
  }
  double DistUnivSchedStats::getNumTimeSlotsBacklogsDiff ()
  {
    return numTimeSlotsBacklogsDiff;
  }
  double DistUnivSchedStats::getNumTimeSlotsRatesDiff ()
  {
    return numTimeSlotsRatesDiff;
  }
  double DistUnivSchedStats::getNumTimeSlotsCorrChosenRate ()
  {
    return numTimeSlotsCorrChosenRate;
  }
  double DistUnivSchedStats::getNumTimeSlotsIncorrTrx ()
  {
    return numTimeSlotsIncorrTrx;
  }
  double DistUnivSchedStats::getNumTimeSlotsIncorrSilent ()
  {
    return numTimeSlotsIncorrSilent;
  }
  
  int DistUnivSchedStats::getCurrentQueueLength (int i) // may need to change
  {
    return currentQueueLength[i];
  }
  // END GET FUNCTIONS
  
  // START SET FUNCTIONS
  void 
  DistUnivSchedStats::setPacketsReceived (int i, int j, double newVal) // [time slot number][commodity]
  {
     packetsReceived[i][j] = newVal;
  }
  void DistUnivSchedStats::setPacketsTrx (int i, int j, int newVal) // [time slot number][commodity]
  {
     packetsTrx[i][j] = newVal;
  }
  void DistUnivSchedStats::setThroughput(int i, int j, int newVal) // [time slot number][commodity]
  {
     throughput[i][j] = newVal;
  }
  void DistUnivSchedStats::setAvgThroughput(int i, int j, double newVal) // [time slot number][commodity]
  {
     avgThroughput[i][j] = newVal;
  }
  void DistUnivSchedStats::setThroughputSum (int i, double newVal) // [commodity]
  {
     throughputSum[i] = newVal;
  }
  void DistUnivSchedStats::setQueueLength(int i, int j, int newVal) // [time slot number][commodity]
  {
     queueLength[i][j] = newVal;
  }
  void DistUnivSchedStats::setAvgQueueLength(int i, int j, double newVal) // [time slot number][commodity]
  {
     avgQueueLength[i][j] = newVal;
  }
  void DistUnivSchedStats::setQueueLengthSum (int i, double newVal) // [commodity]
  {
     queueLengthSum[i] = newVal;
  }
  void DistUnivSchedStats::setNumPacketsReachDest (int i, double newVal) // [commodity]
  {
     numPacketsReachDest[i] = newVal;
  }
  void DistUnivSchedStats::setNumPacketsReachDestThisSecond (int i, double newVal) // [commodity]
  {
     numPacketsReachDestThisSecond[i] = newVal;
  }
  void DistUnivSchedStats::setNumTimeSlotsRcvdAck (double newVal)
  {
     numTimeSlotsRcvdAck = newVal;
  }
  void DistUnivSchedStats::setNumTimeSlotsAckNotRcvd (double newVal)
  {
     numTimeSlotsAckNotRcvd = newVal;
  }
  void DistUnivSchedStats::setNumTimeSlotsExchangedMatchesGlobal (double newVal)
  {
     numTimeSlotsExchangedMatchesGlobal = newVal;
  }
  void DistUnivSchedStats::setNumTimeSlotsExchangedGlobalDiff (double newVal)
  {
     numTimeSlotsExchangedGlobalDiff = newVal;
  }
  void DistUnivSchedStats::setNumTimeSlotsBacklogsDiff (double newVal)
  {
     numTimeSlotsBacklogsDiff = newVal;
  }
  void DistUnivSchedStats::setNumTimeSlotsRatesDiff (double newVal)
  {
     numTimeSlotsRatesDiff = newVal;
  }
  void DistUnivSchedStats::setNumTimeSlotsCorrChosenRate (double newVal)
  {
     numTimeSlotsCorrChosenRate = newVal;
  }
  void DistUnivSchedStats::setNumTimeSlotsIncorrTrx (double newVal)
  {
     numTimeSlotsIncorrTrx = newVal;
  }
  void DistUnivSchedStats::setNumTimeSlotsIncorrSilent (double newVal)
  {
     numTimeSlotsIncorrSilent = newVal;
  }
  
  void DistUnivSchedStats::setCurrentQueueLength (int i, int newVal) // may need to change
  {
     currentQueueLength[i] = newVal;
  }
  // END SET FUNCTIONS
}

