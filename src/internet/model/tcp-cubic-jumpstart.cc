// tcp-cubic.cc (Modified for JumpStart)

#include "tcp-cubic-jumpstart.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/tcp-socket-base.h"
#include "tcp-tx-buffer.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("TcpCubicJumpstart");
NS_OBJECT_ENSURE_REGISTERED(TcpCubicJumpstart);

TypeId TcpCubicJumpstart::GetTypeId()
{
  static TypeId tid = TypeId("ns3::TcpCubicJumpstart")
    .SetParent<TcpCongestionOps>()
    .AddConstructor<TcpCubicJumpstart>()
    .SetGroupName("Internet")
    .AddAttribute("FastConvergence", "Enable fast convergence", BooleanValue(true),
                MakeBooleanAccessor(&TcpCubicJumpstart::m_fastConvergence), MakeBooleanChecker())
    .AddAttribute("TcpFriendliness", "Enable TCP friendliness", BooleanValue(true),
                MakeBooleanAccessor(&TcpCubicJumpstart::m_tcpFriendliness), MakeBooleanChecker())
    .AddAttribute("Beta", "Multiplicative decrease factor",
                DoubleValue(0.7), MakeDoubleAccessor(&TcpCubicJumpstart::m_beta), MakeDoubleChecker<double>(0.0))
    .AddAttribute("C", "CUBIC scaling factor",
                DoubleValue(0.4), MakeDoubleAccessor(&TcpCubicJumpstart::m_c), MakeDoubleChecker<double>(0.0))
    .AddAttribute("CubicDelta", "Time to wait after fast recovery",
                TimeValue(MilliSeconds(10)), MakeTimeAccessor(&TcpCubicJumpstart::m_cubicDelta), MakeTimeChecker())
    .AddAttribute("CntClamp",
                "Counter value when no losses are detected (counter is used"
                " when incrementing cWnd in congestion avoidance, to avoid"
                " floating point arithmetic). It is the modulo of the (avoided)"
                " division",
                UintegerValue(20),
                MakeUintegerAccessor(&TcpCubicJumpstart::m_cntClamp),
                MakeUintegerChecker<uint8_t>());
    return tid;
}

TcpCubicJumpstart::TcpCubicJumpstart()
  : TcpCongestionOps(),
    m_fastConvergence(true),
    m_tcpFriendliness(true),
    m_beta(0.7),
    m_c(0.4),
    m_cubicDelta(MilliSeconds(10)),
    m_jumpstartDone(false),
    m_initialBurstSize(0),
    m_initialRtt(MilliSeconds(100)),
    m_cWndCnt(0),
    m_lastMaxCwnd(0),
    m_bicOriginPoint(0),
    m_bicK(0.0),
    m_delayMin(Time::Min()),
    m_epochStart(Time::Min()),
    m_ackCnt(0),
    m_tcpCwnd(0)
    
{
    NS_LOG_FUNCTION(this);
}

TcpCubicJumpstart::TcpCubicJumpstart(const TcpCubicJumpstart &sock)
  : TcpCongestionOps(sock),
    m_fastConvergence(sock.m_fastConvergence),
    m_tcpFriendliness(sock.m_tcpFriendliness),
    m_beta(sock.m_beta),
    m_c(sock.m_c),
    m_cubicDelta(sock.m_cubicDelta),
    m_jumpstartDone(false),
    m_initialBurstSize(sock.m_initialBurstSize),
    m_initialRtt(sock.m_initialRtt),
    m_cWndCnt(sock.m_cWndCnt),
    m_lastMaxCwnd(sock.m_lastMaxCwnd),
    m_bicOriginPoint(sock.m_bicOriginPoint),
    m_bicK(sock.m_bicK),
    m_delayMin(sock.m_delayMin),
    m_epochStart(sock.m_epochStart),
    m_ackCnt(sock.m_ackCnt),
    m_tcpCwnd(sock.m_tcpCwnd),
    m_cntClamp(sock.m_cntClamp)
{
    NS_LOG_FUNCTION(this);
}

std::string TcpCubicJumpstart::GetName() const { return "TcpCubicJumpstart"; }

void TcpCubicJumpstart::Init(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this);
    // NS_LOG_UNCOND("JumpStart Init Called");

    m_jumpstartDone = false;

    if (tcb->m_lastRtt.Get().IsPositive())
    {
        m_initialRtt = tcb->m_lastRtt;
    }
    else
    {
        m_initialRtt = MilliSeconds(100);
    }

    // Ptr<TcpSocketBase> sock = tcb->GetObject<TcpSocketBase>();
    // if (!sock)
    // {
    //     NS_LOG_UNCOND("No socket found");
    //     return;
    // }

    // uint32_t txSize = sock->GetTxBuffer()->Size();
    // uint32_t advWnd = tcb->m_rxBuffer->MaxBufferSize() * tcb->m_segmentSize;
    uint32_t advWnd = tcb->m_rxBuffer->MaxBufferSize();
    m_initialBurstSize = advWnd / tcb->m_segmentSize;
    // m_initialBurstSize = std::min(txSize, advWnd) / tcb->m_segmentSize;


    uint32_t oldCwnd = tcb->m_cWnd;
    uint32_t newCwnd = std::max(tcb->m_cWnd.Get(), advWnd);
    // uint32_t newCwnd = std::max(tcb->m_cWnd.Get(), m_initialBurstSize * tcb->m_segmentSize);

    if (oldCwnd != newCwnd)
    {
        // NS_LOG_UNCOND("JumpStart: old cwnd " << oldCwnd << " new cwnd " << newCwnd);
        tcb->m_cWnd = newCwnd;
    }

    // tcb->m_cWnd = std::max(tcb->m_cWnd.Get(), m_initialBurstSize * tcb->m_segmentSize);

    // Time interval = m_initialRtt / m_initialBurstSize;

    tcb->m_pacingRate = DataRate(tcb->m_cWnd * 8 / m_initialRtt.GetSeconds());

    // for (uint32_t i = 0; i < m_initialBurstSize; ++i)
    // {
    //     Simulator::Schedule(interval * i, &TcpCubicJumpstart::SendOneSegment, this, sock, tcb);
    //     NS_LOG_UNCOND("Scheduled packet" << i << "out of " << m_initialBurstSize);
    // }

    tcb->m_ssThresh = tcb->m_cWnd;

    // NS_LOG_UNCOND("CWND: " << tcb->m_cWnd);
    // NS_LOG_UNCOND("SSTHRESH: " << tcb->m_ssThresh);

    // m_jumpstartDone = true;
    // NS_LOG_UNCOND("JumpStart Done");
}

void TcpCubicJumpstart::JumpStart(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this);
    
    if (tcb->m_lastRtt.Get().IsPositive())
    {
        m_initialRtt = tcb->m_lastRtt;
    }
    else
    {
        m_initialRtt = MilliSeconds(100);
    }

    uint32_t advWnd = tcb->m_rxBuffer->MaxBufferSize();
    m_initialBurstSize = advWnd / tcb->m_segmentSize;

    uint32_t oldCwnd = tcb->m_cWnd;
    uint32_t newCwnd = std::max(tcb->m_cWnd.Get(), advWnd);

    if (oldCwnd != newCwnd)
    {
        tcb->m_cWnd = newCwnd;
    }

    tcb->m_pacing = true;
    tcb->m_paceInitialWindow = true;
    tcb->m_pacingRate = DataRate(tcb->m_cWnd * 8 / m_initialRtt.GetSeconds());

    tcb->m_ssThresh = tcb->m_cWnd;

    m_jumpstartDone = true;

}

void TcpCubicJumpstart::SendOneSegment(Ptr<TcpSocketBase> sock, Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this);
    uint32_t segmentSize = tcb->m_segmentSize;
    if (sock)
    {
        Ptr<Packet> packet = sock->GetTxBuffer()->CopyFromSequence(segmentSize, sock->GetTxBuffer()->HeadSequence())->GetPacket()->Copy();
        sock->Send(packet, 0);
    }
}

void TcpCubicJumpstart::IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);

    tcb->m_pacingRate = DataRate(0);

    if (!tcb->m_isCwndLimited)
    {
        NS_LOG_DEBUG("No increase because current cwnd " << tcb->m_cWnd
                                                            << " is not limiting the flow");
        return;
    }

    if (tcb->m_cWnd < tcb->m_ssThresh)
    {

        // In Linux, the QUICKACK socket option enables the receiver to send
        // immediate acks initially (during slow start) and then transition
        // to delayed acks.  ns-3 does not implement QUICKACK, and if ack
        // counting instead of byte counting is used during slow start window
        // growth, when TcpSocket::DelAckCount==2, then the slow start will
        // not reach as large of an initial window as in Linux.  Therefore,
        // we can approximate the effect of QUICKACK by making this slow
        // start phase perform Appropriate Byte Counting (RFC 3465)
        tcb->m_cWnd += segmentsAcked * tcb->m_segmentSize;
        segmentsAcked = 0;

        NS_LOG_INFO("In SlowStart, updated to cwnd " << tcb->m_cWnd << " ssthresh "
                                                        << tcb->m_ssThresh);
    }

    if (tcb->m_cWnd >= tcb->m_ssThresh && segmentsAcked > 0)
    {
        m_cWndCnt += segmentsAcked;
        uint32_t cnt = Update(tcb, segmentsAcked);

        /* According to RFC 6356 even once the new cwnd is
            * calculated you must compare this to the number of ACKs received since
            * the last cwnd update. If not enough ACKs have been received then cwnd
            * cannot be updated.
            */
        if (m_cWndCnt >= cnt)
        {
            tcb->m_cWnd += tcb->m_segmentSize;
            m_cWndCnt -= cnt;
            NS_LOG_INFO("In CongAvoid, updated to cwnd " << tcb->m_cWnd);
        }
        else
        {
            NS_LOG_INFO("Not enough segments have been ACKed to increment cwnd."
                        "Until now "
                        << m_cWndCnt << " cnd " << cnt);
        }
    }

}

uint32_t
TcpCubicJumpstart::Update(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    NS_LOG_FUNCTION(this);

    Time t;
    uint32_t delta;
    uint32_t bicTarget;
    uint32_t cnt = 0;
    uint32_t maxCnt;
    double offs;
    uint32_t segCwnd = tcb->GetCwndInSegments();

    m_ackCnt += segmentsAcked;

    if (m_epochStart == Time::Min())
    {
        m_epochStart = Simulator::Now(); // record the beginning of an epoch
        m_ackCnt = segmentsAcked;
        m_tcpCwnd = segCwnd;

        if (m_lastMaxCwnd <= segCwnd)
        {
            NS_LOG_DEBUG("lastMaxCwnd <= m_cWnd. K=0 and origin=" << segCwnd);
            m_bicK = 0.0;
            m_bicOriginPoint = segCwnd;
        }
        else
        {
            m_bicK = std::pow((m_lastMaxCwnd - segCwnd) / m_c, 1 / 3.);
            m_bicOriginPoint = m_lastMaxCwnd;
            NS_LOG_DEBUG("lastMaxCwnd > m_cWnd. K=" << m_bicK << " and origin=" << m_lastMaxCwnd);
        }
    }

    t = Simulator::Now() + m_delayMin - m_epochStart;

    if (t.GetSeconds() < m_bicK) /* t - K */
    {
        offs = m_bicK - t.GetSeconds();
        NS_LOG_DEBUG("t=" << t.GetSeconds() << " <k: offs=" << offs);
    }
    else
    {
        offs = t.GetSeconds() - m_bicK;
        NS_LOG_DEBUG("t=" << t.GetSeconds() << " >= k: offs=" << offs);
    }

    /* Constant value taken from Experimental Evaluation of Cubic Tcp, available at
     * eprints.nuim.ie/1716/1/Hamiltonpfldnet2007_cubic_final.pdf */
    delta = m_c * std::pow(offs, 3);

    NS_LOG_DEBUG("delta: " << delta);

    if (t.GetSeconds() < m_bicK)
    {
        // below origin
        bicTarget = m_bicOriginPoint - delta;
        NS_LOG_DEBUG("t < k: Bic Target: " << bicTarget);
    }
    else
    {
        // above origin
        bicTarget = m_bicOriginPoint + delta;
        NS_LOG_DEBUG("t >= k: Bic Target: " << bicTarget);
    }

    // Next the window target is converted into a cnt or count value. CUBIC will
    // wait until enough new ACKs have arrived that a counter meets or exceeds
    // this cnt value. This is how the CUBIC implementation simulates growing
    // cwnd by values other than 1 segment size.
    if (bicTarget > segCwnd)
    {
        cnt = segCwnd / (bicTarget - segCwnd);
        NS_LOG_DEBUG("target>cwnd. cnt=" << cnt);
    }
    else
    {
        cnt = 100 * segCwnd;
    }

    if (m_lastMaxCwnd == 0 && cnt > m_cntClamp)
    {
        cnt = m_cntClamp;
    }

    if (m_tcpFriendliness)
    {
        auto scale = static_cast<uint32_t>(8 * (1024 + m_beta * 1024) / 3 / (1024 - m_beta * 1024));
        delta = (segCwnd * scale) >> 3;
        while (m_ackCnt > delta)
        {
            m_ackCnt -= delta;
            m_tcpCwnd++;
        }
        if (m_tcpCwnd > segCwnd)
        {
            delta = m_tcpCwnd - segCwnd;
            maxCnt = segCwnd / delta;
            if (cnt > maxCnt)
            {
                cnt = maxCnt;
            }
        }
    }

    // The maximum rate of cwnd increase CUBIC allows is 1 packet per
    // 2 packets ACKed, meaning cwnd grows at 1.5x per RTT.
    return std::max(cnt, 2U);
}

void TcpCubicJumpstart::PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time &rtt)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked << rtt);

    /* Discard delay samples right after fast recovery */
    if (m_epochStart != Time::Min() && (Simulator::Now() - m_epochStart) < m_cubicDelta)
    {
        return;
    }

    /* first time call or link delay decreases */
    if (m_delayMin == Time::Min() || m_delayMin > rtt)
    {
        m_delayMin = rtt;
    }
}


uint32_t TcpCubicJumpstart::GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
    NS_LOG_FUNCTION(this << tcb << bytesInFlight);

    uint32_t segCwnd = tcb->GetCwndInSegments();
    NS_LOG_DEBUG("Loss at cWnd=" << segCwnd
                                 << " segments in flight=" << bytesInFlight / tcb->m_segmentSize);

    /* Wmax and fast convergence */
    if (segCwnd < m_lastMaxCwnd && m_fastConvergence)
    {
        m_lastMaxCwnd = (segCwnd * (1 + m_beta)) / 2; // Section 4.6 in RFC 8312
    }
    else
    {
        m_lastMaxCwnd = segCwnd;
    }

    m_epochStart = Time::Min(); // end of epoch

    /* Formula taken from the Linux kernel */
    uint32_t ssThresh = std::max(static_cast<uint32_t>(segCwnd * m_beta), 2U) * tcb->m_segmentSize;

    NS_LOG_DEBUG("SsThresh = " << ssThresh);

    return ssThresh;
}

void TcpCubicJumpstart::CongestionStateSet(Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCongState_t newState)
{
    NS_LOG_FUNCTION(this << tcb << newState);

    if (newState == TcpSocketState::CA_OPEN && !m_jumpstartDone)
    {
        JumpStart(tcb);
    }

    if (newState == TcpSocketState::CA_LOSS)
    {
        CubicReset(tcb);
    }
}

void TcpCubicJumpstart::CubicReset(Ptr<const TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);
    m_bicOriginPoint = 0;
    m_bicK = 0;
    m_ackCnt = 0;
    m_tcpCwnd = 0;
    m_delayMin = Time::Min();
}

Ptr<TcpCongestionOps>
TcpCubicJumpstart::Fork()
{
    NS_LOG_FUNCTION(this);
    return CopyObject<TcpCubicJumpstart>(this);
}

} // namespace ns3
