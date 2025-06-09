/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 * Authors: Vivek Jain <jain.vivek.anand@gmail.com>
 *          Viyom Mittal <viyommittal@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#ifndef TCPBBR2_H
#define TCPBBR2_H

#include "ns3/tcp-congestion-ops.h"
#include "ns3/traced-value.h"
#include "ns3/data-rate.h"
#include "ns3/random-variable-stream.h"
#include "ns3/windowed-filter.h"

namespace ns3 {

// Forward declaration for RateSample
struct RateSample
{
    DataRate m_deliveryRate{0};
    Time m_interval{Seconds(0)};
    uint32_t m_delivered{0};
    uint32_t m_priorDelivered{0};
    uint32_t m_deliveredEce{0};
    uint32_t m_priorDeliveredEce{0};
    uint32_t m_packetLoss{0};
    uint32_t m_ackedSacked{0};
    uint32_t m_lastAckedSackedBytes{0};
    uint32_t m_priorInFlight{0};
    bool m_isAppLimited{false};
};

class TcpBbr2 : public TcpCongestionOps
{
public:
  /**
   * \brief The number of phases in the BBR ProbeBW gain cycle.
   */
  static const uint8_t GAIN_CYCLE_LENGTH = 8;

  /**
   * \brief BBR uses an eight-phase cycle with the given pacing_gain value
   * in the BBR ProbeBW gain cycle.
   */
  const static double PACING_GAIN_CYCLE [];

  /**
   * \brief BBR+ Pacing gain cycle values
   */
  const static double PACING_GAIN_CYCLE_HSR [];

  /**
   * \brief Delay-BBR Pacing gain cycle value
   */
  const static double PACING_GAIN_CYCLE_DELAY [];

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Constructor
   */
  TcpBbr2 ();

  /**
   * Copy constructor.
   * \param sock The socket to copy from.
   */
  TcpBbr2 (const TcpBbr2 &sock);

  /* BBR has the following modes for deciding how fast to send: */
  typedef enum
  {
    BBR_STARTUP,        /* ramp up sending rate rapidly to fill pipe */
    BBR_DRAIN,          /* drain any queue created during startup */
    BBR_PROBE_BW,       /* discover, share bw: pace around estimated bw */
    BBR_PROBE_RTT,      /* cut inflight to min to probe min_rtt */
  } BbrMode_t;

  typedef WindowedFilter<DataRate,
                         MaxFilter<DataRate>,
                         uint32_t,
                         uint32_t>
  MaxBandwidthFilter_t;

  // Different Variations of BBR
  typedef enum
  {
    BBR,
    BBR_PRIME,
    BBR_PLUS,
    BBR_HSR,
    BBR_V2,
    BBR_DELAY
  } BbrVar;

  // ProbeBW Pacing Gain Phase Indexes
  typedef enum
  {
    BBR_BW_PROBE_UP = 0,
    BBR_BW_PROBE_DOWN = 1,
    BBR_BW_PROBE_CRUISE = 2,
    BBR_BW_PROBE_REFILL = 3,
    BBR_BW_PROBE_NS = 4
  } BbrBwPhase;

  // Relation between the ACK stream and the bandwidth probing
  typedef enum
  {
    BBR_ACKS_INIT,            // Not probing
    BBR_ACKS_REFILLING,       // Sending at estimated bw to fill pipe
    BBR_ACK_PROBE_STARTING,   // Inflight rising to probe bw
    BBR_ACK_PROBE_FEEDBACK,   // Getting feedback from bw probing
    BBR_ACK_PROBE_STOPPING    // Stopped probing
  } BbrAckPhase;

  /**
   * Assign a fixed random variable stream number to the random variables
   * used by this model.  Return the number of streams (possibly zero) that
   * have been assigned.
   *
   * \param stream first stream index to use
   * \return the number of stream indices assigned by this model
   */
  virtual int64_t AssignStreams (int64_t stream);

  /**
   * \brief Gets BBR state.
   * \return returns BBR state.
   */
  uint32_t GetBbrState ();

  /**
   * \brief Gets current pacing gain.
   * \return returns current pacing gain.
   */
  double GetPacingGain ();

  /**
   * \brief Gets current cwnd gain.
   * \return returns current cwnd gain.
   */
  double GetCwndGain ();

  /**
   * \brief Updates variables specific to BBR_DRAIN state
   */
  void EnterDrain ();

  /**
   * \brief Updates variables specific to BBR_PROBE_BW state
   * \param tcb the socket state.
   */
  void EnterProbeBW (Ptr<TcpSocketState> tcb);

  /**
   * \brief Updates variables specific to BBR_PROBE_RTT state
   */
  void EnterProbeRTT ();

  /**
   * \brief Updates variables specific to BBR_STARTUP state
   */
  void EnterStartup ();

  /**
   * \brief Called on exiting from BBR_PROBE_RTT state, it eithers invoke EnterProbeBW () or EnterStartup ()
   * \param tcb the socket state.
   */
  void ExitProbeRTT (Ptr<TcpSocketState> tcb);

  virtual std::string GetName () const;
  virtual void PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                          const Time& rtt);
  virtual bool HasCongControl () const;
  virtual void CongControl (Ptr<TcpSocketState> tcb, const struct RateSample *rs);
  virtual void CongestionStateSet (Ptr<TcpSocketState> tcb,
                                   const TcpSocketState::TcpCongState_t newState);
  virtual void CwndEvent (Ptr<TcpSocketState> tcb,
                          const TcpSocketState::TcpCAEvent_t event);
  virtual uint32_t GetSsThresh (Ptr<const TcpSocketState> tcb,
                                uint32_t bytesInFlight);
  virtual Ptr<TcpCongestionOps> Fork ();

protected:
  /**
   * \brief Advances pacing gain using cycle gain algorithm, while in BBR_PROBE_BW state
   */
  void AdvanceCyclePhase ();

  /**
   * \brief Checks whether to advance pacing gain in BBR_PROBE_BW state,
   *  and if allowed calls AdvanceCyclePhase ()
   * \param tcb the socket state.
   * \param rs  rate sample
   */
  void CheckCyclePhase (Ptr<TcpSocketState> tcb, const struct RateSample * rs);

  /**
   * \brief Checks whether its time to enter BBR_DRAIN or BBR_PROBE_BW state
   * \param tcb the socket state.
   */
  void CheckDrain (Ptr<TcpSocketState> tcb);

  /**
   * \brief Identifies whether pipe or BDP is already full
   * \param rs  rate sample
   */
  void CheckFullPipe (const struct RateSample * rs);

  /**
   * \brief This method handles the steps related to the ProbeRTT state
   * \param tcb the socket state.
   */
  void CheckProbeRTT (Ptr<TcpSocketState> tcb);

  /**
   * \brief Handles the steps for BBR_PROBE_RTT state.
   * \param tcb the socket state.
   */
  void HandleProbeRTT (Ptr<TcpSocketState> tcb);

  /**
   * \brief Updates pacing rate if socket is restarting from idle state.
   * \param tcb the socket state.
   * \param rs  rate sample
   */
  void HandleRestartFromIdle (Ptr<TcpSocketState> tcb, const RateSample * rs);

  /**
   * \brief Estimates the target value for congestion window
   * \param tcb the socket state.
   * \param gain cwnd gain
   * \return estimated value for congestion window
   */
  uint32_t InFlight (Ptr<TcpSocketState> tcb, double gain);

  /**
   * \brief Initializes the full pipe estimator.
   */
  void InitFullPipe ();

  /**
   * \brief Initializes the pacing rate.
   * \param tcb the socket state.
   */
  void InitPacingRate (Ptr<TcpSocketState> tcb);

  /**
   * \brief Initializes the round counting.
   */
  void InitRoundCounting ();

  /**
   * \brief Checks whether its time to exit an ongoing BBR_PROBE_BW cycle phase and start the next phase.
   * \param tcb the socket state.
   * \param rs rate sample
   * \return true if time to advance to next phase, false otherwise
   */
  bool IsNextCyclePhase (Ptr<TcpSocketState> tcb, const struct RateSample * rs);

  /**
   * \brief Modulates congestion window in BBR_PROBE_RTT state
   * \param tcb the socket state.
   */
  void ModulateCwndForProbeRTT (Ptr<TcpSocketState> tcb);

  /**
   * \brief Modulates congestion window in CA_RECOVERY state
   * \param tcb the socket state.
   * \param rs rate sample
   */
  void ModulateCwndForRecovery (Ptr<TcpSocketState> tcb, const struct RateSample * rs);

  /**
   * \brief Helper to restore the last-known good congestion window
   * \param tcb the socket state.
   */
  void RestoreCwnd (Ptr<TcpSocketState> tcb);

  /**
   * \brief Advances max bw filter window and updates current max bw and lt bw.
   * \param tcb the socket state.
   */
  void SaveCwnd (Ptr<const TcpSocketState> tcb);

  /**
   * \brief Updates congestion window based on the network conditions
   * \param tcb the socket state.
   * \param rs rate sample
   */
  void SetCwnd (Ptr<TcpSocketState> tcb, const struct RateSample * rs);

  /**
   * \brief Updates pacing rate based on network conditions
   * \param tcb the socket state.
   * \param gain pacing gain
   */
  void SetPacingRate (Ptr<TcpSocketState> tcb, double gain);

  /**
   * \brief Updates send quantum
   * \param tcb the socket state.
   */
  void SetSendQuantum (Ptr<TcpSocketState> tcb);

  /**
   * \brief Updates max bottleneck bandwidth to send at
   * \param tcb the socket state.
   * \param rs rate sample
   */
  void UpdateBtlBw (Ptr<TcpSocketState> tcb, const struct RateSample * rs);

  /**
   * \brief Updates control parameters congestion windowm, pacing rate, send quantum
   * \param tcb the socket state.
   * \param rs rate sample
   */
  void UpdateControlParameters (Ptr<TcpSocketState> tcb, const struct RateSample * rs);

  /**
   * \brief Updates BBR network model
   * \param tcb the socket state.
   * \param rs rate sample
   */
  void UpdateModelAndState (Ptr<TcpSocketState> tcb, const struct RateSample * rs);

  /**
   * \brief Updates round counting and round start flag
   * \param tcb the socket state.
   * \param rs rate sample
   */
  void UpdateRound (Ptr<TcpSocketState> tcb, const struct RateSample * rs);

  /**
   * \brief Updates minimum RTT
   * \param tcb the socket state.
   */
  void UpdateRTprop (Ptr<TcpSocketState> tcb);

  /**
   * \brief Updates target congestion window
   * \param tcb the socket state.
   */
  void UpdateTargetCwnd (Ptr<TcpSocketState> tcb);

  /**
   * \brief Sets BBR state
   * \param state BBR state
   */
  void SetBbrState (BbrMode_t state);

  /**
   * \brief Returns state in string format
   * \param state BBR state
   * \return state in string format
   */
  std::string WhichState (BbrMode_t state) const;

  /**
   * \brief Gets BBR variant
   * \return BBR variant
   */
  uint32_t GetBbrVariant ();

  /**
   * \brief Sets BBR variant
   * \param variant BBR variant
   */
  void SetBbrVariant (BbrVar variant);

  /**
   * \brief Drain to target cycling for BBR+
   * \param tcb the socket state
   * \param rs rate sample
   */
  void DrainToTargetCycling (Ptr<TcpSocketState> tcb, const struct RateSample *rs);

  /**
   * \brief Sets cycle index for BBR+
   * \param index cycle index
   */
  void SetCycleIndex (BbrBwPhase index);

  /**
   * \brief Checks for congestion delay for Delay-BBR
   * \param tcb the socket state
   */
  void CheckCongestionDelay (Ptr<TcpSocketState> tcb);

  // BBR v2 functions
  void UpdateCongestionSignals (Ptr<TcpSocketState> tcb, const struct RateSample * rs);
  void ResetCongestionSignals ();
  bool IsProbingBandwidth ();
  void AdaptLowerBounds (Ptr<TcpSocketState> tcb);
  bool AdaptUpperBounds (Ptr<TcpSocketState> tcb, const struct RateSample * rs);
  void CheckExcessiveLossStartup (Ptr<TcpSocketState> tcb, const struct RateSample * rs);
  bool IsInflightTooHigh (Ptr<TcpSocketState> tcb, const struct RateSample * rs);
  void HandleInflightTooHigh (Ptr<TcpSocketState> tcb, const struct RateSample * rs);
  void HandleInflightTooHighEcn ();
  void EnterProbeDown (Ptr<TcpSocketState> tcb);
  void EnterProbeRefill (Ptr<TcpSocketState> tcb, uint32_t bwProbeUpRounds);
  void EnterProbeUp (Ptr<TcpSocketState> tcb);
  void EnterProbeCruise ();
  void UpdateCyclePhase (Ptr<TcpSocketState> tcb, const struct RateSample * rs);
  void AdvanceBwMaxFilter ();
  uint32_t TargetInflight ();
  void ProbeInflightHighUpward (Ptr<TcpSocketState> tcb, const struct RateSample * rs);
  void RaiseInflightHiSlope (Ptr<TcpSocketState> tcb);
  bool IsTimeToProbe (Ptr<TcpSocketState> tcb);
  bool IsTimeToCruise (Ptr<TcpSocketState> tcb);
  bool IsTimeToProbeRenoCoexistence ();
  uint32_t InflightWithHeadroom ();
  void BoundCwndForInflightModel (Ptr<TcpSocketState> tcb);
  void UpdateEcn (Ptr<TcpSocketState> tcb, const struct RateSample * rs);
  void CheckEcnTooHighStartup (Ptr<TcpSocketState> tcb, const struct RateSample * rs, uint32_t ratio);

private:
  BbrMode_t m_state{BBR_STARTUP};                          //!< Current state of BBR state machine
  MaxBandwidthFilter_t m_maxBwFilter;                      //!< Maximum bandwidth filter
  uint32_t m_bandwidthWindowLength{0};                     //!< A constant specifying the length of the BBR.BtlBw max filter window, default 10 packet-timed round trips.
  double m_pacingGain{0};                                  //!< The dynamic pacing gain factor used to scale BBR.bw to produce BBR.pacing_rate.
  double m_cWndGain{0};                                    //!< The dynamic congestion window gain factor used to scale the estimated BDP to produce a congestion window (cwnd).
  double m_highGain{0};                                    //!< A constant specifying the minimum gain value for increasing the pacing rate or congestion window when network is under-utilized, default 2.89.
  bool m_isPipeFilled{false};                              //!< A boolean that records whether BBR has filled the pipe.
  uint32_t m_minPipeCwnd{0};                               //!< The minimal congestion window value BBR tries to target, default 4 Segment size.
  uint32_t m_roundCount{0};                                //!< Count of packet-timed round trips.
  bool m_roundStart{false};                                //!< A boolean that BBR sets to true once per packet-timed round trip, on ACKs that advance BBR.round_count.
  uint32_t m_nextRoundDelivered{0};                        //!< Denotes the end of a packet-timed round trip.
  Time m_probeRttDuration{MilliSeconds (200)};             //!< A constant specifying the minimum duration for which ProbeRTT state, default 200 milliseconds.
  Time m_probeRtPropStamp{Seconds (0)};                    //!< The wall clock time at which the current BBR.RTProp sample was obtained.
  Time m_probeRttDoneStamp{Seconds (0)};                   //!< Time to exit from BBR_PROBE_RTT state.
  bool m_probeRttRoundDone{false};                         //!< True when it is time to exit BBR_PROBE_RTT.
  bool m_packetConservation{false};                        //!< Enable/Disable packet conservation mode.
  uint32_t m_priorCwnd{0};                                 //!< The last-known good congestion window.
  bool m_idleRestart{false};                               //!< When restarting from idle, set it true.
  uint32_t m_targetCWnd{0};                                //!< Target value for congestion window, adapted to the estimated BDP.
  DataRate m_fullBandwidth{0};                             //!< Value of full bandwidth recorded.
  uint32_t m_fullBandwidthCount{0};                        //!< Count of full bandwidth recorded consistently.
  TracedValue<Time> m_rtProp{Time::Max ()};                //!< Estimated two-way round-trip propagation delay of the path, estimated from the windowed minimum RTT sample.
  uint32_t m_sendQuantum{0};                               //!< The maximum size of a data aggregate scheduled and transmitted together.
  Time m_cycleStamp{Seconds (0)};                          //!< Last time stamp when cycle updated.
  uint32_t m_cycleIndex{0};                                //!< Current index of gain cycle.
  bool m_rtPropExpired{false};                             //!< If RTT has expired.
  Time m_rtPropFilterLen{};                                //!< A constant specifying the approximate period of the BBR.RTprop min filter window, default 10 secs.
  Time m_rtPropStamp{Seconds (0)};                         //!< The wall clock time at which the current BBR.RTProp sample was obtained.
  bool m_isInitialized{false};                             //!< Set to true after first time initialization variables.
  Ptr<UniformRandomVariable> m_uv{nullptr};                //!< Uniform Random Variable.

  // BBR variant-specific members
  BbrVar m_variant{BBR_V2};                                //!< BBR variant
  double m_lambda{1.0/8.0};                                //!< Lambda for BBR+
  bool m_enableEcn{true};                                  //!< Enable ECN functionality
  bool m_enableExp{false};                                 //!< Enable experimental features

  // BBR v2 specific members
  uint32_t m_bwProbeSamples{0};
  bool m_prevProbeTooHigh{false};
  uint32_t m_bwProbeUpRounds{0};
  uint32_t m_bwProbeUpAcks{0};
  uint32_t m_bwProbeUpCount{0};
  BbrAckPhase m_ackPhase{BBR_ACKS_INIT};
  bool m_lossInRound{false};
  bool m_ecnInRound{false};
  bool m_lossInCycle{false};
  bool m_ecnInCycle{false};
  DataRate m_bwLatest{0};
  uint32_t m_inflightLatest{0};
  uint32_t m_lossRoundDelivered{0};
  bool m_lossRoundStart{false};
  DataRate m_bwLo{std::numeric_limits<uint32_t>::max()};
  uint32_t m_inflightLo{std::numeric_limits<uint32_t>::max()};
  DataRate m_bwHi{std::numeric_limits<uint32_t>::max()};
  uint32_t m_inflightHi{std::numeric_limits<uint32_t>::max()};
  DataRate m_bwMax[2] = {DataRate(0), DataRate(0)};
  uint32_t m_startupLossEvents{0};
  uint32_t m_fullLossCount{8};
  double m_lossThresh{0.02};
  double m_ecnThresh{0.5};
  double m_bbrBeta{0.3};
  double m_ecnAlpha{1.0};
  double m_ecnFactor{0.5};
  double m_ecnGain{1.0/16.0};
  uint32_t m_alphaLastDelivered{0};
  uint32_t m_alphaLastDeliveredEce{0};
  uint32_t m_deliveredEce{0};
  bool m_isEce{false};
  uint32_t m_roundsSinceProbe{0};
  uint32_t m_bwProbeMaxRounds{63};
  uint32_t m_bwProbeRandRounds{2};
  double m_inflightHeadroom{0.15};
  bool m_isRiskyProbe{false};
  bool m_bwProbeSampleOk{false};
  uint32_t m_startupEcnRounds{0};
  uint32_t m_fullEcnCount{3};

  // Additional variant specific members
  Time m_baseRtt{Time::Max()};
  Time m_srtt{Seconds(0)};
  bool m_congestionDelay{false};
  double m_alphaSrtt{1.0/8.0};
  double m_beta{1.5};
  uint32_t m_sentPacketNum{0};
  uint32_t m_cycleLength{8};
  uint32_t m_cycleRand{2};
};

} // namespace ns3

#endif /* TCPBBR2_H */ 