#runTime=500 # set below
numNodes=250
#sumSimilarity=10.0 # set below
timeliness=50
tracing="false"
imageSizeKBytes=10
packetSizeBytes=1463
delayPadding=0.0
dataFilePath="./vary_sum_sim"
sumSimFilename="SumSimRequirements_USC_data_set"
runSeed=1
numRuns=1
numPacketsPerImage=$( bc <<< "($imageSizeKBytes * 1000)/$packetSizeBytes" )
channelRate=2
oneFlow=true

numQueries=100
runTime=$(( $(($timeliness + 5))*$numQueries))
runTime=500

echo "run time = $runTime"

sumSimilarityStart=3.0
sumSimilarityEnd=3.0
sumSimilarityInc=1.0
sumSimilarity=$sumSimilarityStart
while [ $( bc <<< "$sumSimilarity <= $sumSimilarityEnd" ) -eq 1 ]; do
	./waf --run topk-query-line-net --command-template="gdb --args %s --runTime=$runTime --numNodes=$numNodes --sumSimilarity=$sumSimilarity --timeliness=$timeliness --tracing=$tracing --imageSizeKBytes=$imageSizeKBytes --delayPadding=$delayPadding --dataFilePath=${dataFilePath} --sumSimFilename=${sumSimFilename} --runSeed=$runSeed --numRuns=$numRuns --numPacketsPerImage=$numPacketsPerImage --packetSizeBytes=$packetSizeBytes --channelRate=$channelRate --oneFlow=$oneFlow"
	
	sumSimilarity=$( bc <<< "$sumSimilarity + $sumSimilarityInc" )
done
