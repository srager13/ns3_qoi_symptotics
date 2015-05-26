/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/distUnivSched-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;
/**
 * \brief Test script.
 * 
 * This script creates 2 nodes and sends packet from node 1 to node 2 over Wifi
 * 
 * [Node 1] -- Packet --> [Node 2] 
 * 
 * Application: send packets from 1 to 2 using on/off application
 */

//NS_OBJECT_ENSURE_REGISTERED(MaxDivSchedExample);

class MaxDivSchedExample
{
public:
  MaxDivSchedExample ();
  /// Configure script parameters, \return true on successful configuration
  //bool Configure (int argc, char **argv);
  static TypeId GetTypeId (void);
  /// Run simulation
  void Run ();
  /// Report results
  void Report (std::ostream & os);

  void setNumNodes( int x ) { numNodes = x; };
  ///\name parameters
  //\{
  /// Number of nodes
  uint32_t numNodes;

  /// Dimensions of 2-D simulation environment area 
  uint32_t areaWidth;
  uint32_t areaLength;

  /// Maximum distance that nodes can trx successfully.  This is the maximum distance nodes will be placed from HQ.
  double maxRadioRange;

  /// Number of packets per second that the application should generate
  double packetsPerSec;
  double dupPacketRate;
  double timeSlotDuration; // in Seconds
  double maxBackoffWindow;
  int longTermAvg;
  int oneShot;
  int randChoice;
  int approxOneShot;
  int approxOneShotRatio;
  int approxGreedyVQ;
  int outputPowerVsTime;
  int fixedPacketLength;
  int minItemCovDist;
  int maxItemCovDist;
  int fixedPacketSendSize;
  int V;
  int runSeed;
  int cacheProblem;
  int singleHop;
  int singleItem;
  int numSlotsPerFrame;
	int futureKnowledge;

  double timeBudg;
  double powerBudg;
  double initBatteryPower;

  double nodeSpeed;
  double maxPause;

  /// Distance between nodes, meters
  double step;

  /// Simulation time, seconds
  double totalTime;

  /// Write per-device PCAP traces if true
  bool pcap;

  //\}
  
private:
  
  ///\name network
  //\{
  NodeContainer nodes;
  NodeContainer HQ;
  NodeContainer nonHQ;
  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;
  //\}
  
private:
  void CreateNodes ();
  void CreateDevices ();
  void InstallInternetStack ();
  void InstallApplications ();
};

int 
main (int argc, char *argv[])
{
  bool verbose = true;


  MaxDivSchedExample test;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("pcap", "Write PCAP traces.", test.pcap);
  cmd.AddValue ("numNodes", "Number of nodes.", test.numNodes);
  cmd.AddValue ("areaWidth", "Width of Simulation Area.", test.areaWidth);
  cmd.AddValue ("areaLength", "Length of Simulation Area.", test.areaLength);
  cmd.AddValue ("time", "Simulation time, s.", test.totalTime);
  cmd.AddValue ("timeBudg", "Time budget.", test.timeBudg);
  cmd.AddValue ("powerBudg", "Power budget.", test.powerBudg);
  cmd.AddValue ("initBatteryPower", "Battery's total power supply.", test.initBatteryPower);
  cmd.AddValue ("nodeSpeed", "Maximum node speed in Random Waypoint mobility.", test.nodeSpeed);
  cmd.AddValue ("maxPause", "Maximum node pause time in Random Waypoint mobility.", test.maxPause);
  cmd.AddValue ("packetsPerSec", "Application data rate in packets per second.", test.packetsPerSec);
  cmd.AddValue ("longTermAvg", "True to run long term average problem.", test.longTermAvg);
  cmd.AddValue ("oneShot", "True to run one shot problem.", test.oneShot);
  cmd.AddValue ("randChoice", "True to run random choice problem.", test.randChoice);
  cmd.AddValue ("approxOneShot", "True to solve one shot problem with greedy approximation algorithm (max additional coverage).", test.approxOneShot);
  cmd.AddValue ("approxOneShotRatio", "True to solve one shot problem with greedy coverage/cost ratio approximation algorithm (max additional coverage/addition cost).", test.approxOneShot);
  cmd.AddValue ("approxGreedyVQ", "True to solve one shot problem with greedy virtual queue approximation algorithm.", test.approxGreedyVQ);
  cmd.AddValue ("outputPowerVsTime", "True to output power vs time for each node.", test.outputPowerVsTime);
  cmd.AddValue ("dupPacketRate", "Number of packets duplicated each time slot.", test.dupPacketRate);
  cmd.AddValue ("timeSlotDuration", "Number of packets duplicated each time slot.", test.timeSlotDuration);
  cmd.AddValue ("maxBackoffWindow", "Node randomly chooses a delay from 0 to this time when sending a control packet to prevent collisions (in milliseconds).", test.maxBackoffWindow);
  cmd.AddValue ("fixedPacketLength", "Size of one side of packet coverage (if packet sizes are fixed).", test.fixedPacketLength);
  cmd.AddValue ("minItemCovDist", "Minimum size of a packet side length (coverage, not cost to send).", test.minItemCovDist);
  cmd.AddValue ("maxItemCovDist", "Maximum size of a packet side length (coverage, not cost to send).", test.maxItemCovDist);
  cmd.AddValue ("fixedPacketSendSize", "Maximum size of a packet side length (coverage, not cost to send).", test.fixedPacketSendSize);
  cmd.AddValue ("V", "V parameter", test.V);
  cmd.AddValue ("runSeed", "Seed for random number generator", test.runSeed);
  cmd.AddValue ("cacheProblem", "True to solve problem with multiple time slots per frame and cached data.", test.cacheProblem);
  cmd.AddValue ("numSlotsPerFrame", "Number of slots in each frame.", test.numSlotsPerFrame);
  cmd.AddValue ("singleHop", "True to only use single hop between reporting nodes and HQ.", test.singleHop);
  cmd.AddValue ("singleItem", "True to only allow selection of single item to transmit each slot.", test.singleItem);
  cmd.AddValue ("futureKnowledge", "True to solve problem with caching and perfect future knowledge through entire frame", test.futureKnowledge);
 
  cmd.Parse (argc,argv);
  
  test.Run ();
  test.Report (std::cout);
  return 0;
}

//-----------------------------------------------------------------------------
MaxDivSchedExample::MaxDivSchedExample () :
numNodes (5),
areaWidth(350),
areaLength(350),
maxRadioRange(174.0),
packetsPerSec(6.0),
dupPacketRate(0.0),
timeSlotDuration(1.0),
maxBackoffWindow(4.0),
longTermAvg(0),
oneShot(0),
randChoice(0),
approxOneShot(0),
approxOneShotRatio(0),
approxGreedyVQ(0),
outputPowerVsTime(0),
fixedPacketLength(140),
minItemCovDist(90),
maxItemCovDist(180),
fixedPacketSendSize(1600),
V(1000),
runSeed(2),
cacheProblem(0),
singleHop(0),
singleItem(0),
numSlotsPerFrame(10),
futureKnowledge(0),
timeBudg(1.0),
powerBudg(20.0),
initBatteryPower(5120.0),
nodeSpeed(10.0),
maxPause(5.0),
step (20),
totalTime (50),
pcap (false)
{
}


void
MaxDivSchedExample::Run ()
{

  SeedManager::SetRun(runSeed); 

  //  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (1)); // enable rts cts all the time.
  CreateNodes ();
  CreateDevices ();
  InstallInternetStack ();
  InstallApplications ();
  
  //std::cout << "Starting simulation for " << totalTime << " s ...\n";
  
  Simulator::Stop (Seconds (totalTime));
  Simulator::Run ();
  Simulator::Destroy ();
}

void
MaxDivSchedExample::Report (std::ostream &)
{ 
  //std::cout << "In MaxDivSchedExample::Report()\n";
}

void
MaxDivSchedExample::CreateNodes ()
{
  //std::cout << "Creating " << (unsigned)numNodes << " nodes " << step << " m apart.\n";
  HQ.Create( 1 );
  nonHQ.Create( numNodes - 1 );
 
  // Put HQ node in the center of the simulation environment
  Ptr<ListPositionAllocator> positionHQ =
    CreateObject<ListPositionAllocator>();
    
  positionHQ->Add (Vector ((double)areaWidth/2.0,(double)areaLength/2.0,0.0));

  MobilityHelper mobilityHQ;
  mobilityHQ.SetPositionAllocator (positionHQ);
  mobilityHQ.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobilityHQ.Install( HQ );

  ObjectFactory pos;
  pos.SetTypeId ("ns3::UniformDiscPositionAllocator");
  if( singleHop )
    pos.Set ("rho", DoubleValue (maxRadioRange)); // rho is radius of disk
  else
		pos.Set ("rho", DoubleValue (areaWidth/2.0)); // rho is radius of disk
  pos.Set( "X", DoubleValue ((double)areaWidth/2.0)); // X and Y are coordinates of center of disk
  pos.Set( "Y", DoubleValue ((double)areaLength/2.0)); 
                
  Ptr<PositionAllocator> posPtr = pos.Create()->GetObject<PositionAllocator>(); 

  // place all other nodes in a disc aroung the HQ
  MobilityHelper mobility;

  if( singleHop )
    mobility.SetPositionAllocator ("ns3::UniformDiscPositionAllocator",
                                 "rho", DoubleValue (maxRadioRange), // rho is radius of disk
                                 "X", DoubleValue ((double)areaWidth/2.0), // X and Y are coordinates of center of disk
                                 "Y", DoubleValue((double)areaLength/2.0) );
  else
    mobility.SetPositionAllocator ("ns3::UniformDiscPositionAllocator",
                                 "rho", DoubleValue ((double)areaWidth/2.0), // rho is radius of disk
                                 "X", DoubleValue ((double)areaWidth/2.0), // X and Y are coordinates of center of disk
                                 "Y", DoubleValue((double)areaLength/2.0) );
  char nodeSpeedBuf[100];
  char maxPauseBuf[100];
	sprintf( nodeSpeedBuf, "ns3::ConstantRandomVariable[Constant=%f]", nodeSpeed );
  sprintf( maxPauseBuf, "ns3::ConstantRandomVariable[Constant=%f]", maxPause );
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel", 
														 "Speed", StringValue(nodeSpeedBuf),
														 "Pause", StringValue(maxPauseBuf),
														 //"Speed", RandomVariableValue (ConstantRandomVariable (nodeSpeed)),
                             //"Pause", RandomVariableValue (ConstantRandomVariable (maxPause)), 
														 "PositionAllocator", PointerValue (posPtr) );
  mobility.Install( nonHQ );

  // join HQ and non-HQ nodes into same node container
  nodes.Add (HQ);
  nodes.Add (nonHQ);
  // Name nodes
  for (uint32_t i = 0; i < numNodes; ++i)
  {
    std::ostringstream os;
    os << "node-" << i;
    Names::Add (os.str (), nodes.Get (i));
  }
}

void
MaxDivSchedExample::CreateDevices ()
{
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("DsssRate1Mbps"), "RtsCtsThreshold", UintegerValue (100000));
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "ControlMode", StringValue ("DsssRate1Mbps"), "RtsCtsThreshold", UintegerValue (100000));
  devices = wifi.Install (wifiPhy, wifiMac, nodes); 
  
  if (pcap)
  {
    wifiPhy.EnablePcapAll (std::string ("maxDivSched"));
  }
}

void
MaxDivSchedExample::InstallInternetStack ()
{
  MaxDivSchedHelper maxDivSched;
  // you can configure MAX_DIV_SCHED attributes here using distUnivSched.Set(name, value)
 
  // NOTE: Anything set here cannot be set on the command line 
  maxDivSched.Set( "GLOBAL_KNOWLEDGE", BooleanValue (true) );
  if( longTermAvg == 1 )
  {
    maxDivSched.Set( "longTermAvgProblem", BooleanValue (true) );
  }
  else
  {
    maxDivSched.Set( "longTermAvgProblem", BooleanValue (false) );
  }
  if( oneShot == 1 )
  {
    maxDivSched.Set( "oneShotProblem", BooleanValue (true) );
  }
  else
  {
    maxDivSched.Set( "oneShotProblem", BooleanValue (false) );
  }
  if( randChoice == 1 )
  {
    maxDivSched.Set( "randomChoiceProblem", BooleanValue (true) );
  }
  else
  {
    maxDivSched.Set( "randomChoiceProblem", BooleanValue (false) );
  }
  if( approxOneShot == 1 )
  {
    maxDivSched.Set( "approxOneShotProblem", BooleanValue (true) );
  }
  else
  {
    maxDivSched.Set( "approxOneShotProblem", BooleanValue (false) );
  }
  if( approxOneShotRatio == 1 )
  {
    maxDivSched.Set( "approxOneShotRatioProblem", BooleanValue (true) );
  }
  else
  {
    maxDivSched.Set( "approxOneShotRatioProblem", BooleanValue (false) );
  }
  if( approxGreedyVQ == 1 )
  {
    maxDivSched.Set( "approxGreedyVQProblem", BooleanValue (true) );
  }
  else
  {
    maxDivSched.Set( "approxGreedyVQProblem", BooleanValue (false) );
  }
  if( cacheProblem == 1 )
  {
    maxDivSched.Set( "cacheProblem", BooleanValue (true) );
  }
  else
  {
    maxDivSched.Set( "cacheProblem", BooleanValue (false) );
  }
  if( singleHop == 1 )
  {
    maxDivSched.Set( "singleHop", BooleanValue (true) );
  }
  else
  {
    maxDivSched.Set( "singleHop", BooleanValue (false) );
  }
  if( singleItem == 1 )
  {
    maxDivSched.Set( "singleItem", BooleanValue (true) );
  }
  else
  {
    maxDivSched.Set( "singleItem", BooleanValue (false) );
  }
  if( futureKnowledge == 1 )
  {
    maxDivSched.Set( "futureKnowledge", BooleanValue (true) );
  }
  else
  {
    maxDivSched.Set( "futureKnowledge", BooleanValue (false) );
  }
  if( outputPowerVsTime == 1 )
  {
    maxDivSched.Set( "outputPowerVsTime", BooleanValue (true) );
  }
  else
  {
    maxDivSched.Set( "outputPowerVsTime", BooleanValue (false) );
  }
  maxDivSched.Set( "timeSlotDuration", TimeValue(Seconds(timeSlotDuration)) );
//  maxDivSched.Set( "maxBackoffWindow", IntegerValue(maxBackoffWindow) );
  maxDivSched.Set( "controlInfoExchangeTime", TimeValue(MilliSeconds(100)) );
  maxDivSched.Set( "waitForAckTimeoutTime", TimeValue(MilliSeconds(15)) );
  maxDivSched.Set( "simulationTime", TimeValue(Seconds(totalTime)) );
  maxDivSched.Set( "numNodes", IntegerValue((int)numNodes) );
  maxDivSched.Set( "areaWidth", IntegerValue((int)areaWidth) );
  maxDivSched.Set( "areaLength", IntegerValue((int)areaLength) );
  maxDivSched.Set( "numRun", IntegerValue(runSeed) );
  maxDivSched.Set( "numSlotsPerFrame", IntegerValue(numSlotsPerFrame) );
  maxDivSched.Set( "numCommodities", IntegerValue((int)numNodes) );
  maxDivSched.Set( "areaWidth", IntegerValue((int)areaWidth) );
  maxDivSched.Set( "areaLength", IntegerValue((int)areaLength) );
  maxDivSched.Set( "minItemCovDist", IntegerValue((int)minItemCovDist) );
  maxDivSched.Set( "maxItemCovDist", IntegerValue((int)maxItemCovDist) );
  maxDivSched.Set( "fixedPacketSendSize", IntegerValue((int)fixedPacketSendSize) );
  maxDivSched.Set( "nodePossCovDist", IntegerValue(maxItemCovDist) ); // numNodes of box around node in which packets can be generated
  maxDivSched.Set( "nodePowerBudget", DoubleValue(powerBudg) );
  maxDivSched.Set( "avgPowerBudget", DoubleValue(powerBudg) );
  maxDivSched.Set( "timeBudget", TimeValue(Seconds(timeBudg)) );
  maxDivSched.Set( "batteryPowerLevel", DoubleValue(initBatteryPower) );
  maxDivSched.Set( "nodeRWSpeed", DoubleValue(nodeSpeed) );
  maxDivSched.Set( "nodeRWPause", DoubleValue(maxPause) );
  maxDivSched.Set( "packetDataRate", DoubleValue(packetsPerSec) ); // this needs to match application data rate below
  maxDivSched.Set( "fixedPacketSize", BooleanValue (false) );
  maxDivSched.Set( "fixedPacketLength", IntegerValue(fixedPacketLength) );
  maxDivSched.Set( "duplicatePacketDataRate", IntegerValue((int)dupPacketRate) );
  maxDivSched.Set( "V", IntegerValue(V) );

  InternetStackHelper stack;
  stack.SetRoutingHelper (maxDivSched); // has effect on the next Install ()
  stack.Install (nodes);
  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.0.0.0");
  interfaces = address.Assign (devices);
 
}

void
MaxDivSchedExample::InstallApplications ()
{
  uint16_t port = 1300;
  char buf[1024];
  int packetSize = 512; // numNodes of packets in bytes...doesn't really matter, because this is changed in simulation

  sprintf( buf, "%ib/s", (int)packetsPerSec*packetSize*8 );
  
  //printf( "%s\n", buf);

  for( uint32_t i = 1; i < numNodes; i++ )
  {
    OnOffHelper onOff ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address ("10.0.0.1"), port)));
    //onOff.SetAttribute ("DataRate", DataRateValue( DataRate ( buf ))); //"4506b/s")));
    onOff.SetAttribute ("PacketSize", UintegerValue (packetSize));
  //char onTimeBuf[100];
  //char offTimeBuf[100];
	//sprintf( onTimeBuf, "ns3::ConstantRandomVariable[Constant=%i]", 1 );
  //sprintf( offTimeBuf, "ns3::ConstantRandomVariable[Constant=%i]", 0 );
//    onOff.SetAttribute ("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"));
//    onOff.SetAttribute ("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"));
		onOff.SetConstantRate( DataRate( "512B/s" ));
 
    ApplicationContainer app = onOff.Install (nodes.Get (i));

    app.Start (Seconds (0.0));
    app.Stop (Seconds (totalTime) - Seconds (0.001));
  }
}

