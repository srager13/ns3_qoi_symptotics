#!/bin/bash

if [ $# -lt 28 ]
then
  echo "Not enough arguments passed into $0...exiting"
  exit
else
  inputRate=$1
  numRuns=$2
  runTime=$3
  numNodes=$4
  timeSlotDuration=$5
  timeBudget=$6
  powBudgStart=$7
  powBudgEnd=$8
  powBudgIncrement=$9
  initBatteryPower=${10}
  nodeSpeed=${11}
  maxPause=${12}
  minItemCovDist=${13}
  maxItemCovDist=${14}
  fixedPacketSendSize=${15}
  areaWidth=${16}
  areaLength=${17}
  longTermAvg=${18}
  oneShot=${19}
  randChoice=${20}
  approxOneShot=${21}
  approxGreedyVQ=${22}
  V=${23}
  cacheProblem=${24}
  numSlotsPerFrame=${25}
  singleHop=${26}
  singleItem=${27}
	randChoice=${28}
	futureKnowledge=${29}
fi

i=$powBudgStart # i is 100*power budget
while [ $i -le $powBudgEnd ]; do
  powerBudgetTemp=$i$(echo ".0/1000.0")
  powerBudget=$(echo "scale=3; "$powerBudgetTemp | bc -q)
  j=1
  while [ $j -le $numRuns ]; do
    qsub -v inputRate=${inputRate},numRuns=${numRuns},runTime=${runTime},numNodes=${numNodes},timeSlotDuration=${timeSlotDuration},timeBudget=${timeBudget},powerBudget=${powerBudget},initBatteryPower=${initBatteryPower},runSeed=${j},nodeSpeed=${nodeSpeed},maxPause=${maxPause},minItemCovDist=${minItemCovDist},maxItemCovDist=${maxItemCovDist},fixedPacketSendSize=${fixedPacketSendSize},areaWidth=${areaWidth},areaLength=${areaLength},longTermAvg=${longTermAvg},oneShot=${oneShot},randChoice=${randChoice},approxOneShot=${approxOneShot},approxGreedyVQ=${approxGreedyVQ},V=${V},cacheProblem=${cacheProblem},numSlotsPerFrame=${numSlotsPerFrame},singleHop=${singleHop},singleItem=${singleItem},randChoice=${randChoice},futureKnowledge=${futureKnowledge} $NS3_DIR/cache_run_scripts/vary_power_budget/run_mds_cache_vary_PB.pbs
    sleep 0.3
    j=$(($j + 1))
  done
  i=$(($i + $powBudgIncrement))
done

