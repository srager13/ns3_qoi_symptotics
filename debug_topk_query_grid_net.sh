# runTime=500 # set below
numNodes=9
interval=5.0
sumSimilarity=7.5
timeliness=50
tracing="false"
numReturnImages=10
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
runTime=300

echo "run time = $runTime"

./waf --run topk-query-grid-net --command-template="gdb --args %s --runTime=$runTime --numNodes=$numNodes --interval=$interval --sumSimilarity=$sumSimilarity --timeliness=$timeliness --tracing=$tracing --numReturnImages=$numReturnImages --imageSizeKBytes=$imageSizeKBytes --delayPadding=$delayPadding --dataFilePath=${dataFilePath} --sumSimFilename=${sumSimFilename} --runSeed=$runSeed --numRuns=$numRuns --numPacketsPerImage=$numPacketsPerImage --packetSizeBytes=$packetSizeBytes"

