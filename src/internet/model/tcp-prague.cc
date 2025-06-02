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
 *
 */

#include "tcp-prague.h"

#include "math.h"
#include "tcp-socket-state.h"

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/trace-source-accessor.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TcpPrague");

NS_OBJECT_ENSURE_REGISTERED(TcpPrague);

TypeId
TcpPrague::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TcpPrague")
            .SetParent<TcpCongestionOps>()
            .AddConstructor<TcpPrague>()
            .SetGroupName("Internet")
            .AddAttribute("Gain",
                          "Parameter g (EWMA gain) for updating alpha",
                          DoubleValue(0.0625),
                          MakeDoubleAccessor(&TcpPrague::m_g),
                          MakeDoubleChecker<double>(0, 1))
            .AddAttribute("AlphaOnInit",
                          "Initial alpha value",
                          // GWhite testing 3/19/24
                          // DoubleValue(1.0),
                          DoubleValue(0.0625),
                          MakeDoubleAccessor(&TcpPrague::m_alpha),
                          MakeDoubleChecker<double>(0, 1))
            .AddAttribute("UseEct0",
                          "Use ECT(0) for ECN codepoint, if false use ECT(1)",
                          BooleanValue(false),
                          MakeBooleanAccessor(&TcpPrague::m_useEct0),
                          MakeBooleanChecker())
            .AddAttribute("RttVirt",
                          "Virtual RTT",
                          TimeValue(MilliSeconds(25)),
                          MakeTimeAccessor(&TcpPrague::m_rttVirt),
                          MakeTimeChecker())
            .AddAttribute(
                "RttTransitionDelay",
                "Number of rounds post Slow Start after which RTT independence is enabled",
                UintegerValue(4),
                MakeUintegerAccessor(&TcpPrague::m_rttTransitionDelay),
                MakeUintegerChecker<uint32_t>())
            .AddAttribute("RttScalingMode",
                          "RTT Independence Scaling Heuristic",
                          EnumValue(TcpPrague::RTT_CONTROL_RATE),
                          MakeEnumAccessor(&TcpPrague::SetRttScalingMode),
                          MakeEnumChecker(TcpPrague::RTT_CONTROL_NONE,
                                          "None",
                                          TcpPrague::RTT_CONTROL_RATE,
                                          "Rate",
                                          TcpPrague::RTT_CONTROL_SCALABLE,
                                          "Scalable",
                                          TcpPrague::RTT_CONTROL_ADDITIVE,
                                          "Additive"))
            .AddTraceSource("Alpha",
                            "Value of TCP Prague alpha variable",
                            MakeTraceSourceAccessor(&TcpPrague::m_alpha),
                            "ns3::TracedValueCallback::Double");
    return tid;
}

std::string
TcpPrague::GetName() const
{
    return "TcpPrague";
}

TcpPrague::TcpPrague()
    : TcpCongestionOps()
{
    NS_LOG_FUNCTION(this);
    m_ackedBytesEcn = 0;
    m_ackedBytesTotal = 0;
    m_priorRcvNxt = SequenceNumber32(0);
    m_priorRcvNxtFlag = false;
    m_nextSeq = SequenceNumber32(0);
    m_nextSeqFlag = false;
    m_ceState = false;
    m_delayedAckReserved = false;
}

TcpPrague::TcpPrague(const TcpPrague& sock)
    : TcpCongestionOps(sock),
      m_ackedBytesEcn(sock.m_ackedBytesEcn),
      m_ackedBytesTotal(sock.m_ackedBytesTotal),
      m_priorRcvNxt(sock.m_priorRcvNxt),
      m_priorRcvNxtFlag(sock.m_priorRcvNxtFlag),
      m_alpha(sock.m_alpha),
      m_nextSeq(sock.m_nextSeq),
      m_nextSeqFlag(sock.m_nextSeqFlag),
      m_ceState(sock.m_ceState),
      m_delayedAckReserved(sock.m_delayedAckReserved),
      m_g(sock.m_g),
      m_useEct0(sock.m_useEct0)
{
    NS_LOG_FUNCTION(this);
}

TcpPrague::~TcpPrague()
{
    NS_LOG_FUNCTION(this);
}

Ptr<TcpCongestionOps>
TcpPrague::Fork()
{
    NS_LOG_FUNCTION(this);
    return CopyObject<TcpPrague>(this);
}

void
TcpPrague::Init(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);
    if (!m_initialized)
    {
        // these steps occur prior to SYN exchange
        NS_LOG_INFO(this << "Enabling DctcpEcn for TCP Prague");
        tcb->m_useEcn = TcpSocketState::On;
        tcb->m_ecnMode = TcpSocketState::DctcpEcn;
        tcb->m_ectCodePoint = m_useEct0 ? TcpSocketState::Ect0 : TcpSocketState::Ect1;
        m_initialized = true;
    }
    else
    {
        // these steps occur upon moving to ESTABLISHED
        tcb->m_pacing = true;
        tcb->m_paceInitialWindow = true;
        tcb->m_pacingCaRatio = 100;
        UpdatePacingRate(tcb);

        // related to rtt independence
        m_round = 0;
        m_alphaStamp = Simulator::Now();
        NewRound(tcb);
    }
}

bool
TcpPrague::HasCongControl() const
{
    NS_LOG_FUNCTION(this);
    return true;
}

void
TcpPrague::CongControl(Ptr<TcpSocketState> tcb,
                       const TcpRateOps::TcpRateConnection& rc,
                       const TcpRateOps::TcpRateSample& rs)
{
    NS_LOG_FUNCTION(this << tcb << rs);
    UpdatePacingRate(tcb);
}

void
TcpPrague::UpdatePacingRate(Ptr<TcpSocketState> tcb) const
{
    NS_LOG_FUNCTION(this);

    uint32_t pacingFactor;
    if (tcb->m_cWnd < tcb->m_ssThresh / 2)
    {
        pacingFactor = tcb->m_pacingSsRatio;
    }
    else
    {
        pacingFactor = tcb->m_pacingCaRatio;
    }

    // m_pacingCaRatio and m_pacingSsRatio are in units of percentages
    // multiplying the numerator by a pacing ratio and then by 1e4 (total
    // of 1e6) is compensated by dividing by microseconds; resulting
    // DataRate argument is in units of bits/sec
    uint64_t r = static_cast<uint64_t>(std::max(tcb->m_cWnd, tcb->m_bytesInFlight)) * 8 *
                 pacingFactor * 1e4 / static_cast<uint64_t>(tcb->m_lastRtt.Get().GetMicroSeconds());
    DataRate rate(r);
    if (rate < tcb->m_maxPacingRate)
    {
        NS_LOG_DEBUG("Pacing rate updated to: " << rate);
        tcb->m_pacingRate = rate;
    }
    else
    {
        NS_LOG_DEBUG("Pacing capped by max pacing rate: " << tcb->m_maxPacingRate);
        tcb->m_pacingRate = tcb->m_maxPacingRate;
    }
}

uint32_t
TcpPrague::GetSsThresh(Ptr<const TcpSocketState> state, uint32_t bytesInFlight)
{
    NS_LOG_FUNCTION(this << state << bytesInFlight);

    if (!m_sawCE)
    {
        NS_LOG_DEBUG("Haven't seen CE; returning ssthresh = " << bytesInFlight / 2);
        return std::max(2 * state->m_segmentSize, bytesInFlight / 2);
    }
    else
    {
        NS_LOG_DEBUG("Have seen CE; returning ssthresh = " << state->m_ssThresh);
        return state->m_ssThresh;
    }
}

void
TcpPrague::ReduceCwnd(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);

    if (m_rttScalingMode != RttScalingMode_t::RTT_CONTROL_NONE && m_rttVirt > (Simulator::Now () - m_cwrStamp))
    {
        NS_LOG_DEBUG("Suppressing ReduceCwnd() since last reduction was at " << m_cwrStamp.As (Time::S));
        return;
    }
    m_cwrStamp = Simulator::Now ();

    uint32_t cwnd_segs = tcb->m_cWnd / tcb->m_segmentSize;
    double_t reduction = m_alpha * cwnd_segs / 2.0;
    // GWhite testing 3/19/24
    if (!m_sawCE)
    {
        reduction = cwnd_segs / 2.0;
    }
    m_cWndCnt -= reduction;
    NS_LOG_DEBUG("ReduceCwnd: alpha " << m_alpha << " cwnd_segs " << cwnd_segs << " reduction "
                                      << reduction << " m_cWndCnt " << m_cWndCnt);
}

uint32_t
TcpPrague::SlowStart(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);
    if (tcb->m_cWnd >= tcb->m_ssThresh)
    {
        NS_LOG_DEBUG("Return from SlowStart without action");
        return segmentsAcked;
    }
    uint32_t cwnd = std::min(((uint32_t)tcb->m_cWnd + (segmentsAcked * tcb->m_segmentSize)),
                             (uint32_t)tcb->m_ssThresh);
    NS_ABORT_MSG_IF(cwnd < tcb->m_cWnd, "Subtraction overflow");
    segmentsAcked -= ((cwnd - tcb->m_cWnd) / tcb->m_segmentSize);
    tcb->m_cWnd = cwnd;
    NS_LOG_INFO("In SlowStart, updated to cwnd " << tcb->m_cWnd << "; returning " << segmentsAcked);
    return segmentsAcked;
}

void
TcpPrague::RenoCongestionAvoidance(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);

    uint32_t w = tcb->m_cWnd / tcb->m_segmentSize;
    // Floor w to 1 if w == 0
    if (w == 0)
    {
        w = 1;
    }
    NS_LOG_DEBUG("w in segments " << w << " m_cWndCntReno " << m_cWndCntReno << " segments acked "
                                  << segmentsAcked);
    if (m_cWndCntReno >= w)
    {
        m_cWndCntReno = 0;
        tcb->m_cWnd += tcb->m_segmentSize;
        NS_LOG_DEBUG("Adding 1 segment to m_cWnd");
    }
    m_cWndCntReno += segmentsAcked;
    NS_LOG_DEBUG("Adding " << segmentsAcked << " segments to m_cWndCntReno");
    if (m_cWndCntReno >= w)
    {
        uint32_t delta = m_cWndCntReno / w;

        m_cWndCntReno -= delta * w;
        tcb->m_cWnd += delta * tcb->m_segmentSize;
        NS_LOG_DEBUG("Subtracting delta * w from m_cWndCntReno " << delta * w);
    }
    NS_LOG_DEBUG("At end of CongestionAvoidance(), m_cWnd: " << tcb->m_cWnd << " m_cWndCntReno: "
                                                             << m_cWndCntReno);
}

void
TcpPrague::UpdateCwnd(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);

    if (!m_inLoss)
    {
        uint32_t acked = segmentsAcked;
        if (tcb->m_cWnd < tcb->m_ssThresh)
        {
            // Slow Start similar to ns3::TcpLinuxReno
            acked = SlowStart(tcb, segmentsAcked);
            if (!acked)
            {
                CwndChanged(tcb);
                return;
            }
        }
        // Congestion Avoidance
        uint32_t cwnd_segs = tcb->m_cWnd / tcb->m_segmentSize;
        double_t increase = 1.0 * acked * m_aiAckIncrease / cwnd_segs;
        m_cWndCnt += increase;
    }
    else
    {
        // Apply EnterLoss() cwnd reduction here
        if (m_lossWindowReduction)
        {
            NS_LOG_INFO("Reducing cwnd from " << tcb->m_cWnd << " by " << m_lossWindowReduction
                                              << " bytes");
            if (tcb->m_cWnd > m_lossWindowReduction)
            {
                tcb->m_cWnd -= m_lossWindowReduction;
                if (tcb->m_cWnd < 2 * tcb->m_segmentSize)
                {
                    tcb->m_cWnd = 2 * tcb->m_segmentSize;
                }
            }
            else
            {
                tcb->m_cWnd = 2 * tcb->m_segmentSize;
            }
            m_lossWindowReduction = 0;
        }
        uint32_t acked = SlowStart(tcb, segmentsAcked);
        if (!acked)
        {
            NS_LOG_DEBUG("Slow start increase only of " << segmentsAcked << " segs");
            return;
        }
        RenoCongestionAvoidance(tcb, acked);
        NS_LOG_DEBUG("Congestion avoidance after loss, cwnd updated to "
                     << tcb->m_cWnd << " ssthresh " << tcb->m_ssThresh);
        return;
    }

    if (m_cWndCnt <= -1)
    {
        m_cWndCnt++;
        NS_LOG_DEBUG("Decrementing m_cWnd from " << tcb->m_cWnd << " to "
                                                 << tcb->m_cWnd - tcb->m_segmentSize);
        tcb->m_cWnd -= tcb->m_segmentSize;
        if (tcb->m_cWnd < 2 * tcb->m_segmentSize)
        {
            tcb->m_cWnd = 2 * tcb->m_segmentSize;
            m_cWndCnt = 0;
        }
        tcb->m_ssThresh = tcb->m_cWnd;
        CwndChanged(tcb);
    }
    else if (m_cWndCnt >= 1)
    {
        m_cWndCnt--;
        NS_LOG_DEBUG("Incrementing m_cWnd from " << tcb->m_cWnd << " to "
                                                 << tcb->m_cWnd + tcb->m_segmentSize);
        tcb->m_cWnd += tcb->m_segmentSize;
        CwndChanged(tcb);
    }
}

void
TcpPrague::UpdateAlpha(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);

    if (m_sawCE)
    {
        if (!m_nextSeqFlag)
        {
            m_nextSeq = tcb->m_nextTxSequence;
            m_nextSeqFlag = true;
        }
        if (tcb->m_lastAckedSeq >= m_nextSeq)
        {
            double bytesEcn = 0.0;
            if (m_ackedBytesTotal > 0)
            {
                bytesEcn = static_cast<double>(m_ackedBytesEcn * 1.0 / m_ackedBytesTotal);
            }
            m_alpha = (1.0 - m_g) * m_alpha + m_g * bytesEcn;
            NS_LOG_INFO("bytesEcn " << bytesEcn << ", m_alpha " << m_alpha);
            m_alphaStamp = Simulator::Now();
        }
    }
    NewRound(tcb);
}

void
TcpPrague::PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked << rtt);

    // This method is similar to prague_cong_control() in Linux

    m_ackedBytesTotal += segmentsAcked * tcb->m_segmentSize;
    if (tcb->m_ecnState == TcpSocketState::ECN_ECE_RCVD)
    {
        m_sawCE = true;
        m_ackedBytesEcn += segmentsAcked * tcb->m_segmentSize;
        UpdateCwnd(tcb, 0);
    }
    else
    {
        UpdateCwnd(tcb, segmentsAcked);
    }
    if (ShouldUpdateEwma(tcb))
    {
        UpdateAlpha(tcb, segmentsAcked);
    }
}

void
TcpPrague::NewRound(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);

    m_nextSeq = tcb->m_nextTxSequence;
    m_ackedBytesEcn = 0;
    m_ackedBytesTotal = 0;
    if (tcb->m_cWnd >= tcb->m_ssThresh)
    {
        ++m_round;
    }
    AiAckIncrease(tcb);
}

void
TcpPrague::CwndChanged(Ptr<TcpSocketState> tcb)
{
    // This method is similar to prague_cwnd_changed() in Linux
    NS_LOG_FUNCTION(this << tcb);

    AiAckIncrease(tcb);
}

void
TcpPrague::EnterLoss(Ptr<TcpSocketState> tcb)
{
    // This method is similar to prague_enter_loss() in Linux
    NS_LOG_FUNCTION(this << tcb);

    m_cWndCnt = 0;
    m_inLoss = true;
    m_lossWindowReduction = tcb->m_cWnd / 2; // Will be applied later to m_cWnd
    tcb->m_ssThresh = tcb->m_cWnd / 2;
    m_cWndCntReno = 0;
}

void
TcpPrague::CeState0to1(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);

    if (!m_ceState && m_delayedAckReserved && m_priorRcvNxtFlag)
    {
        SequenceNumber32 tmpRcvNxt;
        /* Save current NextRxSequence. */
        tmpRcvNxt = tcb->m_rxBuffer->NextRxSequence();

        /* Generate previous ACK without ECE */
        tcb->m_rxBuffer->SetNextRxSequence(m_priorRcvNxt);
        tcb->m_sendEmptyPacketCallback(TcpHeader::ACK);

        /* Recover current RcvNxt. */
        tcb->m_rxBuffer->SetNextRxSequence(tmpRcvNxt);
    }

    if (!m_priorRcvNxtFlag)
    {
        m_priorRcvNxtFlag = true;
    }
    m_priorRcvNxt = tcb->m_rxBuffer->NextRxSequence();
    m_ceState = true;
    tcb->m_ecnState = TcpSocketState::ECN_CE_RCVD;
}

void
TcpPrague::CeState1to0(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);

    if (m_ceState && m_delayedAckReserved && m_priorRcvNxtFlag)
    {
        SequenceNumber32 tmpRcvNxt;
        /* Save current NextRxSequence. */
        tmpRcvNxt = tcb->m_rxBuffer->NextRxSequence();

        /* Generate previous ACK with ECE */
        tcb->m_rxBuffer->SetNextRxSequence(m_priorRcvNxt);
        tcb->m_sendEmptyPacketCallback(TcpHeader::ACK | TcpHeader::ECE);

        /* Recover current RcvNxt. */
        tcb->m_rxBuffer->SetNextRxSequence(tmpRcvNxt);
    }

    if (!m_priorRcvNxtFlag)
    {
        m_priorRcvNxtFlag = true;
    }
    m_priorRcvNxt = tcb->m_rxBuffer->NextRxSequence();
    m_ceState = false;

    if ((tcb->m_ecnState == TcpSocketState::ECN_CE_RCVD) ||
        (tcb->m_ecnState == TcpSocketState::ECN_SENDING_ECE))
    {
        tcb->m_ecnState = TcpSocketState::ECN_IDLE;
    }
}

void
TcpPrague::UpdateAckReserved(Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCAEvent_t event)
{
    NS_LOG_FUNCTION(this << tcb << event);

    switch (event)
    {
    case TcpSocketState::CA_EVENT_DELAYED_ACK:
        if (!m_delayedAckReserved)
        {
            m_delayedAckReserved = true;
        }
        break;
    case TcpSocketState::CA_EVENT_NON_DELAYED_ACK:
        if (m_delayedAckReserved)
        {
            m_delayedAckReserved = false;
        }
        break;
    default:
        /* Don't care for the rest. */
        break;
    }
}

void
TcpPrague::CwndEvent(Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCAEvent_t event)
{
    NS_LOG_FUNCTION(this << tcb << TcpSocketState::TcpCongAvoidName[event]);

    switch (event)
    {
    case TcpSocketState::CA_EVENT_ECN_IS_CE:
        CeState0to1(tcb);
        break;
    case TcpSocketState::CA_EVENT_ECN_NO_CE:
        CeState1to0(tcb);
        break;
    case TcpSocketState::CA_EVENT_DELAYED_ACK:
    case TcpSocketState::CA_EVENT_NON_DELAYED_ACK:
        UpdateAckReserved(tcb, event);
        break;
    case TcpSocketState::CA_EVENT_CWND_RESTART:
    case TcpSocketState::CA_EVENT_LOSS:
        EnterLoss(tcb);
        break;
    case TcpSocketState::CA_EVENT_TX_START:
    default:
        break;
    }
}

void
TcpPrague::CongestionStateSet(Ptr<TcpSocketState> tcb,
                              const TcpSocketState::TcpCongState_t newState)
{
    NS_LOG_FUNCTION(this << tcb << TcpSocketState::TcpCongStateName[newState]);
    switch (newState)
    {
    case TcpSocketState::CA_OPEN:
        m_inLoss = false;
        break;
    case TcpSocketState::CA_RECOVERY:
        EnterLoss(tcb);
        break;
    case TcpSocketState::CA_CWR:
        ReduceCwnd(tcb);
        break;
    case TcpSocketState::CA_DISORDER:
    case TcpSocketState::CA_LOSS:  // No need to act here; CA_EVENT_LOSS will cover
    default:
        break;
    }
}

void
TcpPrague::SetRttTransitionDelay(uint32_t rounds)
{
    m_rttTransitionDelay = rounds;
}

void
TcpPrague::SetRttScalingMode(TcpPrague::RttScalingMode_t scalingMode)
{
    m_rttScalingMode = scalingMode;
}

bool
TcpPrague::IsRttIndependent(Ptr<TcpSocketState> tcb)
{
    // This method is similar to prague_is_rtt_indep in Linux
    NS_LOG_FUNCTION(this << tcb);

    return m_rttScalingMode != RttScalingMode_t::RTT_CONTROL_NONE &&
           !(tcb->m_cWnd < tcb->m_ssThresh) && m_round >= m_rttTransitionDelay;
}

double_t
TcpPrague::GetCwndCnt() const
{
    return m_cWndCnt;
}

Time
TcpPrague::GetTargetRtt(Ptr<TcpSocketState> tcb)
{
    // This method is similar to prague_target_rtt in Linux
    NS_LOG_FUNCTION(this << tcb);

    /* Referred from TcpOptionTS::NowToTsValue */
    Time target = m_rttVirt;
    if (m_rttScalingMode != RttScalingMode_t::RTT_CONTROL_ADDITIVE)
    {
        return target;
    }
    Time lastRtt = tcb->m_lastRtt;
    target += lastRtt;
    return target;
}

bool
TcpPrague::ShouldUpdateEwma(Ptr<TcpSocketState> tcb)
{
    // This method is similar to prague_should_update_ewma in Linux
    NS_LOG_FUNCTION(this << tcb);

    bool pragueE2eRttElapsed = !(tcb->m_txBuffer->HeadSequence() < m_nextSeq);

    // Instead of Linux-like tcp_mstamp, use simulator time
    // GWhite testing 3/19/24
    // bool targetRttElapsed = false;
    // if (m_rttScalingMode != RttScalingMode_t::RTT_CONTROL_NONE)
    // {
    //     targetRttElapsed =
    //         (GetTargetRtt(tcb).GetSeconds() <=
    //          std::max(Simulator::Now().GetSeconds() - m_alphaStamp.GetSeconds(), 0.0));
    // }
    NS_LOG_DEBUG("pragueE2eRttElapsed " << pragueE2eRttElapsed << " m_nextTxSequence "
                                        << tcb->m_nextTxSequence << " m_nextSeq " << m_nextSeq);
    // GWhite testing 3/19/24
    // return (pragueE2eRttElapsed && ((m_rttScalingMode == RttScalingMode_t::RTT_CONTROL_NONE) ||
    //                                 !IsRttIndependent(tcb) || targetRttElapsed));
    return (pragueE2eRttElapsed);
}

void
TcpPrague::AiAckIncrease(Ptr<TcpSocketState> tcb)
{
    // This method is similar to prague_ai_ack_increase in Linux
    NS_LOG_FUNCTION(this << tcb);

    Time lastRtt = tcb->m_lastRtt;
    Time maxScaledRtt = MilliSeconds(100);
    if (m_rttScalingMode == RttScalingMode_t::RTT_CONTROL_NONE || m_round < m_rttTransitionDelay ||
        lastRtt > maxScaledRtt)
    {
        m_aiAckIncrease = 1;
        return;
    }

    // Use other heuristics
    if (m_rttScalingMode == RttScalingMode_t::RTT_CONTROL_RATE ||
        m_rttScalingMode == RttScalingMode_t::RTT_CONTROL_ADDITIVE)
    {
        // Linux would call prague_rate_scaled_ai_ack_increase
        Time target = GetTargetRtt(tcb);
        if (lastRtt.GetSeconds() > target.GetSeconds())
        {
            m_aiAckIncrease = 1;
            return;
        }
        m_aiAckIncrease = 1.0 * lastRtt.GetSeconds() * lastRtt.GetSeconds() /
                          (target.GetSeconds() * target.GetSeconds());
    }
    else
    {
        // Linux would call prague_scalable_ai_ack_increase
        Time R0 = Seconds(0.016);
        Time R1 = Seconds(0.0015); // 16ms and 1.5ms
        double_t increase =
            R0.GetSeconds() / 8 +
            std::min(std::max(lastRtt.GetSeconds() - R1.GetSeconds(), 0.0), R0.GetSeconds());
        increase = increase * lastRtt.GetSeconds() / R0.GetSeconds() * R0.GetSeconds();
        m_aiAckIncrease = increase;
    }
}

} // namespace ns3
