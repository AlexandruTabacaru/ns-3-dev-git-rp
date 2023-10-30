/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2023 CableLabs (change to L4s over Wi-Fi scenario)
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

// Nodes 0                     Node 1                           Nodes 2+
//
// server ---------------------> AP -------------------------- > STA * N clients
//         1 Gbps
//         20 ms base RTT            BW 20/80/160 MHz            # N/2 for L4S flows
//                                   Fixed MCS                   # N/2 for classic flows
//
// One server with Prague and Cubic TCP connections to the STA under test
// The first Wi-Fi STA (node index 2) is the STA under test
// Additional STA nodes (node indices 3+) for sending background load
// 80 MHz 11ac (MCS 8) is initially configured in 5 GHz (channel 42)
//
// Configuration inputs:
// - number of Cubic flows under test
// - number of Prague flows under test
// - number of background flows
// - number of bytes for TCP flows
// - whether to disable flow control
// - Wi-Fi queue limit when flow control is enabled (base limit and scale factor)
//
// Benavior:
// - at simulation time 1 second, each flow starts
// - simulation ends 1 second after last foreground flow terminates, unless
//   a specific duration was configured
//
// Outputs (some of these are for future definition):
// 1) one-way latency sample for each flow measured at application layer
// 2) queueing and medium access delay
// 3) PCAP files at TCP endpoints
// 4) PCAP files on Wifi endpoints
// 5) queue depth of the overlying and Wi-Fi AC_BE queue
// 6) throughputs of flows and of Wi-Fi downlink
// 7) Wi-Fi MCS for data

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/stats.h"
#include "ns3/traffic-control-module.h"
#include "ns3/wifi-module.h"

#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("L4sWifi");

// Declare trace functions that are defined later in this file
std::ofstream g_fileBytesInAcBeQueue;
void TraceBytesInAcBeQueue(uint32_t oldVal, uint32_t newVal);
std::ofstream g_fileBytesInDualPi2Queue;
void TraceBytesInDualPi2Queue(uint32_t oldVal, uint32_t newVal);

uint32_t g_dequeuedData = 0;
std::ofstream g_fileDequeue;
void TraceDequeue(Ptr<const WifiMpdu> mpdu);

std::ofstream g_fileDequeueThroughput;
Time g_dequeueThroughputInterval = MilliSeconds(100);
void TraceDequeueThroughput(void);

uint32_t g_pragueData = 0;
std::ofstream g_filePragueThroughput;
std::ofstream g_filePragueLatency;
Time g_pragueThroughputInterval = MilliSeconds(100);
void TracePragueData(Ptr<const Packet> p,
                     const Address& from [[maybe_unused]],
                     const Address& to [[maybe_unused]],
                     const SeqTsSizeHeader& header);
void TracePragueThroughput(void);

uint32_t g_cubicData = 0;
std::ofstream g_fileCubicThroughput;
std::ofstream g_fileCubicLatency;
Time g_cubicThroughputInterval = MilliSeconds(100);
void TraceCubicData(Ptr<const Packet> p,
                    const Address& from [[maybe_unused]],
                    const Address& to [[maybe_unused]],
                    const SeqTsSizeHeader& header);
void TraceCubicThroughput(void);

// Count the number of flows to wait for completion before stopping the simulation
uint32_t g_flowsToClose = 0;
// Hook these methods to the PacketSink objects
void HandlePeerClose(Ptr<const Socket> socket);
void HandlePeerError(Ptr<const Socket> socket);

// These methods work around the lack of ability to configure different TCP socket types
// on the same node on a per-socket (per-application) basis. Instead, these methods can
// be scheduled (right before a socket creation) to change the default value
void ConfigurePragueSockets(Ptr<TcpL4Protocol> tcp1, Ptr<TcpL4Protocol> tcp2);
void ConfigureCubicSockets(Ptr<TcpL4Protocol> tcp1, Ptr<TcpL4Protocol> tcp2);

// Declare some statistics counters here so that they are updated in traces
MinMaxAvgTotalCalculator<double> pragueLatencyCalculator;      // units of ms
MinMaxAvgTotalCalculator<uint32_t> pragueThroughputCalculator; // units of Mbps
MinMaxAvgTotalCalculator<double> cubicLatencyCalculator;       // units of ms
MinMaxAvgTotalCalculator<uint32_t> cubicThroughputCalculator;  // units of Mbps

int
main(int argc, char* argv[])
{
    // Variable declaration, and constants
    std::string wifiControlMode = "OfdmRate24Mbps";
    std::string delay = "10ms"; // base RTT is 20ms
    double staDistance = 10;    // meters
    const double pi = 3.1415927;
    Time progressInterval = Seconds(5);

    // Variables that can be changed by command-line argument
    uint32_t numCubic = 1;
    uint32_t numPrague = 1;
    uint32_t numBackground = 0;
    uint32_t numBytes = 10e6;   // default 10 MB
    Time duration = Seconds(0); // By default, close one second after last TCP flow closes
    uint16_t mcs = 2;
    bool flowControl = true;
    uint32_t limit = 65535; // default flow control limit (max A-MPDU size in bytes)
    double scale = 1.0;     // default flow control scale factor
    bool showProgress = false;

    // Increase some defaults (command-line can override below)
    // ns-3 TCP does not automatically adjust MSS from the device MTU
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    // ns-3 TCP socket buffer sizes do not dynamically grow, so set to tcp_rmem max value
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(6291456));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(6291456));
    // Enable a timestamp (for latency sampling) in the bulk send application
    Config::SetDefault("ns3::BulkSendApplication::EnableSeqTsSizeHeader", BooleanValue(true));
    Config::SetDefault("ns3::PacketSink::EnableSeqTsSizeHeader", BooleanValue(true));
    // The bulk send application should do 1448-byte writes (one timestamp per TCP packet)
    Config::SetDefault("ns3::BulkSendApplication::SendSize", UintegerValue(1448));

    CommandLine cmd;
    cmd.Usage("The l4s-wifi program experiments with TCP flows over L4S Wi-Fi configuration");
    cmd.AddValue("numCubic", "Number of foreground Cubic flows", numCubic);
    cmd.AddValue("numPrague", "Number of foreground Prague flows", numPrague);
    cmd.AddValue("numBackground", "Number of background flows", numBackground);
    cmd.AddValue("numBytes", "Number of bytes for each TCP transfer", numBytes);
    cmd.AddValue("duration", "(optional) scheduled end of simulation", duration);
    cmd.AddValue("mcs", "Index (0-11) of 11ax HE MCS", mcs);
    cmd.AddValue("flowControl", "Whether to enable flow control (set also the limit)", flowControl);
    cmd.AddValue("limit", "Queue limit (bytes)", limit);
    cmd.AddValue("scale", "Scaling factor for queue limit", scale);
    cmd.AddValue("showProgress", "Show simulation progress every 5s", showProgress);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_UNLESS(mcs < 12, "Only MCS 0-11 supported");
    NS_ABORT_MSG_IF(numCubic == 0 && numPrague == 0,
                    "Error: configure at least one foreground flow");
    std::ostringstream ossDataMode;
    ossDataMode << "HeMcs" << mcs;

    // When using DCE with ns-3, or reading pcaps with Wireshark,
    // enable checksum computations in ns-3 models
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    // Create the nodes and use containers for further configuration below
    NodeContainer serverNode;
    serverNode.Create(1);
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(1 + numBackground);

    // Create point-to-point links between server and AP
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));
    NetDeviceContainer wanDevices = pointToPoint.Install(serverNode.Get(0), apNode.Get(0));

    // Wifi configuration; use the simpler Yans physical layer model
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    // Reference Loss for Friss at 1 m with 5.15 GHz
    wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                                   "Exponent",
                                   DoubleValue(2.0),
                                   "ReferenceDistance",
                                   DoubleValue(1.0),
                                   "ReferenceLoss",
                                   DoubleValue(46.6777));
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue(ossDataMode.str()),
                                 "ControlMode",
                                 StringValue(wifiControlMode));
    // Set guard interval and MPDU buffer size
    wifi.ConfigHeOptions("GuardInterval",
                         TimeValue(NanoSeconds(800)),
                         "MpduBufferSize",
                         UintegerValue(64));

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("l4s")));
    NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, apNode);

    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("l4s")));
    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

    // Set positions
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    // Set postion for AP
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // X,Y,Z cartesian

    // Set position for STAs; simple routine to distribute around a ring of distance 'staDistance'
    double angle = (static_cast<double>(360) / (staNodes.GetN()));
    for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
        positionAlloc->Add(Vector(staDistance * cos((i * angle * pi) / 180),
                                  staDistance * sin((i * angle * pi) / 180),
                                  0.0));
    }

    // Create some additional container objects to simplify the below configuration
    NodeContainer wifiNodes;
    wifiNodes.Add(apNode);
    wifiNodes.Add(staNodes);
    NetDeviceContainer wifiDevices;
    wifiDevices.Add(apDevice);
    wifiDevices.Add(staDevices);

    // Add Mobility (position objects) to the Wi-Fi nodes, for propagation
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(wifiNodes);

    // Internet and Linux stack installation
    InternetStackHelper internetStack;
    internetStack.Install(serverNode);
    internetStack.Install(apNode);
    internetStack.Install(staNodes);

    // By default, Ipv4AddressHelper below will configure a MqQueueDisc
    // with FqCoDelQueueDisc as child queue discs (one per AC)
    // The following statements change this configuration on the AP to
    // an MqQueueDisc with a DualPi2QueueDisc as child queue disc
    TrafficControlHelper tch;
    uint16_t handle = tch.SetRootQueueDisc("ns3::MqQueueDisc");
    TrafficControlHelper::ClassIdList cls =
        tch.AddQueueDiscClasses(handle, 4, "ns3::QueueDiscClass");
    tch.AddChildQueueDiscs(handle, cls, "ns3::DualPi2QueueDisc");

    // The next statements configure flow control between Wi-Fi and DualPi2
    if (flowControl)
    {
        tch.SetQueueLimits("ns3::DynamicQueueLimits",
                           "HoldTime",
                           StringValue("500ms"),
                           "MinLimit",
                           UintegerValue(static_cast<uint32_t>(scale * limit)),
                           "MaxLimit",
                           UintegerValue(static_cast<uint32_t>(scale * limit)));
    }
    else
    {
        // Leave a very small queue at the AQM layer
        Config::SetDefault("ns3::DualPi2QueueDisc::QueueLimit", UintegerValue(1500));
    }
    // Install the traffic control configuration on the AP Wi-Fi device
    // and on STA devices
    QueueDiscContainer apQueueDiscContainer = tch.Install(apDevice);
    QueueDiscContainer staQueueDiscContainer = tch.Install(staDevices);

    // Configure IP addresses for all links
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(wanDevices);
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces = address.Assign(wifiDevices);

    // Use a helper to add static routes
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Get pointers to the TcpL4Protocol instances of the primary nodes
    Ptr<TcpL4Protocol> tcpL4ProtocolServer = serverNode.Get(0)->GetObject<TcpL4Protocol>();
    Ptr<TcpL4Protocol> tcpL4ProtocolSta = staNodes.Get(0)->GetObject<TcpL4Protocol>();
    // Set the TCP type for the TCP under test

    // Application configuration for Prague flow under test
    uint16_t port = 100;
    ApplicationContainer serverApp;
    ApplicationContainer clientApp;
    for (auto i = 0u; i < numPrague; i++)
    {
        BulkSendHelper bulk("ns3::TcpSocketFactory",
                            InetSocketAddress(wifiInterfaces.GetAddress(1), port + i));
        bulk.SetAttribute("MaxBytes", UintegerValue(numBytes));
        serverApp = bulk.Install(serverNode.Get(0));
        serverApp.Start(Seconds(1.0));
        PacketSinkHelper sink =
            PacketSinkHelper("ns3::TcpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), port + i));
        clientApp = sink.Install(staNodes.Get(0));
        clientApp.Start(Seconds(1.0));
        g_flowsToClose++;
        Simulator::Schedule(
            Seconds(1.0) - TimeStep(1),
            MakeBoundCallback(&ConfigurePragueSockets, tcpL4ProtocolServer, tcpL4ProtocolSta));
    }

    // Application configuration for background flows
    port = 200;
    ApplicationContainer serverAppCubic;
    ApplicationContainer clientAppCubic;
    for (auto i = 0u; i < numCubic; i++)
    {
        BulkSendHelper bulkCubic("ns3::TcpSocketFactory",
                                 InetSocketAddress(wifiInterfaces.GetAddress(1), port + i));
        bulkCubic.SetAttribute("MaxBytes", UintegerValue(numBytes));
        serverAppCubic = bulkCubic.Install(serverNode.Get(0));
        serverAppCubic.Start(Seconds(1.05));
        PacketSinkHelper sinkCubic =
            PacketSinkHelper("ns3::TcpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), port + i));
        clientAppCubic = sinkCubic.Install(staNodes.Get(0));
        clientAppCubic.Start(Seconds(1.05));
        g_flowsToClose++;
        Simulator::Schedule(
            Seconds(1.05) - TimeStep(1),
            MakeBoundCallback(&ConfigureCubicSockets, tcpL4ProtocolServer, tcpL4ProtocolSta));
    }

    // Add a cubic application on the server for each background flow
    // Send the traffic from a different STA.
    port = 300;
    Simulator::Schedule(
        Seconds(1.1) - TimeStep(1),
        MakeBoundCallback(&ConfigureCubicSockets, tcpL4ProtocolServer, tcpL4ProtocolSta));
    for (auto i = 0u; i < numBackground; i++)
    {
        ApplicationContainer serverAppBackground;
        BulkSendHelper bulkBackground("ns3::TcpSocketFactory",
                                      InetSocketAddress(interfaces1.GetAddress(0), port + i));
        bulkBackground.SetAttribute("MaxBytes", UintegerValue(numBytes));
        serverAppBackground = bulkBackground.Install(staNodes.Get(1 + i));
        serverAppBackground.Start(Seconds(1.1));
        ApplicationContainer clientAppBackground;
        PacketSinkHelper sinkBackground =
            PacketSinkHelper("ns3::TcpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), port + i));
        clientAppBackground = sinkBackground.Install(serverNode.Get(0));
        clientAppBackground.Start(Seconds(1.1));
    }

    // Control the random variable stream assignments for Wi-Fi models (the value 100 is arbitrary)
    wifi.AssignStreams(wifiDevices, 100);

    // PCAP traces
    pointToPoint.EnablePcapAll("l4s-wifi");
    wifiPhy.EnablePcap("l4s-wifi", wifiDevices);

    // Set up traces
    // Bytes and throughput in WifiMacQueue
    g_fileBytesInAcBeQueue.open("wifi-queue-bytes.dat", std::ofstream::out);
    apDevice.Get(0)
        ->GetObject<WifiNetDevice>()
        ->GetMac()
        ->GetTxopQueue(AC_BE)
        ->TraceConnectWithoutContext("BytesInQueue", MakeCallback(&TraceBytesInAcBeQueue));
    g_fileDequeue.open("wifi-dequeue-events.dat", std::ofstream::out);
    apDevice.Get(0)
        ->GetObject<WifiNetDevice>()
        ->GetMac()
        ->GetTxopQueue(AC_BE)
        ->TraceConnectWithoutContext("Dequeue", MakeCallback(&TraceDequeue));
    g_fileDequeueThroughput.open("wifi-dequeue-throughput.dat", std::ofstream::out);
    Simulator::Schedule(g_dequeueThroughputInterval, &TraceDequeueThroughput);

    // Throughput and latency for foreground flows, and set up close callbacks
    for (auto i = 0u; i < clientApp.GetN(); i++)
    {
        clientApp.Get(i)->GetObject<PacketSink>()->TraceConnectWithoutContext(
            "RxWithSeqTsSize",
            MakeCallback(&TracePragueData));
        g_filePragueThroughput.open("wifi-prague-throughput.dat", std::ofstream::out);
        g_filePragueLatency.open("wifi-prague-latency.dat", std::ofstream::out);
        Simulator::Schedule(g_pragueThroughputInterval, &TracePragueThroughput);
        clientApp.Get(i)->GetObject<PacketSink>()->TraceConnectWithoutContext(
            "PeerClose",
            MakeCallback(&HandlePeerClose));
        clientApp.Get(i)->GetObject<PacketSink>()->TraceConnectWithoutContext(
            "PeerError",
            MakeCallback(&HandlePeerError));
    }

    for (auto i = 0u; i < clientAppCubic.GetN(); i++)
    {
        clientAppCubic.Get(0)->GetObject<PacketSink>()->TraceConnectWithoutContext(
            "RxWithSeqTsSize",
            MakeCallback(&TraceCubicData));
        g_fileCubicThroughput.open("wifi-cubic-throughput.dat", std::ofstream::out);
        g_fileCubicLatency.open("wifi-cubic-latency.dat", std::ofstream::out);
        Simulator::Schedule(g_cubicThroughputInterval, &TraceCubicThroughput);
        clientAppCubic.Get(i)->GetObject<PacketSink>()->TraceConnectWithoutContext(
            "PeerClose",
            MakeCallback(&HandlePeerClose));
        clientAppCubic.Get(i)->GetObject<PacketSink>()->TraceConnectWithoutContext(
            "PeerError",
            MakeCallback(&HandlePeerError));
    }

    // Trace bytes in DualPi2 queue
    g_fileBytesInDualPi2Queue.open("wifi-dualpi2-bytes.dat", std::ofstream::out);
    apQueueDiscContainer.Get(0)->GetQueueDiscClass(0)->GetQueueDisc()->TraceConnectWithoutContext(
        "BytesInQueue",
        MakeCallback(&TraceBytesInDualPi2Queue));

    if (duration > Seconds(0))
    {
        Simulator::Stop(duration);
    }
    else
    {
        // Keep the simulator from running forever in case Stop() is not triggered.
        // However, the simulation should stop on the basis of the close callbacks.
        Simulator::Stop(Seconds(1000));
    }
    std::cout << "Foreground flows: Cubic: " << numCubic << " Prague: " << numPrague << std::endl;
    std::cout << "Background flows: " << numBackground << std::endl;
    if (showProgress)
    {
        std::cout << std::endl;
        // Keep progress object in scope of the Run() method
        ShowProgress progress(progressInterval);
        Simulator::Run();
    }
    else
    {
        Simulator::Run();
    }

    std::string stopReason = "automatic";
    if (duration == Seconds(0) && Simulator::Now() >= Seconds(1000))
    {
        stopReason = "fail-safe";
    }
    else if (duration > Seconds(0))
    {
        stopReason = "scheduled";
    }
    std::cout << std::endl
              << "Reached simulation " << stopReason << " stop time after "
              << Simulator::Now().GetSeconds() << " seconds" << std::endl
              << std::endl;

    std::cout << std::fixed << std::setprecision(2);
    if (numCubic)
    {
        std::cout << "Cubic throughput (Mbps) mean: " << cubicThroughputCalculator.getMean()
                  << " max: " << cubicThroughputCalculator.getMax()
                  << " min: " << cubicThroughputCalculator.getMin() << std::endl;
        std::cout << "Cubic latency (ms) mean: " << cubicLatencyCalculator.getMean()
                  << " max: " << cubicLatencyCalculator.getMax()
                  << " min: " << cubicLatencyCalculator.getMin() << std::endl;
    }
    if (numPrague)
    {
        std::cout << "Prague throughput (Mbps) mean: " << pragueThroughputCalculator.getMean()
                  << " max: " << pragueThroughputCalculator.getMax()
                  << " min: " << pragueThroughputCalculator.getMin() << std::endl;
        std::cout << "Prague latency (ms) mean: " << pragueLatencyCalculator.getMean()
                  << " max: " << pragueLatencyCalculator.getMax()
                  << " min: " << pragueLatencyCalculator.getMin() << std::endl;
    }

    g_fileBytesInAcBeQueue.close();
    g_fileBytesInDualPi2Queue.close();
    g_fileDequeue.close();
    g_fileDequeueThroughput.close();
    g_filePragueThroughput.close();
    g_filePragueLatency.close();
    g_fileCubicThroughput.close();
    g_fileCubicLatency.close();
    Simulator::Destroy();
    return 0;
}

void
ConfigurePragueSockets(Ptr<TcpL4Protocol> tcp1, Ptr<TcpL4Protocol> tcp2)
{
    tcp1->SetAttribute("SocketType", TypeIdValue(TcpPrague::GetTypeId()));
    tcp2->SetAttribute("SocketType", TypeIdValue(TcpPrague::GetTypeId()));
}

void
ConfigureCubicSockets(Ptr<TcpL4Protocol> tcp1, Ptr<TcpL4Protocol> tcp2)
{
    tcp1->SetAttribute("SocketType", TypeIdValue(TcpCubic::GetTypeId()));
    tcp2->SetAttribute("SocketType", TypeIdValue(TcpCubic::GetTypeId()));
}

void
TraceBytesInDualPi2Queue(uint32_t oldVal, uint32_t newVal)
{
    g_fileBytesInDualPi2Queue << Now().GetSeconds() << " " << newVal << std::endl;
}

void
TraceBytesInAcBeQueue(uint32_t oldVal, uint32_t newVal)
{
    g_fileBytesInAcBeQueue << Now().GetSeconds() << " " << newVal << std::endl;
}

void
TraceDequeue(Ptr<const WifiMpdu> mpdu)
{
    if (mpdu->GetHeader().GetType() == WIFI_MAC_QOSDATA)
    {
        g_dequeuedData += mpdu->GetPacket()->GetSize();
        g_fileDequeue << Now().GetSeconds() << " " << mpdu->GetPacket()->GetSize() << " "
                      << mpdu->GetHeader() << std::endl;
    }
}

void
TracePragueData(Ptr<const Packet> p,
                const Address& from [[maybe_unused]],
                const Address& to [[maybe_unused]],
                const SeqTsSizeHeader& header)
{
    pragueLatencyCalculator.Update((Now().GetSeconds() - header.GetTs().GetSeconds()) * 1000);
    g_filePragueLatency << Now().GetSeconds() << " " << std::fixed
                        << (Now().GetSeconds() - header.GetTs().GetSeconds()) * 1000 << std::endl;
    g_pragueData += p->GetSize();
}

void
TracePragueThroughput(void)
{
    g_filePragueThroughput << Now().GetSeconds() << " " << std::fixed
                           << (g_pragueData * 8) / g_pragueThroughputInterval.GetSeconds() / 1e6
                           << std::endl;
    pragueThroughputCalculator.Update((g_pragueData * 8) / g_pragueThroughputInterval.GetSeconds() /
                                      1e6);
    Simulator::Schedule(g_pragueThroughputInterval, &TracePragueThroughput);
    g_pragueData = 0;
}

void
TraceCubicData(Ptr<const Packet> p,
               const Address& from [[maybe_unused]],
               const Address& to [[maybe_unused]],
               const SeqTsSizeHeader& header)
{
    cubicLatencyCalculator.Update((Now().GetSeconds() - header.GetTs().GetSeconds()) * 1000);
    g_fileCubicLatency << Now().GetSeconds() << " " << std::fixed
                       << (Now().GetSeconds() - header.GetTs().GetSeconds()) * 1000 << std::endl;
    g_cubicData += p->GetSize();
}

void
TraceCubicThroughput(void)
{
    g_fileCubicThroughput << Now().GetSeconds() << " " << std::fixed
                          << (g_cubicData * 8) / g_cubicThroughputInterval.GetSeconds() / 1e6
                          << std::endl;
    Simulator::Schedule(g_cubicThroughputInterval, &TraceCubicThroughput);
    g_cubicData = 0;
}

void
TraceDequeueThroughput(void)
{
    g_fileDequeueThroughput << Now().GetSeconds() << " " << std::fixed
                            << (g_dequeuedData * 8) / g_dequeueThroughputInterval.GetSeconds() / 1e6
                            << std::endl;
    cubicThroughputCalculator.Update((g_cubicData * 8) / g_cubicThroughputInterval.GetSeconds() /
                                     1e6);
    Simulator::Schedule(g_dequeueThroughputInterval, &TraceDequeueThroughput);
    g_dequeuedData = 0;
}

void
HandlePeerClose(Ptr<const Socket> socket)
{
    if (--g_flowsToClose == 0)
    {
        // Close 1 second after last TCP flow closes
        Simulator::Stop(Seconds(1));
    }
}

void
HandlePeerError(Ptr<const Socket> socket)
{
    std::cout << "Warning:  socket closed abnormally" << std::endl;
    if (--g_flowsToClose == 0)
    {
        // Close 1 second after last TCP flow closes
        Simulator::Stop(Seconds(1));
    }
}
