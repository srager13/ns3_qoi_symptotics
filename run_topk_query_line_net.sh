#runTime=500 # set below
numNodes=500
#sumSimilarity=10.0 # set below
timeliness=10
#tracing="true"
tracing="false"
imageSizeKBytes=10
packetSizeBytes=1463
delayPadding=0.0
dataFilePath="./vary_sum_sim"
dataFilePath_2="./vary_sum_sim"
sumSimFilename="SumSimRequirements_USC_data_set"
runSeed=1
numRuns=1
numPacketsPerImage=$( bc <<< "($imageSizeKBytes * 1000)/$packetSizeBytes" )
channelRate=2
oneFlow=true
satAllQueries=true

numQueries=100
runTime=$(( $(($timeliness + 5))*$numQueries))
runTime=1000

echo "run time = $runTime"

sumSimilarityStart=0.5
sumSimilarityEnd=0.5
sumSimilarityInc=1.0
sumSimilarity=$sumSimilarityStart
while [ $( bc <<< "$sumSimilarity <= $sumSimilarityEnd" ) -eq 1 ]; do
	./waf --run "topk-query-line-net --runTime=$runTime --numNodes=$numNodes --sumSimilarity=$sumSimilarity --timeliness=$timeliness --tracing=$tracing --imageSizeKBytes=$imageSizeKBytes --delayPadding=$delayPadding --dataFilePath=${dataFilePath} --dataFilePath_2=${dataFilePath_2} --sumSimFilename=${sumSimFilename} --runSeed=$runSeed --numRuns=$numRuns --numPacketsPerImage=$numPacketsPerImage --packetSizeBytes=$packetSizeBytes --channelRate=$channelRate --oneFlow=$oneFlow --satAllQueries=${satAllQueries}"
	
	sumSimilarity=$( bc <<< "$sumSimilarity + $sumSimilarityInc" )
done
