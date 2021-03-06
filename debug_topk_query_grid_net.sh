# runTime=500 # set below
numNodes=0
sumSimilarity=2.0
timeliness=50
tracing="false"
imageSizeKBytes=100
packetSizeBytes=1463
delayPadding=0.0
dataFilePath="./vary_sum_sim"
dataFilePath_2="./vary_sum_sim"
sumSimFilename="SumSimRequirements_USC_data_set"
runSeed=1
numRuns=1
numPacketsPerImage=$( bc <<< "($imageSizeKBytes * 1000)/$packetSizeBytes" )

#numQueries=100
#runTime=$(( $(($timeliness + 5))*$numQueries))
runTime=500

echo "run time = $runTime"

./waf --run topk-query-grid-net --command-template="gdb --args %s --runTime=$runTime --numNodes=$numNodes --sumSimilarity=$sumSimilarity --timeliness=$timeliness --tracing=$tracing --imageSizeKBytes=$imageSizeKBytes --delayPadding=$delayPadding --dataFilePath=${dataFilePath} --dataFilePath_2=${dataFilePath_2} --sumSimFilename=${sumSimFilename} --runSeed=$runSeed --numRuns=$numRuns --numPacketsPerImage=$numPacketsPerImage --packetSizeBytes=$packetSizeBytes"

