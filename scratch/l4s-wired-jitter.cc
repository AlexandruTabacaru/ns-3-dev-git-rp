/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2023 CableLabs (L4S over wired scenario with RTT jitter)
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

// Nodes 0               Node 1                     Node 2          Nodes 3+
//                                                         ------->
// server -------------> router ------------------> router -------> N clients
//        2 Gbps;               configurable rate;         -------> (foreground/background)
//        configurable          100 us base RTT            2 Gbps;
//        base RTT                                         100 us base RTT
//
// This version adds configurable RTT jitter to test L4S performance under realistic
// network conditions with varying RTTs.

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/stats.h"
#include "ns3/traffic-control-module.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/packet.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace ns3 {

static void
DropTailEnqueueTrace (Ptr<const Packet> p)
{
  std::cout << ">>> DT Enqueue @ "
            << Simulator::Now().GetSeconds() << "s  "
            << "pkt=" << p->GetSize() << "B\n";
}

static void
DropTailDropTrace (Ptr<const Packet> p)
{
  std::cout << ">>> DT   Drop @ "
            << Simulator::Now().GetSeconds() << "s  "
            << "pkt=" << p->GetSize() << "B\n";
}

}

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("L4sWiredJitter");

// Declare trace functions that are defined later in this file
std::ofstream g_fileBytesInAcBeQueue;
void TraceBytesInAcBeQueue(uint32_t oldVal, uint32_t newVal);
std::ofstream g_fileBytesInDualPi2Queue;
void TraceBytesInDualPi2Queue(uint32_t oldVal, uint32_t newVal);
std::ofstream g_fileLSojourn;
void TraceLSojourn(Time sojourn);
std::ofstream g_fileCSojourn;
void TraceCSojourn(Time sojourn);
std::ofstream g_fileBytesInFqCoDelQueue;
void TraceBytesInFqCoDelQueue(uint32_t oldVal, uint32_t newVal);

// Add jitter-related variables
double g_jitterUs = 0.0;  // Jitter in microseconds
static double                 g_currentOffsetUs = 0.0;          // running jitter offset (µs)
static double                 g_maxStepUs       = 0.0;          // max |step| per update (µs)
static Time                   g_jitterPeriod    = MilliSeconds(10); // update every 10 ms
static Ptr<UniformRandomVariable> g_stepRng;                    // generates ±step
Ptr<UniformRandomVariable> g_jitterRng;
Time g_wanBaseDelay = MilliSeconds(0);  // Base delay for the server->R1 link

// Add throughput tracking variables
uint32_t g_pragueData = 0;
Time g_lastSeenPrague = Seconds(0);
std::ofstream g_filePragueThroughput;
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
void TracePragueCwnd(uint32_t oldVal, uint32_t newVal);
void TracePragueSsthresh(uint32_t oldVal, uint32_t newVal);
void TracePraguePacingRate(DataRate oldVal, DataRate newVal);
void TracePragueCongState(TcpSocketState::TcpCongState_t oldVal,
                          TcpSocketState::TcpCongState_t newVal);
void TracePragueEcnState(TcpSocketState::EcnState_t oldVal, TcpSocketState::EcnState_t newVal);
void TracePragueRtt(Time oldVal, Time newVal);
void TracePragueSocket(Ptr<Application>, uint32_t);

uint32_t g_cubicData = 0;
Time g_lastSeenCubic = Seconds(0);
std::ofstream g_fileCubicThroughput;
std::ofstream g_fileCubicCwnd;
std::ofstream g_fileCubicSsthresh;
std::ofstream g_fileCubicSendInterval;
std::ofstream g_fileCubicPacingRate;
std::ofstream g_fileCubicCongState;
std::ofstream g_fileCubicRtt;
Time g_cubicThroughputInterval = MilliSeconds(100);
void TraceCubicThroughput();
void TraceCubicTx(Ptr<const Packet> packet,
                  const TcpHeader& header,
                  Ptr<const TcpSocketBase> socket);
void TraceCubicCwnd(uint32_t oldVal, uint32_t newVal);
void TraceCubicSsthresh(uint32_t oldVal, uint32_t newVal);
void TraceCubicPacingRate(DataRate oldVal, DataRate newVal);
void TraceCubicCongState(TcpSocketState::TcpCongState_t oldVal,
                         TcpSocketState::TcpCongState_t newVal);
void TraceCubicRtt(Time oldVal, Time newVal);
void TraceCubicSocket(Ptr<Application>, uint32_t);

// Count the number of flows to wait for completion before stopping the simulation
uint32_t g_flowsToClose = 0;
// Hook these methods to the PacketSink objects
void HandlePeerClose(std::string context, Ptr<const Socket> socket);
void HandlePeerError(std::string context, Ptr<const Socket> socket);

// These methods work around the lack of ability to configure different TCP socket types
// on the same node on a per-socket (per-application) basis. Instead, these methods can
// be scheduled (right before a socket creation) to change the default value
void ConfigurePragueSockets(Ptr<TcpL4Protocol> tcp1, Ptr<TcpL4Protocol> tcp2);
void ConfigureCubicSockets(Ptr<TcpL4Protocol> tcp1, Ptr<TcpL4Protocol> tcp2);

// Declare some statistics counters here so that they are updated in traces
MinMaxAvgTotalCalculator<uint32_t> pragueThroughputCalculator; // units of Mbps
MinMaxAvgTotalCalculator<uint32_t> cubicThroughputCalculator;  // units of Mbps

// Add sink RX tracing variables
std::ofstream g_filePragueSinkRx;
std::ofstream g_fileCubicSinkRx;
Time g_sinkRxInterval = MilliSeconds(100);
uint32_t g_pragueSinkRx = 0;
uint32_t g_cubicSinkRx = 0;
void TracePragueSinkRx(Ptr<const Packet> packet, const Address& from);
void TraceCubicSinkRx(Ptr<const Packet> packet, const Address& from);
void TracePragueSinkRxPeriodic();
void TraceCubicSinkRxPeriodic();

void TraceDualPi2Drop(Ptr<const QueueDiscItem> item)
{
    std::cout << "DualPI2 DROP  " << Simulator::Now().GetSeconds()
              << "  " << item->GetSize() << " B\n";
}

void TraceWanDrop(Ptr<const Packet> pkt)
{
    std::cout << "WAN DROP   " << Simulator::Now().GetSeconds()
              << "  " << pkt->GetSize() << " B\n";
}


// Function to update channel delay with jitter
void
UpdateChannelDelay (Ptr<PointToPointChannel> ch)
{
    // No jitter requested → keep static delay and bail out
    if (g_jitterUs <= 0.0)
        return;

    /* ---- bounded random-walk update of g_currentOffsetUs ---------------- */
    double step = g_stepRng->GetValue ();                   // ± step  (µs)
    double newOffset = g_currentOffsetUs + step;

    // Reflect at the ± jitter amplitude boundaries
    if (newOffset >  g_jitterUs)  newOffset =  g_jitterUs;
    if (newOffset < -g_jitterUs)  newOffset = -g_jitterUs;

    g_currentOffsetUs = newOffset;

    /* ---- apply the new one-way propagation delay ------------------------ */
    Time newDelay = g_wanBaseDelay + MicroSeconds (g_currentOffsetUs);

    // Never go below 1 µs for safety
    if (newDelay < MicroSeconds (1))
        newDelay = MicroSeconds (1);

    ch->SetAttribute ("Delay", TimeValue (newDelay));

    /* ---- reschedule next update ---------------------------------------- */
    Simulator::Schedule (g_jitterPeriod, &UpdateChannelDelay, ch);
}

int
main(int argc, char* argv[])
{
    // Variable declaration, and constants
    Time progressInterval = Seconds(5);

    // Variables that can be changed by command-line argument
    uint32_t numCubic = 1;
    uint32_t numPrague = 1;
    uint32_t numBackground = 0;
    uint32_t numBytes = 0;              // 0 means unlimited
    Time duration = Seconds(60);        // Default 60 second runtime
    Time wanLinkDelay = MilliSeconds(10); // base RTT is 20ms
    DataRate bottleneckRate = DataRate("100Mbps");
    bool useReno = false;
    bool showProgress = false;
    bool enablePcapAll = false;
    bool enablePcap = false;            // Disabled by default
    bool enableDualPI2 = true;          // Enable DualPI2 by default
    std::string lossSequence = "";
    std::string lossBurst = "";
    std::string testName = "";
    uint32_t rngRun = 1;                // Random number generator run number
    uint32_t sndBufSize = 0;            // 0 means auto-calculate based on BDP
    uint32_t rcvBufSize = 0;            // 0 means auto-calculate based on BDP

    // Increase some defaults (command-line can override below)
    // ns-3 TCP does not automatically adjust MSS from the device MTU
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    // Enable pacing for all TCP variants
    Config::SetDefault("ns3::TcpSocketState::EnablePacing", BooleanValue(true));
    Config::SetDefault("ns3::TcpSocketState::PaceInitialWindow", BooleanValue(true));
    // Enable ECN globally
    Config::SetDefault("ns3::TcpSocketBase::UseEcn", StringValue("On"));
    // Enable a timestamp (for latency sampling) in the bulk send application
    Config::SetDefault("ns3::BulkSendApplication::EnableSeqTsSizeHeader", BooleanValue(true));
    Config::SetDefault("ns3::PacketSink::EnableSeqTsSizeHeader", BooleanValue(true));
    // The bulk send application should do 1448-byte writes (one timestamp per TCP packet)
    Config::SetDefault("ns3::BulkSendApplication::SendSize", UintegerValue(1448));

    CommandLine cmd;
    cmd.Usage("The l4s-wired-jitter program experiments with TCP flows over L4S wired configuration with RTT jitter");
    cmd.AddValue("numCubic", "Number of foreground Cubic flows", numCubic);
    cmd.AddValue("numPrague", "Number of foreground Prague flows", numPrague);
    cmd.AddValue("numBackground", "Number of background flows", numBackground);
    cmd.AddValue("numBytes", "Number of bytes for each TCP transfer (0 for unlimited)", numBytes);
    cmd.AddValue("duration", "Scheduled end of simulation", duration);
    cmd.AddValue("wanLinkDelay", "one-way base delay from server to AP", wanLinkDelay);
    cmd.AddValue("bottleneckRate", "bottleneck data rate between routers", bottleneckRate);
    cmd.AddValue("useReno", "Use Linux Reno instead of Cubic", useReno);
    cmd.AddValue("lossSequence", "Packets to drop", lossSequence);
    cmd.AddValue("lossBurst", "Packets to drop", lossBurst);
    cmd.AddValue("testName", "Test name", testName);
    cmd.AddValue("showProgress", "Show simulation progress every 5s", showProgress);
    cmd.AddValue("enablePcapAll", "Whether to enable PCAP trace output at all interfaces", enablePcapAll);
    cmd.AddValue("enablePcap", "Whether to enable PCAP trace output only at endpoints", enablePcap);
    cmd.AddValue("jitterUs", "RTT jitter in microseconds (uniform distribution)", g_jitterUs);
    cmd.AddValue("rngRun", "Random number generator run number", rngRun);
    cmd.AddValue("enableDualPI2", "Enable DualPI2 queue disc (L4S mode)", enableDualPI2);
    cmd.AddValue("sndBufSize", "TCP send buffer size in bytes (0 for auto-calculate)", sndBufSize);
    cmd.AddValue("rcvBufSize", "TCP receive buffer size in bytes (0 for auto-calculate)", rcvBufSize);
    cmd.Parse(argc, argv);

    // Store the configured one-way base delay for the jitter callback
    g_wanBaseDelay = wanLinkDelay;

    // Calculate buffer sizes based on BDP if not specified
    if (sndBufSize == 0 || rcvBufSize == 0) {
        // Calculate BDP: (rate * RTT) / 8
        // Use 2 * wanLinkDelay for RTT (round trip)
        double bdp = (bottleneckRate.GetBitRate() * (2 * wanLinkDelay.GetSeconds())) / 8;
        // Use 2 * BDP for buffer size
        uint32_t bufSize = static_cast<uint32_t>(2 * bdp);
        if (sndBufSize == 0) sndBufSize = bufSize;
        if (rcvBufSize == 0) rcvBufSize = bufSize;
    }

    // Set buffer sizes after BDP calculation and CLI parsing
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(sndBufSize));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(rcvBufSize));

    NS_ABORT_MSG_IF(numCubic == 0 && numPrague == 0,
                    "Error: configure at least one foreground flow");
    NS_ABORT_MSG_IF(numBackground > 0, "Background flows not yet supported");

    // When using DCE with ns-3, or reading pcaps with Wireshark,
    // enable checksum computations in ns-3 models
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    if (useReno)
    {
        std::cout << "Using ns-3 LinuxReno model instead of Cubic" << std::endl;
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpLinuxReno::GetTypeId()));
    }
    // Workaround until PRR response is debugged
    Config::SetDefault("ns3::TcpL4Protocol::RecoveryType", TypeIdValue(TcpClassicRecovery::GetTypeId()));

    // Initialize jitter random number generator
    g_jitterRng = CreateObject<UniformRandomVariable>();
    g_jitterRng->SetAttribute("Min", DoubleValue(-g_jitterUs));
    g_jitterRng->SetAttribute("Max", DoubleValue(g_jitterUs));
    g_jitterRng->SetStream(rngRun);

    if (g_jitterUs > 0.0)
{
    // step size = 10 % of total amplitude   (≥ 1 µs)
    g_maxStepUs = std::max (1.0, 0.10 * g_jitterUs);

    g_stepRng = CreateObject<UniformRandomVariable> ();
    g_stepRng->SetAttribute ("Min", DoubleValue (-g_maxStepUs));
    g_stepRng->SetAttribute ("Max", DoubleValue (  g_maxStepUs));
    g_stepRng->SetStream (rngRun + 99);          // different stream from g_jitterRng
}
    
    // Create the nodes and use containers for further configuration below
    NodeContainer serverNode;
    serverNode.Create(1);
    NodeContainer routerNodes;
    routerNodes.Create(2);
    NodeContainer clientNodes;
    clientNodes.Create(1 + numBackground);

    // Create point-to-point links between server and AP
    PointToPointHelper pointToPoint;
    pointToPoint.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1p"));
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("2Gbps"));
    pointToPoint.SetChannelAttribute("Delay", TimeValue(wanLinkDelay));
    NetDeviceContainer wanDevices = pointToPoint.Install(serverNode.Get(0), routerNodes.Get(0));

    // 1) Trace the DropTail right on the device queue:
    Ptr<PointToPointNetDevice> serverDev =
  DynamicCast<PointToPointNetDevice>(wanDevices.Get(0));
Ptr<DropTailQueue<Packet>> dtq =
  serverDev->GetQueue()->GetObject<DropTailQueue<Packet>>();
if (dtq)
{
//   dtq->TraceConnectWithoutContext(
//           "Enqueue",
//           MakeCallback(&DropTailEnqueueTrace));
      dtq->TraceConnectWithoutContext(
          "Drop",
          MakeCallback(&DropTailDropTrace));
}
    wanDevices.Get(0)                             // server side NIC
    ->TraceConnectWithoutContext("PhyTxDrop", // or "PhyRxDrop"
        MakeCallback(&TraceWanDrop));
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

    // Set up jitter updates for the server->R1 link instead of bottleneck
    Ptr<PointToPointChannel> serverLink = 
        wanDevices.Get(0)->GetChannel()->GetObject<PointToPointChannel>();
    if (serverLink) {
        Simulator::ScheduleNow (&UpdateChannelDelay, serverLink);
    }

    // Internet and Linux stack installation
    InternetStackHelper internetStack;
    internetStack.Install(serverNode);
    internetStack.Install(routerNodes);
    internetStack.Install(clientNodes);

    // By default, Ipv4AddressHelper below will configure a FqCoDelQueueDiscs on routers
    // The following statements change this configuration on the bottleneck link
    TrafficControlHelper tch;
    if (enableDualPI2)
    {
        tch.SetRootQueueDisc("ns3::DualPi2QueueDisc");
        tch.SetQueueLimits("ns3::DynamicQueueLimits"); // enable BQL
    }
    else
    {
        tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    }
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
    Time pragueStartOffset = MilliSeconds(50) / (numPrague + 1);
    for (auto i = 0U; i < numPrague; i++)
    {
        BulkSendHelper bulk("ns3::TcpSocketFactory", InetSocketAddress(clientInterfaces.GetAddress(1), port + i));
        bulk.SetAttribute("MaxBytes", UintegerValue(numBytes));
        bulk.SetAttribute("StartTime", TimeValue(Seconds(1.0) + i * pragueStartOffset));
        pragueServerApps.Add(bulk.Install(serverNode.Get(0)));
        NS_LOG_DEBUG("Creating Prague foreground flow " << i);
        PacketSinkHelper sink = PacketSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + i));
        sink.SetAttribute("StartTime", TimeValue(Seconds(1.0) + i * pragueStartOffset));
        pragueClientApps.Add(sink.Install(clientNodes.Get(0)));
        g_flowsToClose++;
        Simulator::Schedule(Seconds(1.0) - TimeStep(1), MakeBoundCallback(&ConfigurePragueSockets, tcpL4ProtocolServer, tcpL4ProtocolClient));
    }

    // Application configuration for Cubic flows under test
    port = 200;
    ApplicationContainer cubicServerApps;
    ApplicationContainer cubicClientApps;
    Time cubicStartOffset = MilliSeconds(50) / (numCubic + 1);
    for (auto i = 0U; i < numCubic; i++)
    {
        BulkSendHelper bulkCubic("ns3::TcpSocketFactory", InetSocketAddress(clientInterfaces.GetAddress(1), port + i));
        bulkCubic.SetAttribute("MaxBytes", UintegerValue(numBytes));
        bulkCubic.SetAttribute("StartTime", TimeValue(Seconds(1.05) + i * cubicStartOffset));
        cubicServerApps.Add(bulkCubic.Install(serverNode.Get(0)));
        NS_LOG_DEBUG("Creating Cubic foreground flow " << i);
        PacketSinkHelper sinkCubic = PacketSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + i));
        sinkCubic.SetAttribute("StartTime", TimeValue(Seconds(1.05) + i * cubicStartOffset));
        cubicClientApps.Add(sinkCubic.Install(clientNodes.Get(0)));
        g_flowsToClose++;
        Simulator::Schedule(Seconds(1.05) - TimeStep(1), MakeBoundCallback(&ConfigureCubicSockets, tcpL4ProtocolServer, tcpL4ProtocolClient));
    }

    // PCAP traces
    if (enablePcapAll)
    {
        std::string prefixName = "l4s-wired-jitter" + ((testName != "") ? ("-" + testName) : "");
        pointToPoint.EnablePcapAll(prefixName.c_str());
    }
    else if (enablePcap)
    {
        std::string prefixName = "l4s-wired-jitter" + ((testName != "") ? ("-" + testName) : "");
        pointToPoint.EnablePcap(prefixName.c_str(), wanDevices.Get(0));
        pointToPoint.EnablePcap(prefixName.c_str(), clientDevices.Get(0));
    }

    // Set up tracing and callbacks
    if (pragueClientApps.GetN())
    {
        std::string traceName = "prague-throughput." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_filePragueThroughput.open(traceName.c_str(), std::ofstream::out);
        traceName = "prague-sink-rx." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_filePragueSinkRx.open(traceName.c_str(), std::ofstream::out);
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
        Simulator::Schedule(Seconds(1.0) + i * MilliSeconds(10) + TimeStep(1), MakeBoundCallback(&TracePragueSocket, pragueServerApps.Get(i), i));
        std::ostringstream oss;
        oss << "Prague:" << i;
        NS_LOG_DEBUG("Setting up callbacks on Prague sockets " << pragueClientApps.Get(i));
        pragueClientApps.Get(i)->GetObject<PacketSink>()->TraceConnect("PeerClose", oss.str(), MakeCallback(&HandlePeerClose));
        pragueClientApps.Get(i)->GetObject<PacketSink>()->TraceConnect("PeerError", oss.str(), MakeCallback(&HandlePeerError));
        pragueClientApps.Get(i)->GetObject<PacketSink>()->TraceConnectWithoutContext("Rx", MakeCallback(&TracePragueSinkRx));
        if (i == 0) {
            Simulator::Schedule(g_sinkRxInterval, &TracePragueSinkRxPeriodic);
        }
    }

    if (cubicClientApps.GetN())
    {
        std::string traceName = "cubic-throughput." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileCubicThroughput.open(traceName.c_str(), std::ofstream::out);
        traceName = "cubic-sink-rx." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileCubicSinkRx.open(traceName.c_str(), std::ofstream::out);
        traceName = "cubic-cwnd." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileCubicCwnd.open(traceName.c_str(), std::ofstream::out);
        traceName = "cubic-ssthresh." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileCubicSsthresh.open(traceName.c_str(), std::ofstream::out);
        traceName = "cubic-send-interval." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileCubicSendInterval.open(traceName.c_str(), std::ofstream::out);
        traceName = "cubic-pacing-rate." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileCubicPacingRate.open(traceName.c_str(), std::ofstream::out);
        traceName = "cubic-cong-state." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileCubicCongState.open(traceName.c_str(), std::ofstream::out);
        traceName = "cubic-rtt." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileCubicRtt.open(traceName.c_str(), std::ofstream::out);
    }

    for (auto i = 0U; i < cubicClientApps.GetN(); i++)
    {
        Simulator::Schedule(Seconds(1.05) + i * MilliSeconds(10) + TimeStep(1), MakeBoundCallback(&TraceCubicSocket, cubicServerApps.Get(i), i));
        std::ostringstream oss;
        oss << "Cubic:" << i;
        NS_LOG_DEBUG("Setting up callbacks on Cubic sockets " << i << " " << cubicClientApps.Get(i));
        cubicClientApps.Get(i)->GetObject<PacketSink>()->TraceConnect("PeerClose", oss.str(), MakeCallback(&HandlePeerClose));
        cubicClientApps.Get(i)->GetObject<PacketSink>()->TraceConnect("PeerError", oss.str(), MakeCallback(&HandlePeerError));
        cubicClientApps.Get(i)->GetObject<PacketSink>()->TraceConnectWithoutContext("Rx", MakeCallback(&TraceCubicSinkRx));
        if (i == 0) {
            Simulator::Schedule(g_sinkRxInterval, &TraceCubicSinkRxPeriodic);
        }
    }

    // Trace bytes in DualPi2 queue
    Ptr<DualPi2QueueDisc> dualPi2 = 
        enableDualPI2 ? routerQueueDiscContainer.Get(0)->GetObject<DualPi2QueueDisc>()
                      : nullptr;
    Ptr<FqCoDelQueueDisc> fqcodel = 
        !enableDualPI2 ? routerQueueDiscContainer.Get(0)->GetObject<FqCoDelQueueDisc>()
                      : nullptr;

    if (dualPi2)        // only connect traces when the queue actually exists
    {
        dualPi2->TraceConnectWithoutContext("Drop",
        MakeCallback(&TraceDualPi2Drop));
        std::string traceName = "wired-dualpi2-bytes." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileBytesInDualPi2Queue.open(traceName.c_str(), std::ofstream::out);
        dualPi2->TraceConnectWithoutContext("BytesInQueue", MakeCallback(&TraceBytesInDualPi2Queue));
        traceName = "wired-dualpi2-l-sojourn." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileLSojourn.open(traceName.c_str(), std::ofstream::out);
        dualPi2->TraceConnectWithoutContext("L4sSojournTime", MakeCallback(&TraceLSojourn));
        traceName = "wired-dualpi2-c-sojourn." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileCSojourn.open(traceName.c_str(), std::ofstream::out);
        dualPi2->TraceConnectWithoutContext("ClassicSojournTime", MakeCallback(&TraceCSojourn));
    }
    else if (fqcodel)   // connect traces for FqCoDel queue
    {
        std::string traceName = "wired-fqcodel-bytes." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileBytesInFqCoDelQueue.open(traceName.c_str(), std::ofstream::out);
        fqcodel->TraceConnectWithoutContext("BytesInQueue", MakeCallback(&TraceBytesInFqCoDelQueue));
        
        traceName = "wired-fqcodel-sojourn." + ((testName != "") ? (testName + ".") : "") + "dat";
        g_fileLSojourn.open(traceName.c_str(), std::ofstream::out);
        fqcodel->TraceConnectWithoutContext("SojournTime",
                                        MakeCallback(&TraceLSojourn));
        // Back-compat: some ns-3 versions still call it "Sojourn"
        fqcodel->TraceConnectWithoutContext("Sojourn",
                                            MakeCallback(&TraceLSojourn));
    
        // Optional debug to stdout so you see it's hooked
        std::cout << "*** FqCoDel Sojourn trace hooked at "
              << Simulator::Now().GetSeconds() << "s\n";
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

    std::cout << "Foreground flows: Cubic: " << numCubic << " Prague: " << numPrague << std::endl;
    std::cout << "Background flows: " << numBackground << std::endl;
    std::cout << "RTT jitter: ±" << g_jitterUs << " microseconds" << std::endl;
    std::cout << "Random seed: " << rngRun << std::endl;

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
        std::cout << "** Expected " << numCubic + numPrague << " flows to close, but "
                  << g_flowsToClose << " are remaining" << std::endl
                  << std::endl;
    }

    std::cout << std::fixed << std::setprecision(2);
    if (numCubic)
    {
        std::cout << "Cubic throughput (Mbps) mean: " << cubicThroughputCalculator.getMean()
                  << " max: " << cubicThroughputCalculator.getMax()
                  << " min: " << cubicThroughputCalculator.getMin() << std::endl;
    }
    if (numPrague)
    {
        std::cout << "Prague throughput (Mbps) mean: " << pragueThroughputCalculator.getMean()
                  << " max: " << pragueThroughputCalculator.getMax()
                  << " min: " << pragueThroughputCalculator.getMin() << std::endl;
    }

    g_fileBytesInDualPi2Queue.close();
    g_fileBytesInFqCoDelQueue.close();
    g_fileLSojourn.close();
    g_fileCSojourn.close();
    g_filePragueThroughput.close();
    g_filePragueCwnd.close();
    g_filePragueSsthresh.close();
    g_filePragueSendInterval.close();
    g_filePraguePacingRate.close();
    g_filePragueCongState.close();
    g_filePragueEcnState.close();
    g_filePragueRtt.close();
    g_fileCubicThroughput.close();
    g_fileCubicCwnd.close();
    g_fileCubicSsthresh.close();
    g_fileCubicSendInterval.close();
    g_fileCubicPacingRate.close();
    g_fileCubicCongState.close();
    g_fileCubicRtt.close();
    g_filePragueSinkRx.close();
    g_fileCubicSinkRx.close();
    Simulator::Destroy();
    return 0;
}

// Copy all the remaining functions from l4s-wired.cc
void ConfigurePragueSockets(Ptr<TcpL4Protocol> tcp1, Ptr<TcpL4Protocol> tcp2)
{
    tcp1->SetAttribute("SocketType", TypeIdValue(TcpPrague::GetTypeId()));
    tcp2->SetAttribute("SocketType", TypeIdValue(TcpPrague::GetTypeId()));
}

void ConfigureCubicSockets(Ptr<TcpL4Protocol> tcp1, Ptr<TcpL4Protocol> tcp2)
{
    tcp1->SetAttribute("SocketType", TypeIdValue(TcpCubic::GetTypeId()));
    tcp2->SetAttribute("SocketType", TypeIdValue(TcpCubic::GetTypeId()));
}

void TraceBytesInDualPi2Queue(uint32_t oldVal, uint32_t newVal)
{
    g_fileBytesInDualPi2Queue << Now().GetSeconds() << " " << newVal << std::endl;
}

void TraceLSojourn(Time sojourn)
{
    g_fileLSojourn << Now().GetSeconds() << " " << sojourn.GetMicroSeconds() / 1000.0 << std::endl;
}

void TraceCSojourn(Time sojourn)
{
    g_fileCSojourn << Now().GetSeconds() << " " << sojourn.GetMicroSeconds() / 1000.0 << std::endl;
}

void TracePragueTx(Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket)
{
    g_pragueData += packet->GetSize();
    if (g_lastSeenPrague > Seconds(0))
    {
        g_filePragueSendInterval << std::fixed << std::setprecision(6) << Now().GetSeconds() << " "
                                 << (Now() - g_lastSeenPrague).GetSeconds() << std::endl;
    }
    g_lastSeenPrague = Now();
}

void TracePragueThroughput()
{
    g_filePragueThroughput << Now().GetSeconds() << " " << std::fixed
                           << (g_pragueData * 8) / g_pragueThroughputInterval.GetSeconds() / 1e6
                           << std::endl;
    pragueThroughputCalculator.Update((g_pragueData * 8) / g_pragueThroughputInterval.GetSeconds() /
                                      1e6);
    Simulator::Schedule(g_pragueThroughputInterval, &TracePragueThroughput);
    g_pragueData = 0;
}

void TracePragueCwnd(uint32_t oldVal, uint32_t newVal)
{
    g_filePragueCwnd << Now().GetSeconds() << " " << newVal << std::endl;
}

void TracePragueSsthresh(uint32_t oldVal, uint32_t newVal)
{
    g_filePragueSsthresh << Now().GetSeconds() << " " << newVal << std::endl;
}

void TracePraguePacingRate(DataRate oldVal, DataRate newVal)
{
    g_filePraguePacingRate << Now().GetSeconds() << " " << newVal.GetBitRate() << std::endl;
}

void TracePragueCongState(TcpSocketState::TcpCongState_t oldVal, TcpSocketState::TcpCongState_t newVal)
{
    g_filePragueCongState << Now().GetSeconds() << " " << TcpSocketState::TcpCongStateName[newVal]
                          << std::endl;
}

void TracePragueEcnState(TcpSocketState::EcnState_t oldVal, TcpSocketState::EcnState_t newVal)
{
    g_filePragueEcnState << Now().GetSeconds() << " " << TcpSocketState::EcnStateName[newVal]
                         << std::endl;
}

void TracePragueRtt(Time oldVal, Time newVal)
{
    g_filePragueRtt << Now().GetSeconds() << " " << newVal.GetMicroSeconds() / 1000.0 << std::endl;
}

void TracePragueSocket(Ptr<Application> a, uint32_t i)
{
    Ptr<BulkSendApplication> bulk = DynamicCast<BulkSendApplication>(a);
    NS_ASSERT_MSG(bulk, "Application failed");
    Ptr<Socket> s = a->GetObject<BulkSendApplication>()->GetSocket();
    NS_ASSERT_MSG(s, "Socket downcast failed");
    Ptr<TcpSocketBase> tcp = DynamicCast<TcpSocketBase>(s);
    NS_ASSERT_MSG(tcp, "TCP socket downcast failed");
    tcp->TraceConnectWithoutContext("Tx", MakeCallback(&TracePragueTx));
    if (i == 0)
    {
        tcp->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&TracePragueCwnd));
        tcp->TraceConnectWithoutContext("SlowStartThreshold", MakeCallback(&TracePragueSsthresh));
        tcp->TraceConnectWithoutContext("PacingRate", MakeCallback(&TracePraguePacingRate));
        tcp->TraceConnectWithoutContext("CongState", MakeCallback(&TracePragueCongState));
        tcp->TraceConnectWithoutContext("EcnState", MakeCallback(&TracePragueEcnState));
        tcp->TraceConnectWithoutContext("RTT", MakeCallback(&TracePragueRtt));
        Simulator::Schedule(g_pragueThroughputInterval, &TracePragueThroughput);
    }
}

void TraceCubicTx(Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket)
{
    g_cubicData += packet->GetSize();
    if (g_lastSeenCubic > Seconds(0))
    {
        g_fileCubicSendInterval << std::fixed << std::setprecision(6) << Now().GetSeconds() << " "
                                << (Now() - g_lastSeenCubic).GetSeconds() << std::endl;
    }
    g_lastSeenCubic = Now();
}

void TraceCubicThroughput()
{
    g_fileCubicThroughput << Now().GetSeconds() << " " << std::fixed
                          << (g_cubicData * 8) / g_cubicThroughputInterval.GetSeconds() / 1e6
                          << std::endl;
    cubicThroughputCalculator.Update((g_cubicData * 8) / g_cubicThroughputInterval.GetSeconds() /
                                     1e6);
    Simulator::Schedule(g_cubicThroughputInterval, &TraceCubicThroughput);
    g_cubicData = 0;
}

void TraceCubicCwnd(uint32_t oldVal, uint32_t newVal)
{
    g_fileCubicCwnd << Now().GetSeconds() << " " << newVal << std::endl;
}

void TraceCubicSsthresh(uint32_t oldVal, uint32_t newVal)
{
    g_fileCubicSsthresh << Now().GetSeconds() << " " << newVal << std::endl;
}

void TraceCubicPacingRate(DataRate oldVal, DataRate newVal)
{
    g_fileCubicPacingRate << Now().GetSeconds() << " " << newVal.GetBitRate() << std::endl;
}

void TraceCubicCongState(TcpSocketState::TcpCongState_t oldVal, TcpSocketState::TcpCongState_t newVal)
{
    g_fileCubicCongState << Now().GetSeconds() << " " << TcpSocketState::TcpCongStateName[newVal]
                         << std::endl;
}

void TraceCubicRtt(Time oldVal, Time newVal)
{
    g_fileCubicRtt << Now().GetSeconds() << " " << newVal.GetMicroSeconds() / 1000.0 << std::endl;
}

void TraceCubicSocket(Ptr<Application> a, uint32_t i)
{
    Ptr<BulkSendApplication> bulk = DynamicCast<BulkSendApplication>(a);
    NS_ASSERT_MSG(bulk, "Application failed");
    Ptr<Socket> s = a->GetObject<BulkSendApplication>()->GetSocket();
    NS_ASSERT_MSG(s, "Socket downcast failed");
    Ptr<TcpSocketBase> tcp = DynamicCast<TcpSocketBase>(s);
    NS_ASSERT_MSG(tcp, "TCP socket downcast failed");
    tcp->TraceConnectWithoutContext("Tx", MakeCallback(&TraceCubicTx));
    if (i == 0)
    {
        tcp->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&TraceCubicCwnd));
        tcp->TraceConnectWithoutContext("SlowStartThreshold", MakeCallback(&TraceCubicSsthresh));
        tcp->TraceConnectWithoutContext("PacingRate", MakeCallback(&TraceCubicPacingRate));
        tcp->TraceConnectWithoutContext("CongState", MakeCallback(&TraceCubicCongState));
        tcp->TraceConnectWithoutContext("RTT", MakeCallback(&TraceCubicRtt));
        Simulator::Schedule(g_cubicThroughputInterval, &TraceCubicThroughput);
    }
}

void HandlePeerClose(std::string context, Ptr<const Socket> socket)
{
    NS_LOG_DEBUG("Handling close of socket " << context);
    if (--g_flowsToClose == 0)
    {
        // Close 1 second after last TCP flow closes
        Simulator::Stop(Seconds(1));
    }
}

void HandlePeerError(std::string context, Ptr<const Socket> socket)
{
    NS_LOG_DEBUG("Handling abnormal close of socket " << context);
    std::cout << "Warning:  socket closed abnormally" << std::endl;
    if (--g_flowsToClose == 0)
    {
        // Close 1 second after last TCP flow closes
        Simulator::Stop(Seconds(1));
    }
}

void TracePragueSinkRx(Ptr<const Packet> packet, const Address& from)
{
    g_pragueSinkRx += packet->GetSize();
}

void TraceCubicSinkRx(Ptr<const Packet> packet, const Address& from)
{
    g_cubicSinkRx += packet->GetSize();
}

void TracePragueSinkRxPeriodic()
{
    g_filePragueSinkRx << Now().GetSeconds() << " " << g_pragueSinkRx << std::endl;
    Simulator::Schedule(g_sinkRxInterval, &TracePragueSinkRxPeriodic);
}

void TraceCubicSinkRxPeriodic()
{
    g_fileCubicSinkRx << Now().GetSeconds() << " " << g_cubicSinkRx << std::endl;
    Simulator::Schedule(g_sinkRxInterval, &TraceCubicSinkRxPeriodic);
}

void TraceBytesInFqCoDelQueue(uint32_t oldVal, uint32_t newVal)
{
    g_fileBytesInFqCoDelQueue << Now().GetSeconds() << " " << newVal << std::endl;
} 