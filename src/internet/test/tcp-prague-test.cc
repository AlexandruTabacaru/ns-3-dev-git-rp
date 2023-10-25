/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2020 NITK Surathkal
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
 * Author: Deepak Kumaraswamy <deepakkavoor99@gmail.com>
 */

#include "ns3/test.h"
#include "ns3/log.h"
#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/tcp-prague.h"
#include "ns3/tcp-linux-reno.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpPragueTestSuite");

/**
 * \ingroup internet-test
 * \ingroup tests
 *
 * \brief ns3::TcpPrague should behave same as ns3::TcpLinuxReno during Slow Start
 */
class TcpPragueSlowStartTest : public TestCase
{
public:
  /**
   * \brief Constructor
   *
   * \param cWnd congestion window
   * \param segmentSize segment size
   * \param ssThresh slow start threshold
   * \param segmentsAcked segments acked
   * \param highTxMark high tx mark
   * \param lastAckedSeq last acked seq
   * \param rtt RTT
   * \param name Name of the test
   */
  TcpPragueSlowStartTest (uint32_t cWnd, uint32_t segmentSize, uint32_t ssThresh,
                          uint32_t segmentsAcked, SequenceNumber32 highTxMark,
                          SequenceNumber32 lastAckedSeq, Time rtt, const std::string &name);

private:
  virtual void DoRun (void);

  /**
   * \brief Execute the test.
   */
  void ExecuteTest (void);

  uint32_t m_cWnd;                        //!< cWnd
  uint32_t m_segmentSize;                 //!< segment size
  uint32_t m_segmentsAcked;               //!< segments acked
  uint32_t m_ssThresh;                    //!< ss thresh
  Time m_rtt;                             //!< rtt
  SequenceNumber32 m_highTxMark;          //!< high tx mark
  SequenceNumber32 m_lastAckedSeq;        //!< last acked seq
  Ptr<TcpSocketState> m_state;            //!< state
};

TcpPragueSlowStartTest::TcpPragueSlowStartTest (uint32_t cWnd, uint32_t segmentSize, uint32_t ssThresh,
                                                uint32_t segmentsAcked, SequenceNumber32 highTxMark,
                                                SequenceNumber32 lastAckedSeq, Time rtt, const std::string &name)
  : TestCase (name),
    m_cWnd (cWnd),
    m_segmentSize (segmentSize),
    m_segmentsAcked (segmentsAcked),
    m_ssThresh (ssThresh),
    m_rtt (rtt),
    m_highTxMark (highTxMark),
    m_lastAckedSeq (lastAckedSeq)
{}

void
TcpPragueSlowStartTest::DoRun ()
{
  Simulator::Schedule (Seconds (0.0), &TcpPragueSlowStartTest::ExecuteTest, this);
  Simulator::Run ();
  Simulator::Destroy ();
}

void
TcpPragueSlowStartTest::ExecuteTest ()
{
  m_state = CreateObject <TcpSocketState> ();
  m_state->m_cWnd = m_cWnd;
  m_state->m_ssThresh = m_ssThresh;
  m_state->m_segmentSize = m_segmentSize;
  m_state->m_highTxMark = m_highTxMark;
  m_state->m_lastAckedSeq = m_lastAckedSeq;

  Ptr<TcpSocketState> state = CreateObject <TcpSocketState> ();
  state->m_cWnd = m_cWnd;
  state->m_ssThresh = m_ssThresh;
  state->m_segmentSize = m_segmentSize;
  state->m_highTxMark = m_highTxMark;
  state->m_lastAckedSeq = m_lastAckedSeq;

  NS_TEST_ASSERT_MSG_LT (m_state->m_cWnd.Get (), m_state->m_ssThresh.Get (), "cWnd should be less than ssThresh in Slow Start test");

  Ptr<TcpPrague> congPrague = CreateObject <TcpPrague> ();
  congPrague->Init (m_state);
  congPrague->UpdateCwnd (m_state, m_segmentsAcked);

  Ptr<TcpLinuxReno> congLinuxReno = CreateObject <TcpLinuxReno> ();
  congLinuxReno->IncreaseWindow (state, m_segmentsAcked);

  NS_TEST_ASSERT_MSG_EQ (m_state->m_cWnd.Get (), state->m_cWnd.Get (),
                         "Prague cWnd has not updated according to LinuxReno in Slow Start");
}

/**
 * \ingroup internet-test
 * \ingroup tests
 *
 * \brief ns3::TcpPrague should behave same as ns3::TcpLinuxReno during Congestion Avoidance
 */
class TcpPragueCongestionAvoidanceTest : public TestCase
{
public:
  /**
   * \brief Constructor
   *
   * \param cWnd congestion window
   * \param segmentSize segment size
   * \param ssThresh slow start threshold
   * \param segmentsAcked segments acked
   * \param highTxMark high tx mark
   * \param lastAckedSeq last acked seq
   * \param rtt RTT
   * \param name Name of the test
   */
  TcpPragueCongestionAvoidanceTest (uint32_t cWnd, uint32_t segmentSize, uint32_t ssThresh,
                                    uint32_t segmentsAcked, SequenceNumber32 highTxMark,
                                    SequenceNumber32 lastAckedSeq, Time rtt, const std::string &name);

private:
  virtual void DoRun (void);

  /**
   * \brief Execute the test.
   */
  void ExecuteTest (void);

  uint32_t m_cWnd;                        //!< cWnd
  uint32_t m_segmentSize;                 //!< segment size
  uint32_t m_segmentsAcked;               //!< segments acked
  uint32_t m_ssThresh;                    //!< ss thresh
  Time m_rtt;                             //!< rtt
  SequenceNumber32 m_highTxMark;          //!< high tx mark
  SequenceNumber32 m_lastAckedSeq;        //!< last acked seq
  Ptr<TcpSocketState> m_state;            //!< state
};

TcpPragueCongestionAvoidanceTest::TcpPragueCongestionAvoidanceTest (uint32_t cWnd, uint32_t segmentSize, uint32_t ssThresh,
                                                                    uint32_t segmentsAcked, SequenceNumber32 highTxMark,
                                                                    SequenceNumber32 lastAckedSeq, Time rtt, const std::string &name)
  : TestCase (name),
    m_cWnd (cWnd),
    m_segmentSize (segmentSize),
    m_segmentsAcked (segmentsAcked),
    m_ssThresh (ssThresh),
    m_rtt (rtt),
    m_highTxMark (highTxMark),
    m_lastAckedSeq (lastAckedSeq)
{}

void
TcpPragueCongestionAvoidanceTest::DoRun ()
{
  Simulator::Schedule (Seconds (0.0), &TcpPragueCongestionAvoidanceTest::ExecuteTest, this);
  Simulator::Run ();
  Simulator::Destroy ();
}

void
TcpPragueCongestionAvoidanceTest::ExecuteTest ()
{
  m_state = CreateObject <TcpSocketState> ();
  m_state->m_cWnd = m_cWnd;
  m_state->m_ssThresh = m_ssThresh;
  m_state->m_segmentSize = m_segmentSize;
  m_state->m_highTxMark = m_highTxMark;
  m_state->m_lastAckedSeq = m_lastAckedSeq;

  Ptr<TcpSocketState> state = CreateObject <TcpSocketState> ();
  state->m_cWnd = m_cWnd;
  state->m_ssThresh = m_ssThresh;
  state->m_segmentSize = m_segmentSize;
  state->m_highTxMark = m_highTxMark;
  state->m_lastAckedSeq = m_lastAckedSeq;

  NS_TEST_ASSERT_MSG_LT (m_state->m_ssThresh.Get (), m_state->m_cWnd.Get (), "cWnd should be more than ssThresh in Congestion Avoidance test");

  Ptr<TcpPrague> congPrague = CreateObject <TcpPrague> ();
  congPrague->Init (m_state);
  Ptr<TcpLinuxReno> congLinuxReno = CreateObject <TcpLinuxReno> ();
  // ensure that cWnd is updated by 1 segment per RTT
  uint32_t totalSegsAcked = 0; // total segments ACKed in 1 RTT
  while (totalSegsAcked < m_cWnd / m_segmentSize)
    {
      congPrague->UpdateCwnd (m_state, m_segmentsAcked);
      congLinuxReno->IncreaseWindow (state, m_segmentsAcked);
      NS_TEST_ASSERT_MSG_EQ (m_state->m_cWnd.Get (), state->m_cWnd.Get (),
                             "Prague cWnd has not updated according to LinuxReno in Congestion Avoidance");
      totalSegsAcked += m_segmentsAcked;
    }

  NS_TEST_ASSERT_MSG_EQ (m_state->m_cWnd.Get (), m_cWnd + m_segmentSize,
                         "Prague cWnd has not updated according to LinuxReno in Congestion Avoidance");
}

/**
 * \ingroup internet-test
 * \ingroup tests
 *
 * \brief Test to validate RTT independence in TcpPrague
 */
class TcpPragueRttIndependenceTest : public TestCase
{
public:
  /**
   * \brief Constructor
   *
   * \param cWnd congestion window
   * \param segmentSize segment size
   * \param ssThresh slow start threshold
   * \param segmentsAcked segments acked
   * \param highTxMark high tx mark
   * \param lastAckedSeq last acked seq
   * \param rtt RTT
   * \param mode RTT scaling heuristic
   * \param name Name of the test
   */
  TcpPragueRttIndependenceTest (uint32_t cWnd, uint32_t segmentSize, uint32_t ssThresh,
                                uint32_t segmentsAcked, SequenceNumber32 highTxMark,
                                SequenceNumber32 lastAckedSeq, Time rtt, TcpPrague::RttScalingMode_t mode, const std::string &name);

private:
  virtual void DoRun (void);

  /**
   * \brief Execute the test.
   */
  void ExecuteTest (void);

  /**
   * \brief Return the additive increase factor
   * \returns Additive increase factor
   */
  double_t GetAdditiveFactor (void);

  uint32_t m_cWnd;                        //!< cWnd
  uint32_t m_segmentSize;                 //!< segment size
  uint32_t m_segmentsAcked;               //!< segments acked
  uint32_t m_ssThresh;                    //!< ss thresh
  Time m_rtt;                             //!< rtt
  SequenceNumber32 m_highTxMark;          //!< high tx mark
  SequenceNumber32 m_lastAckedSeq;        //!< last acked seq
  Ptr<TcpSocketState> m_state;            //!< state
  TcpPrague::RttScalingMode_t m_mode;     //!< rtt scaling heuristic
  Time m_rttTarget;                       //!< target rtt
  double_t m_cWndCnt;                     //!< congestion window counter
  double_t m_increaseFactor;              //!< additive increase factor
};

TcpPragueRttIndependenceTest::TcpPragueRttIndependenceTest (uint32_t cWnd, uint32_t segmentSize, uint32_t ssThresh,
                                                            uint32_t segmentsAcked, SequenceNumber32 highTxMark,
                                                            SequenceNumber32 lastAckedSeq, Time rtt, TcpPrague::RttScalingMode_t mode, const std::string &name)
  : TestCase (name),
    m_cWnd (cWnd),
    m_segmentSize (segmentSize),
    m_segmentsAcked (segmentsAcked),
    m_ssThresh (ssThresh),
    m_rtt (rtt),
    m_highTxMark (highTxMark),
    m_lastAckedSeq (lastAckedSeq),
    m_mode (mode)
{}

void
TcpPragueRttIndependenceTest::DoRun ()
{
  Simulator::Schedule (Seconds (0.0), &TcpPragueRttIndependenceTest::ExecuteTest, this);
  Simulator::Run ();
  Simulator::Destroy ();
}

void
TcpPragueRttIndependenceTest::ExecuteTest ()
{
  m_state = CreateObject <TcpSocketState> ();
  m_state->m_cWnd = m_cWnd;
  m_state->m_ssThresh = m_ssThresh;
  m_state->m_segmentSize = m_segmentSize;
  m_state->m_highTxMark = m_highTxMark;
  m_state->m_lastAckedSeq = m_lastAckedSeq;
  m_state->m_lastRtt = m_rtt;

  NS_TEST_ASSERT_MSG_LT (m_state->m_ssThresh.Get (), m_state->m_cWnd.Get (), "RTT independence should be tested in Congestion Avoidance");

  Ptr<TcpPrague> congPrague = CreateObject <TcpPrague> ();
  TimeValue rttTarget;
  congPrague->GetAttribute ("RttTarget", rttTarget);
  m_rttTarget = rttTarget.Get ();
  congPrague->SetRttScalingMode (m_mode);
  congPrague->SetRttTransitionDelay (0); // enforce rtt independence immediately in congestion avoidance
  congPrague->Init (m_state);

  congPrague->UpdateCwnd (m_state, m_segmentsAcked);
  double_t cWndCnt = congPrague->GetCwndCnt ();

  m_increaseFactor = GetAdditiveFactor ();
  uint32_t segCwnd = m_cWnd / m_segmentSize;
  m_cWndCnt = 1.0 * m_segmentsAcked * m_increaseFactor / segCwnd;

  NS_TEST_ASSERT_MSG_EQ (m_cWndCnt, cWndCnt,
                         "Prague cWnd counter has not updated as per the RTT scaling heuristic");
}

double_t
TcpPragueRttIndependenceTest::GetAdditiveFactor (void)
{
  Time lastRtt = m_rtt;
  Time maxScaledRtt = MilliSeconds (100);
  if (m_mode == TcpPrague::RTT_CONTROL_NONE || lastRtt > maxScaledRtt)
    {
      return 1;
    }

  if (m_mode == TcpPrague::RTT_CONTROL_RATE || m_mode == TcpPrague::RTT_CONTROL_ADDITIVE)
    {
      Time target = m_rttTarget;
      if (m_mode == TcpPrague::RTT_CONTROL_ADDITIVE)
        {
          target += m_rtt;
        }

      if (m_rtt.GetSeconds () > target.GetSeconds ())
        {
          return 1;
        }
      return 1.0 * m_rtt.GetSeconds () * m_rtt.GetSeconds () / (target.GetSeconds () * target.GetSeconds ());
    }
  else
    {
      Time R0 = Seconds (0.016), R1 = Seconds (0.0015);
      double_t increase = R0.GetSeconds () / 8 + std::min (std::max (lastRtt.GetSeconds () - R1.GetSeconds (), 0.0), R0.GetSeconds ());
      increase = increase * lastRtt.GetSeconds () / R0.GetSeconds () * R0.GetSeconds ();
      return increase;
    }
}


/**
 * \ingroup internet-test
 * \ingroup tests
 *
 * \brief TCP Prague TestSuite
 */
class TcpPragueTestSuite : public TestSuite
{
public:
  TcpPragueTestSuite () : TestSuite ("tcp-prague-test", UNIT)
  {
    // slow start test, cWnd < ssThresh
    // to ensure that Prague behaves like LinuxReno, it is better to set segmentsAcked to a value greater than 1
    AddTestCase (new TcpPragueSlowStartTest (2 * 1446, 1446, 4 * 1446, 2, SequenceNumber32 (4753), SequenceNumber32 (3216), MilliSeconds (10), "TcpPrague behaves like TcpLinuxReno during Slow Start"), TestCase::QUICK);

    // congestion avoidance test, cWnd >= ssThresh
    AddTestCase (new TcpPragueCongestionAvoidanceTest (5 * 1446, 1446, 3 * 1446, 2, SequenceNumber32 (4753), SequenceNumber32 (3216), MilliSeconds (10), "TcpPrague behaves like TcpLinuxReno during Congestion Avoidance"), TestCase::QUICK);

    // rtt independence is handled in congestion avoidance, cWnd >= ssThresh
    AddTestCase (new TcpPragueRttIndependenceTest (5 * 1446, 1446, 3 * 1446, 2, SequenceNumber32 (4753), SequenceNumber32 (3216), MilliSeconds (10), TcpPrague::RTT_CONTROL_NONE, "TcpPrague with the RTT_CONTROL_NONE scaling heuristic"), TestCase::QUICK);

    AddTestCase (new TcpPragueRttIndependenceTest (5 * 1446, 1446, 3 * 1446, 2, SequenceNumber32 (4753), SequenceNumber32 (3216), MilliSeconds (10), TcpPrague::RTT_CONTROL_RATE, "TcpPrague with the RTT_CONTROL_RATE scaling heuristic"), TestCase::QUICK);

    AddTestCase (new TcpPragueRttIndependenceTest (5 * 1446, 1446, 3 * 1446, 2, SequenceNumber32 (4753), SequenceNumber32 (3216), MilliSeconds (10), TcpPrague::RTT_CONTROL_SCALABLE, "TcpPrague with the RTT_CONTROL_SCALABLE scaling heuristic"), TestCase::QUICK);

    AddTestCase (new TcpPragueRttIndependenceTest (5 * 1446, 1446, 3 * 1446, 2, SequenceNumber32 (4753), SequenceNumber32 (3216), MilliSeconds (100), TcpPrague::RTT_CONTROL_ADDITIVE, "TcpPrague with the RTT_CONTROL_ADDITIVE scaling heuristic"), TestCase::QUICK);
  }
};

static TcpPragueTestSuite g_tcpPragueTest; //!< Static variable for test initialization
