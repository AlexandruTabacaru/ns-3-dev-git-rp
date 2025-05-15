/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 Tsinghua University
 * Copyright (c) 2018 NITK Surathkal
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
 * Authors: Wenying Dai <dwy927@gmail.com>
 *          Mohit P. Tahiliani <tahiliani.nitk@gmail.com>
 */

#include "ns3/test.h"
#include "ns3/socket-factory.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/simulator.h"
#include "ns3/simple-channel.h"
#include "ns3/simple-net-device.h"
#include "ns3/simple-net-device-helper.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/tcp-option-accecn.h"
#include "ns3/tcp-option.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/inet-socket-address.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpAccEcnTestSuite");

/**
 * \ingroup internet-test
 * \ingroup tests
 *
 * \brief TCP AccECN Test
 */
class TcpAccEcnTest : public TestCase
{
public:
  TcpAccEcnTest (void);
  void DoRun (void);

  void ReceivePacket (Ptr<Socket> socket);
  void AcceptConnection (Ptr<Socket> socket, const Address& from);
  void StartFlow (Ptr<Socket> socket, const Address& addr);
  void QueueDrop (SocketWho who);
  void SendAckNow (Ptr<Socket> socket);
  Ptr<TcpSocketBase> CreateSenderSocket (Ptr<Node> senderNode);
  Ptr<TcpSocketBase> CreateReceiverSocket (Ptr<Node> receiverNode);
  void Tx (const Ptr<const Packet> p, const TcpHeader&h, Ptr<TcpSocketBase> sock);
  void Rx (const Ptr<const Packet> p, const TcpHeader&h, Ptr<TcpSocketBase> sock);

private:
  Ptr<Node> m_receiverNode;
  Ptr<Node> m_senderNode;
  Ptr<TcpSocketBase> m_sender;
  Ptr<TcpSocketBase> m_receiver;

  uint32_t m_totalTx;
  uint32_t m_accEcnE0bS;
  uint32_t m_accEcnE1bS;
  uint32_t m_accEcnCebS;
  uint32_t m_accEcnCepS;
  uint32_t m_accEcnE0bR;
  uint32_t m_accEcnE1bR;
  uint32_t m_accEcnCebR;
  uint32_t m_accEcnCepR;
};

TcpAccEcnTest::TcpAccEcnTest (void)
  : TestCase ("TCP AccECN test"),
    m_totalTx (0),
    m_accEcnE0bS (0),
    m_accEcnE1bS (0),
    m_accEcnCebS (0),
    m_accEcnCepS (0),
    m_accEcnE0bR (0),
    m_accEcnE1bR (0),
    m_accEcnCebR (0),
    m_accEcnCepR (0)
{
}

void
TcpAccEcnTest::ReceivePacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;

  while ((packet = socket->RecvFrom (from)))
    {
      if (packet->GetSize () > 0)
        {
          // do nothing
        }
    }
}

void
TcpAccEcnTest::AcceptConnection (Ptr<Socket> socket, const Address &from)
{
  socket->SetRecvCallback (MakeCallback (&TcpAccEcnTest::ReceivePacket, this));
}

void
TcpAccEcnTest::StartFlow (Ptr<Socket> socket, const Address &addr)
{
  NS_TEST_ASSERT_MSG_EQ (socket->Connect (addr), 0, "Connection failed");
  socket->SetSendCallback (MakeNullCallback<void, Ptr<Socket>, uint32_t> ());
  socket->SetRecvCallback (MakeCallback (&TcpAccEcnTest::ReceivePacket, this));
}

Ptr<TcpSocketBase>
TcpAccEcnTest::CreateSenderSocket (Ptr<Node> senderNode)
{
  Ptr<TcpSocketBase> socket = DynamicCast<TcpSocketBase> (Socket::CreateSocket (senderNode, TcpSocketFactory::GetTypeId ()));
  return socket;
}

Ptr<TcpSocketBase>
TcpAccEcnTest::CreateReceiverSocket (Ptr<Node> receiverNode)
{
  Ptr<TcpSocketBase> socket = DynamicCast<TcpSocketBase> (Socket::CreateSocket (receiverNode, TcpSocketFactory::GetTypeId ()));
  socket->SetAttribute ("SegmentSize", UintegerValue (1000));
  socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), 4477));
  socket->Listen ();
  socket->SetAcceptCallback (MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
                            MakeCallback (&TcpAccEcnTest::AcceptConnection, this));
  return socket;
}

void
TcpAccEcnTest::Tx (const Ptr<const Packet> p, const TcpHeader &h, Ptr<TcpSocketBase> sock)
{
  m_totalTx++;
}

void
TcpAccEcnTest::Rx (const Ptr<const Packet> p, const TcpHeader &h, Ptr<TcpSocketBase> sock)
{
  if (sock == m_sender)
    {
      // Check if AccECN Option is present
      if (h.HasExperimentalOption (TcpOptionExperimental::ACCECN))
        {
          Ptr<const TcpOptionAccEcn> option = DynamicCast<const TcpOptionAccEcn> (h.GetExperimentalOption (TcpOptionExperimental::ACCECN));
          NS_TEST_ASSERT_MSG_NE (option, 0, "Option not found");
        }
    }
}

void
TcpAccEcnTest::DoRun ()
{
  Ptr<SimpleNetDevice> txDev;
  Ptr<SimpleNetDevice> rxDev;
  Ptr<SimpleChannel> channel = CreateObject<SimpleChannel> ();

  m_receiverNode = CreateObject<Node> ();
  m_senderNode = CreateObject<Node> ();

  // Install internet stack
  InternetStackHelper internet;
  internet.Install (m_receiverNode);
  internet.Install (m_senderNode);

  // Install devices
  SimpleNetDeviceHelper helperChannel;
  helperChannel.SetNetDevicePointToPointMode (true);
  NetDeviceContainer devices = helperChannel.Install (NodeContainer (m_senderNode, m_receiverNode));
  devices.Get (0)->SetMtu (1500);
  devices.Get (1)->SetMtu (1500);

  // Assign addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ipv4Ints = ipv4.Assign (devices);

  // Setup ECN with AccECN
  m_sender = CreateSenderSocket (m_senderNode);
  m_sender->SetAttribute ("SegmentSize", UintegerValue (1000));
  m_sender->SetAttribute ("Sack", BooleanValue (true));
  m_sender->SetEcn (TcpSocketBase::AccEcn);
  
  m_receiver = CreateReceiverSocket (m_receiverNode);
  m_receiver->SetAttribute ("SegmentSize", UintegerValue (1000));
  m_receiver->SetAttribute ("Sack", BooleanValue (true));
  m_receiver->SetEcn (TcpSocketBase::AccEcn);

  // Setup callbacks
  m_sender->TraceConnectWithoutContext ("Tx", MakeCallback (&TcpAccEcnTest::Tx, this));
  m_sender->TraceConnectWithoutContext ("Rx", MakeCallback (&TcpAccEcnTest::Rx, this));
  m_receiver->TraceConnectWithoutContext ("Tx", MakeCallback (&TcpAccEcnTest::Tx, this));
  m_receiver->TraceConnectWithoutContext ("Rx", MakeCallback (&TcpAccEcnTest::Rx, this));

  // Connect tracing events for AccECN counters
  m_sender->TraceConnectWithoutContext ("AccEcnE0bS", MakeCallback (&TcpAccEcnTest::AccEcnE0bSCallback, this));
  m_sender->TraceConnectWithoutContext ("AccEcnE1bS", MakeCallback (&TcpAccEcnTest::AccEcnE1bSCallback, this));
  m_sender->TraceConnectWithoutContext ("AccEcnCebS", MakeCallback (&TcpAccEcnTest::AccEcnCebSCallback, this));
  m_sender->TraceConnectWithoutContext ("AccEcnCepS", MakeCallback (&TcpAccEcnTest::AccEcnCepSCallback, this));
  m_receiver->TraceConnectWithoutContext ("AccEcnE0bR", MakeCallback (&TcpAccEcnTest::AccEcnE0bRCallback, this));
  m_receiver->TraceConnectWithoutContext ("AccEcnE1bR", MakeCallback (&TcpAccEcnTest::AccEcnE1bRCallback, this));
  m_receiver->TraceConnectWithoutContext ("AccEcnCebR", MakeCallback (&TcpAccEcnTest::AccEcnCebRCallback, this));
  m_receiver->TraceConnectWithoutContext ("AccEcnCepR", MakeCallback (&TcpAccEcnTest::AccEcnCepRCallback, this));

  Simulator::Schedule (Seconds (0.0), &TcpAccEcnTest::StartFlow, this, m_sender, InetSocketAddress (ipv4Ints.GetAddress (1), 4477));

  // Send some data
  Simulator::Schedule (Seconds (1.0), &Socket::Send, m_sender, Create<Packet> (1000), 0);
  Simulator::Schedule (Seconds (1.1), &Socket::Send, m_sender, Create<Packet> (1000), 0);
  Simulator::Schedule (Seconds (1.2), &Socket::Send, m_sender, Create<Packet> (1000), 0);

  Simulator::Run ();
  Simulator::Destroy ();

  // Make sure we received data
  NS_TEST_ASSERT_MSG_GT (m_totalTx, 0, "No data was transmitted");
}

// Callback implementations
void TcpAccEcnTest::AccEcnE0bSCallback (uint32_t oldValue, uint32_t newValue)
{
  m_accEcnE0bS = newValue;
}

void TcpAccEcnTest::AccEcnE1bSCallback (uint32_t oldValue, uint32_t newValue)
{
  m_accEcnE1bS = newValue;
}

void TcpAccEcnTest::AccEcnCebSCallback (uint32_t oldValue, uint32_t newValue)
{
  m_accEcnCebS = newValue;
}

void TcpAccEcnTest::AccEcnCepSCallback (uint32_t oldValue, uint32_t newValue)
{
  m_accEcnCepS = newValue;
}

void TcpAccEcnTest::AccEcnE0bRCallback (uint32_t oldValue, uint32_t newValue)
{
  m_accEcnE0bR = newValue;
}

void TcpAccEcnTest::AccEcnE1bRCallback (uint32_t oldValue, uint32_t newValue)
{
  m_accEcnE1bR = newValue;
}

void TcpAccEcnTest::AccEcnCebRCallback (uint32_t oldValue, uint32_t newValue)
{
  m_accEcnCebR = newValue;
}

void TcpAccEcnTest::AccEcnCepRCallback (uint32_t oldValue, uint32_t newValue)
{
  m_accEcnCepR = newValue;
}

/**
 * \ingroup internet-test
 * \ingroup tests
 *
 * \brief TCP AccECN TestSuite
 */
class TcpAccEcnTestSuite : public TestSuite
{
public:
  TcpAccEcnTestSuite ()
    : TestSuite ("tcp-accecn-test", UNIT)
  {
    AddTestCase (new TcpAccEcnTest, TestCase::QUICK);
  }
};

static TcpAccEcnTestSuite g_tcpAccEcnTestSuite; 