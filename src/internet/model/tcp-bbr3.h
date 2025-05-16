/*
 * Copyright (c) 2023 L4S Contributors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#ifndef TCPBBR3_H
#define TCPBBR3_H

#include "ns3/data-rate.h"
#include "ns3/random-variable-stream.h"
#include "ns3/tcp-congestion-ops.h"
#include "ns3/traced-value.h"
#include "ns3/windowed-filter.h"

namespace ns3
{

/**
 * \ingroup congestionOps
 *
 * \brief BBRv3 congestion control algorithm with ECN support
 *
 * This class implements the BBRv3 (Bottleneck Bandwidth and Round-trip propagation time)
 * congestion control algorithm with ECN support for L4S environments.
 */
class TcpBbr3 : public TcpCongestionOps
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    /**
     * \brief Constructor
     */
    TcpBbr3();

    /**
     * Copy constructor.
     * \param sock The socket to copy from.
     */
    TcpBbr3(const TcpBbr3& sock);

    ~TcpBbr3() override;

    std::string GetName() const override;

    void PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt) override;

    void CongestionStateSet(Ptr<TcpSocketState> tcb,
                            const TcpSocketState::TcpCongState_t newState) override;

    void IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked) override;

    uint32_t GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight) override;

    Ptr<TcpCongestionOps> Fork() override;

    bool HasCongControl() const override;

    void CongControl(Ptr<TcpSocketState> tcb,
                     const TcpRateOps::TcpRateConnection& rc,
                     const TcpRateOps::TcpRateSample& rs) override;

    void SetRateOps(Ptr<TcpRateOps> rateOps) override;

    void Init(Ptr<TcpSocketState> tcb) override;

    void CwndEvent(Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCAEvent_t event) override;

    /**
     * \brief BBR has the following 4 modes for deciding how fast to send:
     */
    enum BbrMode_t
    {
        BBR_STARTUP,   /**< Ramp up sending rate rapidly to fill pipe */
        BBR_DRAIN,     /**< Drain any queue created during startup */
        BBR_PROBE_BW,  /**< Discover, share bw: pace around estimated bw */
        BBR_PROBE_RTT, /**< Cut inflight to min to probe min_rtt */
    };

    /**
     * \brief Bandwidth probing phases
     */
    enum BbrPacingGainPhase_t
    {
        BBR_BW_PROBE_UP,     /**< Push up inflight to probe for bw/vol */
        BBR_BW_PROBE_DOWN,   /**< Drain excess inflight from the queue */
        BBR_BW_PROBE_CRUISE, /**< Use pipe, w/ headroom in queue/pipe */
        BBR_BW_PROBE_REFILL, /**< Refill the pipe again to 100% */
    };

    typedef WindowedFilter<DataRate,
                           MaxFilter<DataRate>,
                           uint32_t,
                           uint32_t>
        MaxBandwidthFilter_t; //!< Definition of max bandwidth filter.

    /**
     * \brief Bandwidth sample structure
     */
    struct BbrBwSample {
        DataRate bw;
        bool is_app_limited;
    };

  private:
    /**
     * \brief Set BBR state to the specified value
     * \param state the new state to set
     */
    void SetBbrState(BbrMode_t state);

    /**
     * \brief Return the current BBR state
     * \return the current state
     */
    uint32_t GetBbrState();

    /**
     * \brief Initialize round counting
     */
    void InitRoundCounting();

    /**
     * \brief Initialize full pipe estimation
     */
    void InitFullPipe();

    /**
     * \brief Initialize pacing rate
     * \param tcb the socket state
     */
    void InitPacingRate(Ptr<TcpSocketState> tcb);

    /**
     * \brief Enter the STARTUP state
     */
    void EnterStartup();

    /**
     * \brief Enter the DRAIN state
     */
    void EnterDrain();

    /**
     * \brief Enter the PROBE_BW state
     */
    void EnterProbeBW();

    /**
     * \brief Enter the PROBE_RTT state
     */
    void EnterProbeRtt(Ptr<TcpSocketState> tcb);

    /**
     * \brief Check if it's time to enter PROBE_RTT state
     * \param tcb the socket state
     * \param rs rate sample
     */
    void CheckProbeRtt(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs);

    /**
     * \brief Check if pipe is filled
     * \param rs rate sample
     */
    void CheckFullPipe(const TcpRateOps::TcpRateSample& rs);

    /**
     * \brief Check if it's time to enter DRAIN state
     * \param tcb the socket state
     */
    void CheckDrain(Ptr<TcpSocketState> tcb);

    /**
     * \brief Exit from PROBE_RTT state
     * \param tcb the socket state
     */
    void ExitProbeRtt(Ptr<TcpSocketState> tcb);

    /**
     * \brief HandleRestartFromIdle, resuming rate sampling
     * \param tcb the socket state
     * \param rs rate sample
     */
    void HandleRestartFromIdle(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs);

    /**
     * \brief Set pacing rate based on BBR state and model
     * \param tcb the socket state
     * \param gain pacing gain
     */
    void SetPacingRate(Ptr<TcpSocketState> tcb, double gain);

    /**
     * \brief Check if BBR is currently probing for bandwidth
     * \param tcb the socket state
     * \return true if BBR is probing for bandwidth
     */
    bool IsBwProbing(Ptr<TcpSocketState> tcb);

    /**
     * \brief Set CWND based on BBR state and model
     * \param tcb the socket state
     * \param rs rate sample
     */
    void SetCwnd(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs);

    /**
     * \brief Calculate inflight window based on estimated BDP
     * \param tcb the socket state
     * \param gain window gain
     * \return inflight bytes
     */
    uint32_t Inflight(Ptr<TcpSocketState> tcb, double gain);

    /**
     * \brief Update model and state
     * \param tcb the socket state
     * \param rs rate sample
     */
    void UpdateModelAndState(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs);

    /**
     * \brief Update control parameters
     * \param tcb the socket state
     * \param rs rate sample
     */
    void UpdateControlParameters(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs);

    /**
     * \brief Updates round counting related variables.
     * \param tcb the socket state
     * \param rs rate sample
     * \return true if round start event occurred
     */
    bool UpdateRoundStart(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs);

    /**
     * \brief Updates RTT statistics
     * \param tcb the socket state
     * \param rs rate sample
     */
    void UpdateRtt(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs);

    /**
     * \brief Update bandwidth measurements
     * \param tcb the socket state
     * \param rs rate sample
     */
    void UpdateBandwidth(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs);

    /**
     * \brief Update estimated BDP and target CWND
     * \param tcb the socket state
     */
    void UpdateTargetCwnd(Ptr<TcpSocketState> tcb);

    /**
     * \brief Update BBR's pacing gain cycle phase
     */
    void AdvanceCyclePhase();

    /**
     * \brief Check if it's time to advance the pacing gain cycle
     * \param tcb the socket state
     * \param rs rate sample
     * \return true if it's time to advance
     */
    bool IsNextCyclePhase(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs);

    /**
     * \brief Check if cycle phase should be updated
     * \param tcb the socket state
     * \param rs rate sample
     */
    void CheckCyclePhase(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs);

    /**
     * \brief Modulates cwnd for recovery phase
     * \param tcb the socket state 
     * \param rs rate sample
     * \return true if recovery actions were taken
     */
    bool ModulateCwndForRecovery(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs);

    /**
     * \brief Modulates cwnd for PROBE_RTT phase
     * \param tcb the socket state
     */
    void ModulateCwndForProbeRtt(Ptr<TcpSocketState> tcb);

    /**
     * \brief Update BBR's minimum RTT estimate
     * \param tcb the socket state
     */
    void UpdateMinRtt(Ptr<TcpSocketState> tcb);

    /**
     * \brief Sets the send quantum
     * \param tcb the socket state
     */
    void SetSendQuantum(Ptr<TcpSocketState> tcb);

    /**
     * \brief Exit from loss recovery
     * \param tcb the socket state
     */
    void ExitLossRecovery(Ptr<TcpSocketState> tcb);
    
    /**
     * \brief Process congestion event from ECN signal
     * \param tcb the socket state
     * \param rs rate sample
     */
    void ProcessEcn(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs);

    /**
     * \brief Set stream instance for random values
     * \param stream stream number
     */
    void SetStream(uint32_t stream);

    // Constants for BBRv3
    static const uint32_t CYCLE_LENGTH = 8; // Number of phases in bandwidth probing cycle
    static const double PACING_GAIN_CYCLE[]; // Pacing gain values for each phase
    static const char* const BbrModeName[]; // State names for logging
    static const char* const BbrCycleName[]; // Cycle phase names for logging

    // BBR model state
    BbrMode_t m_state{BBR_STARTUP};           //!< Current BBR state
    BbrPacingGainPhase_t m_cycleIndex{BBR_BW_PROBE_UP}; //!< Current phase in gain cycle
    MaxBandwidthFilter_t m_maxBwFilter;      //!< Maximum bandwidth filter
    DataRate m_fullBandwidth{0};             //!< Bandwidth when pipe is full
    uint32_t m_fullBandwidthCount{0};        //!< Counter for full bandwidth estimation
    Time m_minRtt{Time::Max()};              //!< Minimum RTT observed
    Time m_minRttStamp{Seconds(0)};          //!< When minimum RTT was last updated
    Time m_probeRttMinStamp{Seconds(0)};     //!< When PROBE_RTT was last updated
    bool m_fullBwReached{false};             //!< True if pipe is filled
    
    // BBR control settings
    double m_pacingGain{0};                  //!< Current pacing gain
    double m_cWndGain{0};                    //!< Current cwnd gain
    Time m_cycleStamp{Seconds(0)};           //!< Last time cycle phase was advanced
    uint32_t m_targetCWnd{0};               //!< Target congestion window
    
    // RTT and window tracking
    uint64_t m_delivered{0};                 //!< Total data delivered (bytes)
    uint32_t m_roundCount{0};                //!< Number of packet-timed rounds
    bool m_roundStart{false};                //!< True if round just started
    bool m_idleRestart{false};               //!< True if restarting from idle
    bool m_probeRttRoundDone{false};         //!< True if PROBE_RTT round is done
    bool m_packetConservation{false};        //!< True if in packet conservation mode
    uint32_t m_priorCwnd{0};                //!< Previous cwnd value
    uint32_t m_nextRoundDelivered{0};       //!< Delivered bytes threshold for next round
    
    // BBR configuration parameters
    uint32_t m_minPipeCwnd{4 * 1448};       //!< Minimum cwnd in PROBE_RTT
    double m_highGain{2.89};                //!< Gain for STARTUP phase
    double m_cwndGainConstant{2.0};         //!< CWND gain
    Time m_probeRttDuration{MilliSeconds(200)}; //!< PROBE_RTT duration
    uint32_t m_bandwidthWindowLength{10};   //!< Max bandwidth filter window length
    uint32_t m_minRttFilterLen{10};         //!< Min RTT filter window length
    uint32_t m_sendQuantum{0};              //!< Pacing quantum
    bool m_hasSeenRtt{false};               //!< True if we've seen a valid RTT sample
    
    // ECN support
    double m_ecnAlpha{1.0};                 //!< ECN marking probability estimate
    double m_ecnAlphaGain{0.125};           //!< Alpha update gain
    uint32_t m_ecnMarked{0};                //!< Number of packets marked with ECN CE
    uint32_t m_ecnAlphaLastUpdate{0};       //!< Last update round for alpha
    uint32_t m_ecnCount{0};                 //!< Total number of packets in the window
    bool m_ecnEnabled{true};                //!< True if ECN is enabled
    bool m_isL4s{true};                     //!< True if L4S mode is enabled
    bool m_ecnSeen{false};                  //!< True if ECN has been observed
    
    // Random number generator
    Ptr<UniformRandomVariable> m_uv{nullptr}; //!< RNG for BBR randomization
    Ptr<TcpRateOps> m_rateOps{nullptr};      //!< Rate operations
};

} // namespace ns3

#endif /* TCPBBR3_H */ 