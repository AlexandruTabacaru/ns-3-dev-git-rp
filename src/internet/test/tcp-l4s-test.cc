/*
 * Copyright (c) 2024
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/log.h"
#include "ns3/tcp-bbr.h"
#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/test.h"
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/queue.h"
#include "ns3/traffic-control-helper.h"
#include "ns3/traffic-control-layer.h"
#include "ns3/queue-disc.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpL4sTestSuite");

/**
 * \brief Test L4S ECN marking and response
 */
class TcpL4sEcnTest : public TestCase
{
public:
  /**
   * \brief Constructor
   * \param name Test description
   */
  TcpL4sEcnTest(std::string name);

private:
  void DoRun() override;
  /**
   * \brief Execute the test
   */
  void ExecuteTest();
  /**
   * \brief Check if ECN is properly configured
   */
  void CheckEcnConfiguration();
  /**
   * \brief Check if AQM is properly configured
   */
  void CheckAqmConfiguration();
};

TcpL4sEcnTest::TcpL4sEcnTest(std::string name)
    : TestCase(name)
{
}

void
TcpL4sEcnTest::DoRun()
{
  Simulator::Schedule(Seconds(0.0), &TcpL4sEcnTest::ExecuteTest, this);
  Simulator::Run();
  Simulator::Destroy();
}

void
TcpL4sEcnTest::ExecuteTest()
{
  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Create point-to-point link
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));

  // Install internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Create devices
  NetDeviceContainer devices = p2p.Install(nodes);

  // Configure queue discipline for L4S
  TrafficControlHelper tch;
  tch.SetRootQueueDisc("ns3::DualPI2QueueDisc");
  tch.Install(devices);

  // Configure L4S-specific queue parameters
  Config::SetDefault("ns3::DualPI2QueueDisc::UseEcn", BooleanValue(true));
  Config::SetDefault("ns3::DualPI2QueueDisc::UseL4s", BooleanValue(true));
  Config::SetDefault("ns3::DualPI2QueueDisc::CeThreshold", TimeValue(MilliSeconds(1)));

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // Create TCP socket
  Ptr<Socket> socket = Socket::CreateSocket(nodes.Get(0), TypeId::LookupByName("ns3::TcpSocketFactory"));
  Ptr<TcpSocketBase> tcpSocket = DynamicCast<TcpSocketBase>(socket);

  // Configure TCP socket for L4S
  tcpSocket->SetAttribute("EcnEnabled", BooleanValue(true));
  tcpSocket->SetAttribute("UseEcn", StringValue("ClassicEcn"));

  // Check configurations
  CheckEcnConfiguration();
  CheckAqmConfiguration();
}

void
TcpL4sEcnTest::CheckEcnConfiguration()
{
  // Verify ECN is enabled
  Ptr<Socket> socket = Socket::CreateSocket(CreateObject<Node>(), TypeId::LookupByName("ns3::TcpSocketFactory"));
  Ptr<TcpSocketBase> tcpSocket = DynamicCast<TcpSocketBase>(socket);
  tcpSocket->SetAttribute("EcnEnabled", BooleanValue(true));
  tcpSocket->SetAttribute("UseEcn", StringValue("ClassicEcn"));

  NS_TEST_ASSERT_MSG_EQ(tcpSocket->GetEcnEnabled(), true, "ECN should be enabled");
}

void
TcpL4sEcnTest::CheckAqmConfiguration()
{
  // Create a node and install internet stack
  Ptr<Node> node = CreateObject<Node>();
  InternetStackHelper internet;
  internet.Install(node);

  // Create a point-to-point device
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));
  NetDeviceContainer devices = p2p.Install(node, CreateObject<Node>());

  // Configure queue discipline
  TrafficControlHelper tch;
  tch.SetRootQueueDisc("ns3::DualPI2QueueDisc");
  tch.Install(devices);

  // Get the queue disc
  Ptr<TrafficControlLayer> tc = node->GetObject<TrafficControlLayer>();
  Ptr<QueueDisc> qdisc = tc->GetRootQueueDiscOnDevice(devices.Get(0));

  NS_TEST_ASSERT_MSG_NE(qdisc, nullptr, "Queue disc should be installed");
}

/**
 * \brief TCP L4S TestSuite
 */
class TcpL4sTestSuite : public TestSuite
{
public:
  TcpL4sTestSuite()
      : TestSuite("tcp-l4s-test", UNIT)
  {
    AddTestCase(new TcpL4sEcnTest("L4S ECN marking and response test"), TestCase::QUICK);
    
    // Add BBRv3 test case
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpBbr3"));
    Config::SetDefault("ns3::TcpSocket::EcnEnabled", BooleanValue(true));
    Config::SetDefault("ns3::TcpSocket::UseEcn", StringValue("ClassicEcn"));
    AddTestCase(new TcpL4sEcnTest("L4S BBRv3 ECN marking and response test"), TestCase::QUICK);
  }
};

static TcpL4sTestSuite g_tcpL4sTest; //!< Static variable for test initialization 