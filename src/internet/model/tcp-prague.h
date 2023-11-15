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

#ifndef TCP_PRAGUE_H
#define TCP_PRAGUE_H

#include "tcp-congestion-ops.h"

namespace ns3
{

/**
 * \ingroup tcp
 *
 * \brief An implementation of TCP Prague that is aligned with
 * Linux: https://github.com/L4STeam/linux/tree/testing
 *
 * As of now, ns3::TcpPrague supports dynamic pacing rate and
 * RTT Independence, both of which are present in Linux as well.
 *
 * This implementation is different from ns3::TcpDctcp in the
 * following ways:
 * 1. cWnd increment during Slow Start aligns with ns3::TcpLinuxReno
 *
 * 2. cWnd increment during Congestion Avoidance occurs discretely
 * with the help of a cWnd counter, and is updated for every ACK
 *
 * 3. In case of ECE marks, cWnd is not immediately reduced by a
 * factor of (1 - \alpha) / 2 but instead decremented by one segment
 * for every ACK depending on the value of cWnd counter
 *
 */

class TcpPrague : public TcpCongestionOps
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    TcpPrague();

    /**
     * \brief Copy constructor
     * \param sock the object to copy
     */
    TcpPrague(const TcpPrague& sock);

    ~TcpPrague() override;

    std::string GetName() const override;

    /**
     * \brief Set configuration required by congestion control algorithm.
     *        This method will force DctcpEcn mode and will force usage of
     *        either ECT(0) or ECT(1) (depending on the 'UseEct0' attribute),
     *        despite any other configuration in the base classes.
     *
     * \param tcb internal congestion state
     */
    void Init(Ptr<TcpSocketState> tcb) override;

    /**
     * \brief Return target RTT, equivalent to prague_target_rtt in Linux
     *
     * \param tcb internal congestion state
     * \return The target RTT
     */
    Time GetTargetRtt(Ptr<TcpSocketState> tcb);

    /**
     * \brief Sets the number of post-Slow Start rounds after which RTT independence is enabled
     *
     * \param rounds The RTT independence transition round delay
     */
    void SetRttTransitionDelay(uint32_t rounds);

    /**
     * \brief Decide RTT scaling heuristics mode
     */
    typedef enum
    {
        RTT_CONTROL_NONE,     //!< No RTT Independence
        RTT_CONTROL_RATE,     //!< Flows with e2e RTT < target try to achieve same throughput
        RTT_CONTROL_SCALABLE, //!< At low RTT, trade throughput balance for same marks/RTT
        RTT_CONTROL_ADDITIVE  //!< Behave as a flow operating with extra target RTT
    } RttScalingMode_t;

    /**
     * \brief Sets the RTT independence scaling heuristic
     *
     * \param scalingMode The RTT independence scaling mode
     */
    void SetRttScalingMode(RttScalingMode_t scalingMode);

    /** \brief Return true if Prague is trying to achieve RTT independence
     *
     * \param tcb internal congestion state
     * \return True if Prague is trying to achieve RTT independence
     */
    bool IsRttIndependent(Ptr<TcpSocketState> tcb);

    /** \brief Return the congestion window counter
     *
     * \param tcb internal congestion state
     * \return The congestion window counter
     */
    double_t GetCwndCnt() const;

    /**
     * \brief Update the congestion window
     *
     * \param tcb internal congestion state
     * \param segmentsAcked count of segments acked
     */
    void UpdateCwnd(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);

    /**
     * \brief Update the value of alpha
     *
     * \param tcb internal congestion state
     * \param segmentsAcked count of segments acked
     */
    void UpdateAlpha(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);

    // documented in base class
    Ptr<TcpCongestionOps> Fork() override;
    virtual void ReduceCwnd(Ptr<TcpSocketState> tcb);
    void CwndEvent(Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCAEvent_t event) override;
    uint32_t GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight) override;
    void PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt) override;
    void CongestionStateSet(Ptr<TcpSocketState> tcb,
                            const TcpSocketState::TcpCongState_t newState) override;

  private:
    /**
     * \brief Update the congestion window during Slow Start
     *
     * \param tcb internal state
     * \param segmentsAcked count of segments acked
     */
    virtual uint32_t SlowStart(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);

    /**
     * \brief Update the cWnd additive increase factor per ACK during Congestion Avoidance
     *
     * \param tcb internal state
     */
    void AiAckIncrease(Ptr<TcpSocketState> tcb);

    /**
     * \brief Return true if Prague EWMA should be updated
     *
     * \param tcb internal congestion state
     */
    bool ShouldUpdateEwma(Ptr<TcpSocketState> tcb);

    /**
     * \brief Update internal state when all packets in cWnd are ACKed
     *
     * \param tcb internal congestion state
     */
    void NewRound(Ptr<TcpSocketState> tcb);

    /**
     * \brief Update internal state whenever cWnd is updated
     *
     * \param tcb internal congestion state
     */
    void CwndChanged(Ptr<TcpSocketState> tcb);

    /**
     * \brief Update internal state when Prague encounters a loss
     *
     * \param tcb internal congestion state
     */
    void EnterLoss(Ptr<TcpSocketState> tcb);

    /**
     * \brief Resets the value of m_ackedBytesEcn, m_ackedBytesTotal and m_nextSeq
     *
     * \param tcb internal congestion state
     */
    void Reset(Ptr<TcpSocketState> tcb);

    /**
     * \brief Sets the value of m_alpha
     *
     * \param alpha Prague alpha parameter
     */
    void SetPragueAlpha(double alpha);

    /**
     * \brief Changes state of m_ceState to true
     *
     * \param tcb internal congestion state
     */
    void CeState0to1(Ptr<TcpSocketState> tcb);

    /**
     * \brief Changes state of m_ceState to false
     *
     * \param tcb internal congestion state
     */
    void CeState1to0(Ptr<TcpSocketState> tcb);

    /**
     * \brief Updates the value of m_delayedAckReserved
     *
     * \param tcb internal congestion state
     * \param event the congestion window event
     */
    void UpdateAckReserved(Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCAEvent_t event);

    /* Variables also present in ns3::TcpDctcp */
    uint32_t m_ackedBytesEcn;       //!< Number of acked bytes which are marked
    uint32_t m_ackedBytesTotal;     //!< Total number of acked bytes
    SequenceNumber32 m_priorRcvNxt; //!< Sequence number of the first missing byte in data
    bool m_priorRcvNxtFlag; //!< Variable used in setting the value of m_priorRcvNxt for first time
    double m_alpha;         //!< Parameter used to estimate the amount of network congestion
    SequenceNumber32
        m_nextSeq;      //!< TCP sequence number threshold for beginning a new observation window
    bool m_nextSeqFlag; //!< Variable used in setting the value of m_nextSeq for first time
    bool m_ceState;     //!< Prague Congestion Experienced state
    bool m_delayedAckReserved; //!< Delayed Ack state
    double m_g;                //!< Estimation gain
    bool m_useEct0;            //!< Use ECT(0) for ECN codepoint

    double_t m_cWndCnt{0}; //!< Prague cWnd update counter in segments
    bool m_sawCE{false};   //!< True if Prague has received ECE flag before
    bool m_inLoss{false};  //!< True if a packet loss occurs

    /* Related to RTT Independence */
    uint32_t m_round = 0;                                //!< Round count since last slow start exit
    RttScalingMode_t m_rttScalingMode{RTT_CONTROL_NONE}; //!< RTT independence scaling mode
    uint32_t m_rttTransitionDelay; //!< Amount of post-SS rounds to transition to be RTT independent
    Time m_alphaStamp;             //!< EWMA update timestamp
    Time m_rttVirt;                //!< virtual RTT
    double_t m_aiAckIncrease;      //!< AI increase per non-CE ACKed MSS
};

} // namespace ns3

#endif /* TCP_PRAGUE_H */
