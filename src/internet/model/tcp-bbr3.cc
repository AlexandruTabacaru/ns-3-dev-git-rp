#include "tcp-bbr3.h"

#include "ns3/log.h"
#include "ns3/simulator.h"


#define  bbr_main CongControl

#define NOW Now().GetSeconds()
#define COUT(log) std::cout << log << std::endl
namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TcpBbr3");
NS_OBJECT_ENSURE_REGISTERED(TcpBbr3);

const double TcpBbr3::PACING_GAIN_CYCLE[] = {5.0 / 4,  91.0 / 100, 1, 1 };

TypeId
TcpBbr3::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TcpBbr3")
            .SetParent<TcpCongestionOps>()
            .AddConstructor<TcpBbr3>()
            .SetGroupName("Internet")
            .AddTraceSource("inflightHi",
                "infliht hi",
                MakeTraceSourceAccessor(&TcpBbr3::m_inflightHi),
                "ns3::TracedValueCallback::Uint32")
            .AddTraceSource("inflightLo",
                "infliht low",
                MakeTraceSourceAccessor(&TcpBbr3::m_inflightLo),
                "ns3::TracedValueCallback::Uint32")
            .AddTraceSource("rt_prop",
                "rt prop",
                MakeTraceSourceAccessor(&TcpBbr3::m_rtProp),
                "ns3::TracedValueCallback::Time")
            .AddTraceSource("maxBw",
                "Maximum bw estimate",
                MakeTraceSourceAccessor(&TcpBbr3::maxBw),
                "ns3::TracedValueCallback::Uint32")
            .AddTraceSource("wildcard",
                "wild card value",
                MakeTraceSourceAccessor(&TcpBbr3::wildcard),
                "ns3::TracedValueCallback::Uint32")
            .AddAttribute("Stream",
                          "Random number stream (default is set to 4 to align with Linux results)",
                          UintegerValue(4),
                          MakeUintegerAccessor(&TcpBbr3::SetStream),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("HighGain",
                          "Value of high gain",
                          DoubleValue(2.89),
                          MakeDoubleAccessor(&TcpBbr3::m_highGain),
                          MakeDoubleChecker<double>())
            .AddAttribute("BwWindowLength",
                          "Length of bandwidth windowed filter",
                          UintegerValue(10),
                          MakeUintegerAccessor(&TcpBbr3::m_bandwidthWindowLength),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("RttWindowLength",
                          "Length of RTT windowed filter",
                          TimeValue(Seconds(10)),
                          MakeTimeAccessor(&TcpBbr3::m_rtPropFilterLen),
                          MakeTimeChecker())
            .AddAttribute("ProbeRttDuration",
                          "Time to be spent in PROBE_RTT phase",
                          TimeValue(MilliSeconds(200)),
                          MakeTimeAccessor(&TcpBbr3::m_probeRttDuration),
                          MakeTimeChecker())
            .AddAttribute("ExtraAckedRttWindowLength",
                          "Window length of extra acked window",
                          UintegerValue(5),
                          MakeUintegerAccessor(&TcpBbr3::m_extraAckedWinRttLength),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute(
                "AckEpochAckedResetThresh",
                "Max allowed val for m_ackEpochAcked, after which sampling epoch is reset",
                UintegerValue(1 << 12),
                MakeUintegerAccessor(&TcpBbr3::m_ackEpochAckedResetThresh),
                MakeUintegerChecker<uint32_t>());
    return tid;
}

TcpBbr3::TcpBbr3()
    : TcpCongestionOps()
{
    NS_LOG_FUNCTION(this);
    m_uv = CreateObject<UniformRandomVariable>();
}

TcpBbr3::TcpBbr3(const TcpBbr3& sock)
    : TcpCongestionOps(sock),
      m_bandwidthWindowLength(sock.m_bandwidthWindowLength),
      m_pacingGain(sock.m_pacingGain),
      m_cWndGain(sock.m_cWndGain),
      m_highGain(sock.m_highGain),
      m_fullBwReached(sock.m_fullBwReached),
      m_minPipeCwnd(sock.m_minPipeCwnd),
      m_roundCount(sock.m_roundCount),
      m_roundStart(sock.m_roundStart),
      m_nextRoundDelivered(sock.m_nextRoundDelivered),
      m_probeRttDuration(sock.m_probeRttDuration),
      m_probeRttMinStamp(sock.m_probeRttMinStamp),
      m_probeRttDoneStamp(sock.m_probeRttDoneStamp),
      m_probeRttRoundDone(sock.m_probeRttRoundDone),
      m_packetConservation(sock.m_packetConservation),
      m_priorCwnd(sock.m_priorCwnd),
      m_idleRestart(sock.m_idleRestart),
      m_targetCWnd(sock.m_targetCWnd),
      m_fullBandwidth(sock.m_fullBandwidth),
      m_fullBandwidthCount(sock.m_fullBandwidthCount),
      m_rtProp(Time::Max()),
      m_sendQuantum(sock.m_sendQuantum),
      m_cycleStamp(sock.m_cycleStamp),
      m_cycleIndex(sock.m_cycleIndex),
      m_rtPropExpired(sock.m_rtPropExpired),
      m_rtPropFilterLen(sock.m_rtPropFilterLen),
      m_rtPropStamp(sock.m_rtPropStamp),
      m_isInitialized(sock.m_isInitialized),
      m_uv(sock.m_uv),
      m_delivered(sock.m_delivered),
      m_appLimited(sock.m_appLimited),
      m_txItemDelivered(sock.m_txItemDelivered),
      m_extraAckedGain(sock.m_extraAckedGain),
      m_extraAckedWinRtt(sock.m_extraAckedWinRtt),
      m_extraAckedWinRttLength(sock.m_extraAckedWinRttLength),
      m_ackEpochAckedResetThresh(sock.m_ackEpochAckedResetThresh),
      m_extraAckedIdx(sock.m_extraAckedIdx),
      m_ackEpochTime(sock.m_ackEpochTime),
      m_ackEpochAcked(sock.m_ackEpochAcked),
      m_hasSeenRtt(sock.m_hasSeenRtt)
{
    NS_LOG_FUNCTION(this);
}

const char* const TcpBbr3::BbrModeName[BBR_PROBE_RTT + 1] = {
    "BBR_STARTUP",
    "BBR_DRAIN",
    "BBR_PROBE_BW",
    "BBR_PROBE_RTT",
};


const char* const TcpBbr3::BbrCycleName[BBR_BW_PROBE_REFILL + 1] = {
    "BBR_BW_PROBE_UP",
    "BBR_BW_PROBE_DOWN",
    "BBR_BW_PROBE_CRUISE",
    "BBR_BW_PROBE_REFILL",
};

void
TcpBbr3::SetStream(uint32_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    m_uv->SetStream(stream);
}

void
TcpBbr3::InitRoundCounting()
{
    NS_LOG_FUNCTION(this);
    m_nextRoundDelivered = 0;
    m_roundStart = false;
    m_roundCount = 0;
}

void
TcpBbr3::InitFullPipe()
{
    NS_LOG_FUNCTION(this);
    m_fullBwReached = false;
    m_fullBandwidth = 0;
    m_fullBandwidthCount = 0;
}

void
TcpBbr3::InitPacingRate(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);

    if (!tcb->m_pacing)
    {
        NS_LOG_WARN("BBR must use pacing");
        tcb->m_pacing = true;
    }

    Time rtt;
    if (tcb->m_minRtt != Time::Max())
    {
        rtt = MilliSeconds(std::max<long int>(tcb->m_minRtt.GetMilliSeconds(), 1));
        m_hasSeenRtt = true;
    }
    else
    {
        rtt = MilliSeconds(1);
    }

    DataRate nominalBandwidth(tcb->m_cWnd * 8 / rtt.GetSeconds());
    tcb->m_pacingRate = DataRate(m_pacingGain * nominalBandwidth.GetBitRate());
    bbr_take_max_bw_sample(nominalBandwidth);
}

bool
TcpBbr3::bbr_is_probing_bandwidth(Ptr<TcpSocketState> tcb)
{
    return (m_state == BBR_STARTUP) ||
    (m_state == BBR_PROBE_BW &&
        (m_cycleIndex == BBR_BW_PROBE_REFILL ||
        m_cycleIndex == BBR_BW_PROBE_UP));
}

void
TcpBbr3::bbr_set_pacing_rate(Ptr<TcpSocketState> tcb, double gain)
{
    NS_LOG_FUNCTION(this << tcb << gain);
    DataRate rate((gain * bbr_bw().GetBitRate()) / 100 * 99); //  margin 
    rate = std::min(rate, tcb->m_maxPacingRate);
    if (!m_hasSeenRtt && tcb->m_minRtt != Time::Max())
        InitPacingRate(tcb);
    
    if (m_fullBwReached || rate > tcb->m_pacingRate)
        tcb->m_pacingRate = rate;
}

void 
TcpBbr3::bbr_init_lower_bounds(Ptr<TcpSocketState> tcb, bool init_bw)
{
    if (init_bw && m_bwLo == std::numeric_limits<int>::max ())
        m_bwLo = bbr_max_bw();
    if (m_inflightLo == std::numeric_limits<int>::max ())
        m_inflightLo = tcb->m_cWnd.Get();   
}

void
TcpBbr3::bbr_loss_lower_bounds(Ptr<TcpSocketState> tcb)
{
    m_bwLo =  std::max<DataRate>(m_bwLatest, DataRate(m_bwLo.GetBitRate() / 100 * 70));
    m_inflightLo = std::max<uint32_t>(m_inflightLatest, m_inflightLo / 100 * 70);
}

uint32_t 
TcpBbr3::bbr_inflight(Ptr<TcpSocketState> tcb, DataRate bw, double gain)
{
    NS_LOG_FUNCTION(this << tcb << gain);
    if (m_rtProp == Time::Max())
    {
        return tcb->m_initialCWnd * tcb->m_segmentSize;
    }    
    double quanta = 3 * m_sendQuantum;
    double estimatedBdp = bbr_bdp(tcb, bw, gain);
    if (m_state == BbrMode_t::BBR_PROBE_BW && m_cycleIndex == 0)
    {
        return (estimatedBdp) + quanta + (2 * tcb->m_segmentSize);
    }
    return (estimatedBdp) + quanta;
}

bool 
TcpBbr3::bbr_is_inflight_too_high(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs)
{
    if (rs.delivered_ce > 0 && rs.m_delivered > 0 && m_ecnEligible /* && bbr_param(sk, ecn_thresh)*/)    //ECN FUNCTIONALITY 
    { 
        double ecn_thresh = rs.m_delivered * bbr_ecn_thresh;
        if (rs.delivered_ce > static_cast<uint32_t>(ecn_thresh))
        {
            return true;
        }
    }
    if (rs.m_bytesLoss > 0 && tcb->m_bytesInFlight > 0)
    {
        if (m_cycleIndex == BBR_BW_PROBE_UP)
        COUT("bytes lost " << rs.m_bytesLoss << " bytes in flight " << tcb->m_bytesInFlight.Get() / 100 * 2);
        if (rs.m_bytesLoss > (tcb->m_bytesInFlight.Get() / 100 * 2))
        {
            return true;
        }
            
        
    }
    return false;
}

void 
TcpBbr3::bbr_handle_inflight_too_high(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs, bool rsmode)
{
    m_prevProbeTooHigh = true;
    m_bwProbeSamples = 0;
    if (rsmode && !rs.m_isAppLimited)
    {
        m_inflightHi = std::max(rs.m_priorInFlight, static_cast<uint32_t>(bbr_target_inflight(tcb) * (1 - bbr_beta)));
        goto done2;

    }
    if (!rs.m_isAppLimited)
        //m_inflightHi = std::max(tcb->m_bytesInFlight.Get(), static_cast<uint32_t>(bbr_target_inflight(tcb) * (1 - bbr_beta)));
        m_inflightHi = (bbr_target_inflight(tcb) * (1 - bbr_beta));
    done2:
    if (m_state == BbrMode_t::BBR_PROBE_BW && m_cycleIndex == BBR_BW_PROBE_UP)
    { 
        bbr_start_bw_probe_down();
    }
}

void
TcpBbr3::bbr_probe_inflight_hi_upward(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs)
{
    NS_LOG_FUNCTION(this << tcb << rs);
    if (tcb->m_cWnd < m_inflightHi)
    {
        m_bwProbeUpAcks = 0;
        return;
    }


    m_bwProbeUpAcks += rs.m_ackedSacked;
    if (m_bwProbeUpAcks >= m_bwProbeUpCount)
    {

        uint32_t delta = m_bwProbeUpAcks / m_bwProbeUpCount;
        m_bwProbeUpAcks -= delta * m_bwProbeUpCount;
        m_inflightHi += delta * tcb->m_segmentSize;
        m_tryFastPath = false;
    }

    if (m_roundStart)
        bbr_raise_inflight_hi_slope(tcb);

}

void 
TcpBbr3::bbr_raise_inflight_hi_slope(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);   //m_cWnd
    uint32_t growth_this_round = 1 << m_bwProbeUpRounds;
    m_bwProbeUpRounds = std::min<uint32_t>(m_bwProbeUpRounds + 1, 30);
    m_bwProbeUpCount = std::max<uint32_t> (tcb->m_cWnd / growth_this_round, 1);
}

uint32_t 
TcpBbr3::bbr_target_inflight(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);
    uint32_t bdp = bbr_inflight(tcb, bbr_bw(), 1);   //REPLACE WITH BBR VERSION LATER
    return std::min(bdp, tcb->m_cWnd.Get());
}

uint32_t
TcpBbr3::bbr_inflight_with_headroom()
{
    NS_LOG_FUNCTION (this);
    if (m_inflightHi == std::numeric_limits<int>::max ())
        return std::numeric_limits<int>::max ();

    uint32_t headroom = (m_inflightHi * bbr_inflight_headroom) ;
    headroom = std::max<uint32_t>(headroom, 1);

    return std::max<uint32_t>(m_inflightHi - headroom, m_minPipeCwnd);
}

void
TcpBbr3::bbr_bound_cwnd_for_inflight_model(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);
    if (!m_isInitialized)
        return;
    uint32_t cap = std::numeric_limits<int>::max();   
    if (m_state == BBR_PROBE_BW && m_cycleIndex  != BBR_BW_PROBE_CRUISE)
    {
        cap = m_inflightHi;
    } else {
        if (m_state == BBR_PROBE_RTT  || ( m_state == BBR_PROBE_BW && m_cycleIndex  == BBR_BW_PROBE_CRUISE)){
            cap = bbr_inflight_with_headroom();
        }
    }
    cap = std::min(cap, m_inflightLo.Get());
    cap = std::max(cap, m_minPipeCwnd);
    tcb->m_cWnd = std::min(tcb->m_cWnd.Get(), cap); 
}

bool
TcpBbr3::bbr_check_time_to_probe_bw(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);
    if (m_ecnEligible && m_ecn_in_cycle && !m_lossInCycle && tcb->m_congState == TcpSocketState::CA_OPEN)      // ECN FUNCTIONALITY 
    {
        uint32_t n = static_cast<uint32_t>(log2(m_inflightHi * bbr_ecn_reprobe_gain));
        bbr_start_bw_probe_refill(tcb, n);
        return true;
    }
    if (bbr_has_elapsed_in_phase(tcb, m_probeWaitTime) || bbr_is_reno_coexistence_probe_time(tcb))
    {
        bbr_start_bw_probe_refill(tcb, 0);
        return true;
    }
    return false;
}

bool
TcpBbr3::bbr_check_time_to_cruise(Ptr<TcpSocketState> tcb,  const TcpRateOps::TcpRateSample& rs, DataRate bw)
{
    
    if (rs.m_priorInFlight > bbr_inflight_with_headroom())
        return false;
    return rs.m_priorInFlight <= bbr_inflight(tcb, bw, 1);
}


void
TcpBbr3::bbr_start_bw_probe_up(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs, const struct bbr_context* ctx)
{
	m_ackPhase = BBR_ACKS_PROBE_STARTING;
	m_nextRoundDelivered = m_delivered;
	m_cycleStamp = Simulator::Now();
	bbr_reset_full_bw();
	m_fullBandwidth = ctx->sample_bw;
	bbr_set_cycle_idx(BBR_BW_PROBE_UP);
	bbr_raise_inflight_hi_slope(tcb);
}

void 
TcpBbr3::bbr_start_bw_probe_down()
{
    bbr_reset_congestion_signals();
    m_bwProbeUpCount = std::numeric_limits<int>::max ();
    bbr_pick_probe_wait();
    m_cycleStamp = Simulator::Now();
    m_ackPhase = BbrAckPhase_t::BBR_ACKS_PROBE_STOPPING;
    m_nextRoundDelivered = m_delivered;
    bbr_set_cycle_idx(BBR_BW_PROBE_DOWN);
}

void
TcpBbr3::bbr_start_bw_probe_refill(Ptr<TcpSocketState> tcb, uint32_t bw_probe_up_rounds)
{
    bbr_reset_lower_bounds();
    m_bwProbeUpRounds = bw_probe_up_rounds;
    m_bwProbeUpAcks = 0;
    m_stoppedRiskyProbe = 0;
    m_cycleStamp = Simulator::Now();
    m_ackPhase = BbrAckPhase_t::BBR_ACKS_REFILLING;
    m_nextRoundDelivered = m_delivered;
    bbr_set_cycle_idx(BBR_BW_PROBE_REFILL);
}

void
TcpBbr3::bbr_start_bw_probe_cruise()
{
    m_cycleStamp = Simulator::Now();
    if (m_inflightLo != std::numeric_limits<int>::max ())
        m_inflightLo = std::min(m_inflightLo.Get(), m_inflightHi.Get());
    bbr_set_cycle_idx(BBR_BW_PROBE_CRUISE);
}

bool 
TcpBbr3::bbr_has_elapsed_in_phase(Ptr<TcpSocketState> tcb, Time interval)
{
    return (interval >= MilliSeconds(0) &&  Simulator::Now() > m_cycleStamp + interval);
}

bool 
TcpBbr3::bbr_is_reno_coexistence_probe_time(Ptr<TcpSocketState> tcb)
{
    return m_roundCount >= std::max<uint32_t>( bbr_bw_probe_max_rounds, 0);
}

void 
TcpBbr3::bbr_set_cycle_idx(uint32_t cycle_idx)
{
    m_cycleIndex = cycle_idx;
    //bbr_update_gains();
}

bool
TcpBbr3::bbr_adapt_upper_bounds(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs)
{
    if (!rs.m_isAppLimited)
    {
        DataRate sample_bw = DataRate( rs.m_deliveryRate * 8);
        bw_hi[1] = sample_bw;
        return true;
    }

    return false;

}

void 
TcpBbr3::bbr_adapt_lower_bounds(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs)
{
    NS_LOG_FUNCTION(this << tcb << rs);
    if (rs.m_isAppLimited)
    {
        return;
    }
    if ( (m_lossInRound || m_ecnInRound) )
    {
        m_inflightLatest = std::min(m_inflightLatest, rs.m_priorInFlight);
    }
    else
    {
        if (m_inflightLatest == std::numeric_limits<int>::max ())
        {
            m_inflightLatest = rs.m_priorInFlight;
        }
        else
        {
            m_inflightLatest = std::max(m_inflightLatest, rs.m_priorInFlight);
        }
    }
    m_bwLatest = std::max(m_bwLatest, DataRate(rs.m_deliveryRate * 8));

    bbr_init_lower_bounds(tcb, true);

    if (m_ackPhase == BBR_ACKS_PROBE_FEEDBACK &&
        m_ackPhase == BBR_ACKS_PROBE_STOPPING)
    {
        if ((m_lossInRound || m_ecnInRound) 
           && ( m_bwLatest  > m_bwLo)
           && m_inflightLatest < m_inflightLo )
        {
            m_bwLo = m_bwLatest;
            m_inflightLo = m_inflightLatest;
        }
    }

    if (m_roundEnd) // I need to check the condition of this round_end;
    {
        m_bwLatest = std::numeric_limits<int>::max ();
        m_inflightLatest = std::numeric_limits<int>::max ();
    }
}

void
TcpBbr3::bbr_reset_congestion_signals()
{
    m_lossInCycle = false;
    m_ecn_in_cycle = false;
}

uint32_t
TcpBbr3::bbr_probe_rtt_cwnd(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);
    /* We use a minimum of 4 packets to ensure a probe reaches full BWE. */
    return std::max<uint32_t>(m_minPipeCwnd, 4 * tcb->m_segmentSize);
}

void
TcpBbr3::bbr_check_drain(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);
    if (m_state == BBR_STARTUP && m_fullBwReached)
    {
        m_state = BBR_DRAIN;
        m_pacingGain = bbr_drain_gain;
        m_cWndGain = bbr_startup_cwnd_gain;
    }
    if (m_state == BBR_DRAIN && tcb->m_bytesInFlight <= bbr_inflight(tcb, bbr_max_bw(), 1))
    {
        m_state = BBR_PROBE_BW;
        m_pacingGain = 1;
        m_cWndGain = bbr_cwnd_gain;
        bbr_start_bw_probe_down();
    }
}

void
TcpBbr3::bbr_check_full_bw_reached(const TcpRateOps::TcpRateSample& rs, const struct bbr_context *ctx)
{
    NS_LOG_FUNCTION(this << rs);
    /*
     * If the delivery rate hasn't increased, advance to probe_bw. If
     * delivery rate > 1.25 * full_bw, advance to probe_bw.
     */
    //COUT(rs.m_deliveryRate *8 << " " << (m_fullBandwidth.GetBitRate()*bbr_full_bw_thresh));
    if (!rs.m_isAppLimited)
    {
        /*
         * Check for a new maximum delivery rate sample in the current
         * measurement window.
         */
        if (ctx->sample_bw >= m_fullBandwidth)
        {
            /*
             * Found a new peak. Flow is not yet at full bw, so restart
             * the full_bw filter at the current time.
             */
            m_fullBandwidth = ctx->sample_bw;
            m_fullBandwidthCount = 0;
            return;
        }

        if (rs.m_deliveryRate * 8 >= (m_fullBandwidth.GetBitRate()*bbr_full_bw_thresh))
        {
            m_fullBandwidthCount = 0;
            return;
        }
    }

    /* Another round w/o much delivery rate growth; evidence that we're at bw. */
    ++m_fullBandwidthCount;
    if (m_fullBandwidthCount >= bbr_full_bw_cnt)
    {
        m_fullBwReached = true;
    }
}

bool
TcpBbr3::bbr_full_bw_reached()
{
    return m_fullBwReached;
}

uint32_t
TcpBbr3::GetBbrState()
{
    NS_LOG_FUNCTION(this);
    return m_state;
}

double
TcpBbr3::GetPacingGain()
{
    NS_LOG_FUNCTION(this);
    return m_pacingGain;
}

double
TcpBbr3::GetCwndGain()
{
    NS_LOG_FUNCTION(this);
    return m_cWndGain;
}

void
TcpBbr3::bbr_update_min_rtt(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs)
{
    NS_LOG_FUNCTION(this << tcb << rs);
    bool filter_expired;

    m_hasSeenRtt = true;
    // Track min RTT seen in the min filter window, to version in key model params.
    filter_expired = Simulator::Now() > m_rtPropStamp + m_rtPropFilterLen;
    m_rtPropExpired = filter_expired ;
    if (rs.m_rtt >= MilliSeconds(0) && (rs.m_rtt < m_rtProp || filter_expired))
    {
        m_rtProp = rs.m_rtt;
        m_rtPropStamp = Simulator::Now();
    }

    bool probe_rtt_expired = Simulator::Now() > m_probeRttMinStamp + bbr_probe_rtt_win;
    if (rs.m_rtt >= MilliSeconds(0) && (rs.m_rtt < m_probeRttMin || probe_rtt_expired))
    {
        m_probeRttMin = rs.m_rtt;
        m_probeRttMinStamp = Simulator::Now();
    }
}

void
TcpBbr3::UpdateRTprop(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);
    if (tcb->m_lastRtt > MilliSeconds(0) && (tcb->m_lastRtt < m_rtProp || m_rtPropExpired))
    {
        m_rtProp = tcb->m_lastRtt;
        m_rtPropStamp = Simulator::Now();
        m_rtPropExpired = false;
    }
}

void 
TcpBbr3::UpdateTargetCwnd(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);
    m_targetCWnd = bbr_inflight(tcb, bbr_bw(), m_cWndGain);
    m_targetCWnd = std::max(m_targetCWnd, m_minPipeCwnd);
}

void
TcpBbr3::ProcessEcn(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateSample& rs)
{
    NS_LOG_FUNCTION(this << tcb << rs);
    
    if (!rs.m_ecn)
    {
        return;
    }

    // Handle L4S ECN marking
    if (tcb->m_ecnState == TcpSocketState::ECN_CE_RCVD)
    {
        // Reduce congestion window by 50% on CE marking
        tcb->m_cWnd = std::max(tcb->m_cWnd / 2, tcb->m_segmentSize);
        
        // Enter drain state to reduce queue
        if (m_state == BBR_PROBE_BW)
        {
            EnterDrain();
        }
        
        // Reset ECN state
        tcb->m_ecnState = TcpSocketState::ECN_IDLE;
    }
}

void
TcpBbr3::Init(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);
    
    // Initialize BBR state
    m_state = BBR_STARTUP;
    m_pacingGain = m_highGain;
    m_cWndGain = m_highGain;
    
    // Initialize ECN support for L4S
    tcb->m_ecnEnabled = true;
    tcb->m_useEcn = TcpSocketState::On;
    
    // Initialize other BBR parameters
    InitRoundCounting();
    InitFullPipe();
    InitPacingRate(tcb);
    
    m_rtProp = Time::Max();
    m_rtPropStamp = Simulator::Now();
    m_nextRoundDelivered = 0;
    m_probeRttDoneStamp = Time::Max();
    m_probeRttRoundDone = false;
    m_packetConservation = false;
    m_priorCwnd = 0;
    m_idleRestart = false;
    m_targetCWnd = 0;
    m_fullBandwidth = 0;
    m_fullBandwidthCount = 0;
    m_rtPropExpired = false;
    m_isInitialized = true;
}
} 