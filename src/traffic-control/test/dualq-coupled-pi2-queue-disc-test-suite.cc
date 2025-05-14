/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 NITK Surathkal
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
 * Author: Shravya K.S. <shravya.ks0@gmail.com>
 *
 */
#include "ns3/test.h"
#include "ns3/dualq-coupled-pi2-queue-disc.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

using namespace ns3;

class DualQueueL4SQueueDiscTestItem : public QueueDiscItem
{
public:
  DualQueueL4SQueueDiscTestItem (Ptr<Packet> p, const Address & addr, uint16_t protocol);
  virtual ~DualQueueL4SQueueDiscTestItem ();
  virtual void AddHeader (void);
  virtual bool Mark (void);
  virtual bool IsL4S (void);

private:
  DualQueueL4SQueueDiscTestItem ();
  DualQueueL4SQueueDiscTestItem (const DualQueueL4SQueueDiscTestItem &);
  DualQueueL4SQueueDiscTestItem &operator = (const DualQueueL4SQueueDiscTestItem &);
};

DualQueueL4SQueueDiscTestItem::DualQueueL4SQueueDiscTestItem (Ptr<Packet> p, const Address & addr, uint16_t protocol)
  : QueueDiscItem (p, addr, protocol)
{
}

DualQueueL4SQueueDiscTestItem::~DualQueueL4SQueueDiscTestItem ()
{
}

void
DualQueueL4SQueueDiscTestItem::AddHeader (void)
{
}

bool
DualQueueL4SQueueDiscTestItem::Mark (void)
{
  return true;
}

bool
DualQueueL4SQueueDiscTestItem::IsL4S (void)
{
  return true;
}

class DualQueueClassicQueueDiscTestItem : public QueueDiscItem
{
public:
  DualQueueClassicQueueDiscTestItem (Ptr<Packet> p, const Address & addr, uint16_t protocol);
  virtual ~DualQueueClassicQueueDiscTestItem ();
  virtual void AddHeader (void);
  virtual bool Mark (void);
  virtual bool IsL4S (void);

private:
  DualQueueClassicQueueDiscTestItem ();
  DualQueueClassicQueueDiscTestItem (const DualQueueClassicQueueDiscTestItem &);
  DualQueueClassicQueueDiscTestItem &operator = (const DualQueueClassicQueueDiscTestItem &);
};

DualQueueClassicQueueDiscTestItem::DualQueueClassicQueueDiscTestItem (Ptr<Packet> p, const Address & addr, uint16_t protocol)
  : QueueDiscItem (p, addr, protocol)
{
}

DualQueueClassicQueueDiscTestItem::~DualQueueClassicQueueDiscTestItem ()
{
}

void
DualQueueClassicQueueDiscTestItem::AddHeader (void)
{
}

bool
DualQueueClassicQueueDiscTestItem::Mark (void)
{
  return true;
}

bool
DualQueueClassicQueueDiscTestItem::IsL4S (void)
{
  return false;
}

class DualQCoupledPi2QueueDiscTestCase : public TestCase
{
public:
  DualQCoupledPi2QueueDiscTestCase ();
  virtual void DoRun (void);

private:
  void Enqueue (Ptr<DualQCoupledPi2QueueDisc> queue, uint32_t size, uint32_t nPkt, StringValue trafficType);
  void EnqueueWithDelay (Ptr<DualQCoupledPi2QueueDisc> queue, uint32_t size, uint32_t nPkt, StringValue trafficType);
  void Dequeue (Ptr<DualQCoupledPi2QueueDisc> queue, uint32_t nPkt);
  void DequeueWithDelay (Ptr<DualQCoupledPi2QueueDisc> queue, double delay, uint32_t nPkt);
  void RunPiSquareTest (void);
};

DualQCoupledPi2QueueDiscTestCase::DualQCoupledPi2QueueDiscTestCase ()
  : TestCase ("Sanity check on the DualQ Coupled PI Square queue disc implementation")
{
}

void
DualQCoupledPi2QueueDiscTestCase::RunPiSquareTest (void)
{
  uint32_t pktSize = 1000;
  uint32_t qSize = 50;
  Ptr<DualQCoupledPi2QueueDisc> queue = CreateObject<DualQCoupledPi2QueueDisc> ();
  queue->SetAttribute ("Mtu", UintegerValue (1500));

  // test 1: simple enqueue/dequeue with defaults, no drops
  Address dest;
  // pktSize should be same as MeanPktSize to avoid performance gap between byte and packet mode
  qSize = qSize * pktSize;
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("QueueLimit", UintegerValue (qSize)), true,
                         "Verify that we can actually set the attribute QueueLimit");

  Ptr<Packet> p1, p2, p3, p4, p5, p6, p7, p8;
  p1 = Create<Packet> (pktSize);
  p2 = Create<Packet> (pktSize);
  p3 = Create<Packet> (pktSize);
  p4 = Create<Packet> (pktSize);
  p5 = Create<Packet> (pktSize);
  p6 = Create<Packet> (pktSize);
  p7 = Create<Packet> (pktSize);
  p8 = Create<Packet> (pktSize);

  queue->Initialize ();

  NS_TEST_EXPECT_MSG_EQ (queue->GetQueueSize (), 0 * pktSize, "There should be no packets in there");
  queue->Enqueue (Create<DualQueueClassicQueueDiscTestItem> (p1, dest, 0));
  NS_TEST_EXPECT_MSG_EQ (queue->GetQueueSize (), 1 * pktSize, "There should be one packet in there");
  queue->Enqueue (Create<DualQueueClassicQueueDiscTestItem> (p2, dest, 0));
  NS_TEST_EXPECT_MSG_EQ (queue->GetQueueSize (), 2 * pktSize, "There should be two packets in there");
  queue->Enqueue (Create<DualQueueClassicQueueDiscTestItem> (p3, dest, 0));
  queue->Enqueue (Create<DualQueueClassicQueueDiscTestItem> (p4, dest, 0));
  queue->Enqueue (Create<DualQueueL4SQueueDiscTestItem> (p5, dest, 0));
  queue->Enqueue (Create<DualQueueL4SQueueDiscTestItem> (p6, dest, 0));
  queue->Enqueue (Create<DualQueueL4SQueueDiscTestItem> (p7, dest, 0));
  queue->Enqueue (Create<DualQueueL4SQueueDiscTestItem> (p8, dest, 0));
  NS_TEST_EXPECT_MSG_EQ (queue->GetQueueSize (), 8 * pktSize, "There should be eight packets in there");

  Ptr<QueueDiscItem> item;

  item = queue->Dequeue ();
  NS_TEST_EXPECT_MSG_EQ (static_cast<bool>(item), true, "I want to remove the first packet");
  NS_TEST_EXPECT_MSG_EQ (queue->GetQueueSize (), 7 * pktSize, "There should be seven packets in there");
  item = queue->Dequeue ();
  NS_TEST_EXPECT_MSG_EQ (static_cast<bool>(item), true, "I want to remove the second packet");
  NS_TEST_EXPECT_MSG_EQ (queue->GetQueueSize (), 6 * pktSize, "There should be six packet in there");
  item = queue->Dequeue ();
  NS_TEST_EXPECT_MSG_EQ (static_cast<bool>(item), true, "I want to remove the third packet");
  NS_TEST_EXPECT_MSG_EQ (queue->GetQueueSize (), 5 * pktSize, "There should be five packets in there");
  item = queue->Dequeue ();
  item = queue->Dequeue ();
  item = queue->Dequeue ();
  item = queue->Dequeue ();
  item = queue->Dequeue ();
  item = queue->Dequeue ();
  NS_TEST_EXPECT_MSG_EQ (static_cast<bool>(item), false, "There are really no packets in there");

  // test 2: more data with defaults, unforced drops but no forced drops
  queue = CreateObject<DualQCoupledPi2QueueDisc> ();
  queue->SetAttribute ("Mtu", UintegerValue (1500));
  pktSize = 1000;  // pktSize != 0 because DequeueThreshold always works in bytes
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("QueueLimit", UintegerValue (qSize)), true,
                         "Verify that we can actually set the attribute QueueLimit");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("A", DoubleValue (10)), true,
                         "Verify that we can actually set the attribute A");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("B", DoubleValue (100)), true,
                         "Verify that we can actually set the attribute B");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("Tupdate", TimeValue (Seconds (0.016))), true,
                         "Verify that we can actually set the attribute Tupdate");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("L4SMarkThresold", TimeValue (Seconds (0.001))), true,
                         "Verify that we can actually set the attribute L4SMarkThresold");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("K", DoubleValue (2)), true,
                         "Verify that we can actually set the attribute K");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("Target", TimeValue (Seconds (0.15))), true,
                         "Verify that we can actually set the attribute QueueDelayReference");

  queue->Initialize ();

  EnqueueWithDelay (queue, pktSize, 200, StringValue ("L4S"));
  EnqueueWithDelay (queue, pktSize, 200, StringValue ("Classic"));

  DequeueWithDelay (queue, 0.012, 400);

  Simulator::Stop (Seconds (8.0));
  Simulator::Run ();

  uint32_t test2ClassicMark = queue->GetStats ().GetNMarkedPackets (DualQCoupledPi2QueueDisc::UNFORCED_CLASSIC_MARK);
  uint32_t test2L4SMark = queue->GetStats ().GetNMarkedPackets (DualQCoupledPi2QueueDisc::UNFORCED_L4S_MARK);
  NS_TEST_EXPECT_MSG_NE (test2ClassicMark, 0, "There should some unforced classic marks");
  NS_TEST_EXPECT_MSG_NE (test2L4SMark, 0, "There should some unforced l4s marks");
  NS_TEST_EXPECT_MSG_GT (test2L4SMark, test2ClassicMark, "Packets of L4S traffic should have more unforced marks than packets of Classic traffic");

  QueueDisc::Stats st = queue->GetStats ();
  NS_TEST_EXPECT_MSG_NE (st.GetNDroppedPackets (DualQCoupledPi2QueueDisc::FORCED_DROP), 0, "There should be some forced drops");

  // test 3: Test by sending L4S traffic only
  queue = CreateObject<DualQCoupledPi2QueueDisc> ();
  queue->SetAttribute ("Mtu", UintegerValue (1500));
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("QueueLimit", UintegerValue (qSize)), true,
                         "Verify that we can actually set the attribute QueueLimit");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("A", DoubleValue (10)), true,
                         "Verify that we can actually set the attribute A");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("B", DoubleValue (100)), true,
                         "Verify that we can actually set the attribute B");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("Tupdate", TimeValue (Seconds (0.016))), true,
                         "Verify that we can actually set the attribute Tupdate");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("L4SMarkThresold", TimeValue (Seconds (0.001))), true,
                         "Verify that we can actually set the attribute L4SMarkThresold");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("K", DoubleValue (2)), true,
                         "Verify that we can actually set the attribute K");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("Target", TimeValue (Seconds (0.15))), true,
                         "Verify that we can actually set the attribute QueueDelayReference");

  queue->Initialize ();

  EnqueueWithDelay (queue, pktSize, 400, StringValue ("L4S"));

  DequeueWithDelay (queue, 0.012, 400);

  Simulator::Stop (Seconds (8.0));
  Simulator::Run ();

  st = queue->GetStats ();
  uint32_t test3ClassicDrop = queue->GetStats ().GetNMarkedPackets (DualQCoupledPi2QueueDisc::UNFORCED_CLASSIC_DROP);
  NS_TEST_EXPECT_MSG_EQ (test3ClassicDrop, 0, "There should be zero unforced classic drops since only L4S traffic is pumped ");
  uint32_t test3ClassicMark = queue->GetStats ().GetNMarkedPackets (DualQCoupledPi2QueueDisc::UNFORCED_CLASSIC_MARK);
  NS_TEST_EXPECT_MSG_EQ (test3ClassicMark, 0, "There should be zero unforced classic marks since only L4S traffic is pumped");
  uint32_t test3L4SMark = queue->GetStats ().GetNMarkedPackets (DualQCoupledPi2QueueDisc::UNFORCED_L4S_MARK);
  NS_TEST_EXPECT_MSG_NE (test3L4SMark, 0, "There should be some L4S marks");

  // test 4: Test by sending Classic traffic only
  queue = CreateObject<DualQCoupledPi2QueueDisc> ();
  queue->SetAttribute ("Mtu", UintegerValue (1500));
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("QueueLimit", UintegerValue (qSize)), true,
                         "Verify that we can actually set the attribute QueueLimit");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("A", DoubleValue (10)), true,
                         "Verify that we can actually set the attribute A");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("B", DoubleValue (100)), true,
                         "Verify that we can actually set the attribute B");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("Tupdate", TimeValue (Seconds (0.016))), true,
                         "Verify that we can actually set the attribute Tupdate");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("L4SMarkThresold", TimeValue (Seconds (0.001))), true,
                         "Verify that we can actually set the attribute L4SMarkThresold");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("K", DoubleValue (2)), true,
                         "Verify that we can actually set the attribute K");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("Target", TimeValue (Seconds (0.15))), true,
                         "Verify that we can actually set the attribute QueueDelayReference");

  queue->Initialize ();

  EnqueueWithDelay (queue, pktSize, 400, StringValue ("Classic"));

  DequeueWithDelay (queue, 0.012, 400);

  Simulator::Stop (Seconds (8.0));
  Simulator::Run ();

  uint32_t test4ClassicDrop = queue->GetStats ().GetNMarkedPackets (DualQCoupledPi2QueueDisc::UNFORCED_CLASSIC_DROP);
  NS_TEST_EXPECT_MSG_EQ (test4ClassicDrop, 0, "There should be zero unforced classic drops since packets are ECN capable ");
  uint32_t test4ClassicMark = queue->GetStats ().GetNMarkedPackets (DualQCoupledPi2QueueDisc::UNFORCED_CLASSIC_MARK);
  NS_TEST_EXPECT_MSG_NE (test4ClassicMark, 0, "There should be some unforced classic marks");
  uint32_t test4L4SMark = queue->GetStats ().GetNMarkedPackets (DualQCoupledPi2QueueDisc::UNFORCED_L4S_MARK);
  NS_TEST_EXPECT_MSG_EQ (test4L4SMark, 0, "There should be zero L4S marks since only Classic traffic is pumped");
}

void
DualQCoupledPi2QueueDiscTestCase::Enqueue (Ptr<DualQCoupledPi2QueueDisc> queue, uint32_t size, uint32_t nPkt, StringValue trafficType)
{
  Address dest;
  for (uint32_t i = 0; i < nPkt; i++)
    {
      if (trafficType.Get () == "L4S")
        {
          queue->Enqueue (Create<DualQueueL4SQueueDiscTestItem> (Create<Packet> (size), dest, 0));
        }
      else if (trafficType.Get () == "Classic")
        {
          queue->Enqueue (Create<DualQueueClassicQueueDiscTestItem> (Create<Packet> (size), dest, 0));
        }
    }
}

void
DualQCoupledPi2QueueDiscTestCase::EnqueueWithDelay (Ptr<DualQCoupledPi2QueueDisc> queue, uint32_t size, uint32_t nPkt, StringValue trafficType)
{
  Address dest;
  double delay = 0.01;  // enqueue packets with delay
  for (uint32_t i = 0; i < nPkt; i++)
    {
      Simulator::Schedule (Time (Seconds (i * delay)), &DualQCoupledPi2QueueDiscTestCase::Enqueue, this, queue, size, 1, trafficType);
    }
}

void
DualQCoupledPi2QueueDiscTestCase::Dequeue (Ptr<DualQCoupledPi2QueueDisc> queue, uint32_t nPkt)
{
  for (uint32_t i = 0; i < nPkt; i++)
    {
      Ptr<QueueDiscItem> item = queue->Dequeue ();
    }
}

void
DualQCoupledPi2QueueDiscTestCase::DequeueWithDelay (Ptr<DualQCoupledPi2QueueDisc> queue, double delay, uint32_t nPkt)
{
  for (uint32_t i = 0; i < nPkt; i++)
    {
      Simulator::Schedule (Time (Seconds ((i + 1) * delay)), &DualQCoupledPi2QueueDiscTestCase::Dequeue, this, queue, 1);
    }
}

void
DualQCoupledPi2QueueDiscTestCase::DoRun (void)
{
  RunPiSquareTest ();
  Simulator::Destroy ();
}

static class DualQCoupledPi2QueueDiscTestSuite : public TestSuite
{
public:
  DualQCoupledPi2QueueDiscTestSuite ()
    : TestSuite ("dualq-coupled-pi2-queue-disc", UNIT)
  {
    AddTestCase (new DualQCoupledPi2QueueDiscTestCase (), TestCase::QUICK);
  }
} g_DualQCoupledPi2QueueTestSuite; 