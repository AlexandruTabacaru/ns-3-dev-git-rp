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

// Nodes 0                     Node 1                           Node 2
//
// client ---------------------> AP -------------------------- > STA (server)
//       10 Gbps
//       configurable delay            channel widths 20/80/160 MHz
//                                     Configurable MCS
//                                     Configurable num. spatial streams
//
//                       ----  ----  ----  ---- > TCP data transfer direction
//                       < -  -  -  -  -  -   -   ACK direction
//
// 0..N flows from client to server can be configured, for both Prague
// and Cubic.  A special case is if zero Prague and Cubic flows are
// configured, for which UDP saturating traffic will be sent-- this can be
// used to test for maximum possible throughput of a configuration.
// Data transfer is from client to server (iperf naming convention).
//
// In addition (not depicted in the diagram), a configurable number of additional
// STAs are added to the same Wi-Fi channel, communicating with another AP
// (also on the same channel, but configured with a different SSID).  These STAs,
// if configured, will send saturating UDP traffic on the channel, to create
// contention on the channel.  The other STAs are configured on a different
// BSS and are served by a different AP.
//
// Configuration inputs:
// - number of Cubic flows under test
// - number of Prague flows under test
// - number of bytes for TCP flows, or zero bytes for unlimited
// - duration of TCP application flows
// - number of UDP senders
// - whether to disable flow control
// - Wi-Fi aggregation queue limit when flow control is enabled (scale factor)
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
//    (i.e., if multiple Prague or Cubic are configured, only the first such
//    flow is traced in detail)

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

#define MTU_SIZE 1500                                           //bytes
#define MAC_HEADER_SIZE 44                                      //bytes
#define GUARD_INTERVAL 800                                      //nanoseconds
const Time PREAMBLE_AND_HEADER_DURATION = MicroSeconds(52);     //microseconds
const Time PROTECTION_TIME = MicroSeconds(88);                  //microseconds
const Time ACK_TIME = MicroSeconds(56);                         //microseconds

NS_LOG_COMPONENT_DEFINE("L4sWifi");

// Declare trace functions that are defined later in this file
std::ofstream g_fileBytesInAcBeQueue;
void TraceBytesInAcBeQueue(uint32_t oldVal, uint32_t newVal);
std::ofstream g_fileBytesInDualPi2Queue;
void TraceBytesInDualPi2Queue(uint32_t oldVal, uint32_t newVal);

uint32_t g_wifiThroughputData = 0;
uint32_t g_wifiFgThroughputData = 0;
uint32_t g_wifiBgThroughputData = 0;
Time g_lastQosDataTime;
std::ofstream g_fileWifiPhyTxPsduBegin;
void TraceWifiPhyTxPsduBegin(std::string context,
                             WifiConstPsduMap psduMap,
                             WifiTxVector txVector,
                             double txPower);

std::ofstream g_fileWifiThroughput;
std::ofstream g_fileWifiFgThroughput;
std::ofstream g_fileWifiBgThroughput;
void TraceWifiThroughput();
Time g_wifiThroughputInterval = MilliSeconds(100);

std::ofstream g_fileLSojourn;
void TraceLSojourn(Time sojourn);
std::ofstream g_fileCSojourn;
void TraceCSojourn(Time sojourn);

std::ofstream g_fileTraceProbChanges;
void TraceProbcL(double oldVal, double pCL);
void TraceProbL(double oldVal, double pL);
void TraceProbC(double oldVal, double pC);

uint32_t g_pragueData = 0;
std::map<uint16_t, uint32_t> g_pragueDataforEachPort;

Time g_lastSeenPrague = Seconds(0);
std::ofstream g_filePragueThroughput;
std::ofstream g_filePragueThroughputPerStream;
std::ofstream g_filePragueCwnd;
std::ofstream g_filePragueSsthresh;
std::ofstream g_filePragueSendInterval;
std::ofstream g_filePraguePacingRate;
std::ofstream g_filePragueCongState;
std::ofstream g_filePragueEcnState;
std::ofstream g_filePragueRtt;
Time g_pragueThroughputInterval = MilliSeconds(100);
void TracePragueThroughput();
void CalculatePragueThroughput();
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
void TracePragueClientSocket(Ptr<Application>, uint32_t, bool, bool);
void TracePragueServerSocket(Ptr<Socket>);

uint32_t g_cubicData = 0;
std::map<uint16_t, uint32_t> g_cubicDataforEachPort; // Dst Port Number , Rx packet size

Time g_lastSeenCubic = Seconds(0);
std::ofstream g_fileCubicThroughput;
std::ofstream g_fileCubicThroughputPerStream;
std::ofstream g_fileCubicCwnd;
std::ofstream g_fileCubicSsthresh;
std::ofstream g_fileCubicSendInterval;
std::ofstream g_fileCubicPacingRate;
std::ofstream g_fileCubicCongState;
std::ofstream g_fileCubicRtt;
Time g_cubicThroughputInterval = MilliSeconds(100);
void TraceCubicThroughput();
void CalculateCubicThroughput();
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
void TraceCubicClientSocket(Ptr<Application>, uint32_t, bool, bool);
void TraceCubicServerSocket(Ptr<Socket>);

// Count the number of flows to wait for completion before stopping the simulation
uint32_t g_flowsToClose = 0;
// Hook these methods to the PacketSink objects
void HandlePeerClose(std::string context, Ptr<const Socket> socket);
void HandlePeerError(std::string context, Ptr<const Socket> socket);

// For use in dynamically changing MCS and aggregation buffer limit
Ptr<ConstantRateWifiManager> apWifiMgr;
Ptr<WifiNetDevice> apWifiNetDevice;
Ptr<WifiNetDevice> staWifiNetDevice;
QueueDiscContainer apQueueDiscContainer;
QueueDiscContainer staQueueDiscContainer;
uint32_t CalculateLimit(uint32_t mcs,
                        uint32_t channelWidth,
                        uint32_t spatialStreams,
                        Time txopLimit);
void ChangeMcs(uint16_t mcs,
               uint32_t limit,
               uint16_t nextMcs,
               uint32_t nextLimit,
               double scale,
               Time mcsChangeInterval);

// Helper function for EDCA overrides
void SetAcBeEdcaParameters(const NetDeviceContainer& ndc,
                           uint32_t cwMin,
                           uint32_t cwMax,
                           uint8_t aifsn,
                           Time txopLimit);

// These methods work around the lack of ability to configure different TCP socket types
// on the same node on a per-socket (per-application) basis. Instead, these methods can
// be scheduled (right before a socket creation) to change the default value
void ConfigurePragueSockets(Ptr<TcpL4Protocol> tcp1, Ptr<TcpL4Protocol> tcp2);
void ConfigureCubicSockets(Ptr<TcpL4Protocol> tcp1, Ptr<TcpL4Protocol> tcp2);

// Declare some statistics counters here so that they are updated in traces
MinMaxAvgTotalCalculator<uint32_t> pragueThroughputCalculator; // units of Mbps
MinMaxAvgTotalCalculator<uint32_t> cubicThroughputCalculator;  // units of Mbps

// Utility function to connect DualPi2QueueDisc to WifiMacQueue callback
void ConnectPendingDequeueCallback(const NetDeviceContainer& devices,
                                   const QueueDiscContainer& queueDiscs);

// Utility function to inform DualPi2 queues about a change to the limit
void UpdateDualPi2AggBufferLimit(const QueueDiscContainer& queueDiscs,
                                 double scale,
                                 uint32_t limit);
// Utility function to inform dynamic queue limits about a change to the limit
void UpdateDynamicQueueLimits(Ptr<WifiNetDevice> device, double scale, uint32_t limit);

// Utility function to populate ARP cache after simulation start
// Because of an interaction with LinkUp events (ns-3 issue #851)
void AddManualArpEntries(Ptr<Channel> channel);

int
main(int argc, char* argv[])
{
    // Variable declaration, and constants
    std::string wifiControlMode = "OfdmRate24Mbps";
    double staDistance = 1; // meters; distance of 10 m or more will cause packet loss at MCS 11
    const double pi = 3.1415927;
    DataRate obssUdpRate{"2400Mbps"}; // MCS 6 maximum PHY rate with 160 MHz channel and 4 SS
    DataRate perStaUdpRate;           // Will be updated below
    Time obssUdpStartTime{Seconds(1)};

    // Variables that can be changed by command-line argument
    uint32_t numCubic = 1;
    uint32_t numPrague = 1;
    uint32_t numBytes = 0;             // default 0 = unlimited transfer for RQ2
    Time duration = Seconds(0);           // By default, close one second after last TCP flow closes
    Time wanLinkDelay = MilliSeconds(10); // base RTT is 20ms
    bool useReno = false;
    uint32_t numBackgroundUdp = 0;
    uint16_t mcs = 2;
    uint16_t secondMcs = 2;
    Time mcsChangeInterval; // By default, zero means disabled
    uint32_t channelWidth = 20;
    uint32_t spatialStreams = 1;
    // Default WifiMacQueue depth is roughly 40 ms at 2.4 Gbps ~= 8000 packets
    // 2.4 Gbps is the maximum PHY rate for 160 MHz channels, 2 SS, MCS 11
    std::string wifiQueueSize = "5000p";
    bool flowControl = true;
    uint32_t limit = 65535;       // default flow control limit (max A-MPDU size in bytes)
    double scale = 1.0;           // default flow control scale factor
    uint32_t rtsCtsThreshold = 0; // RTS/CTS disabled by default
    Time processingDelay = MicroSeconds(10);
    Time flowStartOffset = MicroSeconds(1); // Time between starting each TCP flow
    bool showProgress = false;
    uint32_t maxAmsduSize = 0; // zero means A-MSDU is disabled
    Time progressInterval = Seconds(5);
    Time arpCacheInstallTime = Seconds(0.5); // manually populate ARP cache at this time
    bool enablePcapAll = false;
    bool enablePcap = true;
    bool enableTracesAll = false;
    bool enableTraces = true;
    // Default AC_BE EDCA configuration
    uint32_t cwMin = 15;
    uint32_t cwMax = 1023;
    uint8_t aifsn = 3;
    Time txopLimit = MicroSeconds(2528);
    bool enableLogs = false;
    uint16_t rngRun = 1;

    // Increase some defaults (command-line can override below)
    // ns-3 TCP does not automatically adjust MSS from the device MTU
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    // ns-3 TCP socket buffer sizes do not dynamically grow, so set to ~1.5 * BDP product
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(750000));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(750000));
    // Enable pacing for Cubic
    Config::SetDefault("ns3::TcpSocketState::EnablePacing", BooleanValue(true));
    // Config::SetDefault("ns3::TcpSocketState::PaceInitialWindow", BooleanValue(true));
    // Enable a timestamp (for latency sampling) in the bulk send application
    Config::SetDefault("ns3::BulkSendApplication::EnableSeqTsSizeHeader", BooleanValue(true));
    Config::SetDefault("ns3::PacketSink::EnableSeqTsSizeHeader", BooleanValue(true));
    // The bulk send application should do 1448-byte writes (one timestamp per TCP packet)
    Config::SetDefault("ns3::BulkSendApplication::SendSize", UintegerValue(1448));
    // Bypass Laqm when using Wi-Fi
    Config::SetDefault("ns3::DualPi2QueueDisc::DisableLaqm", BooleanValue(true));
    // Set Classic AQM target to 30ms
    Config::SetDefault("ns3::DualPi2QueueDisc::Target", TimeValue(MilliSeconds(30)));
    // Set AC_BE max AMPDU to maximum 802.11ax value
    Config::SetDefault("ns3::WifiMac::BE_MaxAmpduSize", UintegerValue(6500631));

    CommandLine cmd;
    cmd.Usage("The l4s-wifi program experiments with TCP flows over L4S Wi-Fi configuration");
    cmd.AddValue("numCubic", "Number of foreground Cubic flows", numCubic);
    cmd.AddValue("numPrague", "Number of foreground Prague flows", numPrague);
    cmd.AddValue("numBytes", "Number of bytes for each TCP transfer", numBytes);
    cmd.AddValue("duration", "(optional) scheduled end of simulation", duration);
    cmd.AddValue("wanLinkDelay", "one-way base delay from server to AP", wanLinkDelay);
    cmd.AddValue("useReno", "Use Linux Reno instead of Cubic", useReno);
    cmd.AddValue("numBackgroundUdp", "Number of background UDP flows", numBackgroundUdp);
    cmd.AddValue("mcs", "Index (0-11) of 11ax HE MCS", mcs);
    cmd.AddValue("secondMcs", "Index (0-11) of 11ax HE MCS", secondMcs);
    cmd.AddValue("mcsChangeInterval",
                 "if set, will toggle between mcs and secondMcs at this interval",
                 mcsChangeInterval);
    cmd.AddValue("channelWidth", "Width (MHz) of channel", channelWidth);
    cmd.AddValue("spatialStreams", "Number of spatial streams", spatialStreams);
    cmd.AddValue("wifiQueueSize", "WifiMacQueue size", wifiQueueSize);
    cmd.AddValue("flowControl", "Whether to enable flow control (set also the limit)", flowControl);
    cmd.AddValue("limit", "Queue limit (bytes)", limit);
    cmd.AddValue("scale", "Scaling factor for queue limit", scale);
    cmd.AddValue("rtsCtsThreshold", "RTS/CTS threshold (bytes)", rtsCtsThreshold);
    cmd.AddValue("processingDelay", "Notional packet processing delay", processingDelay);
    cmd.AddValue("flowStartOffset",
                 "Time between successive TCP flow start times",
                 flowStartOffset);
    cmd.AddValue("maxAmsduSize", "BE Max A-MSDU size in bytes", maxAmsduSize);
    cmd.AddValue("cwMin", "BE CWmin in slots", cwMin);
    cmd.AddValue("cwMax", "BE CWmax in slots", cwMax);
    cmd.AddValue("aifsn", "BE AIFSN in slots", aifsn);
    cmd.AddValue("txopLimit", "BE TXOP Limit", txopLimit);
    cmd.AddValue("showProgress", "Show simulation progress every 5s", showProgress);
    cmd.AddValue("enablePcapAll",
                 "Whether to enable PCAP trace output at all interfaces",
                 enablePcapAll);
    cmd.AddValue("enablePcap", "Whether to enable PCAP trace output only at endpoints", enablePcap);
    cmd.AddValue("enableTracesAll",
                 "Whether to enable full time-series trace output",
                 enableTracesAll);
    cmd.AddValue("enableTraces",
                 "Whether to enable time-series traces necessary for plot-l4s-wifi.py",
                 enableTraces);
    cmd.AddValue("enableLogs", "Whether to enable logs of DualPi2QueueDisc class", enableLogs);
    cmd.AddValue("rngRun", "Random Number Generator run number", rngRun);
    cmd.Parse(argc, argv);

    if (limit < 65535)
    {
        std::cout << "Warning:  'limit' command-line argument is ignored; limit is now autoset"
                  << std::endl;
    }

    if (enableLogs)
    {
        //LogComponentEnableAll(LOG_PREFIX_ALL);
        //LogComponentEnable("DualPi2QueueDisc", LOG_LEVEL_INFO);
        LogComponentEnable("L4sWifi", LOG_LEVEL_DEBUG);

    }

    NS_ABORT_MSG_UNLESS(mcs < 12, "Only MCS 0-11 supported");
    NS_ABORT_MSG_UNLESS(secondMcs < 12, "Only MCS 0-11 supported");

    limit = CalculateLimit(mcs, channelWidth, spatialStreams, txopLimit);

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
    // Set AC_BE max AMSDU to four packets
    Config::SetDefault("ns3::WifiMac::BE_MaxAmsduSize", UintegerValue(maxAmsduSize));

    // Set DualPi2 buffer size (both L & C)
    Config::SetDefault("ns3::DualPi2QueueDisc::QueueLimit",
                       UintegerValue(static_cast<uint32_t>(scale * limit * 100)));

    if (useReno)
    {
        std::cout << "Using ns-3 LinuxReno model instead of Cubic" << std::endl;
        Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                           TypeIdValue(TcpLinuxReno::GetTypeId()));
    }

    RngSeedManager::SetRun(rngRun);

    // Workaround until PRR response is debugged
    Config::SetDefault("ns3::TcpL4Protocol::RecoveryType",
                       TypeIdValue(TcpClassicRecovery::GetTypeId()));

    // Create the nodes and use containers for further configuration below
    NodeContainer clientNode;
    clientNode.Create(1);
    NodeContainer apNode;
    apNode.Create(1);
    Names::Add("AP", apNode.Get(0));
    NodeContainer staNode;
    staNode.Create(1);
    Names::Add("STA", staNode.Get(0));
    // NodeContainers for nodes outside of the BSS under test (OBSS)
    NodeContainer obssClientNode;
    obssClientNode.Create(1);
    NodeContainer obssApNode;
    obssApNode.Create(1);
    Names::Add("OBSS-AP", obssApNode.Get(0));
    NodeContainer obssStaNodes;
    obssStaNodes.Create(numBackgroundUdp);
    for (uint32_t i = 0; i < numBackgroundUdp; i++)
    {
        Names::Add("OBSS-STA" + std::to_string(i), obssStaNodes.Get(i));
    }

    // Create point-to-point links between server and AP
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    pointToPoint.SetChannelAttribute("Delay", TimeValue(wanLinkDelay));
    NetDeviceContainer wanDevices = pointToPoint.Install(clientNode.Get(0), apNode.Get(0));
    NetDeviceContainer obssWanDevices =
        pointToPoint.Install(obssClientNode.Get(0), obssApNode.Get(0));

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
    auto wifiChannelPtr = wifiChannel.Create();
    wifiPhy.SetChannel(wifiChannelPtr);
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
    SetAcBeEdcaParameters(apDevice, cwMin, cwMax, aifsn, txopLimit);
    apWifiMgr = apDevice.Get(0)
                    ->GetObject<WifiNetDevice>()
                    ->GetRemoteStationManager()
                    ->GetObject<ConstantRateWifiManager>();
    NS_ABORT_MSG_UNLESS(apWifiMgr, "Downcast failed");
    apWifiNetDevice = apDevice.Get(0)->GetObject<WifiNetDevice>();

    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("l4s")));
    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNode);
    SetAcBeEdcaParameters(staDevices, cwMin, cwMax, aifsn, txopLimit);
    staWifiNetDevice = staDevices.Get(0)->GetObject<WifiNetDevice>();

    // OBSS configuration
    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("obss")));
    NetDeviceContainer obssApDevice = wifi.Install(wifiPhy, wifiMac, obssApNode);
    SetAcBeEdcaParameters(obssApDevice, cwMin, cwMax, aifsn, txopLimit);

    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("obss")));
    NetDeviceContainer obssStaDevices = wifi.Install(wifiPhy, wifiMac, obssStaNodes);
    SetAcBeEdcaParameters(obssStaDevices, cwMin, cwMax, aifsn, txopLimit);

    // Set positions
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    // Set position for AP
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // X,Y,Z cartesian

    // Set position for STAs; simple routine to distribute around a ring of distance 'staDistance'
    uint32_t numSta = staNode.GetN() + obssStaNodes.GetN();
    double angle = (static_cast<double>(360) / numSta);
    for (uint32_t i = 0; i < numSta; ++i)
    {
        positionAlloc->Add(Vector(staDistance * cos((i * angle * pi) / 180),
                                  staDistance * sin((i * angle * pi) / 180),
                                  0.0));
    }

    // Create some additional container objects to simplify the below configuration
    NodeContainer wifiNodes;
    wifiNodes.Add(apNode);
    wifiNodes.Add(staNode);

    // Add Mobility (position objects) to the Wi-Fi nodes, for propagation
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(wifiNodes);
    mobility.Install(obssApNode);
    mobility.Install(obssStaNodes);

    // Internet stack installation
    InternetStackHelper internetStack;
    internetStack.Install(clientNode);
    internetStack.Install(obssClientNode);
    internetStack.Install(wifiNodes);
    internetStack.Install(obssApNode);
    internetStack.Install(obssStaNodes);

    // Schedule an event to manually set ARP cache entries so that
    // no neighbor discovery is needed
    Simulator::Schedule(arpCacheInstallTime,
                        MakeBoundCallback(&AddManualArpEntries, wifiChannelPtr));

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
    apQueueDiscContainer = tch.Install(apDevice);
    staQueueDiscContainer = tch.Install(staDevices);

    // Hook DualPi2 queue to WifiMacQueue::PendingDequeue trace source
    ConnectPendingDequeueCallback(apDevice, apQueueDiscContainer);
    ConnectPendingDequeueCallback(staDevices, staQueueDiscContainer);

    // Inform dualPi2 of aggregation buffer (scaled) limit
    // Call this function any time that 'scale' or 'limit' changes, on
    // all of the DualPi2QueueDisc in the simulation
    // Note: if there are runtime changes to these values, the
    // DynamicQueueLimits objects need to also be updated (not handled here)
    UpdateDualPi2AggBufferLimit(apQueueDiscContainer, scale, limit);
    UpdateDualPi2AggBufferLimit(staQueueDiscContainer, scale, limit);

    // Configure IP addresses for all links
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(wanDevices);
    NetDeviceContainer wifiDevices;
    wifiDevices.Add(apDevice);
    wifiDevices.Add(staDevices);
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces = address.Assign(wifiDevices);
    // OBSS network
    address.SetBase("172.16.1.0", "255.255.255.0");
    NetDeviceContainer obssWifiDevices;
    obssWifiDevices.Add(obssApDevice);
    obssWifiDevices.Add(obssStaDevices);
    Ipv4InterfaceContainer obssWifiInterfaces = address.Assign(obssWifiDevices);
    address.SetBase("20.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign(obssWanDevices);

    // The staNode and clientNode both need a static IPv4 route added; the AP node does not
    // The obssStaNodes and obssClientNode both need static IPv4 routes added
    Ptr<Ipv4StaticRouting> staticRouting;
    staticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(
        clientNode.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    staticRouting->SetDefaultRoute("10.1.1.2", 1); // next hop, outgoing interface index
    staticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(
        staNode.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    staticRouting->SetDefaultRoute("192.168.1.1", 1); // next hop, outgoing interface index
    staticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(
        obssClientNode.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    staticRouting->SetDefaultRoute("20.1.1.2", 1); // next hop, outgoing interface index
    for (uint32_t i = 0; i < numBackgroundUdp; i++)
    {
        staticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(
            obssStaNodes.Get(i)->GetObject<Ipv4>()->GetRoutingProtocol());
        staticRouting->SetDefaultRoute("172.16.1.1", 1); // next hop, outgoing interface index
    }

    // Application traffic configuration

    // Get pointers to the TcpL4Protocol instances of the primary nodes
    Ptr<TcpL4Protocol> tcpL4ProtocolClient = clientNode.Get(0)->GetObject<TcpL4Protocol>();
    Ptr<TcpL4Protocol> tcpL4ProtocolSta = staNode.Get(0)->GetObject<TcpL4Protocol>();

    // Application configuration for Prague flows under test
    uint16_t port = 100;
    ApplicationContainer pragueClientApps;
    ApplicationContainer pragueServerApps;
    for (auto i = 0U; i < numPrague; i++)
    {
        BulkSendHelper bulk("ns3::TcpSocketFactory",
                            InetSocketAddress(wifiInterfaces.GetAddress(1), port + i));
        bulk.SetAttribute("MaxBytes", UintegerValue(numBytes));
        bulk.SetAttribute("StartTime", TimeValue(Seconds(1) + i * flowStartOffset));
        pragueClientApps.Add(bulk.Install(clientNode.Get(0)));
        NS_LOG_DEBUG("Creating Prague foreground flow " << i);
        PacketSinkHelper sink =
            PacketSinkHelper("ns3::TcpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), port + i));
        sink.SetAttribute("StartTime", TimeValue(Seconds(1) + i * flowStartOffset));
        pragueServerApps.Add(sink.Install(staNode.Get(0)));
        g_flowsToClose++;
        Simulator::Schedule(
            Seconds(1) - TimeStep(1),
            MakeBoundCallback(&ConfigurePragueSockets, tcpL4ProtocolClient, tcpL4ProtocolSta));
    }
    // The TCP socket factory needs to be reconfigured to create Cubic
    // sockets after Prague sockets were generated.
    Time factoryReconfigurationTime = Seconds(1) + (numPrague * flowStartOffset) - TimeStep(1);
    Simulator::Schedule(
        factoryReconfigurationTime,
        MakeBoundCallback(&ConfigureCubicSockets, tcpL4ProtocolClient, tcpL4ProtocolSta));

    // Application configuration for Cubic flows under test
    port = 200;
    ApplicationContainer cubicClientApps;
    ApplicationContainer cubicServerApps;
    for (auto i = 0U; i < numCubic; i++)
    {
        BulkSendHelper bulkCubic("ns3::TcpSocketFactory",
                                 InetSocketAddress(wifiInterfaces.GetAddress(1), port + i));
        bulkCubic.SetAttribute("MaxBytes", UintegerValue(numBytes));
        bulkCubic.SetAttribute("StartTime",
                               TimeValue(Seconds(1) + (i + numPrague) * flowStartOffset));
        cubicClientApps.Add(bulkCubic.Install(clientNode.Get(0)));
        NS_LOG_DEBUG("Creating Cubic foreground flow " << i);
        PacketSinkHelper sinkCubic =
            PacketSinkHelper("ns3::TcpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), port + i));
        sinkCubic.SetAttribute("StartTime",
                               TimeValue(Seconds(1) + (i + numPrague) * flowStartOffset));
        cubicServerApps.Add(sinkCubic.Install(staNode.Get(0)));
        g_flowsToClose++;
    }
    // Allow the primary network to send saturating UDP traffic if no TCP is configured
    if (!numCubic && !numPrague)
    {
        uint16_t udpPort = 9;
        UdpServerHelper server(udpPort);
        ApplicationContainer serverApp = server.Install(staNode.Get(0));
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

    // schedule MCS changes, if configured above
    // RQ2: Only change MCS once at t=10s
    if (mcs != secondMcs) {
        uint32_t secondLimit = CalculateLimit(secondMcs, channelWidth, spatialStreams, txopLimit);
        Simulator::Schedule(Seconds(10),
            &ChangeMcs,
            mcs,
            limit,
            secondMcs,
            secondLimit,
            scale,
            Seconds(10000)); // Large interval so it never flips back
    }

    // OBSS traffic configuration

    if (numBackgroundUdp)
    {
        perStaUdpRate = obssUdpRate * (1.0 / numBackgroundUdp);

        uint16_t udpPort = 999;
        std::string socketType = "ns3::UdpSocketFactory";

        PacketSinkHelper packetSinkHelper(socketType,
                                          InetSocketAddress(Ipv4Address::GetAny(), udpPort));
        packetSinkHelper.SetAttribute("StartTime", TimeValue(obssUdpStartTime));
        packetSinkHelper.Install(obssClientNode.Get(0));

        OnOffHelper onOffHelper(socketType, InetSocketAddress(interfaces2.GetAddress(0), udpPort));
        onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onOffHelper.SetAttribute("PacketSize", UintegerValue(1472));
        onOffHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Zero means no sending limits
        onOffHelper.SetAttribute("DataRate", DataRateValue(perStaUdpRate));
        onOffHelper.SetAttribute("StartTime", TimeValue(obssUdpStartTime));
        onOffHelper.Install(obssStaNodes);
    }

    // PCAP traces
    if (enablePcapAll)
    {
        pointToPoint.EnablePcapAll("l4s-wifi");
        wifiPhy.EnablePcap("l4s-wifi", wifiDevices);
        internetStack.EnablePcapIpv4("l4s-wifi-2-0-ip.pcap",
                                     staNode.Get(0)->GetObject<Ipv4>(),
                                     1,
                                     true);
    }
    else if (enablePcap)
    {
        pointToPoint.EnablePcap("l4s-wifi", wanDevices.Get(0));
        internetStack.EnablePcapIpv4("l4s-wifi-2-0-ip.pcap",
                                     staNode.Get(0)->GetObject<Ipv4>(),
                                     1,
                                     true);
    }

    // Set up traces
    // Bytes and throughput in WifiMacQueue
    Ptr<WifiMacQueue> apWifiMacQueue =
        apDevice.Get(0)->GetObject<WifiNetDevice>()->GetMac()->GetTxopQueue(AC_BE);
    Ptr<WifiPhy> apPhy = apDevice.Get(0)->GetObject<WifiNetDevice>()->GetPhy();
    NS_ASSERT_MSG(apPhy, "Could not acquire pointer to AP's WifiPhy");
    if (enableTracesAll || enableTraces)
    {
        g_fileBytesInAcBeQueue.open("wifi-queue-bytes.dat", std::ofstream::out);
        NS_ASSERT_MSG(apWifiMacQueue, "Could not acquire pointer to AC_BE WifiMacQueue on the AP");
        apWifiMacQueue->TraceConnectWithoutContext("BytesInQueue",
                                                   MakeCallback(&TraceBytesInAcBeQueue));
        apPhy->TraceConnect("PhyTxPsduBegin", "foreground", MakeCallback(&TraceWifiPhyTxPsduBegin));
        for (uint32_t i = 0; i < obssStaDevices.GetN(); i++)
        {
            Ptr<WifiPhy> staPhy = obssStaDevices.Get(i)->GetObject<WifiNetDevice>()->GetPhy();
            staPhy->TraceConnect("PhyTxPsduBegin",
                                 "background",
                                 MakeCallback(&TraceWifiPhyTxPsduBegin));
        }
        g_fileWifiThroughput.open("wifi-throughput.dat", std::ofstream::out);
        g_fileWifiFgThroughput.open("wifi-foreground-throughput.dat", std::ofstream::out);
        g_fileWifiBgThroughput.open("wifi-background-throughput.dat", std::ofstream::out);
        Simulator::Schedule(g_wifiThroughputInterval, &TraceWifiThroughput);
    }
    if (enableTracesAll)
    {
        g_fileWifiPhyTxPsduBegin.open("wifi-phy-tx-psdu-begin.dat", std::ofstream::out);
    }

    // Throughput and latency for foreground flows, and set up close callbacks
    if (pragueClientApps.GetN())
    {
        if (enableTracesAll || enableTraces)
        {
            g_filePragueThroughput.open("prague-throughput.dat", std::ofstream::out);
            g_filePragueThroughputPerStream.open("prague-throughput-per-stream.dat",
                                                 std::ofstream::out);
            g_filePragueCwnd.open("prague-cwnd.dat", std::ofstream::out);
            g_filePragueRtt.open("prague-rtt.dat", std::ofstream::out);
        }
        if (enableTracesAll)
        {
            g_filePragueSsthresh.open("prague-ssthresh.dat", std::ofstream::out);
            g_filePragueSendInterval.open("prague-send-interval.dat", std::ofstream::out);
            g_filePraguePacingRate.open("prague-pacing-rate.dat", std::ofstream::out);
            g_filePragueCongState.open("prague-cong-state.dat", std::ofstream::out);
            g_filePragueEcnState.open("prague-ecn-state.dat", std::ofstream::out);
        }
    }
    for (auto i = 0U; i < pragueClientApps.GetN(); i++)
    {
        // The TCP sockets that we want to connect
        Simulator::Schedule(Seconds(1) + i * flowStartOffset + TimeStep(1),
                            MakeBoundCallback(&TracePragueClientSocket,
                                              pragueClientApps.Get(i),
                                              i,
                                              enableTracesAll,
                                              enableTraces));

        pragueServerApps.Get(i)->GetObject<PacketSink>()->TraceConnectWithoutContext(
            "Accept",
            MakeCallback(&TracePragueServerSocket));

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
        if (enableTracesAll || enableTraces)
        {
            g_fileCubicThroughput.open("cubic-throughput.dat", std::ofstream::out);
            g_fileCubicThroughputPerStream.open("cubic-throughput-per-stream.dat",
                                                std::ofstream::out);
            g_fileCubicCwnd.open("cubic-cwnd.dat", std::ofstream::out);
            g_fileCubicRtt.open("cubic-rtt.dat", std::ofstream::out);
        }
        if (enableTracesAll)
        {
            g_fileCubicSsthresh.open("cubic-ssthresh.dat", std::ofstream::out);
            g_fileCubicSendInterval.open("cubic-send-interval.dat", std::ofstream::out);
            g_fileCubicPacingRate.open("cubic-pacing-rate.dat", std::ofstream::out);
            g_fileCubicCongState.open("cubic-cong-state.dat", std::ofstream::out);
            g_fileCubicRtt.open("cubic-rtt.dat", std::ofstream::out);
        }
    }
    for (auto i = 0U; i < cubicClientApps.GetN(); i++)
    {
        // The TCP sockets that we want to connect
        Simulator::Schedule(Seconds(1) + (i + numPrague) * flowStartOffset + TimeStep(1),
                            MakeBoundCallback(&TraceCubicClientSocket,
                                              cubicClientApps.Get(i),
                                              i,
                                              enableTracesAll,
                                              enableTraces));

        cubicServerApps.Get(i)->GetObject<PacketSink>()->TraceConnectWithoutContext(
            "Accept",
            MakeCallback(&TraceCubicServerSocket));

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
    if (enableTracesAll || enableTraces)
    {
        g_fileBytesInDualPi2Queue.open("wifi-dualpi2-bytes.dat", std::ofstream::out);
        dualPi2->TraceConnectWithoutContext("BytesInQueue",
                                            MakeCallback(&TraceBytesInDualPi2Queue));
    }
    if (enableTracesAll)
    {
        g_fileLSojourn.open("wifi-dualpi2-l-sojourn.dat", std::ofstream::out);
        dualPi2->TraceConnectWithoutContext("L4sSojournTime", MakeCallback(&TraceLSojourn));
        g_fileCSojourn.open("wifi-dualpi2-c-sojourn.dat", std::ofstream::out);
        dualPi2->TraceConnectWithoutContext("ClassicSojournTime", MakeCallback(&TraceCSojourn));
        // Trace Probabilities
        g_fileTraceProbChanges.open("wifi-dualpi2-TracedProbabilites.dat", std::ofstream::out);
        dualPi2->TraceConnectWithoutContext("ProbCL", MakeCallback(&TraceProbcL));
        dualPi2->TraceConnectWithoutContext("ProbL", MakeCallback(&TraceProbL));
        dualPi2->TraceConnectWithoutContext("ProbC", MakeCallback(&TraceProbC));
    }

    WifiCoTraceHelper coHelper;
    coHelper.Enable(NodeContainer(apNode, staNode, obssApNode, obssStaNodes));

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
    if (numBackgroundUdp)
    {
        std::cout << "Background UDP flows: " << numBackgroundUdp << " at per-STA rate of "
                  << perStaUdpRate.GetBitRate() / 1e6 << " Mbps" << std::endl;
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
    coHelper.PrintStatistics(std::cout);

    g_fileBytesInAcBeQueue.close();
    g_fileBytesInDualPi2Queue.close();
    g_fileLSojourn.close();
    g_fileCSojourn.close();
    g_fileWifiPhyTxPsduBegin.close();
    g_fileWifiThroughput.close();
    g_fileWifiFgThroughput.close();
    g_fileWifiBgThroughput.close();
    g_filePragueThroughput.close();
    g_filePragueThroughputPerStream.close();
    g_filePragueCwnd.close();
    g_filePragueSsthresh.close();
    g_filePragueSendInterval.close();
    g_filePraguePacingRate.close();
    g_filePragueCongState.close();
    g_filePragueEcnState.close();
    g_filePragueRtt.close();
    g_fileCubicThroughput.close();
    g_fileCubicThroughputPerStream.close();
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
TraceWifiPhyTxPsduBegin(std::string context,
                        WifiConstPsduMap psduMap,
                        WifiTxVector txVector,
                        double txPower)
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
            if (context == "foreground")
            {
                g_wifiFgThroughputData += it->second->GetAmpduSubframeSize(i);
            }
            else if (context == "background")
            {
                g_wifiBgThroughputData += it->second->GetAmpduSubframeSize(i);
            }
            else
            {
                NS_FATAL_ERROR("Unknown context: " << context);
            }
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
                                 << " " << nMpdus << " " << totalAmpduSize << " " << context
                                 << std::endl;
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
TraceProbcL(double oldVal, double pCL)
{
    g_fileTraceProbChanges << Now().GetSeconds() << " pCL " << pCL << std::endl;
}

void
TraceProbL(double oldVal, double pL)
{
    g_fileTraceProbChanges << Now().GetSeconds() << " pL " << pL << std::endl;
}

void
TraceProbC(double oldVal, double pC)
{
    g_fileTraceProbChanges << Now().GetSeconds() << " pC " << pC << std::endl;
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
    uint16_t dstPort = header.GetDestinationPort();
    g_pragueData += packet->GetSize(); // aggregate Prague throughput

    if (g_pragueDataforEachPort.find(dstPort) != g_pragueDataforEachPort.end())
    {
        g_pragueDataforEachPort[dstPort] += packet->GetSize();
    }
    else // not found , insert it
    {
        g_pragueDataforEachPort.insert({dstPort, packet->GetSize()});
    }
}

void
TracePragueThroughput()
{
    g_filePragueThroughput << Now().GetSeconds() << " " << std::fixed
                           << (g_pragueData * 8) / g_pragueThroughputInterval.GetSeconds() / 1e6
                           << std::endl;

    for (auto itr = g_pragueDataforEachPort.begin(); itr != g_pragueDataforEachPort.end();
         itr++) // print Throughput per Stream
    {
        g_filePragueThroughputPerStream
            << Now().GetSeconds() << " " << std::fixed << itr->first << " "
            << (itr->second * 8) / g_pragueThroughputInterval.GetSeconds() / 1e6 << std::endl;
        itr->second = 0;
    }
    pragueThroughputCalculator.Update((g_pragueData * 8) / g_pragueThroughputInterval.GetSeconds() /
                                      1e6);
    Simulator::Schedule(g_pragueThroughputInterval, &TracePragueThroughput);
    g_pragueData = 0;
}

void
CalculatePragueThroughput()
{
    pragueThroughputCalculator.Update((g_pragueData * 8) / g_pragueThroughputInterval.GetSeconds() /
                                      1e6);
    Simulator::Schedule(g_pragueThroughputInterval, &CalculatePragueThroughput);
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
TracePragueClientSocket(Ptr<Application> a, uint32_t i, bool enableTracesAll, bool enableTraces)
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
        if (enableTracesAll || enableTraces)
        {
            tcp->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&TracePragueCwnd));
            tcp->TraceConnectWithoutContext("RTT", MakeCallback(&TracePragueRtt));
            Simulator::Schedule(g_pragueThroughputInterval, &TracePragueThroughput);
        }
        else
        {
            Simulator::Schedule(g_pragueThroughputInterval, &CalculatePragueThroughput);
        }
        if (enableTracesAll)
        {
            tcp->TraceConnectWithoutContext("SlowStartThreshold",
                                            MakeCallback(&TracePragueSsthresh));
            tcp->TraceConnectWithoutContext("PacingRate", MakeCallback(&TracePraguePacingRate));
            tcp->TraceConnectWithoutContext("CongState", MakeCallback(&TracePragueCongState));
            tcp->TraceConnectWithoutContext("EcnState", MakeCallback(&TracePragueEcnState));
        }
    }
}

void
TracePragueServerSocket(Ptr<Socket> s)
{
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
    uint16_t dstPort = header.GetDestinationPort();
    g_cubicData += packet->GetSize(); // aggregate cubic throughput

    if (g_cubicDataforEachPort.find(dstPort) != g_cubicDataforEachPort.end())
    {
        g_cubicDataforEachPort[dstPort] += packet->GetSize();
    }
    else // not found , insert it
    {
        g_cubicDataforEachPort.insert({dstPort, packet->GetSize()});
    }
}

void
TraceCubicThroughput()
{
    g_fileCubicThroughput << Now().GetSeconds() << " " << std::fixed
                          << (g_cubicData * 8) / g_cubicThroughputInterval.GetSeconds() / 1e6
                          << std::endl;
    for (auto itr = g_cubicDataforEachPort.begin(); itr != g_cubicDataforEachPort.end();
         itr++) // print Throughput per Stream
    {
        g_fileCubicThroughputPerStream
            << Now().GetSeconds() << " " << std::fixed << itr->first << " "
            << (itr->second * 8) / g_cubicThroughputInterval.GetSeconds() / 1e6 << std::endl;
        itr->second = 0;
    }
    cubicThroughputCalculator.Update((g_cubicData * 8) / g_cubicThroughputInterval.GetSeconds() /
                                     1e6);
    Simulator::Schedule(g_cubicThroughputInterval, &TraceCubicThroughput);
    g_cubicData = 0;
}

void
CalculateCubicThroughput()
{
    cubicThroughputCalculator.Update((g_cubicData * 8) / g_cubicThroughputInterval.GetSeconds() /
                                     1e6);
    Simulator::Schedule(g_cubicThroughputInterval, &CalculateCubicThroughput);
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
TraceCubicClientSocket(Ptr<Application> a, uint32_t i, bool enableTracesAll, bool enableTraces)
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
        if (enableTracesAll || enableTraces)
        {
            tcp->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&TraceCubicCwnd));
            tcp->TraceConnectWithoutContext("RTT", MakeCallback(&TraceCubicRtt));
            Simulator::Schedule(g_cubicThroughputInterval, &TraceCubicThroughput);
        }
        else
        {
            Simulator::Schedule(g_cubicThroughputInterval, &CalculateCubicThroughput);
        }
        if (enableTracesAll)
        {
            tcp->TraceConnectWithoutContext("SlowStartThreshold",
                                            MakeCallback(&TraceCubicSsthresh));
            tcp->TraceConnectWithoutContext("PacingRate", MakeCallback(&TraceCubicPacingRate));
            tcp->TraceConnectWithoutContext("CongState", MakeCallback(&TraceCubicCongState));
        }
    }
}

void
TraceCubicServerSocket(Ptr<Socket> s)
{
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
    g_fileWifiFgThroughput << Now().GetSeconds() << " " << std::fixed
                           << (g_wifiFgThroughputData * 8) / g_wifiThroughputInterval.GetSeconds() /
                                  1e6
                           << std::endl;
    g_fileWifiBgThroughput << Now().GetSeconds() << " " << std::fixed
                           << (g_wifiBgThroughputData * 8) / g_wifiThroughputInterval.GetSeconds() /
                                  1e6
                           << std::endl;
    Simulator::Schedule(g_wifiThroughputInterval, &TraceWifiThroughput);
    g_wifiThroughputData = 0;
    g_wifiFgThroughputData = 0;
    g_wifiBgThroughputData = 0;
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

// EDCA parameters cannot currently be set using attribute values
// because there is some post-construction configuration in WifiMac
// that overwrites the attributes.  This method can be used after
// WifiHelper::Install() has been called.
void
SetAcBeEdcaParameters(const NetDeviceContainer& ndc,
                      uint32_t cwMin,
                      uint32_t cwMax,
                      uint8_t aifsn,
                      Time txopLimit)
{
    for (uint32_t i = 0; i < ndc.GetN(); i++)
    {
        auto wifiNetDevice = ndc.Get(i)->GetObject<WifiNetDevice>();
        NS_ASSERT_MSG(wifiNetDevice, "Not a WifiNetDevice: " << i);
        auto qosTxop = wifiNetDevice->GetMac()->GetQosTxop(AC_BE);
        qosTxop->SetMinCw(cwMin);
        qosTxop->SetMaxCw(cwMax);
        qosTxop->SetAifsn(aifsn);
        qosTxop->SetTxopLimit(txopLimit);
    }
}

// Note:  The below method connects the pending dequeue callback for the
// DualPi2QueueDisc that sits above the AC_BE WifiMacQueue.  It is referenced
// by the GetQueueDiscClass(0) statement below.  If, in the future, there
// is interest in hooking all four access categories, the four access
// categories (referenced by AC_BE, AC_VO, AC_VI, AC_BK) must be paired
// with the correct QueueDiscClass index.
void
ConnectPendingDequeueCallback(const NetDeviceContainer& devices,
                              const QueueDiscContainer& queueDiscs)
{
    NS_ASSERT_MSG(devices.GetN() == queueDiscs.GetN(),
                  "Configuration error: container size mismatch");
    for (uint32_t i = 0; i < devices.GetN(); i++)
    {
        auto wifiMacQueue =
            devices.Get(i)->GetObject<WifiNetDevice>()->GetMac()->GetTxopQueue(AC_BE);
        NS_ASSERT_MSG(wifiMacQueue, "Could not acquire pointer to WifiMacQueue");
        auto phy = devices.Get(i)->GetObject<WifiNetDevice>()->GetPhy();
        NS_ASSERT_MSG(phy, "Could not acquire pointer to WifiPhy");
        auto dualPi2 =
            queueDiscs.Get(i)->GetQueueDiscClass(0)->GetQueueDisc()->GetObject<DualPi2QueueDisc>();
        NS_ASSERT_MSG(dualPi2, "Could not acquire pointer to DualPi2QueueDisc");
        // Hook DualPi2 queue to WifiMacQueue::PendingDequeue trace source
        bool connected = wifiMacQueue->TraceConnectWithoutContext(
            "PendingDequeue",
            MakeCallback(&DualPi2QueueDisc::PendingDequeueCallback, dualPi2));
        NS_ASSERT_MSG(connected, "Could not hook DualPi2 queue to WifiMacQueue trace source");
    }
}

void
UpdateDualPi2AggBufferLimit(const QueueDiscContainer& queueDiscs, double scale, uint32_t limit)
{
    for (uint32_t i = 0; i < queueDiscs.GetN(); i++)
    {
        // There are four queue disc classes (this is an MQ queue disc)
        // so update the limit for each DualPi2QueueDisc (even though we
        // are only using the AC_BE one for now)
        for (uint32_t j = 0; j < queueDiscs.Get(i)->GetNQueueDiscClasses(); j++)
        {
            auto dualPi2 = queueDiscs.Get(i)
                               ->GetQueueDiscClass(j)
                               ->GetQueueDisc()
                               ->GetObject<DualPi2QueueDisc>();
            if (dualPi2)
            {
                dualPi2->SetAggregationBufferLimit(static_cast<uint32_t>(scale * limit));
                dualPi2->SetAttribute("QueueLimit",
                                      UintegerValue(static_cast<uint32_t>(scale * limit * 100)));
            }
        }
    }
}

void
UpdateDynamicQueueLimits(Ptr<WifiNetDevice> device, double scale, uint32_t limit)
{
    Ptr<NetDeviceQueueInterface> interface = device->GetObject<NetDeviceQueueInterface>();
    for (uint32_t i = 0; i < interface->GetNTxQueues(); i++)
    {
        Ptr<NetDeviceQueue> queueInterface = interface->GetTxQueue(i);
        Ptr<DynamicQueueLimits> queueLimits =
        DynamicCast<DynamicQueueLimits>(queueInterface->GetQueueLimits());
        NS_ABORT_MSG_UNLESS(queueLimits, "Downcast failed");
        queueLimits->SetAttribute("MinLimit", UintegerValue(static_cast<uint32_t>(scale * limit)));
        queueLimits->SetAttribute("MaxLimit", UintegerValue(static_cast<uint32_t>(scale * limit)));
    }
}

void
ChangeMcs(uint16_t mcs,
          uint32_t limit,
          uint16_t nextMcs,
          uint32_t nextLimit,
          double scale,
          Time mcsChangeInterval)
{
    NS_LOG_DEBUG("Changing MCS from " << mcs << " limit from " << limit << " to MCS " << nextMcs
                                      << " limit " << nextLimit);

    UpdateDynamicQueueLimits(apWifiNetDevice, scale, nextLimit);
    UpdateDynamicQueueLimits(staWifiNetDevice, scale, nextLimit);
    UpdateDualPi2AggBufferLimit(apQueueDiscContainer, scale, nextLimit);
    UpdateDualPi2AggBufferLimit(staQueueDiscContainer, scale, nextLimit);

    std::ostringstream newDataMode;
    newDataMode << "HeMcs" << nextMcs;
    apWifiMgr->SetAttribute("DataMode",StringValue(newDataMode.str())); //change mcs level

    // reschedule
    Simulator::Schedule(mcsChangeInterval,
                        &ChangeMcs,
                        nextMcs,
                        nextLimit,
                        mcs,
                        limit,
                        scale,
                        mcsChangeInterval);
}

uint32_t
CalculateLimit(uint32_t mcs, uint32_t channelWidth, uint32_t spatialStreams, Time txopLimit)
{
     /*
        NOTES:
        ppduDurationLimit = availableTime - protectionTime - acknowledgmentTime; //From  L:386 , QosFrameExchangeManager

    */

    auto dataRate = HePhy::GetDataRate(mcs, channelWidth, GUARD_INTERVAL, spatialStreams); // bits/sec
    Time mpduTxDuration = Seconds((MTU_SIZE + MAC_HEADER_SIZE) * 8.0 / dataRate);
    Time ppduDurationLimit= txopLimit - (PROTECTION_TIME + ACK_TIME);
    Time actualTxopLimit= ppduDurationLimit - PREAMBLE_AND_HEADER_DURATION;
    
    uint32_t actualTransmitNPackets=static_cast<uint32_t> ((actualTxopLimit / mpduTxDuration).GetHigh());

    if (actualTransmitNPackets < 1) 
    {
        actualTransmitNPackets = 1;
        actualTxopLimit = mpduTxDuration;
        Time newtxopLimit = actualTxopLimit + PROTECTION_TIME + ACK_TIME + PREAMBLE_AND_HEADER_DURATION;
        std::cout << "Warning: txopLimit set too low (" << txopLimit << ") using " << newtxopLimit << std::endl;
    }

    uint32_t calculatedLimitNBytes = (actualTransmitNPackets) * MTU_SIZE;

    NS_LOG_DEBUG("mcs: "<< mcs <<"   channelWidth: "<< channelWidth
    <<"   spatialStreams: "<< spatialStreams <<"   txopLimit: "<<txopLimit.As(Time::US) 
    <<"   actualTxopLimit: "<<actualTxopLimit.GetMicroSeconds()<<"   TxDuration: "<<actualTransmitNPackets*mpduTxDuration.GetMicroSeconds()<<"   mpduTxDuration: "<<mpduTxDuration.GetMicroSeconds()
    <<"   actualTransmitNPackets: "<< actualTransmitNPackets <<"   calculatedLimitNBytes: "<<calculatedLimitNBytes);

    return calculatedLimitNBytes;
}

void
AddManualArpEntries(Ptr<Channel> channel)
{
    NeighborCacheHelper nch;
    nch.PopulateNeighborCache(channel);
}
