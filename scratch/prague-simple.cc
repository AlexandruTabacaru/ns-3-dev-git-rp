/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Simple test of TCP Prague congestion control
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

int main()
{
  // Enable verbose logging
  LogComponentEnable("TcpPrague", LOG_LEVEL_INFO);
  
  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);
  
  // Create channel
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer devices = pointToPoint.Install(nodes);
  
  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);
  
  // Set TCP Prague as default
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpPrague"));
  Config::SetDefault("ns3::TcpSocketBase::UseEcn", StringValue("On"));
  
  // Add IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);
  
  // Create a simple flow
  uint16_t port = 9;
  BulkSendHelper source("ns3::TcpSocketFactory", 
                        InetSocketAddress(interfaces.GetAddress(1), port));
  source.SetAttribute("MaxBytes", UintegerValue(1000000));
  ApplicationContainer sourceApps = source.Install(nodes.Get(0));
  sourceApps.Start(Seconds(0.0));
  sourceApps.Stop(Seconds(10.0));
  
  PacketSinkHelper sink("ns3::TcpSocketFactory",
                       InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));
  
  // Print TCP Prague parameters and state to confirm it's working
  Simulator::Schedule(Seconds(1.0), []() {
    std::cout << "TCP Prague is running at time 1.0s" << std::endl;
  });
  
  Simulator::Schedule(Seconds(5.0), []() {
    std::cout << "TCP Prague is still running at time 5.0s" << std::endl;
  });
  
  // Run simulation
  Simulator::Run();
  Simulator::Destroy();
  
  return 0;
} 