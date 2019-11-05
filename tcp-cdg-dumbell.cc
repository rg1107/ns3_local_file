#include <iostream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/uinteger.h"
#include "ns3/point-to-point-dumbbell.h"
#include "ns3/tcp-cdg.h"
#include "ns3/ptr.h"
#include "ns3/csma-module.h"
#include <fstream>

using namespace ns3;
uint32_t qsize=0;
bool printRTT = false;
bool printQueue = false;
std::vector<int64_t> rtt_records;

void queue_callback(uint32_t oldValue, uint32_t newValue) {
   if (printQueue) {
     std::cout << "Packets in queue:" << newValue << ":at time:" << ns3::Simulator::Now().GetMicroSeconds() << std::endl; 
   }
   qsize = newValue;
}

void trace_rtt(int64_t rtt)
{
  if (printRTT) {
   std::cout << "RTT:" << rtt << ":at time:" << ns3::Simulator::Now().GetMicroSeconds() << std::endl; 
  }
   rtt_records.push_back(rtt);
} 

uint32_t getQSize() { return qsize; }

int main (int argc, char *argv[])
  {
  std::string tcpType = "CDG";
  std::string socketType;
  bool traceRTT=true;
  uint32_t numLtNodes = 2;
  uint32_t numRtNodes = 2;
  uint32_t queueSize = 500000;
  uint32_t maxBytes   = 0;
  uint32_t backoff_beta = 0.70 * 1024;
  uint32_t backoff_factor = 333;
  uint32_t ineffective_thresh = 5;
  uint32_t ineffective_hold = 5;
  uint32_t rtt_seq = 0;
  uint32_t loss_cwnd = 0;
  uint32_t backoff_cnt = 0;
  uint32_t delack = 0;
  uint32_t numBulkSendApps = 2;
  //double emwa = 0.1, addstep = 4.0, beta = 0.01, thigh = 500, tlow = 50;

  // Parse command line arguments
  CommandLine cmd;
  cmd.AddValue ("tcpType",    "TCP type (use NewReno or CDG)",      tcpType);
  cmd.AddValue ("tracing",    "Flag to enable/disable tracing",          traceRTT);
  cmd.AddValue ("queueSize",    "Queue limit on the bottleneck link",      queueSize);
  cmd.AddValue ("maxBytes",   "Max bytes soure will send",               maxBytes);
  cmd.AddValue ("numBulkSendApps", "Number of BulkSendApps",             numBulkSendApps);
  cmd.AddValue ("printRTT", "Get RTT timestamps", printRTT);
  cmd.AddValue ("printQueue","Get Queue occupancy state",printQueue);
  cmd.Parse(argc, argv);

  // Set default values
 Config::SetDefault ("ns3::QueueBase::MaxPackets", UintegerValue(queueSize));
 socketType = "ns3::TcpSocketFactory";
 
  // Set transport protocol based on user input
  if (tcpType.compare("CDG") == 0)
    {
     std::cout << "\nSetting default protocol to Tcp-CDG" << std::endl;
     Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpCDG::GetTypeId ()));
     Config::SetDefault("ns3::TcpCDG::BackoffBeta", uint32_t(backoff_beta));
     Config::SetDefault("ns3::TcpCDG::BackoffFactor", uint32_t(backoff_factor));
     Config::SetDefault("ns3::TcpCDG::IneffectiveThresh", uint32_t(ineffective_thresh));
     Config::SetDefault("ns3::TcpCDG::IneffectiveHold", uint32_t(ineffective_hold));
     Config::SetDefault("ns3::TcpCDG::RTTSequence", uint32_t(rtt_seq));
     Config::SetDefault("ns3::TcpCDG::LossCwnd", uint32_t(loss_cwnd));
     Config::SetDefault("ns3::TcpCDG::BackoffCount", uint32_t(backoff_count));
     Config::SetDefault("ns3::TcpCDG::Delack", uint32_t(delack));
    
     
     /*Config::SetDefault("ns3::TcpCDG::QSizeCallback", CallbackValue(MakeCallback(&getQSize)));
     Config::SetDefault("ns3::TcpCDG::EMWA", DoubleValue(emwa));
     Config::SetDefault("ns3::TcpTimely::Addstep", DoubleValue(addstep));
     Config::SetDefault("ns3::TcpTimely::Beta", DoubleValue(beta));
     Config::SetDefault("ns3::TcpTimely::THigh", DoubleValue(thigh));
     Config::SetDefault("ns3::TcpTimely::TLow", DoubleValue(tlow));
     Config::SetDefault("ns3::TcpOptionTS::UseNS", BooleanValue(true));
     Config::SetDefault("ns3::TcpSocketBase::ClockGranularity", TimeValue(Time("1ns")));*/
   } 
  else if(tcpType.compare("NewReno") == 0)
    {
     std::cout << "\nSetting default protocol to Tcp-NewReno" << std::endl;
     Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId ()));
    } 
  else
    {
     std::cout<<"\nInvalid protocol selected"<<std::endl;
    }
  /*if (traceRTT) 
    {
    Config::SetDefault("ns3::TcpCongestionOps::TraceRTTCallback", CallbackValue(MakeCallback(&trace_rtt)));
    }*/

  // using the dumbbell helper to setup left and right nodes with two routers in the middle
  PointToPointHelper p2pLeaf, p2pRouters;
  p2pLeaf.SetDeviceAttribute     ("DataRate", StringValue ("50Mbps"));
  p2pLeaf.SetChannelAttribute    ("Delay",    StringValue ("1us"));
  p2pRouters.SetDeviceAttribute  ("DataRate", StringValue ("50Mbps"));
  p2pRouters.SetChannelAttribute ("Delay",    StringValue ("1us"));
  PointToPointDumbbellHelper dumbbell (numLtNodes, p2pLeaf,numRtNodes, p2pLeaf,p2pRouters);

  // Adding TCP/IP stack to all nodes (Transmission Control Protocol / Internet Protocol)
  InternetStackHelper stack;
  dumbbell.InstallStack (stack);

  // assigning IP addresses
  Ipv4AddressHelper ltIps     = Ipv4AddressHelper ("10.1.1.0", "255.255.255.0");
  Ipv4AddressHelper rtIps     = Ipv4AddressHelper ("10.2.1.0", "255.255.255.0");
  Ipv4AddressHelper routerIps = Ipv4AddressHelper ("10.3.1.0", "255.255.255.0");
  dumbbell.AssignIpv4Addresses(ltIps, rtIps, routerIps);

  uint16_t port = 9000;

  ApplicationContainer sourceApps;
  ApplicationContainer sinkApps;

  for (uint16_t i=0; i<numBulkSendApps; ++i)
    {
    // Creating BulkSendApplication source using a BulkSendHelper, whose constructor specifies the protocol to use and the address of the    remote node to send traffic to. Installing it on the left most node    
    BulkSendHelper source ("ns3::TcpSocketFactory",InetSocketAddress (dumbbell.GetRightIpv4Address (0), port + i));
    source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
    sourceApps.Add (source.Install (dumbbell.GetLeft (0)));

    sourceApps.Start (Seconds (0));
    sourceApps.Stop  (Seconds (10.0));

    // Creating PacketSinkApplication sink using a PacketSinkHelper, whose constructor specifies the protocol to use and the address of the sink.Installing it on the right-most node.
    PacketSinkHelper sink ("ns3::TcpSocketFactory",InetSocketAddress (Ipv4Address::GetAny (), port+i));
    sinkApps.Add (sink.Install (dumbbell.GetRight (0)));
    sinkApps.Start (Seconds (0.0));
    sinkApps.Stop  (Seconds (10.0));
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Ptr<NetDevice> nd = dumbbell.m_rightLeafDevices.Get(0);
  Ptr<PointToPointNetDevice> outgoingPort = DynamicCast<PointToPointNetDevice>(nd);
  Ptr<QueueBase> q = outgoingPort->GetQueue();
  q->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&queue_callback));

  p2pLeaf.EnablePcapAll ("Dumbbell", true);

  // Running simulation
  std::cout << "\nRuning simulation..." << std::endl;
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  std::cout << "\nSimulation finished!" << std::endl;

  std::vector<uint32_t> throughputs;
  uint32_t i = 0;
  for(ApplicationContainer::Iterator it = sinkApps.Begin(); it != sinkApps.End(); ++it)
    {
     Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(*it);
     uint32_t bytesRcvd = sink1->GetTotalRx ();
     uint32_t tp =bytesRcvd;
     throughputs.push_back(tp);
     std::cout << "Throughput: " << throughputs.back() << " bps" <<" at port: "<<port+i<<std::endl;
     ++i;
    }
 /*if (traceRTT) 
    {
     std::cout<< "95th percentile RTT: ";
     std::sort(rtt_records.begin(), rtt_records.end());
     std::cout<< rtt_records.at((int)(rtt_records.size() * 0.95)) << std::endl;
    }*/
    
    
  return 0;
}
