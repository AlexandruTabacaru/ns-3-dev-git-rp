/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Author: Faker Moatamri <faker.moatamri@sophia.inria.fr>
 *
 */
/**
 * This is the test code for ipv4-l3-protocol.cc
 */

#include "ns3/arp-l3-protocol.h"
#include "ns3/boolean.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/simple-net-device.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

using namespace ns3;

/**
 * @ingroup internet-test
 *
 * @brief IPv4 Test
 */
class Ipv4L3ProtocolTestCase : public TestCase
{
  public:
    Ipv4L3ProtocolTestCase();
    ~Ipv4L3ProtocolTestCase() override;
    void DoRun() override;
};

Ipv4L3ProtocolTestCase::Ipv4L3ProtocolTestCase()
    : TestCase("Verify the IPv4 layer 3 protocol")
{
}

Ipv4L3ProtocolTestCase::~Ipv4L3ProtocolTestCase()
{
}

void
Ipv4L3ProtocolTestCase::DoRun()
{
    Ptr<Node> node = CreateObject<Node>();
    Ptr<Ipv4L3Protocol> ipv4 = CreateObject<Ipv4L3Protocol>();
    Ptr<Ipv4Interface> interface = CreateObject<Ipv4Interface>();
    Ptr<SimpleNetDevice> device = CreateObject<SimpleNetDevice>();

    // The following allows the interface to run without ARP
    device->SetAttribute("PointToPointMode", BooleanValue(true));

    node->AddDevice(device);
    node->AggregateObject(ipv4);
    interface->SetDevice(device);
    interface->SetNode(node);

    // Interface 0 is the Loopback
    uint32_t index = ipv4->AddIpv4Interface(interface);
    NS_TEST_ASSERT_MSG_EQ(index, 1, "The index is not 1??");
    interface->SetUp();
    Ipv4InterfaceAddress ifaceAddr1 = Ipv4InterfaceAddress("192.168.0.1", "255.255.255.0");
    interface->AddAddress(ifaceAddr1);
    Ipv4InterfaceAddress ifaceAddr2 = Ipv4InterfaceAddress("192.168.0.2", "255.255.255.0");
    interface->AddAddress(ifaceAddr2);
    Ipv4InterfaceAddress ifaceAddr3 = Ipv4InterfaceAddress("10.30.0.1", "255.255.255.0");
    interface->AddAddress(ifaceAddr3);
    Ipv4InterfaceAddress ifaceAddr4 = Ipv4InterfaceAddress("250.0.0.1", "255.255.255.0");
    interface->AddAddress(ifaceAddr4);
    uint32_t num = interface->GetNAddresses();
    NS_TEST_ASSERT_MSG_EQ(num, 4, "Should find 4 interfaces??");
    interface->RemoveAddress(2);
    num = interface->GetNAddresses();
    NS_TEST_ASSERT_MSG_EQ(num, 3, "Should find 3 interfaces??");
    Ipv4InterfaceAddress output = interface->GetAddress(2);
    NS_TEST_ASSERT_MSG_EQ(ifaceAddr4, output, "The addresses should be identical");

    /* Test Ipv4Interface()::RemoveAddress(address) */
    output = interface->RemoveAddress(Ipv4Address("250.0.0.1"));
    NS_TEST_ASSERT_MSG_EQ(ifaceAddr4, output, "Wrong Interface Address Removed??");
    num = interface->GetNAddresses();
    NS_TEST_ASSERT_MSG_EQ(num, 2, "Should find 2 addresses??");

    /* Remove a non-existent Address */
    output = interface->RemoveAddress(Ipv4Address("253.123.9.81"));
    NS_TEST_ASSERT_MSG_EQ(Ipv4InterfaceAddress(), output, "Removed non-existent address??");
    num = interface->GetNAddresses();
    NS_TEST_ASSERT_MSG_EQ(num, 2, "Should find 2 addresses??");

    /* Remove a Loopback Address */
    output = interface->RemoveAddress(Ipv4Address::GetLoopback());
    NS_TEST_ASSERT_MSG_EQ(Ipv4InterfaceAddress(), output, "Able to remove loopback address??");
    num = interface->GetNAddresses();
    NS_TEST_ASSERT_MSG_EQ(num, 2, "Should find 2 addresses??");

    /* Test Ipv4Address::RemoveAddress(i, address) */
    bool result = ipv4->RemoveAddress(index, Ipv4Address("192.168.0.2"));
    NS_TEST_ASSERT_MSG_EQ(true, result, "Unable to remove Address??");
    num = interface->GetNAddresses();
    NS_TEST_ASSERT_MSG_EQ(num, 1, "Should find 1 addresses??");

    /* Remove a non-existent Address */
    result = ipv4->RemoveAddress(index, Ipv4Address("189.0.0.1"));
    NS_TEST_ASSERT_MSG_EQ(false, result, "Removed non-existent address??");
    num = interface->GetNAddresses();
    NS_TEST_ASSERT_MSG_EQ(num, 1, "Should find 1 addresses??");

    /* Remove a loopback Address */
    result = ipv4->RemoveAddress(index, Ipv4Address::GetLoopback());
    NS_TEST_ASSERT_MSG_EQ(false, result, "Able to remove loopback address??");
    num = interface->GetNAddresses();
    NS_TEST_ASSERT_MSG_EQ(num, 1, "Should find 1 addresses??");

    Simulator::Destroy();
}

/**
 * @ingroup internet-test
 *
 * @brief IPv4 TestSuite
 */
class IPv4L3ProtocolTestSuite : public TestSuite
{
  public:
    IPv4L3ProtocolTestSuite()
        : TestSuite("ipv4-protocol", Type::UNIT)
    {
        AddTestCase(new Ipv4L3ProtocolTestCase(), TestCase::Duration::QUICK);
    }
};

static IPv4L3ProtocolTestSuite g_ipv4protocolTestSuite; //!< Static variable for test initialization
