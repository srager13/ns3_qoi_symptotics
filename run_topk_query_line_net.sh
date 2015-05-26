#runTime=500 # set below
numNodes=125
distance=75
sumSimilarity=0.5
timeliness=16
#tracing="true"
tracing="false"
imageSizeKBytes=18
packetSizeBytes=1500
delayPadding=0.0
dataFilePath="./delay_dists"
dataFilePath_2="./delay_dists"
sumSimFilename="SumSimRequirements_USC_data_set"
initGuessFilename="initial_guesses"
runSeed=1
numRuns=1
numPacketsPerImage=$( bc <<< "($imageSizeKBytes * 1000)/$packetSizeBytes" )
channelRate=2
oneFlow=false
sourceNode=1
destNode=0
#sourceNode=0
#destNode=0
satAllQueries=false
runOnceDebug=true
#clientDebug=true
#serverDebug=true
#runOnceDebug=false
clientDebug=false
serverDebug=false
useWifi=false

numQueries=100
runTime=$(( $(($timeliness + 5))*$numQueries))
runTime=300

echo "run time = $runTime"

./waf --run "topk-query-line-net --runTime=$runTime --numNodes=$numNodes --sumSimilarity=$sumSimilarity --timeliness=$timeliness --tracing=$tracing --imageSizeKBytes=$imageSizeKBytes --delayPadding=$delayPadding --dataFilePath=${dataFilePath} --dataFilePath_2=${dataFilePath_2} --sumSimFilename=${sumSimFilename} --initGuessFilename=${initGuessFilename} --runSeed=$runSeed --numRuns=$numRuns --numPacketsPerImage=$numPacketsPerImage --packetSizeBytes=$packetSizeBytes --channelRate=$channelRate --oneFlow=$oneFlow --sourceNode=$sourceNode --destNode=$destNode --satAllQueries=${satAllQueries} --runOnceDebug=${runOnceDebug} --clientDebug=${clientDebug} --serverDebug=${serverDebug} --useWifi=${useWifi} --distance=$distance"
	
