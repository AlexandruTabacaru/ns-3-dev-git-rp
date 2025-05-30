/*
 * Copyright (c) 2013 Budiarto Herman
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Budiarto Herman <budiarto.herman@magister.fi>
 *
 */

#include "lte-test-cell-selection.h"

#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/integer.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/log.h"
#include "ns3/lte-enb-net-device.h"
#include "ns3/lte-helper.h"
#include "ns3/lte-ue-net-device.h"
#include "ns3/lte-ue-rrc.h"
#include "ns3/mobility-helper.h"
#include "ns3/net-device-container.h"
#include "ns3/node-container.h"
#include "ns3/point-to-point-epc-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/simulator.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteCellSelectionTest");

/*
 * This test suite sets up four cells (four eNBs) and six UEs.  Two of the
 * cells are CSG and two are non-CSG.  The six UEs associate with cells
 * based on their positions and random access procedures.  The test checks
 * that the UEs, at specirfic simulation times, are associated to expected
 * cells.  See the header Doxygen for more specific descriptions.
 *
 * The test conditions that are checked rely on the RA preamble values
 * (randomly selected) being unique for the six UEs.  For most values of
 * random seed and run number, this will be the case, but certain values
 * will cause a collision (same RA preamble drawn for two UEs).  Therefore,
 * this test fixes the RngSeed and RngRun values, and uses AssignStreams,
 * to ensure that an RA preamble collision does not occur.
 */

LteCellSelectionTestSuite::LteCellSelectionTestSuite()
    : TestSuite("lte-cell-selection", Type::SYSTEM)
{
    std::vector<LteCellSelectionTestCase::UeSetup_t> w;

    // REAL RRC PROTOCOL

    w = {
        // x, y, csgMember, checkPoint, cell1, cell2
        LteCellSelectionTestCase::UeSetup_t(0.0, 0.55, false, MilliSeconds(283), 1, 0),
        LteCellSelectionTestCase::UeSetup_t(0.0, 0.45, false, MilliSeconds(283), 1, 0),
        LteCellSelectionTestCase::UeSetup_t(0.5, 0.45, false, MilliSeconds(363), 1, 3),
        LteCellSelectionTestCase::UeSetup_t(0.5, 0.0, true, MilliSeconds(283), 2, 4),
        LteCellSelectionTestCase::UeSetup_t(1.0, 0.55, true, MilliSeconds(283), 3, 0),
        LteCellSelectionTestCase::UeSetup_t(1.0, 0.45, true, MilliSeconds(283), 4, 0),
    };

    AddTestCase(new LteCellSelectionTestCase("EPC, real RRC", true, false, 60.0 /* isd */, w),
                TestCase::Duration::QUICK);

    // IDEAL RRC PROTOCOL

    w = {
        // x, y, csgMember, checkPoint, cell1, cell2
        LteCellSelectionTestCase::UeSetup_t(0.0, 0.55, false, MilliSeconds(266), 1, 0),
        LteCellSelectionTestCase::UeSetup_t(0.0, 0.45, false, MilliSeconds(266), 1, 0),
        LteCellSelectionTestCase::UeSetup_t(0.5, 0.45, false, MilliSeconds(346), 1, 3),
        LteCellSelectionTestCase::UeSetup_t(0.5, 0.0, true, MilliSeconds(266), 2, 4),
        LteCellSelectionTestCase::UeSetup_t(1.0, 0.55, true, MilliSeconds(266), 3, 0),
        LteCellSelectionTestCase::UeSetup_t(1.0, 0.45, true, MilliSeconds(266), 4, 0),
    };

    AddTestCase(new LteCellSelectionTestCase("EPC, ideal RRC", true, true, 60.0 /* isd */, w),
                TestCase::Duration::QUICK);

} // end of LteCellSelectionTestSuite::LteCellSelectionTestSuite ()

/**
 * @ingroup lte-test
 * Static variable for test initialization
 */
static LteCellSelectionTestSuite g_lteCellSelectionTestSuite;

/*
 * Test Case
 */

LteCellSelectionTestCase::UeSetup_t::UeSetup_t(double relPosX,
                                               double relPosY,
                                               bool isCsgMember,
                                               Time checkPoint,
                                               uint16_t expectedCellId1,
                                               uint16_t expectedCellId2)
    : position(Vector(relPosX, relPosY, 0.0)),
      isCsgMember(isCsgMember),
      checkPoint(checkPoint),
      expectedCellId1(expectedCellId1),
      expectedCellId2(expectedCellId2)
{
}

LteCellSelectionTestCase::LteCellSelectionTestCase(std::string name,
                                                   bool isEpcMode,
                                                   bool isIdealRrc,
                                                   double interSiteDistance,
                                                   std::vector<UeSetup_t> ueSetupList)
    : TestCase(name),
      m_isEpcMode(isEpcMode),
      m_isIdealRrc(isIdealRrc),
      m_interSiteDistance(interSiteDistance),
      m_ueSetupList(ueSetupList)
{
    NS_LOG_FUNCTION(this << GetName());
    m_lastState.resize(m_ueSetupList.size(), LteUeRrc::NUM_STATES);
}

LteCellSelectionTestCase::~LteCellSelectionTestCase()
{
    NS_LOG_FUNCTION(this << GetName());
}

void
LteCellSelectionTestCase::DoRun()
{
    NS_LOG_FUNCTION(this << GetName());

    // In ns-3 test suite operation, static variables persist across all
    // tests (all test suites execute within a single ns-3 process).
    // Therefore, to fix a seed and run number for a specific test, the
    // current values of seed and run number should be saved and restored
    // after the test is run.
    uint32_t previousSeed = RngSeedManager::GetSeed();
    uint64_t previousRun = RngSeedManager::GetRun();
    // Values of 1 and 2 here will prevent RA preamble collisions
    Config::SetGlobal("RngSeed", UintegerValue(1));
    Config::SetGlobal("RngRun", UintegerValue(2));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetAttribute("PathlossModel", StringValue("ns3::FriisSpectrumPropagationLossModel"));
    lteHelper->SetAttribute("UseIdealRrc", BooleanValue(m_isIdealRrc));

    Ptr<PointToPointEpcHelper> epcHelper;

    if (m_isEpcMode)
    {
        epcHelper = CreateObject<PointToPointEpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);
    }

    /*
     * The topology is the following (the number on the node indicate the cell ID)
     *
     *      [1]        [3]
     *    non-CSG -- non-CSG
     *       |          |
     *       |          | 60 m
     *       |          |
     *      [2]        [4]
     *      CSG ------ CSG
     *           60 m
     */

    // Create Nodes
    NodeContainer enbNodes;
    enbNodes.Create(4);
    NodeContainer ueNodes;
    auto nUe = static_cast<uint16_t>(m_ueSetupList.size());
    ueNodes.Create(nUe);

    // Assign nodes to position
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    // eNodeB
    positionAlloc->Add(Vector(0.0, m_interSiteDistance, 0.0));
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(m_interSiteDistance, m_interSiteDistance, 0.0));
    positionAlloc->Add(Vector(m_interSiteDistance, 0.0, 0.0));
    // UE
    std::vector<UeSetup_t>::const_iterator itSetup;
    for (itSetup = m_ueSetupList.begin(); itSetup != m_ueSetupList.end(); itSetup++)
    {
        Vector uePos(m_interSiteDistance * itSetup->position.x,
                     m_interSiteDistance * itSetup->position.y,
                     m_interSiteDistance * itSetup->position.z);
        NS_LOG_INFO("UE position " << uePos);
        positionAlloc->Add(uePos);
    }

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Create Devices and install them in the Nodes (eNB and UE)
    int64_t stream = 1;
    NetDeviceContainer enbDevs;

    // cell ID 1 is a non-CSG cell
    lteHelper->SetEnbDeviceAttribute("CsgId", UintegerValue(0));
    lteHelper->SetEnbDeviceAttribute("CsgIndication", BooleanValue(false));
    enbDevs.Add(lteHelper->InstallEnbDevice(enbNodes.Get(0)));

    // cell ID 2 is a CSG cell
    lteHelper->SetEnbDeviceAttribute("CsgId", UintegerValue(1));
    lteHelper->SetEnbDeviceAttribute("CsgIndication", BooleanValue(true));
    enbDevs.Add(lteHelper->InstallEnbDevice(enbNodes.Get(1)));

    // cell ID 3 is a non-CSG cell
    lteHelper->SetEnbDeviceAttribute("CsgId", UintegerValue(0));
    lteHelper->SetEnbDeviceAttribute("CsgIndication", BooleanValue(false));
    enbDevs.Add(lteHelper->InstallEnbDevice(enbNodes.Get(2)));

    // cell ID 4 is a CSG cell
    lteHelper->SetEnbDeviceAttribute("CsgId", UintegerValue(1));
    lteHelper->SetEnbDeviceAttribute("CsgIndication", BooleanValue(true));
    enbDevs.Add(lteHelper->InstallEnbDevice(enbNodes.Get(3)));

    NetDeviceContainer ueDevs;
    Time lastCheckPoint;
    NS_ASSERT(m_ueSetupList.size() == ueNodes.GetN());
    NodeContainer::Iterator itNode;
    for (itSetup = m_ueSetupList.begin(), itNode = ueNodes.Begin();
         itSetup != m_ueSetupList.end() || itNode != ueNodes.End();
         itSetup++, itNode++)
    {
        if (itSetup->isCsgMember)
        {
            lteHelper->SetUeDeviceAttribute("CsgId", UintegerValue(1));
        }
        else
        {
            lteHelper->SetUeDeviceAttribute("CsgId", UintegerValue(0));
        }

        NetDeviceContainer devs = lteHelper->InstallUeDevice(*itNode);
        Ptr<LteUeNetDevice> ueDev = devs.Get(0)->GetObject<LteUeNetDevice>();
        NS_ASSERT(ueDev);
        ueDevs.Add(devs);
        Simulator::Schedule(itSetup->checkPoint,
                            &LteCellSelectionTestCase::CheckPoint,
                            this,
                            ueDev,
                            itSetup->expectedCellId1,
                            itSetup->expectedCellId2);

        if (lastCheckPoint < itSetup->checkPoint)
        {
            lastCheckPoint = itSetup->checkPoint;
        }
    }

    stream += lteHelper->AssignStreams(enbDevs, stream);
    stream += lteHelper->AssignStreams(ueDevs, stream);

    // Tests
    NS_ASSERT(m_ueSetupList.size() == ueDevs.GetN());
    NetDeviceContainer::Iterator itDev;
    for (itSetup = m_ueSetupList.begin(), itDev = ueDevs.Begin();
         itSetup != m_ueSetupList.end() || itDev != ueDevs.End();
         itSetup++, itDev++)
    {
        Ptr<LteUeNetDevice> ueDev = (*itDev)->GetObject<LteUeNetDevice>();
    }

    if (m_isEpcMode)
    {
        // Create P-GW node
        Ptr<Node> pgw = epcHelper->GetPgwNode();

        // Create a single RemoteHost
        NodeContainer remoteHostContainer;
        remoteHostContainer.Create(1);
        Ptr<Node> remoteHost = remoteHostContainer.Get(0);
        InternetStackHelper internet;
        internet.Install(remoteHostContainer);

        // Create the Internet
        PointToPointHelper p2ph;
        p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
        p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
        p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
        NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
        Ipv4AddressHelper ipv4h;
        ipv4h.SetBase("1.0.0.0", "255.0.0.0");
        Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

        // Routing of the Internet Host (towards the LTE network)
        Ipv4StaticRoutingHelper ipv4RoutingHelper;
        Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
        remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"),
                                                   Ipv4Mask("255.0.0.0"),
                                                   1);

        // Install the IP stack on the UEs
        internet.Install(ueNodes);
        Ipv4InterfaceContainer ueIpIfaces;
        ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

        // Assign IP address to UEs
        for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
        {
            Ptr<Node> ueNode = ueNodes.Get(u);
            // Set the default gateway for the UE
            Ptr<Ipv4StaticRouting> ueStaticRouting =
                ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
            ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
        }
    }
    else
    {
        NS_FATAL_ERROR("No support yet for LTE-only simulations");
    }

    // Connect to trace sources in UEs
    Config::Connect("/NodeList/*/DeviceList/*/LteUeRrc/StateTransition",
                    MakeCallback(&LteCellSelectionTestCase::StateTransitionCallback, this));
    Config::Connect(
        "/NodeList/*/DeviceList/*/LteUeRrc/InitialCellSelectionEndOk",
        MakeCallback(&LteCellSelectionTestCase::InitialCellSelectionEndOkCallback, this));
    Config::Connect(
        "/NodeList/*/DeviceList/*/LteUeRrc/InitialCellSelectionEndError",
        MakeCallback(&LteCellSelectionTestCase::InitialCellSelectionEndErrorCallback, this));
    Config::Connect("/NodeList/*/DeviceList/*/LteUeRrc/ConnectionEstablished",
                    MakeCallback(&LteCellSelectionTestCase::ConnectionEstablishedCallback, this));

    // Enable Idle mode cell selection
    lteHelper->Attach(ueDevs);

    // Run simulation
    Simulator::Stop(lastCheckPoint);
    Simulator::Run();

    NS_LOG_INFO("Simulation ends");
    Simulator::Destroy();

    // Restore the seed and run number that were in effect before this test
    Config::SetGlobal("RngSeed", UintegerValue(previousSeed));
    Config::SetGlobal("RngRun", UintegerValue(previousRun));
}

void
LteCellSelectionTestCase::CheckPoint(Ptr<LteUeNetDevice> ueDev,
                                     uint16_t expectedCellId1,
                                     uint16_t expectedCellId2)
{
    uint16_t actualCellId = ueDev->GetRrc()->GetCellId();

    if (expectedCellId2 == 0)
    {
        NS_TEST_ASSERT_MSG_EQ(actualCellId,
                              expectedCellId1,
                              "IMSI " << ueDev->GetImsi() << " has attached to an unexpected cell");
    }
    else
    {
        bool pass = (actualCellId == expectedCellId1) || (actualCellId == expectedCellId2);
        NS_TEST_ASSERT_MSG_EQ(pass,
                              true,
                              "IMSI " << ueDev->GetImsi() << " has attached to an unexpected cell"
                                      << " (actual: " << actualCellId << ","
                                      << " expected: " << expectedCellId1 << " or "
                                      << expectedCellId2 << ")");
    }

    if (expectedCellId1 > 0)
    {
        NS_TEST_ASSERT_MSG_EQ(m_lastState.at(static_cast<unsigned int>(ueDev->GetImsi() - 1)),
                              LteUeRrc::CONNECTED_NORMALLY,
                              "UE " << ueDev->GetImsi() << " is not at CONNECTED_NORMALLY state");
    }
}

void
LteCellSelectionTestCase::StateTransitionCallback(std::string context,
                                                  uint64_t imsi,
                                                  uint16_t cellId,
                                                  uint16_t rnti,
                                                  LteUeRrc::State oldState,
                                                  LteUeRrc::State newState)
{
    NS_LOG_FUNCTION(this << imsi << cellId << rnti << oldState << newState);
    m_lastState.at(static_cast<unsigned int>(imsi - 1)) = newState;
}

void
LteCellSelectionTestCase::InitialCellSelectionEndOkCallback(std::string context,
                                                            uint64_t imsi,
                                                            uint16_t cellId)
{
    NS_LOG_FUNCTION(this << imsi << cellId);
}

void
LteCellSelectionTestCase::InitialCellSelectionEndErrorCallback(std::string context,
                                                               uint64_t imsi,
                                                               uint16_t cellId)
{
    NS_LOG_FUNCTION(this << imsi << cellId);
}

void
LteCellSelectionTestCase::ConnectionEstablishedCallback(std::string context,
                                                        uint64_t imsi,
                                                        uint16_t cellId,
                                                        uint16_t rnti)
{
    NS_LOG_FUNCTION(this << imsi << cellId << rnti);
}
