/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2020 NITK Surathkal
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
 * 
 * Author: Deepak Kumaraswamy <deepakkavoor99@gmail.com>
 */

#include <iomanip>
#include <iostream>
#include <string>
#include <fstream>
#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/test.h"
#include "ns3/pcap-file.h"
#include "ns3/config.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/data-rate.h"
#include "ns3/inet-socket-address.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/traffic-control-helper.h"
#include "ns3/simulator.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/pointer.h"
#include "ns3/queue.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ns3PragueSystemTest");

std::ofstream cwndStream;

class SourceApplication : public Application 
{
public:

  SourceApplication ();
  virtual ~SourceApplication();

  /**
   * Register this type.
   * \return The TypeId.
   */
  static TypeId GetTypeId (void);
  
  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

SourceApplication::SourceApplication ()
  : m_socket (0), 
    m_peer (), 
    m_packetSize (0), 
    m_nPackets (0), 
    m_dataRate (0), 
    m_sendEvent (), 
    m_running (false), 
    m_packetsSent (0)
{
}

SourceApplication::~SourceApplication()
{
  m_socket = 0;
}

/* static */
TypeId
SourceApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("SourceApplication")
    .SetParent<Application> ()
    .SetGroupName ("Stats")
    .AddConstructor<SourceApplication> ()
    ;
  return tid;
}
  
void
SourceApplication::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
SourceApplication::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void 
SourceApplication::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void 
SourceApplication::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void 
SourceApplication::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &SourceApplication::SendPacket, this);
    }
}


// ===========================================================================
// Test case for cwnd changes due to out-of-order packets. A bottleneck 
// link is created, and a limited droptail queue is used in order to 
// force dropped packets, resulting in out-of-order packet delivery. 
// This out-of-order delivery will result in a different congestion 
// window behavior than testcase 1.  Specifically, duplicate ACKs
// are encountered.
//
// Network topology
//
//        1Mb/s, 10ms      100kb/s, 10ms     1Mb/s, 10ms
//    n0--------------n1-----------------n2---------------n3
//
// ===========================================================================
class Ns3TcpPragueTestCase1 : public TestCase
{
public:
  Ns3TcpPragueTestCase1 ();
  virtual ~Ns3TcpPragueTestCase1 ();

private:
  virtual void DoRun (void);
  void VerifyCwndRun (uint32_t beginIdx, uint32_t endIdx, uint32_t initialCwnd, uint32_t mss);
  bool m_writeResults;

  class CwndEvent {
public:
    uint32_t m_oldCwnd;
    uint32_t m_newCwnd;
  };

  TestVectors<CwndEvent> m_responses;

  void ScheduleFirstTcpCwndTraceConnection (void);
  void CwndChange (uint32_t oldCwnd, uint32_t newCwnd);
};

Ns3TcpPragueTestCase1::Ns3TcpPragueTestCase1 ()
  : TestCase ("Check to see that the ns-3 TCP congestion window works as expected for out-of-order packet delivery"),
    m_writeResults (false)
{
}

Ns3TcpPragueTestCase1::~Ns3TcpPragueTestCase1 ()
{
}

void
Ns3TcpPragueTestCase1::CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
{
  CwndEvent event;

  event.m_oldCwnd = oldCwnd;
  event.m_newCwnd = newCwnd;

  m_responses.Add (event);
  if (newCwnd == 0)
    {
      std::cout << "Cwnd changed to zero" << std::endl;
    }
  NS_LOG_DEBUG ("Cwnd change event " << m_responses.GetN () << " at " << Now ().As (Time::S) << " " << oldCwnd << " " << newCwnd);
  uint32_t segmentSize = 1448;
  cwndStream << std::fixed << std::setprecision (6) << Simulator::Now ().GetSeconds () << std::setw (12) << newCwnd / segmentSize << std::endl;
}

void
Ns3TcpPragueTestCase1::ScheduleFirstTcpCwndTraceConnection (void)
{
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&Ns3TcpPragueTestCase1::CwndChange, this));
}

void
Ns3TcpPragueTestCase1::DoRun (void)
{ 
  NS_LOG_INFO ("Starting test case 1");

  // Default values for the simulation
  Time stopTime = Seconds (10);
  Time startTime = Seconds (5);
  Time baseRtt = MilliSeconds (80);
  Time marksSamplingInterval = MilliSeconds (100);
  Time throughputSamplingInterval = MilliSeconds (200);
  DataRate link3Rate ("100Mbps");
  double link5RateRatio = 0.95;
  DataRate firstServerDataRate ("1000Mbps");
  bool useEct0 = true;

  bool controlScenario = false;
  std::string firstTcpType = "prague";
  std::string m3QueueType = "fq";

  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize",UintegerValue (8192000));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize",UintegerValue (8192000));
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));
  Config::SetDefault ("ns3::TcpL4Protocol::RecoveryType", TypeIdValue (TcpPrrRecovery::GetTypeId ()));
  Config::SetDefault ("ns3::FifoQueueDisc::MaxSize", QueueSizeValue (QueueSize ("5000p")));
  Config::SetDefault ("ns3::FqCoDelQueueDisc::UseEcn", BooleanValue (true));
  Config::SetDefault ("ns3::FqCoDelQueueDisc::CeThreshold", TimeValue (MilliSeconds (1)));
  Config::SetDefault ("ns3::TcpPrague::UseEct0", BooleanValue (useEct0));

  Time oneWayDelay = baseRtt/2;

  TypeId firstTcpTypeId;
  if (firstTcpType == "prague")
    {
      firstTcpTypeId = TcpPrague::GetTypeId ();
    }
  else if (firstTcpType == "reno")
    {
      firstTcpTypeId = TcpLinuxReno::GetTypeId ();
    }
  else
    {
      NS_FATAL_ERROR ("Fatal error:  tcp unsupported");
    }
  TypeId m3QueueTypeId;
  if (m3QueueType == "fq")
    {
      m3QueueTypeId = FqCoDelQueueDisc::GetTypeId ();
    }
  else
    {
      NS_FATAL_ERROR ("Fatal error:  m3QueueType unsupported");
    }
  Config::SetDefault ("ns3::TcpSocketBase::UseEcn", StringValue ("On"));
  
  NS_LOG_INFO ("first TCP: " << firstTcpTypeId.GetName () << "; M3 queue: " << m3QueueTypeId.GetName () << "; control: " << controlScenario);

  Ptr<Node> firstServer = CreateObject<Node> ();
  Ptr<Node> wanRouter = CreateObject<Node> ();
  Ptr<Node> M1 = CreateObject<Node> ();
  Ptr<Node> M2 = CreateObject<Node> ();
  Ptr<Node> M3 = CreateObject<Node> ();
  Ptr<Node> lanRouter = CreateObject<Node> ();
  Ptr<Node> firstClient = CreateObject<Node> ();

  NetDeviceContainer firstServerDevices;
  NetDeviceContainer wanRouterM1Devices;
  NetDeviceContainer M1M2Devices;
  NetDeviceContainer M2M3Devices;
  NetDeviceContainer M3LanRouterDevices;
  NetDeviceContainer firstClientDevices;

  PointToPointHelper p2p;
  p2p.SetQueue ("ns3::DropTailQueue", "MaxSize", QueueSizeValue (QueueSize ("3p")));
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("1000Mbps")));
  p2p.SetChannelAttribute ("Delay", TimeValue (oneWayDelay));
  firstServerDevices = p2p.Install (firstServer, wanRouter);
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("1000Mbps")));
  p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (1)));
  wanRouterM1Devices = p2p.Install (wanRouter, M1);
  M1M2Devices = p2p.Install (M1, M2);
  M2M3Devices = p2p.Install (M2, M3);
  M3LanRouterDevices = p2p.Install (M3, lanRouter);
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("1000Mbps")));
  p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (1)));
  firstClientDevices = p2p.Install (lanRouter, firstClient);

  Ptr<PointToPointNetDevice> p = M3LanRouterDevices.Get (0)->GetObject<PointToPointNetDevice> ();
  DataRate link5Rate (link5RateRatio * link3Rate.GetBitRate ());
  p->SetAttribute ("DataRate", DataRateValue (link5Rate));

  if (!controlScenario)
    {
      p = M1M2Devices.Get (0)->GetObject<PointToPointNetDevice> ();
      p->SetAttribute ("DataRate", DataRateValue (link3Rate));
    }

  InternetStackHelper stackHelper;
  stackHelper.Install (firstServer);
  stackHelper.Install (wanRouter);
  stackHelper.Install (M1);
  stackHelper.Install (M2);
  stackHelper.Install (M3);
  stackHelper.Install (lanRouter);
  stackHelper.Install (firstClient);

  Ptr<TcpL4Protocol> protocol;
  protocol = firstClient->GetObject<TcpL4Protocol> ();
  protocol->SetAttribute ("SocketType", TypeIdValue (firstTcpTypeId));
  protocol = firstServer->GetObject<TcpL4Protocol> ();
  protocol->SetAttribute ("SocketType", TypeIdValue (firstTcpTypeId));

  TrafficControlHelper tchFq;
  tchFq.SetRootQueueDisc ("ns3::FqCoDelQueueDisc");
  tchFq.SetQueueLimits ("ns3::DynamicQueueLimits", "HoldTime", StringValue ("1ms"));
  tchFq.Install (firstServerDevices);
  tchFq.Install (wanRouterM1Devices);
  tchFq.Install (M1M2Devices.Get (1));
  tchFq.Install (M2M3Devices);
  tchFq.Install (M3LanRouterDevices.Get (1));
  tchFq.Install (firstClientDevices);

  TrafficControlHelper tchM1;
  tchM1.SetRootQueueDisc ("ns3::FifoQueueDisc");
  tchM1.SetQueueLimits ("ns3::DynamicQueueLimits", "HoldTime", StringValue ("1ms"));
  tchM1.Install (M1M2Devices.Get (0));
  TrafficControlHelper tchM3;
  tchM3.SetRootQueueDisc (m3QueueTypeId.GetName ());
  tchM3.SetQueueLimits ("ns3::DynamicQueueLimits", "HoldTime", StringValue ("1ms"));
  tchM3.Install (M3LanRouterDevices.Get (0));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer firstServerIfaces = ipv4.Assign (firstServerDevices);
  ipv4.SetBase ("172.16.1.0", "255.255.255.0");
  Ipv4InterfaceContainer wanRouterM1Ifaces = ipv4.Assign (wanRouterM1Devices);
  ipv4.SetBase ("172.16.2.0", "255.255.255.0");
  Ipv4InterfaceContainer M1M2Ifaces = ipv4.Assign (M1M2Devices);
  ipv4.SetBase ("172.16.3.0", "255.255.255.0");
  Ipv4InterfaceContainer M2M3Ifaces = ipv4.Assign (M2M3Devices);
  ipv4.SetBase ("172.16.4.0", "255.255.255.0");
  Ipv4InterfaceContainer M3LanRouterIfaces = ipv4.Assign (M3LanRouterDevices);
  ipv4.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer firstClientIfaces = ipv4.Assign (firstClientDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  BulkSendHelper tcp ("ns3::TcpSocketFactory", Address ());
  // set to large value:  e.g. 1000 Mb/s for 60 seconds = 7500000000 bytes
  tcp.SetAttribute ("MaxBytes", UintegerValue (7500000000));
  uint16_t firstPort = 5000;
  ApplicationContainer firstApp;
  InetSocketAddress firstDestAddress (firstClientIfaces.GetAddress (1), firstPort);
  tcp.SetAttribute ("Remote", AddressValue (firstDestAddress));
  firstApp = tcp.Install (firstServer);
  firstApp.Start (startTime);
  firstApp.Stop (stopTime - MicroSeconds (100));

  Address firstSinkAddress (InetSocketAddress (Ipv4Address::GetAny (), firstPort));
  PacketSinkHelper firstSinkHelper ("ns3::TcpSocketFactory", firstSinkAddress);
  ApplicationContainer firstSinkApp;
  firstSinkApp = firstSinkHelper.Install (firstClient);
  firstSinkApp.Start (startTime);
  firstSinkApp.Stop (stopTime);

  if (m_writeResults)
    {
      PointToPointHelper pointToPoint;
      pointToPoint.EnablePcapAll ("tcp-prague-system-test");
    }

  Simulator::Schedule (startTime + MicroSeconds (10), &Ns3TcpPragueTestCase1::ScheduleFirstTcpCwndTraceConnection, this);
  cwndStream.open ("ns3-tcp-prague-system-test-cwnd.dat", std::ios::out);
  cwndStream << "#Time(s) Congestion Window (B)" << std::endl;

  Simulator::Stop (stopTime);
  Simulator::Run ();

  cwndStream.close ();
  std::string gnuplotFile = "cwnd-plot.plt";
  system (("gnuplot " + gnuplotFile).c_str ());

  // //
  // // As new acks are received by the TCP under test, the congestion window 
  // // should be opened up by one segment (MSS bytes) each time.  This should
  // // trigger a congestion window change event which we hooked and saved above.
  // // We should now be able to look through the saved response vectors and follow
  // // the congestion window as it opens up when the ns-3 TCP under test 
  // // transmits its bits
  // //
  // // From inspecting the results, we know that we should see N_EVENTS congestion
  // // window change events. On the tenth change event, the window should 
  // // be cut from 5360 to 4288 due to 3 dup acks (NewReno behavior is to
  // // cut in half, and then add 3 segments (5360/2 + 3*536 = 4288)
  // //
  
  // const uint32_t MSS = 536;
  // const uint32_t N_EVENTS = 38;

  // CwndEvent event;

  // NS_LOG_DEBUG ("Number of response events: " << m_responses.GetN ());
  // //NS_TEST_ASSERT_MSG_EQ (m_responses.GetN (), N_EVENTS, "Unexpected number of cwnd change events");

  // // Ignore the first event logged (i=0) when m_cWnd goes from 0 to MSS bytes
  // VerifyCwndRun (1, 10, 2 * MSS, MSS);
  
  // // At the point of loss, sndNxt = 15545; sndUna = 9113.  FlightSize is 4824, 
  // // so there are 9 segments outstanding.  Cut ssthresh to 9/2 (2412) and 
  // // cwnd to (9/2 + 3) = 4020
  // event = m_responses.Get (10);
  // NS_TEST_ASSERT_MSG_EQ (event.m_newCwnd, (MSS * 15)/2, "Wrong new cwnd value in cwnd change event " << 10);

  // // Verify that cwnd increments by one for a few segments
  // // from 8.5 at index 11 to 12.5 at index 15
  // VerifyCwndRun (11, 15, (MSS * 17)/2, MSS);

  // // partial ack at event 16, cwnd reset from 6700 (12.5*MSS) to 5092 (9.5*MSS)
  // NS_TEST_ASSERT_MSG_EQ (m_responses.Get (16).m_newCwnd, (MSS * 19)/2, "Wrong new cwnd value in cwnd change event " << 16);

  // // partial ack again of 3 segments after one more acks, cwnd reset to 7.5 
  // NS_TEST_ASSERT_MSG_EQ (m_responses.Get (18).m_newCwnd, (MSS * 15)/2, "Wrong new cwnd value in cwnd change event " << 18);

  // //DUP ACKS in remaining fast recovery
  // VerifyCwndRun (18, 20, (MSS * 15)/2, MSS);

  // // Process another partial ack
  // VerifyCwndRun (21, 24, (MSS * 13)/2, MSS);
 
  // // Leaving fast recovery at event 25; set cwnd to 4 segments as per RFC 2582
  // // Sec. 3 para. 5 option 2 (sshthresh is 2412, 4.5 times MSS)
  // NS_TEST_ASSERT_MSG_EQ (m_responses.Get (25).m_newCwnd,  2412, "Wrong new cwnd value in cwnd change event " << 25);
  
  // //In CongAvoid each event will increase cwnd by (MSS * MSS / cwnd)
  // uint32_t cwnd = 2412;
  // for (uint32_t i = 26; i < N_EVENTS; ++i)
  //   {
  //     double adder = static_cast<double> (MSS * MSS) / cwnd;
  //     adder = std::max (1.0, adder);
  //     cwnd += static_cast<uint32_t> (adder);    
  //     NS_TEST_ASSERT_MSG_EQ (m_responses.Get (i).m_newCwnd, cwnd, "Wrong new cwnd value in cwnd change event " << i); 
  //   }
  // NS_LOG_DEBUG ("Reading out the cwnd event log");  
  // for (uint32_t i = 0; i < N_EVENTS; ++i)
  // {
  //   NS_LOG_DEBUG ("i: " << i << " newCwnd: " << m_responses.Get(i).m_newCwnd << " newCwnd segments " << static_cast<double> (m_responses.Get(i).m_newCwnd)/MSS);
  // }
  Simulator::Destroy ();
}

void 
Ns3TcpPragueTestCase1::VerifyCwndRun (uint32_t beginIdx, uint32_t endIdx, uint32_t initialCwnd, uint32_t mss)
{

  CwndEvent event;
  
  for(uint32_t i = beginIdx, to = initialCwnd; i < endIdx; ++i, to += mss)
    {
      event = m_responses.Get (i);
      NS_TEST_ASSERT_MSG_EQ (event.m_newCwnd, to, "Wrong new cwnd value in cwnd change event " << i);      
    }
}

class Ns3PragueTestSuite : public TestSuite
{
public:
  Ns3PragueTestSuite ();
};

Ns3PragueTestSuite::Ns3PragueTestSuite ()
  : TestSuite ("ns3-tcp-prague", SYSTEM)
{
  AddTestCase (new Ns3TcpPragueTestCase1, TestCase::QUICK);
}

Ns3PragueTestSuite ns3PragueTestSuite;
