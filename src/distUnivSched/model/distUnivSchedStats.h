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
#ifndef __DISTUNIVSCHEDSTATS_H__
#define __DISTUNIVSCHEDSTATS_H__

#define MAX_NUM_COMMODITIES 100

#define DIST_UNIV_SCHED_STATS_DEBUG 0

#include "ns3/object.h"

namespace ns3 {

/* ... */


  /**
   * \ingroup distUnivSchedStats
   * 
   * \brief DIST_UNIV_SCHED routing protocol
   */
  class DistUnivSchedStats : public Object
  {
  public:
    static TypeId GetTypeId (void);
    
    // Constructor
    DistUnivSchedStats ( int num_time_slots, int num_commoditites, bool TIME_STATS );
    
    // Destructor
    ~DistUnivSchedStats ();
   
   // START GET FUNCTIONS
    double getPacketsReceived (int, int); // [time slot number][session]
    int getPacketsTrx (int, int); // [time slot number][commodity]
    int getThroughput(int, int); // [time slot number][session]
    double getAvgThroughput(int, int); // [time slot number][session]
    double getThroughputSum (int); // [commodity]
    int getQueueLength(int, int); // [time slot number][commodity]
    double getAvgQueueLength(int, int); // [time slot number][commodity]
    double getQueueLengthSum (int); // [commodity]
    double getNumPacketsReachDest (int); // [commodity]
    double getNumPacketsReachDestThisSecond (int); // [commodity]
    double getNumTimeSlotsRcvdAck ();
    double getNumTimeSlotsAckNotRcvd ();
    double getNumTimeSlotsExchangedMatchesGlobal ();
    double getNumTimeSlotsExchangedGlobalDiff ();
    double getNumTimeSlotsBacklogsDiff ();
    double getNumTimeSlotsRatesDiff ();
    double getNumTimeSlotsCorrChosenRate ();
    double getNumTimeSlotsIncorrTrx ();
    double getNumTimeSlotsIncorrSilent ();
    
    int getCurrentQueueLength (int); // may need to change
    // END GET FUNCITONS

    // START SET FUNCTIONS
    void setPacketsReceived (int i, int j, double newVal); // [time slot number][session]
    void setPacketsTrx (int i, int j, int newVal); // [time slot number][commodity]
    void setThroughput(int i, int j, int newVal); // [time slot number][session]
    void setAvgThroughput(int i, int j, double newVal); // [time slot number][session]
    void setThroughputSum (int i, double newVal); // [commodity]
    void setQueueLength(int i, int j, int newVal); // [time slot number][commodity]
    void setAvgQueueLength(int i, int j, double newVal); // [time slot number][commodity]
    void setQueueLengthSum (int i, double newVal); // [commodity]
    void setNumPacketsReachDest (int i, double newVal); // [commodity]
    void setNumPacketsReachDestThisSecond (int i, double newVal); // [commodity]
    void setNumTimeSlotsRcvdAck (double newVal);
    void setNumTimeSlotsAckNotRcvd (double newVal);
    void setNumTimeSlotsExchangedMatchesGlobal (double newVal);
    void setNumTimeSlotsExchangedGlobalDiff (double newVal);
    void setNumTimeSlotsBacklogsDiff (double newVal);
    void setNumTimeSlotsRatesDiff (double newVal);
    void setNumTimeSlotsCorrChosenRate (double newVal);
    void setNumTimeSlotsIncorrTrx (double newVal);
    void setNumTimeSlotsIncorrSilent (double newVal);
    
    void setCurrentQueueLength (int i, int newVal); // may need to change
    // END SET FUNCTIONS

  private:
    ///\name Protocol parameters.
    //\{
    int **packetsTrx; // [time slot number][commodity]
    int **throughput; // [time slot number][session]
    int **queueLength; // [time slot number][commodity]
    double **avgThroughput; // [time slot number][session]
    double *throughputSum; // [commodity]
    double **packetsReceived; // [time slot number][session]
    double **avgQueueLength; // [time slot number][commodity]
    double *queueLengthSum; // [commodity]
    double *numPacketsReachDest; // [commodity]
    double *numPacketsReachDestThisSecond; // [commodity]
    double numTimeSlotsRcvdAck;
    double numTimeSlotsAckNotRcvd;
    double numTimeSlotsExchangedMatchesGlobal;
    double numTimeSlotsExchangedGlobalDiff;
    double numTimeSlotsBacklogsDiff;
    double numTimeSlotsRatesDiff;
    double numTimeSlotsCorrChosenRate;
    double numTimeSlotsIncorrTrx;
    double numTimeSlotsIncorrSilent;
    
    int currentQueueLength[MAX_NUM_COMMODITIES]; // may need to change
    
    
  };
}
  
#endif /* __DISTUNIVSCHEDSTATS_H__ */

