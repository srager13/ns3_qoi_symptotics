#!/bin/bash
debug=0
while (( "$#" ))
do 
	key="$1"
	case $key in 
		-d)
		debug=1
		shift
		;;
	esac
	shift
done

numNodes=3
runSeed=2
runTime=10
inputRate=1.0
powerBudget=5.00
timeSlotDuration=1.0
maxBackoffWindow=10
timeBudget=1.0
initBatteryPower=5120
areaWidth=300
areaLength=300
nodeSpeed=0.1
maxPause=100.0
oneShot=0
approxOneShot=0
approxOneShotRatio=0
longTermAvg=0
approxGreedyVQ=0
minItemCovDist=90
maxItemCovDist=180
fixedPacketSendSize=2200
V=100
cacheProblem=1
numSlotsPerFrame=5
singleHop=1
singleItem=1
randChoice=1
pcap=true


cd $NS3_DIR

echo "Starting mds cached mobile: "
echo "Debug = $debug"
echo "Input Rate = $inputRate"
echo "Run Seed = $runSeed"
echo "Run time = $runTime"
echo "Power Budget = $powerBudget"
echo "Time Slot Duration = $timeSlotDuration"
echo "Time Budget = $timeBudget"
echo "Initial Battery Power = $initBatteryPower"
echo "Area Width = $areaWidth"
echo "Area Length = $areaLength"
echo "Node Speed = $nodeSpeed"
echo "Pause Time = $maxPause"
echo "One shot = $oneShot"
echo "Approx One Shot = $approxOneShot"
echo "Approx One Shot Ratio = $approxOneShotRatio"
echo "Long Term Avg = $longTermAvg"
echo "Approx Greedy VQ = $approxGreedyVQ"
echo "Min Item Cov Distance = $minItemCovDist"
echo "Max Item Cov Distance = $maxItemCovDist"
echo "Fixed Packet Send Size = $fixedPacketSendSize"
echo "V = $V"
echo "cacheProblem = $cacheProblem"
echo "numSlotsPerFrame = $numSlotsPerFrame"
echo "singleHop = $singleHop"
echo "singleItem = $singleItem"
echo "randChoice = $randChoice"

if [ $debug -eq 0 ]
then
	./waf --run "scratch/mds-cache-mobile --numNodes=$numNodes --runSeed=$runSeed --time=$runTime --timeSlotDuration=$timeSlotDuration --timeBudg=$timeBudget --powerBudg=$powerBudget --packetsPerSec=$inputRate --initBatteryPower=$initBatteryPower --areaWidth=$areaWidth --areaLength=$areaLength --nodeSpeed=$nodeSpeed --maxPause=$maxPause --oneShot=$oneShot --approxOneShot=$approxOneShot --approxOneShotRatio=$approxOneShotRatio --longTermAvg=$longTermAvg --approxGreedyVQ=$approxGreedyVQ --minItemCovDist=$minItemCovDist --maxItemCovDist=$maxItemCovDist --fixedPacketSendSize=$fixedPacketSendSize --V=$V --cacheProblem=$cacheProblem --numSlotsPerFrame=$numSlotsPerFrame --singleHop=$singleHop --singleItem=$singleItem --randChoice=$randChoice --ns3::dus::RoutingProtocol::dataFilePath='./data_files/mds_cache/vary_power_budget/initBattery_5120/IR_1_nodes_10/PB_5.00/run_seed_2/'"
else
	./waf --run scratch/mds-cache-mobile --command-template="gdb --args %s --numNodes=$numNodes --runSeed=$runSeed --time=$runTime --timeSlotDuration=$timeSlotDuration --timeBudg=$timeBudget --powerBudg=$powerBudget --packetsPerSec=$inputRate --initBatteryPower=$initBatteryPower --areaWidth=$areaWidth --areaLength=$areaLength --nodeSpeed=$nodeSpeed --maxPause=$maxPause --oneShot=$oneShot --approxOneShot=$approxOneShot --approxOneShotRatio=$approxOneShotRatio --longTermAvg=$longTermAvg --approxGreedyVQ=$approxGreedyVQ --minItemCovDist=$minItemCovDist --maxItemCovDist=$maxItemCovDist --fixedPacketSendSize=$fixedPacketSendSize --V=$V --cacheProblem=$cacheProblem --numSlotsPerFrame=$numSlotsPerFrame --singleHop=$singleHop --singleItem=$singleItem --randChoice=$randChoice --pcap=$pcap --ns3::dus::RoutingProtocol::dataFilePath='./data_files/mds_cache/vary_power_budget/initBattery_5120/IR_1_nodes_10/PB_5.00/run_seed_2/'"
fi

