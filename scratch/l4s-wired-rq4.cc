/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*

 * Copyright (c) 2023 CableLabs (L4S RQ3 Wired Fairness Testing)
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

// RQ3 Wired Fairness & Coexistence Testing
//
// Tests fairness between Prague and Cubic flows sharing a bottleneck:
// P-FC1, P-FC4, P-FC8: Prague vs 1,4,8 Cubic flows
// P-FP2, P-FP4, P-FP8: 2,4,8 Prague flows only
// P-FMIX, P-FMIX2, P-FMIX3: Mixed scenarios with both algorithms
//
// Configuration:
// - 100 Mbps bottleneck, 10 ms one-way delay (≈20 ms RTT)
// - Single DualPI2 queue for both Prague (L4S) and Cubic flows
// - Duration: 60s, unlimited transfer (MaxBytes = 0)
// - Measures throughput fairness using Jain's fairness index

// Nodes 0               Node 1                     Node 2          Nodes 3+
//                                                         ------->
// server -------------> router ------------------> router -------> N clients
//        2 Gbps;               configurable rate;         -------> (foreground/background)
//        configurable          100 us base RTT            2 Gbps;
//        base RTT                                         100 us base RTT
//
//
// One server with Prague and Cubic TCP connections to the STA under test
// The first wired client (node index 3) is the client under test
// Additional STA nodes (node indices 4+) for sending background load
//
// Configuration inputs:
// - number of Cubic flows under test
// - number of Prague flows under test
// - number of background flows
// - number of bytes for TCP flows
//
// Behavior:
// - at around simulation time 1 second, each flow starts
// - simulation ends 1 second after last foreground flow terminates, unless
//   a specific duration was configured
//
// Outputs (some of these are for future definition):
// 1) PCAP files at TCP endpoints
// 2) Socket statistics for the first foreground Prague and Cubic flows defined

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/stats.h"
#include "ns3/traffic-control-module.h"
#include "ns3/tcp-bbr3.h"
#include "ns3/ipv4-header.h"
#include "ns3/tcp-header.h"

#include <iomanip>
#include <sstream>
#include <deque>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("L4sWired");

// Declare trace functions that are defined later in this file
std::ofstream g_fileBytesInDualPi2Queue;
void TraceBytesInDualPi2Queue(uint32_t oldVal, uint32_t newVal);
std::ofstream g_fileLSojourn;
void TraceLSojourn(Time sojourn);
std::ofstream g_fileCSojourn;
void TraceCSojourn(Time sojourn);

// RQ4: Per-flow throughput tracking for fairness analysis
std::vector<uint32_t> g_pragueFlowData;     // Per-flow data counters (reset every interval)
std::vector<uint32_t> g_bbrFlowData;        // BBRv3 Per-flow data counters (reset every interval)
std::vector<uint32_t> g_pragueTotalData;    // RQ4: Total data counters (never reset, for final fairness)
std::vector<uint32_t> g_bbrTotalData;       // BBRv3 Total data counters (never reset, for final fairness)
std::map<uint32_t, uint32_t> g_flowPortToIndex;  // Map port numbers to flow indices

// ENHANCED ANALYTICS: Separate queueing delay tracking
std::vector<double> g_pragueSojournTimes;   // Prague sojourn times for analysis
std::vector<double> g_bbrSojournTimes;      // BBRv3 sojourn times for analysis
std::ofstream g_filePragueSojourn;         // Prague-specific sojourn times
std::ofstream g_fileBbrSojourn;            // BBRv3-specific sojourn times
std::ofstream g_fileQueueingAnalytics;     // Combined analytics output

// ENHANCED ANALYTICS: Smart sojourn time tracking
std::map<uint64_t, Time> g_packetTimestamps;  // Track packet enqueue times
std::map<uint64_t, bool> g_packetIsPrague;    // Track which algorithm each packet belongs to
uint64_t g_packetIdCounter = 0;

uint64_t g_pragueData = 0;
Time g_lastSeenPrague = Seconds(0);
std::ofstream g_filePragueThroughput;
std::ofstream g_filePraguePerFlowThroughput; // RQ4: Per-flow throughput
std::ofstream g_filePragueCwnd;
std::ofstream g_filePragueSsthresh;
std::ofstream g_filePragueSendInterval;
std::ofstream g_filePraguePacingRate;
std::ofstream g_filePragueCongState;
std::ofstream g_filePragueEcnState;
std::ofstream g_filePragueRtt;
Time g_pragueThroughputInterval = MilliSeconds(100);
void TracePragueThroughput();
void TracePragueTx(Ptr<const Packet> packet,
                   const TcpHeader& header,
                   Ptr<const TcpSocketBase> socket);
void TracePragueRx (Ptr<const Packet>, const TcpHeader&,
                    Ptr<const TcpSocketBase>);
void TraceBbrRx  (Ptr<const Packet>, const TcpHeader&,
                    Ptr<const TcpSocketBase>);void TracePragueCwnd(uint32_t oldVal, uint32_t newVal);
void TracePragueSsthresh(uint32_t oldVal, uint32_t newVal);
void TracePraguePacingRate(DataRate oldVal, DataRate newVal);
void TracePragueCongState(TcpSocketState::TcpCongState_t oldVal,
                          TcpSocketState::TcpCongState_t newVal);
void TracePragueEcnState(TcpSocketState::EcnState_t oldVal, TcpSocketState::EcnState_t newVal);
void TracePragueRtt(Time oldVal, Time newVal);
void TracePragueSocket(Ptr<Application>, uint32_t);
void TracePragueServerSocket (Ptr<Socket>);
void TraceBbrServerSocket  (Ptr<Socket>);

uint64_t g_bbrData = 0;
Time g_lastSeenBbr = Seconds(0);
std::ofstream g_fileBbrThroughput;
std::ofstream g_fileBbrPerFlowThroughput; // RQ4: Per-flow throughput
std::ofstream g_fileBbrCwnd;
std::ofstream g_fileBbrSsthresh;
std::ofstream g_fileBbrSendInterval;
std::ofstream g_fileBbrPacingRate;
std::ofstream g_fileBbrCongState;
std::ofstream g_fileBbrRtt;
Time g_bbrThroughputInterval = MilliSeconds(100);
void TraceBbrThroughput();
void TraceBbrTx(Ptr<const Packet> packet,
                  const TcpHeader& header,
                  Ptr<const TcpSocketBase> socket);
void TraceBbrCwnd(uint32_t oldVal, uint32_t newVal);
void TraceBbrSsthresh(uint32_t oldVal, uint32_t newVal);
void TraceBbrPacingRate(DataRate oldVal, DataRate newVal);
void TraceBbrCongState(TcpSocketState::TcpCongState_t oldVal,
                         TcpSocketState::TcpCongState_t newVal);
void TraceBbrRtt(Time oldVal, Time newVal);
void TraceBbrSocket(Ptr<Application> a, uint32_t i);
void TracePragueAppRx (uint32_t flowIdx, Ptr<const Packet>,
                       const Address& src, const Address& dst);
void TraceBbrAppRx  (uint32_t flowIdx, Ptr<const Packet>,
                       const Address& src, const Address& dst);
// RQ4: Per-flow throughput tracing functions
void TracePraguePerFlowThroughput();
void TraceBbrPerFlowThroughput();
// RQ4: Fairness analysis functions  
double CalculateJainsFairnessIndex(const std::vector<uint32_t>& flowData);
void ReportFairnessStatistics(std::string testName, uint32_t numPrague, uint32_t numBbr);

// Count the number of flows to wait for completion before stopping the simulation
uint32_t g_flowsToClose = 0;
// Hook these methods to the PacketSink objects
void HandlePeerClose(std::string context, Ptr<const Socket> socket);
void HandlePeerError(std::string context, Ptr<const Socket> socket);

// These methods work around the lack of ability to configure different TCP socket types
// on the same node on a per-socket (per-application) basis. Instead, these methods can
// be scheduled (right before a socket creation) to change the default value
void ConfigurePragueSockets(Ptr<TcpL4Protocol> tcp1, Ptr<TcpL4Protocol> tcp2);
void ConfigureBbrSockets(Ptr<TcpL4Protocol> tcp1, Ptr<TcpL4Protocol> tcp2);

// Declare some statistics counters here so that they are updated in traces
MinMaxAvgTotalCalculator<uint32_t> pragueThroughputCalculator; // units of Mbps
MinMaxAvgTotalCalculator<uint32_t> bbrThroughputCalculator;  // units of Mbps

// RQ4: Enhanced analytics functions
void TracePragueSpecificSojourn(Time sojourn);
void TraceBbrSpecificSojourn(Time sojourn);
void TraceAlgorithmSpecificSojourn(Ptr<const QueueDiscItem> item, Time sojourn);
void GenerateQueueingAnalytics(std::string testName, uint32_t numPrague, uint32_t numBbr);
bool IsPortPrague(uint16_t port);
bool IsPortBbr(uint16_t port);

int
main(int argc, char* argv[])
{
    // Variable declaration, and constants
    Time progressInterval = Seconds(5);

    // Variables that can be changed by command-line argument
    uint32_t numBbr = 1;         // Changed from numCubic to numBbr
    uint32_t numPrague = 1;
    uint32_t numBackground = 0;
    uint32_t numBytes = 0;                // RQ4: Unlimited transfer (MaxBytes = 0)
    Time duration = Seconds(60);          // RQ4: Fixed 60 second duration
    Time wanLinkDelay = MilliSeconds(10); // base RTT is 20ms
    DataRate bottleneckRate = DataRate("100Mbps");
    bool useReno = false;
    bool showProgress = false;
    bool enablePcapAll = false;
    bool enablePcap = true;
    std::string lossSequence = "";
    std::string lossBurst = "";
    std::string testName = "";
    uint16_t rngRun = 1;  // RQ4: Add random number generator run parameter

    // Increase some defaults (command-line can override below)
    // ns-3 TCP does not automatically adjust MSS from the device MTU
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    // ns-3 TCP socket buffer sizes do not dynamically grow, so set to ~3 * BWD product
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(750000));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(750000));
    // Enable pacing for Cubic
    Config::SetDefault("ns3::TcpSocketState::EnablePacing", BooleanValue(true));
    Config::SetDefault("ns3::TcpSocketState::PaceInitialWindow", BooleanValue(true));
    // Enable a timestamp (for latency sampling) in the bulk send application
    Config::SetDefault("ns3::BulkSendApplication::EnableSeqTsSizeHeader", BooleanValue(true));
    Config::SetDefault("ns3::PacketSink::EnableSeqTsSizeHeader", BooleanValue(true));
    // The bulk send application should do 1448-byte writes (one timestamp per TCP packet)
    Config::SetDefault("ns3::BulkSendApplication::SendSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocketBase::UseEcn", StringValue("On"));
    CommandLine cmd;
    cmd.Usage("RQ4: L4S Wired Fairness & Coexistence Testing - Prague vs BBRv3 flows sharing bottleneck");
    cmd.AddValue("numBbr", "Number of foreground BBRv3 flows", numBbr);
    cmd.AddValue("numPrague", "Number of foreground Prague flows", numPrague);
    cmd.AddValue("numBackground", "Number of background flows", numBackground);
    cmd.AddValue("numBytes", "Number of bytes for each TCP transfer (0=unlimited)", numBytes);
    cmd.AddValue("duration", "Simulation duration", duration);
    cmd.AddValue("wanLinkDelay", "one-way base delay from server to AP", wanLinkDelay);
    cmd.AddValue("bottleneckRate", "bottleneck data rate between routers", bottleneckRate);
    cmd.AddValue("useReno", "Use Linux Reno instead of Cubic", useReno);
    cmd.AddValue("lossSequence", "Packets to drop", lossSequence);
    cmd.AddValue("lossBurst", "Packets to drop", lossBurst);
    cmd.AddValue("testName", "Test name (P1-B1, P1-B2, P2-B1, P1-B4, P4-B1, P2-B2, P4-B4)", testName);
    cmd.AddValue("showProgress", "Show simulation progress every 5s", showProgress);
    cmd.AddValue("enablePcapAll", "Whether to enable PCAP trace output at all interfaces", enablePcapAll);
    cmd.AddValue("enablePcap", "Whether to enable PCAP trace output only at endpoints", enablePcap);
    cmd.AddValue("rngRun", "Random number generator run", rngRun);
    cmd.Parse(argc, argv);

    // RQ4 validation
    NS_ABORT_MSG_IF(numBbr == 0 && numPrague == 0, "RQ4: Configure at least one foreground flow");
    NS_ABORT_MSG_IF(testName == "", "RQ4: testName is required for fairness experiments");
    NS_ABORT_MSG_IF(numBackground > 0, "Background flows not yet supported");

    // RQ4: Initialize per-flow tracking
    g_pragueFlowData.resize(numPrague, 0);
    g_bbrFlowData.resize(numBbr, 0);
    g_pragueTotalData.resize(numPrague, 0);  // RQ4: Total data arrays for final fairness
    g_bbrTotalData.resize(numBbr, 0);    // RQ4: Total data arrays for final fairness

    // Map port numbers to flow indices for tracking
    for (uint32_t i = 0; i < numPrague; i++)
    {
        g_flowPortToIndex[100 + i] = i;  // Prague ports start at 100
    }
    for (uint32_t i = 0; i < numBbr; i++)
    {
        g_flowPortToIndex[200 + i] = i;  // BBRv3 ports start at 200
    }

    // When using DCE with ns-3, or reading pcaps with Wireshark,
    // enable checksum computations in ns-3 models
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    if (useReno)
    {
        std::cout << "Using ns-3 LinuxReno model instead of Cubic" << std::endl;
        Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                           TypeIdValue(TcpLinuxReno::GetTypeId()));
    }
    
    // RQ3: Set the random number generator run
    RngSeedManager::SetRun(rngRun);
    
    // Workaround until PRR response is debugged
    Config::SetDefault("ns3::TcpL4Protocol::RecoveryType",
                       TypeIdValue(TcpClassicRecovery::GetTypeId()));

    // Create the nodes and use containers for further configuration below
    NodeContainer serverNode;
    serverNode.Create(1);
    NodeContainer routerNodes;
    routerNodes.Create(2);
    NodeContainer clientNodes;
    clientNodes.Create(1 + numBackground);

    // Create point-to-point links between server and AP
    PointToPointHelper pointToPoint;
    pointToPoint.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("2p"));
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("2Gbps"));
    pointToPoint.SetChannelAttribute("Delay", TimeValue(wanLinkDelay));
    NetDeviceContainer wanDevices = pointToPoint.Install(serverNode.Get(0), routerNodes.Get(0));

    pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(bottleneckRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue("50us"));
    NetDeviceContainer routerDevices = pointToPoint.Install(routerNodes);

    if (lossSequence != "")
    {
        std::list<uint32_t> lossSequenceList;
        std::stringstream ss(lossSequence);
        for (uint32_t i; ss >> i;)
        {
            lossSequenceList.push_back(i);
            if (ss.peek() == ',')
            {
                ss.ignore();
            }
        }
        auto em = CreateObject<ReceiveListErrorModel>();
        em->SetList(lossSequenceList);
        routerDevices.Get(1)->GetObject<PointToPointNetDevice>()->SetReceiveErrorModel(em);
    }
    else if (lossBurst != "")
    {
        std::string delimiter = "-";
        std::size_t pos = lossBurst.find(delimiter);
        uint32_t start = std::stoi(lossBurst.substr(0, pos));
        uint32_t end = std::stoi(lossBurst.substr(pos + 1, lossBurst.size()));
        std::list<uint32_t> lossBurstList;
        for (uint32_t i = start; i <= end; i++)
        {
            lossBurstList.push_back(i);
        }
        auto em = CreateObject<ReceiveListErrorModel>();
        em->SetList(lossBurstList);
        routerDevices.Get(1)->GetObject<PointToPointNetDevice>()->SetReceiveErrorModel(em);
    }

    pointToPoint.SetDeviceAttribute("DataRate", StringValue("2Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("50us"));
    NetDeviceContainer clientDevices = pointToPoint.Install(routerNodes.Get(1), clientNodes.Get(0));

    // Internet and Linux stack installation
    InternetStackHelper internetStack;
    internetStack.Install(serverNode);
    internetStack.Install(routerNodes);
    internetStack.Install(clientNodes);

    // By default, Ipv4AddressHelper below will configure a FqCoDelQueueDiscs on routers
    // The following statements change this configuration on the bottleneck link
    // RQ3: Use single DualPI2 queue where Prague and Cubic flows compete
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::DualPi2QueueDisc");
    tch.SetQueueLimits("ns3::DynamicQueueLimits"); // enable BQL
    QueueDiscContainer routerQueueDiscContainer = tch.Install(routerDevices);

    // Configure IP addresses for all links
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wanInterfaces = address.Assign(wanDevices);
    address.SetBase("172.16.1.0", "255.255.255.0");
    Ipv4InterfaceContainer routerInterfaces = address.Assign(routerDevices);
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer clientInterfaces = address.Assign(clientDevices);

    // Use a helper to add static routes
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Get pointers to the TcpL4Protocol instances of the primary nodes
    Ptr<TcpL4Protocol> tcpL4ProtocolServer = serverNode.Get(0)->GetObject<TcpL4Protocol>();
    Ptr<TcpL4Protocol> tcpL4ProtocolClient = clientNodes.Get(0)->GetObject<TcpL4Protocol>();

    // Application configuration for Prague flows under test
    uint16_t port = 100;
    ApplicationContainer pragueServerApps;
    ApplicationContainer pragueClientApps;
    // The following offset is used to prevent all Prague flows from starting
    // at the same time.  However, this program has a special constraint in
    // that the TCP socket TypeId is changed from Prague to Cubic after 50 ms
    // (to allow for installation of both Prague and Cubic sockets on the
    // same node).  Therefore, adjust this start offset based on the number
    // of flows, and make sure that the last value is less than 50 ms.
    Time pragueStartOffset = MilliSeconds(50) / (numPrague + 1);
    for (auto i = 0U; i < numPrague; i++)
    {
        BulkSendHelper bulk("ns3::TcpSocketFactory",
                            InetSocketAddress(clientInterfaces.GetAddress(1), port + i));
        bulk.SetAttribute("MaxBytes", UintegerValue(numBytes));
        bulk.SetAttribute("StartTime", TimeValue(Seconds(1.0) + i * pragueStartOffset));
        pragueServerApps.Add(bulk.Install(serverNode.Get(0)));
        NS_LOG_DEBUG("Creating Prague foreground flow " << i);
        PacketSinkHelper sink =
            PacketSinkHelper("ns3::TcpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), port + i));
        sink.SetAttribute("StartTime", TimeValue(Seconds(1.0) + i * pragueStartOffset));
        pragueClientApps.Add(sink.Install(clientNodes.Get(0)));
        g_flowsToClose++;
        Simulator::Schedule(
            Seconds(1.0) - TimeStep(1),
            MakeBoundCallback(&ConfigurePragueSockets, tcpL4ProtocolServer, tcpL4ProtocolClient));
    }

    // Application configuration for BBRv3 flows under test
    port = 200;
    ApplicationContainer bbrServerApps;
    ApplicationContainer bbrClientApps;
    // FAIRNESS FIX: Start BBRv3 flows at the same base time as Prague (1.0s)
    // instead of 1.05s to ensure fair competition. Use small offsets within each
    // algorithm to prevent simultaneous starts.
    Time bbrStartOffset = MilliSeconds(50) / (numBbr + 1);
    for (auto i = 0U; i < numBbr; i++)
    {
        BulkSendHelper bulkBbr("ns3::TcpSocketFactory",
                                 InetSocketAddress(clientInterfaces.GetAddress(1), port + i));
        bulkBbr.SetAttribute("MaxBytes", UintegerValue(numBytes));
        bulkBbr.SetAttribute("StartTime", TimeValue(Seconds(1.0) + i * bbrStartOffset));
        bbrServerApps.Add(bulkBbr.Install(serverNode.Get(0)));
        NS_LOG_DEBUG("Creating BBRv3 foreground flow " << i);
        PacketSinkHelper sinkBbr =
            PacketSinkHelper("ns3::TcpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), port + i));
        sinkBbr.SetAttribute("StartTime", TimeValue(Seconds(1.0) + i * bbrStartOffset));
        bbrClientApps.Add(sinkBbr.Install(clientNodes.Get(0)));
        g_flowsToClose++;
        // Configure BBRv3 sockets at the same time as Prague (50ms before start)
        Simulator::Schedule(
            Seconds(1.0) - TimeStep(1),
            MakeBoundCallback(&ConfigureBbrSockets, tcpL4ProtocolServer, tcpL4ProtocolClient));
    }

    // Add a cubic application on the server for each background flow
    // Send the traffic from a different STA.
    port = 300;
    // Configure background flows sockets at the same time as foreground flows
    Simulator::Schedule(
        Seconds(1.0) - TimeStep(1),
        MakeBoundCallback(&ConfigureBbrSockets, tcpL4ProtocolServer, tcpL4ProtocolClient));
    for (auto i = 0U; i < numBackground; i++)
    {
        ApplicationContainer serverAppBackground;
        BulkSendHelper bulkBackground("ns3::TcpSocketFactory",
                                      InetSocketAddress(wanInterfaces.GetAddress(0), port + i));
        bulkBackground.SetAttribute("MaxBytes", UintegerValue(numBytes));
        serverAppBackground = bulkBackground.Install(clientNodes.Get(1 + i));
        serverAppBackground.Start(Seconds(1.0)); // Start background at same time as foreground
        ApplicationContainer clientAppBackground;
        PacketSinkHelper sinkBackground =
            PacketSinkHelper("ns3::TcpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), port + i));
        clientAppBackground = sinkBackground.Install(serverNode.Get(0));
        clientAppBackground.Start(Seconds(1.0)); // Start background at same time as foreground
    }

    // PCAP traces
    if (enablePcapAll)
    {
        std::string prefixName = "l4s-wired" + ((testName != "") ? ("-" + testName) : "");
        pointToPoint.EnablePcapAll(prefixName.c_str());
    }
    else if (enablePcap)
    {
        std::string prefixName = "l4s-wired" + ((testName != "") ? ("-" + testName) : "");
        pointToPoint.EnablePcap(prefixName.c_str(), wanDevices.Get(0));
        pointToPoint.EnablePcap(prefixName.c_str(), clientDevices.Get(0));
    }

    // Throughput and latency for foreground flows, and set up close callbacks
    if (pragueClientApps.GetN())
    {
        std::string traceName =
            "prague-throughput." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_filePragueThroughput.open(traceName.c_str(), std::ofstream::out);
        // RQ3: Per-flow throughput file
        traceName = "prague-per-flow-throughput." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_filePraguePerFlowThroughput.open(traceName.c_str(), std::ofstream::out);
        traceName = "prague-cwnd." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_filePragueCwnd.open(traceName.c_str(), std::ofstream::out);
        traceName = "prague-ssthresh." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_filePragueSsthresh.open(traceName.c_str(), std::ofstream::out);
        traceName = "prague-send-interval." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_filePragueSendInterval.open(traceName.c_str(), std::ofstream::out);
        traceName = "prague-cong-state." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_filePragueCongState.open(traceName.c_str(), std::ofstream::out);

        traceName = "prague-pacing-rate." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_filePraguePacingRate.open(traceName.c_str(), std::ofstream::out);
        traceName = "prague-ecn-state." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_filePragueEcnState.open(traceName.c_str(), std::ofstream::out);
        traceName = "prague-rtt." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_filePragueRtt.open(traceName.c_str(), std::ofstream::out);
    }
    for (auto i = 0U; i < pragueClientApps.GetN(); i++)
    {
        // The TCP sockets that we want to connect
        Simulator::Schedule(Seconds(1.0) + i * MilliSeconds(10) + TimeStep(1),
                            MakeBoundCallback(&TracePragueSocket, pragueServerApps.Get(i), i));
        std::ostringstream oss;
        oss << "Prague:" << i;
        NS_LOG_DEBUG("Setting up callbacks on Prague sockets " << pragueClientApps.Get(i));
        pragueClientApps.Get(i)->GetObject<PacketSink>()->TraceConnect(
            "PeerClose",
            oss.str(),
            MakeCallback(&HandlePeerClose));
        pragueClientApps.Get(i)->GetObject<PacketSink>()->TraceConnect(
            "PeerError",
            oss.str(),
            MakeCallback(&HandlePeerError));
        // RQ3: Connect PacketSink Rx trace for per-flow throughput tracking
         pragueClientApps.Get(i)->GetObject<PacketSink>()
     ->TraceConnectWithoutContext("RxWithAddresses",
                                  MakeBoundCallback(&TracePragueAppRx, i));
    }

    if (bbrClientApps.GetN())
    {
        std::string traceName =
            "bbr-throughput." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileBbrThroughput.open(traceName.c_str(), std::ofstream::out);
        // RQ4: Per-flow throughput file
        traceName = "bbr-per-flow-throughput." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileBbrPerFlowThroughput.open(traceName.c_str(), std::ofstream::out);
        traceName = "bbr-cwnd." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileBbrCwnd.open(traceName.c_str(), std::ofstream::out);
        traceName = "bbr-ssthresh." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileBbrSsthresh.open(traceName.c_str(), std::ofstream::out);
        traceName = "bbr-send-interval." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileBbrSendInterval.open(traceName.c_str(), std::ofstream::out);
        traceName = "bbr-pacing-rate." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileBbrPacingRate.open(traceName.c_str(), std::ofstream::out);
        traceName = "bbr-cong-state." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileBbrCongState.open(traceName.c_str(), std::ofstream::out);
        traceName = "bbr-rtt." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileBbrRtt.open(traceName.c_str(), std::ofstream::out);
    }
    for (auto i = 0U; i < bbrClientApps.GetN(); i++)
    {
        // The TCP sockets that we want to connect
        Simulator::Schedule(Seconds(1.05) + i * MilliSeconds(10) + TimeStep(1),
                            MakeBoundCallback(&TraceBbrSocket, bbrServerApps.Get(i), i));
        std::ostringstream oss;
        oss << "BBRv3:" << i;
        NS_LOG_DEBUG("Setting up callbacks on BBRv3 sockets " << i << " "
                                                              << bbrClientApps.Get(i));
        bbrClientApps.Get(i)->GetObject<PacketSink>()->TraceConnect(
            "PeerClose",
            oss.str(),
            MakeCallback(&HandlePeerClose));
        bbrClientApps.Get(i)->GetObject<PacketSink>()->TraceConnect(
            "PeerError",
            oss.str(),
            MakeCallback(&HandlePeerError));
        // RQ4: Connect PacketSink Rx trace for per-flow throughput tracking
        bbrClientApps.Get(i)->GetObject<PacketSink>()
            ->TraceConnectWithoutContext("RxWithAddresses",
                                         MakeBoundCallback(&TraceBbrAppRx, i));
    }

    // Trace bytes in DualPi2 queue
    Ptr<DualPi2QueueDisc> dualPi2 = routerQueueDiscContainer.Get(0)->GetObject<DualPi2QueueDisc>();
    NS_ASSERT_MSG(dualPi2, "Could not acquire pointer to DualPi2 queue");
    std::string traceName =
        "wired-dualpi2-bytes." + ((testName != "") ? (testName + ".") : "") + "dat";
    g_fileBytesInDualPi2Queue.open(traceName.c_str(), std::ofstream::out);
    dualPi2->TraceConnectWithoutContext("BytesInQueue", MakeCallback(&TraceBytesInDualPi2Queue));
    traceName = "wired-dualpi2-l-sojourn." + ((testName != "") ? (testName + ".") : "") + "dat";
    g_fileLSojourn.open(traceName.c_str(), std::ofstream::out);
    dualPi2->TraceConnectWithoutContext("L4sSojournTime", MakeCallback(&TraceLSojourn));
    traceName = "wired-dualpi2-c-sojourn." + ((testName != "") ? (testName + ".") : "") + "dat";
    g_fileCSojourn.open(traceName.c_str(), std::ofstream::out);
    dualPi2->TraceConnectWithoutContext("ClassicSojournTime", MakeCallback(&TraceCSojourn));
    
    // ENHANCED ANALYTICS: Open algorithm-specific sojourn files
    if (pragueClientApps.GetN() || bbrClientApps.GetN())
    {
        std::string traceName;
        
        // Prague-specific sojourn times
        traceName = "prague-sojourn." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_filePragueSojourn.open(traceName.c_str(), std::ofstream::out);
        
        // BBRv3-specific sojourn times
        traceName = "bbr-sojourn." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileBbrSojourn.open(traceName.c_str(), std::ofstream::out);
        
        // Combined analytics output
        traceName = "queueing-analytics." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileQueueingAnalytics.open(traceName.c_str(), std::ofstream::out);
        
        // Write headers
        g_filePragueSojourn << "# Time(s) SojournTime(ms)" << std::endl;
        g_fileBbrSojourn << "# Time(s) SojournTime(ms)" << std::endl;
        g_fileQueueingAnalytics << "# L4S Algorithm Queueing Delay Analytics" << std::endl;
    }

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
    std::cout << "Foreground flows: BBRv3: " << numBbr << " Prague: " << numPrague << std::endl;
    std::cout << "Background flows: " << numBackground << std::endl;
    
    // Enable logging for debugging ECN and L4S classification
    if (showProgress)
    {
        // LogComponentEnable("TcpPrague", LOG_LEVEL_INFO);
        // LogComponentEnable("TcpBbr3", LOG_LEVEL_INFO);
        // LogComponentEnable("DualPi2QueueDisc", LOG_LEVEL_INFO);
    }

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

    if (stopReason == "fail-safe")
    {
        std::cout << "** Expected " << numBbr + numPrague << " flows to close, but "
                  << g_flowsToClose << " are remaining" << std::endl
                  << std::endl;
    }

    std::cout << std::fixed << std::setprecision(2);
    if (numBbr)
    {
        std::cout << "BBRv3 throughput (Mbps) mean: " << bbrThroughputCalculator.getMean()
                  << " max: " << bbrThroughputCalculator.getMax()
                  << " min: " << bbrThroughputCalculator.getMin() << std::endl;
    }
    if (numPrague)
    {
        std::cout << "Prague throughput (Mbps) mean: " << pragueThroughputCalculator.getMean()
                  << " max: " << pragueThroughputCalculator.getMax()
                  << " min: " << pragueThroughputCalculator.getMin() << std::endl;
    }

    // RQ3: Report fairness statistics and ENHANCED ANALYTICS
    ReportFairnessStatistics(testName, numPrague, numBbr);
    GenerateQueueingAnalytics(testName, numPrague, numBbr);

    g_fileBytesInDualPi2Queue.close();
    g_fileLSojourn.close();
    g_fileCSojourn.close();
    g_filePragueThroughput.close();
    g_filePraguePerFlowThroughput.close(); // RQ3: Close per-flow file
    g_filePragueCwnd.close();
    g_filePragueSsthresh.close();
    g_filePragueSendInterval.close();
    g_filePraguePacingRate.close();
    g_filePragueCongState.close();
    g_filePragueEcnState.close();
    g_filePragueRtt.close();
    g_fileBbrThroughput.close();
    g_fileBbrPerFlowThroughput.close(); // RQ3: Close per-flow file
    g_fileBbrCwnd.close();
    g_fileBbrSsthresh.close();
    g_fileBbrSendInterval.close();
    g_fileBbrPacingRate.close();
    g_fileBbrCongState.close();
    g_fileBbrRtt.close();
    
    // ENHANCED ANALYTICS: Close algorithm-specific files
    g_filePragueSojourn.close();
    g_fileBbrSojourn.close();
    g_fileQueueingAnalytics.close();

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
ConfigureBbrSockets(Ptr<TcpL4Protocol> tcp1, Ptr<TcpL4Protocol> tcp2)
{
    tcp1->SetAttribute("SocketType", TypeIdValue(TcpBbr3::GetTypeId()));
    tcp2->SetAttribute("SocketType", TypeIdValue(TcpBbr3::GetTypeId()));
}

void
TraceBytesInDualPi2Queue(uint32_t oldVal, uint32_t newVal)
{
    g_fileBytesInDualPi2Queue << Now().GetSeconds() << " " << newVal << std::endl;
}

void
TraceLSojourn(Time sojourn)
{
    g_fileLSojourn << Now().GetSeconds() << " " << sojourn.GetMicroSeconds() / 1000.0 << std::endl;
    
    // We need to modify this to get packet information - this basic version just logs all L4S sojourns
    // The precise tracking will be implemented via queue disc item inspection
}

void
TraceCSojourn(Time sojourn)
{
    g_fileCSojourn << Now().GetSeconds() << " " << sojourn.GetMicroSeconds() / 1000.0 << std::endl;
}

void
TracePragueTx(Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket)
{
    // RQ3: Remove double-counting - only count on Rx side
    // g_pragueData += packet->GetSize();  // REMOVED - was double-counting
    if (g_lastSeenPrague > Seconds(0))
    {
        g_filePragueSendInterval << std::fixed << std::setprecision(6) << Now().GetSeconds() << " "
                                 << (Now() - g_lastSeenPrague).GetSeconds() << std::endl;
    }
    g_lastSeenPrague = Now();
}

void
TracePragueThroughput()
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
TracePragueCwnd(uint32_t oldVal, uint32_t newVal)
{
    g_filePragueCwnd << Now().GetSeconds() << " " << newVal << std::endl;
}

void
TracePragueSsthresh(uint32_t oldVal, uint32_t newVal)
{
    g_filePragueSsthresh << Now().GetSeconds() << " " << newVal << std::endl;
}

void
TracePraguePacingRate(DataRate oldVal, DataRate newVal)
{
    g_filePraguePacingRate << Now().GetSeconds() << " " << newVal.GetBitRate() << std::endl;
}

void
TracePragueCongState(TcpSocketState::TcpCongState_t oldVal, TcpSocketState::TcpCongState_t newVal)
{
    g_filePragueCongState << Now().GetSeconds() << " " << TcpSocketState::TcpCongStateName[newVal]
                          << std::endl;
}

void
TracePragueEcnState(TcpSocketState::EcnState_t oldVal, TcpSocketState::EcnState_t newVal)
{
    g_filePragueEcnState << Now().GetSeconds() << " " << TcpSocketState::EcnStateName[newVal]
                         << std::endl;
}

void
TracePragueRtt(Time oldVal, Time newVal)
{
    g_filePragueRtt << Now().GetSeconds() << " " << newVal.GetMicroSeconds() / 1000.0 << std::endl;
}

void
TracePragueSocket(Ptr<Application> a, uint32_t i)
{
    Ptr<BulkSendApplication> bulk = DynamicCast<BulkSendApplication>(a);
    NS_ASSERT_MSG(bulk, "Application failed");
    
    // Check if socket is ready, if not, reschedule
    Ptr<Socket> s = bulk->GetSocket();
    if (!s)
    {
        // Socket not ready yet, reschedule for later
        Simulator::Schedule(MilliSeconds(10), MakeBoundCallback(&TracePragueSocket, a, i));
        return;
    }
    
    Ptr<TcpSocketBase> tcp = DynamicCast<TcpSocketBase>(s);
    NS_ASSERT_MSG(tcp, "TCP socket downcast failed");
    tcp->TraceConnectWithoutContext("Tx", MakeCallback(&TracePragueTx));
    // RQ3: Rx trace not needed on client side - we'll trace server side
    if (i == 0)
    {
        tcp->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&TracePragueCwnd));
        tcp->TraceConnectWithoutContext("SlowStartThreshold", MakeCallback(&TracePragueSsthresh));
        tcp->TraceConnectWithoutContext("PacingRate", MakeCallback(&TracePraguePacingRate));
        tcp->TraceConnectWithoutContext("CongState", MakeCallback(&TracePragueCongState));
        tcp->TraceConnectWithoutContext("EcnState", MakeCallback(&TracePragueEcnState));
        tcp->TraceConnectWithoutContext("RTT", MakeCallback(&TracePragueRtt));
        Simulator::Schedule(g_pragueThroughputInterval, &TracePragueThroughput);
        // RQ3: Schedule per-flow throughput tracing
        if (g_pragueFlowData.size() > 0)
        {
            Simulator::Schedule(g_pragueThroughputInterval, &TracePraguePerFlowThroughput);
        }
    }
}

void
TraceBbrTx(Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket)
{
    // RQ3: Remove double-counting - only count on Rx side  
    // g_cubicData += packet->GetSize();  // REMOVED - was double-counting
    if (g_lastSeenBbr > Seconds(0))
    {
        g_fileBbrSendInterval << std::fixed << std::setprecision(6) << Now().GetSeconds() << " "
                                << (Now() - g_lastSeenBbr).GetSeconds() << std::endl;
    }
    g_lastSeenBbr = Now();
}

void
TraceBbrThroughput()
{
    g_fileBbrThroughput << Now().GetSeconds() << " " << std::fixed
                          << (g_bbrData * 8) / g_bbrThroughputInterval.GetSeconds() / 1e6
                          << std::endl;
    bbrThroughputCalculator.Update((g_bbrData * 8) / g_bbrThroughputInterval.GetSeconds() /
                                     1e6);
    Simulator::Schedule(g_bbrThroughputInterval, &TraceBbrThroughput);
    g_bbrData = 0;
}

void
TraceBbrCwnd(uint32_t oldVal, uint32_t newVal)
{
    g_fileBbrCwnd << Now().GetSeconds() << " " << newVal << std::endl;
}

void
TraceBbrSsthresh(uint32_t oldVal, uint32_t newVal)
{
    g_fileBbrSsthresh << Now().GetSeconds() << " " << newVal << std::endl;
}

void
TraceBbrPacingRate(DataRate oldVal, DataRate newVal)
{
    g_fileBbrPacingRate << Now().GetSeconds() << " " << newVal.GetBitRate() << std::endl;
}

void
TraceBbrCongState(TcpSocketState::TcpCongState_t oldVal, TcpSocketState::TcpCongState_t newVal)
{
    g_fileBbrCongState << Now().GetSeconds() << " " << TcpSocketState::TcpCongStateName[newVal]
                         << std::endl;
}

void
TraceBbrRtt(Time oldVal, Time newVal)
{
    g_fileBbrRtt << Now().GetSeconds() << " " << newVal.GetMicroSeconds() / 1000.0 << std::endl;
}

void
TraceBbrSocket(Ptr<Application> a, uint32_t i)
{
    Ptr<BulkSendApplication> bulk = DynamicCast<BulkSendApplication>(a);
    NS_ASSERT_MSG(bulk, "Application failed");
    
    // Check if socket is ready, if not, reschedule
    Ptr<Socket> s = bulk->GetSocket();
    if (!s)
    {
        // Socket not ready yet, reschedule for later
        Simulator::Schedule(MilliSeconds(10), MakeBoundCallback(&TraceBbrSocket, a, i));
        return;
    }
    
    Ptr<TcpSocketBase> tcp = DynamicCast<TcpSocketBase>(s);
    NS_ASSERT_MSG(tcp, "TCP socket downcast failed");
    tcp->TraceConnectWithoutContext("Tx", MakeCallback(&TraceBbrTx));
    // RQ3: Rx trace not needed on client side - we'll trace server side
    if (i == 0)
    {
        tcp->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&TraceBbrCwnd));
        tcp->TraceConnectWithoutContext("SlowStartThreshold", MakeCallback(&TraceBbrSsthresh));
        tcp->TraceConnectWithoutContext("PacingRate", MakeCallback(&TraceBbrPacingRate));
        tcp->TraceConnectWithoutContext("CongState", MakeCallback(&TraceBbrCongState));
        tcp->TraceConnectWithoutContext("RTT", MakeCallback(&TraceBbrRtt));
        Simulator::Schedule(g_bbrThroughputInterval, &TraceBbrThroughput);
        // RQ3: Schedule per-flow throughput tracing
        if (g_bbrFlowData.size() > 0)
        {
            Simulator::Schedule(g_bbrThroughputInterval, &TraceBbrPerFlowThroughput);
        }
    }
}

void
HandlePeerClose(std::string context, Ptr<const Socket> socket)
{
    NS_LOG_DEBUG("Handling close of socket " << context);
    if (--g_flowsToClose == 0)
    {
        // Close 1 second after last TCP flow closes
        Simulator::Stop(Seconds(1));
    }
}

void
HandlePeerError(std::string context, Ptr<const Socket> socket)
{
    NS_LOG_DEBUG("Handling abnormal close of socket " << context);
    std::cout << "Warning:  socket closed abnormally" << std::endl;
    if (--g_flowsToClose == 0)
    {
        // Close 1 second after last TCP flow closes
        Simulator::Stop(Seconds(1));
    }
}

void
TracePragueRx (Ptr<const Packet> pkt,
               const TcpHeader&  hdr,
               Ptr<const TcpSocketBase> /*socket*/)
{
    g_pragueData += pkt->GetSize ();

    uint16_t port = hdr.GetDestinationPort ();          // 100,101…
    if (port >= 100 && port < 200)
    {
        uint32_t idx = port - 100;
        if (idx < g_pragueFlowData.size ())
        {
            g_pragueFlowData [idx]   += pkt->GetSize ();
            g_pragueTotalData [idx]  += pkt->GetSize ();
        }
    }
}

void
TraceBbrRx (Ptr<const Packet> pkt,
              const TcpHeader&  hdr,
              Ptr<const TcpSocketBase> /*socket*/)
{
    g_bbrData += pkt->GetSize ();

    uint16_t port = hdr.GetDestinationPort ();          // 200,201…
    if (port >= 200 && port < 300)
    {
        uint32_t idx = port - 200;
        if (idx < g_bbrFlowData.size ())
        {
            g_bbrFlowData [idx]   += pkt->GetSize ();
            g_bbrTotalData [idx]  += pkt->GetSize ();
        }
    }
}

void
TracePragueServerSocket (Ptr<Socket> sock)
{
    // cast away const so we can attach another trace
    Ptr<Socket> s = ConstCast<Socket> (sock);
    Ptr<TcpSocketBase> tcp = DynamicCast<TcpSocketBase> (sock);
    if (tcp)
    {
        // *server* side Rx gets the correct destination port (100+ / 200+)
        tcp->TraceConnectWithoutContext ("Rx", MakeCallback (&TracePragueRx));
    }
}

void
TraceBbrServerSocket (Ptr<Socket> sock)
{
    Ptr<Socket> s = ConstCast<Socket> (sock);
    Ptr<TcpSocketBase> tcp = DynamicCast<TcpSocketBase> (sock);
    if (tcp)
    {
        tcp->TraceConnectWithoutContext ("Rx", MakeCallback (&TraceBbrRx));
    }
}

void TracePragueAppRx (uint32_t flowIdx, Ptr<const Packet> pkt,
                  const Address&, const Address&)
 {
     g_pragueData += pkt->GetSize ();
     g_pragueFlowData [flowIdx] += pkt->GetSize ();
     g_pragueTotalData[flowIdx] += pkt->GetSize ();
 }
 
void TraceBbrAppRx  (uint32_t flowIdx, Ptr<const Packet> pkt,
                  const Address&, const Address&)
 {
     g_bbrData += pkt->GetSize ();
     g_bbrFlowData [flowIdx]  += pkt->GetSize ();
     g_bbrTotalData[flowIdx]  += pkt->GetSize ();
 }

// RQ4: Enhanced analytics functions
void TracePragueSpecificSojourn(Time sojourn)
{
    double sojournMs = sojourn.GetMicroSeconds() / 1000.0;
    g_pragueSojournTimes.push_back(sojournMs);
    g_filePragueSojourn << Now().GetSeconds() << " " << sojournMs << std::endl;
}

void TraceBbrSpecificSojourn(Time sojourn)
{
    double sojournMs = sojourn.GetMicroSeconds() / 1000.0;
    g_bbrSojournTimes.push_back(sojournMs);
    g_fileBbrSojourn << Now().GetSeconds() << " " << sojournMs << std::endl;
}

void TraceAlgorithmSpecificSojourn(Ptr<const QueueDiscItem> item, Time sojourn)
{
    // Extract the packet from the queue disc item
    Ptr<const Packet> packet = item->GetPacket();
    
    // Extract IPv4 header
    Ipv4Header ipv4Header;
    packet->PeekHeader(ipv4Header);
    
    // Extract TCP header  
    TcpHeader tcpHeader;
    packet->PeekHeader(tcpHeader);
    
    // Get destination port to determine algorithm
    uint16_t destPort = tcpHeader.GetDestinationPort();
    
    double sojournMs = sojourn.GetMicroSeconds() / 1000.0;
    
    // PRECISE ALGORITHM DETERMINATION:
    // Prague flows: ports 100-199
    // BBRv3 flows: ports 200-299
    if (destPort >= 100 && destPort < 200) {
        // This is a Prague packet
        TracePragueSpecificSojourn(sojourn);
    } else if (destPort >= 200 && destPort < 300) {
        // This is a BBRv3 packet  
        TraceBbrSpecificSojourn(sojourn);
    }
    // Ignore other ports (background traffic, etc.)
}

void GenerateQueueingAnalytics(std::string testName, uint32_t numPrague, uint32_t numBbr)
{
    if (!g_fileQueueingAnalytics.is_open()) return;
    
    g_fileQueueingAnalytics << "=== L4S ALGORITHM QUEUEING ANALYTICS (" << testName << ") ===" << std::endl;
    g_fileQueueingAnalytics << "Simulation Time: " << Simulator::Now().GetSeconds() << " seconds" << std::endl;
    g_fileQueueingAnalytics << "Prague Flows: " << numPrague << ", BBRv3 Flows: " << numBbr << std::endl;
    g_fileQueueingAnalytics << std::endl;
    
    // Prague statistics
    if (!g_pragueSojournTimes.empty())
    {
        double pragueSum = 0, pragueMin = std::numeric_limits<double>::max(), pragueMax = 0;
        for (double sojourn : g_pragueSojournTimes)
        {
            pragueSum += sojourn;
            pragueMin = std::min(pragueMin, sojourn);
            pragueMax = std::max(pragueMax, sojourn);
        }
        double pragueMean = pragueSum / g_pragueSojournTimes.size();
        
        g_fileQueueingAnalytics << "PRAGUE QUEUEING DELAYS:" << std::endl;
        g_fileQueueingAnalytics << "  Mean: " << std::fixed << std::setprecision(3) << pragueMean << " ms" << std::endl;
        g_fileQueueingAnalytics << "  Min:  " << pragueMin << " ms" << std::endl;
        g_fileQueueingAnalytics << "  Max:  " << pragueMax << " ms" << std::endl;
        g_fileQueueingAnalytics << "  Samples: " << g_pragueSojournTimes.size() << std::endl;
    }
    
    // BBRv3 statistics  
    if (!g_bbrSojournTimes.empty())
    {
        double bbrSum = 0, bbrMin = std::numeric_limits<double>::max(), bbrMax = 0;
        for (double sojourn : g_bbrSojournTimes)
        {
            bbrSum += sojourn;
            bbrMin = std::min(bbrMin, sojourn);
            bbrMax = std::max(bbrMax, sojourn);
        }
        double bbrMean = bbrSum / g_bbrSojournTimes.size();
        
        g_fileQueueingAnalytics << std::endl << "BBRv3 QUEUEING DELAYS:" << std::endl;
        g_fileQueueingAnalytics << "  Mean: " << std::fixed << std::setprecision(3) << bbrMean << " ms" << std::endl;
        g_fileQueueingAnalytics << "  Min:  " << bbrMin << " ms" << std::endl;
        g_fileQueueingAnalytics << "  Max:  " << bbrMax << " ms" << std::endl;
        g_fileQueueingAnalytics << "  Samples: " << g_bbrSojournTimes.size() << std::endl;
        
        // COMPARISON
        if (!g_pragueSojournTimes.empty())
        {
            double pragueSum = 0;
            for (double sojourn : g_pragueSojournTimes) pragueSum += sojourn;
            double pragueMean = pragueSum / g_pragueSojournTimes.size();
            
            g_fileQueueingAnalytics << std::endl << "ALGORITHM COMPARISON:" << std::endl;
            g_fileQueueingAnalytics << "  BBRv3 vs Prague Mean Delay Ratio: " << (bbrMean / pragueMean) << "x" << std::endl;
            g_fileQueueingAnalytics << "  BBRv3 Mean - Prague Mean: " << (bbrMean - pragueMean) << " ms" << std::endl;
        }
    }
    
    g_fileQueueingAnalytics << "=================================" << std::endl;
}

bool IsPortPrague(uint16_t port)
{
    return (port >= 100 && port < 200);  // Prague ports: 100-199
}

bool IsPortBbr(uint16_t port)
{
    return (port >= 200 && port < 300);  // BBRv3 ports: 200-299
}

void
TracePraguePerFlowThroughput()
{
    for (uint32_t i = 0; i < g_pragueFlowData.size(); i++)
    {
        double throughput = (g_pragueFlowData[i] * 8) / g_pragueThroughputInterval.GetSeconds() / 1e6;
        g_filePraguePerFlowThroughput << Now().GetSeconds() << " " << (100 + i) << " " 
                                      << std::fixed << throughput << std::endl;
        g_pragueFlowData[i] = 0; // Reset for next interval
    }
    Simulator::Schedule(g_pragueThroughputInterval, &TracePraguePerFlowThroughput);
}

void
TraceBbrPerFlowThroughput()
{
    for (uint32_t i = 0; i < g_bbrFlowData.size(); i++)
    {
        double throughput = (g_bbrFlowData[i] * 8) / g_bbrThroughputInterval.GetSeconds() / 1e6;
        g_fileBbrPerFlowThroughput << Now().GetSeconds() << " " << (200 + i) << " " 
                                     << std::fixed << throughput << std::endl;
        g_bbrFlowData[i] = 0; // Reset for next interval
    }
    Simulator::Schedule(g_bbrThroughputInterval, &TraceBbrPerFlowThroughput);
}

double
CalculateJainsFairnessIndex(const std::vector<uint32_t>& flowData)
{
    if (flowData.empty())
    {
        return 1.0; // Perfect fairness for empty set
    }
    
    double sumThroughput = 0.0;
    double sumSquaredThroughput = 0.0;
    double totalTimeSeconds = Simulator::Now().GetSeconds(); // RQ3: Use actual simulation time
    uint32_t totalFlows = flowData.size(); // RQ3: Include ALL flows, even starved ones
    
    for (uint32_t data : flowData)
    {
        // RQ3: Include flows with 0 data (starved flows) in calculation
        double throughput = (data * 8) / totalTimeSeconds / 1e6; // RQ3: Fixed time calculation
        sumThroughput += throughput;
        sumSquaredThroughput += throughput * throughput;
    }
    
    if (totalFlows == 0 || sumSquaredThroughput == 0.0)
    {
        return 1.0; // Perfect fairness when no flows or all zero
    }
    
    // Jain's Fairness Index: (sum of xi)^2 / (n * sum of xi^2)
    // RQ3: Use totalFlows (not activeFlows) to properly penalize starved flows
    return (sumThroughput * sumThroughput) / (totalFlows * sumSquaredThroughput);
}

void
ReportFairnessStatistics(std::string testName, uint32_t numPrague, uint32_t numBbr)
{
    std::cout << std::endl << "=== RQ4 Fairness Analysis for " << testName << " ===" << std::endl;
    
    // Calculate individual algorithm fairness using total data (never reset)
    if (numPrague > 0)
    {
        double pragueJFI = CalculateJainsFairnessIndex(g_pragueTotalData);
        std::cout << "Prague flows JFI: " << std::fixed << std::setprecision(4) << pragueJFI << std::endl;
    }
    
    if (numBbr > 0)
    {
        double bbrJFI = CalculateJainsFairnessIndex(g_bbrTotalData);
        std::cout << "BBRv3 flows JFI: " << std::fixed << std::setprecision(4) << bbrJFI << std::endl;
    }
    
    // Calculate overall fairness across all flows using total data
    std::vector<uint32_t> allFlowTotalData;
    allFlowTotalData.insert(allFlowTotalData.end(), g_pragueTotalData.begin(), g_pragueTotalData.end());
    allFlowTotalData.insert(allFlowTotalData.end(), g_bbrTotalData.begin(), g_bbrTotalData.end());
    
    if (!allFlowTotalData.empty())
    {
        double overallJFI = CalculateJainsFairnessIndex(allFlowTotalData);
        std::cout << "Overall JFI (all flows): " << std::fixed << std::setprecision(4) << overallJFI << std::endl;
        
        // Report individual flow throughputs using total data
        std::cout << "Individual flow throughputs (Mbps):" << std::endl;
        for (uint32_t i = 0; i < g_pragueTotalData.size(); i++)
        {
            double throughput = (g_pragueTotalData[i] * 8) / Simulator::Now().GetSeconds() / 1e6;
            std::cout << "  Prague-" << i << ": " << std::fixed << std::setprecision(2) << throughput << std::endl;
        }
        for (uint32_t i = 0; i < g_bbrTotalData.size(); i++)
        {
            double throughput = (g_bbrTotalData[i] * 8) / Simulator::Now().GetSeconds() / 1e6;
            std::cout << "  BBRv3-" << i << ": " << std::fixed << std::setprecision(2) << throughput << std::endl;
        }
    }
    
    std::cout << "=================================" << std::endl;
}