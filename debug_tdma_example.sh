if [[ $# -gt 0 ]]; then
	nWifis=$1
else
	nWifis=5
fi

export 'NS_LOG=TdmaController=level_all|prefix_func:TdmaCentralMac=level_all|prefix_func:TdmaMac=level_all|prefix_func:TdmaMacQueue=level_all|prefix_func:SimpleWirelessChannel=level_all|prefix_func:TdmaMacLow=level_all|prefix_func:TdmaNetDevice=level_all|prefix_func:DsdvRoutingProtocol=level_all|prefix_func'

totalTime=30
nSinks=1
tracing="true"
numReturnImages=5
imageSizeBytes=200

./waf --run tdma-example --command-template="gdb --args %s --nWifis=$nWifis --nSinks=$nSinks --totalTime=$totalTime"
