#runTime=500 # set below
numNodes=36
sumSimilarity=0.0
timeliness=30
#tracing="true"
imageSizeKBytes=48
packetSizeBytes=1500
delayPadding=0.0
dataFilePath="./vary_sum_sim"
dataFilePath_2="./vary_sum_sim"
sumSimFilename="SumSimRequirements_USC_data_set"
initGuessFilename="initial_guesses"
runSeed=1
numRuns=1
numPacketsPerImage=$( bc <<< "($imageSizeKBytes * 1000)/$packetSizeBytes" )
channelRate=2
#oneFlow=true
oneFlow=false
#sourceNode=$(($numNodes-1))
#sourceNode=1
destNode=0
sourceNode=0
satAllQueries=false
varyReqSumSim=true
#runOnceDebug=true
#clientDebug=true
#serverDebug=true
runOnceDebug=false
clientDebug=false
serverDebug=false

numQueries=100
#runTime=$(( $(($timeliness + 5))*$numQueries))
runTime=500

echo "run time = $runTime"

./waf --run "topk-query-grid-net --runTime=$runTime --numNodes=$numNodes --sumSimilarity=$sumSimilarity --timeliness=$timeliness --tracing=$tracing --imageSizeKBytes=$imageSizeKBytes --delayPadding=$delayPadding --dataFilePath=${dataFilePath} --dataFilePath_2=${dataFilePath_2} --sumSimFilename=${sumSimFilename} --initGuessFilename=${initGuessFilename} --runSeed=$runSeed --numRuns=$numRuns --numPacketsPerImage=$numPacketsPerImage --packetSizeBytes=$packetSizeBytes --channelRate=$channelRate --oneFlow=$oneFlow --sourceNode=$sourceNode --destNode=$destNode --satAllQueries=$satAllQueries --runOnceDebug=${runOnceDebug} --clientDebug=${clientDebug} --serverDebug=${serverDebug} --varyReqSumSim=${varyReqSumSim}"
	
