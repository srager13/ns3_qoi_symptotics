runTime=1000
numNodes=9
interval=5.0
sumSimilarity=2
timeliness=50
tracing="false"
numReturnImages=10
imageSizeBytes=1400
delayPadding=0.1
dataFilePath="./vary_sum_sim"
sumSimFilename="SumSimRequirements_USC_data_set"
runSeed=1
numRuns=1
numPacketsPerImage=5

./waf --run "qoi-query-flood --runTime=$runTime --numNodes=$numNodes --interval=$interval --sumSimilarity=$sumSimilarity --timeliness=$timeliness --tracing=$tracing --numReturnImages=$numReturnImages --imageSizeBytes=$imageSizeBytes --delayPadding=$delayPadding --dataFilePath=${dataFilePath} --sumSimFilename=${sumSimFilename} --runSeed=$runSeed --numRuns=$numRuns --numPacketsPerImage=$numPacketsPerImage"
