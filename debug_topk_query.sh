runTime=100
numNodes=144
interval=5.0
sumSimilarity=5
timeliness=20.0
tracing="false"
numReturnImages=10
imageSizeBytes=1000
delayPadding=0.1
dataFilePath="./vary_sum_sim"
sumSimFilename="SumSimRequirements"
runSeed=1
numRuns=1

./waf --run topk-query --command-template="gdb --args %s --runTime=$runTime --numNodes=$numNodes --interval=$interval sumSimilarity=$sumSimilarity --timeliness=$timeliness  --tracing=$tracing --numReturnImages=$numReturnImages --imageSizeBytes=$imageSizeBytes --delayPadding=$delayPadding --dataFilePath=${dataFilePath} --sumSimFilename=${sumSimFilename} --runSeed=$runSeed --numRuns=$numRuns"

