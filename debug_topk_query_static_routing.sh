runTime=500
numNodes=9
interval=5.0
sumSimilarity=1.5
timeliness=5
tracing="true"
numReturnImages=10
imageSizeBytes=1400
delayPadding=0.0
dataFilePath="./vary_sum_sim"
sumSimFilename="SumSimRequirements_USC_data_set"
runSeed=1
numRuns=1
numPacketsPerImage=100

numQueries=100
runTime=$(( $(($timeliness + 5))*$numQueries))
runTime=500

echo "run time = $runTime"
echo "sum similarity = $sumSimilarity"

./waf --run topk-query-static-routing --command-template="gdb --args %s --runTime=$runTime --numNodes=$numNodes --interval=$interval sumSimilarity=$sumSimilarity --timeliness=$timeliness  --tracing=$tracing --numReturnImages=$numReturnImages --imageSizeBytes=$imageSizeBytes --delayPadding=$delayPadding --dataFilePath=${dataFilePath} --sumSimFilename=${sumSimFilename} --runSeed=$runSeed --numRuns=$numRuns --numPacketsPerImage=$numPacketsPerImage"

