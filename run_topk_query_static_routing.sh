runTime=500
numNodes=25
interval=5.0
sumSimilarity=0.6
timeliness=5
tracing="false"
numReturnImages=50
imageSizeKBytes=3
packetSizeBytes=1463
delayPadding=0.0
dataFilePath="./vary_sum_sim"
sumSimFilename="SumSimRequirements_USC_data_set"
runSeed=1
numRuns=1
numPacketsPerImage=$( bc <<< "($imageSizeKBytes * 1000)/$packetSizeBytes" )

numQueries=100
runTime=$(( $(($timeliness + 5))*$numQueries))
runTime=250

echo "run time = $runTime"

sumSimilarityStart=4.0
sumSimilarityEnd=12.0
sumSimilarityInc=1.0
sumSimilarity=$sumSimilarityStart
while [ $( bc <<< "$sumSimilarity <= $sumSimilarityEnd" ) -eq 1 ]; do
	./waf --run "topk-query-static-routing --runTime=$runTime --numNodes=$numNodes --interval=$interval --sumSimilarity=$sumSimilarity --timeliness=$timeliness --tracing=$tracing --numReturnImages=$numReturnImages --imageSizeKBytes=$imageSizeKBytes --delayPadding=$delayPadding --dataFilePath=${dataFilePath} --sumSimFilename=${sumSimFilename} --runSeed=$runSeed --numRuns=$numRuns --numPacketsPerImage=$numPacketsPerImage --packetSizeBytes=$packetSizeBytes"
	
	sumSimilarity=$( bc <<< "$sumSimilarity + $sumSimilarityInc" )
done
