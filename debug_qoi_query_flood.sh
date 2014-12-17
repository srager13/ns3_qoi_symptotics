runTime=500
numNodes=9
interval=5.0
sumSimilarity=7.5
timeliness=50
tracing="false"
imageSizeKBytes=3
packetSizeBytes=1463
delayPadding=0.1
dataFilePath="./vary_sum_sim"
sumSimFilename="SumSimRequirements_USC_data_set"
runSeed=1
numRuns=1
numPacketsPerImage=1

./waf --run qoi-query-flood --command-template="gdb --args %s --runTime=$runTime --numNodes=$numNodes --interval=$interval --sumSimilarity=$sumSimilarity --timeliness=$timeliness --tracing=$tracing --imageSizeKBytes=$imageSizeKBytes --delayPadding=$delayPadding --dataFilePath=${dataFilePath} --sumSimFilename=${sumSimFilename} --runSeed=$runSeed --numRuns=$numRuns --packetsPerImage=$packetsPerImage"
