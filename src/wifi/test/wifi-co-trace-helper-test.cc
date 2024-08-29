/*
 * Copyright (c) 2009 University of Washington
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
 */

#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/he-phy.h"
#include "ns3/mobility-helper.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/object.h"
#include "ns3/packet-socket-client.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-server.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/test.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-co-trace-helper.h"
#include "ns3/wifi-helper.h"
#include "ns3/wifi-mpdu.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy-state.h"
#include "ns3/wifi-psdu.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("WifiCoHelperTest");

/**
 * It's a base class with some utility methods for other test cases in this file.
 */
class WifiCoHelperBaseTestCase : public TestCase
{
  public:
    WifiCoHelperBaseTestCase(std::string message)
        : TestCase(message)
    {
        m_wificohelper.Stop(m_simulationStop);
    }

    ~WifiCoHelperBaseTestCase() override
    {
    }

  protected:
    /**
     * It broadcasts a number of packets from a node and its Phy.
     */
    void SendPackets(size_t num, size_t nodeId, size_t phyId);

    /*
       This function measures the time-duration that a state occupies on a Phy. It should be
       attached to the WifiPhyStateHelper's trace source to receive callbacks. It sums up the
       durations in the function argument received by reference.
    */
    void MeasureExpectedDuration(WifiPhyState forState,
                                 Time& sumOfDurations,
                                 Time callbackStart,
                                 Time callbackDuration,
                                 WifiPhyState callbackState);

    /**
     * It gets WifiPhyStateHelper attached to a node and its Phy.
     */
    Ptr<WifiPhyStateHelper> GetPhyStateHelper(size_t nodeId, size_t phyId);

    /**
     * It get the channel occupancy measured by WifiCoTraceHelper attached to a node and its Phy.
     */
    const std::map<WifiPhyState, Time>& GetChannelOccupancy(size_t nodeId, size_t phyId);

    /**
     * It asserts that the two channel occupancy values match with each other.
     */
    void CheckChannelOccupancy(const std::map<WifiPhyState, Time>& actual,
                               const std::map<WifiPhyState, Time>& expected);

    Time m_simulationStop{Seconds(10.0)};
    WifiCoTraceHelper m_wificohelper{};
    NodeContainer m_nodes;
    NetDeviceContainer m_devices;
};

const std::map<WifiPhyState, Time>&
WifiCoHelperBaseTestCase::GetChannelOccupancy(size_t nodeId, size_t phyId)
{
    auto& devRecords = m_wificohelper.GetDeviceRecords();
    auto senderRecord = std::find_if(devRecords.begin(), devRecords.end(), [nodeId](auto& x) {
        return x.m_nodeId == nodeId;
    });
    auto& phy0Stats = senderRecord->m_linkStateDurations[phyId];
    return phy0Stats;
}

Ptr<WifiPhyStateHelper>
WifiCoHelperBaseTestCase::GetPhyStateHelper(size_t nodeId, size_t phyId)
{
    auto wifiDevice = DynamicCast<WifiNetDevice>(m_devices.Get(nodeId));
    PointerValue v;
    wifiDevice->GetPhy(phyId)->GetAttribute("State", v);
    auto wifiPhyStateHelper = v.Get<WifiPhyStateHelper>();
    return wifiPhyStateHelper;
}

void
WifiCoHelperBaseTestCase::MeasureExpectedDuration(WifiPhyState forState,
                                                  Time& expected,
                                                  Time callbackStart,
                                                  Time callbackDuration,
                                                  WifiPhyState callbackState)
{
    if (forState == callbackState)
    {
        expected += callbackDuration;
    }
}

void
WifiCoHelperBaseTestCase::SendPackets(size_t num, size_t nodeId, size_t phyId)
{
    auto dev = DynamicCast<WifiNetDevice>(m_devices.Get(nodeId));
    auto tx_phy = dev->GetPhys().at(phyId);
    double txPower = -80;
    tx_phy->SetTxPowerStart(txPower);
    tx_phy->SetTxPowerEnd(txPower);

    WifiTxVector txVector =
        WifiTxVector(HePhy::GetHeMcs0(), 0, WIFI_PREAMBLE_HE_SU, 800, 1, 1, 0, 20, true);

    std::vector<Ptr<WifiMpdu>> mpduList;

    WifiMacHeader hdr1;
    hdr1.SetType(WIFI_MAC_QOSDATA);
    hdr1.SetQosTid(0);
    hdr1.SetAddr1(Mac48Address::GetBroadcast());
    auto p1 = Create<Packet>(1000);
    mpduList.emplace_back(Create<WifiMpdu>(p1, hdr1));

    auto psdu = Create<WifiPsdu>(mpduList);
    tx_phy->Send(psdu, txVector);
}

void
WifiCoHelperBaseTestCase::CheckChannelOccupancy(const std::map<WifiPhyState, Time>& actual,
                                                const std::map<WifiPhyState, Time>& expected)
{
    for (const WifiPhyState s :
         {WifiPhyState::TX, WifiPhyState::RX, WifiPhyState::IDLE, WifiPhyState::CCA_BUSY})
    {
        if (expected.at(s) == Seconds(0))
        {
            NS_TEST_ASSERT_MSG_EQ((actual.find(s) == actual.end()),
                                  true,
                                  "State " << s << " shouldn't be measured");
        }
        else
        {
            auto it = actual.find(s);
            NS_TEST_ASSERT_MSG_EQ((it != actual.end()),
                                  true,
                                  "State " << s << " should be measured");
            NS_TEST_ASSERT_MSG_EQ(it->second, expected.at(s), "Measured duration should be same");
        }
    }
}

/**
 * \ingroup wifi-test
 * \brief
 *
 * This test case configures two ad-hoc Wi-Fi STAs and configures one STA to send a single
 * broadcast packet to the other at time 5 seconds.  It also configures a WifCoHelper
 * on both STAs.
 *
 * Send one packet from one WifiNetDevice to other.
 * Assert on TX duration at the sender and RX duration at the receiver.
 */
class SendOnePacketTestCase : public WifiCoHelperBaseTestCase
{
  public:
    /** Constructor. */
    SendOnePacketTestCase();
    /** Destructor. */
    ~SendOnePacketTestCase() override;

  private:
    void DoRun() override;
    void DoSetup() override;
    void DoTeardown() override;
};

SendOnePacketTestCase::SendOnePacketTestCase()
    : WifiCoHelperBaseTestCase("Send one packet from one WifiNetDevice to other.")
{
}

/**
 * This destructor does nothing but we include it as a reminder that
 * the test case should clean up after itself
 */
SendOnePacketTestCase::~SendOnePacketTestCase()
{
}

void
SendOnePacketTestCase::DoRun()
{
    // The network is setup such that there are only two nodes. Each node is a single-link device
    // (SLD). One node transmits a packet to another.
    const size_t numDevices = 2;
    const size_t numPhys = 1;

    std::map<WifiPhyState, Time> expectedDurations[numDevices][numPhys];

    /**
     * Calculate expected durations through trace callback.
     */
    for (size_t i = 0; i < numDevices; i++)
    {
        for (size_t j = 0; j < numPhys; j++)
        {
            for (const WifiPhyState s :
                 {WifiPhyState::TX, WifiPhyState::RX, WifiPhyState::IDLE, WifiPhyState::CCA_BUSY})
            {
                auto phyHelper = GetPhyStateHelper(i, j);

                auto callback = MakeCallback(&SendOnePacketTestCase::MeasureExpectedDuration, this)
                                    .Bind(s, std::ref(expectedDurations[i][j][s]));
                phyHelper->TraceConnectWithoutContext("State", callback);
            }
        }
    }

    Simulator::Schedule(Seconds(1.0),
                        &SendOnePacketTestCase::SendPackets,
                        this,
                        1,
                        0 /*nodeId*/,
                        0 /*phyId*/);
    Simulator::Stop(m_simulationStop);

    Simulator::Run();
    Simulator::Destroy();

    m_wificohelper.PrintStatistics(std::cout);

    for (size_t d = 0; d < numDevices; d++)
    {
        for (size_t p = 0; p < numPhys; p++)
        {
            // Match the map returned by WifiCoTraceHelper with Expected values.
            auto& actual = GetChannelOccupancy(d, p);
            auto& expected = expectedDurations[d][p];
            CheckChannelOccupancy(actual, expected);
        }
    }
}

void
SendOnePacketTestCase::DoSetup()
{
    // LogComponentEnable("WifiCoTraceHelper", LOG_LEVEL_INFO);

    uint32_t nWifi = 2;
    m_nodes.Create(nWifi);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    WifiHelper wifi;

    mac.SetType("ns3::AdhocWifiMac");
    m_devices = wifi.Install(phy, mac, m_nodes);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    auto distance = 0.1;
    positionAlloc->Add(Vector(distance, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(m_nodes);

    // Adding nodes to wificohelper
    m_wificohelper.Enable(m_nodes);
}

void
SendOnePacketTestCase::DoTeardown()
{
    for (size_t i = 0; i < m_nodes.GetN(); i++)
    {
        m_nodes.Get(i)->Dispose();
    }
}

/**
 * \ingroup wifi-test
 * \brief
 *
 * This test case configures two ad-hoc Wi-Fi STAs with three links. It configures one STA to send a
 * single broadcast packet to the other on each link.  It also configures a WifCoHelper on both
 * STAs.
 *
 */
class MLOTestCase : public WifiCoHelperBaseTestCase
{
  public:
    /** Constructor. */
    MLOTestCase();
    /** Destructor. */
    ~MLOTestCase() override;

  private:
    void DoRun() override;
    void DoSetup() override;
    void DoTeardown() override;
};

MLOTestCase::MLOTestCase()
    : WifiCoHelperBaseTestCase(
          "Assert that channel occupancy is measured on each link of a multi-link device (MLD).")
{
}

/**
 * This destructor does nothing but we include it as a reminder that
 * the test case should clean up after itself
 */
MLOTestCase::~MLOTestCase()
{
}

void
MLOTestCase::DoRun()
{
    // The network is setup such that there are only two nodes. Each node is a multi-link device
    // (MLD) with three links. One node transmits a packet to another on each link.
    const size_t numDevices = 2;
    const size_t numPhys = 3;

    std::map<WifiPhyState, Time> expectedDurations[numDevices][numPhys];

    for (size_t i = 0; i < numDevices; i++)
    {
        for (size_t j = 0; j < numPhys; j++)
        {
            for (const WifiPhyState s :
                 {WifiPhyState::TX, WifiPhyState::RX, WifiPhyState::IDLE, WifiPhyState::CCA_BUSY})
            {
                auto phyHelper = GetPhyStateHelper(i, j);

                auto callback = MakeCallback(&MLOTestCase::MeasureExpectedDuration, this)
                                    .Bind(s, std::ref(expectedDurations[i][j][s]));
                phyHelper->TraceConnectWithoutContext("State", callback);
            }
        }
    }

    Simulator::Schedule(Seconds(1.0),
                        &MLOTestCase::SendPackets,
                        this,
                        1,
                        0 /*nodeId*/,
                        0 /*phyId*/);
    Simulator::Schedule(Seconds(1.1),
                        &MLOTestCase::SendPackets,
                        this,
                        1,
                        0 /*nodeId*/,
                        1 /*phyId*/);
    Simulator::Schedule(Seconds(1.2),
                        &MLOTestCase::SendPackets,
                        this,
                        1,
                        0 /*nodeId*/,
                        2 /*phyId*/);
    Simulator::Stop(m_simulationStop);

    Simulator::Run();
    Simulator::Destroy();

    m_wificohelper.PrintStatistics(std::cout);

    for (size_t d = 0; d < numDevices; d++)
    {
        for (size_t p = 0; p < numPhys; p++)
        {
            // Match the map returned by WifiCoTraceHelper with Expected values.
            auto& actual = GetChannelOccupancy(d, p);
            auto& expected = expectedDurations[d][p];
            CheckChannelOccupancy(actual, expected);
        }
    }

    // Assert that statistics after reset should be cleared.
    m_wificohelper.Reset();
    NS_TEST_ASSERT_MSG_EQ((m_wificohelper.GetDeviceRecords().size()),
                          numDevices,
                          "Placeholder for device records shouldn't be cleared");
    m_wificohelper.PrintStatistics(std::cout);
    for (size_t d = 0; d < numDevices; d++)
    {
        for (size_t p = 0; p < numPhys; p++)
        {
            // Match the map returned by WifiCoTraceHelper with Expected values.
            auto& statistics = GetChannelOccupancy(d, p);
            NS_TEST_ASSERT_MSG_EQ((statistics.empty()), true, "Statistics should be cleared");
        }
    }
}

void
MLOTestCase::DoSetup()
{
    // LogComponentEnable("WifiCoTraceHelper", LOG_LEVEL_INFO);

    uint32_t nWifi = 2;
    m_nodes.Create(nWifi);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211be);

    // Create multiple spectrum channels
    Ptr<MultiModelSpectrumChannel> spectrumChannel2_4Ghz =
        CreateObject<MultiModelSpectrumChannel>();
    Ptr<MultiModelSpectrumChannel> spectrumChannel5Ghz = CreateObject<MultiModelSpectrumChannel>();
    Ptr<MultiModelSpectrumChannel> spectrumChannel6Ghz = CreateObject<MultiModelSpectrumChannel>();

    // SpectrumWifiPhyHelper (3 links)
    SpectrumWifiPhyHelper phy(3);
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.AddChannel(spectrumChannel2_4Ghz, WIFI_SPECTRUM_2_4_GHZ);
    phy.AddChannel(spectrumChannel5Ghz, WIFI_SPECTRUM_5_GHZ);
    phy.AddChannel(spectrumChannel6Ghz, WIFI_SPECTRUM_6_GHZ);

    // configure operating channel for each link
    phy.Set(0, "ChannelSettings", StringValue("{0, 20, BAND_2_4GHZ, 0}"));
    phy.Set(1, "ChannelSettings", StringValue("{0, 20, BAND_5GHZ, 0}"));
    phy.Set(2, "ChannelSettings", StringValue("{0, 20, BAND_6GHZ, 0}"));

    // configure rate manager for each link
    wifi.SetRemoteStationManager(1,
                                 "ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("EhtMcs9"),
                                 "ControlMode",
                                 StringValue("OfdmRate24Mbps"));
    wifi.SetRemoteStationManager(2,
                                 "ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("EhtMcs7"),
                                 "ControlMode",
                                 StringValue("HeMcs4"));

    uint8_t linkId = 0;
    wifi.SetRemoteStationManager(linkId,
                                 "ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("EhtMcs11"),
                                 "ControlMode",
                                 StringValue("OfdmRate24Mbps"));

    mac.SetType("ns3::AdhocWifiMac");
    m_devices = wifi.Install(phy, mac, m_nodes);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    auto distance = 0.1;
    positionAlloc->Add(Vector(distance, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(m_nodes);

    m_wificohelper.Enable(m_nodes);
}

void
MLOTestCase::DoTeardown()
{
    for (size_t i = 0; i < m_nodes.GetN(); i++)
    {
        m_nodes.Get(i)->Dispose();
    }
}

/**
 * \ingroup wifi-test
 * \brief
 *
 * This test case configures one AP and one STA on a single link. It configures the STA to send
 * traffic to AP at a saturated offered load. It configures WifiCoTraceHelper on both AP and STA.
 */
class SaturatedOfferedLoadTestCase : public WifiCoHelperBaseTestCase
{
  public:
    /** Constructor. */
    SaturatedOfferedLoadTestCase();
    /** Destructor. */
    ~SaturatedOfferedLoadTestCase() override;

  private:
    void DoRun() override;
    void DoSetup() override;
    Ptr<PacketSocketClient> GetClientApplication(const PacketSocketAddress& sockAddr,
                                                 const std::size_t pktSize,
                                                 const Time& interval,
                                                 const Time& start);
    void DoTeardown() override;
};

SaturatedOfferedLoadTestCase::SaturatedOfferedLoadTestCase()
    : WifiCoHelperBaseTestCase("A saturated wifi network with one AP and an uplink STA")
{
}

/**
 * This destructor does nothing but we include it as a reminder that
 * the test case should clean up after itself
 */
SaturatedOfferedLoadTestCase::~SaturatedOfferedLoadTestCase()
{
}

void
SaturatedOfferedLoadTestCase::DoRun()
{
    // The network is setup such that there is one uplink STA (Node 0) and one AP (Node 1).
    // Each node is a single-link device (SLD). Application installed on STA generates a saturating
    // workload.
    const size_t numDevices = 2;
    const size_t numPhys = 1;

    std::map<WifiPhyState, Time> expectedDurations[numDevices][numPhys];

    for (size_t i = 0; i < numDevices; i++)
    {
        for (size_t j = 0; j < numPhys; j++)
        {
            for (const WifiPhyState s :
                 {WifiPhyState::TX, WifiPhyState::RX, WifiPhyState::IDLE, WifiPhyState::CCA_BUSY})
            {
                auto phyHelper = GetPhyStateHelper(i, j);

                auto callback =
                    MakeCallback(&SaturatedOfferedLoadTestCase::MeasureExpectedDuration, this)
                        .Bind(s, std::ref(expectedDurations[i][j][s]));
                phyHelper->TraceConnectWithoutContext("State", callback);
            }
        }
    }

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();

    m_wificohelper.PrintStatistics(std::cout);

    for (size_t d = 0; d < numDevices; d++)
    {
        for (size_t p = 0; p < numPhys; p++)
        {
            // Match the map returned by WifiCoTraceHelper with Expected values.
            auto& actual = GetChannelOccupancy(d, p);
            auto& expected = expectedDurations[d][p];
            CheckChannelOccupancy(actual, expected);
        }
    }
}

void
SaturatedOfferedLoadTestCase::DoSetup()
{
    // LogComponentEnable("WifiCoTraceHelper", LOG_LEVEL_INFO);

    uint32_t nWifi = 1;

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWifi);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    WifiHelper wifi;

    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    NetDeviceContainer apDevices;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    apDevices = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(0.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(5.0),
                                  "DeltaY",
                                  DoubleValue(10.0),
                                  "GridWidth",
                                  UintegerValue(3),
                                  "LayoutType",
                                  StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    m_nodes.Add(wifiStaNodes);
    m_nodes.Add(wifiApNode);

    m_devices.Add(staDevices);
    m_devices.Add(apDevices);

    m_wificohelper.Enable(m_nodes);

    // Install packet socket on all nodes
    PacketSocketHelper packetSocket;
    packetSocket.Install(m_nodes);

    for (auto nodeIt = wifiApNode.Begin(); nodeIt != wifiApNode.End(); ++nodeIt)
    {
        PacketSocketAddress srvAddr;
        auto device = DynamicCast<WifiNetDevice>((*nodeIt)->GetDevice(0));
        srvAddr.SetSingleDevice(device->GetIfIndex());
        srvAddr.SetProtocol(1);
        auto psServer = CreateObject<PacketSocketServer>();
        psServer->SetLocal(srvAddr);
        (*nodeIt)->AddApplication(psServer);
        psServer->SetStartTime(Seconds(0));
        psServer->SetStopTime(m_simulationStop);
    }

    for (uint32_t i = 0; i < staDevices.GetN(); ++i)
    {
        Ptr<WifiNetDevice> clientDevice = DynamicCast<WifiNetDevice>(staDevices.Get(i));
        Ptr<WifiNetDevice> serverDevice = DynamicCast<WifiNetDevice>(apDevices.Get(0));

        PacketSocketAddress sockAddr;
        sockAddr.SetSingleDevice(clientDevice->GetIfIndex());
        sockAddr.SetPhysicalAddress(serverDevice->GetAddress());
        sockAddr.SetProtocol(1);

        wifiStaNodes.Get(i)->AddApplication(
            GetClientApplication(sockAddr, 1000, MicroSeconds(10), Seconds(0.0)));
    }
}

Ptr<PacketSocketClient>
SaturatedOfferedLoadTestCase::GetClientApplication(const PacketSocketAddress& sockAddr,
                                                   const std::size_t pktSize,
                                                   const Time& interval,
                                                   const Time& start)
{
    auto client = CreateObject<PacketSocketClient>();
    client->SetAttribute("PacketSize", UintegerValue(pktSize));
    client->SetAttribute("MaxPackets", UintegerValue(0));
    client->SetAttribute("Interval", TimeValue(interval));
    client->SetAttribute("Priority", UintegerValue(0));
    client->SetRemote(sockAddr);
    client->SetStartTime(start);
    return client;
}

void
SaturatedOfferedLoadTestCase::DoTeardown()
{
    for (size_t i = 0; i < m_nodes.GetN(); i++)
    {
        m_nodes.Get(i)->Dispose();
    }
}

/**
 * \ingroup wifi-test
 * \brief Wifi Channel Occupancy Helper Test Suite
 */
class WifiCoHelperTestSuite : public TestSuite
{
  public:
    /** Constructor. */
    WifiCoHelperTestSuite();
};

WifiCoHelperTestSuite::WifiCoHelperTestSuite()
    : TestSuite("wifi-co-trace-helper", Type::UNIT)
{
    AddTestCase(new SendOnePacketTestCase, TestCase::Duration::QUICK);
    AddTestCase(new MLOTestCase, TestCase::Duration::QUICK);
    AddTestCase(new SaturatedOfferedLoadTestCase, TestCase::Duration::QUICK);
}

/**
 * \ingroup wifi-test
 * WifiCoHelperTestSuite instance variable.
 */
static WifiCoHelperTestSuite g_WifiCoHelperTestSuite;
