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
// client ---------------------> AP -------------------------- > STA * N servers
//         2 Gbps
//         20 ms base RTT            BW 20/80/160 MHz            # N/2 for L4S flows
//                                   Fixed MCS                   # N/2 for classic flows
//
// One server with Prague and Cubic TCP connections to the STA under test
// The first Wi-Fi STA (node index 2) is the STA under test
// Additional STA nodes (node indices 3+) for sending background load
// 80 MHz 11ax (MCS 8) is initially configured in 5 GHz (channel 42)
// Data transfer from client to server (iperf naming convention)
//
// Configuration inputs:
// - number of Cubic flows under test
// - number of Prague flows under test
// - number of background flows
// - number of bytes for TCP flows
// - whether to disable flow control
// - Wi-Fi queue limit when flow control is enabled (base limit and scale factor)
//
// Behavior:
// - at around simulation time 1 second, each flow starts
// - simulation ends 1 second after last foreground flow terminates, unless
//   a specific duration was configured
//
// Outputs (some of these are for future definition):
// 1) PCAP files at TCP endpoints
// 2) queue depth of the overlying and Wi-Fi AC_BE queue
// 3) queue depth of the WifiMacQueue AC_BE queue
// 4) dequeue events of the WifiMacQueue
// 5) Socket statistics for the first foreground Prague and Cubic flows defined

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
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("L4sWifi");

// Declare trace functions that are defined later in this file
std::ofstream g_fileBytesInAcBeQueue;
void TraceBytesInAcBeQueue(uint32_t oldVal, uint32_t newVal);
std::ofstream g_fileBytesInDualPi2Queue;
void TraceBytesInDualPi2Queue(uint32_t oldVal, uint32_t newVal);

uint32_t g_wifiThroughputData = 0;
Time g_lastQosDataTime;
std::ofstream g_fileWifiPhyTxPsduBegin;
void TraceWifiPhyTxPsduBegin(WifiConstPsduMap psduMap, WifiTxVector txVector, double txPower);

std::ofstream g_fileWifiThroughput;
void TraceWifiThroughput();
Time g_wifiThroughputInterval = MilliSeconds(100);

std::ofstream g_fileLSojourn;
void TraceLSojourn(Time sojourn);
std::ofstream g_fileCSojourn;
void TraceCSojourn(Time sojourn);

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
void TracePragueRx(Ptr<const Packet> packet,
                   const TcpHeader& header,
                   Ptr<const TcpSocketBase> socket);
void TracePragueCwnd(uint32_t oldVal, uint32_t newVal);
void TracePragueSsthresh(uint32_t oldVal, uint32_t newVal);
void TracePraguePacingRate(DataRate oldVal, DataRate newVal);
void TracePragueCongState(TcpSocketState::TcpCongState_t oldVal,
                          TcpSocketState::TcpCongState_t newVal);
void TracePragueEcnState(TcpSocketState::EcnState_t oldVal, TcpSocketState::EcnState_t newVal);
void TracePragueRtt(Time oldVal, Time newVal);
void TracePragueClientSocket(Ptr<Application>, uint32_t);
void TracePragueServerSocket(Ptr<Application>);

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
void TraceCubicRx(Ptr<const Packet> packet,
                  const TcpHeader& header,
                  Ptr<const TcpSocketBase> socket);
void TraceCubicCwnd(uint32_t oldVal, uint32_t newVal);
void TraceCubicSsthresh(uint32_t oldVal, uint32_t newVal);
void TraceCubicPacingRate(DataRate oldVal, DataRate newVal);
void TraceCubicCongState(TcpSocketState::TcpCongState_t oldVal,
                         TcpSocketState::TcpCongState_t newVal);
void TraceCubicRtt(Time oldVal, Time newVal);
void TraceCubicClientSocket(Ptr<Application>, uint32_t);
void TraceCubicServerSocket(Ptr<Application>);

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

int
main(int argc, char* argv[])
{
    // Variable declaration, and constants
    std::string wifiControlMode = "OfdmRate24Mbps";
    double staDistance = 10; // meters
    const double pi = 3.1415927;
    Time progressInterval = Seconds(5);

    // Variables that can be changed by command-line argument
    uint32_t numCubic = 1;
    uint32_t numPrague = 1;
    uint32_t numBackground = 0;
    uint32_t numBytes = 50e6;             // default 50 MB
    Time duration = Seconds(0);           // By default, close one second after last TCP flow closes
    Time wanLinkDelay = MilliSeconds(10); // base RTT is 20ms
    bool useReno = false;
    uint16_t mcs = 2;
    uint32_t channelWidth = 80;
    uint32_t spatialStreams = 1;
    // Default WifiMacQueue depth is roughly 300 ms at 200 Mbps ~= 5000 packets
    std::string wifiQueueSize = "5000p";
    bool flowControl = true;
    uint32_t limit = 65535;       // default flow control limit (max A-MPDU size in bytes)
    double scale = 1.0;           // default flow control scale factor
    uint32_t rtsCtsThreshold = 0; // RTS/CTS disabled by default
    Time processingDelay = MicroSeconds(10);
    bool showProgress = false;

    // Increase some defaults (command-line can override below)
    // ns-3 TCP does not automatically adjust MSS from the device MTU
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    // ns-3 TCP socket buffer sizes do not dynamically grow, so set to ~3 * BWD product
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(30000000));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(40000000));
    // Enable pacing for Cubic
    Config::SetDefault("ns3::TcpSocketState::EnablePacing", BooleanValue(true));
    Config::SetDefault("ns3::TcpSocketState::PaceInitialWindow", BooleanValue(true));
    // Enable a timestamp (for latency sampling) in the bulk send application
    Config::SetDefault("ns3::BulkSendApplication::EnableSeqTsSizeHeader", BooleanValue(true));
    Config::SetDefault("ns3::PacketSink::EnableSeqTsSizeHeader", BooleanValue(true));
    // The bulk send application should do 1448-byte writes (one timestamp per TCP packet)
    Config::SetDefault("ns3::BulkSendApplication::SendSize", UintegerValue(1448));
    // Bypass Laqm when using Wi-Fi
    Config::SetDefault("ns3::DualPi2QueueDisc::DisableLaqm", BooleanValue(true));
    // Set AC_BE max AMPDU to maximum 802.11ax value
    Config::SetDefault("ns3::WifiMac::BE_MaxAmpduSize", UintegerValue(6500631));
#if 0
    // Set AC_BE max AMSDU to four packets
    Config::SetDefault("ns3::WifiMac::BE_MaxAmsduSize", UintegerValue(6500));
#endif

    CommandLine cmd;
    cmd.Usage("The l4s-wifi program experiments with TCP flows over L4S Wi-Fi configuration");
    cmd.AddValue("numCubic", "Number of foreground Cubic flows", numCubic);
    cmd.AddValue("numPrague", "Number of foreground Prague flows", numPrague);
    cmd.AddValue("numBackground", "Number of background flows", numBackground);
    cmd.AddValue("numBytes", "Number of bytes for each TCP transfer", numBytes);
    cmd.AddValue("duration", "(optional) scheduled end of simulation", duration);
    cmd.AddValue("wanLinkDelay", "one-way base delay from server to AP", wanLinkDelay);
    cmd.AddValue("useReno", "Use Linux Reno instead of Cubic", useReno);
    cmd.AddValue("mcs", "Index (0-11) of 11ax HE MCS", mcs);
    cmd.AddValue("channelWidth", "Width (MHz) of channel", channelWidth);
    cmd.AddValue("spatialStreams", "Number of spatial streams", spatialStreams);
    cmd.AddValue("wifiQueueSize", "WifiMacQueue size", wifiQueueSize);
    cmd.AddValue("flowControl", "Whether to enable flow control (set also the limit)", flowControl);
    cmd.AddValue("limit", "Queue limit (bytes)", limit);
    cmd.AddValue("scale", "Scaling factor for queue limit", scale);
    cmd.AddValue("rtsCtsThreshold", "RTS/CTS threshold (bytes)", rtsCtsThreshold);
    cmd.AddValue("processingDelay", "Notional packet processing delay", processingDelay);
    cmd.AddValue("showProgress", "Show simulation progress every 5s", showProgress);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_UNLESS(mcs < 12, "Only MCS 0-11 supported");
    if (processingDelay > Seconds(0))
    {
        Config::SetDefault("ns3::WifiMacQueue::ProcessingDelay", TimeValue(processingDelay));
    }
    if (rtsCtsThreshold > 0)
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold",
                           UintegerValue(rtsCtsThreshold));
    }
    std::ostringstream ossDataMode;
    ossDataMode << "HeMcs" << mcs;

    NS_ABORT_MSG_UNLESS(channelWidth == 20 || channelWidth == 40 || channelWidth == 80 ||
                            channelWidth == 160,
                        "Only widths 20, 40, 80, 160 supported");
    // ns-3 format for Wi-Fi channel configuration:
    // {channelNumber, channelWidth(MHz), band, and primary 20 MHz index}
    // channel number of zero will cause the first such channel in the band to be used
    std::string channelString("{0, " + std::to_string(channelWidth) + ", BAND_5GHZ, 0}");

    // When using DCE with ns-3, or reading pcaps with Wireshark,
    // enable checksum computations in ns-3 models
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    Config::SetDefault("ns3::WifiMacQueue::MaxSize", StringValue(wifiQueueSize));

    if (useReno)
    {
        std::cout << "Using ns-3 LinuxReno model instead of Cubic" << std::endl;
        Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                           TypeIdValue(TcpLinuxReno::GetTypeId()));
    }

    // Create the nodes and use containers for further configuration below
    NodeContainer clientNode;
    clientNode.Create(1);
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(1 + numBackground);

    // Create point-to-point links between server and AP
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("2Gbps"));
    pointToPoint.SetChannelAttribute("Delay", TimeValue(wanLinkDelay));
    NetDeviceContainer wanDevices = pointToPoint.Install(clientNode.Get(0), apNode.Get(0));

    // Wifi configuration; use the simpler Yans physical layer model
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                                   "Exponent",
                                   DoubleValue(2.0),
                                   "ReferenceDistance",
                                   DoubleValue(1.0),
                                   "ReferenceLoss",
                                   DoubleValue(46.6777));
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.Set("Antennas", UintegerValue(spatialStreams));
    wifiPhy.Set("MaxSupportedTxSpatialStreams", UintegerValue(spatialStreams));
    wifiPhy.Set("MaxSupportedRxSpatialStreams", UintegerValue(spatialStreams));
    wifiPhy.Set("ChannelSettings", StringValue(channelString));

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
                         UintegerValue(256));

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
    internetStack.Install(clientNode);
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
    Ptr<TcpL4Protocol> tcpL4ProtocolClient = clientNode.Get(0)->GetObject<TcpL4Protocol>();
    Ptr<TcpL4Protocol> tcpL4ProtocolSta = staNodes.Get(0)->GetObject<TcpL4Protocol>();

    // Application configuration for Prague flows under test
    uint16_t port = 100;
    ApplicationContainer pragueClientApps;
    ApplicationContainer pragueServerApps;
    for (auto i = 0U; i < numPrague; i++)
    {
        BulkSendHelper bulk("ns3::TcpSocketFactory",
                            InetSocketAddress(wifiInterfaces.GetAddress(1), port + i));
        bulk.SetAttribute("MaxBytes", UintegerValue(numBytes));
        bulk.SetAttribute("StartTime", TimeValue(Seconds(1.0) + i * MilliSeconds(10)));
        pragueClientApps.Add(bulk.Install(clientNode.Get(0)));
        NS_LOG_DEBUG("Creating Prague foreground flow " << i);
        PacketSinkHelper sink =
            PacketSinkHelper("ns3::TcpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), port + i));
        sink.SetAttribute("StartTime", TimeValue(Seconds(1.0) + i * MilliSeconds(10)));
        pragueServerApps.Add(sink.Install(staNodes.Get(0)));
        g_flowsToClose++;
        Simulator::Schedule(
            Seconds(1.0) - TimeStep(1),
            MakeBoundCallback(&ConfigurePragueSockets, tcpL4ProtocolClient, tcpL4ProtocolSta));
    }

    // Application configuration for Cubic flows under test
    port = 200;
    ApplicationContainer cubicClientApps;
    ApplicationContainer cubicServerApps;
    for (auto i = 0U; i < numCubic; i++)
    {
        BulkSendHelper bulkCubic("ns3::TcpSocketFactory",
                                 InetSocketAddress(wifiInterfaces.GetAddress(1), port + i));
        bulkCubic.SetAttribute("MaxBytes", UintegerValue(numBytes));
        bulkCubic.SetAttribute("StartTime", TimeValue(Seconds(1.05) + i * MilliSeconds(10)));
        cubicClientApps.Add(bulkCubic.Install(clientNode.Get(0)));
        NS_LOG_DEBUG("Creating Cubic foreground flow " << i);
        PacketSinkHelper sinkCubic =
            PacketSinkHelper("ns3::TcpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), port + i));
        sinkCubic.SetAttribute("StartTime", TimeValue(Seconds(1.05) + i * MilliSeconds(10)));
        cubicServerApps.Add(sinkCubic.Install(staNodes.Get(0)));
        g_flowsToClose++;
        Simulator::Schedule(
            Seconds(1.05) - TimeStep(1),
            MakeBoundCallback(&ConfigureCubicSockets, tcpL4ProtocolClient, tcpL4ProtocolSta));
    }

    // Add a cubic application on the server for each background flow
    // Send the traffic from a different STA.
    port = 300;
    Simulator::Schedule(
        Seconds(1.1) - TimeStep(1),
        MakeBoundCallback(&ConfigureCubicSockets, tcpL4ProtocolClient, tcpL4ProtocolSta));
    for (auto i = 0U; i < numBackground; i++)
    {
        ApplicationContainer clientAppBackground;
        BulkSendHelper bulkBackground("ns3::TcpSocketFactory",
                                      InetSocketAddress(interfaces1.GetAddress(0), port + i));
        bulkBackground.SetAttribute("MaxBytes", UintegerValue(numBytes));
        clientAppBackground = bulkBackground.Install(staNodes.Get(1 + i));
        clientAppBackground.Start(Seconds(1.1));
        ApplicationContainer serverAppBackground;
        PacketSinkHelper sinkBackground =
            PacketSinkHelper("ns3::TcpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), port + i));
        serverAppBackground = sinkBackground.Install(clientNode.Get(0));
        serverAppBackground.Start(Seconds(1.1));
    }

    if (!numCubic && !numPrague && !numBackground)
    {
        uint16_t udpPort = 9;
        UdpServerHelper server(udpPort);
        ApplicationContainer serverApp = server.Install(staNodes.Get(0));
        serverApp.Start(Seconds(1.0));

        UdpClientHelper client(InetSocketAddress(wifiInterfaces.GetAddress(1), udpPort));
        client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
        client.SetAttribute("Interval", TimeValue(Time("0.00001"))); // packets/s
        client.SetAttribute("PacketSize", UintegerValue(1440));
        ApplicationContainer clientApp = client.Install(clientNode.Get(0));
        clientApp.Start(Seconds(1.1));
    }

    // Control the random variable stream assignments for Wi-Fi models (the value 100 is arbitrary)
    wifi.AssignStreams(wifiDevices, 100);

    // PCAP traces
    pointToPoint.EnablePcapAll("l4s-wifi");
    wifiPhy.EnablePcap("l4s-wifi", wifiDevices);
    internetStack.EnablePcapIpv4("l4s-wifi-2-0-ip.pcap",
                                 staNodes.Get(0)->GetObject<Ipv4>(),
                                 1,
                                 true);

    // Set up traces
    // Bytes and throughput in WifiMacQueue
    g_fileBytesInAcBeQueue.open("wifi-queue-bytes.dat", std::ofstream::out);
    Ptr<WifiMacQueue> apWifiMacQueue =
        apDevice.Get(0)->GetObject<WifiNetDevice>()->GetMac()->GetTxopQueue(AC_BE);
    NS_ASSERT_MSG(apWifiMacQueue, "Could not acquire pointer to AC_BE WifiMacQueue on the AP");
    apWifiMacQueue->TraceConnectWithoutContext("BytesInQueue",
                                               MakeCallback(&TraceBytesInAcBeQueue));

    Ptr<WifiPhy> apPhy = apDevice.Get(0)->GetObject<WifiNetDevice>()->GetPhy();
    NS_ASSERT_MSG(apPhy, "Could not acquire pointer to AP's WifiPhy");
    g_fileWifiPhyTxPsduBegin.open("wifi-phy-tx-psdu-begin.dat", std::ofstream::out);
    apPhy->TraceConnectWithoutContext("PhyTxPsduBegin", MakeCallback(&TraceWifiPhyTxPsduBegin));
    g_fileWifiThroughput.open("wifi-throughput.dat", std::ofstream::out);
    Simulator::Schedule(g_wifiThroughputInterval, &TraceWifiThroughput);

    // Throughput and latency for foreground flows, and set up close callbacks
    if (pragueClientApps.GetN())
    {
        g_filePragueThroughput.open("prague-throughput.dat", std::ofstream::out);
        g_filePragueCwnd.open("prague-cwnd.dat", std::ofstream::out);
        g_filePragueSsthresh.open("prague-ssthresh.dat", std::ofstream::out);
        g_filePragueSendInterval.open("prague-send-interval.dat", std::ofstream::out);
        g_filePraguePacingRate.open("prague-pacing-rate.dat", std::ofstream::out);
        g_filePragueCongState.open("prague-cong-state.dat", std::ofstream::out);
        g_filePragueEcnState.open("prague-ecn-state.dat", std::ofstream::out);
        g_filePragueRtt.open("prague-rtt.dat", std::ofstream::out);
    }
    for (auto i = 0U; i < pragueClientApps.GetN(); i++)
    {
        // The TCP sockets that we want to connect
        Simulator::Schedule(
            Seconds(1.0) + i * MilliSeconds(10) + TimeStep(1),
            MakeBoundCallback(&TracePragueClientSocket, pragueClientApps.Get(i), i));
        if (!i)
        {
            Simulator::Schedule(
                Seconds(1.0) + MilliSeconds(100),
                MakeBoundCallback(&TracePragueServerSocket, pragueServerApps.Get(0)));
        }
        std::ostringstream oss;
        oss << "Prague:" << i;
        NS_LOG_DEBUG("Setting up callbacks on Prague sockets " << pragueServerApps.Get(i));
        pragueServerApps.Get(i)->GetObject<PacketSink>()->TraceConnect(
            "PeerClose",
            oss.str(),
            MakeCallback(&HandlePeerClose));
        pragueServerApps.Get(i)->GetObject<PacketSink>()->TraceConnect(
            "PeerError",
            oss.str(),
            MakeCallback(&HandlePeerError));
    }

    if (cubicClientApps.GetN())
    {
        g_fileCubicThroughput.open("cubic-throughput.dat", std::ofstream::out);
        g_fileCubicCwnd.open("cubic-cwnd.dat", std::ofstream::out);
        g_fileCubicSsthresh.open("cubic-ssthresh.dat", std::ofstream::out);
        g_fileCubicSendInterval.open("cubic-send-interval.dat", std::ofstream::out);
        g_fileCubicPacingRate.open("cubic-pacing-rate.dat", std::ofstream::out);
        g_fileCubicCongState.open("cubic-cong-state.dat", std::ofstream::out);
        g_fileCubicRtt.open("cubic-rtt.dat", std::ofstream::out);
    }
    for (auto i = 0U; i < cubicClientApps.GetN(); i++)
    {
        // The TCP sockets that we want to connect
        Simulator::Schedule(Seconds(1.05) + i * MilliSeconds(10) + TimeStep(1),
                            MakeBoundCallback(&TraceCubicClientSocket, cubicClientApps.Get(i), i));
        if (!i)
        {
            Simulator::Schedule(Seconds(1.0) + MilliSeconds(100),
                                MakeBoundCallback(&TraceCubicServerSocket, cubicServerApps.Get(0)));
        }
        std::ostringstream oss;
        oss << "Cubic:" << i;
        NS_LOG_DEBUG("Setting up callbacks on Cubic sockets " << i << " "
                                                              << cubicServerApps.Get(i));
        cubicServerApps.Get(i)->GetObject<PacketSink>()->TraceConnect(
            "PeerClose",
            oss.str(),
            MakeCallback(&HandlePeerClose));
        cubicServerApps.Get(i)->GetObject<PacketSink>()->TraceConnect(
            "PeerError",
            oss.str(),
            MakeCallback(&HandlePeerError));
    }

    // Trace bytes in DualPi2 queue
    Ptr<DualPi2QueueDisc> dualPi2 = apQueueDiscContainer.Get(0)
                                        ->GetQueueDiscClass(0)
                                        ->GetQueueDisc()
                                        ->GetObject<DualPi2QueueDisc>();
    NS_ASSERT_MSG(dualPi2, "Could not acquire pointer to DualPi2 queue");
    g_fileBytesInDualPi2Queue.open("wifi-dualpi2-bytes.dat", std::ofstream::out);
    dualPi2->TraceConnectWithoutContext("BytesInQueue", MakeCallback(&TraceBytesInDualPi2Queue));
    g_fileLSojourn.open("wifi-dualpi2-l-sojourn.dat", std::ofstream::out);
    dualPi2->TraceConnectWithoutContext("L4sSojournTime", MakeCallback(&TraceLSojourn));
    g_fileCSojourn.open("wifi-dualpi2-c-sojourn.dat", std::ofstream::out);
    dualPi2->TraceConnectWithoutContext("ClassicSojournTime", MakeCallback(&TraceCSojourn));

    // Hook DualPi2 queue to WifiMacQueue::PendingDequeue trace source
    bool connected = apWifiMacQueue->TraceConnectWithoutContext(
        "PendingDequeue",
        MakeCallback(&DualPi2QueueDisc::PendingDequeueCallback, dualPi2));
    NS_ASSERT_MSG(connected, "Could not hook DualPi2 queue to AP WifiMacQueue trace source");

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

    g_fileBytesInAcBeQueue.close();
    g_fileBytesInDualPi2Queue.close();
    g_fileLSojourn.close();
    g_fileCSojourn.close();
    g_fileWifiPhyTxPsduBegin.close();
    g_fileWifiThroughput.close();
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
    g_fileCubicCongState.close();
    g_fileCubicRtt.close();
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
TraceWifiPhyTxPsduBegin(WifiConstPsduMap psduMap, WifiTxVector txVector, double txPower)
{
    NS_ASSERT(psduMap.size() == 1);
    bool isQosData = false;
    const auto& it [[maybe_unused]] = psduMap.begin();
    auto nMpdus = it->second->GetNMpdus();
    uint32_t totalAmpduSize = 0;
    for (std::size_t i = 0; i < nMpdus; i++)
    {
        if (it->second->GetHeader(i).IsQosData())
        {
            isQosData = true;
            totalAmpduSize += it->second->GetAmpduSubframeSize(i);
            g_wifiThroughputData += it->second->GetAmpduSubframeSize(i);
        }
    }
    if (isQosData)
    {
        g_fileWifiPhyTxPsduBegin << Now().GetSeconds() << " "
                                 << (Now() - g_lastQosDataTime).GetMicroSeconds() << " "
                                 << WifiPhy::CalculateTxDuration(psduMap,
                                                                 txVector,
                                                                 WifiPhyBand::WIFI_PHY_BAND_5GHZ)
                                        .GetMicroSeconds()
                                 << " " << nMpdus << " " << totalAmpduSize << std::endl;
        g_lastQosDataTime = Now();
    }
}

void
TraceLSojourn(Time sojourn)
{
    g_fileLSojourn << Now().GetSeconds() << " " << sojourn.GetMicroSeconds() / 1000.0 << std::endl;
}

void
TraceCSojourn(Time sojourn)
{
    g_fileCSojourn << Now().GetSeconds() << " " << sojourn.GetMicroSeconds() / 1000.0 << std::endl;
}

void
TracePragueTx(Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket)
{
    if (g_lastSeenPrague > Seconds(0))
    {
        g_filePragueSendInterval << std::fixed << std::setprecision(6) << Now().GetSeconds() << " "
                                 << (Now() - g_lastSeenPrague).GetSeconds() << std::endl;
    }
    g_lastSeenPrague = Now();
}

void
TracePragueRx(Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket)
{
    g_pragueData += packet->GetSize();
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
TracePragueClientSocket(Ptr<Application> a, uint32_t i)
{
    Ptr<BulkSendApplication> bulk = DynamicCast<BulkSendApplication>(a);
    NS_ASSERT_MSG(bulk, "Application downcast failed");
    Ptr<Socket> s = bulk->GetSocket();
    NS_ASSERT_MSG(s, "Socket empty");
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

void
TracePragueServerSocket(Ptr<Application> a)
{
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(a);
    NS_ASSERT_MSG(sink, "Application downcast failed");
    Ptr<Socket> s = sink->GetAcceptedSockets().front();
    NS_ASSERT_MSG(s, "Socket empty");
    Ptr<TcpSocketBase> tcp = DynamicCast<TcpSocketBase>(s);
    NS_ASSERT_MSG(tcp, "TCP socket downcast failed");
    tcp->TraceConnectWithoutContext("Rx", MakeCallback(&TracePragueRx));
}

void
TraceCubicTx(Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket)
{
    if (g_lastSeenCubic > Seconds(0))
    {
        g_fileCubicSendInterval << std::fixed << std::setprecision(6) << Now().GetSeconds() << " "
                                << (Now() - g_lastSeenCubic).GetSeconds() << std::endl;
    }
    g_lastSeenCubic = Now();
}

void
TraceCubicRx(Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket)
{
    g_cubicData += packet->GetSize();
}

void
TraceCubicThroughput()
{
    g_fileCubicThroughput << Now().GetSeconds() << " " << std::fixed
                          << (g_cubicData * 8) / g_cubicThroughputInterval.GetSeconds() / 1e6
                          << std::endl;
    cubicThroughputCalculator.Update((g_cubicData * 8) / g_cubicThroughputInterval.GetSeconds() /
                                     1e6);
    Simulator::Schedule(g_cubicThroughputInterval, &TraceCubicThroughput);
    g_cubicData = 0;
}

void
TraceCubicCwnd(uint32_t oldVal, uint32_t newVal)
{
    g_fileCubicCwnd << Now().GetSeconds() << " " << newVal << std::endl;
}

void
TraceCubicSsthresh(uint32_t oldVal, uint32_t newVal)
{
    g_fileCubicSsthresh << Now().GetSeconds() << " " << newVal << std::endl;
}

void
TraceCubicPacingRate(DataRate oldVal, DataRate newVal)
{
    g_fileCubicPacingRate << Now().GetSeconds() << " " << newVal.GetBitRate() << std::endl;
}

void
TraceCubicCongState(TcpSocketState::TcpCongState_t oldVal, TcpSocketState::TcpCongState_t newVal)
{
    g_fileCubicCongState << Now().GetSeconds() << " " << TcpSocketState::TcpCongStateName[newVal]
                         << std::endl;
}

void
TraceCubicRtt(Time oldVal, Time newVal)
{
    g_fileCubicRtt << Now().GetSeconds() << " " << newVal.GetMicroSeconds() / 1000.0 << std::endl;
}

void
TraceCubicClientSocket(Ptr<Application> a, uint32_t i)
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

void
TraceCubicServerSocket(Ptr<Application> a)
{
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(a);
    NS_ASSERT_MSG(sink, "Application downcast failed");
    Ptr<Socket> s = sink->GetAcceptedSockets().front();
    NS_ASSERT_MSG(s, "Socket empty");
    Ptr<TcpSocketBase> tcp = DynamicCast<TcpSocketBase>(s);
    NS_ASSERT_MSG(tcp, "TCP socket downcast failed");
    tcp->TraceConnectWithoutContext("Rx", MakeCallback(&TraceCubicRx));
}

void
TraceWifiThroughput()
{
    g_fileWifiThroughput << Now().GetSeconds() << " " << std::fixed
                         << (g_wifiThroughputData * 8) / g_wifiThroughputInterval.GetSeconds() / 1e6
                         << std::endl;
    Simulator::Schedule(g_wifiThroughputInterval, &TraceWifiThroughput);
    g_wifiThroughputData = 0;
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
