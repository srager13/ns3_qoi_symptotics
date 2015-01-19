#runTime=500 # set below
numNodes=45
sumSimilarity=0.5
timeliness=5
#tracing="true"
tracing="false"
imageSizeKBytes=395
#packetSizeBytes=750
packetSizeBytes=6000
delayPadding=0.0
dataFilePath="./vary_sum_sim"
dataFilePath_2="./vary_sum_sim"
sumSimFilename="SumSimRequirements_USC_data_set"
initGuessFilename="initial_guesses"
runSeed=1
numRuns=1
numPacketsPerImage=$( bc <<< "($imageSizeKBytes * 1000)/$packetSizeBytes" )
channelRate=2
oneFlow=true
#sourceNode=1
#destNode=0
sourceNode=0
destNode=0
#destNode=$(($numNodes-1))
satAllQueries=false
runOnceDebug=false
clientDebug=false
serverDebug=false

numQueries=100
runTime=$(( $(($timeliness + 5))*$numQueries))
runTime=500

echo "run time = $runTime"

#./waf --run topk-query-line-net --command-template="gdb --args %s --runTime=$runTime --numNodes=$numNodes --sumSimilarity=$sumSimilarity --timeliness=$timeliness --tracing=$tracing --imageSizeKBytes=$imageSizeKBytes --delayPadding=$delayPadding --dataFilePath=${dataFilePath} --dataFilePath_2=${dataFilePath_2} --sumSimFilename=${sumSimFilename} --runSeed=$runSeed --numRuns=$numRuns --numPacketsPerImage=$numPacketsPerImage --packetSizeBytes=$packetSizeBytes --channelRate=$channelRate --oneFlow=$oneFlow"
./waf --run topk-query-line-net --command-template="gdb --args %s --runTime=$runTime --numNodes=$numNodes --sumSimilarity=$sumSimilarity --timeliness=$timeliness --tracing=$tracing --imageSizeKBytes=$imageSizeKBytes --delayPadding=$delayPadding --dataFilePath=${dataFilePath} --dataFilePath_2=${dataFilePath_2} --sumSimFilename=${sumSimFilename} --initGuessFilename=${initGuessFilename} --runSeed=$runSeed --numRuns=$numRuns --numPacketsPerImage=$numPacketsPerImage --packetSizeBytes=$packetSizeBytes --channelRate=$channelRate --oneFlow=$oneFlow --sourceNode=$sourceNode --destNode=$destNode --satAllQueries=${satAllQueries} --runOnceDebug=${runOnceDebug} --clientDebug=${clientDebug} --serverDebug=${serverDebug}"
	
